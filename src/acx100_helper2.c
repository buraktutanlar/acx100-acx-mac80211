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

#include <p80211hdr.h>
#include <p80211mgmt.h>
#include <acx100.h>
#include <p80211types.h>
#include <acx100_helper.h>
#include <idma.h>
#include <acx100_helper2.h>
#include <ihw.h>
#include <acx80211frm.h>

static client_t *acx_sta_list_alloc(wlandevice_t *priv, const u8 *address); 
static client_t *acx_sta_list_add(wlandevice_t *priv, const u8 *address);
static inline client_t *acx_sta_list_get_from_hash(wlandevice_t *priv, const u8 *address);
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
static void acx_process_probe_response(const struct rxbuffer *mmt, wlandevice_t *priv,
 		                          const acxp80211_hdr_t *hdr);
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


static alloc_p80211_mgmt_req_t alloc_p80211mgmt_req;

static u16 CurrentAID = 1;

static const char * const state_str[] = { "STARTED", "SCANNING", "WAIT_AUTH", "AUTHENTICATED", "ASSOCIATED", "INVALID??" };

static const u8 bcast_addr[ETH_ALEN] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

/*----------------------------------------------------------------
* acx_sta_list_init
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

/* acx_sta_list_init()
 * STATUS: should be ok..
 */
void acx_sta_list_init(wlandevice_t *priv)
{
	FN_ENTER;
	memset(priv->sta_hash_tab, 0, sizeof(priv->sta_hash_tab));
	memset(priv->sta_list, 0, sizeof(priv->sta_list));
	FN_EXIT0();
}

/*----------------------------------------------------------------
* acx_sta_list_alloc
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

/* acx_sta_list_alloc()
 * STATUS: FINISHED, except for struct defs.
 * Hmm, does this function have one "silent" parameter or 0 parameters?
 * Doesn't matter much anyway...
 */
static inline client_t *acx_sta_list_alloc(wlandevice_t *priv, const u8 *address)
{
	unsigned int i = 0;

	FN_ENTER;
	for (i = 0; i <= 31; i++) {
		if (priv->sta_list[i].used == 0) {
			priv->sta_list[i].used = 1;
			priv->sta_list[i].auth_alg = WLAN_AUTH_ALG_SHAREDKEY;
			priv->sta_list[i].val0xe = 1;
			FN_EXIT1((int)&(priv->sta_list[i]));
			return &(priv->sta_list[i]);
		}
	}
	FN_EXIT1((int)NULL);
	return NULL;
}

/*----------------------------------------------------------------
* acx_sta_list_add
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

/* acx_sta_list_add()
 * STATUS: FINISHED.
 */
static client_t *acx_sta_list_add(wlandevice_t *priv, const u8 *address)
{
	client_t *client;
	int index;

	FN_ENTER;
	client = acx_sta_list_alloc(priv, address);
	if (NULL == client)
		goto done;

	/* computing hash table index */
	index = ((address[4] << 8) + address[5]);
	index -= index & 0x3ffc0;

	client->next = priv->sta_hash_tab[index];
	priv->sta_hash_tab[index] = client;

	acxlog(L_BINSTD | L_ASSOC,
	       "<acx_sta_list_add> sta = %02X:%02X:%02X:%02X:%02X:%02X\n",
	       address[0], address[1], address[2],
	       address[3], address[4], address[5]);

done:
	FN_EXIT1((int) client);
	return client;
}

/*----------------------------------------------------------------
* acx_sta_list_get_from_hash
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


static inline client_t *acx_sta_list_get_from_hash(wlandevice_t *priv, const u8 *address)
{
	int index;

	FN_ENTER;
	/* computing hash table index */
	index = ((address[4] << 8) + address[5]);
	index -= index & 0x3ffc0;

	FN_EXIT0();
	return priv->sta_hash_tab[index];
}

/*----------------------------------------------------------------
* acx_sta_list_get
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

/* acx_get_sta_list()
 * STATUS: FINISHED.
 */
static client_t *acx_sta_list_get(wlandevice_t *priv, const u8 *address)
{
	client_t *client;
	client_t *result = NULL;	/* can be removed if tracing unneeded */

	FN_ENTER;
	client = acx_sta_list_get_from_hash(priv, address);

	for (; client; client = client->next) {
		if (0 == memcmp(address, client->address, ETH_ALEN)) {
			result = client;
			goto done;
		}
	}

done:
	FN_EXIT1((int) result);
	return result;
}

#define VEC_SIZE(a) (sizeof(a)/sizeof(a[0]))

inline const char *acx_get_status_name(u16 status)
{
	if (status < VEC_SIZE(state_str))
		return state_str[status];
	else
		return state_str[VEC_SIZE(state_str)-1];
}

/*----------------------------------------------------------------
* acx_set_status
*
* Arguments:
*
* Returns:
*
* Side effects:
*
* Call context:
*
* STATUS: should be ok.
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
	if (ISTATUS_4_ASSOCIATED == new_status) {
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
		MAC_FILL(wrqu.ap_addr.sa_data, 0x0);
		wrqu.ap_addr.sa_family = ARPHRD_ETHER;
		wireless_send_event(priv->netdev, SIOCGIWAP, &wrqu, NULL);

		if (ISTATUS_0_STARTED == new_status) {
			if (memcmp(priv->netdev->dev_addr, priv->dev_addr, ETH_ALEN)) {
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
	}
#endif

	priv->status = new_status;

	if (priv->status == ISTATUS_1_SCANNING) {
		priv->scan_retries = 0;
		acx_set_timer(priv, 2500000); /* 2.5s initial scan time (used to be 1.5s, but failed to find WEP APs!) */
	} else if ((ISTATUS_2_WAIT_AUTH <= priv->status) && (ISTATUS_3_AUTHENTICATED >= priv->status)) {
		priv->auth_or_assoc_retries = 0;
		acx_set_timer(priv, 1500000); /* 1.5 s */
	}

#if QUEUE_OPEN_AFTER_ASSOC
	if (new_status == ISTATUS_4_ASSOCIATED)	{
		if (old_status < ISTATUS_4_ASSOCIATED) {
			/* ah, we're newly associated now,
			 * so let's indicate carrier */
			acx_carrier_on(priv->netdev, "after association");
			acx_wake_queue(priv->netdev, "after association");
		}
	} else {
		/* not associated any more, so let's kill carrier */
		if (old_status >= ISTATUS_4_ASSOCIATED) {
			acx_carrier_off(priv->netdev, "after losing association");
			acx_stop_queue(priv->netdev, "after losing association");
		}
	}
#endif
	FN_EXIT0();
}

static inline p80211_hdr_t *acx_get_p80211_hdr(wlandevice_t *priv, const rxhostdescriptor_t *rxdesc)
{
	if (priv->rx_config_1 & RX_CFG1_INCLUDE_ADDIT_HDR) {
		/* take into account additional header in front of packet */
		return (p80211_hdr_t *)((u8 *)&rxdesc->data->buf + 4);
	} else {
		return (p80211_hdr_t *)&rxdesc->data->buf;
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
int acx_rx_ieee802_11_frame(wlandevice_t *priv, rxhostdescriptor_t *rxdesc)
{
	unsigned int ftype, fstype;
	const p80211_hdr_t *p80211_hdr;
	int result = NOT_OK;

	FN_ENTER;

	p80211_hdr = acx_get_p80211_hdr(priv, rxdesc);
/*	printk("Rx_CONFIG_1 = %X\n",priv->rx_config_1 & RX_CFG1_INCLUDE_ADDIT_HDR); */

	/* see IEEE 802.11-1999.pdf chapter 7 "MAC frame formats" */
	ftype = WLAN_GET_FC_FTYPE(ieee2host16(p80211_hdr->a3.fc));
	fstype = WLAN_GET_FC_FSTYPE(ieee2host16(p80211_hdr->a3.fc));

	switch (ftype) {
	/* check data frames first, for speed */
	case WLAN_FTYPE_DATA:
#ifdef BUGGY
		/* binary driver did ftype-1 to appease jump
		 * table layout */
		if (fstype == WLAN_FSTYPE_DATAONLY) {
			if (ACX_MODE_3_MANAGED_AP == priv->macmode_joined) {
				result = acx_process_data_frame_master(rxdesc, priv);
			} else if (ISTATUS_4_ASSOCIATED == priv->status) {
				result = acx_process_data_frame_client(rxdesc, priv);
			}
		} else switch (ftype) {
		case WLAN_FSTYPE_DATA_CFACK:
		case WLAN_FSTYPE_DATA_CFPOLL:
		case WLAN_FSTYPE_DATA_CFACK_CFPOLL:
		case WLAN_FSTYPE_CFPOLL:
		case WLAN_FSTYPE_CFACK_CFPOLL:
		/*   see above.
		   acx_process_class_frame(rxdesc, priv, 3); */
			break;
		case WLAN_FSTYPE_NULL:
			acx_process_NULL_frame(rxdesc, priv, 3);
			break;
		/* FIXME: same here, see above */
		case WLAN_FSTYPE_CFACK:
		default:
			break;
		}
#else
		switch (fstype) {
		case WLAN_FSTYPE_DATAONLY:
			if (ACX_MODE_3_MANAGED_AP == priv->macmode_joined) {
				result = acx_process_data_frame_master(rxdesc, priv);
			} else if (ISTATUS_4_ASSOCIATED == priv->status) {
				result = acx_process_data_frame_client(rxdesc, priv);
			}
		case WLAN_FSTYPE_DATA_CFACK:
		case WLAN_FSTYPE_DATA_CFPOLL:
		case WLAN_FSTYPE_DATA_CFACK_CFPOLL:
		case WLAN_FSTYPE_CFPOLL:
		case WLAN_FSTYPE_CFACK_CFPOLL:
		/*   see above.
		   acx_process_class_frame(rxdesc, priv, 3); */
			break;
		case WLAN_FSTYPE_NULL:
			acx_process_NULL_frame(rxdesc, priv, 3);
			break;
		/* FIXME: same here, see above */
		case WLAN_FSTYPE_CFACK:
		default:
			break;
		}
#endif
		break;
	case WLAN_FTYPE_MGMT:
		result = acx_process_mgmt_frame(rxdesc, priv);
		break;
	case WLAN_FTYPE_CTL:
		if (fstype != WLAN_FSTYPE_PSPOLL)
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

/* acx_transmit_assocresp()
 * STATUS: should be ok, but UNVERIFIED.
 */
static u32 acx_transmit_assocresp(const wlan_fr_assocreq_t *arg_0,
			  wlandevice_t *priv)
{
	u8 var_1c[6];
	const u8 *da;
	const u8 *sa;
	const u8 *bssid;
	TxData *hd;
	struct assocresp_frame_body *payload;
	struct txdescriptor *tx_desc;
	struct txhostdescriptor *hdesc_header;
	struct txhostdescriptor *hdesc_payload;
	client_t *clt;

	FN_ENTER;
	/* FIXME: or is the order the other way round ?? */
	var_1c[0] = 0;
	var_1c[1] = 1;
	var_1c[2] = 3;
	var_1c[3] = 7;
	var_1c[4] = 0xf;
	var_1c[5] = 0x1f;

	acxlog(L_BINDEBUG | L_ASSOC | L_XFER, "<acx_transmit_assocresp 1>\n");

	if (WLAN_GET_FC_TODS(ieee2host16(arg_0->hdr->a3.fc)) || WLAN_GET_FC_FROMDS(ieee2host16(arg_0->hdr->a3.fc))) {
		FN_EXIT1(NOT_OK);
		return NOT_OK;
	}
	
	sa = arg_0->hdr->a3.a1;
	da = arg_0->hdr->a3.a2;
	bssid = arg_0->hdr->a3.a3;

	clt = acx_sta_list_get(priv, da);

	if (!clt) {
		FN_EXIT1(OK);
		return OK;
	}

	if (clt->used == 1) {
		acx_transmit_deauthen(da, clt, priv, WLAN_MGMT_REASON_CLASS2_NONAUTH /* 6 */);
		FN_EXIT0();
		return NOT_OK;
	}

	clt->used = 3;

	if (clt->aid == 0) {
		clt->aid = CurrentAID;
		CurrentAID++;
	}
	clt->val0xa = ieee2host16(*(arg_0->listen_int));

	memcpy(clt->val0x10, arg_0->ssid, arg_0->ssid->len);

	/* FIXME: huh, why choose the ESSID length
	 * directly as the index!?!? */
	if (arg_0->ssid->len <= 5) {
		clt->val0x9a = var_1c[arg_0->ssid->len];
	} else {
		clt->val0x9a = 0x1f;
	}

	tx_desc = acx_get_tx_desc(priv);
	if (!tx_desc) {
		FN_EXIT1(NOT_OK);
		return NOT_OK;
	}

	hdesc_header = tx_desc->fixed_size.s.host_desc;
	hdesc_payload = tx_desc->fixed_size.s.host_desc + 1;

	hd = (TxData *)hdesc_header->data;
	payload = (struct assocresp_frame_body *)hdesc_payload->data;

	hd->frame_control = host2ieee16(WLAN_SET_FC_FSTYPE(WLAN_FSTYPE_ASSOCRESP));	/* 0x10 */
	hd->duration_id = arg_0->hdr->a3.dur;

	MAC_COPY(hd->da, da);
	MAC_COPY(hd->sa, sa);
	MAC_COPY(hd->bssid, bssid);

	hd->sequence_control = arg_0->hdr->a3.seq;

	hdesc_header->length = cpu_to_le16(WLAN_HDR_A3_LEN);
	hdesc_header->data_offset = 0;

	payload->cap_info = host2ieee16(priv->capabilities);
	payload->status = host2ieee16(0);
	payload->aid = host2ieee16(clt->aid);

	payload->rates.element_ID = 1;
	payload->rates.length = priv->rate_supported_len;
	memcpy(payload->rates.sup_rates, priv->rate_supported, priv->rate_supported_len);
	hdesc_payload->length = cpu_to_le16(priv->rate_supported_len + 8);
	hdesc_payload->data_offset = 0;

	tx_desc->total_length = cpu_to_le16(WLAN_HDR_A3_LEN + priv->rate_supported_len + 8);

	acx_dma_tx_data(priv, tx_desc);

	FN_EXIT1(OK);
	return OK;
}

/*----------------------------------------------------------------
* acx_transmit_reassocresp
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

/* acx_transmit_reassocresp()
 * STATUS: should be ok, but UNVERIFIED.
 */
static u32 acx_transmit_reassocresp(const wlan_fr_reassocreq_t *arg_0, wlandevice_t *priv)
{
	const u8 *da = NULL;
	const u8 *sa = NULL;
	const u8 *bssid = NULL;
	struct reassocresp_frame_body *payload;
	struct txdescriptor *tx_desc;
	struct txhostdescriptor *hdesc_header;
	struct txhostdescriptor *hdesc_payload;

	client_t *clt;
	TxData *fr;

	FN_ENTER;
	if (WLAN_GET_FC_TODS(ieee2host16(arg_0->hdr->a3.fc)) || WLAN_GET_FC_FROMDS(ieee2host16(arg_0->hdr->a3.fc))) {
		FN_EXIT1(NOT_OK);
		return NOT_OK;
	}

	sa = arg_0->hdr->a3.a1;
	da = arg_0->hdr->a3.a2;
	bssid = arg_0->hdr->a3.a3;

	clt = acx_sta_list_get(priv, da);
	if (clt != NULL) {
		if (clt->used == 1)
			clt->used = 2;
	} else {
		clt = acx_sta_list_add(priv, da);
		MAC_COPY(clt->address, da);
		clt->used = 2;
	}

	if (clt->used != 2) {
		FN_EXIT1(OK);
		return OK;
	}

	clt->used = 3;
	if (clt->aid == 0) {
		clt->aid = CurrentAID;
		CurrentAID++;
	}
	clt->val0xa = ieee2host16(*(arg_0->cap_info));

	memcpy(clt->val0x10, arg_0->supp_rates, arg_0->supp_rates->len);

	switch (arg_0->supp_rates->len) {
	case 1:
		clt->val0x9a = 1;
		break;
	case 2:
		clt->val0x9a = 3;
		break;
	case 3:
		clt->val0x9a = 7;
		break;
	case 4:
		clt->val0x9a = 0xf;
		break;
	default:
		clt->val0x9a = 0x1f;
		break;
	}

	tx_desc = acx_get_tx_desc(priv);
	if (!tx_desc) {
		FN_EXIT1(OK);
		return OK;
	}

	hdesc_header = tx_desc->fixed_size.s.host_desc;
	hdesc_payload = tx_desc->fixed_size.s.host_desc + 1;
	fr = (TxData*)hdesc_header->data;
	payload = (struct reassocresp_frame_body *)hdesc_payload->data;
	fr->frame_control = host2ieee16(WLAN_SET_FC_FSTYPE(WLAN_FSTYPE_REASSOCRESP));	/* 0x30 */
	fr->duration_id = arg_0->hdr->a3.dur;

	MAC_COPY(fr->da, da);
	MAC_COPY(fr->sa, sa);
	MAC_COPY(fr->bssid, bssid);

	fr->sequence_control = arg_0->hdr->a3.seq;

	hdesc_header->length = cpu_to_le16(WLAN_HDR_A3_LEN);
	hdesc_header->data_offset = 0;

	payload->cap_info = host2ieee16(priv->capabilities);
	payload->status = host2ieee16(0);
	payload->aid = host2ieee16(clt->aid);

	payload->rates.element_ID = 1;
	payload->rates.length = priv->rate_supported_len;
	memcpy(payload->rates.sup_rates, priv->rate_supported, priv->rate_supported_len);
	hdesc_payload->data_offset = 0;
	hdesc_payload->length = cpu_to_le16(priv->rate_supported_len + 8);

	tx_desc->total_length = cpu_to_le16(WLAN_HDR_A3_LEN + priv->rate_supported_len + 8);

	acx_dma_tx_data(priv, tx_desc);

	FN_EXIT1(OK);

	return OK;
}

/*----------------------------------------------------------------
* acx_process_disassoc
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

/* acx_process_disassoc()
 * STATUS: UNVERIFIED.
 */
static int acx_process_disassoc(const wlan_fr_disassoc_t *arg_0, wlandevice_t *priv)
{
	const p80211_hdr_t *hdr;
	int res = 0;
	const u8 *TA = NULL;
	client_t *clts;

	FN_ENTER;
	hdr = arg_0->hdr;

	if (WLAN_GET_FC_TODS(ieee2host16(hdr->a4.fc)) || WLAN_GET_FC_FROMDS(ieee2host16(hdr->a4.fc))) {
		res = 1;
		goto ret;
	}

	TA = hdr->a3.a2;
	clts = acx_sta_list_get(priv, TA);
	if (!clts) {
		res = 1;
		goto ret;
	}
	if (clts->used == 1) {
		acxlog(L_BINDEBUG | L_ASSOC | L_XFER, "<transmit_deauth 2>\n");
		acx_transmit_deauthen(TA, clts, priv, WLAN_MGMT_REASON_CLASS2_NONAUTH /* 6 */);
	} else
		clts->used = 2;
ret:
	FN_EXIT1(res);
	return res;
}

/*----------------------------------------------------------------
* acx_process_disassociate
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

/* acx_process_disassociate()
 * STATUS: UNVERIFIED.
 */
static int acx_process_disassociate(const wlan_fr_disassoc_t *req, wlandevice_t *priv)
{
	const p80211_hdr_t *hdr;
	int res = 0;

	FN_ENTER;
	hdr = req->hdr;

	if (WLAN_GET_FC_TODS(ieee2host16(hdr->a3.fc)) || WLAN_GET_FC_FROMDS(ieee2host16(hdr->a3.fc)))
		res = 1;
	else {
		if (ACX_MODE_0_IBSS_ADHOC == priv->macmode_joined)
			res = 1;
		else if (OK == acx_is_mac_address_equal(priv->dev_addr, hdr->a3.a1 /* RA */)) {
			res = 1;
			if (priv->status > ISTATUS_3_AUTHENTICATED) {
				/* priv->val0x240 = ieee2host16(*(req->reason)); Unused, so removed */
				acx_set_status(priv, ISTATUS_3_AUTHENTICATED);
#if (POWER_SAVE_80211 == 0)
				ActivatePowerSaveMode(priv, 2);
#endif
			}
			res = 0;
		} else
			res = 1;
	}
	FN_EXIT1(res);
	return res;
}

/*----------------------------------------------------------------
* acx_process_data_frame_master
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

static int acx_process_data_frame_master(struct rxhostdescriptor *rxdesc, wlandevice_t *priv)
{
	const client_t *clt, *var_24 = NULL;
	p80211_hdr_t* p80211_hdr;
	struct txdescriptor *tx_desc;
	u8 esi = 0;
	int result = NOT_OK;
	unsigned int to_ds, from_ds;
	const u8 *da, *sa, *bssid;

	FN_ENTER;

        p80211_hdr = acx_get_p80211_hdr(priv, rxdesc);

	to_ds = WLAN_GET_FC_TODS(ieee2host16(p80211_hdr->a3.fc));
	from_ds = WLAN_GET_FC_FROMDS(ieee2host16(p80211_hdr->a3.fc));

	if ((!to_ds) && (!from_ds)) {
		/* To_DS = 0, From_DS = 0 */
		da = p80211_hdr->a3.a1;
		sa = p80211_hdr->a3.a2;
		bssid = p80211_hdr->a3.a3;
	} else
	if ((!to_ds) && (from_ds)) {
		/* To_DS = 0, From_DS = 1 */
		da = p80211_hdr->a3.a1;
		sa = p80211_hdr->a3.a3;
		bssid = p80211_hdr->a3.a2;
	} else
	if ((to_ds) && (!from_ds)) {
		/* To_DS = 1, From_DS = 0 */
		da = p80211_hdr->a3.a3;
		sa = p80211_hdr->a3.a2;
		bssid = p80211_hdr->a3.a1;
	} else {
		/* To_DS = 1, From_DS = 1 */
		acxlog(L_DEBUG, "WDS frame received. Unimplemented\n");
		goto done;
	}

	/* check if it is our BSSID, if not, leave */
	if (memcmp(bssid, priv->bssid, ETH_ALEN) != 0) {
		goto done;
	}

	clt = acx_sta_list_get(priv, bcast_addr);
	if (!clt || (clt->used != 3)) {
		acx_transmit_deauthen(bcast_addr, 0, priv, WLAN_MGMT_REASON_RSVD /* 0 */);
		acxlog(L_STD, "frame error #2??\n");
		priv->stats.rx_errors++;
		goto fail;
	} else {
		esi = 2;
		/* check if the da is not broadcast */
		if (OK != acx_is_mac_address_broadcast(da)) {
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
			p80211_hdr->a3.fc =
			    host2ieee16(WLAN_SET_FC_FROMDS(1) +
			    WLAN_SET_FC_FTYPE(WLAN_FTYPE_DATA));

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

/* acx_process_data_frame_client()
 * STATUS: FINISHED, UNVERIFIED.
 */
static int acx_process_data_frame_client(struct rxhostdescriptor *rxdesc, wlandevice_t *priv)
{
	const u8 *da, *bssid;
	const p80211_hdr_t *p80211_hdr;
	/* unsigned int to_ds, from_ds; */
	int result = NOT_OK;
	netdevice_t *dev = priv->netdev;

	FN_ENTER;

	p80211_hdr = acx_get_p80211_hdr(priv, rxdesc);

#if 0
	to_ds = WLAN_GET_FC_TODS(ieee2host16(p80211_hdr->a3.fc));
	from_ds = WLAN_GET_FC_FROMDS(ieee2host16(p80211_hdr->a3.fc));

	acxlog(L_DEBUG, "rx: to_ds %i, from_ds %i\n", to_ds, from_ds);

	if ((!to_ds) && (!from_ds)) {
		/* To_DS = 0, From_DS = 0 */
		da = p80211_hdr->a3.a1;
		bssid = p80211_hdr->a3.a3;
	} else
	if ((!to_ds) && (from_ds)) {
		/* To_DS = 0, From_DS = 1 */
		da = p80211_hdr->a3.a1;
		bssid = p80211_hdr->a3.a2;
	} else
	if ((to_ds) && (!from_ds)) {
		/* To_DS = 1, From_DS = 0 */
		da = p80211_hdr->a3.a3;
		bssid = p80211_hdr->a3.a1;
	} else {
		/* To_DS = 1, From_DS = 1 */
		acxlog(L_DEBUG, "WDS frame received. Unimplemented\n");
		goto drop;
	}
#else
	switch (ieee2host16(p80211_hdr->a3.fc) & 0x300) {
	case 0x000:
		/* To_DS = 0, From_DS = 0 */
		da = p80211_hdr->a3.a1;
		bssid = p80211_hdr->a3.a3;
		break;
	case 0x200:
		/* To_DS = 0, From_DS = 1 */
		da = p80211_hdr->a3.a1;
		bssid = p80211_hdr->a3.a2;
		break;
	case 0x100:
		/* To_DS = 1, From_DS = 0 */
		da = p80211_hdr->a3.a3;
		bssid = p80211_hdr->a3.a1;
		break;
	default: /* 0x300 */
		/* To_DS = 1, From_DS = 1 */
		acxlog(L_DEBUG, "WDS frame received. Unimplemented\n");
		goto drop;
	}
#endif

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
        if (OK != acx_is_mac_address_equal(priv->bssid, bssid)) {
		/* is not our BSSID, so bail out */
		goto drop;
	}

	/* then, check if it is our address */
	if (OK == acx_is_mac_address_directed((mac_t *)da)) {
		if (OK == acx_is_mac_address_equal(da, priv->dev_addr)) {
			goto process;
		}
	}

	/* then, check if it is broadcast */
	if (OK == acx_is_mac_address_broadcast(da)) {
		goto process;
	}
	
	if (OK == acx_is_mac_address_multicast((mac_t *)da)) {
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

/* acx_process_mgmt_frame()
 * STATUS: FINISHED, UNVERIFIED. namechange!! (from process_mgnt_frame())
 * FIXME: uses global struct alloc_p80211mgmt_req, make sure there's no
 * race condition involved! (proper locking/processing)
 */
static u32 acx_process_mgmt_frame(struct rxhostdescriptor *rxdesc, wlandevice_t *priv)
{
	const u8 *a;
	p80211_hdr_t *p80211_hdr;
	unsigned int wep_offset = 0;

	FN_ENTER;

	p80211_hdr = acx_get_p80211_hdr(priv, rxdesc);
	if (WLAN_GET_FC_ISWEP(ieee2host16(p80211_hdr->a3.fc))) {
		wep_offset = 0x10;
	}

	switch (WLAN_GET_FC_FSTYPE(ieee2host16(p80211_hdr->a3.fc))) {
	/* beacons first, for speed */
	case WLAN_FSTYPE_BEACON /* 0x08 */ :
		if (ACX_MODE_3_MANAGED_AP == priv->macmode_joined) {
			acxlog(L_DEBUG,
			       "Incoming beacon message not handled in mode %d\n",
			       priv->macmode_joined);
			break;
		}

		switch (priv->status) {
		case ISTATUS_1_SCANNING:
			memset(&alloc_p80211mgmt_req.a.beacon, 0,
			       0xe * 4);
			alloc_p80211mgmt_req.a.beacon.buf =
			    (char *) p80211_hdr;
			alloc_p80211mgmt_req.a.beacon.len =
			    MAC_CNT_RCVD(rxdesc->data) - wep_offset;
			if (debug & L_DATA) {
				acxlog(L_DATA, "BCN fc: %X, dur: %X, seq: %X\n",
				       p80211_hdr->a3.fc, p80211_hdr->a3.dur, p80211_hdr->a3.seq);
				a = p80211_hdr->a3.a1;
				acxlog(L_DATA,
				       "BCN a1: %02X:%02X:%02X:%02X:%02X:%02X\n",
				       a[0], a[1], a[2], a[3], a[4], a[5]);
				a = p80211_hdr->a3.a2;
				acxlog(L_DATA,
				       "BCN a2: %02X:%02X:%02X:%02X:%02X:%02X\n",
				       a[0], a[1], a[2], a[3], a[4], a[5]);
				a = p80211_hdr->a3.a3;
				acxlog(L_DATA,
				       "BCN a3: %02X:%02X:%02X:%02X:%02X:%02X\n",
				       a[0], a[1], a[2], a[3], a[4], a[5]);
			}
			acx_mgmt_decode_beacon(&alloc_p80211mgmt_req.a.beacon);
			acx_process_probe_response(rxdesc->data,
					     priv,
					     (acxp80211_hdr_t *)
					     alloc_p80211mgmt_req.
					     a.beacon.hdr);
			break;
		default:
			/* acxlog(L_ASSOC | L_DEBUG,
			   "Incoming beacon message not handled during status %i.\n",
			   priv->status); */
			break;
		}
		break;
	case WLAN_FSTYPE_ASSOCREQ /* 0x00 */ :
		if (ACX_MODE_2_MANAGED_STA == priv->macmode_joined)
			break;

		memset(&alloc_p80211mgmt_req, 0, 8 * 4);
		alloc_p80211mgmt_req.a.assocreq.buf =
		    (u8 *) p80211_hdr;
		alloc_p80211mgmt_req.a.assocreq.len =
		    MAC_CNT_RCVD(rxdesc->data) - wep_offset;

		acx_mgmt_decode_assocreq(&alloc_p80211mgmt_req.a.
					 assocreq);

		if (!memcmp
		    (alloc_p80211mgmt_req.a.assocreq.hdr->a3.a2,
		     priv->bssid, ETH_ALEN)) {
			acx_transmit_assocresp(&alloc_p80211mgmt_req.a.
					   assocreq, priv);
		}
		break;
	case WLAN_FSTYPE_ASSOCRESP /* 0x01 */ :
		if (ACX_MODE_3_MANAGED_AP == priv->macmode_joined)
			break;

		memset(&alloc_p80211mgmt_req, 0, 8 * 4);
		alloc_p80211mgmt_req.a.assocresp.buf =
		    (u8 *) p80211_hdr;
		alloc_p80211mgmt_req.a.assocresp.len =
		    MAC_CNT_RCVD(rxdesc->data) - wep_offset;
		acx_mgmt_decode_assocresp(&alloc_p80211mgmt_req.a.
					  assocresp);
		acx_process_assocresp(&alloc_p80211mgmt_req.a.
				  assocresp, priv);
		break;
	case WLAN_FSTYPE_REASSOCREQ /* 0x02 */ :
		if (ACX_MODE_2_MANAGED_STA != priv->macmode_joined) {
			memset(&alloc_p80211mgmt_req.a.assocreq, 0, 9 * 4);
			alloc_p80211mgmt_req.a.assocreq.buf =
			    (u8 *) p80211_hdr;
			alloc_p80211mgmt_req.a.assocreq.len =
			    MAC_CNT_RCVD(rxdesc->data) - wep_offset;

			acx_mgmt_decode_assocreq(&alloc_p80211mgmt_req.a.
						 assocreq);

			/* reassocreq and assocreq are equivalent */
			acx_transmit_reassocresp(&alloc_p80211mgmt_req.a.
					     reassocreq, priv);
		}
		break;
	case WLAN_FSTYPE_REASSOCRESP /* 0x03 */ :
		if (ACX_MODE_3_MANAGED_AP == priv->macmode_joined)
			break;

		memset(&alloc_p80211mgmt_req.a.assocresp, 0,
		       8 * 4);
		alloc_p80211mgmt_req.a.assocresp.buf =
		    (u8 *) p80211_hdr;
		alloc_p80211mgmt_req.a.assocresp.len =
		    MAC_CNT_RCVD(rxdesc->data) - wep_offset;
		acx_mgmt_decode_assocresp(&alloc_p80211mgmt_req.a.
					  assocresp);
		acx_process_reassocresp(&alloc_p80211mgmt_req.a.
				    reassocresp, priv);
		break;
	case WLAN_FSTYPE_PROBEREQ /* 0x04 */ :
		if (ACX_MODE_3_MANAGED_AP == priv->macmode_joined) {
			acxlog(L_ASSOC, "FIXME: since we're supposed to be an AP, we need to return a Probe Response packet!\n");
		}
		break;
	case WLAN_FSTYPE_PROBERESP /* 0x05 */ :
		if (ACX_MODE_3_MANAGED_AP == priv->macmode_joined)
			break;

		memset(&alloc_p80211mgmt_req, 0, 0xd * 4);
		alloc_p80211mgmt_req.a.proberesp.buf =
		    (u8 *) p80211_hdr;
		alloc_p80211mgmt_req.a.proberesp.len =
		    MAC_CNT_RCVD(rxdesc->data) - wep_offset;
		acx_mgmt_decode_proberesp(&alloc_p80211mgmt_req.a.
					  proberesp);
		if (priv->status == ISTATUS_1_SCANNING)
			acx_process_probe_response(rxdesc->data,
					     priv,
					     (acxp80211_hdr_t *)
					     alloc_p80211mgmt_req.
					     a.proberesp.hdr);
		break;
	case 6:
	case 7:
		/* exit */
		break;
	case WLAN_FSTYPE_ATIM /* 0x09 */ :
		/* exit */
		break;
	case WLAN_FSTYPE_DISASSOC /* 0x0a */ :
		memset(&alloc_p80211mgmt_req.a.disassoc, 0, 5 * 4);
		alloc_p80211mgmt_req.a.disassoc.buf =
		    (u8 *) p80211_hdr;
		alloc_p80211mgmt_req.a.disassoc.len =
			    MAC_CNT_RCVD(rxdesc->data) - wep_offset;
		acx_mgmt_decode_disassoc(&alloc_p80211mgmt_req.a.disassoc);
		if (ACX_MODE_3_MANAGED_AP != priv->macmode_joined) {
			acx_process_disassoc(&alloc_p80211mgmt_req.a.disassoc,
					 priv);
		}
		else
		if ((ACX_MODE_0_IBSS_ADHOC == priv->macmode_joined)
		 || (ACX_MODE_2_MANAGED_STA == priv->macmode_joined)) {
			acx_process_disassociate(&alloc_p80211mgmt_req.a.
					     disassoc, priv);
		}
		break;
	case WLAN_FSTYPE_AUTHEN /* 0x0b */ :
		memset(&alloc_p80211mgmt_req.a.authen, 0, 8 * 4);
		alloc_p80211mgmt_req.a.authen.buf =
		    (u8 *) p80211_hdr;
		alloc_p80211mgmt_req.a.authen.len =
			    MAC_CNT_RCVD(rxdesc->data) - wep_offset;
		acx_mgmt_decode_authen(&alloc_p80211mgmt_req.a.authen);
		if (!memcmp(priv->bssid,
			    alloc_p80211mgmt_req.a.authen.hdr->a3.a2,
			    ETH_ALEN)) {
			acx_process_authen(&alloc_p80211mgmt_req.a.authen,
				       priv);
		}
		break;
	case WLAN_FSTYPE_DEAUTHEN /* 0x0c */ :
		memset(&alloc_p80211mgmt_req.a.deauthen, 0, 5 * 4);
		alloc_p80211mgmt_req.a.deauthen.buf =
		    (u8 *) p80211_hdr;
		alloc_p80211mgmt_req.a.deauthen.len =
		    MAC_CNT_RCVD(rxdesc->data) - wep_offset;
		acx_mgmt_decode_deauthen(&alloc_p80211mgmt_req.a.deauthen);
		/* FIXME: this check is buggy: it should be ==,
		 * but then our complete deauthen handling would have to be
		 * changed, so better leave it at that and make sure to adapt
		 * our driver to generic Linux 802.11 stack ASAP! */
		if (ACX_MODE_3_MANAGED_AP != priv->macmode_joined) {
			acx_process_deauthen(&alloc_p80211mgmt_req.a.deauthen,
					 priv);
		}
		else
		if ((ACX_MODE_0_IBSS_ADHOC == priv->macmode_joined)
		 || (ACX_MODE_2_MANAGED_STA == priv->macmode_joined)) {
			acx_process_deauthenticate(&alloc_p80211mgmt_req.a.
					       deauthen, priv);
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
	u16 fc;
	const signed char *esi = NULL;
	const u8 *ebx = NULL;
	const p80211_hdr_t *p80211_hdr;
	const client_t *client;
	const client_t *resclt = NULL;
	int result = NOT_OK;

	p80211_hdr = acx_get_p80211_hdr(priv, rxdesc);
		
	fc = ieee2host16(p80211_hdr->a3.fc);

	if ((!WLAN_GET_FC_TODS(fc)) && (!WLAN_GET_FC_FROMDS(fc))) {
		esi = p80211_hdr->a3.a1;
		ebx = p80211_hdr->a3.a2;
	} else if ((!WLAN_GET_FC_TODS(fc)) && (WLAN_GET_FC_FROMDS(fc))) {
		esi = p80211_hdr->a3.a1;
		ebx = p80211_hdr->a3.a3;
	} else if ((WLAN_GET_FC_TODS(fc)) && (!WLAN_GET_FC_FROMDS(fc))) {
		ebx = p80211_hdr->a3.a2;
		esi = p80211_hdr->a3.a1;
	} else
		ebx = p80211_hdr->a3.a2;

	if (esi[0x0] < 0) {
		result = OK;
		goto done;
	}
	for (client = acx_sta_list_get_from_hash(priv, ebx); client;
	     client = client->next) {
		if (!memcmp(ebx, client->address, ETH_ALEN)) {
			resclt = client;
			break;
		}
	}

	if (resclt)
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
static void acx_process_probe_response(const struct rxbuffer *mmt, wlandevice_t *priv,
			  const acxp80211_hdr_t *hdr)
{
	const u8 *pSuppRates;
	const u8 *pDSparms;
	unsigned int station;
	const u8 *a;
	struct bss_info *newbss;
	u8 rate_count;
	unsigned int i, max_rate = 0;

	acxlog(L_ASSOC, "%s: previous bss_table_count: %u.\n",
	       __func__, priv->bss_table_count);

	FN_ENTER;

	/* uh oh, we found more sites/stations than we can handle with
	 * our current setup: pull the emergency brake and stop scanning! */
	if (priv->bss_table_count > MAX_NUMBER_OF_SITE) {
		acx_issue_cmd(priv, ACX1xx_CMD_STOP_SCAN, NULL, 0, ACX_CMD_TIMEOUT_DEFAULT);
		acx_set_status(priv, ISTATUS_2_WAIT_AUTH);

		acxlog(L_BINDEBUG | L_ASSOC,
		       "<Scan Beacon> bss_table_count > MAX_NUMBER_OF_SITE\n");
		FN_EXIT0();
		return;
	}

	if (OK == acx_is_mac_address_equal(hdr->a4.a3, priv->dev_addr)) {
		acxlog(L_ASSOC, "huh, scan found our own MAC!?\n");
		FN_EXIT0();
		return; /* just skip this one silently */
	}
			
	/* filter out duplicate stations we already registered in our list */
	for (station = 0; station < priv->bss_table_count; station++) {
		u8 *a = priv->bss_table[station].bssid;
		acxlog(L_DEBUG,
		       "checking station %u [%02X %02X %02X %02X %02X %02X]\n",
		       station, a[0], a[1], a[2], a[3], a[4], a[5]);
		if (OK == acx_is_mac_address_equal
		    (hdr->a4.a3,
		     priv->bss_table[station].bssid)) {
			acxlog(L_DEBUG,
			       "station already in our list, no need to add.\n");
			FN_EXIT0();
			return;
		}
	}

	newbss = &priv->bss_table[priv->bss_table_count];

	/* pSuppRates points to the Supported Rates element: info[1] is essid_len */
	pSuppRates = &hdr->info[hdr->info[0x1] + 0x2];
	/* pDSparms points to the DS Parameter Set */
	pDSparms = &pSuppRates[pSuppRates[0x1] + 0x2];

	/* Let's completely zero out the entry that we're
	 * going to fill next in order to not risk any corruption. */
	memset(newbss, 0, sizeof(struct bss_info));

	/* copy the BSSID element */
	MAC_COPY(newbss->bssid, hdr->a4.a3);
	/* copy the MAC address element (source address) */
	MAC_COPY(newbss->mac_addr, hdr->a4.a2);

	/* copy the ESSID element */
	if (hdr->info[0x1] <= IW_ESSID_MAX_SIZE) {
		newbss->essid_len = hdr->info[0x1];
		memcpy(newbss->essid, &hdr->info[0x2], hdr->info[0x1]);
		newbss->essid[hdr->info[0x1]] = '\0';
	}
	else {
		acxlog(L_STD, "huh, ESSID overflow in scanned station data?\n");
	}

	newbss->channel = pDSparms[2];
	newbss->wep = (ieee2host16(hdr->caps) & IEEE802_11_MGMT_CAP_WEP);
	newbss->caps = ieee2host16(hdr->caps);
	rate_count = pSuppRates[1];
	if (rate_count > 63)
		rate_count = 63;
	memcpy(newbss->supp_rates, &pSuppRates[2], rate_count);
	newbss->supp_rates[rate_count] = 0;
	/* now, one just uses strlen() on it to know how many rates are there */
 
	newbss->sir = acx_signal_to_winlevel(mmt->phy_level);
	newbss->snr = acx_signal_to_winlevel(mmt->phy_snr);

	a = newbss->bssid;

	/* find max. transfer rate and do optional debug log */
	acxlog(L_DEBUG, "Peer - Supported Rates:\n");
	for (i=0; i < rate_count; i++){
		acxlog(L_DEBUG, "%s Rate: %d%sMbps (0x%02X)\n",
			(pSuppRates[2+i] & 0x80) ? "Basic" : "Operational",
			(int)((pSuppRates[2+i] & ~0x80) / 2),
			(pSuppRates[2+i] & 1) ? ".5" : "", pSuppRates[2+i]);
		if ((pSuppRates[2+i] & ~0x80) > max_rate)
			max_rate = pSuppRates[2+i] & ~0x80;
	}

	acxlog(L_STD | L_ASSOC,
	       "%s: found and registered station %u: ESSID \"%s\" on channel %d, "
	       "BSSID %02X:%02X:%02X:%02X:%02X:%02X, %s/%d%sMbps, "
	       "Caps 0x%04x, SIR %d, SNR %d.\n",
	       __func__,
	       priv->bss_table_count,
	       newbss->essid, newbss->channel,
	       a[0], a[1], a[2], a[3], a[4], a[5],
               (newbss->caps & IEEE802_11_MGMT_CAP_IBSS) ? "Ad-Hoc peer" : "Access Point",
	       (int)(max_rate / 2), (max_rate & 1) ? ".5" : "",
	       newbss->caps, newbss->sir, newbss->snr);

	/* found one station --> increment counter */
	priv->bss_table_count++;
	FN_EXIT0();
}

#define STATUS_NUM_ENTRIES	22
static const char * const status_str[STATUS_NUM_ENTRIES] =
{ "Successful", "Unspecified failure",
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
  "Association denied due to requesting station not supporting the Channel Agility option. TRANSLATION: Bug in ACX100 driver?"};
static inline const char * const get_status_string(int status)
{
	unsigned int idx = status < STATUS_NUM_ENTRIES ? status : 2;
	return status_str[idx];
}
#undef STATUS_NUM_ENTRIES

/*----------------------------------------------------------------
* acx_process_assocresp
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

/* acx_process_assocresp()
 * STATUS: should be ok, UNVERIFIED.
 */
static int acx_process_assocresp(const wlan_fr_assocresp_t *req, wlandevice_t *priv)
{
	const p80211_hdr_t *hdr;
	int res = NOT_OK;
/*	acx_ie_generic_t pdr; */

	FN_ENTER;
	hdr = req->hdr;

	if (WLAN_GET_FC_TODS(ieee2host16(hdr->a4.fc)) || WLAN_GET_FC_FROMDS(ieee2host16(hdr->a4.fc)))
		res = OK;
	else {
		if (OK == acx_is_mac_address_equal(priv->dev_addr, hdr->a4.a1 /* RA */)) {
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
	}
	FN_EXIT1(res);
	return res;
}

/*----------------------------------------------------------------
* acx_process_reassocresp
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

/* acx_process_reassocresp()
 * STATUS: should be ok, UNVERIFIED.
 */
static int acx_process_reassocresp(const wlan_fr_reassocresp_t *req, wlandevice_t *priv)
{
	const p80211_hdr_t *hdr = req->hdr;
	int result = 4;

	FN_ENTER;

	if (WLAN_GET_FC_TODS(ieee2host16(hdr->a3.fc)) || WLAN_GET_FC_FROMDS(ieee2host16(hdr->a3.fc))) {
		result = 1;
	} else {
		if (ACX_MODE_0_IBSS_ADHOC == priv->macmode_joined) {
			result = 2;
		} else if (OK == acx_is_mac_address_equal(priv->dev_addr, hdr->a3.a1 /* RA */)) {
			if (ieee2host16(*(req->status)) == WLAN_MGMT_STATUS_SUCCESS) {
				acx_set_status(priv, ISTATUS_4_ASSOCIATED);
			} else {
				acxlog(L_STD | L_ASSOC, "Reassociation FAILED: response status code %d: \"%s\"!\n", ieee2host16(*(req->status)), get_status_string(ieee2host16(*(req->status))));
			}
			result = 0;
		} else {
			result = 3;
		}
	}
	FN_EXIT1(result);
	return result;
}

/* acx_process_authen()
 * STATUS: FINISHED, UNVERIFIED.
 */
static int acx_process_authen(const wlan_fr_authen_t *req, wlandevice_t *priv)
{
	const p80211_hdr_t *hdr;
	client_t *clt, *currclt;
	int result = NOT_OK;

	FN_ENTER;
	hdr = req->hdr;
	if (WLAN_GET_FC_TODS(ieee2host16(hdr->a3.fc)) || WLAN_GET_FC_FROMDS(ieee2host16(hdr->a3.fc))) {
		result = NOT_OK;
		goto end;
	}

	if (!priv) {
		result = NOT_OK;
		goto end;
	}

	acx_log_mac_address(L_ASSOC, priv->dev_addr, " ");
	acx_log_mac_address(L_ASSOC, hdr->a3.a1, " ");
	acx_log_mac_address(L_ASSOC, hdr->a3.a2, " ");
	acx_log_mac_address(L_ASSOC, hdr->a3.a3, " ");
	acx_log_mac_address(L_ASSOC, priv->bssid, "\n");
	
	if ((OK != acx_is_mac_address_equal(priv->dev_addr, hdr->a3.a1)) &&
			(OK != acx_is_mac_address_equal(priv->bssid, hdr->a3.a1))) {
		result = OK;
		goto end;
	}
	if (ACX_MODE_0_IBSS_ADHOC == priv->macmode_joined) {
		result = NOT_OK;
		goto end;
	}

	if (priv->auth_alg <= 1) {
		if (priv->auth_alg != ieee2host16(*(req->auth_alg)))
		{
			acxlog(L_ASSOC, "authentication algorithm mismatch: want: %d, req: %d\n", priv->auth_alg, ieee2host16(*(req->auth_alg)));
			result = NOT_OK;
			goto end;
		}
	}
	acxlog(L_ASSOC,"Algorithm is ok\n");
	currclt = acx_sta_list_get_from_hash(priv, hdr->a3.a2);
	acxlog(L_ASSOC,"Got current client for sta hash tab\n");
	clt = NULL;
	while (currclt) {
		if (0 == memcmp(hdr->a3.a2, currclt->address, ETH_ALEN)) {
			clt = currclt;
			break;
		}
		currclt = currclt->next;
	}
	acxlog(L_ASSOC,"Found acceptable client\n");
	/* create a new entry if station isn't registered yet */
	if (!clt) {
		clt = acx_sta_list_add(priv, hdr->a3.a2);
		if (!clt)
		{
			acxlog(L_ASSOC,"Could not allocate room for this client\n");
			result = NOT_OK;
			goto end;
		}

		MAC_COPY(clt->address, hdr->a3.a2);
		clt->used = 1;
	}
	/* now check which step in the authentication sequence we are
	 * currently in, and act accordingly */
	acxlog(L_ASSOC, "acx_process_authen auth seq step %d.\n", ieee2host16(req->auth_seq[0]));
	switch (ieee2host16(req->auth_seq[0])) {
	case 1:
		if (ACX_MODE_2_MANAGED_STA == priv->macmode_joined)
			break;
		acx_transmit_authen2(req, clt, priv);
		break;
	case 2:
		if (ACX_MODE_3_MANAGED_AP == priv->macmode_joined)
			break;
		if (ieee2host16(*(req->status)) == WLAN_MGMT_STATUS_SUCCESS) {
			if (ieee2host16(*(req->auth_alg)) == WLAN_AUTH_ALG_OPENSYSTEM) {
				acx_set_status(priv, ISTATUS_3_AUTHENTICATED);
				acx_transmit_assoc_req(priv);
			} else
			if (ieee2host16(*(req->auth_alg)) == WLAN_AUTH_ALG_SHAREDKEY) {
				acx_transmit_authen3(req, priv);
			}
		} else {
			acxlog(L_ASSOC, "Authentication FAILED (status code %d: \"%s\"), still waiting for authentication.\n", ieee2host16(*(req->status)), get_status_string(ieee2host16(*(req->status))));
			acx_set_status(priv, ISTATUS_2_WAIT_AUTH);
		}
		break;
	case 3:
		if ((ACX_MODE_2_MANAGED_STA == priv->macmode_joined)
		    || (clt->auth_alg != WLAN_AUTH_ALG_SHAREDKEY)
		    || (ieee2host16(*(req->auth_alg)) != WLAN_AUTH_ALG_SHAREDKEY)
		    || (clt->val0xe != 2))
			break;
		acxlog(L_STD,
		       "FIXME: TODO: huh??? incompatible data type!\n");
		currclt = (client_t *)req->challenge;
		if (0 == memcmp(currclt->address, clt->val0x18, 0x80)
		    && ( (currclt->aid & 0xff) != 0x10 )
		    && ( (currclt->aid >> 8) != 0x80 ))
				break;
		acx_transmit_authen4(req, priv);
		MAC_COPY(clt->address, hdr->a3.a2);
		clt->used = 2;
		clt->val0xe = 4;
		clt->val0x98 = ieee2host16(hdr->a3.seq);
		break;
	case 4:
		if (ACX_MODE_3_MANAGED_AP == priv->macmode_joined)
			break;

		/* ok, we're through: we're authenticated. Woohoo!! */
		acx_set_status(priv, ISTATUS_3_AUTHENTICATED);
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
* Side effects:
*
* Call context:
*
* STATUS:
*
* Comment:
*
*----------------------------------------------------------------*/

/* acx_process_deauthen()
 * STATUS: FINISHED, UNVERIFIED.
 */
static int acx_process_deauthen(const wlan_fr_deauthen_t *arg_0, wlandevice_t *priv)
{
	const p80211_hdr_t *hdr;
	int result;
	const u8 *addr;
	client_t *client, *resclt = NULL;

	FN_ENTER;

	hdr = arg_0->hdr;

	if (WLAN_GET_FC_TODS(ieee2host16(hdr->a3.fc)) 
	 || WLAN_GET_FC_FROMDS(ieee2host16(hdr->a3.fc)) ) {
		result = NOT_OK;
		goto end;
	}

	acxlog(L_ASSOC, "DEAUTHEN ");
	acx_log_mac_address(L_ASSOC, priv->dev_addr, " ");
	acx_log_mac_address(L_ASSOC, hdr->a3.a1, " ");
	acx_log_mac_address(L_ASSOC, hdr->a3.a2, " ");
	acx_log_mac_address(L_ASSOC, hdr->a3.a3, " ");
	acx_log_mac_address(L_ASSOC, priv->bssid, "\n");
	
	if ((OK != acx_is_mac_address_equal(priv->dev_addr, hdr->a3.a1)) &&
			(OK != acx_is_mac_address_equal(priv->bssid, hdr->a3.a1))) {
		result = NOT_OK;
		goto end;
	}
	if (ACX_MODE_0_IBSS_ADHOC == priv->macmode_joined) {
		result = OK;
		goto end;
	}
	
	acxlog(L_STD, "Processing deauthen packet. Hmm, should this have happened?\n");

	addr = hdr->a3.a2;
	if (memcmp(addr, priv->dev_addr, ETH_ALEN)) {
		/* OK, we've been asked to leave the ESS. Do we 
		 * ask to return or do we leave quietly? I'm 
		 * guessing that since we are still up and 
		 * running we should attempt to rejoin, from the 
		 * starting point. So:
		 */
		acx_set_status(priv,ISTATUS_2_WAIT_AUTH);
		result = OK;
		goto end;
	}			

	client = acx_sta_list_get_from_hash(priv, addr);

	if (client == NULL) {
		result = OK;
		goto end;
	}

	do {
		if (0 == memcmp(addr, &client->address, ETH_ALEN)) {
			resclt = client;
			goto end;
		}
		client = client->next;
	} while (client);
	resclt = NULL;

end:
	if (resclt) {
		resclt->used = 1;
		result = OK;
	} else
		result = NOT_OK;

	FN_EXIT1(result);
	return result;
}

/*----------------------------------------------------------------
* acx_process_deauthenticate
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

/* acx_process_deauthenticate()
 * STATUS: FINISHED, UNVERIFIED.
 */
static int acx_process_deauthenticate(const wlan_fr_deauthen_t *req, wlandevice_t *priv)
{
	const p80211_hdr_t *hdr;

	FN_ENTER;
	acxlog(L_STD, "processing deauthenticate packet. Hmm, should this have happened?\n");
	hdr = req->hdr;
	if (WLAN_GET_FC_TODS(ieee2host16(hdr->a3.fc))
	 || WLAN_GET_FC_FROMDS(ieee2host16(hdr->a3.fc)) )
		return NOT_OK;
	else {
		if (ACX_MODE_0_IBSS_ADHOC == priv->macmode_joined)
			return NOT_OK;
		if (OK == acx_is_mac_address_equal(priv->dev_addr, hdr->a3.a1)) {
			if (priv->status > ISTATUS_2_WAIT_AUTH) {
				acx_set_status(priv, ISTATUS_2_WAIT_AUTH);
				return OK;
			}
		}
	}
	FN_EXIT1(NOT_OK);
	return NOT_OK;
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
	d->element_ID = 0x10;
	d->length = 0x80;
	acx_get_random(d->text, 0x80);
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

/* acx_transmit_deauthen()
 * STATUS: should be ok, but UNVERIFIED.
 */
static int acx_transmit_deauthen(const u8 *addr, client_t *clt, wlandevice_t *priv, u16 reason)
{
	TxData *hd;
	struct deauthen_frame_body *payload;
	struct txdescriptor *tx_desc;
	struct txhostdescriptor *hdesc_header;
	struct txhostdescriptor *hdesc_payload;

	FN_ENTER;

	tx_desc = acx_get_tx_desc(priv);
	if (!tx_desc) {
		return NOT_OK;
	}

	hdesc_header = tx_desc->fixed_size.s.host_desc;
	hdesc_payload = tx_desc->fixed_size.s.host_desc + 1;

	hd = (TxData *)hdesc_header->data;
	payload = (struct deauthen_frame_body *)hdesc_payload->data;

	hd->frame_control = host2ieee16(WLAN_SET_FC_FTYPE(WLAN_FTYPE_MGMT) | WLAN_SET_FC_FSTYPE(WLAN_FSTYPE_DEAUTHEN));	/* 0xc0 */
	hd->duration_id = 0;

	if (clt) {
		clt->used = 1;
		MAC_COPY(hd->da, clt->address);
	} else {
		MAC_COPY(hd->da, addr);
	}
	MAC_COPY(hd->sa, priv->dev_addr);
	/* FIXME: this used to use dev_addr, but I think it should use
	 * the BSSID of the network we're associated to: priv->bssid */
	MAC_COPY(hd->bssid, priv->bssid);
	
	hd->sequence_control = 0;

	acxlog(L_BINDEBUG | L_ASSOC | L_XFER,
	       "<acx_transmit_deauthen>sta=%02X:%02X:%02X:%02X:%02X:%02X for %d\n",
	       hd->da[0x0], hd->da[0x1], hd->da[0x2],
	       hd->da[0x3], hd->da[0x4], hd->da[0x5], reason);

	hdesc_header->length = cpu_to_le16(WLAN_HDR_A3_LEN);
	hdesc_header->data_offset = 0;

	payload->reason = host2ieee16(reason);

	hdesc_payload->length = cpu_to_le16(sizeof(deauthen_frame_body_t));
	hdesc_payload->data_offset = 0;

	tx_desc->total_length = cpu_to_le16(WLAN_HDR_A3_LEN + sizeof(deauthen_frame_body_t));

	acx_dma_tx_data(priv, tx_desc);

	FN_EXIT1(OK);
	return OK;
}

/*----------------------------------------------------------------
* acx_transmit_authen1
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

/* acx_transmit_authen1()
 * STATUS: UNVERIFIED
 */
static int acx_transmit_authen1(wlandevice_t *priv)
{
	struct auth_frame_body *payload;
	struct txdescriptor *tx_desc;
	struct txhostdescriptor *hdesc_header;
	struct txhostdescriptor *hdesc_payload;
	TxData *hd;

	FN_ENTER;

	acxlog(L_BINSTD | L_ASSOC, "Sending authentication1 request, awaiting response!\n");

	tx_desc = acx_get_tx_desc(priv);
	if (!tx_desc) {
		FN_EXIT0();
		return NOT_OK;
	}

	hdesc_header = tx_desc->fixed_size.s.host_desc;
	hdesc_payload = tx_desc->fixed_size.s.host_desc + 1;

	hd = (TxData *)hdesc_header->data;
	payload = (struct auth_frame_body *)hdesc_payload->data;

	hd->frame_control = host2ieee16(WLAN_SET_FC_FSTYPE(WLAN_FSTYPE_AUTHEN)); /* 0xb0 */
	hd->duration_id = host2ieee16(0x8000);

	MAC_COPY(hd->da, priv->bssid);
	MAC_COPY(hd->sa, priv->dev_addr);
	MAC_COPY(hd->bssid, priv->bssid);

	hd->sequence_control = 0;

	hdesc_header->length = cpu_to_le16(WLAN_HDR_A3_LEN);
	hdesc_header->data_offset = 0;

	payload->auth_alg = host2ieee16(priv->auth_alg);
	payload->auth_seq = host2ieee16(1);
	payload->status = host2ieee16(0);

	hdesc_payload->length = cpu_to_le16(2 + 2 + 2); /* 6 */
	hdesc_payload->data_offset = 0;

	tx_desc->total_length = cpu_to_le16(WLAN_HDR_A3_LEN + 2 + 2 + 2);

	acx_dma_tx_data(priv, tx_desc);
	FN_EXIT0();
	return OK;
}

/*----------------------------------------------------------------
* acx_transmit_authen2
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

/* acx_transmit_authen2()
 * STATUS: UNVERIFIED. (not binary compatible yet)
 */
static int acx_transmit_authen2(const wlan_fr_authen_t *arg_0, client_t *sta_list,
		      wlandevice_t *priv)
{
	unsigned int packet_len;
	struct auth_frame_body *payload;
	struct txdescriptor *tx_desc;
	struct txhostdescriptor *hdesc_header;
	struct txhostdescriptor *hdesc_payload;
	TxData *hd;

	FN_ENTER;
	packet_len = WLAN_HDR_A3_LEN;

	if (!sta_list) {
		FN_EXIT1(OK);
		return OK;
	}

	MAC_COPY(sta_list->address, arg_0->hdr->a3.a2);
	sta_list->val0x8 = WLAN_GET_FC_PWRMGT(ieee2host16(arg_0->hdr->a3.fc));
	sta_list->auth_alg = ieee2host16(*(arg_0->auth_alg));
	sta_list->val0xe = 2;
	sta_list->val0x98 = ieee2host16(arg_0->hdr->a3.seq);

	tx_desc = acx_get_tx_desc(priv);
	if (!tx_desc) {
		FN_EXIT1(NOT_OK);
		return NOT_OK;
	}

	hdesc_header = tx_desc->fixed_size.s.host_desc;
	hdesc_payload = tx_desc->fixed_size.s.host_desc + 1;

	hd = (TxData*)hdesc_header->data;
	payload = (struct auth_frame_body *)hdesc_payload->data;

	hd->frame_control = host2ieee16(WLAN_SET_FC_FSTYPE(WLAN_FSTYPE_AUTHEN)); /* 0xb0 */
	hd->duration_id = arg_0->hdr->a3.dur;
	hd->sequence_control = arg_0->hdr->a3.seq;

	MAC_COPY(hd->da, arg_0->hdr->a3.a2);
	MAC_COPY(hd->sa, arg_0->hdr->a3.a1);
	MAC_COPY(hd->bssid, arg_0->hdr->a3.a3);

	/* already in IEEE format, no endianness conversion */
	payload->auth_alg = *(arg_0->auth_alg);

	payload->auth_seq = host2ieee16(2);

	payload->status = host2ieee16(0);

	if (ieee2host16(*(arg_0->auth_alg)) == WLAN_AUTH_ALG_OPENSYSTEM) {
		sta_list->used = 2;
		packet_len += 2 + 2 + 2;
	} else {	/* shared key */
		acx_gen_challenge(&payload->challenge);
		memcpy(&sta_list->val0x18, payload->challenge.text, 0x80);
		packet_len += 2 + 2 + 2 + 1+1+0x80;
	}

	hdesc_header->length = cpu_to_le16(WLAN_HDR_A3_LEN);
	hdesc_header->data_offset = 0;
	hdesc_payload->length = cpu_to_le16(packet_len - WLAN_HDR_A3_LEN);
	hdesc_payload->data_offset = 0;

	acxlog(L_BINDEBUG | L_ASSOC | L_XFER,
	       "<transmit_auth2> BSSID=%02X:%02X:%02X:%02X:%02X:%02X\n",
	       hd->bssid[0], hd->bssid[1], hd->bssid[2],
	       hd->bssid[3], hd->bssid[4], hd->bssid[5]);

	tx_desc->total_length = cpu_to_le16(packet_len);

	acx_dma_tx_data(priv, tx_desc);

	FN_EXIT1(OK);
	return OK;
}

/*----------------------------------------------------------------
* acx_transmit_authen3
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

/* acx_transmit_authen3()
 * STATUS: UNVERIFIED.
 */
static int acx_transmit_authen3(const wlan_fr_authen_t *arg_0, wlandevice_t *priv)
{
	unsigned int packet_len;
	struct txdescriptor *tx_desc;
	struct txhostdescriptor *hdesc_header;
	struct txhostdescriptor *hdesc_payload;
	TxData *hd;
	struct auth_frame_body *payload;

	FN_ENTER;
	tx_desc = acx_get_tx_desc(priv);
	if (!tx_desc) {
		FN_EXIT1(NOT_OK);
		return OK;
	}

	packet_len = WLAN_HDR_A3_LEN;

	hdesc_header = tx_desc->fixed_size.s.host_desc;
	hdesc_payload = tx_desc->fixed_size.s.host_desc + 1;

	hd = (TxData *)hdesc_header->data;
	payload = (struct auth_frame_body *)hdesc_payload->data;

	hd->frame_control = host2ieee16(WLAN_SET_FC_ISWEP(1) + WLAN_SET_FC_FSTYPE(WLAN_FSTYPE_AUTHEN));	/* 0x40b0 */

	/* FIXME: is this needed?? authen4 does it...
	hd->duration_id = arg_0->hdr->a3.dur;
	hd->sequence_control = arg_0->hdr->a3.seq;
	*/
	MAC_COPY(hd->da, priv->bssid);
	MAC_COPY(hd->sa, priv->dev_addr);
	MAC_COPY(hd->bssid, priv->bssid);

	hdesc_header->length = cpu_to_le16(WLAN_HDR_A3_LEN);
	hdesc_header->data_offset = 0;

	/* already in IEEE format, no endianness conversion */
	payload->auth_alg = *(arg_0->auth_alg);

	payload->auth_seq = host2ieee16(3);

	payload->status = host2ieee16(0);

	memcpy(&payload->challenge, arg_0->challenge, arg_0->challenge->len + 2);

	packet_len += 8 + arg_0->challenge->len;

	hdesc_payload->length = cpu_to_le16(packet_len - WLAN_HDR_A3_LEN);
	hdesc_payload->data_offset = 0;

	acxlog(L_BINDEBUG | L_ASSOC | L_XFER, "transmit_authen3!\n");

	tx_desc->total_length = cpu_to_le16(packet_len);
	
	acx_dma_tx_data(priv, tx_desc);
	FN_EXIT1(OK);
	return OK;
}

/*----------------------------------------------------------------
* acx_transmit_authen4
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

/* acx_transmit_authen4()
 * STATUS: UNVERIFIED.
 */
static int acx_transmit_authen4(const wlan_fr_authen_t *arg_0, wlandevice_t *priv)
{
	struct txdescriptor *tx_desc;
	struct txhostdescriptor *hdesc_header;
	struct txhostdescriptor *hdesc_payload;
	TxData *hd;
	struct auth_frame_body *payload;

	FN_ENTER;

	tx_desc = acx_get_tx_desc(priv);
	if (!tx_desc) {
		FN_EXIT1(OK);
		return OK;
	}

	hdesc_header = tx_desc->fixed_size.s.host_desc;
	hdesc_payload = tx_desc->fixed_size.s.host_desc + 1;

	hd = (TxData *)hdesc_header->data;
	payload = (struct auth_frame_body *)hdesc_payload->data;

	hd->frame_control = host2ieee16(WLAN_SET_FC_FSTYPE(WLAN_FSTYPE_AUTHEN)); /* 0xb0 */
	hd->duration_id = arg_0->hdr->a3.dur;
	hd->sequence_control = arg_0->hdr->a3.seq;

	MAC_COPY(hd->da, arg_0->hdr->a3.a2);
	/* FIXME: huh? why was there no "sa"? Added, assume should do like authen2 */
	MAC_COPY(hd->sa, arg_0->hdr->a3.a1);
	MAC_COPY(hd->bssid, arg_0->hdr->a3.a3);

	hdesc_header->length = cpu_to_le16(WLAN_HDR_A3_LEN);
	hdesc_header->data_offset = 0;

	/* already in IEEE format, no endianness conversion */
	payload->auth_alg = *(arg_0->auth_alg);
	payload->auth_seq = host2ieee16(4);
	payload->status = host2ieee16(0);

	hdesc_payload->length = cpu_to_le16(6);
	hdesc_payload->data_offset = 0;

	tx_desc->total_length = cpu_to_le16(WLAN_HDR_A3_LEN + 6);

	acx_dma_tx_data(priv, tx_desc);
	FN_EXIT1(OK);
	return OK;
}

/*----------------------------------------------------------------
* acx_transmit_assoc_req
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

/* acx_transmit_assoc_req()
 * STATUS: almost ok, but UNVERIFIED.
 */
static int acx_transmit_assoc_req(wlandevice_t *priv)
{
	unsigned int packet_len;
	struct txdescriptor *tx_desc;
	struct txhostdescriptor *header;
	struct txhostdescriptor *payload;
	TxData *hd;
	u8 *pCurrPos;

	FN_ENTER;

	acxlog(L_BINSTD | L_ASSOC, "Sending association request, awaiting response! NOT ASSOCIATED YET.\n");
	tx_desc = acx_get_tx_desc(priv);
	if (!tx_desc) {
		FN_EXIT1(NOT_OK);
		return NOT_OK;
	}

	packet_len = WLAN_HDR_A3_LEN;

	header = tx_desc->fixed_size.s.host_desc;      /* hostdescriptor for header */
	payload = tx_desc->fixed_size.s.host_desc + 1; /* hostdescriptor for payload */

	hd = (TxData *)header->data;
	pCurrPos = (u8 *)payload->data;

	hd->frame_control = host2ieee16(WLAN_SET_FC_FSTYPE(WLAN_FSTYPE_ASSOCREQ));  /* 0x00 */;
	hd->duration_id = host2ieee16(0x8000);

	MAC_COPY(hd->da, priv->bssid);
	MAC_COPY(hd->sa, priv->dev_addr);
	MAC_COPY(hd->bssid, priv->bssid);

	hd->sequence_control = 0;

	header->length = cpu_to_le16(WLAN_HDR_A3_LEN);
	header->data_offset = 0;

	/* now start filling the AssocReq frame body */
#if BROKEN
	*(u16 *)pCurrPos = host2ieee16(priv->capabilities & ~(WLAN_SET_MGMT_CAP_INFO_IBSS(1)));
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
	*(u16 *)pCurrPos = host2ieee16((priv->capabilities & ~(WLAN_SET_MGMT_CAP_INFO_IBSS(1))) | WLAN_SET_MGMT_CAP_INFO_ESS(1));
	*/
	*(u16 *)pCurrPos = host2ieee16(WLAN_SET_MGMT_CAP_INFO_ESS(1));
	if (priv->wep_restricted)
		SET_BIT(*(u16 *)pCurrPos, host2ieee16(WLAN_SET_MGMT_CAP_INFO_PRIVACY(1)));
	/* only ask for short preamble if the peer station supports it */
	if (priv->station_assoc.caps & IEEE802_11_MGMT_CAP_SHORT_PRE)
		SET_BIT(*(u16 *)pCurrPos, host2ieee16(WLAN_SET_MGMT_CAP_INFO_SHORT(1)));
	/* only ask for PBCC support if the peer station supports it */
	if (priv->station_assoc.caps & IEEE802_11_MGMT_CAP_PBCC)
		SET_BIT(*(u16 *)pCurrPos, host2ieee16(WLAN_SET_MGMT_CAP_INFO_PBCC(1)));
#endif
	acxlog(L_ASSOC, "association: requesting capabilities 0x%04X\n", *(u16 *)pCurrPos);
	pCurrPos += 2;

	/* add listen interval */
	*(u16 *)pCurrPos = host2ieee16(priv->listen_interval);
	pCurrPos += 2;

	/* add ESSID */
	*(u8 *)pCurrPos = 0; /* Element ID */
	pCurrPos += 1;
	*(u8 *)pCurrPos = strlen(priv->essid_for_assoc); /* Length */
	memcpy(&pCurrPos[1], priv->essid_for_assoc, pCurrPos[0]);
	pCurrPos += 1 + pCurrPos[0];

	/* add rates */
	*(u8 *)pCurrPos = 1; /* Element ID */
	pCurrPos += 1;
	*(u8 *)pCurrPos = priv->rate_supported_len; /* Length */
	pCurrPos += 1;
	memcpy(pCurrPos, priv->rate_supported, priv->rate_supported_len);
	pCurrPos += priv->rate_supported_len;

	/* calculate lengths */
	packet_len += (int)pCurrPos - (int)payload->data;

	payload->length = cpu_to_le16(packet_len - WLAN_HDR_A3_LEN);
	payload->data_offset = 0;

	tx_desc->total_length = cpu_to_le16(packet_len);

	acx_dma_tx_data(priv, tx_desc);
	FN_EXIT1(OK);
	return OK;
}

/*----------------------------------------------------------------
* acx_transmit_disassoc
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

/* acx_transmit_disassoc()
 * STATUS: almost ok, but UNVERIFIED.
 */
/* FIXME: type of clt is a guess */
/* I'm not sure if clt is needed. */
u32 acx_transmit_disassoc(client_t *clt, wlandevice_t *priv)
{
	struct disassoc_frame_body *payload;
	struct txdescriptor *tx_desc;
	struct txhostdescriptor *hdesc_header;
	struct txhostdescriptor *hdesc_payload;
	TxData *hd;

	FN_ENTER;
/*	if (clt != NULL) { */
		tx_desc = acx_get_tx_desc(priv);
		if (!tx_desc) {
			FN_EXIT1(NOT_OK);
			return NOT_OK;
		}

		hdesc_header = tx_desc->fixed_size.s.host_desc;
		hdesc_payload = tx_desc->fixed_size.s.host_desc + 1;

		hd = (TxData *)hdesc_header->data;
		payload = (struct disassoc_frame_body *)hdesc_payload->data;

/*		clt->used = 2; */

		hd->frame_control = host2ieee16(WLAN_SET_FC_FSTYPE(WLAN_FSTYPE_DISASSOC));	/* 0xa0 */
		hd->duration_id = 0;
		MAC_COPY(hd->da, priv->bssid);
		MAC_COPY(hd->sa, priv->dev_addr);
		MAC_COPY(hd->bssid, priv->dev_addr);
		hd->sequence_control = 0;

		hdesc_header->length = cpu_to_le16(WLAN_HDR_A3_LEN);
		hdesc_header->data_offset = 0;

		payload->reason = host2ieee16(7);	/* "Class 3 frame received from nonassociated station." */

		hdesc_payload->length = cpu_to_le16(priv->rate_supported_len + 8);
		hdesc_payload->data_offset = 0;

		tx_desc->total_length = cpu_to_le16(WLAN_HDR_A3_LEN + priv->rate_supported_len + 8);

		/* FIXME: lengths missing! */
		acx_dma_tx_data(priv, tx_desc);
		FN_EXIT1(OK);
		return OK;
/*	} */
	FN_EXIT1(0);
	return 0;
}

/*----------------------------------------------------------------
* acx_complete_dot11_scan
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

/* acx_complete_dot11_scan()
 * STATUS: FINISHED.
 */
void acx_complete_dot11_scan(wlandevice_t *priv)
{
	unsigned int idx;
	u16 needed_cap;
	s32 idx_found = -1;
	u32 found_station = 0;

	FN_ENTER;

	switch(priv->macmode_wanted) {
		case ACX_MODE_0_IBSS_ADHOC:
			needed_cap = WLAN_SET_MGMT_CAP_INFO_IBSS(1); /* 2, we require Ad-Hoc */
			break;
		case ACX_MODE_2_MANAGED_STA:
			needed_cap = WLAN_SET_MGMT_CAP_INFO_ESS(1); /* 1, we require Managed */
			break;
		case ACX_MODE_FF_AUTO:
			needed_cap = 0; /* 3, Ad-Hoc or Managed */
			break;
		default:
			acxlog(L_STD, "unsupported MAC mode %d!\n", priv->macmode_wanted);
			needed_cap = WLAN_SET_MGMT_CAP_INFO_IBSS(1); /* resort to Ad-Hoc */
			break;
	}
	acxlog(L_BINDEBUG | L_ASSOC, "Radio scan found %d stations in this area.\n", priv->bss_table_count);
	for (idx = 0; idx < priv->bss_table_count; idx++) {
		struct bss_info *this_bss = &priv->bss_table[idx];

		acxlog(L_BINDEBUG | L_ASSOC,
		       "<Scan Table> %d: SSID=\"%s\",CH=%d,SIR=%d,SNR=%d\n",
		       (int) idx,
		       this_bss->essid,
		       (int) this_bss->channel,
		       (int) this_bss->sir,
		       (int) this_bss->snr);

		if (OK != acx_is_mac_address_broadcast(priv->ap))
			if (OK != acx_is_mac_address_equal(this_bss->bssid, priv->ap))
				continue;

		/* broken peer with no mode flags set? */
		if (0 == (this_bss->caps & (IEEE802_11_MGMT_CAP_ESS | IEEE802_11_MGMT_CAP_IBSS))) {

			u16 new_mode = IEEE802_11_MGMT_CAP_ESS;
			const char *mode_str;

			switch(priv->macmode_wanted) {
				case ACX_MODE_0_IBSS_ADHOC:
					new_mode = IEEE802_11_MGMT_CAP_IBSS;
					mode_str = "Ad-Hoc";
					break;
				case ACX_MODE_2_MANAGED_STA:
					mode_str = "Managed";
					break;
				case ACX_MODE_FF_AUTO:
					mode_str = "Auto: chose Managed";
					break;
				default:
					mode_str = "Unknown: chose Managed";
					break;
			}
			acxlog(L_ASSOC, "STRANGE: peer station announces neither ESS (Managed) nor IBSS (Ad-Hoc) capability: patching to assume our currently wanted mode (%s)! Firmware upgrade of the peer would be a good idea...\n", mode_str);
			SET_BIT(this_bss->caps, new_mode);
		}
		acxlog(L_ASSOC, "peer_cap 0x%04x, needed_cap 0x%04x\n",
		       this_bss->caps, needed_cap);

		/* peer station doesn't support what we need? */
		if (needed_cap && ((this_bss->caps & needed_cap) != needed_cap))
			continue; /* keep looking */

		if (!(priv->reg_dom_chanmask & (1 << (this_bss->channel - 1) ) )) {
			acxlog(L_STD|L_ASSOC, "WARNING: peer station %d is using channel %d, which is outside the channel range of the regulatory domain the driver is currently configured for: couldn't join in case of matching settings, might want to adapt your config!\n", idx, this_bss->channel);
			continue; /* keep looking */
		}

		if ((0 == priv->essid_active)
		 || (0 == memcmp(this_bss->essid, priv->essid, priv->essid_len))
		) {
			acxlog(L_ASSOC,
			       "Found station with matching ESSID!! (\"%s\" station, \"%s\" config)\n",
			       this_bss->essid,
			       (priv->essid_active) ? priv->essid : "[any]");
			idx_found = idx;
			found_station = 1;

			/* stop searching if this station is
			 * on the current channel, otherwise
			 * keep looking for an even better match */
			if (this_bss->channel == priv->channel)
				break;
		}
		else
		if (('\0' == this_bss->essid[0])
		|| ((1 == strlen(this_bss->essid)) && (' ' == this_bss->essid[0]))) {
			/* hmm, station with empty or single-space SSID:
			 * using hidden SSID broadcast?
			 */
			idx_found = idx;
			found_station = 1;
			acxlog(L_ASSOC, "found station with empty or single-space (hidden?) SSID, considering for assoc attempt.\n");
			/* ...and keep looking for better matches */
		}
		else {
		    acxlog(L_ASSOC, "ESSID doesn't match! (\"%s\" station, \"%s\" config)\n",
			    this_bss->essid, (priv->essid_active) ? priv->essid : "[any]");
		}
	}
	if (found_station) {
		u8 *a;
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

		if (WLAN_GET_MGMT_CAP_INFO_ESS(priv->station_assoc.caps))
			priv->macmode_chosen = ACX_MODE_2_MANAGED_STA;
		else
			priv->macmode_chosen = ACX_MODE_0_IBSS_ADHOC;
		memcpy(priv->essid_for_assoc, essid_src, essid_len);
		priv->essid_for_assoc[essid_len] = '\0';
		priv->channel = priv->station_assoc.channel;
		MAC_COPY(priv->address, priv->station_assoc.bssid);

		a = priv->address;
		acxlog(L_STD | L_ASSOC,
		       "%s: matching station FOUND (idx %d), JOINING (%02X %02X %02X %02X %02X %02X).\n",
		       __func__, idx_found, a[0], a[1], a[2], a[3], a[4], a[5]);

		acx_join_bssid(priv);
		acx_update_peerinfo(priv, &priv->ap_peer, &priv->station_assoc); /* e.g. shortpre */

		if (ACX_MODE_0_IBSS_ADHOC != priv->macmode_chosen) {
			acx_transmit_authen1(priv);
			acx_set_status(priv, ISTATUS_2_WAIT_AUTH);
		} else {
			acx_set_status(priv, ISTATUS_4_ASSOCIATED);
		}
	} else {		/* uh oh, no station found in range */
		if ((ACX_MODE_0_IBSS_ADHOC == priv->macmode_wanted)
		 || (ACX_MODE_FF_AUTO == priv->macmode_wanted)) { /* phew, we're safe: we intended to use Ad-Hoc mode */
			acxlog(L_STD | L_ASSOC,
			       "%s: no matching station found in range, CANNOT JOIN: generating our own IBSSID instead.\n",
			       __func__);
			acx_ibssid_gen(priv, priv->address);
			acx_update_capabilities(priv);
			priv->macmode_chosen = ACX_MODE_0_IBSS_ADHOC;
			acx_join_bssid(priv);
			acx_set_status(priv, ISTATUS_4_ASSOCIATED);
		} else {
			acxlog(L_STD | L_ASSOC,
			       "%s: no matching station found in range and not in Ad-Hoc or Auto mode --> giving up scanning.\n",
			       __func__);
			acx_set_status(priv, ISTATUS_0_STARTED);
		}
	}
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
	case ISTATUS_1_SCANNING:
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
		}
		else
		{
			acxlog(L_ASSOC, "Stopping scan (%s).\n", priv->bss_table_count ? "stations found" : "scan timeout");
			/* stop the scan when we leave the interrupt context */
			acx_schedule_after_interrupt_task(priv, ACX_AFTER_IRQ_CMD_STOP_SCAN);
			/* HACK: set the IRQ bit, since we won't get a
			 * scan complete IRQ any more on ACX111 (works on ACX100!),
			 * since we will have stopped the scan */
			SET_BIT(priv->irq_status, HOST_INT_SCAN_COMPLETE);
		}
		break;
	case ISTATUS_2_WAIT_AUTH:
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
			acx_set_status(priv, ISTATUS_1_SCANNING);
		}
		acx_set_timer(priv, 2500000); /* used to be 1500000, but some other driver uses 2.5s wait time  */
		break;
	case ISTATUS_3_AUTHENTICATED:
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
			acx_set_status(priv, ISTATUS_1_SCANNING);
		}
		acx_set_timer(priv, 2500000); /* see above */
		break;
	case ISTATUS_0_STARTED:
	case ISTATUS_4_ASSOCIATED:
	default:
		break;
	}
	acx_unlock(priv, &flags);
	FN_EXIT0();
}
