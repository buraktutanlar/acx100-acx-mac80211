/* src/acx100_helper2.c - helper functions for 802.11 protocol management
 *
 * --------------------------------------------------------------------
 *
 * Copyright (C) 2003  ACX100 Open Source Project
 *
 *   The contents of this file are subject to the Mozilla Public
 *   License Version 1.1 (the "License"); you may not use this file
 *   except in compliance with the License. You may obtain a copy of
 *   the License at http://www.mozilla.org/MPL/
 *
 *   Software distributed under the License is distributed on an "AS
 *   IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 *   implied. See the License for the specific language governing
 *   rights and limitations under the License.
 *
 *   Alternatively, the contents of this file may be used under the
 *   terms of the GNU Public License version 2 (the "GPL"), in which
 *   case the provisions of the GPL are applicable instead of the
 *   above.  If you wish to allow the use of your version of this file
 *   only under the terms of the GPL and not to allow others to use
 *   your version of this file under the MPL, indicate your decision
 *   by deleting the provisions above and replace them with the notice
 *   and other provisions required by the GPL.  If you do not delete
 *   the provisions above, a recipient may use your version of this
 *   file under either the MPL or the GPL.
 *
 * --------------------------------------------------------------------
 *
 * Inquiries regarding the ACX100 Open Source Project can be
 * made directly to:
 *
 * acx100-users@lists.sf.net
 * http://acx100.sf.net
 *
 * --------------------------------------------------------------------
 */

#ifdef S_SPLINT_S /* some crap that splint needs to not crap out */
#define __signed__ signed
#define __u64 unsigned long long
#define loff_t unsigned long
#define sigval_t unsigned long
#define siginfo_t unsigned long
#define stack_t unsigned long
#define __s64 signed long long
#endif
#include <linux/config.h>
#include <linux/version.h>

#include <linux/module.h>
#include <linux/kernel.h>

#include <linux/sched.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/if_arp.h>
#include <linux/rtnetlink.h>
#include <linux/wireless.h>
#if WIRELESS_EXT >= 13
#include <net/iw_handler.h>
#endif /* WE >= 13 */
#include <linux/netdevice.h>
#include <asm/io.h>
#include <linux/delay.h>
#include <asm/byteorder.h>
#include <asm/bitops.h>
#include <asm/uaccess.h>
#include <asm/string.h>

#include <wlan_compat.h>

#include <linux/ioport.h>
#include <linux/pm.h>

#include <linux/dcache.h>
#include <linux/highmem.h>
#include <linux/etherdevice.h>


/*================================================================*/
/* Project Includes */

#include <acx.h>

static client_t *acx_sta_list_alloc(wlandevice_t *priv);
static client_t *acx_sta_list_add(wlandevice_t *priv, const u8 *address);
static client_t *acx_sta_list_get_from_hash(wlandevice_t *priv, const u8 *address);
static client_t *acx_sta_list_get(wlandevice_t *priv, const u8 *address);
static u32 acx_transmit_assocresp(const wlan_fr_assocreq_t *arg_0,
		                          wlandevice_t *priv);
static u32 acx_transmit_reassocresp(const wlan_fr_reassocreq_t *arg_0,
		                            wlandevice_t *priv);
static int acx_process_disassoc(const wlan_fr_disassoc_t *req, wlandevice_t *priv);
static int acx_process_disassociate(const wlan_fr_disassoc_t *req, wlandevice_t *priv);
static u32 acx_process_mgmt_frame(struct rxhostdescriptor *rxdesc,
				 wlandevice_t *priv);
static int acx_process_data_frame_master(struct rxhostdescriptor *rxdesc,
				 wlandevice_t *priv);
static int acx_process_data_frame_client(struct rxhostdescriptor *rxdesc,
				     wlandevice_t *priv);
static int acx_process_NULL_frame(struct rxhostdescriptor *rxdesc, 
			      wlandevice_t *priv, int vala);
static void acx_process_probe_response(wlan_fr_proberesp_t *req,
				const struct rxbuffer *mmt, wlandevice_t *priv);
static int acx_process_assocresp(const wlan_fr_assocresp_t *req, wlandevice_t *priv);
static int acx_process_reassocresp(const wlan_fr_reassocresp_t *req, wlandevice_t *priv);
static int acx_process_authen(const wlan_fr_authen_t *req, wlandevice_t *priv);
 
static int acx_process_deauthen(const wlan_fr_deauthen_t *req, wlandevice_t *priv);
static int acx_process_deauthenticate(const wlan_fr_deauthen_t *req, wlandevice_t *priv);
static int acx_transmit_deauthen(const u8 *addr, client_t *clt, wlandevice_t *priv,
 		                      u16 reason);
static int acx_transmit_authen1(wlandevice_t *priv);
static int acx_transmit_authen2(const wlan_fr_authen_t *arg_0, client_t *sta_list,
 		                      wlandevice_t *priv);
static int acx_transmit_authen3(const wlan_fr_authen_t *arg_0, wlandevice_t *priv);
static int acx_transmit_authen4(const wlan_fr_authen_t *arg_0, wlandevice_t *priv);
static int acx_transmit_assoc_req(wlandevice_t *priv);
static void ActivatePowerSaveMode(wlandevice_t *priv, /*@unused@*/ int vala);


static u16 CurrentAID = 1;

static const u8 bcast_addr[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

const char * const g_wlan_state_str[COUNT_STATE_STR] = { /* global! */
	"STOPPED", "SCANNING", "WAIT_AUTH",
	"AUTHENTICATED", "ASSOCIATED", "INVALID??"
};

/*----------------------------------------------------------------
* acx_sta_list_init
*----------------------------------------------------------------*/
void acx_sta_list_init(wlandevice_t *priv)
{
	FN_ENTER;
	memset(priv->sta_hash_tab, 0, sizeof(priv->sta_hash_tab));
	memset(priv->sta_list, 0, sizeof(priv->sta_list));
	FN_EXIT0();
}

/*----------------------------------------------------------------
* acx_sta_list_alloc
*----------------------------------------------------------------*/
static inline client_t *acx_sta_list_alloc(wlandevice_t *priv)
{
	unsigned int i = 0;

	FN_ENTER;
	for (i = 0; i < VEC_SIZE(priv->sta_list); i++) {
		if (priv->sta_list[i].used == 0) {
			priv->sta_list[i].used = CLIENT_EXIST_1;
			priv->sta_list[i].auth_alg = WLAN_AUTH_ALG_SHAREDKEY;
			priv->sta_list[i].auth_step = 1;
			FN_EXIT1((int)&(priv->sta_list[i]));
			return &(priv->sta_list[i]);
		}
	}
	FN_EXIT1((int)NULL);
	return NULL;
}

/*----------------------------------------------------------------
* acx_sta_list_add
*----------------------------------------------------------------*/
static client_t *acx_sta_list_add(wlandevice_t *priv, const u8 *address)
{
	client_t *client;
	int index;

	FN_ENTER;

	client = acx_sta_list_alloc(priv);
	if (!client)
		goto done;
	MAC_COPY(client->address, address);
	client->used = CLIENT_EXIST_1;

	index = address[5] % VEC_SIZE(priv->sta_hash_tab);
	client->next = priv->sta_hash_tab[index];
	priv->sta_hash_tab[index] = client;

	acxlog(L_BINSTD | L_ASSOC,
		"<acx_sta_list_add> sta="MACSTR"\n", MAC(address));
done:
	FN_EXIT1((int) client);
	return client;
}

/*----------------------------------------------------------------
* acx_sta_list_get_from_hash
*----------------------------------------------------------------*/
static inline client_t *acx_sta_list_get_from_hash(wlandevice_t *priv, const u8 *address)
{
	return priv->sta_hash_tab[address[5] % VEC_SIZE(priv->sta_hash_tab)];
}

/*----------------------------------------------------------------
* acx_sta_list_get
*----------------------------------------------------------------*/
static client_t *acx_sta_list_get(wlandevice_t *priv, const u8 *address)
{
	client_t *client;
	FN_ENTER;
	client = acx_sta_list_get_from_hash(priv, address);
	while (client) {
		if (mac_is_equal(address, client->address))
			break;
		client = client->next;
	}
	FN_EXIT1((int) client);
	return client;
}

static client_t *acx_sta_list_get_or_add(wlandevice_t *priv, const u8 *address)
{
	client_t *client = acx_sta_list_get(priv, address);
	if (!client)
		client = acx_sta_list_add(priv, address);
	return client;
}

/*----------------------------------------------------------------
* acx_set_status
*----------------------------------------------------------------*/
void acx_set_status(wlandevice_t *priv, u16 new_status)
{
	const char *stat;
#define QUEUE_OPEN_AFTER_ASSOC 1 /* this really seems to be needed now */
#if QUEUE_OPEN_AFTER_ASSOC
#endif
	u16 old_status = priv->status;

	FN_ENTER;
	stat = acx_get_status_name(new_status);

	acxlog(L_BINDEBUG | L_ASSOC, "%s: Setting status = %d (%s)\n",
	       __func__, new_status, stat);

#if WIRELESS_EXT > 13 /* wireless_send_event() and SIOCGIWSCAN */
	if (ACX_STATUS_4_ASSOCIATED == new_status) {
		union iwreq_data wrqu;

		wrqu.data.length = 0;
		wrqu.data.flags = 0;
		wireless_send_event(priv->netdev, SIOCGIWSCAN, &wrqu, NULL);

		wrqu.data.length = 0;
		wrqu.data.flags = 0;
		MAC_COPY(wrqu.ap_addr.sa_data, priv->bssid);
		wrqu.ap_addr.sa_family = ARPHRD_ETHER;
		wireless_send_event(priv->netdev, SIOCGIWAP, &wrqu, NULL);
	} else {
		union iwreq_data wrqu;

		/* send event with empty BSSID to indicate we're not associated */
		MAC_ZERO(wrqu.ap_addr.sa_data);
		wrqu.ap_addr.sa_family = ARPHRD_ETHER;
		wireless_send_event(priv->netdev, SIOCGIWAP, &wrqu, NULL);

		/* TODO: why is this checked *here*??
		** (NB: STOPPED is set ONLY by acx_down)
		** disabling... check acx_up(), acx_start() for equivalent code
		** A: because we had to shutdown the card to init the templates while no traffic is going on. Not sure whether that even still applies, probably not... (we want to get to a state where we update things on-the-fly and make sure to simply notify the network properly in that case) */
#if 0
		if (ACX_STATUS_0_STOPPED == new_status) {
			if (!mac_is_equal(priv->netdev->dev_addr, priv->dev_addr)) {
				/* uh oh, the interface's MAC address changed,
				 * need to update templates (and init STAs??) */
				acxlog(L_STD, "Detected MAC address change, updating card configuration.\n");

				/* the MAC address has to be updated first,
				 * since otherwise we enter an eternal loop,
				 * as update_card_settings calls set_status */
				MAC_COPY(priv->dev_addr, priv->netdev->dev_addr);
				SET_BIT(priv->set_mask, GETSET_STATION_ID|SET_TEMPLATES|SET_STA_LIST);
				acx_update_card_settings(priv, 0, 0, 0);
			}
		}
#endif
	}
#endif

	priv->status = new_status;

	switch (new_status) {
	case ACX_STATUS_1_SCANNING:
		priv->scan_retries = 0;
		acx_set_timer(priv, 2500000); /* 2.5s initial scan time (used to be 1.5s, but failed to find WEP APs!) */
		break;
	case ACX_STATUS_2_WAIT_AUTH:
	case ACX_STATUS_3_AUTHENTICATED:
		priv->auth_or_assoc_retries = 0;
		acx_set_timer(priv, 1500000); /* 1.5 s */
		break;
	}

#if QUEUE_OPEN_AFTER_ASSOC
	if (new_status == ACX_STATUS_4_ASSOCIATED)	{
		if (old_status < ACX_STATUS_4_ASSOCIATED) {
			/* ah, we're newly associated now,
			 * so let's indicate carrier */
			acx_carrier_on(priv->netdev, "after association");
			acx_wake_queue(priv->netdev, "after association");
		}
	} else {
		/* not associated any more, so let's kill carrier */
		if (old_status >= ACX_STATUS_4_ASSOCIATED) {
			acx_carrier_off(priv->netdev, "after losing association");
			acx_stop_queue(priv->netdev, "after losing association");
		}
	}
#endif
	FN_EXIT0();
}

static inline p80211_hdr_t *acx_get_p80211_hdr(wlandevice_t *priv, const rxhostdesc_t *rxdesc)
{
	if (priv->rx_config_1 & RX_CFG1_INCLUDE_ADDIT_HDR) {
		/* take into account additional header in front of packet */
		return (p80211_hdr_t *)((u8 *)&rxdesc->data->hdr_a3 + 4);
	} else {
		return (p80211_hdr_t *)&rxdesc->data->hdr_a3;
	}
}

/*------------------------------------------------------------------------------
 * acx_rx_ieee802_11_frame
 *
 *
 * Arguments:
 *
 * Returns:
 *
 * Side effects:
 *
 * Call context:
 *
 * STATUS: FINISHED, UNVERIFIED.
 *
 * Comment:
 *
 *----------------------------------------------------------------------------*/
int acx_rx_ieee802_11_frame(wlandevice_t *priv, rxhostdesc_t *rxdesc)
{
	unsigned int ftype, fstype;
	const p80211_hdr_t *p80211_hdr;
	int result = NOT_OK;

	FN_ENTER;

	p80211_hdr = acx_get_p80211_hdr(priv, rxdesc);
/*	printk("Rx_CONFIG_1 = %X\n",priv->rx_config_1 & RX_CFG1_INCLUDE_ADDIT_HDR); */

	/* see IEEE 802.11-1999.pdf chapter 7 "MAC frame formats" */
	ftype = p80211_hdr->a3.fc & WF_FC_FTYPEi;
	fstype = p80211_hdr->a3.fc & WF_FC_FSTYPEi;

	switch (ftype) {
	/* check data frames first, for speed */
	case WF_FTYPE_DATAi:
		switch (fstype) {
		case WF_FSTYPE_DATAONLYi:
			switch (priv->mode) {
			case ACX_MODE_3_AP:
				result = acx_process_data_frame_master(rxdesc, priv);
				break;
			case ACX_MODE_0_ADHOC:
			case ACX_MODE_2_STA:
				result = acx_process_data_frame_client(rxdesc, priv);
				break;
			}
		case WF_FSTYPE_DATA_CFACKi:
		case WF_FSTYPE_DATA_CFPOLLi:
		case WF_FSTYPE_DATA_CFACK_CFPOLLi:
		case WF_FSTYPE_CFPOLLi:
		case WF_FSTYPE_CFACK_CFPOLLi:
		/*   see above.
		   acx_process_class_frame(rxdesc, priv, 3); */
			break;
		case WF_FSTYPE_NULLi:
			acx_process_NULL_frame(rxdesc, priv, 3);
			break;
		/* FIXME: same here, see above */
		case WF_FSTYPE_CFACKi:
		default:
			break;
		}
		break;
	case WF_FTYPE_MGMTi:
		result = acx_process_mgmt_frame(rxdesc, priv);
		break;
	case WF_FTYPE_CTLi:
		if (fstype != WF_FSTYPE_PSPOLLi)
			result = NOT_OK;
		else
			result = OK;
		/*   this call is irrelevant, since
		 *   acx_process_class_frame is a stub, so return
		 *   immediately instead.
		 * return acx_process_class_frame(rxdesc, priv, 3); */
		break;
	default:
		break;
	}
	FN_EXIT1(result);
	return result;
}

/*----------------------------------------------------------------
* acx_transmit_assocresp
* STATUS: should be ok, but UNVERIFIED.
*----------------------------------------------------------------*/
static u32 acx_transmit_assocresp(const wlan_fr_assocreq_t *req,
			  wlandevice_t *priv)
{
	struct txdescriptor *tx_desc;
	struct txhostdescriptor *hdesc_head;
	struct txhostdescriptor *hdesc_body;
	struct wlan_hdr_mgmt *head;
	struct assocresp_frame_body *body;
	char *p;
	const u8 *da;
	const u8 *sa;
	const u8 *bssid;
	client_t *clt;

	static const u8 ratemask[] = { 0, 1, 3, 7, 0xf, 0x1f };

	FN_ENTER;

	acxlog(L_BINDEBUG | L_ASSOC | L_XFER, "<acx_transmit_assocresp 1>\n");

	sa = req->hdr->a3.a1;
	da = req->hdr->a3.a2;
	bssid = req->hdr->a3.a3;

	clt = acx_sta_list_get(priv, da);
	if (!clt) {
		FN_EXIT1(OK);
		return OK;
	}

	/* Assoc without auth is a big no-no */
	if (clt->used == CLIENT_EXIST_1) {
		acx_transmit_deauthen(da, clt, priv, WLAN_MGMT_REASON_CLASS2_NONAUTH);
		FN_EXIT0();
		return NOT_OK;
	}

	clt->used = CLIENT_ASSOCIATED_3;

	if (clt->aid == 0) {
		clt->aid = CurrentAID;
		CurrentAID++;
	}
	clt->cap_info = ieee2host16(*(req->listen_int));

#if THIS_IS_A_BUG /* FIXME! TODO! */
	/* Looks like wrong fields of req were used here... */
	clt->cap_info = ieee2host16(*(req->listen_int));
	memcpy(clt->ratevec, req->ssid, req->ssid->len);
	if (req->ssid->len < sizeof(ratemask)) {
		clt->ratemask = ratemask[req->ssid->len];
#else
	clt->cap_info = ieee2host16(*(req->cap_info));
	/* NB: it will crash here because clt->ratevec is NULL -
	** client is memset to 0 and never modified.
	** (in other words, we have 2 bugs here :) */
	/* memcpy(clt->ratevec, req->supp_rates, req->supp_rates->len); */
	if (req->supp_rates->len < sizeof(ratemask)) {
		clt->ratemask = ratemask[req->supp_rates->len];
#endif
	} else {
		clt->ratemask = 0x1f; /* 1,2,5,11,22 Mbit */
	}

	tx_desc = acx_get_tx_desc(priv);
	if (!tx_desc) {
		FN_EXIT1(NOT_OK);
		return NOT_OK;
	}

	hdesc_head = tx_desc->fixed_size.s.host_desc;
	hdesc_body = hdesc_head + 1;
	head = (void*)hdesc_head->data;
	body = (void*)hdesc_body->data;

	head->fc = WF_FSTYPE_ASSOCRESPi;	/* 0x10 */
	head->dur = req->hdr->a3.dur;
	MAC_COPY(head->da, da);
	MAC_COPY(head->sa, sa);
	MAC_COPY(head->bssid, bssid);
	head->seq = req->hdr->a3.seq;

	hdesc_head->length = cpu_to_le16(WLAN_HDR_A3_LEN);
	hdesc_head->data_offset = 0;

	body->cap_info = host2ieee16(priv->capabilities);
	body->status = host2ieee16(0);
	body->aid = host2ieee16(clt->aid);
	p = wlan_fill_ie_rates((char*)&body->rates, priv->rate_supported_len, priv->rate_supported);

	hdesc_body->length = cpu_to_le16(p - (char*)hdesc_body->data);
	hdesc_body->data_offset = 0;

	tx_desc->total_length = cpu_to_le16(WLAN_HDR_A3_LEN + p - (char*)hdesc_body->data);

	acx_dma_tx_data(priv, tx_desc);

	FN_EXIT1(OK);
	return OK;
}

/*----------------------------------------------------------------
* acx_transmit_reassocresp
* STATUS: should be ok, but UNVERIFIED.
*----------------------------------------------------------------*/
static u32 acx_transmit_reassocresp(const wlan_fr_reassocreq_t *req, wlandevice_t *priv)
{
	struct txdescriptor *tx_desc;
	struct txhostdescriptor *hdesc_head;
	struct txhostdescriptor *hdesc_body;
	struct wlan_hdr_mgmt *head;
	struct reassocresp_frame_body *body;
	char *p;
	const u8 *da;
	const u8 *sa;
	const u8 *bssid;
	client_t *clt;

	FN_ENTER;

	sa = req->hdr->a3.a1;
	da = req->hdr->a3.a2;
	bssid = req->hdr->a3.a3;

	clt = acx_sta_list_get_or_add(priv, da);
	if (clt && clt->used == CLIENT_EXIST_1)
		clt->used = CLIENT_AUTHENTICATED_2;
	else {
		FN_EXIT1(OK);
		return OK;
	}

	/* huh. why do we set first AUTH, then ASSOC? */
	clt->used = CLIENT_ASSOCIATED_3;
	if (clt->aid == 0) {
		clt->aid = CurrentAID;
		CurrentAID++;
	}
	clt->cap_info = ieee2host16(*(req->cap_info));

	/* NB: it will crash here because clt->ratevec is NULL -
	** client is memset to 0 and never modified */
	/* memcpy(clt->ratevec, req->supp_rates, req->supp_rates->len); */

	switch (req->supp_rates->len) {
	case 1:
		clt->ratemask = 1;	/* 1 Mbit only */
		break;
	case 2:
		clt->ratemask = 3;	/* 1,2 Mbits */
		break;
	case 3:
		clt->ratemask = 7;	/*  1,2,5 Mbits */
		break;
	case 4:
		clt->ratemask = 0xf;	/* 1,2,5,11 Mbits */
		break;
	default:
		clt->ratemask = 0x1f;	/* 1,2,5,11,22 Mbits */
		break;
	}

	tx_desc = acx_get_tx_desc(priv);
	if (!tx_desc) {
		FN_EXIT1(OK);
		return OK;
	}

	hdesc_head = tx_desc->fixed_size.s.host_desc;
	hdesc_body = hdesc_head + 1;
	head = (void*)hdesc_head->data;
	body = (void*)hdesc_body->data;

	head->fc = WF_FSTYPE_REASSOCRESPi;	/* 0x30 */
	head->dur = req->hdr->a3.dur;
	MAC_COPY(head->da, da);
	MAC_COPY(head->sa, sa);
	MAC_COPY(head->bssid, bssid);
	head->seq = req->hdr->a3.seq;

	hdesc_head->length = cpu_to_le16(WLAN_HDR_A3_LEN);
	hdesc_head->data_offset = 0;

	body->cap_info = host2ieee16(priv->capabilities);
	body->status = host2ieee16(0);
	body->aid = host2ieee16(clt->aid);
	p = wlan_fill_ie_rates((char*)&body->rates, priv->rate_supported_len, priv->rate_supported);

	hdesc_body->length = cpu_to_le16(p - (char*)hdesc_body->data);
	hdesc_body->data_offset = 0;

	tx_desc->total_length = cpu_to_le16(WLAN_HDR_A3_LEN + p - (char*)hdesc_body->data);

	acx_dma_tx_data(priv, tx_desc);

	FN_EXIT1(OK);
	return OK;
}

/*----------------------------------------------------------------
* acx_process_disassoc
* STATUS: UNVERIFIED.
*----------------------------------------------------------------*/
static int acx_process_disassoc(const wlan_fr_disassoc_t *pkt, wlandevice_t *priv)
{
	const p80211_hdr_t *hdr;
	int res = 0;
	const u8 *TA;
	client_t *clt;

	FN_ENTER;

	hdr = pkt->hdr;
	TA = hdr->a3.a2;
	clt = acx_sta_list_get(priv, TA);
	if (!clt) {
		res = 1;
		goto ret;
	}
	if (clt->used == CLIENT_EXIST_1) {
		/* he's disassociating, but he's
		** not even authenticated! go away... */
		acxlog(L_BINDEBUG | L_ASSOC | L_XFER, "<transmit_deauth 2>\n");
		acx_transmit_deauthen(TA, clt, priv, WLAN_MGMT_REASON_CLASS2_NONAUTH);
	} else
		clt->used = CLIENT_AUTHENTICATED_2;
ret:
	FN_EXIT1(res);
	return res;
}

/*----------------------------------------------------------------
* acx_process_disassociate
* STATUS: UNVERIFIED.
*----------------------------------------------------------------*/
static int acx_process_disassociate(const wlan_fr_disassoc_t *req, wlandevice_t *priv)
{
	const p80211_hdr_t *hdr;
	int res = 0;

	FN_ENTER;
	hdr = req->hdr;

	if (ACX_MODE_2_STA != priv->mode) {
		res = NOT_OK;
		goto end;
	}
	if (mac_is_equal(priv->dev_addr, hdr->a3.a1 /* RA */)) {
		if (priv->status > ACX_STATUS_3_AUTHENTICATED) {
			/* priv->val0x240 = ieee2host16(*(req->reason)); Unused, so removed */
			acx_set_status(priv, ACX_STATUS_3_AUTHENTICATED);
#if (POWER_SAVE_80211 == 0)
			ActivatePowerSaveMode(priv, 2);
#endif
		}
		res = OK;
	} else
		res = NOT_OK;
end:
	FN_EXIT1(res);
	return res;
}

/*----------------------------------------------------------------
* acx_process_data_frame_master
*----------------------------------------------------------------*/
static int acx_process_data_frame_master(struct rxhostdescriptor *rxdesc, wlandevice_t *priv)
{
	const client_t *clt, *var_24 = NULL;
	p80211_hdr_t* p80211_hdr;
	struct txdescriptor *tx_desc;
	u8 esi = 0;
	int result = NOT_OK;
	const u8 *da, *sa, *bssid;

	FN_ENTER;

        p80211_hdr = acx_get_p80211_hdr(priv, rxdesc);

	switch (WF_FC_FROMTODSi & p80211_hdr->a3.fc) {
	case 0:
		da = p80211_hdr->a3.a1;
		sa = p80211_hdr->a3.a2;
		bssid = p80211_hdr->a3.a3;
		break;
	case WF_FC_FROMDSi:
		da = p80211_hdr->a3.a1;
		sa = p80211_hdr->a3.a3;
		bssid = p80211_hdr->a3.a2;
		break;
	case WF_FC_TODSi:
		da = p80211_hdr->a3.a3;
		sa = p80211_hdr->a3.a2;
		bssid = p80211_hdr->a3.a1;
		break;
	default: /* WF_FC_FROMTODSi */
		acxlog(L_DEBUG, "WDS frame received. Unimplemented\n");
		goto done;
	}

	/* check if it is our BSSID, if not, leave */
	if (!mac_is_equal(bssid, priv->bssid)) {
		goto done;
	}

	/* huh. bcast client? --vda*/
	/* total mess. supposed to be the part where AP passes packets
	** from one STA to the other */
	clt = acx_sta_list_get(priv, bcast_addr);
	if (!clt || (clt->used != CLIENT_ASSOCIATED_3)) {
		acx_transmit_deauthen(bcast_addr, 0, priv, WLAN_MGMT_REASON_RSVD /* 0 */);
		acxlog(L_STD, "frame error #2??\n");
		priv->stats.rx_errors++;
		goto fail;
	} else {
		esi = 2;
		/* check if the da is not broadcast */
		if (!mac_is_bcast(da)) {
			if ((signed char) da[0x0] >= 0) {
				esi = 0;
				var_24 = acx_sta_list_get(priv, da);
				if (!var_24) {
					goto station_not_found;
				}
				if (var_24->used != 0x3) {
					goto fail;
				}
			} else {
				esi = 1;
			}
		}
		if (var_24 == NULL) {
		      station_not_found:
			if (esi == 0) {
				acx_rx(rxdesc, priv);
				result = NOT_OK;
				goto fail;
			}
		}
		if ((esi == 0) || (esi == 2)) {
			/* repackage, tx, and hope it someday reaches its destination */
			MAC_COPY(p80211_hdr->a3.a1, da);
			MAC_COPY(p80211_hdr->a3.a2, bssid);
			MAC_COPY(p80211_hdr->a3.a3, sa);
			/* To_DS = 0, From_DS = 1 */
			p80211_hdr->a3.fc = WF_FC_FROMDSi + WF_FTYPE_DATAi;

			tx_desc = acx_get_tx_desc(priv);
			if (!tx_desc) {
				return NOT_OK;
			}

			acx_rxdesc_to_txdesc(rxdesc, tx_desc);
			acx_dma_tx_data(priv, tx_desc);

			if (esi != 2) {
				goto done;
			}
		}
		acx_rx(rxdesc, priv);
	}
done:
	result = OK;
fail:
	FN_EXIT1(result);
	return result;
}

/*----------------------------------------------------------------
* acx_process_data_frame_client
* STATUS: FINISHED, UNVERIFIED.
*----------------------------------------------------------------*/
static int acx_process_data_frame_client(struct rxhostdescriptor *rxdesc, wlandevice_t *priv)
{
	const u8 *da, *bssid;
	const p80211_hdr_t *p80211_hdr;
	netdevice_t *dev = priv->netdev;
	int result = NOT_OK;

	FN_ENTER;

	if (ACX_STATUS_4_ASSOCIATED != priv->status) goto drop;

	p80211_hdr = acx_get_p80211_hdr(priv, rxdesc);

	switch (WF_FC_FROMTODSi & p80211_hdr->a3.fc) {
	case 0:	/* ad-hoc data frame */
		da = p80211_hdr->a3.a1;
		bssid = p80211_hdr->a3.a3;
		break;
	case WF_FC_FROMDSi: /* ap->sta */
		da = p80211_hdr->a3.a1;
		bssid = p80211_hdr->a3.a2;
		break;
	case WF_FC_TODSi: /* sta->ap - huh? shouldn't happen... */
		da = p80211_hdr->a3.a3;
		bssid = p80211_hdr->a3.a1;
		break;
	default: /* WF_FC_FROMTODSi: wds->wds */
		acxlog(L_DEBUG, "WDS frame received. Unimplemented\n");
		goto drop;
	}

	if (unlikely(debug & L_DEBUG)) {
		printk("rx: da ");
		acx_log_mac_address(L_DEBUG, da, ",bssid ");
		acx_log_mac_address(L_DEBUG, bssid, ",priv->bssid ");
		acx_log_mac_address(L_DEBUG, priv->bssid, ",dev_addr ");
		acx_log_mac_address(L_DEBUG, priv->dev_addr, ",bcast_addr ");
		acx_log_mac_address(L_DEBUG, bcast_addr, "\n");
	}

	/* promiscuous mode --> receive all packets */
	if (unlikely(dev->flags & IFF_PROMISC))
		goto process;

	/* FIRST, check if it is our BSSID */
        if (!mac_is_equal(priv->bssid, bssid)) {
		/* is not our BSSID, so bail out */
		goto drop;
	}

	/* then, check if it is our address */
	if (mac_is_equal(da, priv->dev_addr)) {
		goto process;
	}

	/* then, check if it is broadcast */
	if (mac_is_bcast(da)) {
		goto process;
	}
	
	if (mac_is_mcast(da)) {
		/* unconditionally receive all multicasts */
		if (dev->flags & IFF_ALLMULTI)
			goto process;

		/* FIXME: check against the list of
		 * multicast addresses that are configured
		 * for the interface (ifconfig) */
		acxlog(L_XFER, "FIXME: multicast packet, need to check against a list of multicast addresses (to be created!); accepting packet for now\n");
		/* for now, just accept it here */
		goto process;
	}

	acxlog(L_DEBUG, "Rx foreign packet, dropping\n");
	goto drop;
process:
	/* receive packet */
	acx_rx(rxdesc, priv);

	result = OK;
drop:
	FN_EXIT1(result);
	return result;
}

/*----------------------------------------------------------------
* acx_process_mgmt_frame
* STATUS: FINISHED, UNVERIFIED. namechange!! (from process_mgnt_frame())
*
* Theory of operation: mgmt packet gets parsed (to make it easy
* to access variable-sized IEs), results stored in 'parsed'.
* Then we react to the packet.
* NB: acx_mgmt_decode_XXX are dev-independent (shoudnt have been named acx_XXX)
*----------------------------------------------------------------*/
typedef union parsed_mgmt_req {
	wlan_fr_mgmt_t mgmt;
	wlan_fr_assocreq_t assocreq;
	wlan_fr_reassocreq_t reassocreq;
	wlan_fr_assocresp_t assocresp;
	wlan_fr_reassocresp_t reassocresp;
	wlan_fr_beacon_t beacon;
	wlan_fr_disassoc_t disassoc;
	wlan_fr_authen_t authen;
	wlan_fr_deauthen_t deauthen;
	wlan_fr_proberesp_t proberesp;
} parsed_mgmt_req_t;
extern void bug_excessive_stack_usage(void);

static u32 acx_process_mgmt_frame(struct rxhostdescriptor *rxdesc, wlandevice_t *priv)
{
	parsed_mgmt_req_t parsed;	/* takes ~100 bytes of stack */
	p80211_hdr_t *hdr;
	int adhoc,sta_scan,sta,ap;
	int len;
	
	if(sizeof(parsed) > 256) bug_excessive_stack_usage();

	FN_ENTER;

	hdr = acx_get_p80211_hdr(priv, rxdesc);

	/* Management frames never have these set */
	if (WF_FC_FROMTODSi & hdr->a3.fc) {
		FN_EXIT1(NOT_OK);
		return NOT_OK;
	}

	len = MAC_CNT_RCVD(rxdesc->data);
	if (WF_FC_ISWEPi & hdr->a3.fc)
		len -= 0x10;

	adhoc = (priv->mode == ACX_MODE_0_ADHOC);
	sta_scan = ((priv->mode == ACX_MODE_2_STA)
	         && (priv->status != ACX_STATUS_4_ASSOCIATED));
	sta = ((priv->mode == ACX_MODE_2_STA)
	    && (priv->status == ACX_STATUS_4_ASSOCIATED));
	ap = (priv->mode == ACX_MODE_3_AP);

	switch (WF_FC_FSTYPEi & hdr->a3.fc) {
	/* beacons first, for speed */
	case WF_FSTYPE_BEACONi:
		if (!adhoc && !sta_scan) {
			acxlog(L_DEBUG,
			       "Incoming beacon message not handled in mode %d\n",
			       priv->mode);
			break;
		}
		/* BTW, it's not such a bad idea to always receive
		** beacons and keep info about nearby stations
		** (used for automatic WDS links, etc...) */
		switch (priv->status) {
		case ACX_STATUS_1_SCANNING:
			memset(&parsed.beacon, 0, sizeof(parsed.beacon));
			parsed.beacon.buf = (void*)hdr;
			parsed.beacon.len = len;
			if (debug & L_DATA) {
				printk("BCN len:%d fc:%04x dur:%04x seq:%04x\n",
				       len, hdr->a3.fc, hdr->a3.dur, hdr->a3.seq);
				printk("BCN a1: "MACSTR"\n", MAC(hdr->a3.a1));
				printk("BCN a2: "MACSTR"\n", MAC(hdr->a3.a2));
				printk("BCN a3: "MACSTR"\n", MAC(hdr->a3.a3));
			}
			acx_mgmt_decode_beacon(&parsed.beacon);
			/* beacon and probe response are very similar, so... */
			acx_process_probe_response(&parsed.beacon, rxdesc->data, priv);
			break;
		default:
			/* acxlog(L_ASSOC | L_DEBUG,
			   "Incoming beacon message not handled during status %i.\n",
			   priv->status); */
			break;
		}
		break;
	case WF_FSTYPE_ASSOCREQi:
		if (!ap)
			break;
		memset(&parsed.assocreq, 0, sizeof(parsed.assocreq));
		parsed.assocreq.buf = (void*)hdr;
		parsed.assocreq.len = len;
		acx_mgmt_decode_assocreq(&parsed.assocreq);
		if (mac_is_equal(hdr->a3.a2, priv->bssid)) {
			acx_transmit_assocresp(&parsed.assocreq, priv);
		}
		break;
	case WF_FSTYPE_REASSOCREQi:
		if (!ap)
			break;
		/* NB: was clearing 4 extra bytes (bug?) */
		memset(&parsed.assocreq, 0, sizeof(parsed.assocreq));
		parsed.assocreq.buf = (void*)hdr;
		parsed.assocreq.len = len;
		acx_mgmt_decode_assocreq(&parsed.assocreq);
		/* reassocreq and assocreq are equivalent */
		acx_transmit_reassocresp(&parsed.reassocreq, priv);
		break;
	case WF_FSTYPE_ASSOCRESPi:
		if (!sta_scan)
			break;
		memset(&parsed.assocresp, 0, sizeof(parsed.assocresp));
		parsed.assocresp.buf = (void*)hdr;
		parsed.assocresp.len = len;
		acx_mgmt_decode_assocresp(&parsed.assocresp);
		acx_process_assocresp(&parsed.assocresp, priv);
		break;
	case WF_FSTYPE_REASSOCRESPi:
		if (!sta_scan)
			break;
		memset(&parsed.assocresp, 0, sizeof(parsed.assocresp));
		parsed.assocresp.buf = (void*)hdr;
		parsed.assocresp.len = len;
		acx_mgmt_decode_assocresp(&parsed.assocresp);
		acx_process_reassocresp(&parsed.reassocresp, priv);
		break;
	case WF_FSTYPE_PROBEREQi:
		if (ap) {
			acxlog(L_ASSOC, "FIXME: since we're supposed to be "
				"an AP, we need to return a "
				"Probe Response packet!\n");
		}
		break;
	case WF_FSTYPE_PROBERESPi:
		if (priv->status != ACX_STATUS_1_SCANNING)
			break;
		memset(&parsed.proberesp, 0, sizeof(parsed.proberesp));
		parsed.proberesp.buf = (void*)hdr;
		parsed.proberesp.len = len;
		acx_mgmt_decode_proberesp(&parsed.proberesp);
		acx_process_probe_response(&parsed.proberesp, rxdesc->data, priv);
		break;
	case 6:
	case 7:
		/* exit */
		break;
	case WF_FSTYPE_ATIMi:
		/* exit */
		break;
	case WF_FSTYPE_DISASSOCi:
		if (!sta && !ap)
			break;
		memset(&parsed.disassoc, 0, sizeof(parsed.disassoc));
		parsed.disassoc.buf = (void*)hdr;
		parsed.disassoc.len = len;
		acx_mgmt_decode_disassoc(&parsed.disassoc);
/* vda: FIXME disassociate/disassoc ?! */
		if (ACX_MODE_3_AP != priv->mode) {
			acx_process_disassoc(&parsed.disassoc, priv);
		}
		else
		if ((ACX_MODE_0_ADHOC == priv->mode)
		 || (ACX_MODE_2_STA == priv->mode)) {
			acx_process_disassociate(&parsed.disassoc, priv);
		}
		break;
	case WF_FSTYPE_AUTHENi:
		if (!sta_scan && !ap)
			break;
		memset(&parsed.authen, 0, sizeof(parsed.authen));
		parsed.authen.buf = (void*)hdr;
		parsed.authen.len = len;
		acx_mgmt_decode_authen(&parsed.authen);
		if (mac_is_equal(priv->bssid, hdr->a3.a2)) {
			acx_process_authen(&parsed.authen, priv);
		}
		break;
	case WF_FSTYPE_DEAUTHENi:
		if (!sta && !ap)
			break;
		memset(&parsed.deauthen, 0, sizeof(parsed.deauthen));
		parsed.deauthen.buf = (void*)hdr;
		parsed.deauthen.len = len;
		acx_mgmt_decode_deauthen(&parsed.deauthen);
		/* FIXME: this check is buggy: it should be ==,
		 * but then our complete deauthen handling would have to be
		 * changed, so better leave it at that and make sure to adapt
		 * our driver to generic Linux 802.11 stack ASAP! */
/* vda: FIXME deauthenticate/deauthen ?! */
		if (ACX_MODE_3_AP != priv->mode) {
			acx_process_deauthen(&parsed.deauthen, priv);
		}
		else
		if ((ACX_MODE_0_ADHOC == priv->mode)
		 || (ACX_MODE_2_STA == priv->mode)) {
			acx_process_deauthenticate(&parsed.deauthen, priv);
		}
		break;
	}

	FN_EXIT1(OK);
	return OK;
}

#if UNUSED
/*----------------------------------------------------------------
* acx_process_class_frame
*
* Arguments:
*
* Returns:
*
* Side effects:
*
* Call context:
*
* STATUS:
*
* Comment:
*
*----------------------------------------------------------------*/


/* acx_process_class_frame()
 * STATUS: FINISHED.
 */
static int acx_process_class_frame(struct rxhostdescriptor *rxdesc, wlandevice_t *priv, int vala)
{
	return OK;
}
#endif

/*----------------------------------------------------------------
* acx_process_NULL_frame
*
* Arguments:
*
* Returns:
*
* Side effects:
*
* Call context:
*
* STATUS:
*
* Comment:
*   Processing of Null Function ("nullfunc") frames.
*
*----------------------------------------------------------------*/

/* acx_process_NULL_frame()
 * STATUS: FINISHED, UNVERIFIED.
 */
static int acx_process_NULL_frame(struct rxhostdescriptor *rxdesc, wlandevice_t *priv, int vala)
{
	const signed char *esi;
	const u8 *ebx;
	const p80211_hdr_t *p80211_hdr;
	const client_t *client;
	int result = NOT_OK;

	p80211_hdr = acx_get_p80211_hdr(priv, rxdesc);
		
	switch (WF_FC_FROMTODSi & p80211_hdr->a3.fc) {
	case 0:
		esi = p80211_hdr->a3.a1;
		ebx = p80211_hdr->a3.a2;
		break;
	case WF_FC_FROMDSi:
		esi = p80211_hdr->a3.a1;
		ebx = p80211_hdr->a3.a3;
		break;
	case WF_FC_TODSi:
		esi = p80211_hdr->a3.a1;
		ebx = p80211_hdr->a3.a2;
		break;
	default: /* WF_FC_FROMTODSi */
		esi = p80211_hdr->a3.a1; /* added by me! --vda */
		ebx = p80211_hdr->a3.a2;
	}

	if (esi[0x0] < 0) {
		result = OK;
		goto done;
	}

	client = acx_sta_list_get(priv, ebx);
	if (client)
		result = NOT_OK;
	else {
#if IS_IT_BROKEN
		acxlog(L_BINDEBUG | L_XFER, "<transmit_deauth 7>\n");
		acx_transmit_deauthen(ebx, 0x0, priv, WLAN_MGMT_REASON_CLASS2_NONAUTH /* 6 */);
#else
		acxlog(L_STD, "received NULL frame from unknown client! We really shouldn't send deauthen here, right?\n");
#endif
		result = OK;
	}
done:
	return result;
}

/*----------------------------------------------------------------
* acx_process_probe_response
*
* Arguments:
*
* Returns:
*
* Side effects:
*
* Call context:
*
* STATUS: working on it.  UNVERIFIED.
*
* Comment:
*
*----------------------------------------------------------------*/
static void acx_process_probe_response(wlan_fr_proberesp_t *req, const struct rxbuffer *mmt, wlandevice_t *priv)
{
	struct bss_info *newbss;
	p80211_hdr_t *hdr;
	unsigned int station;
	u8 rate_count;
	unsigned int i, max_rate = 0;

	FN_ENTER;

	acxlog(L_ASSOC, "previous bss_table_count: %u\n", priv->bss_table_count);

	hdr = req->hdr;

	/* uh oh, we found more sites/stations than we can handle with
	 * our current setup: pull the emergency brake and stop scanning! */
	if (priv->bss_table_count > MAX_NUMBER_OF_SITE) {
		acx_issue_cmd(priv, ACX1xx_CMD_STOP_SCAN, NULL, 0, ACX_CMD_TIMEOUT_DEFAULT);
		acx_set_status(priv, ACX_STATUS_2_WAIT_AUTH);

		acxlog(L_BINDEBUG | L_ASSOC,
		       "<Scan Beacon> bss_table_count > MAX_NUMBER_OF_SITE\n");
		FN_EXIT0();
		return;
	}

	if (mac_is_equal(hdr->a3.a3, priv->dev_addr)) {
		acxlog(L_ASSOC, "huh, scan found our own MAC!?\n");
		FN_EXIT0();
		return; /* just skip this one silently */
	}
			
	/* filter out duplicate stations we already registered in our list */
	for (station = 0; station < priv->bss_table_count; station++) {
		u8 *a = priv->bss_table[station].bssid;
		acxlog(L_DEBUG,
			"checking station %u ["MACSTR"]\n",
			station, MAC(a));
		if (mac_is_equal(hdr->a3.a3, priv->bss_table[station].bssid)) {
			acxlog(L_DEBUG,
			       "station already in our list, no need to add\n");
			FN_EXIT0();
			return;
		}
	}

	newbss = &priv->bss_table[priv->bss_table_count];

	/* Let's completely zero out the entry that we're
	 * going to fill next in order to not risk any corruption. */
	memset(newbss, 0, sizeof(struct bss_info));

	/* copy the BSSID element */
	MAC_COPY(newbss->bssid, hdr->a3.a3);
	/* copy the MAC address element (source address) */
	MAC_COPY(newbss->mac_addr, hdr->a3.a2);

	/* copy the ESSID element */
	if (req->ssid->len <= IW_ESSID_MAX_SIZE) {
		newbss->essid_len = req->ssid->len;
		memcpy(newbss->essid, req->ssid->ssid, req->ssid->len);
		newbss->essid[req->ssid->len] = '\0';
	}
	else {
		acxlog(L_STD, "huh, ESSID overflow in scanned station data?\n");
	}

	newbss->channel = req->ds_parms->curr_ch;
	newbss->wep = (ieee2host16(*req->cap_info) & WF_MGMT_CAP_PRIVACY);
	newbss->caps = ieee2host16(*req->cap_info);

	rate_count = req->supp_rates->len;
	if (rate_count >= sizeof(req->supp_rates->rates))
		rate_count = sizeof(req->supp_rates->rates) - 1;
	memcpy(newbss->supp_rates, req->supp_rates->rates, rate_count);
	newbss->supp_rates[rate_count] = '\0';
	/* now, one just uses strlen() on it to know how many rates are there */
 
	newbss->sir = acx_signal_to_winlevel(mmt->phy_level);
	newbss->snr = acx_signal_to_winlevel(mmt->phy_snr);

	/* find max. transfer rate and do optional debug log */
	acxlog(L_DEBUG, "Peer - Supported Rates:\n");
	for (i=0; i < rate_count; i++) {
		int rate = req->supp_rates->rates[i];
		acxlog(L_DEBUG, "%s Rate: %d%sMbps (0x%02X)\n",
			(rate & 0x80) ? "Basic" : "Operational",
			(rate & ~0x80) / 2, (rate & 1) ? ".5" : "", rate);
		if ((rate & ~0x80) > max_rate)
			max_rate = rate & ~0x80;
	}

	acxlog(L_STD | L_ASSOC,
		"found and registered station %u: ESSID \"%s\" on channel %d, "
		"BSSID "MACSTR", %s/%d%sMbps, caps 0x%04x, SIR %d, SNR %d\n",
		priv->bss_table_count, newbss->essid, newbss->channel,
		MAC(newbss->bssid),
        	(newbss->caps & WF_MGMT_CAP_IBSS) ? "Ad-Hoc peer" : "Access Point",
		max_rate / 2, (max_rate & 1) ? ".5" : "",
		newbss->caps, newbss->sir, newbss->snr);

	/* found one station --> increment counter */
	priv->bss_table_count++;
	FN_EXIT0();
}

static const char * const status_str[] = {
  "Successful", "Unspecified failure",
  "Reserved error code", "Reserved error code", "Reserved error code",
  "Reserved error code", "Reserved error code", "Reserved error code",
  "Reserved error code", "Reserved error code",
  "Cannot support all requested capabilities in the Capability Information field. TRANSLATION: Bug in ACX100 driver?",
  "Reassociation denied due to reason outside the scope of 802.11b standard. TRANSLATION: Bug in ACX100 driver?",
  "Association denied due to reason outside the scope of 802.11b standard. TRANSLATION: peer station perhaps has MAC filtering enabled, FIX IT!",
  "Responding station does not support the specified authentication algorithm. TRANSLATION: invalid network data or bug in ACX100 driver?",
  "Received an Authentication frame with transaction sequence number out of expected sequence. TRANSLATION: Bug in ACX100 driver?",
  "Authentication rejected because of challenge failure. TRANSLATION: Bug in ACX100 driver?",
  "Authentication rejected due to timeout waiting for next frame in sequence. TRANSLATION: Bug in ACX100 driver?",
  "Association denied because AP is unable to handle additional associated stations",
  "Association denied due to requesting station not supporting all of the data rates in the BSSBasicRateSet parameter. TRANSLATION: peer station has an incompatible set of data rates configured, FIX IT!",
  "Association denied due to requesting station not supporting the Short Preamble option. TRANSLATION: Bug in ACX100 driver?",
  "Association denied due to requesting station not supporting the PBCC Modulation option. TRANSLATION: Bug in ACX100 driver?",
  "Association denied due to requesting station not supporting the Channel Agility option. TRANSLATION: Bug in ACX100 driver?"
};
static inline const char * const get_status_string(int status)
{
	unsigned int idx = status < VEC_SIZE(status_str) ? status : 2;
	return status_str[idx];
}

/*----------------------------------------------------------------
* acx_process_assocresp
* STATUS: should be ok, UNVERIFIED.
*----------------------------------------------------------------*/
static int acx_process_assocresp(const wlan_fr_assocresp_t *req, wlandevice_t *priv)
{
	const p80211_hdr_t *hdr;
	int res = NOT_OK;
/*	acx_ie_generic_t pdr; */

	FN_ENTER;
	hdr = req->hdr;

	if (mac_is_equal(priv->dev_addr, hdr->a3.a1 /* RA */)) {
		if (WLAN_MGMT_STATUS_SUCCESS == ieee2host16(*(req->status))) {
			priv->aid = ieee2host16(*(req->aid)); /* FIXME: huh?? this didn't get set, or did it? */
			/* tell the card we are associated when we are out of interrupt context */
			acx_schedule_after_interrupt_task(priv, ACX_AFTER_IRQ_CMD_ASSOCIATE);
		}
		else {
			acxlog(L_STD | L_ASSOC, "Association FAILED: peer station sent response status code %d: \"%s\"!\n", ieee2host16(*(req->status)), get_status_string(ieee2host16(*(req->status))));
		}
		res = NOT_OK;
	} else
		res = OK;
	
	FN_EXIT1(res);
	return res;
}

/*----------------------------------------------------------------
* acx_process_reassocresp
* STATUS: should be ok, UNVERIFIED.
*----------------------------------------------------------------*/
static int acx_process_reassocresp(const wlan_fr_reassocresp_t *req, wlandevice_t *priv)
{
	const p80211_hdr_t *hdr = req->hdr;
	int result = 4;
	u16 status = ieee2host16(*(req->status));

	FN_ENTER;

	if (!mac_is_equal(priv->dev_addr, hdr->a3.a1)) {
		result = 3;
		goto end;
	}
	if (status == WLAN_MGMT_STATUS_SUCCESS) {
		acx_set_status(priv, ACX_STATUS_4_ASSOCIATED);
	} else {
		acxlog(L_STD | L_ASSOC, "Reassociation FAILED: response "
			"status code %d: \"%s\"!\n",
			status, get_status_string(status));
	}
	result = 0;
end:
	FN_EXIT1(result);
	return result;
}

/*----------------------------------------------------------------
 * acx_process_authen()
 * STATUS: FINISHED, UNVERIFIED.
 * Called only in AP mode
 *----------------------------------------------------------------*/
static int acx_process_authen(const wlan_fr_authen_t *req, wlandevice_t *priv)
{
	const p80211_hdr_t *hdr;
	client_t *clt;
	u16 alg,seq,status;
	int result = NOT_OK;

	FN_ENTER;

	/* Does this ever happens? */
	if (!priv) {
		result = NOT_OK;
		goto end;
	}

	hdr = req->hdr;
	alg = ieee2host16(*(req->auth_alg));
	seq = ieee2host16(*(req->auth_seq));
	status = ieee2host16(*(req->status));

	acx_log_mac_address(L_ASSOC, priv->dev_addr, " ");
	acx_log_mac_address(L_ASSOC, hdr->a3.a1, " ");
	acx_log_mac_address(L_ASSOC, hdr->a3.a2, " ");
	acx_log_mac_address(L_ASSOC, hdr->a3.a3, " ");
	acx_log_mac_address(L_ASSOC, priv->bssid, "\n");
	
	if ((!mac_is_equal(priv->dev_addr, hdr->a3.a1))
	 && (!mac_is_equal(priv->bssid, hdr->a3.a1))
	) {
		result = OK;
		goto end;
	}

	if (priv->auth_alg <= 1) {
		if (priv->auth_alg != alg) {
			acxlog(L_ASSOC, "authentication algorithm mismatch: "
				"want: %d, req: %d\n", priv->auth_alg,alg);
			result = NOT_OK;
			goto end;
		}
	}
	acxlog(L_ASSOC,"Algorithm is ok\n");

	/* I think this must be done in AP mode only */
	clt = acx_sta_list_get_or_add(priv, hdr->a3.a2);
	if (!clt) {
		acxlog(L_ASSOC, "Could not allocate room for this client\n");
		result = NOT_OK;
		goto end;
	}

	/* now check which step in the authentication sequence we are
	 * currently in, and act accordingly */
	acxlog(L_ASSOC, "acx_process_authen auth seq step %d\n",seq);
	switch (seq) {
	case 1:
		if (ACX_MODE_3_AP != priv->mode)
			break;
		acx_transmit_authen2(req, clt, priv);
		break;
	case 2:
		if (ACX_MODE_2_STA != priv->mode)
			break;
		if (status == WLAN_MGMT_STATUS_SUCCESS) {
			if (alg == WLAN_AUTH_ALG_OPENSYSTEM) {
				acx_set_status(priv, ACX_STATUS_3_AUTHENTICATED);
				acx_transmit_assoc_req(priv);
			} else
			if (alg == WLAN_AUTH_ALG_SHAREDKEY) {
				acx_transmit_authen3(req, priv);
			}
		} else {
			acxlog(L_ASSOC, "Authentication FAILED (status "
				"code %d: \"%s\"), still waiting for "
				"authentication\n",
				status,	get_status_string(status));
			acx_set_status(priv, ACX_STATUS_2_WAIT_AUTH);
		}
		break;
	case 3:
		if ((ACX_MODE_3_AP != priv->mode)
		    || (clt->auth_alg != WLAN_AUTH_ALG_SHAREDKEY)
		    || (alg != WLAN_AUTH_ALG_SHAREDKEY)
		    || (clt->auth_step != 2))
			break;
		/* acxlog(L_STD,
		       "FIXME: TODO: huh??? incompatible data type!\n");
		currclt = (client_t *)req->challenge;
		if (0 == memcmp(currclt->address, clt->challenge_text, 0x80)
		    && ( (currclt->aid & 0xff) != 0x10 )
		    && ( (currclt->aid >> 8) != 0x80 ))...
		*/
		
		/* NB Andreas: variable 'currclnt' got aliased by compiler here.
		   currclt's stack slot was reused for entirely different thing!
		   also memcmp check and &&'s seem to be wrong   -- vda */
		{
		wlan_ie_challenge_t *chal = req->challenge;
		if (memcmp(chal->challenge, clt->challenge_text, WLAN_CHALLENGE_LEN)
		 || (chal->eid != WLAN_EID_CHALLENGE)
		 || (chal->len != WLAN_CHALLENGE_LEN)
		)
			break;
		}
		acx_transmit_authen4(req, priv);
		MAC_COPY(clt->address, hdr->a3.a2);
		clt->used = CLIENT_AUTHENTICATED_2;
		clt->auth_step = 4;
		clt->seq = ieee2host16(hdr->a3.seq);
		break;
	case 4:
		if (ACX_MODE_2_STA != priv->mode)
			break;
		/* ok, we're through: we're authenticated. Woohoo!! */
		acx_set_status(priv, ACX_STATUS_3_AUTHENTICATED);
		acxlog(L_BINSTD | L_ASSOC, "Authenticated!\n");
		/* now that we're authenticated, request association */
		acx_transmit_assoc_req(priv);
		break;
	}
	result = NOT_OK;
end:
	FN_EXIT1(result);
	return result;
}

/*----------------------------------------------------------------
* acx_process_deauthen
*
* Arguments:
*
* Returns:
*	OK on all is good
*	NOT_OK on any error
* STATUS: FINISHED, UNVERIFIED.
*----------------------------------------------------------------*/
static int acx_process_deauthen(const wlan_fr_deauthen_t *pkt, wlandevice_t *priv)
{
	const p80211_hdr_t *hdr;
	int result;
	const u8 *addr;
	client_t *client;

	FN_ENTER;

	hdr = pkt->hdr;

	acxlog(L_ASSOC, "DEAUTHEN ");
	acx_log_mac_address(L_ASSOC, priv->dev_addr, " ");
	acx_log_mac_address(L_ASSOC, hdr->a3.a1, " ");
	acx_log_mac_address(L_ASSOC, hdr->a3.a2, " ");
	acx_log_mac_address(L_ASSOC, hdr->a3.a3, " ");
	acx_log_mac_address(L_ASSOC, priv->bssid, "\n");
	
	if ((!mac_is_equal(priv->dev_addr, hdr->a3.a1))
	 && (!mac_is_equal(priv->bssid, hdr->a3.a1))
	) {
		result = NOT_OK;
		goto end;
	}
	
	acxlog(L_STD, "Processing deauthen packet. Hmm, should this have happened?\n");

	addr = hdr->a3.a2;
	if (!mac_is_equal(addr, priv->dev_addr)) {
		/* OK, we've been asked to leave the ESS. Do we 
		 * ask to return or do we leave quietly? I'm 
		 * guessing that since we are still up and 
		 * running we should attempt to rejoin, from the 
		 * starting point. So:
		 */
		acx_set_status(priv,ACX_STATUS_2_WAIT_AUTH);
		result = OK;
		goto end;
	}			

	client = acx_sta_list_get(priv, addr);
	if (!client) {
		result = NOT_OK;
		goto end;
	}
	client->used = CLIENT_EXIST_1;
	result = OK;
end:
	FN_EXIT1(result);
	return result;
}

/*----------------------------------------------------------------
* acx_process_deauthenticate
* STATUS: FINISHED, UNVERIFIED.
*----------------------------------------------------------------*/
static int acx_process_deauthenticate(const wlan_fr_deauthen_t *req, wlandevice_t *priv)
{
	const p80211_hdr_t *hdr;

	FN_ENTER;
	acxlog(L_STD, "processing deauthenticate packet. Hmm, should this have happened?\n");

	hdr = req->hdr;
	if (mac_is_equal(priv->dev_addr, hdr->a3.a1)) {
		if (priv->status > ACX_STATUS_2_WAIT_AUTH) {
			acx_set_status(priv, ACX_STATUS_2_WAIT_AUTH);
		}
	}
	FN_EXIT1(OK);
	return OK;
}

/*----------------------------------------------------------------
* acx_get_random
*
* Arguments:
*
* Returns:
*
* Side effects:
*
* Call context:
*
* STATUS:
*
* Comment:
*
*----------------------------------------------------------------*/

/* acx_get_random()
 * STATUS: UNVERIFIED. (not binary compatible yet)
 */
static void acx_get_random(u8 *s, u16 stack)
{
	u8 var_8[4];
	u8 seed[4];
	u32 count1;
	u32 count2;

	u16 count, len;
	u32 ran = 0;

	FN_ENTER;
	seed[0] = 0;
	seed[1] = 0;
	seed[2] = 0;
	seed[3] = 0;

	/* FIXME: What is he doing here??? */
	ran = 10000;
	for (count1 = 0; count1 < sizeof(seed); count1++) {
		var_8[count1] = ((0x03ff6010 / ran) & 0xff);
		ran = (ran * 0xCCCCCCCD) >> 3;
	}

	/* Mmmm, strange ... is it really meant to just take some random part of the stack?? Hmm, why not? After all it's a random number generator... */
	len = strlen(var_8);

	/* generate a seed */
	if (len) {
		for (count2 = 0; count2 < len; count2++) {
			seed[count2 & 3] ^= var_8[count2];
		}

		SET_BIT(ran, (seed[0]));
		SET_BIT(ran, (seed[1] << 8));
		SET_BIT(ran, (seed[2] << 16));
		SET_BIT(ran, (seed[3] << 24));

		/* generate some random numbers */
		for (count = 0; count < stack; count++) {
			/* this is a standard random number generator
			   using "magic" numbers */
			ran = (214013 * ran + 2531011);
			s[count] = ((ran >> 16) & 0xff);
		}
	}
	FN_EXIT0();
}

/*----------------------------------------------------------------
* acx_gen_challenge
*
* Arguments:
*
* Returns:
*
* Side effects:
*
* Call context:
*
* STATUS:
*
* Comment:
*
*----------------------------------------------------------------*/

/* acx_gen_challenge()
 * STATUS: FINISHED.
 */
static void acx_gen_challenge(challenge_text_t * d)
{
	FN_ENTER;
	d->element_ID = WLAN_EID_CHALLENGE;
	d->length = WLAN_CHALLENGE_LEN;
	acx_get_random(d->text, WLAN_CHALLENGE_LEN);
	FN_EXIT0();
}

/*----------------------------------------------------------------
* acx_ibssid_gen
*
* Arguments:
*
* Returns:
*
* Side effects:
*
* Call context:
*
* STATUS:
*
* Comment:
*
*----------------------------------------------------------------*/

/* acx_ibssid_gen()
 * This generates a random BSSID to be used for ad-hoc networks
 * (in infrastructure BSS networks, the BSSID is the MAC of the access
 * point)
 * STATUS: should be ok.
 */
static void acx_ibssid_gen(wlandevice_t *priv, unsigned char *p_out)
{
	u8 jifmod;
	unsigned int i;
	u8 oct;

	FN_ENTER;
	for (i = 0; i < ETH_ALEN; i++) {
		/* store jiffies modulo 0xff */
		jifmod = (jiffies % 0xff);
		/* now XOR eax with this value */
		oct = priv->dev_addr[i] ^ jifmod;
		/* WLAN_LOG_NOTICE1("temp = %d\n", oct); */
		p_out[i] = oct;
	}

	p_out[0] = (p_out[0] & ~0x80) | 0x40;
	FN_EXIT0();
}

/*----------------------------------------------------------------
* acx_transmit_deauthen
* STATUS: should be ok, but UNVERIFIED.
*----------------------------------------------------------------*/
static int acx_transmit_deauthen(const u8 *addr, client_t *clt, wlandevice_t *priv, u16 reason)
{
	struct txdescriptor *tx_desc;
	struct txhostdescriptor *hdesc_head;
	struct txhostdescriptor *hdesc_body;
	struct wlan_hdr_mgmt *head;
	struct deauthen_frame_body *body;

	FN_ENTER;

	tx_desc = acx_get_tx_desc(priv);
	if (!tx_desc) {
		return NOT_OK;
	}

	hdesc_head = tx_desc->fixed_size.s.host_desc;
	hdesc_body = hdesc_head + 1;
	head = (void*)hdesc_head->data;
	body = (void*)hdesc_body->data;

	head->fc = (WF_FTYPE_MGMTi | WF_FSTYPE_DEAUTHENi);	/* 0xc0 */
	head->dur = 0;
	if (clt) {
		clt->used = CLIENT_EXIST_1;
		MAC_COPY(head->da, clt->address);
	} else {
		MAC_COPY(head->da, addr);
	}
	MAC_COPY(head->sa, priv->dev_addr);
	/* FIXME: this used to use dev_addr, but I think it should use
	 * the BSSID of the network we're associated to: priv->bssid */
	/* Actually it is the same thing - this code is clearly
	** assumes we're an AP! (this is wrong. STAs can send deauth too) */
	MAC_COPY(head->bssid, priv->bssid);
	head->seq = 0;

	acxlog(L_BINDEBUG | L_ASSOC | L_XFER,
		"<acx_transmit_deauthen> sta="MACSTR" for %d\n",
		MAC(head->da), reason);

	hdesc_head->length = cpu_to_le16(WLAN_HDR_A3_LEN);
	hdesc_head->data_offset = 0;

	body->reason = host2ieee16(reason);
	hdesc_body->length = cpu_to_le16(sizeof(deauthen_frame_body_t));
	hdesc_body->data_offset = 0;

	/* body is fixed size here, but beware of cutting-and-pasting this -
	** do not use sizeof(*body) for variable sized mgmt packets! */
	tx_desc->total_length = cpu_to_le16(WLAN_HDR_A3_LEN + sizeof(*body));

	acx_dma_tx_data(priv, tx_desc);

	FN_EXIT1(OK);
	return OK;
}

/*----------------------------------------------------------------
* acx_transmit_authen1
* STATUS: UNVERIFIED
*----------------------------------------------------------------*/
static int acx_transmit_authen1(wlandevice_t *priv)
{
	struct txdescriptor *tx_desc;
	struct txhostdescriptor *hdesc_head;
	struct txhostdescriptor *hdesc_body;
	struct wlan_hdr_mgmt *head;
	struct auth_frame_body *body;

	FN_ENTER;

	acxlog(L_BINSTD | L_ASSOC, "Sending authentication1 request, awaiting response!\n");

	tx_desc = acx_get_tx_desc(priv);
	if (!tx_desc) {
		FN_EXIT0();
		return NOT_OK;
	}

	hdesc_head = tx_desc->fixed_size.s.host_desc;
	hdesc_body = hdesc_head + 1;

	head = (void*)hdesc_head->data;
	body = (void*)hdesc_body->data;

	head->fc = WF_FSTYPE_AUTHENi; /* 0xb0 */
	head->dur = host2ieee16(0x8000);
	MAC_COPY(head->da, priv->bssid);
	MAC_COPY(head->sa, priv->dev_addr);
	MAC_COPY(head->bssid, priv->bssid);
	head->seq = 0;

	hdesc_head->length = cpu_to_le16(WLAN_HDR_A3_LEN);
	hdesc_head->data_offset = 0;

	body->auth_alg = host2ieee16(priv->auth_alg);
	body->auth_seq = host2ieee16(1);
	body->status = host2ieee16(0);

	hdesc_body->length = cpu_to_le16(2 + 2 + 2);
	hdesc_body->data_offset = 0;

	tx_desc->total_length = cpu_to_le16(WLAN_HDR_A3_LEN + 2 + 2 + 2);

	acx_dma_tx_data(priv, tx_desc);

	FN_EXIT0();
	return OK;
}

/*----------------------------------------------------------------
* acx_transmit_authen2
* STATUS: UNVERIFIED. (not binary compatible yet)
*----------------------------------------------------------------*/
static int acx_transmit_authen2(const wlan_fr_authen_t *req, client_t *sta_list,
		      wlandevice_t *priv)
{
	struct txdescriptor *tx_desc;
	struct txhostdescriptor *hdesc_head;
	struct txhostdescriptor *hdesc_body;
	struct wlan_hdr_mgmt *head;
	struct auth_frame_body *body;
	unsigned int packet_len;

	FN_ENTER;

	if (!sta_list) {
		FN_EXIT1(OK);
		return OK;
	}

	MAC_COPY(sta_list->address, req->hdr->a3.a2);
	sta_list->ps = ((WF_FC_PWRMGTi & req->hdr->a3.fc) != 0);
	sta_list->auth_alg = ieee2host16(*(req->auth_alg));
	sta_list->auth_step = 2;
	sta_list->seq = ieee2host16(req->hdr->a3.seq);

	tx_desc = acx_get_tx_desc(priv);
	if (!tx_desc) {
		FN_EXIT1(NOT_OK);
		return NOT_OK;
	}

	hdesc_head = tx_desc->fixed_size.s.host_desc;
	hdesc_body = hdesc_head + 1;
	head = (void*)hdesc_head->data;
	body = (void*)hdesc_body->data;

	head->fc = WF_FSTYPE_AUTHENi; /* 0xb0 */
	head->dur = req->hdr->a3.dur;
	MAC_COPY(head->da, req->hdr->a3.a2);
	MAC_COPY(head->sa, req->hdr->a3.a1);
	MAC_COPY(head->bssid, req->hdr->a3.a3);
	head->seq = req->hdr->a3.seq;

	hdesc_head->length = cpu_to_le16(WLAN_HDR_A3_LEN);
	hdesc_head->data_offset = 0;

	/* already in IEEE format, no endianness conversion */
	body->auth_alg = *(req->auth_alg);
	body->auth_seq = host2ieee16(2);
	body->status = host2ieee16(0);

	packet_len = WLAN_HDR_A3_LEN + 2 + 2 + 2;
	if (ieee2host16(*(req->auth_alg)) == WLAN_AUTH_ALG_OPENSYSTEM) {
		sta_list->used = CLIENT_AUTHENTICATED_2;
	} else {	/* shared key */
		acx_gen_challenge(&body->challenge);
		memcpy(&sta_list->challenge_text, body->challenge.text, WLAN_CHALLENGE_LEN);
		packet_len += 2 + 2 + 2 + 1+1+WLAN_CHALLENGE_LEN;
	}

	hdesc_body->length = cpu_to_le16(packet_len - WLAN_HDR_A3_LEN);
	hdesc_body->data_offset = 0;

	acxlog(L_BINDEBUG | L_ASSOC | L_XFER,
	       "<transmit_auth2> BSSID="MACSTR"\n", MAC(head->bssid));

	tx_desc->total_length = cpu_to_le16(packet_len);

	acx_dma_tx_data(priv, tx_desc);

	FN_EXIT1(OK);
	return OK;
}

/*----------------------------------------------------------------
* acx_transmit_authen3
* STATUS: UNVERIFIED.
*----------------------------------------------------------------*/
static int acx_transmit_authen3(const wlan_fr_authen_t *req, wlandevice_t *priv)
{
	struct txdescriptor *tx_desc;
	struct txhostdescriptor *hdesc_head;
	struct txhostdescriptor *hdesc_body;
	struct wlan_hdr_mgmt *head;
	struct auth_frame_body *body;
	unsigned int packet_len;

	FN_ENTER;
	tx_desc = acx_get_tx_desc(priv);
	if (!tx_desc) {
		FN_EXIT1(NOT_OK); /* FIXME: Is this ok or not ok? */
		return OK;
	}

	hdesc_head = tx_desc->fixed_size.s.host_desc;
	hdesc_body = hdesc_head + 1;

	head = (void*)hdesc_head->data;
	body = (void*)hdesc_body->data;

	head->fc = WF_FC_ISWEPi + WF_FSTYPE_AUTHENi;	/* 0x40b0 */
	/* FIXME: is this needed?? authen4 does it...
	head->dur = req->hdr->a3.dur;
	head->seq = req->hdr->a3.seq;
	*/
	MAC_COPY(head->da, priv->bssid);
	MAC_COPY(head->sa, priv->dev_addr);
	MAC_COPY(head->bssid, priv->bssid);

	hdesc_head->length = cpu_to_le16(WLAN_HDR_A3_LEN);
	hdesc_head->data_offset = 0;

	/* already in IEEE format, no endianness conversion */
	body->auth_alg = *(req->auth_alg);
	body->auth_seq = host2ieee16(3);
	body->status = host2ieee16(0);
	memcpy(&body->challenge, req->challenge, req->challenge->len + 2);
	packet_len = WLAN_HDR_A3_LEN + 8 + req->challenge->len;

	hdesc_body->length = cpu_to_le16(packet_len - WLAN_HDR_A3_LEN);
	hdesc_body->data_offset = 0;

	acxlog(L_BINDEBUG | L_ASSOC | L_XFER, "transmit_authen3!\n");

	tx_desc->total_length = cpu_to_le16(packet_len);
	
	acx_dma_tx_data(priv, tx_desc);

	FN_EXIT1(OK);
	return OK;
}

/*----------------------------------------------------------------
* acx_transmit_authen4
* STATUS: UNVERIFIED.
*----------------------------------------------------------------*/
static int acx_transmit_authen4(const wlan_fr_authen_t *req, wlandevice_t *priv)
{
	struct txdescriptor *tx_desc;
	struct txhostdescriptor *hdesc_head;
	struct txhostdescriptor *hdesc_body;
	struct wlan_hdr_mgmt *head;
	struct auth_frame_body *body;

	FN_ENTER;

	tx_desc = acx_get_tx_desc(priv);
	if (!tx_desc) {
		FN_EXIT1(OK);
		return OK;
	}

	hdesc_head = tx_desc->fixed_size.s.host_desc;
	hdesc_body = hdesc_head + 1;

	head = (void*)hdesc_head->data;
	body = (void*)hdesc_body->data;

	head->fc = WF_FSTYPE_AUTHENi; /* 0xb0 */
	head->dur = req->hdr->a3.dur;
	MAC_COPY(head->da, req->hdr->a3.a2);
	/* FIXME: huh? why was there no "sa"? Added, assume should do like authen2 */
	MAC_COPY(head->sa, req->hdr->a3.a1);
	MAC_COPY(head->bssid, req->hdr->a3.a3);
	head->seq = req->hdr->a3.seq;

	hdesc_head->length = cpu_to_le16(WLAN_HDR_A3_LEN);
	hdesc_head->data_offset = 0;

	/* already in IEEE format, no endianness conversion */
	body->auth_alg = *(req->auth_alg);
	body->auth_seq = host2ieee16(4);
	body->status = host2ieee16(0);

	hdesc_body->length = cpu_to_le16(2 + 2 + 2);
	hdesc_body->data_offset = 0;

	tx_desc->total_length = cpu_to_le16(WLAN_HDR_A3_LEN + 2 + 2 + 2);

	acx_dma_tx_data(priv, tx_desc);

	FN_EXIT1(OK);
	return OK;
}

/*----------------------------------------------------------------
* acx_transmit_assoc_req
* STATUS: almost ok, but UNVERIFIED.
*----------------------------------------------------------------*/
static int acx_transmit_assoc_req(wlandevice_t *priv)
{
	struct txdescriptor *tx_desc;
	struct txhostdescriptor *hdesc_head;
	struct txhostdescriptor *hdesc_body;
	struct wlan_hdr_mgmt *head;
	char *body,*p;
	unsigned int packet_len;
	u16 cap;

	FN_ENTER;

	acxlog(L_BINSTD | L_ASSOC, "Sending association request, awaiting response! NOT ASSOCIATED YET.\n");
	tx_desc = acx_get_tx_desc(priv);
	if (!tx_desc) {
		FN_EXIT1(NOT_OK);
		return NOT_OK;
	}

	hdesc_head = tx_desc->fixed_size.s.host_desc;
	hdesc_body = hdesc_head + 1;

	head = (void*)hdesc_head->data;
	body = (void*)hdesc_body->data;

	head->fc = WF_FSTYPE_ASSOCREQi;  /* 0x00 */;
	head->dur = host2ieee16(0x8000);
	MAC_COPY(head->da, priv->bssid);
	MAC_COPY(head->sa, priv->dev_addr);
	MAC_COPY(head->bssid, priv->bssid);
	head->seq = 0;

	hdesc_head->length = cpu_to_le16(WLAN_HDR_A3_LEN);
	hdesc_head->data_offset = 0;

	p = body;
	/* now start filling the AssocReq frame body */
#if BROKEN
	cap = host2ieee16(priv->capabilities & ~WF_MGMT_CAP_IBSS);
#else
	/* FIXME: is it correct that we have to manually patc^H^H^H^Hadjust the
	 * Capabilities like that?
	 * I'd venture that priv->capabilities
	 * (acx_update_capabilities()) should have set that
	 * beforehand maybe...
	 * Anyway, now Managed network association works properly
	 * without failing.
	 */
	/*
	cap = host2ieee16((priv->capabilities & ~WF_MGMT_CAP_IBSS) | WF_MGMT_CAP_ESS);
	*/
	cap = WF_MGMT_CAP_ESSi;
	if (priv->wep_restricted)
		SET_BIT(cap, WF_MGMT_CAP_PRIVACYi);
	/* only ask for short preamble if the peer station supports it */
	if (priv->station_assoc.caps & WF_MGMT_CAP_SHORT)
		SET_BIT(cap, WF_MGMT_CAP_SHORTi);
	/* only ask for PBCC support if the peer station supports it */
	if (priv->station_assoc.caps & WF_MGMT_CAP_PBCC)
		SET_BIT(cap, WF_MGMT_CAP_PBCCi);
#endif
	acxlog(L_ASSOC, "association: requesting capabilities 0x%04X\n", cap);
	*(u16*)p = cap;	p += 2;
	/* add listen interval */
	*(u16*)p = host2ieee16(priv->listen_interval); p += 2;
	/* add ESSID */
	p = wlan_fill_ie_ssid(p, strlen(priv->essid_for_assoc), priv->essid_for_assoc);
	/* add rates */
	p = wlan_fill_ie_rates(p, priv->rate_supported_len, priv->rate_supported);
	/* calculate lengths */
	packet_len = WLAN_HDR_A3_LEN + (p - body);

	hdesc_body->length = cpu_to_le16(packet_len - WLAN_HDR_A3_LEN);
	hdesc_body->data_offset = 0;

	tx_desc->total_length = cpu_to_le16(packet_len);

	acx_dma_tx_data(priv, tx_desc);
	FN_EXIT1(OK);
	return OK;
}

/*----------------------------------------------------------------
* acx_transmit_disassoc
* STATUS: almost ok, but UNVERIFIED.
* FIXME: type of clt is a guess
* I'm not sure if clt is needed
*----------------------------------------------------------------*/
u32 acx_transmit_disassoc(client_t *clt, wlandevice_t *priv)
{
	struct txdescriptor *tx_desc;
	struct txhostdescriptor *hdesc_head;
	struct txhostdescriptor *hdesc_body;
	struct wlan_hdr_mgmt *head;
	struct disassoc_frame_body *body;

	FN_ENTER;
/*	if (clt != NULL) { */
		tx_desc = acx_get_tx_desc(priv);
		if (!tx_desc) {
			FN_EXIT1(NOT_OK);
			return NOT_OK;
		}

		hdesc_head = tx_desc->fixed_size.s.host_desc;
		hdesc_body = hdesc_head + 1;

		head = (void*)hdesc_head->data;
		body = (void*)hdesc_body->data;

/*		clt->used = CLIENT_AUTHENTICATED_2; - not (yet?) associated */

		head->fc = WF_FSTYPE_DISASSOCi;	/* 0xa0 */
		head->dur = 0;
		/* huh? It muchly depends on whether we're STA or AP...
		** sta->ap: da=bssid, sa=own, bssid=bssid
		** ap->sta: da=sta, sa=bssid, bssid=bssid. FIXME! */
		MAC_COPY(head->da, priv->bssid);
		MAC_COPY(head->sa, priv->dev_addr);
		MAC_COPY(head->bssid, priv->dev_addr);
		head->seq = 0;

		hdesc_head->length = cpu_to_le16(WLAN_HDR_A3_LEN);
		hdesc_head->data_offset = 0;

		body->reason = host2ieee16(7);	/* "Class 3 frame received from nonassociated station." */

		/* fixed size struct, ok to sizeof */
		hdesc_body->length = cpu_to_le16(sizeof(*body));
		hdesc_body->data_offset = 0;

		tx_desc->total_length = cpu_to_le16(WLAN_HDR_A3_LEN + sizeof(*body));

		acx_dma_tx_data(priv, tx_desc);

		FN_EXIT1(OK);
		return OK;
/*	} */
	FN_EXIT1(OK);
	return OK;
}

/*----------------------------------------------------------------
* acx_complete_dot11_scan
* STATUS: FINISHED.
* Called just after scanning, when we decided to join ESS or IBSS.
* Iterates thru priv->bss_table:
*	if priv->ap is not bcast, will join only specified
*	ESS or IBSS with this bssid
*	checks peers' caps for ESS/IBSS bit
*	checks peer's SSID, allows exact match or hidden SSID
* If station to join is chosen:
*	copies bss_info struct of it to priv->station_assoc
*	sets priv->essid_for_assoc for future assoc attempt
* Auth/assoc is not yet performed
*----------------------------------------------------------------*/
void acx_complete_dot11_scan(wlandevice_t *priv)
{
	unsigned int idx;
	u16 needed_cap;
	s32 idx_found = -1;
	u32 found_station = 0;

	FN_ENTER;

	switch(priv->mode) {
	case ACX_MODE_0_ADHOC:
		needed_cap = WF_MGMT_CAP_IBSS; /* 2, we require Ad-Hoc */
		break;
	case ACX_MODE_2_STA:
		needed_cap = WF_MGMT_CAP_ESS; /* 1, we require Managed */
		break;
	default:
		acxlog(L_STD, "driver bug: macmode %d!\n", priv->mode);
		dump_stack();
		goto end;
	}
	
	acxlog(L_BINDEBUG | L_ASSOC, "Radio scan found %d stations in this area\n",
		priv->bss_table_count);

	for (idx = 0; idx < priv->bss_table_count; idx++) {
		struct bss_info *bss = &priv->bss_table[idx];

		acxlog(L_BINDEBUG | L_ASSOC,
			"<Scan Table> %d: SSID=\"%s\",CH=%d,SIR=%d,SNR=%d\n",
			idx, bss->essid, bss->channel, bss->sir, bss->snr);

		if (!mac_is_bcast(priv->ap))
			if (!mac_is_equal(bss->bssid, priv->ap))
				continue; /* keep looking */

		/* broken peer with no mode flags set? */
		if (!(bss->caps & (WF_MGMT_CAP_ESS | WF_MGMT_CAP_IBSS))) {
			acxlog(L_ASSOC, "STRANGE: peer station announces "
				"neither ESS (Managed) nor IBSS (Ad-Hoc) "
				"capability. Won't try to join it\n");
			continue;
		}
		acxlog(L_ASSOC, "peer_cap 0x%04x, needed_cap 0x%04x\n",
		       bss->caps, needed_cap);

		/* peer station doesn't support what we need? */
		if ((bss->caps & needed_cap) != needed_cap)
			continue; /* keep looking */

		if ( !(priv->reg_dom_chanmask & (1<<(bss->channel-1))) ) {
			acxlog(L_STD|L_ASSOC, "WARNING: peer station %d "
				"is using channel %d, which is outside "
				"the channel range of the regulatory domain "
				"the driver is currently configured for: "
				"couldn't join in case of matching settings, "
				"might want to adapt your config!\n",
				idx, bss->channel);
			continue; /* keep looking */
		}

		if ((0 == priv->essid_active)
		/* FIXME: bss->essid can be LONGER than ours! strcmp? */
		 || (0 == memcmp(bss->essid, priv->essid, priv->essid_len))
		) {
			acxlog(L_ASSOC,
			       "found station with matching ESSID! (\"%s\" "
			       "station, \"%s\" config)\n",
			       bss->essid,
			       (priv->essid_active) ? priv->essid : "[any]");
			idx_found = idx;
			found_station = 1;

			/* stop searching if this station is
			 * on the current channel, otherwise
			 * keep looking for an even better match */
			if (bss->channel == priv->channel)
				break;
		} else
		if (('\0' == bss->essid[0])
		 || ( (1 == strlen(bss->essid)) && (' ' == bss->essid[0]) )
		) {
			/* hmm, station with empty or single-space SSID:
			 * using hidden SSID broadcast?
			 */
			/* This behaviour is broken: which AP from zillion
			** of APs with hidden SSID you'd try?
			** We should use Probe requests to get Probe responses
			** and check for real SSID (are those never hidden?) */
			idx_found = idx;
			found_station = 1;
			acxlog(L_ASSOC, "found station with empty or "
				"single-space (hidden) SSID, considering "
				"for assoc attempt\n");
			/* ...and keep looking for better matches */
		}
		else {
			acxlog(L_ASSOC, "ESSID doesn't match! (\"%s\" "
				"station, \"%s\" config)\n",
				bss->essid,
				(priv->essid_active) ? priv->essid : "[any]");
		}
	}

	if (found_station) {
		char *essid_src;
		size_t essid_len;

		memcpy(&priv->station_assoc, &priv->bss_table[idx_found], sizeof(struct bss_info));
		if (priv->station_assoc.essid[0] == '\0') {
			/* if the ESSID of the station we found is empty
			 * (no broadcast), then use user configured ESSID
			 * instead */
			essid_src = priv->essid;
			essid_len = priv->essid_len;
		}
		else {
			essid_src = priv->bss_table[idx_found].essid;
			essid_len = strlen(priv->bss_table[idx_found].essid);
		}
		
		acx_update_capabilities(priv);

		memcpy(priv->essid_for_assoc, essid_src, essid_len);
		priv->essid_for_assoc[essid_len] = '\0';
		priv->channel = priv->station_assoc.channel;
		MAC_COPY(priv->bssid, priv->station_assoc.bssid);

		acxlog(L_STD | L_ASSOC,
		       "%s: matching station FOUND (idx %d), JOINING ("MACSTR")\n",
		       __func__, idx_found, MAC(priv->bssid));

		/* Inform firmware on our decision */
		acx_cmd_join_bssid(priv, priv->bssid);
		acx_update_peerinfo(priv, &priv->ap_peer, &priv->station_assoc); /* e.g. shortpre */

		if (ACX_MODE_0_ADHOC == priv->mode) {
			acx_set_status(priv, ACX_STATUS_4_ASSOCIATED);
		} else {
			acx_transmit_authen1(priv);
			acx_set_status(priv, ACX_STATUS_2_WAIT_AUTH);
		}
	} else {
		/* uh oh, no station found in range */
		if (ACX_MODE_0_ADHOC == priv->mode) {
			acxlog(L_STD | L_ASSOC,
			       "%s: no matching station found in range, "
			       "generating our own IBSSID instead\n",
			       __func__);
			acx_ibssid_gen(priv, priv->bssid);
			/* add IBSS bit to our caps... */
			acx_update_capabilities(priv);
			acx_cmd_join_bssid(priv, priv->bssid);
			acx_set_status(priv, ACX_STATUS_4_ASSOCIATED);
		} else {
			/* FIXME: we shall scan again! What if AP
			** was just temporarily powered off? */
			acxlog(L_STD | L_ASSOC,
				"%s: no matching station found in range yet\n",
			       __func__);
			acx_set_status(priv, ACX_STATUS_1_SCANNING);
		}
	}
end:
	FN_EXIT0();
}

#if (POWER_SAVE_80211 == 0)
/*----------------------------------------------------------------
* ActivatePowerSaveMode
* FIXME: rename to acx_activate_power_save_mode
*
* Arguments:
*
* Returns:
*
* Side effects:
*
* Call context:
*
* STATUS:
*
* Comment:
*
*----------------------------------------------------------------*/

/* ActivatePowerSaveMode()
 * STATUS: FINISHED, UNVERIFIED.
 */
static void ActivatePowerSaveMode(wlandevice_t *priv, /*@unused@*/ int vala)
{
       acx100_ie_powermgmt_t pm;

       FN_ENTER;

       acx_interrogate(priv, &pm, ACX1xx_IE_POWER_MGMT);
       if (pm.wakeup_cfg != 0x81) {
               FN_EXIT0();
               return;
       }
       pm.wakeup_cfg = 0;
       pm.options = 0;
       pm.hangover_period = 0;
       acx_configure(priv, &pm, ACX1xx_IE_POWER_MGMT);
       FN_EXIT0();
}
#endif

/*------------------------------------------------------------------------------
 * acx_timer
 *
 *
 * Arguments:
 *	@address: a pointer to the private acx100 per-device struct
 *
 * Returns:
 *
 * Side effects:
 *
 * Call context:
 *
 * STATUS: should be ok, but UNVERIFIED.
 *
 * Comment:
 *
 *----------------------------------------------------------------------------*/
void acx_timer(unsigned long address)
{
	wlandevice_t *priv = (wlandevice_t *)address;
	unsigned long flags;
#if 0
	int status;
#endif

	FN_ENTER;
	acxlog(L_BINDEBUG | L_ASSOC, "%s: status = %d\n", __func__,
		priv->status);

	if (acx_lock(priv, &flags)) {
		FN_EXIT0();
		return;
	}

	switch (priv->status) {
	case ACX_STATUS_1_SCANNING:
		if ((++priv->scan_retries < 5) && (0 == priv->bss_table_count)) {
			acx_set_timer(priv, 1000000);
#if 0
			acx_interrogate(priv,&status,ACX1xx_IE_SCAN_STATUS);
			acxlog(L_STD,"scan status=%d\n",status);
			if (status==0) {
				acx_complete_dot11_scan(priv);
			}
#endif
			acxlog(L_ASSOC, "continuing scan (attempt %d).\n", priv->scan_retries);
		} else {
			acxlog(L_ASSOC, "Stopping scan (%s).\n", priv->bss_table_count ? "stations found" : "scan timeout");
			/* stop the scan when we leave the interrupt context */
			acx_schedule_after_interrupt_task(priv, ACX_AFTER_IRQ_CMD_STOP_SCAN);
			/* HACK: set the IRQ bit, since we won't get a
			 * scan complete IRQ any more on ACX111 (works on ACX100!),
			 * since we will have stopped the scan */
			SET_BIT(priv->irq_status, HOST_INT_SCAN_COMPLETE);
		}
		break;
	case ACX_STATUS_2_WAIT_AUTH:
		priv->scan_retries = 0;
		if (++priv->auth_or_assoc_retries < 10) {
			acxlog(L_ASSOC, "resend authen1 request (attempt %d).\n",
			       priv->auth_or_assoc_retries + 1);
			acx_transmit_authen1(priv);
		} else {
			/* time exceeded: fall back to scanning mode */
			acxlog(L_ASSOC,
			       "authen1 request reply timeout, giving up.\n");
			/* simply set status back to scanning (DON'T start scan) */
			acx_set_status(priv, ACX_STATUS_1_SCANNING);
		}
		acx_set_timer(priv, 2500000); /* used to be 1500000, but some other driver uses 2.5s wait time  */
		break;
	case ACX_STATUS_3_AUTHENTICATED:
		if (++priv->auth_or_assoc_retries < 10) {
			acxlog(L_ASSOC,
			       "resend association request (attempt %d).\n",
			       priv->auth_or_assoc_retries + 1);
			acx_transmit_assoc_req(priv);
		} else {
			/* time exceeded: give up */
			acxlog(L_ASSOC,
			       "association request reply timeout, giving up.\n");
			/* simply set status back to scanning (DON'T start scan) */
			acx_set_status(priv, ACX_STATUS_1_SCANNING);
		}
		acx_set_timer(priv, 2500000); /* see above */
		break;
	case ACX_STATUS_4_ASSOCIATED:
	default:
		break;
	}
	acx_unlock(priv, &flags);
	FN_EXIT0();
}
