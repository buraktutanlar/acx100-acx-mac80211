/* src/acx100_helper2.c - helper functions
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
#include <linux/skbuff.h>
#include <linux/slab.h>
#include <linux/if_arp.h>
#include <linux/rtnetlink.h>
#include <linux/wireless.h>
#if WIRELESS_EXT > 12
#include <net/iw_handler.h>
#endif /* WE > 12 */
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
#include <acx100_conv.h>
#include <acx100.h>
#include <p80211types.h>
#include <acx100_helper.h>
#include <idma.h>
#include <acx100_helper2.h>
#include <ihw.h>
#include <acx80211frm.h>

static UINT32 acx100_process_mgmt_frame(struct rxhostdescriptor *skb,
				 wlandevice_t *priv);
static int acx100_process_data_frame_master(struct rxhostdescriptor *skb,
				 wlandevice_t *priv);
static int acx100_process_data_frame_client(struct rxhostdescriptor *skb,
				     wlandevice_t *priv);
static int acx100_process_NULL_frame(struct rxhostdescriptor *a, 
			      wlandevice_t *priv, int vala);

alloc_p80211_mgmt_req_t alloc_p80211mgmt_req;

UINT16 CurrentAID = 1;

char *state_str[7] = { "STARTED", "SCANNING", "WAIT_AUTH", "AUTHENTICATED", "ASSOCIATED", "UNKNOWN", "INVALID??" };

/*----------------------------------------------------------------
* acx100_sta_list_init
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

/* acx100_sta_list_init()
 * STATUS: should be ok..
 */
void acx100_sta_list_init(wlandevice_t *priv)
{
	FN_ENTER;
	memset(priv->sta_hash_tab, 0, sizeof(priv->sta_hash_tab));
	memset(priv->sta_list, 0, sizeof(priv->sta_list));
	FN_EXIT(0, 0);
}

/*----------------------------------------------------------------
* acx100_sta_list_alloc
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

/* acx100_sta_list_alloc()
 * STATUS: FINISHED, except for struct defs.
 * Hmm, does this function have one "silent" parameter or 0 parameters?
 * Doesn't matter much anyway...
 */
inline client_t *acx100_sta_list_alloc(wlandevice_t *priv, UINT8 *address)
{
	int i = 0;

	FN_ENTER;
	for (i = 0; i <= 31; i++) {
		if (priv->sta_list[i].used == 0) {
			priv->sta_list[i].used = 1;
			priv->sta_list[i].auth_alg = WLAN_AUTH_ALG_SHAREDKEY;
			priv->sta_list[i].val0xe = 1;
			FN_EXIT(1, (int)&(priv->sta_list[i]));
			return &(priv->sta_list[i]);
		}
	}
	FN_EXIT(1, 0);
	return 0;
}

/*----------------------------------------------------------------
* acx100_sta_list_add
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

/* acx100_sta_list_add()
 * STATUS: FINISHED.
 */
client_t *acx100_sta_list_add(wlandevice_t *priv, UINT8 *address)
{
	client_t *client;
	int index;

	FN_ENTER;
	client = acx100_sta_list_alloc(priv, address);
	if (!client)
		goto done;

	/* computing hash table index */
	index = ((address[4] << 8) + address[5]);
	index -= index & 0x3ffc0;

	client->next = priv->sta_hash_tab[index];
	priv->sta_hash_tab[index] = client;

	acxlog(L_BINSTD | L_ASSOC,
	       "<acx100_sta_list_add> sta = %02X:%02X:%02X:%02X:%02X:%02X\n",
	       address[0], address[1], address[2], address[3], address[4],
	       address[5]);

      done:
	FN_EXIT(1, (int) client);
	return client;
}

/*----------------------------------------------------------------
* acx100_sta_list_get_from_hash
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


static inline client_t *acx100_sta_list_get_from_hash(wlandevice_t *priv, UINT8 *address)
{
	int index;

	FN_ENTER;
	/* computing hash table index */
	index = ((address[4] << 8) + address[5]);
	index -= index & 0x3ffc0;

	FN_EXIT(0, 0);
	return priv->sta_hash_tab[index];
}

/*----------------------------------------------------------------
* acx100_sta_list_get
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

/* acx100_get_sta_list()
 * STATUS: FINISHED.
 */
client_t *acx100_sta_list_get(wlandevice_t *priv, UINT8 *address)
{
	client_t *client;
	client_t *result = NULL;	/* can be removed if tracing unneeded */

	FN_ENTER;
	client = acx100_sta_list_get_from_hash(priv, address);

	for (; client; client = client->next) {
		if (0 == memcmp(address, client->address, ETH_ALEN)) {
			result = client;
			goto done;
		}
	}

      done:
	FN_EXIT(1, (int) result);
	return result;
}

inline char *acx100_get_status_name(UINT16 status)
{
	if (ISTATUS_5_UNKNOWN >= status)
		return state_str[status];
	else
		return state_str[6];
}

/*----------------------------------------------------------------
* acx100_set_status
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
*
* Comment:
* 0 probably means: just started (acx100_start()).
* 1 probably means: starting scan?
* 2 probably means: not authenticated yet (acx100_process_deauthenticate())
* 3 probably means: authenticated, but not associated yet (acx100_process_disassociate(), acx100_process_authen())
* 4 probably means: associated (acx100_process_assocresp(), acx100_process_reassocresp())
* 5 means: status unknown
*----------------------------------------------------------------*/

void acx100_set_status(wlandevice_t *priv, UINT16 status)
{
	char *stat;
#if QUEUE_OPEN_AFTER_ASSOC
	static int associated = 0;
#endif

	FN_ENTER;
	stat = acx100_get_status_name(status);

	acxlog(L_BINDEBUG | L_ASSOC, "%s: Setting status = %d (%s)\n",
	       __func__, status, stat);

#if WIRELESS_EXT > 12 /* wireless_send_event() */
	if (ISTATUS_4_ASSOCIATED == status) {
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

		if (ISTATUS_0_STARTED == status) {
			if (memcmp(priv->netdev->dev_addr, priv->dev_addr, ETH_ALEN)) {
				/* uh oh, the interface's MAC address changed,
				 * need to update templates (and init STAs??) */
				acxlog(L_STD, "Detected MAC address change, updating card configuration.\n");

				/* the MAC address has to be updated first,
				 * since otherwise we enter an eternal loop,
				 * as update_card_settings calls set_status */
				MAC_COPY(priv->dev_addr, priv->netdev->dev_addr);
				priv->set_mask |= SET_TEMPLATES|SET_STA_LIST;
				acx100_update_card_settings(priv, 0, 0, 0);
			}
		}
	}
#endif

	if (priv->unknown0x2350 == ISTATUS_5_UNKNOWN) {
		priv->unknown0x2350 = priv->status;
		priv->status = ISTATUS_5_UNKNOWN;
	} else {
		priv->status = status;
	}
	if ((priv->status == ISTATUS_1_SCANNING)
	    || (priv->status == ISTATUS_5_UNKNOWN)) {
		priv->scan_retries = 0;
		acx100_set_timer(priv, 1500000); /* 1.5 s initial scan time (used to be 15s, corrected to 1.5)*/
	} else if (priv->status <= ISTATUS_3_AUTHENTICATED) {
		priv->auth_assoc_retries = 0;
		acx100_set_timer(priv, 1500000); /* 1.5 s */
	}

#if QUEUE_OPEN_AFTER_ASSOC
	if (status == ISTATUS_4_ASSOCIATED)
	{
		if (associated == 0)
		{
			/* ah, we're newly associated now,
			 * so let's restart the net queue */
			acxlog(L_XFER, "wake queue after association.\n");
			netif_wake_queue(priv->netdev);
		}
		associated = 1;
	}
	else
	{
		/* not associated any more, so let's stop the net queue */
		if (associated == 1)
		{
			acxlog(L_XFER, "stop queue after losing association.\n");
			netif_stop_queue(priv->netdev);
		}
		associated = 0;
	}
#endif
	FN_EXIT(0, 0);
}

static inline p80211_hdr_t *acx_get_p80211_hdr(wlandevice_t *priv, rxhostdescriptor_t *rxdesc)
{
	if (priv->rx_config_1 & RX_CFG1_INCLUDE_ADDIT_HDR) {
		/* take into account additional header in front of packet */
		return (p80211_hdr_t *)((UINT8 *)&rxdesc->data->buf + 4);
	}
	else
	{
		return (p80211_hdr_t *)&rxdesc->data->buf;
	}
}

/*------------------------------------------------------------------------------
 * acx100_rx_ieee802_11_frame
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
int acx100_rx_ieee802_11_frame(wlandevice_t *priv, rxhostdescriptor_t *rxdesc)
{
	UINT16 ftype;
	UINT fstype;
	p80211_hdr_t *p80211_hdr;
	int result = 0;

	FN_ENTER;

	p80211_hdr = acx_get_p80211_hdr(priv, rxdesc);
//	printk("Rx_CONFIG_1 = %X\n",priv->rx_config_1 & RX_CFG1_INCLUDE_ADDIT_HDR);

	/* see IEEE 802.11-1999.pdf chapter 7 "MAC frame formats" */
	ftype = WLAN_GET_FC_FTYPE(p80211_hdr->a3.fc);
	fstype = WLAN_GET_FC_FSTYPE(p80211_hdr->a3.fc);

	switch (ftype) {
		case WLAN_FTYPE_MGMT:
			result = acx100_process_mgmt_frame(rxdesc, priv);
			break;
		case WLAN_FTYPE_CTL:
			if (fstype != WLAN_FSTYPE_PSPOLL)
				result = 0;
			else
				result = 1;
			/*   this call is irrelevant, since
			 *   acx100_process_class_frame is a stub, so return
			 *   immediately instead.
			 * return acx100_process_class_frame(rxdesc, priv, 3); */
			break;
		case WLAN_FTYPE_DATA:
			/* binary driver did ftype-1 to appease jump
			 * table layout */
			if (fstype == WLAN_FSTYPE_DATAONLY) 
			{
				if (ACX_MODE_3_MANAGED_AP == priv->macmode_joined) {
					result = acx100_process_data_frame_master(rxdesc, priv);
				} else if (ISTATUS_4_ASSOCIATED == priv->status) {
					result = acx100_process_data_frame_client(rxdesc, priv);
				}
			} else switch (ftype) {
				case WLAN_FSTYPE_DATA_CFACK:
				case WLAN_FSTYPE_DATA_CFPOLL:
				case WLAN_FSTYPE_DATA_CFACK_CFPOLL:
				case WLAN_FSTYPE_CFPOLL:
				case WLAN_FSTYPE_CFACK_CFPOLL:
				/*   see above.
				   acx100_process_class_frame(rxdesc, priv, 3); */
					break;
				case WLAN_FSTYPE_NULL:
					acx100_process_NULL_frame(rxdesc, priv, 3);
				/* FIXME: same here, see above */
				case WLAN_FSTYPE_CFACK:
				default:
					break;
			}
			break;
		default:
			break;
	}
	FN_EXIT(1, result);
	return result;
}

/*----------------------------------------------------------------
* acx100_transmit_assocresp
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

/* acx100_transmit_assocresp()
 * STATUS: should be ok, but UNVERIFIED.
 */
UINT32 acx100_transmit_assocresp(wlan_fr_assocreq_t *arg_0,
			  wlandevice_t *priv)
{
	UINT8 var_1c[6];
	UINT8 *da;
	UINT8 *sa;
	UINT8 *bssid;
	TxData *hd;
	struct assocresp_frame_body *payload;
	struct txdescriptor *tx_desc;
	struct txhostdescriptor *hdesc_header;
	struct txhostdescriptor *hdesc_payload;
	client_t *clt;

	FN_ENTER;
	acxlog(L_STATE, "%s: UNVERIFIED.\n", __func__);
	// FIXME: or is the order the other way round ??
	var_1c[0] = 0;
	var_1c[1] = 1;
	var_1c[2] = 3;
	var_1c[3] = 7;
	var_1c[4] = 0xf;
	var_1c[5] = 0x1f;

	acxlog(L_BINDEBUG | L_ASSOC | L_XFER, "<acx100_transmit_assocresp 1>\n");

	if (WLAN_GET_FC_TODS(arg_0->hdr->a3.fc) || WLAN_GET_FC_FROMDS(arg_0->hdr->a3.fc)) {
		FN_EXIT(1, 1);
		return 1;
	}
	
	sa = arg_0->hdr->a3.a1;
	da = arg_0->hdr->a3.a2;
	bssid = arg_0->hdr->a3.a3;

	clt = acx100_sta_list_get(priv, da);

	if (clt != NULL) {
		if (clt->used == 1) {
			acx100_transmit_deauthen(da, clt, priv, WLAN_MGMT_REASON_CLASS2_NONAUTH /* 6 */);
			FN_EXIT(0, 0);
			return 0;
		} else {
			clt->used = 3;

			if (clt->aid == 0) {
				clt->aid = CurrentAID;
				CurrentAID++;
			}
			clt->val0xa = arg_0->listen_int[0];

			memcpy(clt->val0x10, arg_0->ssid, arg_0->ssid->len);

			/* FIXME: huh, why choose the ESSID length
			 * directly as the index!?!? */
			if (arg_0->ssid->len <= 5) {
				clt->val0x9a = var_1c[arg_0->ssid->len];
			} else {
				clt->val0x9a = 0x1f;
			}

			if ((tx_desc = acx100_get_tx_desc(priv)) == NULL) {
				FN_EXIT(1, 0);
				return 0;
			}

			hdesc_header = tx_desc->host_desc;
			hdesc_payload = tx_desc->host_desc + 1;

			hd = (TxData *)hdesc_header->data;
			payload = (struct assocresp_frame_body *)hdesc_payload->data;

			hd->frame_control = host2ieee16(WLAN_SET_FC_FSTYPE(WLAN_FSTYPE_ASSOCRESP));	/* 0x10 */
			hd->duration_id = arg_0->hdr->a3.dur;

			MAC_COPY(hd->da, da);
			MAC_COPY(hd->sa, sa);
			MAC_COPY(hd->bssid, bssid);

			hd->sequence_control = arg_0->hdr->a3.seq;

			hdesc_header->length = WLAN_HDR_A3_LEN;
			hdesc_header->data_offset = 0;

			payload->cap_info = priv->capabilities;
			payload->status = 0;
			payload->aid = clt->aid;

			payload->rates.element_ID = 1;
			payload->rates.length = priv->rate_spt_len;
			payload->rates.sup_rates[0] = 0x82; /* 1 Mbit */
			payload->rates.sup_rates[1] = 0x84; /* 2 Mbit */
			payload->rates.sup_rates[2] = 0x8b; /* 5.5 Mbit */
			payload->rates.sup_rates[3] = 0x96; /* 11 Mbit */
			payload->rates.sup_rates[4] = 0xac; /* 22 Mbit */

			hdesc_payload->length = priv->rate_spt_len + 8;
			hdesc_payload->data_offset = 0;

			tx_desc->total_length = hdesc_payload->length + hdesc_header->length;

			acx100_dma_tx_data(priv, tx_desc);
		}
	}

	FN_EXIT(1, 1);
	return 1;
}

/*----------------------------------------------------------------
* acx100_transmit_reassocresp
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

/* acx100_transmit_reassocresp()
 * STATUS: should be ok, but UNVERIFIED.
 */
UINT32 acx100_transmit_reassocresp(wlan_fr_reassocreq_t *arg_0, wlandevice_t *priv)
{
	UINT8 *da = NULL;
	UINT8 *sa = NULL;
	UINT8 *bssid = NULL;
	struct reassocresp_frame_body *payload;
	struct txdescriptor *tx_desc;
	struct txhostdescriptor *hdesc_header;
	struct txhostdescriptor *hdesc_payload;

	client_t *clt;
	TxData *fr;

	FN_ENTER;
	acxlog(L_STATE, "%s: UNVERIFIED.\n", __func__);
	if (WLAN_GET_FC_TODS(arg_0->hdr->a3.fc) || WLAN_GET_FC_FROMDS(arg_0->hdr->a3.fc)) {
		FN_EXIT(1, 1);
		return 1;
	}

	sa = arg_0->hdr->a3.a1;
	da = arg_0->hdr->a3.a2;
	bssid = arg_0->hdr->a3.a3;

	clt = acx100_sta_list_get(priv, da);
	if (clt != NULL) {
		if (clt->used == 1)
			clt->used = 2;
	} else {
		clt = acx100_sta_list_add(priv, da);
		MAC_COPY(clt->address, da);
		clt->used = 2;
	}

	if (clt->used == 2) {
		clt->used = 3;
		if (clt->aid == 0) {
			clt->aid = CurrentAID;
			CurrentAID += 1;
		}
		clt->val0xa = arg_0->cap_info[0];

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

		if ((tx_desc = acx100_get_tx_desc(priv)) == NULL) {
			return 0;
		}
		hdesc_header = tx_desc->host_desc;
		hdesc_payload = tx_desc->host_desc + 1;
		fr = (TxData*)hdesc_header->data;
		payload = (struct reassocresp_frame_body *)hdesc_payload->data;
		fr->frame_control = WLAN_SET_FC_FSTYPE(WLAN_FSTYPE_REASSOCRESP);	/* 0x30 */
		fr->duration_id = arg_0->hdr->a3.dur;

		MAC_COPY(fr->da, da);
		MAC_COPY(fr->sa, sa);
		MAC_COPY(fr->bssid, bssid);

		fr->sequence_control = arg_0->hdr->a3.seq;

		hdesc_header->length = WLAN_HDR_A3_LEN;
		hdesc_header->data_offset = 0;

		payload->cap_info = priv->capabilities;
		payload->status = 0;
		payload->aid = clt->aid;

		payload->rates.element_ID = 1;
		payload->rates.length = priv->rate_spt_len;
		payload->rates.sup_rates[0] = 0x82; /* 1 Mbit */
		payload->rates.sup_rates[1] = 0x84; /* 2 Mbit */
		payload->rates.sup_rates[2] = 0x8b; /* 5.5 Mbit */
		payload->rates.sup_rates[3] = 0x96; /* 11 Mbit */
		payload->rates.sup_rates[4] = 0xac; /* 22 Mbit */

		hdesc_payload->data_offset = 0;
		hdesc_payload->length = priv->rate_spt_len + 8;

		tx_desc->total_length = hdesc_payload->length + hdesc_header->length;

		acx100_dma_tx_data(priv, tx_desc);
	}
	FN_EXIT(1, 0);

	return 0;
}

/*----------------------------------------------------------------
* acx100_process_disassoc
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

/* acx100_process_disassoc()
 * STATUS: UNVERIFIED.
 */
int acx100_process_disassoc(wlan_fr_disassoc_t *arg_0, wlandevice_t *priv)
{
	p80211_hdr_t *hdr;
	int res = 0;
	UINT8 *TA = NULL;
	client_t *clts;

	FN_ENTER;
	acxlog(L_STATE, "%s: UNVERIFIED.\n", __func__);
	hdr = arg_0->hdr;

	if (WLAN_GET_FC_TODS(hdr->a4.fc) || WLAN_GET_FC_FROMDS(hdr->a4.fc))
		res = 1;
	else
		TA = hdr->a3.a2;

	if (!res) {
		if ((clts = acx100_sta_list_get(priv, TA)) != NULL) {
			if (clts->used == 1) {
				acxlog(L_BINDEBUG | L_ASSOC | L_XFER,
				       "<transmit_deauth 2>\n");
				acx100_transmit_deauthen(TA, clts, priv, WLAN_MGMT_REASON_CLASS2_NONAUTH /* 6 */);
			} else
				clts->used = 2;
		} else
			res = 1;
	}
	FN_EXIT(1, res);
	return res;
}

/*----------------------------------------------------------------
* acx100_process_disassociate
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

/* acx100_process_disassociate()
 * STATUS: UNVERIFIED.
 */
int acx100_process_disassociate(wlan_fr_disassoc_t *req, wlandevice_t *priv)
{
	p80211_hdr_t *hdr;
	int res = 0;

	FN_ENTER;
	acxlog(L_STATE, "%s: UNVERIFIED.\n", __func__);
	hdr = req->hdr;

	if (WLAN_GET_FC_TODS(hdr->a3.fc) || WLAN_GET_FC_FROMDS(hdr->a3.fc))
		res = 1;
	else {
		if (ACX_MODE_0_IBSS_ADHOC == priv->macmode_joined)
			res = 1;
		else if (acx100_is_mac_address_equal(priv->dev_addr, hdr->a3.a1 /* RA */)) {
			res = 1;
			if (priv->status > ISTATUS_3_AUTHENTICATED) {
				/* priv->val0x240 = req->reason[0]; Unused, so removed */
				acx100_set_status(priv, ISTATUS_3_AUTHENTICATED);
#if (POWER_SAVE_80211 == 0)
				ActivatePowerSaveMode(priv, 2);
#endif
			}
			res = 0;
		} else
			res = 1;
	}
	FN_EXIT(1, res);
	return res;
}

/*----------------------------------------------------------------
* acx100_process_data_frame_master
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

static int acx100_process_data_frame_master(struct rxhostdescriptor *rxdesc, wlandevice_t *priv)
{
	client_t *clt;
	UINT8 bcast_addr[ETH_ALEN] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
	client_t *var_24 = NULL;
	p80211_hdr_t* p80211_hdr;
	struct txdescriptor *tx_desc;
	UINT8 esi = 0;
	int result = 0;
	int to_ds, from_ds;
	UINT8 *da = NULL;
	UINT8 *sa = NULL;
	UINT8 *bssid = NULL;

	FN_ENTER;
	acxlog(L_STATE, "%s: UNVERIFIED.\n", __func__);

        p80211_hdr = acx_get_p80211_hdr(priv, rxdesc);

	to_ds = WLAN_GET_FC_TODS(p80211_hdr->a3.fc);
	from_ds = WLAN_GET_FC_FROMDS(p80211_hdr->a3.fc);

	if ((to_ds) && (!from_ds)) {
		/* To_DS = 1, From_DS = 0 */
		da = p80211_hdr->a3.a3;	/* DA */
		sa = p80211_hdr->a3.a2;	/* SA */
		bssid = p80211_hdr->a3.a1;	/* BSSID */
	} else if ((!to_ds) && (from_ds)) {
		/* To_DS = 0, From_DS = 1 */
		da = p80211_hdr->a3.a1;	/* DA */
		sa = p80211_hdr->a3.a3;	/* SA */
		bssid = p80211_hdr->a3.a2;	/* BSSID */
	} else if ((!to_ds) && (!from_ds)) {
		/* To_DS = 0, From_DS = 0 */
		da = p80211_hdr->a3.a1;	/* DA */
		sa = p80211_hdr->a3.a2;	/* SA */
		bssid = p80211_hdr->a3.a3;	/* BSSID */
	} else {
		/* To_DS = 1, From_DS = 1 */
		acxlog(L_DEBUG, "frame error occurred??\n");
		priv->stats.rx_errors++;
		goto done;
	}

	/* check if it is our bssid, if not, leave */
	if (memcmp(bssid, priv->bssid, ETH_ALEN) == 0) {
		if (!(clt = acx100_sta_list_get(priv, bcast_addr)) || (clt->used != 3)) {
			acx100_transmit_deauthen(bcast_addr, 0, priv, WLAN_MGMT_REASON_RSVD /* 0 */);
			acxlog(L_STD, "frame error #2??\n");
			priv->stats.rx_errors++;
			goto fail;
		} else {
			esi = 2;
			/* check if the da is not broadcast */
			if (!acx100_is_mac_address_broadcast(da)) {
				if ((signed char) da[0x0] >= 0) {
					esi = 0;
					if (!(var_24 = acx100_sta_list_get(priv, da))) {
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
					acx100_rx(rxdesc, priv);
					result = 0;
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
				    WLAN_SET_FC_FROMDS(1) +
				    WLAN_SET_FC_FTYPE(WLAN_FTYPE_DATA);

				if ((tx_desc = acx100_get_tx_desc(priv)) == NULL) {
					return 0;
				}

				acx100_rxdesc_to_txdesc(rxdesc, tx_desc);
				acx100_dma_tx_data(priv, tx_desc);

				if (esi != 2) {
					goto done;
				}
			}
			acx100_rx(rxdesc, priv);
		}
	}
done:
	result = 1;
fail:
	FN_EXIT(1, result);
	return result;
}

/*----------------------------------------------------------------
* acx100_process_data_frame_client
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

/* acx100_process_data_frame_client()
 * STATUS: FINISHED, UNVERIFIED.
 */
static int acx100_process_data_frame_client(struct rxhostdescriptor *rxdesc, wlandevice_t *priv)
{
	UINT8 bcast_addr[ETH_ALEN] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
	UINT8 *da = NULL;
	UINT8 *bssid = NULL;
	p80211_hdr_t *p80211_hdr;
	int to_ds, from_ds;
	int result;

	FN_ENTER;
	acxlog(L_STATE, "%s: UNVERIFIED.\n", __func__);

	p80211_hdr = acx_get_p80211_hdr(priv, rxdesc);

	to_ds = WLAN_GET_FC_TODS(p80211_hdr->a3.fc);
	from_ds = WLAN_GET_FC_FROMDS(p80211_hdr->a3.fc);

	acxlog(L_DEBUG, "to_ds %i, from_ds %i\n", to_ds, from_ds);

	if ((to_ds) && (!from_ds)) {
		/* To_DS = 1, From_DS = 0 */
		da = p80211_hdr->a3.a3;	/* DA */
		bssid = p80211_hdr->a3.a1;	/* BSSID */
	} else if ((!to_ds) && (from_ds)) {
		/* To_DS = 0, From_DS = 1 */
		da = p80211_hdr->a3.a1;	/* DA */
		bssid = p80211_hdr->a3.a2;	/* BSSID */
	} else if ((!to_ds) && (!from_ds)) {
		/* To_DS = 0, From_DS = 0 */
		da = p80211_hdr->a3.a1;	/* DA */
		bssid = p80211_hdr->a3.a3;	/* BSSID */
	} else {
		/* To_DS = 1, From_DS = 1 */
		acxlog(L_DEBUG, "frame error occurred??\n");
		priv->stats.rx_errors++;
		goto done;
	}

	acxlog(L_DEBUG, "da ");
	acx100_log_mac_address(L_DEBUG, da);
	acxlog(L_DEBUG, ",bssid ");
	acx100_log_mac_address(L_DEBUG, bssid);
	acxlog(L_DEBUG, ",priv->bssid ");
	acx100_log_mac_address(L_DEBUG, priv->bssid);
	acxlog(L_DEBUG, ",dev_addr ");
	acx100_log_mac_address(L_DEBUG, priv->dev_addr);
	acxlog(L_DEBUG, ",bcast_addr ");
	acx100_log_mac_address(L_DEBUG, bcast_addr);
	acxlog(L_DEBUG, "\n");

#if WE_DONT_WANT_TO_REJECT_MULTICAST
	/* FIXME: we should probably reenable this code and check against
	 * a list of multicast addresses that are configured for the
	 * interface (ifconfig) */

	/* check if it is our bssid */
        if (!acx100_is_mac_address_equal(priv->bssid, bssid)) {
		/* is not our bssid, so bail out */
		goto done;
	}

	/* check if it is broadcast */
	if (!acx100_is_mac_address_broadcast(da)) {
		if ((signed char) da[0] >= 0) {
			/* no broadcast, so check if it is our address */
                        if (!acx100_is_mac_address_equal(da, priv->dev_addr)) {
				/* it's not, so bail out */
				goto done;
			}
		}
	}

	/* packet is from our bssid, and is either broadcast or destined for us, so process it */
#endif
	acx100_rx(rxdesc, priv);

done:
	result = 1;
	FN_EXIT(1, result);
	return result;
}

/*----------------------------------------------------------------
* acx100_process_mgmt_frame
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

/* acx100_process_mgmt_frame()
 * STATUS: FINISHED, UNVERIFIED. namechange!! (from process_mgnt_frame())
 * FIXME: uses global struct alloc_p80211mgmt_req, make sure there's no
 * race condition involved! (proper locking/processing)
 */
static UINT32 acx100_process_mgmt_frame(struct rxhostdescriptor *rxdesc, wlandevice_t *priv)
{
	static UINT8 reassoc_b;
	UINT8 *a;
	p80211_hdr_t *p80211_hdr;
	int wep_offset = 0;

	FN_ENTER;
	acxlog(L_STATE, "%s: UNVERIFIED.\n", __func__);

	p80211_hdr = acx_get_p80211_hdr(priv, rxdesc);
	if (WLAN_GET_FC_ISWEP(p80211_hdr->a3.fc)) {
		wep_offset = 0x10;
	}

	switch (WLAN_GET_FC_FSTYPE(p80211_hdr->a3.fc)) {
	case WLAN_FSTYPE_ASSOCREQ /* 0x00 */ :
		if (ACX_MODE_2_MANAGED_STA != priv->macmode_joined) {
			memset(&alloc_p80211mgmt_req, 0, 8 * 4);
			alloc_p80211mgmt_req.a.assocreq.buf =
			    (UINT8 *) p80211_hdr;
			alloc_p80211mgmt_req.a.assocreq.len =
			    (rxdesc->data->mac_cnt_rcvd & 0xfff) - wep_offset;

			acx_mgmt_decode_assocreq(&alloc_p80211mgmt_req.a.
						 assocreq);

			if (!memcmp
			    (alloc_p80211mgmt_req.a.assocreq.hdr->a3.a2,
			     priv->bssid, ETH_ALEN)) {
				acx100_transmit_assocresp(&alloc_p80211mgmt_req.a.
						   assocreq, priv);
			}
		}
		break;
	case WLAN_FSTYPE_ASSOCRESP /* 0x01 */ :
		if (ACX_MODE_3_MANAGED_AP != priv->macmode_joined) {
			memset(&alloc_p80211mgmt_req, 0, 8 * 4);
			alloc_p80211mgmt_req.a.assocresp.buf =
			    (UINT8 *) p80211_hdr;
			alloc_p80211mgmt_req.a.assocresp.len =
			    (rxdesc->data->mac_cnt_rcvd & 0xfff) - wep_offset;
			acx_mgmt_decode_assocresp(&alloc_p80211mgmt_req.a.
						  assocresp);
			acx100_process_assocresp(&alloc_p80211mgmt_req.a.
					  assocresp, priv);
		}
		break;
	case WLAN_FSTYPE_REASSOCREQ /* 0x02 */ :
		if (ACX_MODE_2_MANAGED_STA != priv->macmode_joined) {
			reassoc_b = 0;

			memset(&alloc_p80211mgmt_req.a.assocreq, 0, 9 * 4);
			alloc_p80211mgmt_req.a.assocreq.buf =
			    (UINT8 *) p80211_hdr;
			alloc_p80211mgmt_req.a.assocreq.len =
			    (rxdesc->data->mac_cnt_rcvd & 0xfff) - wep_offset;

			acx_mgmt_decode_assocreq(&alloc_p80211mgmt_req.a.
						 assocreq);

			//reassocreq and assocreq are equivalent
			acx100_transmit_reassocresp(&alloc_p80211mgmt_req.a.
					     reassocreq, priv);
		}
		break;
	case WLAN_FSTYPE_REASSOCRESP /* 0x03 */ :
		if (ACX_MODE_3_MANAGED_AP != priv->macmode_joined) {
			memset(&alloc_p80211mgmt_req.a.assocresp, 0,
			       8 * 4);
			alloc_p80211mgmt_req.a.assocresp.buf =
			    (UINT8 *) p80211_hdr;
			alloc_p80211mgmt_req.a.assocresp.len =
			    (rxdesc->data->mac_cnt_rcvd & 0xfff) - wep_offset;

			acx_mgmt_decode_assocresp(&alloc_p80211mgmt_req.a.
						  assocresp);
			acx100_process_reassocresp(&alloc_p80211mgmt_req.a.
					    reassocresp, priv);
		}
		break;
	case WLAN_FSTYPE_PROBEREQ /* 0x04 */ :
		if (ACX_MODE_3_MANAGED_AP == priv->macmode_joined) {
			acxlog(L_ASSOC, "FIXME: since we're supposed to be an AP, we need to return a Probe Response packet!\n");
		}
		break;
	case WLAN_FSTYPE_PROBERESP /* 0x05 */ :
		if (ACX_MODE_3_MANAGED_AP != priv->macmode_joined) {

			memset(&alloc_p80211mgmt_req, 0, 0xd * 4);
			alloc_p80211mgmt_req.a.proberesp.buf =
			    (UINT8 *) p80211_hdr;
			alloc_p80211mgmt_req.a.proberesp.len =
			    (rxdesc->data->mac_cnt_rcvd & 0xfff) - wep_offset;
			acx_mgmt_decode_proberesp(&alloc_p80211mgmt_req.a.
						  proberesp);
			if (priv->status == ISTATUS_1_SCANNING)
				acx100_process_probe_response(rxdesc->data,
						     priv,
						     (acxp80211_hdr_t *)
						     alloc_p80211mgmt_req.
						     a.proberesp.hdr);
		}
		break;
	case 6:
	case 7:
		// exit
		break;
	case WLAN_FSTYPE_BEACON /* 0x08 */ :
		if (ACX_MODE_3_MANAGED_AP != priv->macmode_joined) {
			switch (priv->status) {
			   case ISTATUS_1_SCANNING:
			   case ISTATUS_5_UNKNOWN:
				memset(&alloc_p80211mgmt_req.a.beacon, 0,
				       0xe * 4);
				alloc_p80211mgmt_req.a.beacon.buf =
				    (char *) p80211_hdr;
				alloc_p80211mgmt_req.a.beacon.len =
				    (rxdesc->data->mac_cnt_rcvd & 0xfff) - wep_offset;
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
				acx_mgmt_decode_beacon
				    (&alloc_p80211mgmt_req.a.beacon);
				acx100_process_probe_response(rxdesc->data,
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
		} else {
			acxlog(L_DEBUG,
			       "Incoming beacon message not handled in mode %d.\n",
			       priv->macmode_joined);
		}
		break;
	case WLAN_FSTYPE_ATIM /* 0x09 */ :
		// exit
		break;
	case WLAN_FSTYPE_DISASSOC /* 0x0a */ :
		memset(&alloc_p80211mgmt_req.a.disassoc, 0, 5 * 4);
		alloc_p80211mgmt_req.a.disassoc.buf =
		    (UINT8 *) p80211_hdr;
		alloc_p80211mgmt_req.a.disassoc.len = //rxdesc->p80211frmlen;
			    (rxdesc->data->mac_cnt_rcvd & 0xfff) - wep_offset;
		acx_mgmt_decode_disassoc(&alloc_p80211mgmt_req.a.disassoc);
		if (ACX_MODE_3_MANAGED_AP != priv->macmode_joined) {
			acx100_process_disassoc(&alloc_p80211mgmt_req.a.disassoc,
					 priv);
		}
		else
		if ((ACX_MODE_0_IBSS_ADHOC == priv->macmode_joined)
		 || (ACX_MODE_2_MANAGED_STA == priv->macmode_joined)) {
			acx100_process_disassociate(&alloc_p80211mgmt_req.a.
					     disassoc, priv);
		}
		break;
	case WLAN_FSTYPE_AUTHEN /* 0x0b */ :
		memset(&alloc_p80211mgmt_req.a.authen, 0, 8 * 4);
		alloc_p80211mgmt_req.a.authen.buf =
		    (UINT8 *) p80211_hdr;
		alloc_p80211mgmt_req.a.authen.len = //rxdesc->p80211frmlen;
			    (rxdesc->data->mac_cnt_rcvd & 0xfff) - wep_offset;
		acx_mgmt_decode_authen(&alloc_p80211mgmt_req.a.authen);
		if (!memcmp(priv->bssid,
			    alloc_p80211mgmt_req.a.authen.hdr->a3.a2,
			    ETH_ALEN)) {
			acx100_process_authen(&alloc_p80211mgmt_req.a.authen,
				       priv);
		}
		break;
	case WLAN_FSTYPE_DEAUTHEN /* 0x0c */ :
		memset(&alloc_p80211mgmt_req.a.deauthen, 0, 5 * 4);
		alloc_p80211mgmt_req.a.deauthen.buf =
		    (UINT8 *) p80211_hdr;
		alloc_p80211mgmt_req.a.deauthen.len =
		    (rxdesc->data->mac_cnt_rcvd & 0xfff) - wep_offset;
		acx_mgmt_decode_deauthen(&alloc_p80211mgmt_req.a.deauthen);
		if (ACX_MODE_3_MANAGED_AP != priv->macmode_joined) {
			acx100_process_deauthen(&alloc_p80211mgmt_req.a.deauthen,
					 priv);
		}
		else
		if ((ACX_MODE_0_IBSS_ADHOC == priv->macmode_joined)
		 || (ACX_MODE_2_MANAGED_STA == priv->macmode_joined)) {
			acx100_process_deauthenticate(&alloc_p80211mgmt_req.a.
					       deauthen, priv);
		}
		break;
	}

	FN_EXIT(1, 0);
	return 0;
}

#if UNUSED
/*----------------------------------------------------------------
* acx100_process_class_frame
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


/* acx100_process_class_frame()
 * STATUS: FINISHED.
 */
static int acx100_process_class_frame(struct rxhostdescriptor *skb, wlandevice_t *priv, int vala)
{
	return 1;
}
#endif

/*----------------------------------------------------------------
* acx100_process_NULL_frame
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

/* acx100_process_NULL_frame()
 * STATUS: FINISHED, UNVERIFIED.
 */
static int acx100_process_NULL_frame(struct rxhostdescriptor *rxdesc, wlandevice_t *priv, int vala)
{
	UINT16 fc;
	signed char *esi = NULL;
	UINT8 *ebx = NULL;
	p80211_hdr_t *p80211_hdr;
	client_t *client;
	client_t *resclt = NULL;
	int result = 0;

	acxlog(L_STATE, "%s: UNVERIFIED.\n", __func__);

	p80211_hdr = acx_get_p80211_hdr(priv, rxdesc);
		
	fc = p80211_hdr->a3.fc;

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
		result = 1;
		goto done;
	}
	for (client = acx100_sta_list_get_from_hash(priv, ebx); client;
	     client = client->next) {
		if (!memcmp(ebx, client->address, ETH_ALEN)) {
			resclt = client;
			break;
		}
	}

	if (resclt)
		result = 0;
	else {
#if IS_IT_BROKEN
		acxlog(L_BINDEBUG | L_XFER, "<transmit_deauth 7>\n");
		acx100_transmit_deauthen(ebx, 0x0, priv, WLAN_MGMT_REASON_CLASS2_NONAUTH /* 6 */);
#else
		acxlog(L_STD, "received NULL frame from unknown client! We really shouldn't send deauthen here, right?\n");
#endif
		result = 1;
	}
done:
	return result;
}

/*----------------------------------------------------------------
* acx100_process_probe_response
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
void acx100_process_probe_response(struct rxbuffer *mmt, wlandevice_t *priv,
			  acxp80211_hdr_t *hdr)
{
	UINT8 *pSuppRates;
	UINT8 *pDSparms;
	UINT32 station;
	UINT8 *a;
	struct bss_info *ss;
	UINT8 rate_count;
	int i, max_rate = 0;

	acxlog(L_STATE, "%s: UNVERIFIED, previous bss_table_count: %d.\n",
	       __func__, priv->bss_table_count);

	FN_ENTER;

	/* uh oh, we found more sites/stations than we can handle with
	 * our current setup: pull the emergency brake and stop scanning! */
	if ((UINT) priv->bss_table_count > MAX_NUMBER_OF_SITE) {
		acx100_issue_cmd(priv, ACX100_CMD_STOP_SCAN, NULL, 0, 5000);
		acx100_set_status(priv, ISTATUS_2_WAIT_AUTH);

		acxlog(L_BINDEBUG | L_ASSOC,
		       "<Scan Beacon> bss_table_count > MAX_NUMBER_OF_SITE\n");
		FN_EXIT(0, 0);
		return;
	}

	/* pSuppRates points to the Supported Rates element: info[1] is essid_len */
	pSuppRates = &hdr->info[hdr->info[0x1] + 0x2];
	/* pDSparms points to the DS Parameter Set */
	pDSparms = &pSuppRates[pSuppRates[0x1] + 0x2];

	/* filter out duplicate stations we already registered in our list */
	for (station = 0; station < priv->bss_table_count; station++) {
		UINT8 *a = priv->bss_table[station].bssid;
		acxlog(L_DEBUG,
		       "checking station %ld [%02X %02X %02X %02X %02X %02X]\n",
		       station, a[0], a[1], a[2], a[3], a[4], a[5]);
		if (acx100_is_mac_address_equal
		    (hdr->a4.a3,
		     priv->bss_table[station].bssid)) {
			acxlog(L_DEBUG,
			       "station already in our list, no need to add.\n");
			FN_EXIT(0, 0);
			return;
		}
	}

	for (station = 0;
	     station < hdr->info[0x1] && (hdr->info[0x2 + station] == 0);
	     station++) {
	};
	/* FIXME: this line does nothing ^^^^ */

	/* NONBIN_DONE: bug in V3: doesn't reset scan structs when
	 * starting next site survey scan (leading to garbled ESSID strings with
	 * old garbage). Thus let's completely zero out the entry that we're
	 * going to fill next in order to not risk any corruption. */
	memset(&priv->bss_table[priv->bss_table_count], 0, sizeof(struct bss_info));

	/* copy the BSSID element */
	MAC_COPY(priv->bss_table[priv->bss_table_count].bssid, hdr->a4.a3);
	/* copy the MAC address element (source address) */
	MAC_COPY(priv->bss_table[priv->bss_table_count].mac_addr, hdr->a4.a2);

	/* copy the ESSID element */
	if (hdr->info[0x1] <= IW_ESSID_MAX_SIZE) {
		priv->bss_table[priv->bss_table_count].essid_len = hdr->info[0x1];
		memcpy(priv->bss_table[priv->bss_table_count].essid,
		       &hdr->info[0x2], hdr->info[0x1]);
		priv->bss_table[priv->bss_table_count].essid[hdr->info[0x1]] = '\0';
	}
	else {
		acxlog(L_STD, "huh, ESSID overflow in scanned station data?\n");
	}

	priv->bss_table[priv->bss_table_count].channel = pDSparms[2];
	priv->bss_table[priv->bss_table_count].wep = (hdr->caps & IEEE802_11_MGMT_CAP_WEP);
	priv->bss_table[priv->bss_table_count].caps = hdr->caps;
	rate_count = pSuppRates[1];
	if (rate_count > 64)
		rate_count = 64;
	memcpy(priv->bss_table[priv->bss_table_count].supp_rates,
	       &pSuppRates[2], rate_count);
	priv->bss_table[priv->bss_table_count].sir = acx_signal_to_winlevel(mmt->phy_level);
	priv->bss_table[priv->bss_table_count].snr = acx_signal_to_winlevel(mmt->phy_snr);

	a = priv->bss_table[priv->bss_table_count].bssid;
	ss = &priv->bss_table[priv->bss_table_count];

	acxlog(L_DEBUG, "Supported Rates:\n");
	/* find max. transfer rate */
	for (i=0; i < rate_count; i++)
	{
		acxlog(L_DEBUG, "%s Rate: %d%sMbps (0x%02X)\n",
			(pSuppRates[2+i] & 0x80) ? "Basic" : "Operational",
			(int)((pSuppRates[2+i] & ~0x80) / 2),
			(pSuppRates[2+i] & 1) ? ".5" : "", pSuppRates[2+i]);
		if ((pSuppRates[2+i] & ~0x80) > max_rate)
			max_rate = pSuppRates[2+i] & ~0x80;
	}
	acxlog(L_DEBUG, ".\n");

	acxlog(L_STD | L_ASSOC,
	       "%s: found and registered station %d: ESSID \"%s\" on channel %d, BSSID %02X %02X %02X %02X %02X %02X, %s/%d%sMbps, Caps 0x%04x, SIR %ld, SNR %ld.\n",
	       __func__,
	       priv->bss_table_count,
	       ss->essid, ss->channel,
	       a[0], a[1], a[2], a[3], a[4], a[5],
	       (ss->caps & IEEE802_11_MGMT_CAP_IBSS) ? "Ad-Hoc peer" : "Access Point",
	       (int)(max_rate / 2), (max_rate & 1) ? ".5" : "",
	       ss->caps, ss->sir, ss->snr);

	/* found one station --> increment counter */
	priv->bss_table_count++;
	FN_EXIT(0, 0);
}

const char * const status_str[22] =
{ "Successful", "Unspecified failure",
  "Reserved error code", "Reserved error code", "Reserved error code",
  "Reserved error code", "Reserved error code", "Reserved error code",
  "Reserved error code", "Reserved error code",
  "Cannot support all requested capabilities in the Capability Information field. TRANSLATION: Bug in ACX100 driver?",
  "Reassociation denied due to reason outside the scope of 802.11b standard. TRANSLATION: Bug in ACX100 driver?",
  "Association denied due to reason outside the scope of 802.11b standard. TRANSLATION: peer station probably has MAC filtering enabled, FIX IT!",
  "Responding station does not support the specified authentication algorithm. TRANSLATION: invalid network data or bug in ACX100 driver?",
  "Received an Authentication frame with transaction sequence number out of expected sequence. TRANSLATION: Bug in ACX100 driver?",
  "Authentication rejected because of challenge failure. TRANSLATION: Bug in ACX100 driver?",
  "Authentication rejected due to timeout waiting for next frame in sequence. TRANSLATION: Bug in ACX100 driver?",
  "Association denied because AP is unable to handle additional associated stations",
  "Association denied due to requesting station not supporting all of the data rates in the BSSBasicRateSet parameter. TRANSLATION: peer station has an incompatible set of data rates configured, FIX IT!",
  "Association denied due to requesting station not supporting the Short Preamble option. TRANSLATION: Bug in ACX100 driver?",
  "Association denied due to requesting station not supporting the PBCC Modulation option. TRANSLATION: Bug in ACX100 driver?",
  "Association denied due to requesting station not supporting the Channel Agility option. TRANSLATION: Bug in ACX100 driver?"};
inline const char * const get_status_string(int status)
{
	  return status <= 21 ? status_str[status] : "Reserved";
}

/*----------------------------------------------------------------
* acx100_process_assocresp
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

/* acx100_process_assocresp()
 * STATUS: should be ok, UNVERIFIED.
 */
int acx100_process_assocresp(wlan_fr_assocresp_t *req, wlandevice_t *priv)
{
	p80211_hdr_t *hdr;
	int res = 0;
	memmap_t pdr;

	FN_ENTER;
	acxlog(L_STATE, "%s: UNVERIFIED.\n", __func__);
	hdr = req->hdr;

	if (WLAN_GET_FC_TODS(hdr->a4.fc) || WLAN_GET_FC_FROMDS(hdr->a4.fc))
		res = 1;
	else {
		if (0 != acx100_is_mac_address_equal(priv->dev_addr, hdr->a4.a1 /* RA */)) {
			if (WLAN_MGMT_STATUS_SUCCESS == req->status[0]) {
				pdr.m.asid.vala = req->aid[0];
				acx100_configure(priv, &pdr, ACX100_RID_ASSOC_ID);
				acx100_set_status(priv, ISTATUS_4_ASSOCIATED);
				acxlog(L_BINSTD | L_ASSOC, "ASSOCIATED!\n");
			}
			else {
				acxlog(L_STD | L_ASSOC, "Association FAILED: peer station sent response status code %d: \"%s\"!\n", req->status[0], get_status_string(req->status[0]));
			}
			res = 0;
		} else
			res = 1;
	}
	FN_EXIT(1, res);
	return res;
}

/*----------------------------------------------------------------
* acx100_process_reassocresp
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

/* acx100_process_reassocresp()
 * STATUS: should be ok, UNVERIFIED.
 */
int acx100_process_reassocresp(wlan_fr_reassocresp_t *req, wlandevice_t *priv)
{
	p80211_hdr_t *hdr = req->hdr;
	int result = 4;

	FN_ENTER;
	acxlog(L_STATE, "%s: UNVERIFIED.\n", __func__);

	if (WLAN_GET_FC_TODS(hdr->a3.fc) || WLAN_GET_FC_FROMDS(hdr->a3.fc)) {
		result = 1;
	} else {
		if (ACX_MODE_0_IBSS_ADHOC == priv->macmode_joined) {
			result = 2;
		} else if (acx100_is_mac_address_equal(priv->dev_addr, hdr->a3.a1 /* RA */)) {
			if (req->status[0] == WLAN_MGMT_STATUS_SUCCESS) {
				acx100_set_status(priv, ISTATUS_4_ASSOCIATED);
			} else {
				acxlog(L_STD | L_ASSOC, "Reassociation FAILED: response status code %d: \"%s\"!\n", req->status[0], get_status_string(req->status[0]));
			}
			result = 0;
		} else {
			result = 3;
		}
	}
	FN_EXIT(1, result);
	return result;
}

/* acx100_process_authen()
 * STATUS: FINISHED, UNVERIFIED.
 */
int acx100_process_authen(wlan_fr_authen_t *req, wlandevice_t *priv)
{
	p80211_hdr_t *hdr;
	client_t *clt;
	client_t *currclt;
	int result = -1;

	FN_ENTER;
	acxlog(L_STATE|L_ASSOC, "%s: UNVERIFIED.\n", __func__);
	hdr = req->hdr;
	if (WLAN_GET_FC_TODS(hdr->a3.fc) || WLAN_GET_FC_FROMDS(hdr->a3.fc)) {
		result = 0;
		goto end;
	}

	if (!priv) {
		result = 0;
		goto end;
	}

	acx100_log_mac_address(L_ASSOC, priv->dev_addr);
	acx100_log_mac_address(L_ASSOC, hdr->a3.a1);
	acx100_log_mac_address(L_ASSOC, hdr->a3.a2);
	acx100_log_mac_address(L_ASSOC, hdr->a3.a3);
	acx100_log_mac_address(L_ASSOC, priv->bssid);
	acxlog(L_ASSOC, "\n");
	
	if (!acx100_is_mac_address_equal(priv->dev_addr, hdr->a3.a1) &&
			!acx100_is_mac_address_equal(priv->bssid, hdr->a3.a1)) {
		result = 1;
		goto end;
	}
	if (ACX_MODE_0_IBSS_ADHOC == priv->macmode_joined) {
		result = 0;
		goto end;
	}

	if (priv->auth_alg <= 1) {
		if (priv->auth_alg != *(req->auth_alg))
		{
			acxlog(L_ASSOC, "authentication algorithm mismatch: want: %ld, req: %d\n", priv->auth_alg, *(req->auth_alg));
			result = 0;
			goto end;
		}
	}
	acxlog(L_ASSOC,"Algorithm is ok\n");
	currclt = acx100_sta_list_get_from_hash(priv, hdr->a3.a2);
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
		if (!(clt = acx100_sta_list_add(priv, hdr->a3.a2)))
		{
			acxlog(L_ASSOC,"Could not allocate room for this client\n");
			result = 0;
			goto end;
		}

		MAC_COPY(clt->address, hdr->a3.a2);
		clt->used = 1;
	}
	/* now check which step in the authentication sequence we are
	 * currently in, and act accordingly */
	acxlog(L_ASSOC, "acx100_process_authen auth seq step %d.\n", req->auth_seq[0]);
	switch (req->auth_seq[0]) {
	case 1:
		if (ACX_MODE_2_MANAGED_STA == priv->macmode_joined)
			break;
		acx100_transmit_authen2(req, clt, priv);
		break;
	case 2:
		if (ACX_MODE_3_MANAGED_AP == priv->macmode_joined)
			break;
		if (req->status[0] == WLAN_MGMT_STATUS_SUCCESS) {
			if (req->auth_alg[0] == WLAN_AUTH_ALG_OPENSYSTEM) {
				acx100_set_status(priv, ISTATUS_3_AUTHENTICATED);
				acx100_transmit_assoc_req(priv);
			} else
			if (req->auth_alg[0] == WLAN_AUTH_ALG_SHAREDKEY) {
				acx100_transmit_authen3(req, priv);
			}
		} else {
			acxlog(L_ASSOC, "Authentication FAILED (status code %d: \"%s\"), still waiting for authentication.\n", req->status[0], get_status_string(req->status[0]));
			acx100_set_status(priv, ISTATUS_2_WAIT_AUTH);
		}
		break;
	case 3:
		if ((ACX_MODE_2_MANAGED_STA == priv->macmode_joined)
		    || (clt->auth_alg != WLAN_AUTH_ALG_SHAREDKEY)
		    || (req->auth_alg[0] != WLAN_AUTH_ALG_SHAREDKEY)
		    || (clt->val0xe != 2))
			break;
		acxlog(L_STD,
		       "FIXME: TODO: huh??? incompatible data type!\n");
		currclt = (client_t *)req->challenge;
		if (0 == memcmp(currclt->address, clt->val0x18, 0x80)
		    && (((UINT8) currclt->aid) != 0x10))
			if ((currclt->aid >> 8) != 0x80)
				break;
		acx100_transmit_authen4(req, priv);
		MAC_COPY(clt->address, hdr->a3.a2);
		clt->used = 2;
		clt->val0xe = 4;
		clt->val0x98 = hdr->a3.seq;
		break;
	case 4:
		if (ACX_MODE_3_MANAGED_AP == priv->macmode_joined)
			break;

		/* ok, we're through: we're authenticated. Woohoo!! */
		acx100_set_status(priv, ISTATUS_3_AUTHENTICATED);
		acxlog(L_BINSTD | L_ASSOC, "Authenticated!\n");
		acx100_transmit_assoc_req(priv);
		break;
	}
	result = 0;
end:
	FN_EXIT(1, result);
	return result;
}

/*----------------------------------------------------------------
* acx100_process_deauthen
*
* Arguments:
*
* Returns:
*	0 on all is good
*	1 on any error
* Side effects:
*
* Call context:
*
* STATUS:
*
* Comment:
*
*----------------------------------------------------------------*/

/* acx100_process_deauthen()
 * STATUS: FINISHED, UNVERIFIED.
 */
int acx100_process_deauthen(wlan_fr_deauthen_t *arg_0, wlandevice_t *priv)
{
	p80211_hdr_t *hdr;
	int result;
	UINT8 *addr;
	client_t *client;
	client_t *resclt = NULL;

	FN_ENTER;
	acxlog(L_STATE, "%s: UNVERIFIED.\n", __func__);

	hdr = arg_0->hdr;

	if ((0 != WLAN_GET_FC_TODS(hdr->a3.fc)) || (0 != WLAN_GET_FC_FROMDS(hdr->a3.fc)))
	{
		result = 1;
		goto end;
	}

	acxlog(L_ASSOC, "DEAUTHEN ");
	acx100_log_mac_address(L_ASSOC, priv->dev_addr);
	acx100_log_mac_address(L_ASSOC, hdr->a3.a1);
	acx100_log_mac_address(L_ASSOC, hdr->a3.a2);
	acx100_log_mac_address(L_ASSOC, hdr->a3.a3);
	acx100_log_mac_address(L_ASSOC, priv->bssid);
	acxlog(L_ASSOC, "\n");
	
	if (!acx100_is_mac_address_equal(priv->dev_addr, hdr->a3.a1) &&
			!acx100_is_mac_address_equal(priv->bssid, hdr->a3.a1)) {
		result = 1;
		goto end;
	}
	if (ACX_MODE_0_IBSS_ADHOC == priv->macmode_joined) {
		result = 0;
		goto end;
	}
	
	acxlog(L_STD, "Processing deauthen packet. Hmm, should this have happened?\n");

	addr = hdr->a3.a2;
	if (memcmp(addr, priv->dev_addr, ETH_ALEN))
	{
		/* OK, we've been asked to leave the ESS. Do we 
		 * ask to return or do we leave quietly? I'm 
		 * guessing that since we are still up and 
		 * running we should attempt to rejoin, from the 
		 * starting point. So:
		 */
		acx100_set_status(priv,ISTATUS_2_WAIT_AUTH);
		result = 0;
		goto end;
	}			

	client = acx100_sta_list_get_from_hash(priv, addr);

	if (client == NULL) {
		result = 0;
		goto end;
	}

	do {
		if (0 == memcmp(addr, &client->address, ETH_ALEN)) {
			resclt = client;
			goto end;
		}
	} while ((client = client->next));
	resclt = NULL;

end:
	if (resclt) {
		resclt->used = (UINT8)1;
		result = 0;
	} else
		result = 1;

	FN_EXIT(1, result);
	return result;
}

/*----------------------------------------------------------------
* acx100_process_deauthenticate
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

/* acx100_process_deauthenticate()
 * STATUS: FINISHED, UNVERIFIED.
 */
int acx100_process_deauthenticate(wlan_fr_deauthen_t *req, wlandevice_t *priv)
{
	p80211_hdr_t *hdr;

	FN_ENTER;
	acxlog(L_STATE, "%s: UNVERIFIED.\n", __func__);
	acxlog(L_STD, "processing deauthenticate packet. Hmm, should this have happened?\n");
	hdr = req->hdr;
	if (WLAN_GET_FC_TODS(hdr->a3.fc) || WLAN_GET_FC_FROMDS(hdr->a3.fc))
		return 1;
	else {
		if (ACX_MODE_0_IBSS_ADHOC == priv->macmode_joined)
			return 1;
		if (acx100_is_mac_address_equal(priv->dev_addr, hdr->a3.a1)) {
			if (priv->status > ISTATUS_2_WAIT_AUTH) {
				acx100_set_status(priv, ISTATUS_2_WAIT_AUTH);
				return 0;
			}
		}
	}
	FN_EXIT(1, 1);
	return 1;
}

/*----------------------------------------------------------------
* acx100_transmit_deauthen
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

/* acx100_transmit_deauthen()
 * STATUS: should be ok, but UNVERIFIED.
 */
int acx100_transmit_deauthen(char *a, client_t *clt, wlandevice_t *priv, UINT16 reason)
{
	TxData *hd;
	struct deauthen_frame_body *payload;
	struct txdescriptor *tx_desc;
	struct txhostdescriptor *hdesc_header;
	struct txhostdescriptor *hdesc_payload;

	FN_ENTER;
	acxlog(L_STATE, "%s: UNVERIFIED.\n", __func__);

	if ((tx_desc = acx100_get_tx_desc(priv)) == NULL) {
		return 0;
	}

	hdesc_header = tx_desc->host_desc;
	hdesc_payload = tx_desc->host_desc + 1;

	hd = (TxData *)hdesc_header->data;
	payload = (struct deauthen_frame_body *)hdesc_payload->data;

	hd->frame_control = WLAN_SET_FC_FTYPE(WLAN_FTYPE_MGMT) | WLAN_SET_FC_FSTYPE(WLAN_FSTYPE_DEAUTHEN);	/* 0xc0 */
	hd->duration_id = 0;

	if (clt) {
		clt->used = (UINT8)1;
		MAC_COPY(hd->da, clt->address);
	} else {
		MAC_COPY(hd->da, a);
	}
	MAC_COPY(hd->sa, priv->dev_addr);
	/* FIXME: this used to use dev_addr, but I think it should use
	 * the BSSID of the network we're associated to: priv->bssid */
	MAC_COPY(hd->bssid, priv->bssid);
	
	hd->sequence_control = 0;

	acxlog(L_BINDEBUG | L_ASSOC | L_XFER,
	       "<acx100_transmit_deauthen>sta=%02X:%02X:%02X:%02X:%02X:%02X for %d\n",
	       hd->da[0x0], hd->da[0x1], hd->da[0x2],
	       hd->da[0x3], hd->da[0x4], hd->da[0x5], reason);

	hdesc_header->length = WLAN_HDR_A3_LEN;
	hdesc_header->data_offset = 0;

	payload->reason = reason;

	hdesc_payload->length = sizeof(deauthen_frame_body_t);
	hdesc_payload->data_offset = 0;

	tx_desc->total_length = hdesc_payload->length + hdesc_header->length;

	acx100_dma_tx_data(priv, tx_desc);

	FN_EXIT(1, 0);
	return 0;
}

/*----------------------------------------------------------------
* acx100_transmit_authen1
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

/* acx100_transmit_authen1()
 * STATUS: UNVERIFIED
 */
int acx100_transmit_authen1(wlandevice_t *priv)
{
	struct auth_frame_body *payload;
	struct txdescriptor *tx_desc;
	struct txhostdescriptor *hdesc_header;
	struct txhostdescriptor *hdesc_payload;
	TxData *hd;

	acxlog(L_STATE, "%s: UNVERIFIED.\n", __func__);

	FN_ENTER;

	acxlog(L_BINSTD | L_ASSOC, "Sending authentication1 request, awaiting response!\n");

	if ((tx_desc = acx100_get_tx_desc(priv)) == NULL) {
		return 1;
	}

	hdesc_header = tx_desc->host_desc;
	hdesc_payload = tx_desc->host_desc + 1;

	hd = (TxData *)hdesc_header->data;
	payload = (struct auth_frame_body *)hdesc_payload->data;

	hd->frame_control = WLAN_SET_FC_FSTYPE(WLAN_FSTYPE_AUTHEN);	/* 0xb0 */
	hd->duration_id = host2ieee16(0x8000);

	MAC_COPY(hd->da, priv->bssid);
	MAC_COPY(hd->sa, priv->dev_addr);
	MAC_COPY(hd->bssid, priv->bssid);

	hd->sequence_control = 0;

	hdesc_header->length = WLAN_HDR_A3_LEN;
	hdesc_header->data_offset = 0;

	payload->auth_alg = priv->auth_alg;
	payload->auth_seq = 1;
	payload->status = 0;

	hdesc_payload->length = 2 + 2 + 2; /* 6 */
	hdesc_payload->data_offset = 0;

	tx_desc->total_length = hdesc_payload->length + hdesc_header->length;

	acx100_dma_tx_data(priv, tx_desc);
	FN_EXIT(0, 0);
	return 0;
}

/*----------------------------------------------------------------
* acx100_transmit_authen2
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

/* acx100_transmit_authen2()
 * STATUS: UNVERIFIED. (not binary compatible yet)
 */
int acx100_transmit_authen2(wlan_fr_authen_t *arg_0, client_t *sta_list,
		      wlandevice_t *priv)
{
	UINT32 packet_len;
	struct auth_frame_body *payload;
	struct txdescriptor *tx_desc;
	struct txhostdescriptor *hdesc_header;
	struct txhostdescriptor *hdesc_payload;
	TxData *hd;

	FN_ENTER;
	acxlog(L_STATE, "%s: UNVERIFIED.\n", __func__);
	packet_len = WLAN_HDR_A3_LEN;

	if (sta_list != NULL) {
		MAC_COPY(sta_list->address, arg_0->hdr->a3.a2);
		sta_list->val0x8 = WLAN_GET_FC_PWRMGT(arg_0->hdr->a3.fc);
		sta_list->auth_alg = *(arg_0->auth_alg);
		sta_list->val0xe = 2;
		sta_list->val0x98 = arg_0->hdr->a3.seq;

		if ((tx_desc = acx100_get_tx_desc(priv)) == NULL) {
			return 1;
		}

		hdesc_header = tx_desc->host_desc;
		hdesc_payload = tx_desc->host_desc + 1;

		hd = (TxData*)hdesc_header->data;
		payload = (struct auth_frame_body *)hdesc_payload->data;

		hd->frame_control = WLAN_SET_FC_FSTYPE(WLAN_FSTYPE_AUTHEN);	/* 0xb0 */
		hd->duration_id = arg_0->hdr->a3.dur;
		hd->sequence_control = arg_0->hdr->a3.seq;

		MAC_COPY(hd->da, arg_0->hdr->a3.a2);
		MAC_COPY(hd->sa, arg_0->hdr->a3.a1);
		MAC_COPY(hd->bssid, arg_0->hdr->a3.a3);

		payload->auth_alg = *(arg_0->auth_alg);

		payload->auth_seq = 2;

		payload->status = 0;

		if (*(arg_0->auth_alg) == WLAN_AUTH_ALG_OPENSYSTEM) {
			sta_list->used = (UINT8)2;
			packet_len += 2 + 2 + 2;
		} else {	/* shared key */
			acx100_gen_challenge(&payload->challenge);
			memcpy(&sta_list->val0x18, payload->challenge.text, 0x80);
			packet_len += 2 + 2 + 2 + 1+1+0x80;
		}

		hdesc_header->length = WLAN_HDR_A3_LEN;
		hdesc_header->data_offset = 0;
		hdesc_payload->length = packet_len - WLAN_HDR_A3_LEN;
		hdesc_payload->data_offset = 0;
		tx_desc->total_length = hdesc_payload->length + hdesc_header->length;

		acxlog(L_BINDEBUG | L_ASSOC | L_XFER,
		       "<transmit_auth2> BSSID=%02X:%02X:%02X:%02X:%02X:%02X\n",
		       hd->bssid[0], hd->bssid[1], hd->bssid[2],
		       hd->bssid[3], hd->bssid[4], hd->bssid[5]);

		acx100_dma_tx_data(priv, tx_desc);
	}
	FN_EXIT(1, 0);
	return 0;
}

/*----------------------------------------------------------------
* acx100_transmit_authen3
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

/* acx100_transmit_authen3()
 * STATUS: UNVERIFIED.
 */
int acx100_transmit_authen3(wlan_fr_authen_t *arg_0, wlandevice_t *priv)
{
	UINT32 packet_len;
	struct txdescriptor *tx_desc;
	struct txhostdescriptor *hdesc_header;
	struct txhostdescriptor *hdesc_payload;
	TxData *hd;
	struct auth_frame_body *payload;

	FN_ENTER;
	acxlog(L_STATE, "%s: UNVERIFIED.\n", __func__);
	if ((tx_desc = acx100_get_tx_desc(priv)) == NULL) {
		FN_EXIT(1, 1);
		return 1;
	}

	packet_len = WLAN_HDR_A3_LEN;

	hdesc_header = tx_desc->host_desc;
	hdesc_payload = tx_desc->host_desc + 1;

	hd = (TxData *)hdesc_header->data;
	payload = (struct auth_frame_body *)hdesc_payload->data;

	hd->frame_control = WLAN_SET_FC_ISWEP(1) + WLAN_SET_FC_FSTYPE(WLAN_FSTYPE_AUTHEN);	/* 0x40b0 */

	/* FIXME: is this needed?? authen4 does it...
	hd->duration_id = arg_0->hdr->a3.dur;
	hd->sequence_control = arg_0->hdr->a3.seq;
	*/
	MAC_COPY(hd->da, priv->bssid);
	MAC_COPY(hd->sa, priv->dev_addr);
	MAC_COPY(hd->bssid, priv->bssid);

	hdesc_header->length = WLAN_HDR_A3_LEN;
	hdesc_header->data_offset = 0;

	payload->auth_alg = *(arg_0->auth_alg);

	payload->auth_seq = 3;

	payload->status = 0;

	memcpy(&payload->challenge, arg_0->challenge, arg_0->challenge->len + 2);

	packet_len += 8 + arg_0->challenge->len;

	hdesc_payload->length = packet_len - WLAN_HDR_A3_LEN;
	hdesc_payload->data_offset = 0;

	tx_desc->total_length = hdesc_payload->length + hdesc_header->length;

	acxlog(L_BINDEBUG | L_ASSOC | L_XFER, "transmit_auth3!\n");

	acx100_dma_tx_data(priv, tx_desc);
	FN_EXIT(1, 0);
	return 0;
}

/*----------------------------------------------------------------
* acx100_transmit_authen4
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

/* acx100_transmit_authen4()
 * STATUS: UNVERIFIED.
 */
int acx100_transmit_authen4(wlan_fr_authen_t *arg_0, wlandevice_t *priv)
{
	struct txdescriptor *tx_desc;
	struct txhostdescriptor *hdesc_header;
	struct txhostdescriptor *hdesc_payload;
	TxData *hd;
	struct auth_frame_body *payload;

	FN_ENTER;
	acxlog(L_STATE, "%s: UNVERIFIED.\n", __func__);

	if ((tx_desc = acx100_get_tx_desc(priv)) == NULL) {
		FN_EXIT(1, 1);
		return 1;
	}

	hdesc_header = tx_desc->host_desc;
	hdesc_payload = tx_desc->host_desc + 1;

	hd = (TxData *)hdesc_header->data;
	payload = (struct auth_frame_body *)hdesc_payload->data;

	hd->frame_control = WLAN_SET_FC_FSTYPE(WLAN_FSTYPE_AUTHEN);	/* 0xb0 */
	hd->duration_id = arg_0->hdr->a3.dur;
	hd->sequence_control = arg_0->hdr->a3.seq;

	MAC_COPY(hd->da, arg_0->hdr->a3.a2);
	/* FIXME: huh? why was there no "sa"? Added, assume should do like authen2 */
	MAC_COPY(hd->sa, arg_0->hdr->a3.a1);
	MAC_COPY(hd->bssid, arg_0->hdr->a3.a3);

	hdesc_header->length = WLAN_HDR_A3_LEN;
	hdesc_header->data_offset = 0;

	payload->auth_alg = *(arg_0->auth_alg);
	payload->auth_seq = 4;
	payload->status = 0;

	hdesc_payload->length = 6;
	hdesc_payload->data_offset = 0;

	tx_desc->total_length = hdesc_payload->length + hdesc_header->length;

	acx100_dma_tx_data(priv, tx_desc);
	FN_EXIT(1, 0);
	return 0;
}

/*----------------------------------------------------------------
* acx100_transmit_assoc_req
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

/* acx100_transmit_assoc_req()
 * STATUS: almost ok, but UNVERIFIED.
 */
int acx100_transmit_assoc_req(wlandevice_t *priv)
{
	UINT32 packet_len;
	struct txdescriptor *tx_desc;
	struct txhostdescriptor *header;
	struct txhostdescriptor *payload;
	TxData *hd;
	UINT8 *pCurrPos;

	FN_ENTER;
	acxlog(L_STATE, "%s: UNVERIFIED.\n", __func__);

	acxlog(L_BINSTD | L_ASSOC, "Sending association request, awaiting response! NOT ASSOCIATED YET.\n");
	if ((tx_desc = acx100_get_tx_desc(priv)) == NULL) {
		FN_EXIT(1, 1);
		return 1;
	}

	packet_len = WLAN_HDR_A3_LEN;

	header = tx_desc->host_desc;      /* hostdescriptor for header */
	payload = tx_desc->host_desc + 1; /* hostdescriptor for payload */

	hd = (TxData *)header->data;
	pCurrPos = (UINT8 *)payload->data;

	hd->frame_control = WLAN_SET_FC_FSTYPE(WLAN_FSTYPE_ASSOCREQ);  /* 0x00 */;
	hd->duration_id = host2ieee16(0x8000);

	MAC_COPY(hd->da, priv->bssid);
	MAC_COPY(hd->sa, priv->dev_addr);
	MAC_COPY(hd->bssid, priv->bssid);

	hd->sequence_control = 0;

	header->length = WLAN_HDR_A3_LEN;
	header->data_offset = 0;

	/* now start filling the AssocReq frame body */
#if BROKEN
	*(UINT16 *)pCurrPos = host2ieee16(priv->capabilities & ~(WLAN_SET_MGMT_CAP_INFO_IBSS(1)));
#else
	/* FIXME: is it correct that we have to manually patc^H^H^H^Hadjust the
	 * Capabilities like that?
	 * I'd venture that priv->capabilities
	 * (acx100_update_capabilities()) should have set that
	 * beforehand maybe...
	 * Anyway, now Managed network association works properly
	 * without failing.
	 */
	/*
	*(UINT16 *)pCurrPos = host2ieee16((priv->capabilities & ~(WLAN_SET_MGMT_CAP_INFO_IBSS(1))) | WLAN_SET_MGMT_CAP_INFO_ESS(1));
	*/
	*(UINT16 *)pCurrPos = host2ieee16(WLAN_SET_MGMT_CAP_INFO_ESS(1));
	if ((UINT8)0 != priv->wep_restricted)
		*(UINT16 *)pCurrPos |= host2ieee16(WLAN_SET_MGMT_CAP_INFO_PRIVACY(1));
	/* only ask for short preamble if the peer station supports it */
	if (priv->station_assoc.caps & IEEE802_11_MGMT_CAP_SHORT_PRE)
		*(UINT16 *)pCurrPos |= host2ieee16(WLAN_SET_MGMT_CAP_INFO_SHORT(1));
	/* only ask for PBCC support if the peer station supports it */
	if (priv->station_assoc.caps & IEEE802_11_MGMT_CAP_PBCC)
		*(UINT16 *)pCurrPos |= host2ieee16(WLAN_SET_MGMT_CAP_INFO_PBCC(1));
#endif
	acxlog(L_ASSOC, "association: requesting capabilities 0x%04X\n", *(UINT16 *)pCurrPos);
	pCurrPos += 2;

	/* add listen interval */
	*(UINT16 *)pCurrPos = host2ieee16(priv->listen_interval);
	pCurrPos += 2;

	/* add ESSID */
	*(UINT8 *)pCurrPos = (UINT8)0; /* Element ID */
	pCurrPos += 1;
	*(UINT8 *)pCurrPos = (UINT8)strlen(priv->essid_for_assoc); /* Length */
	memcpy(&pCurrPos[1], priv->essid_for_assoc, pCurrPos[0]);
	pCurrPos += 1 + pCurrPos[0];

	/* add rates */
	*(UINT8 *)pCurrPos = (UINT8)1; /* Element ID */
	pCurrPos += 1;
	*(UINT8 *)pCurrPos = priv->rate_spt_len; /* Length */
	pCurrPos += 1;
	memcpy(pCurrPos, priv->rate_support1, priv->rate_spt_len);
	pCurrPos += priv->rate_spt_len;

	/* calculate lengths */
	packet_len += (int)pCurrPos - (int)payload->data;

	payload->length = packet_len - WLAN_HDR_A3_LEN;
	payload->data_offset = 0;

	tx_desc->total_length = payload->length + header->length;

	acx100_dma_tx_data(priv, tx_desc);
	FN_EXIT(1, 0);
	return 0;
}

/*----------------------------------------------------------------
* acx100_transmit_disassoc
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

/* acx100_transmit_disassoc()
 * STATUS: almost ok, but UNVERIFIED.
 */
// FIXME: type of clt is a guess
// I'm not sure if clt is needed.
UINT32 acx100_transmit_disassoc(client_t *clt, wlandevice_t *priv)
{
	struct disassoc_frame_body *payload;
	struct txdescriptor *tx_desc;
	struct txhostdescriptor *hdesc_header;
	struct txhostdescriptor *hdesc_payload;
	TxData *hd;

	FN_ENTER;
	acxlog(L_STATE, "%s: UNVERIFIED.\n", __func__);
//	if (clt != NULL) {
		if ((tx_desc = acx100_get_tx_desc(priv)) == NULL) {
			FN_EXIT(1, 1);
			return 1;
		}

		hdesc_header = tx_desc->host_desc;
		hdesc_payload = tx_desc->host_desc + 1;

		hd = (TxData *)hdesc_header->data;
		payload = (struct disassoc_frame_body *)hdesc_payload->data;

//		clt->used = 2;

		hd->frame_control = WLAN_SET_FC_FSTYPE(WLAN_FSTYPE_DISASSOC);	/* 0xa0 */
		hd->duration_id = 0;
		MAC_COPY(hd->da, priv->bssid);
		MAC_COPY(hd->sa, priv->dev_addr);
		MAC_COPY(hd->bssid, priv->dev_addr);
		hd->sequence_control = 0;

		hdesc_header->length = WLAN_HDR_A3_LEN;
		hdesc_header->data_offset = 0;

		payload->reason = 7;	/* "Class 3 frame received from nonassociated station." */

		hdesc_payload->length = priv->rate_spt_len + 8;
		hdesc_payload->data_offset = 0;

		tx_desc->total_length = hdesc_payload->length + hdesc_header->length;

		/* FIXME: lengths missing! */
		acx100_dma_tx_data(priv, tx_desc);
		FN_EXIT(1, 1);
		return 1;
//	}
	FN_EXIT(1, 0);
	return 0;
}

/*----------------------------------------------------------------
* acx100_gen_challenge
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

/* acx100_gen_challenge()
 * STATUS: FINISHED.
 */
void acx100_gen_challenge(challenge_text_t * d)
{
	FN_ENTER;
	d->element_ID = (UINT8)0x10;
	d->length = (UINT8)0x80;
	acx100_get_random(d->text, 0x80);
	FN_EXIT(0, 0);
}

/*----------------------------------------------------------------
* acx100_get_random
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

/* acx100_get_random()
 * STATUS: UNVERIFIED. (not binary compatible yet)
 */
void acx100_get_random(UINT8 *s, UINT16 stack)
{
	UINT8 var_8[4];
	UINT8 seed[4];
	UINT32 count1;
	UINT32 count2;

	UINT16 count, len;
	UINT32 ran = 0;

	FN_ENTER;
	acxlog(L_STATE, "%s: UNVERIFIED.\n", __func__);
	seed[0] = (UINT8)0;
	seed[1] = (UINT8)0;
	seed[2] = (UINT8)0;
	seed[3] = (UINT8)0;

	/* FIXME: What is he doing here??? */
	ran = 10000;
	for (count1 = 0; count1 < sizeof(seed); count1++) {
		var_8[count1] = (UINT8)((0x03ff6010 / ran) & 0xff);
		ran = (ran * 0xCCCCCCCD) >> 3;
	}

	/* FIXME: Mmmm, strange ... is it really meant to just take some random part of the stack ?? */
	len = strlen(var_8);

	/* generate a seed */
	if (0 != len) {
		for (count2 = 0; count2 < len; count2++) {
			seed[count2 & 3] ^= var_8[count2];
		}

		ran |= (seed[0]);
		ran |= (seed[1] << 8);
		ran |= (seed[2] << 16);
		ran |= (seed[3] << 24);

		/* generate some random numbers */
		for (count = 0; count < stack; count++) {
			/* this is a standard random number generator
			   using "magic" numbers */
			ran = (214013 * ran + 2531011);
			s[count] = (UINT8)((ran >> 16) & 0xff);
		}
	}
	FN_EXIT(0, 0);
}

/*----------------------------------------------------------------
* acx100_ibssid_gen
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

/* acx100_ibssid_gen()
 * This generates a random BSSID to be used for ad-hoc networks
 * (in infrastructure BSS networks, the BSSID is the MAC of the access
 * point)
 * STATUS: should be ok.
 */
void acx100_ibssid_gen(wlandevice_t *priv, unsigned char *p_out)
{
	UINT8 jifmod;
	int i;
	UINT8 oct;

	FN_ENTER;
	for (i = 0; i < 6; i++) {
		/* store jiffies modulo 0xff */
		jifmod = (UINT8)(jiffies % 0xff);
		/* now XOR eax with this value */
		oct = priv->dev_addr[i] ^ jifmod;
		/* WLAN_LOG_NOTICE1("temp = %d\n", oct); */
		p_out[i] = oct;
	}

	p_out[0] = (p_out[0] & ~0x80) | 0x40;
	FN_EXIT(0, 0);
}

/*----------------------------------------------------------------
* acx100_complete_dot11_scan
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

/* acx100_complete_dot11_scan()
 * STATUS: FINISHED.
 */
void acx100_complete_dot11_scan(wlandevice_t *priv)
{
	UINT32 idx;
	UINT16 needed_cap;
	INT32 idx_found = -1;
	UINT32 found_station = 0;

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

		/* V3CHANGE: dbg msg in V1 only */
		acxlog(L_BINDEBUG | L_ASSOC,
		       "<Scan Table> %d: SSID=\"%s\",CH=%d,SIR=%d,SNR=%d\n",
		       (int) idx,
		       this_bss->essid,
		       (int) this_bss->channel,
		       (int) this_bss->sir,
		       (int) this_bss->snr);

		if (0 == acx100_is_mac_address_broadcast(priv->ap))
			if (0 == acx100_is_mac_address_equal(this_bss->bssid, priv->ap))
				continue;
		if (!(this_bss->caps & (IEEE802_11_MGMT_CAP_ESS | IEEE802_11_MGMT_CAP_IBSS)))
		{
			acxlog(L_ASSOC, "STRANGE: peer station has neither ESS (Managed) nor IBSS (Ad-Hoc) capability flag set: patching to assume Ad-Hoc!\n");
			this_bss->caps |= IEEE802_11_MGMT_CAP_IBSS;
		}
		acxlog(L_ASSOC, "peer_cap 0x%02x, needed_cap 0x%02x\n",
		       this_bss->caps, needed_cap);

		/* peer station doesn't support what we need? */
		if ((0 != needed_cap) && ((this_bss->caps & needed_cap) != needed_cap))
			continue; /* keep looking */

		if (!(priv->reg_dom_chanmask & (1 << (this_bss->channel - 1) ) ))
		{
			acxlog(L_STD|L_ASSOC, "WARNING: peer station %ld is using channel %d, which is outside the channel range of the regulatory domain the driver is currently configured for: couldn't join in case of matching settings, might want to adapt your config!\n", idx, this_bss->channel);
			continue; /* keep looking */
		}

		if (((UINT8)0 == priv->essid_active)
		 || (0 == memcmp(this_bss->essid, priv->essid, priv->essid_len)))
		{
			acxlog(L_ASSOC,
			       "ESSID matches: \"%s\" (station), \"%s\" (config)\n",
			       this_bss->essid,
			       (priv->essid_active) ? priv->essid : "[any]");
			idx_found = idx;
			found_station = 1;
			acxlog(L_ASSOC, "matching station found!!\n");

			/* stop searching if this station is
			 * on the current channel, otherwise
			 * keep looking for an even better match */
			if (this_bss->channel == priv->channel)
				break;
		}
		else
		if (('\0' == this_bss->essid[0])
		|| ((1 == strlen(this_bss->essid)) && ((UINT8)' ' == this_bss->essid[0]))) {
			/* hmm, station with empty or single-space SSID:
			 * using hidden SSID broadcast?
			 */
			idx_found = idx;
			found_station = 1;
			acxlog(L_ASSOC, "found station with empty or single-space (hidden?) SSID, considering for association attempt.\n");
			/* ...and keep looking for better matches */
		}
		else {
		    acxlog(L_ASSOC, "ESSID doesn't match: \"%s\" (station), \"%s\" (config)\n",
			    this_bss->essid, (priv->essid_active) ? priv->essid : "[any]");
		}
	}
	if (0 != found_station) {
		UINT8 *a;
		char *essid_src;
		size_t essid_len;

		memcpy(&priv->station_assoc, &priv->bss_table[idx_found], sizeof(struct bss_info));
		if (priv->station_assoc.essid[0] == (UINT8)'\0') {
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
		
		acx100_update_capabilities(priv);

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
		       "%s: matching station FOUND (idx %ld), JOINING (%02X %02X %02X %02X %02X %02X).\n",
		       __func__, idx_found, a[0], a[1], a[2], a[3], a[4], a[5]);
		acx100_join_bssid(priv);

		if ((UINT8)2 == priv->preamble_mode)
			/* if Auto mode, then use Preamble setting which
			 * the station supports */
			priv->preamble_flag = (UINT8)((priv->station_assoc.caps & IEEE802_11_MGMT_CAP_SHORT_PRE) == IEEE802_11_MGMT_CAP_SHORT_PRE);

		if (ACX_MODE_0_IBSS_ADHOC != priv->macmode_chosen) {
			acx100_transmit_authen1(priv);
			acx100_set_status(priv, ISTATUS_2_WAIT_AUTH);
		} else {
			acx100_set_status(priv, ISTATUS_4_ASSOCIATED);
		}
	} else {		/* uh oh, no station found in range */
		if ((ACX_MODE_0_IBSS_ADHOC == priv->macmode_wanted)
		 || (ACX_MODE_FF_AUTO == priv->macmode_wanted)) { /* phew, we're safe: we intended to use Ad-Hoc mode */
			acxlog(L_STD | L_ASSOC,
			       "%s: no matching station found in range, CANNOT JOIN: generating our own IBSSID instead.\n",
			       __func__);
			acx100_ibssid_gen(priv, priv->address);
			acx100_update_capabilities(priv);
			priv->macmode_chosen = ACX_MODE_0_IBSS_ADHOC;
			acx100_join_bssid(priv);
			acx100_set_status(priv, ISTATUS_4_ASSOCIATED);
		} else {
			acxlog(L_STD | L_ASSOC,
			       "%s: no matching station found in range and not in Ad-Hoc or Auto mode --> giving up scanning.\n",
			       __func__);
			acx100_set_status(priv, ISTATUS_0_STARTED);
		}
	}
	FN_EXIT(0, 0);
}

#if (POWER_SAVE_80211 == 0)
/*----------------------------------------------------------------
* ActivatePowerSaveMode
* FIXME: rename to acx100_activate_power_save_mode
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
void ActivatePowerSaveMode(wlandevice_t *priv, /*@unused@*/ int vala)
{
       acx100_powermgmt_t pm;

       FN_ENTER;
       acxlog(L_STATE, "%s: UNVERIFIED.\n", __func__);

       acx100_interrogate(priv, &pm, ACX100_RID_POWER_MGMT);
       if (pm.wakeup_cfg != (UINT8)0x81) {
               FN_EXIT(0, 0);
               return;
       }
       pm.wakeup_cfg = (UINT8)0;
       pm.options = (UINT8)0;
       pm.hangover_period = (UINT8)0;
       acx100_configure(priv, &pm, ACX100_RID_POWER_MGMT);
       FN_EXIT(0, 0);
}
#endif

/*------------------------------------------------------------------------------
 * acx100_timer
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
void acx100_timer(unsigned long address)
{
	wlandevice_t *priv = (wlandevice_t *)address;
	unsigned long flags;
#if (WLAN_HOSTIF==WLAN_USB)
	int status;
#endif

	FN_ENTER;
	acxlog(L_STATE, "%s: UNVERIFIED.\n", __func__);
	acxlog(L_BINDEBUG | L_ASSOC, "%s: status = %d\n", __func__,
	       priv->status);
	if (0 != acx100_lock(priv, &flags)) {
		return;
	}
	acxlog(L_STD,"netdev queue status: %s\n",netif_queue_stopped(priv->netdev)?"stopped":"alive");

	switch (priv->status) {
	case ISTATUS_1_SCANNING:
		if ((++priv->scan_retries < 5) && (0 == priv->bss_table_count)) {
			acx100_set_timer(priv, 1000000);
#if 0
			acx100_interrogate(priv,&status,ACX100_RID_SCAN_STATUS);
			acxlog(L_STD,"scan status=%d\n",status);
			if (status==0) {
				acx100_complete_dot11_scan(priv);
			}
#endif
			acxlog(L_ASSOC, "continuing scan (attempt %d).\n", priv->scan_retries);
		}
		else
		{
			acxlog(L_ASSOC, "Stopping scan (%s).\n", (0 != priv->bss_table_count) ? "stations found" : "scan timeout");
			acx100_issue_cmd(priv, ACX100_CMD_STOP_SCAN, NULL, 0, 5000);
#if (WLAN_HOSTIF==WLAN_USB)
			acx100_complete_dot11_scan(priv);
#endif
		}
		break;
	case ISTATUS_2_WAIT_AUTH:
		priv->scan_retries = 0;
		if (++priv->auth_assoc_retries < 10) {
			acxlog(L_ASSOC, "resend authen1 request (attempt %d).\n",
			       priv->auth_assoc_retries + 1);
			acx100_transmit_authen1(priv);
		} else {
			/* time exceeded: fall back to scanning mode */
			acxlog(L_ASSOC,
			       "authen1 request reply timeout, giving up.\n");
			/* simply set status back to scanning (DON'T start scan) */
			acx100_set_status(priv, ISTATUS_1_SCANNING);
		}
		acx100_set_timer(priv, 2500000); /* used to be 1500000, but some other driver uses 2.5s wait time  */
    break;
	case ISTATUS_3_AUTHENTICATED:
		if (++priv->auth_assoc_retries < 10) {
			acxlog(L_ASSOC,
			       "resend association request (attempt %d).\n",
			       priv->auth_assoc_retries + 1);
			acx100_transmit_assoc_req(priv);
		} else {
			/* time exceeded: give up */
			acxlog(L_ASSOC,
			       "association request reply timeout, giving up.\n");
			/* simply set status back to scanning (DON'T start scan) */
			acx100_set_status(priv, ISTATUS_1_SCANNING);
		}
		acx100_set_timer(priv, 2500000); /* see above */
		break;
	case ISTATUS_5_UNKNOWN:
		acx100_set_status(priv, priv->unknown0x2350);
		priv->unknown0x2350 = 0;
		break;
	case ISTATUS_0_STARTED:
	case ISTATUS_4_ASSOCIATED:
	default:
		break;
	}
	acx100_unlock(priv, &flags);
	FN_EXIT(0, 0);
}
