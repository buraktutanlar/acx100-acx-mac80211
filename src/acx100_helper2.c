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

#include <linux/ioport.h>
#include <linux/pm.h>

#include <linux/dcache.h>
#include <linux/highmem.h>
#include <linux/etherdevice.h>


/*================================================================*/
/* Project Includes */

#include <acx.h>

static client_t *acx_sta_list_alloc(wlandevice_t *priv);
static client_t *acx_sta_list_get_from_hash(wlandevice_t *priv, const u8 *address);
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
		if (!priv->sta_list[i].used) {
			memset(&priv->sta_list[i], 0, sizeof(priv->sta_list[i]));
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
client_t *acx_sta_list_add(wlandevice_t *priv, const u8 *address)
{
	client_t *client;
	int index;

	FN_ENTER;

	client = acx_sta_list_alloc(priv);
	if (!client)
		goto done;

	MAC_COPY(client->address, address);
	client->used = CLIENT_EXIST_1;
	client->auth_alg = WLAN_AUTH_ALG_SHAREDKEY;
	client->auth_step = 1;
	/* give some tentative peer rate values
	** (needed because peer may do auth without probing us first,
	** thus we have no idea of peer's ratevector yet).
	** Will be overwritten by scanning or assoc code */
	client->rate_cap = priv->rate_basic;
	client->rate_cfg = priv->rate_basic;
	client->rate_cur = 1 << lowest_bit(priv->rate_basic);

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
* acx_sta_list_del
*----------------------------------------------------------------*/
void acx_sta_list_del(wlandevice_t *priv, client_t *victim)
{
	client_t *client, *next;

	client = acx_sta_list_get_from_hash(priv, victim->address);
	next = client;
	/* tricky. next = client on first iteration only,
	** on all other iters next = client->next */
	while (next) {
		if (next == victim) {
			client->next = victim->next;
			/* Overkill. Not a hot path... */
			memset(victim, 0, sizeof(*victim));
			break;
		}
		client = next;
		next = client->next;
	}
}

/*----------------------------------------------------------------
* acx_sta_list_get
*----------------------------------------------------------------*/
client_t *acx_sta_list_get(wlandevice_t *priv, const u8 *address)
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

/*----------------------------------------------------------------
* acx_sta_list_get_or_add
*----------------------------------------------------------------*/
client_t *acx_sta_list_get_or_add(wlandevice_t *priv, const u8 *address)
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

	p80211_hdr = acx_get_p80211_hdr(priv, rxdesc->data);
/*	printk("Rx_CONFIG_1 = %X\n",priv->rx_config_1 & RX_CFG1_INCLUDE_PHY_HDR); */

	/* see IEEE 802.11-1999.pdf chapter 7 "MAC frame formats" */
	ftype = p80211_hdr->a3.fc & WF_FC_FTYPEi;
	fstype = p80211_hdr->a3.fc & WF_FC_FSTYPEi;

	switch (ftype) {
	/* check data frames first, for speed */
	case WF_FTYPE_DATAi:
		switch (fstype) {
		case WF_FSTYPE_DATAONLYi:
			if (unlikely(p80211_hdr->a3.seq == priv->last_seq_ctrl)) {
				acxlog(L_STD, "rx: DUP pkt (seq %u)!\n", priv->last_seq_ctrl);
				/* simply discard it and indicate error */
				priv->stats.rx_errors++;
				break;
			}
			else
				priv->last_seq_ctrl = p80211_hdr->a3.seq;
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
*
* We are an AP here
*
* STATUS: should be ok, but UNVERIFIED.
*----------------------------------------------------------------*/
static const u8
dot11ratebyte[] = {
	DOT11RATEBYTE_1,
	DOT11RATEBYTE_2,
	DOT11RATEBYTE_5_5,
	DOT11RATEBYTE_6_G,
	DOT11RATEBYTE_9_G,
	DOT11RATEBYTE_11,
	DOT11RATEBYTE_12_G,
	DOT11RATEBYTE_18_G,
	DOT11RATEBYTE_22,
	DOT11RATEBYTE_24_G,
	DOT11RATEBYTE_36_G,
	DOT11RATEBYTE_48_G,
	DOT11RATEBYTE_54_G,
};

static int
find_pos(const u8 *p, int size, u8 v)
{
	int i;
	for (i = 0; i < size; i++)
		if (p[i] == v)
			return i;
	/* printk a message about strange byte? */
	return 0;
}

static void
add_bits_to_ratemasks(u8* ratevec, int len, u16* brate, u16* orate)
{
	while (len--) {
		int n = 1 << find_pos(dot11ratebyte, sizeof(dot11ratebyte), *ratevec & 0x7f);
		if (*ratevec & 0x80)
			*brate |= n;
		*orate |= n;
		ratevec++;
	}
}

static u32 acx_transmit_assocresp(const wlan_fr_assocreq_t *req,
			  wlandevice_t *priv)
{
	struct txdescriptor *tx_desc;
	struct txhostdescriptor *hdesc_head;
	struct txhostdescriptor *hdesc_body;
	struct wlan_hdr_mgmt *head;
	struct assocresp_frame_body *body;
	u8 *p;
	const u8 *da;
	const u8 *sa;
	const u8 *bssid;
	client_t *clt;

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
	if (clt->used != CLIENT_AUTHENTICATED_2) {
		acx_transmit_deauthen(da, clt, priv, WLAN_MGMT_REASON_CLASS2_NONAUTH);
		FN_EXIT0();
		return NOT_OK;
	}

	clt->used = CLIENT_ASSOCIATED_3;

	if (clt->aid == 0) {
		clt->aid = ++priv->aid;
	}
	clt->cap_info = ieee2host16(*(req->cap_info));
	/* We cheat here a bit. We don't really care which rates are flagged
	** as basic by the client, so we stuff them in single ratemask */
	clt->rate_cap = 0;
	if (req->supp_rates)
		add_bits_to_ratemasks(req->supp_rates->rates,
			req->supp_rates->len, &clt->rate_cap, &clt->rate_cap);
	if (req->ext_rates)
		add_bits_to_ratemasks(req->ext_rates->rates,
			req->ext_rates->len, &clt->rate_cap, &clt->rate_cap);
	/* TODO: what shall we do if it doesn't support whole basic rate set? */

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
	p = wlan_fill_ie_rates((u8*)&body->rates, priv->rate_supported_len, priv->rate_supported);
	p = wlan_fill_ie_rates_ext(p, priv->rate_supported_len, priv->rate_supported);

	hdesc_body->length = cpu_to_le16(p - (u8*)hdesc_body->data);
	hdesc_body->data_offset = 0;

	tx_desc->total_length = cpu_to_le16(WLAN_HDR_A3_LEN + p - (u8*)hdesc_body->data);

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
	u8 *p;
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
		clt->aid = ++priv->aid;
	}
	if (req->cap_info)
		clt->cap_info = ieee2host16(*(req->cap_info));
	/* We cheat here a bit. We don't really care which rates are flagged
	** as basic by the client, so we stuff them in single ratemask */
	clt->rate_cap = 0;
	if (req->supp_rates)
		add_bits_to_ratemasks(req->supp_rates->rates,
			req->supp_rates->len, &clt->rate_cap, &clt->rate_cap);
	if (req->ext_rates)
		add_bits_to_ratemasks(req->ext_rates->rates,
			req->ext_rates->len, &clt->rate_cap, &clt->rate_cap);
	/* TODO: what shall we do if it doesn't support whole basic rate set? */

	tx_desc = acx_get_tx_desc(priv);
	if (!tx_desc) {
		FN_EXIT1(OK);
		return OK;
	}

	hdesc_head = tx_desc->fixed_size.s.host_desc;
	hdesc_body = hdesc_head + 1;
	head = (void*)hdesc_head->data;
	body = (void*)hdesc_body->data;

	head->fc = WF_FSTYPE_REASSOCRESPi;
	head->dur = req->hdr->a3.dur;
	MAC_COPY(head->da, da);
	MAC_COPY(head->sa, sa);
	MAC_COPY(head->bssid, bssid);
	head->seq = req->hdr->a3.seq;

	hdesc_head->length = cpu_to_le16(WLAN_HDR_A3_LEN);
	hdesc_head->data_offset = 0;

	/* IEs: 1. caps */
	body->cap_info = host2ieee16(priv->capabilities);
	/* 2. status code */
	body->status = host2ieee16(0);
	/* 3. AID */
	body->aid = host2ieee16(clt->aid);
	/* 4. supp rates */
	p = wlan_fill_ie_rates((u8*)&body->rates, priv->rate_supported_len, priv->rate_supported);
	/* 5. ext supp rates */
	p = wlan_fill_ie_rates_ext(p, priv->rate_supported_len, priv->rate_supported);

	hdesc_body->length = cpu_to_le16(p - (u8*)hdesc_body->data);
	hdesc_body->data_offset = 0;

	tx_desc->total_length = cpu_to_le16(WLAN_HDR_A3_LEN + p - (u8*)hdesc_body->data);

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
	p80211_hdr_t* p80211_hdr;
	struct txdescriptor *txdesc;
	const u8 *da, *sa, *bssid;
	int result = NOT_OK;

	FN_ENTER;

        p80211_hdr = acx_get_p80211_hdr(priv, rxdesc->data);

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

	if (mac_is_equal(priv->dev_addr, da)) {
		/* this one is for us */
		acx_rx(rxdesc, priv);
		goto done;
	} else {
		/* repackage, tx, and hope it someday reaches its destination */
		/* order is important, we do it in-place */
		MAC_COPY(p80211_hdr->a3.a1, da);
		MAC_COPY(p80211_hdr->a3.a3, sa);
		MAC_COPY(p80211_hdr->a3.a2, priv->bssid);
		/* To_DS = 0, From_DS = 1 */
		p80211_hdr->a3.fc = WF_FC_FROMDSi + WF_FTYPE_DATAi;

		txdesc = acx_get_tx_desc(priv);
		if (!txdesc) {
			result = NOT_OK;
			goto fail;
		}
		acx_rxdesc_to_txdesc(rxdesc, txdesc);
		acx_dma_tx_data(priv, txdesc);
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

	p80211_hdr = acx_get_p80211_hdr(priv, rxdesc->data);

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

	hdr = acx_get_p80211_hdr(priv, rxdesc->data);

	/* Management frames never have these set */
	if (WF_FC_FROMTODSi & hdr->a3.fc) {
		FN_EXIT1(NOT_OK);
		return NOT_OK;
	}

	len = RXBUF_BYTES_RCVD(rxdesc->data);
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
		if (mac_is_equal(hdr->a3.a1, priv->bssid)
		 && mac_is_equal(hdr->a3.a3, priv->bssid)) {
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
		if (ap || adhoc) {
			/* FIXME: since we're supposed to be an AP,
			** we need to return a Probe Response packet.
			** Currently firmware is doing it for us,
			** but firmware is buggy! See comment elsewhere --vda */
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
		acx_process_authen(&parsed.authen, priv);
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

	p80211_hdr = acx_get_p80211_hdr(priv, rxdesc->data);
		
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
* STATUS: working on it.  UNVERIFIED.
*----------------------------------------------------------------*/
static void acx_process_probe_response(wlan_fr_proberesp_t *req, const struct rxbuffer *mmt, wlandevice_t *priv)
{
	struct client *bss;
	p80211_hdr_t *hdr;

	FN_ENTER;

	hdr = req->hdr;

	if (mac_is_equal(hdr->a3.a3, priv->dev_addr)) {
		acxlog(L_ASSOC, "huh, scan found our own MAC!?\n");
		FN_EXIT0();
		return; /* just skip this one silently */
	}
	
	bss = acx_sta_list_get_or_add(priv, hdr->a3.a3);
	if (!bss) {
		/* uh oh, we found more sites/stations than we can handle with
		 * our current setup: pull the emergency brake and stop scanning! */
		acx_issue_cmd(priv, ACX1xx_CMD_STOP_SCAN, NULL, 0, ACX_CMD_TIMEOUT_DEFAULT);
		/* TODO: a nice comment what below call achieves --vda */
		acx_set_status(priv, ACX_STATUS_2_WAIT_AUTH);
		FN_EXIT0();
		return;
	}

	bss->mtime = jiffies;
	/* get_or_add filled bss->address */
	MAC_COPY(bss->bssid, hdr->a3.a3);

	/* copy the ESSID element */
	if (req->ssid && req->ssid->len <= IW_ESSID_MAX_SIZE) {
		bss->essid_len = req->ssid->len;
		memcpy(bss->essid, req->ssid->ssid, req->ssid->len);
		bss->essid[req->ssid->len] = '\0';
	}
	else {
		acxlog(L_STD, "huh, ESSID overflow in scanned station data?\n");
	}

	if (req->ds_parms)
		bss->channel = req->ds_parms->curr_ch;
	if (req->cap_info)
		bss->cap_info = ieee2host16(*req->cap_info);

	bss->sir = acx_signal_to_winlevel(mmt->phy_level);
	bss->snr = acx_signal_to_winlevel(mmt->phy_snr);

	bss->rate_cfg = 0;	/* basic */
	bss->rate_cap = 0;	/* operational */
	bss->rate_cur = 0;
	if (req->supp_rates)
		add_bits_to_ratemasks(req->supp_rates->rates,
			req->supp_rates->len, &bss->rate_cfg, &bss->rate_cap);
	if (req->ext_rates)
		add_bits_to_ratemasks(req->ext_rates->rates,
			req->ext_rates->len, &bss->rate_cfg, &bss->rate_cap);
	if (bss->rate_cfg)
		bss->rate_cur = 1 << lowest_bit(bss->rate_cfg);

	acxlog(L_STD | L_ASSOC,
		"found and registered station: ESSID \"%s\" on channel %d, "
		"BSSID "MACSTR", %s, caps 0x%04x, SIR %d, SNR %d\n",
		bss->essid, bss->channel,
		MAC(bss->bssid),
        	(bss->cap_info & WF_MGMT_CAP_IBSS) ? "Ad-Hoc peer" : "Access Point",
		bss->cap_info, bss->sir, bss->snr);

	FN_EXIT0();
}

static const char * const status_str[] = {
  "Successful", "Unspecified failure",
  "Reserved error code", "Reserved error code", "Reserved error code",
  "Reserved error code", "Reserved error code", "Reserved error code",
  "Reserved error code", "Reserved error code",
  "Cannot support all requested capabilities in the Capability Information field. TRANSLATION: Bug in ACX100 driver?",
  "Reassociation denied due to reason outside the scope of 802.11b standard. TRANSLATION: Bug in ACX100 driver?",
  "Association denied due to reason outside the scope of 802.11b standard. TRANSLATION: peer station may have MAC filtering enabled or assoc to wrong AP (BUG), FIX IT!",
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

static inline const char *get_status_string(int status)
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
 * Called only in STA_SCAN or AP mode
 *----------------------------------------------------------------*/
static int acx_process_authen(const wlan_fr_authen_t *req, wlandevice_t *priv)
{
	const p80211_hdr_t *hdr;
	client_t *clt;
	wlan_ie_challenge_t *chal;
	u16 alg,seq,status;
	int ap,result;

	FN_ENTER;

	hdr = req->hdr;

	acx_log_mac_address(L_ASSOC, priv->dev_addr, " ");
	acx_log_mac_address(L_ASSOC, hdr->a3.a1, " ");
	acx_log_mac_address(L_ASSOC, hdr->a3.a2, " ");
	acx_log_mac_address(L_ASSOC, hdr->a3.a3, " ");
	acx_log_mac_address(L_ASSOC, priv->bssid, "\n");

	/* TODO: move first check into acx_rx_ieee802_11_frame(),
	** it's not auth specific */
	if (!mac_is_equal(priv->dev_addr, hdr->a3.a1)
	 || !mac_is_equal(priv->bssid, hdr->a3.a3)) {
		result = OK;
		goto end;
	}

	alg = ieee2host16(*(req->auth_alg));
	seq = ieee2host16(*(req->auth_seq));
	status = ieee2host16(*(req->status));

	ap = (priv->mode == ACX_MODE_3_AP);

	if (priv->auth_alg <= 1) {
		if (priv->auth_alg != alg) {
			acxlog(L_ASSOC, "authentication algorithm mismatch: "
				"want: %d, req: %d\n", priv->auth_alg, alg);
			result = NOT_OK;
			goto end;
		}
	}
	acxlog(L_ASSOC, "Algorithm is ok\n");

	if (ap) {
		clt = acx_sta_list_get_or_add(priv, hdr->a3.a2);
		if (!clt) {
			acxlog(L_ASSOC, "could not allocate room for client\n");
			result = NOT_OK;
			goto end;
		}
	} else {
		clt = priv->ap_client;
		if (!mac_is_equal(clt->address, hdr->a3.a2)) {
			acxlog(L_ASSOC, "assoc frame from rogue AP?!\n");
			result = NOT_OK;
			goto end;
		}
	}

	/* now check which step in the authentication sequence we are
	 * currently in, and act accordingly */
	acxlog(L_ASSOC, "acx_process_authen auth seq step %d\n",seq);
	switch (seq) {
	case 1:
		if (!ap)
			break;
		acx_transmit_authen2(req, clt, priv);
		break;
	case 2:
		if (ap)
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
		if (!ap)
			break;
		if ((clt->auth_alg != WLAN_AUTH_ALG_SHAREDKEY)
		 || (alg != WLAN_AUTH_ALG_SHAREDKEY)
		 || (clt->auth_step != 2))
			break;
		chal = req->challenge;
		if (!chal
		 || memcmp(chal->challenge, clt->challenge_text, WLAN_CHALLENGE_LEN)
		 || (chal->eid != WLAN_EID_CHALLENGE)
		 || (chal->len != WLAN_CHALLENGE_LEN)
		)
			break;
		acx_transmit_authen4(req, priv);
		MAC_COPY(clt->address, hdr->a3.a2);
		clt->used = CLIENT_AUTHENTICATED_2;
		clt->auth_step = 4;
		clt->seq = ieee2host16(hdr->a3.seq);
		break;
	case 4:
		if (ap)
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
static int acx_transmit_authen2(const wlan_fr_authen_t *req, client_t *clt,
		      wlandevice_t *priv)
{
	struct txdescriptor *tx_desc;
	struct txhostdescriptor *hdesc_head;
	struct txhostdescriptor *hdesc_body;
	struct wlan_hdr_mgmt *head;
	struct auth_frame_body *body;
	unsigned int packet_len;

	FN_ENTER;

	if (!clt) {
		FN_EXIT1(OK);
		return OK;
	}

	MAC_COPY(clt->address, req->hdr->a3.a2);
#if UNUSED
	clt->ps = ((WF_FC_PWRMGTi & req->hdr->a3.fc) != 0);
#endif
	clt->auth_alg = ieee2host16(*(req->auth_alg));
	clt->auth_step = 2;
	clt->seq = ieee2host16(req->hdr->a3.seq);

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
		clt->used = CLIENT_AUTHENTICATED_2;
	} else {	/* shared key */
		acx_gen_challenge(&body->challenge);
		memcpy(&clt->challenge_text, body->challenge.text, WLAN_CHALLENGE_LEN);
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
*
* priv->ap_client is a current candidate AP here
*----------------------------------------------------------------*/
static int acx_transmit_assoc_req(wlandevice_t *priv)
{
	struct txdescriptor *tx_desc;
	struct txhostdescriptor *hdesc_head;
	struct txhostdescriptor *hdesc_body;
	struct wlan_hdr_mgmt *head;
	u8 *body, *p, *prate;
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

	head->fc = WF_FSTYPE_ASSOCREQi;
	head->dur = host2ieee16(0x8000);
	MAC_COPY(head->da, priv->bssid);
	MAC_COPY(head->sa, priv->dev_addr);
	MAC_COPY(head->bssid, priv->bssid);
	head->seq = 0;

	hdesc_head->length = cpu_to_le16(WLAN_HDR_A3_LEN);
	hdesc_head->data_offset = 0;

	p = body;
	/* now start filling the AssocReq frame body */

	/* since this assoc request will most likely only get
	 * sent in the STA to AP case (and not when Ad-Hoc IBSS),
	 * the cap combination indicated here will thus be
	 * WF_MGMT_CAP_ESSi *always* (no IBSS ever)
	 * The specs are more than non-obvious on all that:
	 * 
	 * 802.11 7.3.1.4 Capability Information field
	** APs set the ESS subfield to 1 and the IBSS subfield to 0 within
	** Beacon or Probe Response management frames. STAs within an IBSS
	** set the ESS subfield to 0 and the IBSS subfield to 1 in transmitted
	** Beacon or Probe Response management frames
	**
	** APs set the Privacy subfield to 1 within transmitted Beacon,
	** Probe Response, Association Response, and Reassociation Response
	** if WEP is required for all data type frames within the BSS.
	** STAs within an IBSS set the Privacy subfield to 1 in Beacon
	** or Probe Response management frames if WEP is required
	** for all data type frames within the IBSS */

	/* note that returning 0 will be refused by several APs...
	 * (so this indicates that you're probably supposed to
	 * "confirm" the ESS mode) */
	cap = WF_MGMT_CAP_ESSi;

	/* this one used to be a check on wep_restricted,
	 * but more likely it's wep_enabled instead */
	if (priv->wep_enabled)
		SET_BIT(cap, WF_MGMT_CAP_PRIVACYi);

	/* Probably we can just set these always, because our hw is
	** capable of shortpre and PBCC --vda */
	/* only ask for short preamble if the peer station supports it */
	if (priv->ap_client->cap_info & WF_MGMT_CAP_SHORT)
		SET_BIT(cap, WF_MGMT_CAP_SHORTi);
	/* only ask for PBCC support if the peer station supports it */
	if (priv->ap_client->cap_info & WF_MGMT_CAP_PBCC)
		SET_BIT(cap, WF_MGMT_CAP_PBCCi);

	/* IEs: 1. caps */
	*(u16*)p = cap;	p += 2;
	/* 2. listen interval */
	*(u16*)p = host2ieee16(priv->listen_interval); p += 2;
	/* 3. ESSID */
	p = wlan_fill_ie_ssid(p, strlen(priv->essid_for_assoc), priv->essid_for_assoc);
	/* 4. supp rates */
	prate = p;
	p = wlan_fill_ie_rates(p, priv->rate_supported_len, priv->rate_supported);
	/* 5. ext supp rates */
	p = wlan_fill_ie_rates_ext(p, priv->rate_supported_len, priv->rate_supported);

	if (debug & L_DEBUG)
	{
		acxlog(L_DEBUG, "association: rates element\n");
		acx_dump_bytes(prate, (int)p - (int)prate);
	}

	/* calculate lengths */
	packet_len = WLAN_HDR_A3_LEN + (p - body);

	acxlog(L_ASSOC, "association: requesting caps 0x%04X, ESSID %s\n", cap, priv->essid_for_assoc);

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
* Iterates thru priv->sta_list:
*	if priv->ap is not bcast, will join only specified
*	ESS or IBSS with this bssid
*	checks peers' caps for ESS/IBSS bit
*	checks peers' SSID, allows exact match or hidden SSID
* If station to join is chosen:
*	points priv->ap_client to the chosen struct client
*	sets priv->essid_for_assoc for future assoc attempt
* Auth/assoc is not yet performed
*----------------------------------------------------------------*/
void acx_complete_dot11_scan(wlandevice_t *priv)
{
	struct client *bss;
	u16 needed_cap;
	int i;
	int idx_found = -1;

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
	
	for (i = 0; i < VEC_SIZE(priv->sta_list); i++) {
		bss = &priv->sta_list[i];
		if (!bss->used) continue;

		acxlog(L_BINDEBUG | L_ASSOC,
			"<Scan Table>: SSID=\"%s\",CH=%d,SIR=%d,SNR=%d\n",
			bss->essid, bss->channel, bss->sir, bss->snr);

		if (!mac_is_bcast(priv->ap))
			if (!mac_is_equal(bss->bssid, priv->ap))
				continue; /* keep looking */

		/* broken peer with no mode flags set? */
		if (unlikely(!(bss->cap_info & (WF_MGMT_CAP_ESS | WF_MGMT_CAP_IBSS)))) {
			acxlog(L_ASSOC, "STRANGE: peer station announces "
				"neither ESS (Managed) nor IBSS (Ad-Hoc) "
				"capability. Won't try to join it\n");
			continue;
		}
		acxlog(L_ASSOC, "peer_cap 0x%04x, needed_cap 0x%04x\n",
		       bss->cap_info, needed_cap);

		/* does peer station support what we need? */
		if ((bss->cap_info & needed_cap) != needed_cap)
			continue; /* keep looking */

		/* strange peer with NO basic rates?! */
		if (unlikely(!bss->rate_cfg))
		{
			acxlog(L_ASSOC, "skip strange peer %i: NO rates\n", i);
			continue;
		}

		/* do we support all basic rates of this peer? */
		if ((bss->rate_cfg & priv->rate_oper) != bss->rate_cfg)
		{
/* we probably need to have all rates as operational rates,
   even in case of an 11M-only configuration */
#if THIS_IS_TROUBLESOME
			acxlog(L_ASSOC, "skip peer %i: incompatible basic rates (AP requests 0x%04x, we have 0x%04x)\n", i, bss->rate_cfg, priv->rate_oper);
			continue;
#else
			acxlog(L_ASSOC, "peer %i: incompatible basic rates (AP requests 0x%04x, we have 0x%04x). Considering anyway...\n", i, bss->rate_cfg, priv->rate_oper);
#endif
		}

		if ( !(priv->reg_dom_chanmask & (1<<(bss->channel-1))) ) {
			acxlog(L_STD|L_ASSOC, "WARNING: peer station %d "
				"is using channel %d, which is outside "
				"the channel range of the regulatory domain "
				"the driver is currently configured for: "
				"couldn't join in case of matching settings, "
				"might want to adapt your config!\n",
				i, bss->channel);
			continue; /* keep looking */
		}

		if (!priv->essid_active || !strcmp(bss->essid, priv->essid)) {
			acxlog(L_ASSOC,
			       "found station with matching ESSID! (\"%s\" "
			       "station, \"%s\" config)\n",
			       bss->essid,
			       (priv->essid_active) ? priv->essid : "[any]");
			/* TODO: continue looking for peer with better SNR */
			bss->used = CLIENT_JOIN_CANDIDATE;
			idx_found = i;

			/* stop searching if this station is
			 * on the current channel, otherwise
			 * keep looking for an even better match */
			if (bss->channel == priv->channel)
				break;
		} else
		if (!bss->essid[0]
		 || ((' ' == bss->essid[0]) && !bss->essid[1])
		) {
			/* hmm, station with empty or single-space SSID:
			 * using hidden SSID broadcast?
			 */
			/* This behaviour is broken: which AP from zillion
			** of APs with hidden SSID you'd try?
			** We should use Probe requests to get Probe responses
			** and check for real SSID (are those never hidden?) */
			bss->used = CLIENT_JOIN_CANDIDATE;
			if (idx_found == -1)
				idx_found = i;
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

	/* TODO: iterate thru join candidates instead */
	/* TODO: rescan if not associated within some timeout */
	if (idx_found != -1) {
		char *essid_src;
		size_t essid_len;

		bss = &priv->sta_list[idx_found];
		priv->ap_client = bss;

		if (bss->essid[0] == '\0') {
			/* if the ESSID of the station we found is empty
			 * (no broadcast), then use user configured ESSID
			 * instead */
			essid_src = priv->essid;
			essid_len = priv->essid_len;
		}
		else {
			essid_src = bss->essid;
			essid_len = strlen(bss->essid);
		}
		
		acx_update_capabilities(priv);

		memcpy(priv->essid_for_assoc, essid_src, essid_len);
		priv->essid_for_assoc[essid_len] = '\0';
		priv->channel = bss->channel;
		MAC_COPY(priv->bssid, bss->bssid);

		bss->rate_cfg = (bss->rate_cap & priv->rate_oper);
		bss->rate_cur = 1 << lowest_bit(bss->rate_cfg);
		bss->rate_100 = RATE100_1;	/* quick & dirty. FIXME */

		acxlog(L_STD | L_ASSOC,
		       "%s: matching station found: "MACSTR", joining\n",
		       __func__, MAC(priv->bssid));

		/* Inform firmware on our decision */
		acx_cmd_join_bssid(priv, priv->bssid);

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
			/* we shall scan again, AP can be
			** just temporarily powered off */
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
		if (++priv->scan_retries < 5) {
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
			acxlog(L_ASSOC, "Stopping scan");
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
