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

#include <linux/config.h>
#include <linux/version.h>

#include <linux/module.h>
#include <linux/kernel.h>

#include <linux/sched.h>
#include <linux/types.h>
#include <linux/skbuff.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/if_arp.h>
#include <linux/rtnetlink.h>
#include <linux/wireless.h>
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
#include <linux/pci.h>

#include <linux/dcache.h>
#include <linux/highmem.h>
#include <linux/sched.h>
#include <linux/skbuff.h>
#include <linux/etherdevice.h>


/*================================================================*/
/* Project Includes */

#include <version.h>
#include <p80211hdr.h>
#include <p80211mgmt.h>
#include <acx100_conv.h>
#include <p80211ioctl.h>
#include <acx100.h>
#include <p80211netdev.h>
#include <p80211req.h>
#include <p80211types.h>
#include <acx100_helper.h>
#include <idma.h>
#include <acx100_helper2.h>
#include <ihw.h>
#include <acx80211frm.h>

static UINT32 process_mgmt_frame(struct rxhostdescriptor *skb,
		                /* wlan_pb_t * a,*/ wlandevice_t *
				wlandev);
static int process_data_frame_master(struct rxhostdescriptor *skb,
		                /* wlan_pb_t * a,*/ wlandevice_t *
				wlandev);
static int process_data_frame_client(struct rxhostdescriptor *skb,
		                /* wlan_pb_t * a,*/ wlandevice_t *
				wlandev);
static int process_NULL_frame(struct rxhostdescriptor * a, 
		                wlandevice_t * hw, int vala);


static client_t sta_list[32];

static client_t *sta_hash_tab[0x40];

UINT8 *list2[0x4];

UINT16 sendmod;

alloc_p80211_mgmt_req_t alloc_p80211mgmt_req;

/*----------------------------------------------------------------
* acx_client_sta_list_init
* FIXME: change name to acx100_client_sta_list_init
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

/* acx_client_sta_list_init()
 * STATUS: should be ok..
 */
void acx_client_sta_list_init(void)
{
	int i;

	FN_ENTER;
	for (i = 0; i <= 63; i++) {
		sta_hash_tab[i] = NULL;
	}
	memset(sta_list, 0, sizeof(sta_list));
	FN_EXIT(0, 0);
}

/*----------------------------------------------------------------
* sta_list_add
* FIXME: change to acx100_sta_list_add
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

/* sta_list_add()
 * STATUS: FINISHED.
 */
client_t *sta_list_add(UINT8 * address)
{
	client_t *client;
	int index;

	FN_ENTER;
	client = sta_list_alloc(address);
	if (!client)
		goto done;

	/* computing hash table index */
	index = ((address[4] << 8) + address[5]);
	index -= index & 0x3ffc0;

	client->next = sta_hash_tab[index];
	sta_hash_tab[index] = client;

	acxlog(L_BINSTD | L_ASSOC,
	       "<sta_list_add> sta = %02X:%02X:%02X:%02X:%02X:%02X\n",
	       address[0], address[1], address[2], address[3], address[4],
	       address[5]);

      done:
	FN_EXIT(1, (int) client);
	return client;
}

/*----------------------------------------------------------------
* acx_get_client_from_sta_hash_tab
* FIXME: change name to acx100_get_client_from_hash_tab
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


static inline client_t *acx_get_client_from_sta_hash_tab(UINT8 * address)
{
	int index;

	FN_ENTER;
	/* computing hash table index */
	index = ((address[4] << 8) + address[5]);
	index -= index & 0x3ffc0;

	FN_EXIT(0, 0);
	return sta_hash_tab[index];
}

/*----------------------------------------------------------------
* get_sta_list
* FIXME: change name to acx100_get_sta_list()
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

/* get_sta_list()
 * STATUS: FINISHED.
 */
client_t *get_sta_list(char *address)
{
	client_t *client;
	client_t *result = NULL;	/* can be removed if tracing unneeded */

	FN_ENTER;
	client = acx_get_client_from_sta_hash_tab(address);

	for (; client; client = client->next) {
		if (!memcmp(address, client->address, WLAN_ADDR_LEN)) {
			result = client;
			goto done;
		}
	}

      done:
	FN_EXIT(1, (int) result);
	return result;
}

/*----------------------------------------------------------------
* sta_list_alloc
* FIXME: change name to acx100_sta_list_alloc
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

/* sta_list_alloc()
 * STATUS: FINISHED, except for struct defs.
 * Hmm, does this function have one "silent" parameter or 0 parameters?
 * Doesn't matter much anyway...
 */
client_t *sta_list_alloc(UINT8 *address)
{
	int i = 0;

	FN_ENTER;
	for (i = 0; i <= 31; i++) {
		if (sta_list[i].used == 0) {
			sta_list[i].used = 1;
			sta_list[i].auth_alg = WLAN_AUTH_ALG_SHAREDKEY;
			sta_list[i].val0xe = 1;
			return &(sta_list[i]);
		}
	}
	FN_EXIT(0, 0);
	return 0;
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
* 2 probably means: not authenticated yet (process_deauthenticate())
* 3 probably means: authenticated, but not associated yet (process_disassociate(), process_authen())
* 4 probably means: associated (process_assocresp(), process_reassocresp())
* 5 means: status unknown
*----------------------------------------------------------------*/

void acx100_set_status(wlandevice_t *hw, int status)
{
	char *state_str[6] = { "STARTED", "SCANNING", "WAIT_AUTH", "AUTHENTICATED", "ASSOCIATED", "UNKNOWN" };
	char *stat;
#if QUEUE_OPEN_AFTER_ASSOC
	static int associated = 0;
#endif

	FN_ENTER;
	if (status <= ISTATUS_5_UNKNOWN)
		stat = state_str[status];
	else
		stat = "INVALID??";

	acxlog(L_BINDEBUG | L_ASSOC, "%s: Setting iStatus = %d (%s)\n",
	       __func__, status, stat);

	if (hw->unknown0x2350 == ISTATUS_5_UNKNOWN) {
		hw->unknown0x2350 = hw->iStatus;
		hw->iStatus = ISTATUS_5_UNKNOWN;
	} else {
		hw->iStatus = status;
	}
	if ((hw->iStatus == ISTATUS_1_SCANNING)
	    || (hw->iStatus == ISTATUS_5_UNKNOWN)) {
		hw->scan_retries = 0;
		acx100_set_timer(hw, 3000000);
	} else if (hw->iStatus <= ISTATUS_3_AUTHENTICATED) {
		hw->auth_assoc_retries = 0;
		acx100_set_timer(hw, 1500000);
	}

#if QUEUE_OPEN_AFTER_ASSOC
	if (status == ISTATUS_4_ASSOCIATED)
	{
		if (associated == 0)
		{
			/* ah, we're newly associated now,
			 * so let's restart the net queue */
			acxlog(L_XFER, "wake queue after association.\n");
			netif_wake_queue(hw->netdev);
		}
		associated = 1;
	}
	else
	{
		/* not associated any more, so let's stop the net queue */
		if (associated == 1)
		{
			acxlog(L_XFER, "stop queue after losing association.\n");
			netif_stop_queue(hw->netdev);
		}
		associated = 0;
	}
#endif
	FN_EXIT(0, 0);
}

/*----------------------------------------------------------------
* acx80211_rx
* FIXME: change name to acx100_rx_frame
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

/* acx80211_rx()
 * STATUS: FINISHED, UNVERIFIED.
 */
int acx80211_rx(struct rxhostdescriptor *rxdesc, wlandevice_t * hw)
{
	UINT16 ftype;
	UINT fstype;
	p80211_hdr_t *p80211_hdr;
	int result = 0;

	FN_ENTER;
	p80211_hdr = (p80211_hdr_t*)&rxdesc->data->buf ;

	if (hw->rx_config_1 & RX_CFG1_INCLUDE_ADDIT_HDR) {
		p80211_hdr = (p80211_hdr_t *)((UINT8 *)p80211_hdr + 4);
	}
//	printk("Rx_CONFIG_1 = %X\n",hw->rx_config_1 & RX_CFG1_INCLUDE_ADDIT_HDR);

	/* see IEEE 802.11-1999.pdf chapter 7 "MAC frame formats" */
	ftype = WLAN_GET_FC_FTYPE(p80211_hdr->a3.fc);
	fstype = WLAN_GET_FC_FSTYPE(p80211_hdr->a3.fc);

	switch (ftype) {
		case WLAN_FTYPE_MGMT:
			result = process_mgmt_frame(rxdesc, hw);
			break;
		case WLAN_FTYPE_CTL:
			if (fstype != WLAN_FSTYPE_PSPOLL)
				result = 0;
			else
				result = 1;
			/*   this call is irrelevant, since
			 *   process_class_frame is a stub, so return
			 *   immediately instead.
			 * return process_class_frame(pb, hw, 3); */
			break;
		case WLAN_FTYPE_DATA:
			/* binary driver did ftype-1 to appease jump
			 * table layout */
			if (fstype == WLAN_FSTYPE_DATAONLY) 
			{
				if (hw->macmode == WLAN_MACMODE_ESS_AP /* 3 */ ) {
					result = process_data_frame_master(rxdesc, hw);
				} else if (hw->iStatus == ISTATUS_4_ASSOCIATED) {
					result = process_data_frame_client(rxdesc, hw); 				}
			} else switch (ftype) {
				case WLAN_FSTYPE_DATA_CFACK:
				case WLAN_FSTYPE_DATA_CFPOLL:
				case WLAN_FSTYPE_DATA_CFACK_CFPOLL:
				case WLAN_FSTYPE_CFPOLL:
				case WLAN_FSTYPE_CFACK_CFPOLL:
				/*   see above.
				   process_class_frame(rxdesc, hw, 3); */
/* FIXME: is this a binary driver code flaw, the breaks are not present!?!?
 * indeed, this might be the case. */
				case WLAN_FSTYPE_NULL:
					process_NULL_frame(rxdesc, hw, 3);
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

UINT16 CurrentAID = 1;
/*----------------------------------------------------------------
* transmit_assocresp
* FIXME: change name to acx100_transmit_assocresp
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

/* transmit_assocresp()
 * STATUS: should be ok, but UNVERIFIED.
 */
UINT32 transmit_assocresp(wlan_fr_assocreq_t *arg_0,
			  wlandevice_t *hw)
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

	acxlog(L_BINDEBUG | L_ASSOC | L_XFER, "<transmit_assocresp 1>\n");

	// FIXME: is this correct, should it just test for toDS and fromDS
	if (arg_0->hdr->a3.fc == 0x100 && arg_0->hdr->a3.fc == 0x200) {
		sa = arg_0->hdr->a3.a1;
		da = arg_0->hdr->a3.a2;
		bssid = arg_0->hdr->a3.a3;
	} else {
		FN_EXIT(0, 0);
		return 1;
	}

	clt = get_sta_list(da);

	if (clt != NULL) {
		if (clt->used == 1) {
			transmit_deauthen(da, clt, hw, WLAN_MGMT_REASON_CLASS2_NONAUTH /* 6 */);
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

			if (arg_0->ssid->len <= 5) {
				clt->val0x9a = var_1c[arg_0->ssid->len];
			} else {
				clt->val0x9a = 0x1f;
			}

			if ((tx_desc = acx100_get_tx_desc(hw)) == NULL) {
				return 0;
			}

			hdesc_header = tx_desc->host_desc;
			hdesc_payload = tx_desc->host_desc + 1;

			hd = (TxData *)hdesc_header->data;
			payload = (struct assocresp_frame_body *)hdesc_payload->data;

			hd->frame_control = host2ieee16(WLAN_SET_FC_FSTYPE(WLAN_FSTYPE_ASSOCRESP));	/* 0x10 */
			hd->duration_id = arg_0->hdr->a3.dur;

			memcpy(hd->da, da, WLAN_ADDR_LEN);
			memcpy(hd->sa, sa, WLAN_ADDR_LEN);
			memcpy(hd->bssid, bssid, WLAN_BSSID_LEN);

			hd->sequence_control = arg_0->hdr->a3.seq;

			hdesc_header->length = WLAN_HDR_A3_LEN;
			hdesc_header->val0x4 = 0;

			payload->cap_info = hw->capabilities;
			payload->status = 0;
			payload->aid = clt->aid;

			payload->rates.element_ID = 1;
			payload->rates.length = hw->rate_spt_len;
			payload->rates.sup_rates[0] = 0x82; /* 1 Mbit */
			payload->rates.sup_rates[1] = 0x84; /* 2 Mbit */
			payload->rates.sup_rates[2] = 0x8b; /* 5.5 Mbit */
			payload->rates.sup_rates[3] = 0x96; /* 11 Mbit */
			payload->rates.sup_rates[4] = 0xac; /* 22 Mbit */

			hdesc_payload->length = hw->rate_spt_len + 8;
			hdesc_payload->val0x4 = 0;

			tx_desc->total_length = hdesc_payload->length + hdesc_header->length;

			acx100_dma_tx_data(hw, tx_desc);
		}
	}

	FN_EXIT(0, 0);
	return 1;
}

/*----------------------------------------------------------------
* transmit_reassocresp
* FIXME: rename to acx100_transmit_reassocresp
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

/* transmit_reassocresp()
 * STATUS: should be ok, but UNVERIFIED.
 */
UINT32 transmit_reassocresp(wlan_fr_reassocreq_t *arg_0, wlandevice_t *hw)
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
	if ((arg_0->hdr->a3.fc == 0x100) && (arg_0->hdr->a3.fc == 0x200)) {
		sa = arg_0->hdr->a3.a1;
		da = arg_0->hdr->a3.a2;
		bssid = arg_0->hdr->a3.a3;
	} else {
		FN_EXIT(0, 0);
		return 1;
	}

	clt = get_sta_list(da);
	if (clt != NULL) {
		if (clt->used == 1)
			clt->used = 2;
	} else {
		clt = sta_list_add(da);
		memcpy(clt->address, da, WLAN_ADDR_LEN);
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

		if ((tx_desc = acx100_get_tx_desc(hw)) == NULL) {
			return 0;
		}
		hdesc_header = tx_desc->host_desc;
		hdesc_payload = tx_desc->host_desc + 1;
		fr = (TxData*)hdesc_header->data;
		payload = (struct reassocresp_frame_body *)hdesc_payload->data;
		fr->frame_control = WLAN_SET_FC_FSTYPE(WLAN_FSTYPE_REASSOCRESP);	/* 0x30 */
		fr->duration_id = arg_0->hdr->a3.dur;

		memcpy(fr->da, da, WLAN_ADDR_LEN);
		memcpy(fr->sa, sa, WLAN_ADDR_LEN);
		memcpy(fr->bssid, bssid, WLAN_BSSID_LEN);

		fr->sequence_control = arg_0->hdr->a3.seq;

		hdesc_header->length = WLAN_HDR_A3_LEN;
		hdesc_header->val0x4 = 0;

		payload->cap_info = hw->capabilities;
		payload->status = 0;
		payload->aid = clt->aid;

		payload->rates.element_ID = 1;
		payload->rates.length = hw->rate_spt_len;
		payload->rates.sup_rates[0] = 0x82; /* 1 Mbit */
		payload->rates.sup_rates[1] = 0x84; /* 2 Mbit */
		payload->rates.sup_rates[2] = 0x8b; /* 5.5 Mbit */
		payload->rates.sup_rates[3] = 0x96; /* 11 Mbit */
		payload->rates.sup_rates[4] = 0xac; /* 22 Mbit */

		hdesc_payload->val0x4 = 0;
		hdesc_payload->length = hw->rate_spt_len + 8;

		tx_desc->total_length = hdesc_payload->length + hdesc_header->length;

		acx100_dma_tx_data(hw, tx_desc);
	}
	FN_EXIT(0, 0);

	return 0;
}

/*----------------------------------------------------------------
* process_disassoc
* FIXME: rename to acx100_process_disassoc
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

/* process_disassoc()
 * STATUS: UNVERIFIED.
 */
int process_disassoc(wlan_fr_disassoc_t *arg_0, wlandevice_t *hw)
{
	p80211_hdr_t *hdr;
	int res = 0;
	UINT8 *TA = NULL;
	client_t *clts;

	FN_ENTER;
	acxlog(L_STATE, "%s: UNVERIFIED.\n", __func__);
	hdr = arg_0->hdr;

	// FIXME: toDS and fromDS = 1 ??
	if (hdr->a4.fc & 0x300)
		res = 1;
	else
		TA = hdr->a3.a2;

	if (!res) {
		if ((clts = get_sta_list(TA)) != NULL) {
			if (clts->used == 1) {
				acxlog(L_BINDEBUG | L_ASSOC | L_XFER,
				       "<transmit_deauth 2>\n");
				transmit_deauthen(TA, clts, hw, WLAN_MGMT_REASON_CLASS2_NONAUTH /* 6 */);
			} else
				clts->used = 2;
		} else
			res = 1;
	}
	FN_EXIT(0, 0);
	return res;
}

/*----------------------------------------------------------------
* process_disassociate
* FIXME: rename to acx100_process_disassociate
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

/* process_disassociate()
 * STATUS: UNVERIFIED.
 */
int process_disassociate(wlan_fr_disassoc_t * req, wlandevice_t * hw)
{
	p80211_hdr_t *hdr;
	int res = 0;

	FN_ENTER;
	acxlog(L_STATE, "%s: UNVERIFIED.\n", __func__);
	hdr = req->hdr;

	// FIXME: toDS and fromDS = 1 ??
	if (hdr->a3.fc & 0x300)
		res = 1;
	else {
		if (hw->macmode == WLAN_MACMODE_NONE /* 0 */ )
			res = 1;
		else if (acx100_is_mac_address_equal(hw->dev_addr, hdr->a3.a1 /* RA */)) {
			res = 1;
			if (hw->iStatus > ISTATUS_3_AUTHENTICATED) {
				/* hw->val0x240 = req->reason[0]; Unused, so removed */
				acx100_set_status(hw, ISTATUS_3_AUTHENTICATED);
				ActivatePowerSaveMode(hw, 2);
			}
			res = 0;
		} else
			res = 1;
	}
	FN_EXIT(0, 0);
	return res;
}

/*----------------------------------------------------------------
* is_broadcast_address
* FIXME: rename to acx100_is_broadcast_address
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

int is_broadcast_address(UINT8 *address)
{
	UINT8 bcast_addr[WLAN_ADDR_LEN] =
		{ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

	if (memcmp(address, bcast_addr, WLAN_ADDR_LEN) == 0)
		return 1;


	/* IPv6 broadcast address */
	if ((address[0] == 0x33) && (address[1] == 0x33))
		return 1;

	return 0;
}

/*----------------------------------------------------------------
* process_data_frame_master
* FIXME: rename to acx100_process_data_frame_master
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

static int process_data_frame_master(struct rxhostdescriptor *rxdesc, wlandevice_t *hw)
{
	client_t *clt;
	UINT8 bcast_addr[WLAN_ADDR_LEN] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
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
	p80211_hdr = (p80211_hdr_t*)&rxdesc->data->buf;
	if (hw->rx_config_1 & RX_CFG1_INCLUDE_ADDIT_HDR) {
		p80211_hdr = (p80211_hdr_t*)((UINT8*)p80211_hdr + 4);
	}

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
		hw->stats.rx_errors++;
		goto done;
	}

	/* check if it is our bssid, if not, leave */
	if (memcmp(bssid, hw->bssid, WLAN_BSSID_LEN) == 0) {
		if (!(clt = get_sta_list(bcast_addr)) || (clt->used != 3)) {
			transmit_deauthen(bcast_addr, 0, hw, WLAN_MGMT_REASON_RSVD /* 0 */);
			acxlog(L_STD, "frame error #2??\n");
			hw->stats.rx_errors++;
			goto fail;
		} else {
			esi = 2;
			/* check if the da is not broadcast */
			if (!is_broadcast_address(da)) {
				if ((signed char) da[0x0] >= 0) {
					esi = 0;
					if (!(var_24 = get_sta_list(da))) {
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
					acx100_rx(rxdesc, hw);
					result = 0;
					goto fail;
				}
			}
			if ((esi == 0) || (esi == 2)) {
				/* repackage, tx, and hope it someday reaches its destination */
				memcpy(p80211_hdr->a3.a1, da,
				       WLAN_ADDR_LEN);
				memcpy(p80211_hdr->a3.a2, bssid,
				       WLAN_ADDR_LEN);
				memcpy(p80211_hdr->a3.a3, sa,
				       WLAN_ADDR_LEN);
				/* To_DS = 0, From_DS = 1 */
				p80211_hdr->a3.fc =
				    WLAN_SET_FC_FROMDS(1) +
				    WLAN_SET_FC_FTYPE(WLAN_FTYPE_DATA);

				if ((tx_desc = acx100_get_tx_desc(hw)) == NULL) {
					return 0;
				}

				acx100_rxdesc_to_txdesc(rxdesc, tx_desc);
				acx100_dma_tx_data(hw, tx_desc);

				if (esi != 2) {
					goto done;
				}
			} else {
				if (4 == sendmod) {
					goto done;
				}
			}
			acx100_rx(rxdesc, hw);
		}
	}
done:
	result = 1;
fail:
	FN_EXIT(1, result);
	return result;
}
/*----------------------------------------------------------------
* process_data_frame_client
* FIXME: rename to acx100_process_data_frame_client
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

/* process_data_frame_client()
 * STATUS: FINISHED, UNVERIFIED.
 */
static int process_data_frame_client(struct rxhostdescriptor *rxdesc, wlandevice_t * hw)
{
	UINT8 bcast_addr[WLAN_ADDR_LEN] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
	UINT8 *da = NULL;
	UINT8 *bssid = NULL;
	p80211_hdr_t* p80211_hdr;
	int to_ds, from_ds;
	int result;

	FN_ENTER;
	acxlog(L_STATE, "%s: UNVERIFIED.\n", __func__);
	p80211_hdr = (p80211_hdr_t*)&rxdesc->data->buf;

	if (hw->rx_config_1 & RX_CFG1_INCLUDE_ADDIT_HDR) {
		p80211_hdr = (p80211_hdr_t*)((UINT8*)p80211_hdr + 4);
	}

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
		hw->stats.rx_errors++;
		goto done;
	}

	acxlog(L_DEBUG, "da ");
	acx100_log_mac_address(L_DEBUG, da);
	acxlog(L_DEBUG, ",bssid ");
	acx100_log_mac_address(L_DEBUG, bssid);
	acxlog(L_DEBUG, ",hw->bssid ");
	acx100_log_mac_address(L_DEBUG, hw->bssid);
	acxlog(L_DEBUG, ",dev_addr ");
	acx100_log_mac_address(L_DEBUG, hw->dev_addr);
	acxlog(L_DEBUG, ",bcast_addr ");
	acx100_log_mac_address(L_DEBUG, bcast_addr);
	acxlog(L_DEBUG, "\n");

	/* check if it is our bssid */
        if (!acx100_is_mac_address_equal(hw->bssid, bssid)) {
		/* is not our bssid, so bail out */
		goto done;
	}

	/* check if it is broadcast */
	if (!is_broadcast_address(da)) {
		if ((signed char) da[0] >= 0) {
			/* no broadcast, so check if it is our address */
                        if (!acx100_is_mac_address_equal(da, hw->dev_addr)) {
				/* its not, so bail out */
				goto done;
			}
		}
	}

	/* packet is from our bssid, and is either broadcast or destined for us, so process it */
	acx100_rx(rxdesc, hw);

done:
	result = 1;
	FN_EXIT(1, result);
	return result;
}

/*----------------------------------------------------------------
* process_mgmt_frame
* FIXME: rename to acx100_process_mgmt_frame
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

/* process_mgmt_frame()
 * STATUS: FINISHED, UNVERIFIED. namechange!! (from process_mgnt_frame())
 */
static UINT32 process_mgmt_frame(struct rxhostdescriptor * rxdesc, wlandevice_t * hw)
{
	static UINT8 reassoc_b;
	UINT8 *a;
	p80211_hdr_t *p80211_hdr = (p80211_hdr_t*)&rxdesc->data->buf;
	int wep_offset = 0;

	FN_ENTER;
	acxlog(L_STATE, "%s: UNVERIFIED.\n", __func__);
	if(WLAN_GET_FC_ISWEP(p80211_hdr->a3.fc)){
		wep_offset = 0x10;
	}

	if (hw->rx_config_1 & RX_CFG1_INCLUDE_ADDIT_HDR) {
		p80211_hdr = (p80211_hdr_t*)((UINT8*)p80211_hdr + 4);
	}

	switch (WLAN_GET_FC_FSTYPE(p80211_hdr->a3.fc)) {
	case WLAN_FSTYPE_ASSOCREQ /* 0x00 */ :
		if (hw->macmode != WLAN_MACMODE_ESS_STA /* 2 */ ) {
			memset(&alloc_p80211mgmt_req, 0, 8 * 4);
			alloc_p80211mgmt_req.a.assocreq.buf =
			    (UINT8 *) p80211_hdr;
			alloc_p80211mgmt_req.a.assocreq.len =
			    (rxdesc->data->status & 0xfff) - wep_offset;

			acx_mgmt_decode_assocreq(&alloc_p80211mgmt_req.a.
						 assocreq);

			if (!memcmp
			    (alloc_p80211mgmt_req.a.assocreq.hdr->a3.a2,
			     hw->address, WLAN_ADDR_LEN)) {
				transmit_assocresp(&alloc_p80211mgmt_req.a.
						   assocreq, hw);
			}
		}
		break;
	case WLAN_FSTYPE_ASSOCRESP /* 0x01 */ :
		if (hw->mode != 3) {
			memset(&alloc_p80211mgmt_req, 0, 8 * 4);
			alloc_p80211mgmt_req.a.assocresp.buf =
			    (UINT8 *) p80211_hdr;
			alloc_p80211mgmt_req.a.assocresp.len =
			    (rxdesc->data->status & 0xfff) - wep_offset;
			acx_mgmt_decode_assocresp(&alloc_p80211mgmt_req.a.
						  assocresp);
			process_assocresp(&alloc_p80211mgmt_req.a.
					  assocresp, hw);
		}
		break;
	case WLAN_FSTYPE_REASSOCREQ /* 0x02 */ :
		if (hw->macmode != WLAN_MACMODE_ESS_STA /* 2 */ ) {
			reassoc_b = 0;

			memset(&alloc_p80211mgmt_req.a.assocreq, 0, 9 * 4);
			alloc_p80211mgmt_req.a.assocreq.buf =
			    (UINT8 *) p80211_hdr;
			alloc_p80211mgmt_req.a.assocreq.len =
			    (rxdesc->data->status & 0xfff) - wep_offset;

			acx_mgmt_decode_assocreq(&alloc_p80211mgmt_req.a.
						 assocreq);

			//reassocreq and assocreq are equivalent
			transmit_reassocresp(&alloc_p80211mgmt_req.a.
					     reassocreq, hw);
		}
		break;
	case WLAN_FSTYPE_REASSOCRESP /* 0x03 */ :
		if (hw->mode != 3) {
			memset(&alloc_p80211mgmt_req.a.assocresp, 0,
			       8 * 4);
			alloc_p80211mgmt_req.a.assocresp.buf =
			    (UINT8 *) p80211_hdr;
			alloc_p80211mgmt_req.a.assocresp.len =
			    (rxdesc->data->status & 0xfff) - wep_offset;

			acx_mgmt_decode_assocresp(&alloc_p80211mgmt_req.a.
						  assocresp);
			process_reassocresp(&alloc_p80211mgmt_req.a.
					    reassocresp, hw);
		}
		break;
	case WLAN_FSTYPE_PROBEREQ /* 0x04 */ :
		// exit
		break;
	case WLAN_FSTYPE_PROBERESP /* 0x05 */ :
		if (hw->mode != 3) {

			memset(&alloc_p80211mgmt_req, 0, 0xd * 4);
			alloc_p80211mgmt_req.a.proberesp.buf =
			    (UINT8 *) p80211_hdr;
			alloc_p80211mgmt_req.a.proberesp.len =
			    (rxdesc->data->status & 0xfff) - wep_offset;
			acx_mgmt_decode_proberesp(&alloc_p80211mgmt_req.a.
						  proberesp);
			if (hw->iStatus == ISTATUS_1_SCANNING)
				acx100_process_probe_response(rxdesc->data,
						     hw,
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
		if (hw->mode != 3) {
			switch (hw->iStatus){
			   case ISTATUS_1_SCANNING:
			   case ISTATUS_5_UNKNOWN:
				memset(&alloc_p80211mgmt_req.a.beacon, 0,
				       0xe * 4);
				alloc_p80211mgmt_req.a.beacon.buf =
				    (char *) p80211_hdr;
				alloc_p80211mgmt_req.a.beacon.len =
				    (rxdesc->data->status & 0xfff) - wep_offset;
				acxlog(L_DATA, "fc: %X\n",
				       p80211_hdr->a3.fc);
				acxlog(L_DATA, "dur: %X\n",
				       p80211_hdr->a3.dur);
				a = p80211_hdr->a3.a1;
				acxlog(L_DATA,
				       "a1: %02X:%02X:%02X:%02X:%02X:%02X\n",
				       a[0], a[1], a[2], a[3], a[4], a[5]);
				a = p80211_hdr->a3.a2;
				acxlog(L_DATA,
				       "a2: %02X:%02X:%02X:%02X:%02X:%02X\n",
				       a[0], a[1], a[2], a[3], a[4], a[5]);
				a = p80211_hdr->a3.a3;
				acxlog(L_DATA,
				       "a3: %02X:%02X:%02X:%02X:%02X:%02X\n",
				       a[0], a[1], a[2], a[3], a[4], a[5]);
				acxlog(L_DATA, "seq: %X\n",
				       p80211_hdr->a3.seq);
				acx_mgmt_decode_beacon
				    (&alloc_p80211mgmt_req.a.beacon);
				acx100_process_probe_response(rxdesc->data,
						     hw,
						     (acxp80211_hdr_t *)
						     alloc_p80211mgmt_req.
						     a.beacon.hdr);
				break;
			default:
				/* acxlog(L_ASSOC | L_DEBUG,
				   "Incoming beacon message not handled during iStatus %i.\n",
				   hw->iStatus); */
			break;
			}
		} else {
			acxlog(L_DEBUG,
			       "Incoming beacon message not handled in mode %ld.\n",
			       hw->mode);
		}
		break;
	case WLAN_FSTYPE_ATIM /* 0x09 */ :
		// exit
		break;
	case WLAN_FSTYPE_DISASSOC /* 0x0a */ :
		memset(&alloc_p80211mgmt_req.a.disassoc, 0, 5 * 4);
		alloc_p80211mgmt_req.a.disassoc.buf =
		    (UINT8 *) p80211_hdr;
		alloc_p80211mgmt_req.a.disassoc.len = //pb->p80211frmlen;
			    (rxdesc->data->status & 0xfff) - wep_offset;
		acx_mgmt_decode_disassoc(&alloc_p80211mgmt_req.a.disassoc);
		if (hw->macmode != WLAN_MACMODE_ESS_AP) {
			process_disassoc(&alloc_p80211mgmt_req.a.disassoc,
					 hw);
		} else if (hw->macmode == WLAN_MACMODE_NONE
			   || hw->macmode == WLAN_MACMODE_ESS_STA) {
			process_disassociate(&alloc_p80211mgmt_req.a.
					     disassoc, hw);
		}
		break;
	case WLAN_FSTYPE_AUTHEN /* 0x0b */ :
		memset(&alloc_p80211mgmt_req.a.authen, 0, 8 * 4);
		alloc_p80211mgmt_req.a.authen.buf =
		    (UINT8 *) p80211_hdr;
		alloc_p80211mgmt_req.a.authen.len = //pb->p80211frmlen;
			    (rxdesc->data->status & 0xfff) - wep_offset;
		acx_mgmt_decode_authen(&alloc_p80211mgmt_req.a.authen);
		if (!memcmp(hw->address,
			    alloc_p80211mgmt_req.a.authen.hdr->a3.a2,
			    WLAN_ADDR_LEN)) {
			process_authen(&alloc_p80211mgmt_req.a.authen,
				       hw);
		}
		break;
	case WLAN_FSTYPE_DEAUTHEN /* 0x0c */ :
		memset(&alloc_p80211mgmt_req.a.deauthen, 0, 5 * 4);
		alloc_p80211mgmt_req.a.deauthen.buf =
		    (UINT8 *) p80211_hdr;
		alloc_p80211mgmt_req.a.deauthen.len =
		    (rxdesc->data->status & 0xfff) - wep_offset;
		acx_mgmt_decode_deauthen(&alloc_p80211mgmt_req.a.deauthen);
		if (hw->macmode != WLAN_MACMODE_ESS_AP) {
			process_deauthen(&alloc_p80211mgmt_req.a.deauthen,
					 hw);
		} else if (hw->macmode == WLAN_MACMODE_NONE
			   || hw->macmode == WLAN_MACMODE_ESS_STA) {
			process_deauthenticate(&alloc_p80211mgmt_req.a.
					       deauthen, hw);
		}
		break;
	}
	FN_EXIT(0, 0);

	return 0;
}

#if UNUSED
/*----------------------------------------------------------------
* process_class_frame
* FIXME: rename to acx100_process_class_frame
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


/* process_class_frame()
 * STATUS: FINISHED.
 */
static int process_class_frame(struct rxhostdescriptor * skb, wlandevice_t * hw, int vala)
{
	return 1;
}
#endif

/*----------------------------------------------------------------
* process_NULL_fame
* FIXME: rename to acx100_process_NULL_frame
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

/* process_NULL_frame()
 * STATUS: FINISHED, UNVERIFIED.
 */
static int process_NULL_frame(struct rxhostdescriptor * pb, wlandevice_t * hw, int vala)
{
	UINT16 fc;
	signed char *esi;
	UINT8 *ebx;
	p80211_hdr_t *fr;
	client_t *client;
	int result = 0;
	client_t *resclt;

	acxlog(L_STATE, "%s: UNVERIFIED.\n", __func__);
	esi = NULL;
	fr = (p80211_hdr_t*)&pb->data->buf;
	if (hw->rx_config_1 & RX_CFG1_INCLUDE_ADDIT_HDR) {
		fr = (p80211_hdr_t*)((UINT8*)fr + 4);
	}

	fc = fr->a3.fc;

	if ((!WLAN_GET_FC_TODS(fc)) && (!WLAN_GET_FC_FROMDS(fc))) {
		esi = fr->a3.a1;
		ebx = fr->a3.a2;
	} else if ((!WLAN_GET_FC_TODS(fc)) && (WLAN_GET_FC_FROMDS(fc))) {
		esi = fr->a3.a1;
		ebx = fr->a3.a3;
	} else if ((WLAN_GET_FC_TODS(fc)) && (!WLAN_GET_FC_FROMDS(fc))) {
		ebx = fr->a3.a2;
		esi = fr->a3.a1;
	} else
		ebx = fr->a3.a2;

	if (esi[0x0] < 0) {
		result = 1;
		goto done;
	}
	for (client = acx_get_client_from_sta_hash_tab(ebx); client;
	     client = client->next) {
		if (!memcmp(ebx, client->address, WLAN_ADDR_LEN)) {
			resclt = client;
			goto end;
		}
	}
	resclt = NULL;

      end:
	if (resclt)
		result = 0;
	else {
		acxlog(L_BINDEBUG | L_XFER, "<transmit_deauth 7>\n");
		transmit_deauthen(ebx, 0x0, hw, WLAN_MGMT_REASON_CLASS2_NONAUTH /* 6 */);
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
void acx100_process_probe_response(struct rxbuffer *mmt, wlandevice_t * hw,
			  acxp80211_hdr_t * hdr)
{
	UINT8 *pSuppRates;
	UINT8 *pDSparms;
	UINT32 station;
	UINT8 *a;
	struct iS *ss;
	int i, max_rate = 0;

	acxlog(L_STATE, "%s: UNVERIFIED, previous iStable: %d.\n",
	       __func__, hw->iStable);

	FN_ENTER;

	/* uh oh, we found more sites/stations than we can handle with
	 * our current setup: pull the emergency brake and stop scanning! */
	if ((UINT) hw->iStable > MAX_NUMBER_OF_SITE) {
		acx100_issue_cmd(hw, ACX100_CMD_STOP_SCAN, 0, 0, 5000);
		acx100_set_status(hw, ISTATUS_2_WAIT_AUTH);

		acxlog(L_BINDEBUG | L_ASSOC,
		       "<Scan Beacon> iStable > MAX_NUMBER_OF_SITE\n");

		return;
	}

	/* pSuppRates points to the Supported Rates element: info[1] is essid_len */
	pSuppRates = &hdr->info[hdr->info[0x1] + 0x2];
	/* pDSparms points to the DS Parameter Set */
	pDSparms = &pSuppRates[pSuppRates[0x1] + 0x2];

	/* filter out duplicate stations we already registered in our list */
	for (station = 0; station < hw->iStable; station++) {
		UINT8 *a = hw->val0x126c[station].address;
		acxlog(L_DEBUG,
		       "checking station %ld [%02X %02X %02X %02X %02X %02X]\n",
		       station, a[0], a[1], a[2], a[3], a[4], a[5]);
		if (acx100_is_mac_address_equal
		    (hdr->a4.a3,
		     hw->val0x126c[station].address)) {
			acxlog(L_DEBUG,
			       "station already in our list, no need to add.\n");
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
	memset(&hw->val0x126c[hw->iStable], 0, sizeof(struct iS));

	/* copy the SSID element */
	memcpy(hw->val0x126c[hw->iStable].address,
	       hdr->a4.a3, WLAN_BSSID_LEN);

	if (hdr->info[0x1] <= 0x20) {
		hw->val0x126c[hw->iStable].size = hdr->info[0x1];
		memcpy(hw->val0x126c[hw->iStable].essid,
		       &hdr->info[0x2], hdr->info[0x1]);
	}

	hw->val0x126c[hw->iStable].channel = pDSparms[2];
	hw->val0x126c[hw->iStable].fWEPPrivacy = (hdr->val0x22 >> 0x4) & 1;	/* "Privacy" field */
	hw->val0x126c[hw->iStable].cap = hdr->val0x22;
	memcpy(hw->val0x126c[hw->iStable].supp_rates,
	       &pSuppRates[2], 0x8);
	hw->val0x126c[hw->iStable].sir = mmt->level;
	hw->val0x126c[hw->iStable].snr = mmt->snr;

	a = hw->val0x126c[hw->iStable].address;
	ss = &hw->val0x126c[hw->iStable];

	acxlog(L_DEBUG, "Supported Rates: ");
	/* find max. transfer rate */
	for (i=0; i < pSuppRates[1]; i++)
	{
		acxlog(L_DEBUG, "%s Rate: %d%sMbps, ",
			(pSuppRates[2+i] & 0x80) ? "Basic" : "Operational",
			(int)(pSuppRates[2+i] & ~0x80 / 2),
			(pSuppRates[2+i] & 1) ? ".5" : "");
		if ((pSuppRates[2+i] & ~0x80) > max_rate)
			max_rate = pSuppRates[2+i] & ~0x80;
	}
	acxlog(L_DEBUG, ".\n");

	acxlog(L_STD | L_ASSOC,
	       "%s: found and registered station %d: ESSID \"%s\" on channel %ld, BSSID %02X %02X %02X %02X %02X %02X (%s, %d%sMbps), SIR %ld, SNR %ld.\n",
	       __func__,
	       hw->iStable,
	       ss->essid, ss->channel,
	       a[0], a[1], a[2], a[3], a[4], a[5],
	       (WLAN_GET_MGMT_CAP_INFO_IBSS(ss->cap)) ? "Ad-Hoc peer" : "Access Point",
	       (int)(max_rate / 2), (max_rate & 1) ? ".5" : "",
	       ss->sir, ss->snr);

	/* found one station --> increment counter */
	hw->iStable++;
	FN_EXIT(0, 0);
}

char *get_status_string(int status)
{
	char *status_str[22] =
	{ "Successful", "Unspecified failure",
	  "Reserved error code", "Reserved error code", "Reserved erro code",
	  "Reserved error code", "Reserved error code", "Reserved erro code",
	  "Reserved error code", "Reserved error code",
	  "Cannot support all requested capabilities in the Capability Information field. TRANSLATION: Bug in ACX100 driver?",
	  "Reassociation denied due to reason outside the scope of 802.11b standard. TRANSLATION: Bug in ACX100 driver?",
	  "Association denied due to reason outside the scope of 802.11b standard. TRANSLATION: peer station probably has MAC filtering enabled, FIX IT!",
	  "Responding station does not support the specified authentication algorithm. TRANSLATION: Bug in ACX100 driver?",
	  "Received an Authentication frame with transaction sequence number out of expected sequence. TRANSLATION: Bug in ACX100 driver?",
	  "Authentication rejected because of challenge failure. TRANSLATION: Bug in ACX100 driver?",
	  "Authentication rejected due to timeout waiting for next frame in sequence. TRANSLATION: Bug in ACX100 driver?",
	  "Association denied because AP is unable to handle additional associated stations",
	  "Association denied due to requesting station not supporting all of the data rates in the BSSBasicRateSet parameter. TRANSLATION: peer station has an incompatible set of data rates configured, FIX IT!",
	  "Association denied due to requesting station not supporting the Short Preamble option. TRANSLATION: Bug in ACX100 driver?",
	  "Association denied due to requesting station not supporting the PBCC Modulation option. TRANSLATION: Bug in ACX100 driver?",
	  "Association denied due to requesting station not supporting the Channel Agility option. TRANSLATION: Bug in ACX100 driver?"};

	  return status <= 21 ? status_str[status] : "Reserved";
}

/*----------------------------------------------------------------
* process_assocresp
* FIXME: rename to acx100_process_assocresp
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

/* process_assocresp()
 * STATUS: should be ok, UNVERIFIED.
 */
int process_assocresp(wlan_fr_assocresp_t * req, wlandevice_t * hw)
{
	p80211_hdr_t *hdr;
	int res = 0;
	memmap_t pdr;

	FN_ENTER;
	acxlog(L_STATE, "%s: UNVERIFIED.\n", __func__);
	hdr = req->hdr;

	// FIXME: toDS and fromDS = 1 ??
	if (hdr->a4.fc & 0x300)
		res = 1;
	else {
		if (acx100_is_mac_address_equal(hw->dev_addr, hdr->a4.a1 /* RA */)) {
			if (req->status[0] == WLAN_MGMT_STATUS_SUCCESS) {
				pdr.m.asid.vala = req->aid[0];
				acx100_configure(hw, &pdr, ACX100_RID_ASSOC_ID);
				acx100_set_status(hw, ISTATUS_4_ASSOCIATED);
				acxlog(L_BINSTD | L_ASSOC,
				       "ASSOCIATED!\n");
			}
			else
				acxlog(L_STD | L_ASSOC, "Association FAILED: peer station sent response status code %d: \"%s\"!\n", req->status[0], get_status_string(req->status[0]));
			res = 0;
		} else
			res = 1;
	}
	FN_EXIT(0, 0);
	return res;
}

/*----------------------------------------------------------------
* process_reassocresp
* FIXME: rename to acx100_process_reassocresp
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

/* process_reassocresp()
 * STATUS: should be ok, UNVERIFIED.
 */
int process_reassocresp(wlan_fr_reassocresp_t * req, wlandevice_t * hw)
{
	p80211_hdr_t *hdr;

	FN_ENTER;
	acxlog(L_STATE, "%s: UNVERIFIED.\n", __func__);
	hdr = req->hdr;

	// FIXME: toDS and fromDS = 1 ??
	if (hdr->a3.fc & 0x300)
		return 1;
	else {
		if (hw->macmode == WLAN_MACMODE_NONE /* 0, Ad-Hoc */ )
			return 1;
		if (acx100_is_mac_address_equal(hw->dev_addr, hdr->a3.a1 /* RA */)) {
			if (req->status[0] == WLAN_MGMT_STATUS_SUCCESS) {
				acx100_set_status(hw, ISTATUS_4_ASSOCIATED);
			}
			else
				acxlog(L_STD | L_ASSOC, "Reassociation FAILED: response status code %d: \"%s\"!\n", req->status[0], get_status_string(req->status[0]));
			return 0;
		} else
			return 1;
	}
	FN_EXIT(0, 0);
}

/* process_authen()
 * STATUS: FINISHED, UNVERIFIED.
 */
int process_authen(wlan_fr_authen_t *req, wlandevice_t *hw)
{
	p80211_hdr_t *hdr;
	client_t *clt;
	client_t *currclt;

	FN_ENTER;
	acxlog(L_STATE|L_ASSOC, "%s: UNVERIFIED.\n", __func__);
	hdr = req->hdr;
	if (WLAN_GET_FC_TODS(hdr->a3.fc) || WLAN_GET_FC_FROMDS(hdr->a3.fc))
		return 0;

	if (!hw)
		return 0;

	acx100_log_mac_address(L_ASSOC, hw->dev_addr);
	acx100_log_mac_address(L_ASSOC, hdr->a3.a1);
	acx100_log_mac_address(L_ASSOC, hdr->a3.a2);
	acx100_log_mac_address(L_ASSOC, hdr->a3.a3);
	acx100_log_mac_address(L_ASSOC, hw->bssid);
	if (!acx100_is_mac_address_equal(hw->dev_addr, hdr->a3.a1) &&
			!acx100_is_mac_address_equal(hw->bssid, hdr->a3.a1))
		return 1;
	if (hw->mode == 0)
		return 0;

	if (hw->auth_alg <= 1) {
		if (hw->auth_alg != *(req->auth_alg))
		{
			acxlog(L_ASSOC, "authentication algorithm mismatch: want: %ld, req: %d\n", hw->auth_alg, *(req->auth_alg));
			return 0;
		}
	}
	acxlog(L_ASSOC,"Algorithm is ok\n");
	currclt = acx_get_client_from_sta_hash_tab(hdr->a3.a2);
	acxlog(L_ASSOC,"Got current client for sta hash tab\n");
	clt = NULL;
	while (currclt) {
		if (!memcmp(hdr->a3.a2, currclt->address, WLAN_ADDR_LEN)) {
			clt = currclt;
			break;
		}
		currclt = currclt->next;
	}
	acxlog(L_ASSOC,"Found acceptable client\n");
	/* create a new entry if station isn't registered yet */
	if (!clt) {
		if (!(clt = sta_list_add(hdr->a3.a2)))
		{
			acxlog(L_ASSOC,"Could not allocate room for this client\n");
			return 0;
		}

		memcpy(clt->address, hdr->a3.a2, WLAN_ADDR_LEN);
		clt->used = 1;
	}
	/* now check which step in the authentication sequence we are
	 * currently in, and act accordingly */
	acxlog(L_ASSOC, "process_authen auth seq step %d.\n", req->auth_seq[0]);
	switch (req->auth_seq[0]) {
	case 1:
		if (hw->mode == 2)
			break;
		transmit_authen2(req, clt, hw);
		break;
	case 2:
		if (hw->mode == 3)
			break;
		if (req->status[0] == WLAN_MGMT_STATUS_SUCCESS) {
			if (req->auth_alg[0] == WLAN_AUTH_ALG_OPENSYSTEM) {
				acx100_set_status(hw, ISTATUS_3_AUTHENTICATED);
				transmit_assoc_req(hw);
			} else
			if (req->auth_alg[0] == WLAN_AUTH_ALG_SHAREDKEY) {
				transmit_authen3(req, hw);
			}
		} else {
			acxlog(L_ASSOC, "Authentication FAILED (status code %d: \"%s\"), still waiting for authentication.\n", req->status[0], get_status_string(req->status[0]));
			acx100_set_status(hw, ISTATUS_2_WAIT_AUTH);
		}
		break;
	case 3:
		if ((hw->mode == 2)
		    || (clt->auth_alg != WLAN_AUTH_ALG_SHAREDKEY)
		    || (req->auth_alg[0] != WLAN_AUTH_ALG_SHAREDKEY)
		    || (clt->val0xe != 2))
			break;
		acxlog(L_STD,
		       "FIXME: TODO: huh??? incompatible data type!\n");
		currclt = (client_t *)req->challenge;
		if (!memcmp(currclt->address, clt->val0x18, 0x80)
		    && (((UINT8) currclt->aid) != 0x10))
			if ((currclt->aid >> 8) != 0x80)
				break;
		transmit_authen4(req, hw);
		memcpy(clt->address, hdr->a3.a2, WLAN_ADDR_LEN);
		clt->used = 2;
		clt->val0xe = 4;
		clt->val0x98 = hdr->a3.seq;
		break;
	case 4:
		if (hw->mode == 3)
			break;

		/* ok, we're through: we're authenticated. Woohoo!! */
		acx100_set_status(hw, ISTATUS_3_AUTHENTICATED);
		acxlog(L_BINSTD | L_ASSOC, "Authenticated!\n");
		transmit_assoc_req(hw);
		break;
	}
	FN_EXIT(0, 0);
	return 0;
}
/*----------------------------------------------------------------
* process_deauthen
* FIXME rename to acx100_process_deauthen
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


/* process_deauthen()
 * STATUS: FINISHED, UNVERIFIED.
 */
int process_deauthen(wlan_fr_deauthen_t * arg_0, wlandevice_t * hw)
{
	p80211_hdr_t *hdr;
	int result;
	UINT8 *addr;
	client_t *client;
	client_t *resclt = NULL;

	FN_ENTER;
	acxlog(L_STATE, "%s: UNVERIFIED.\n", __func__);
	acxlog(L_STD, "Processing deauthen packet. Hmm, should this have happened?\n");

	hdr = arg_0->hdr;

	if (hdr->a3.fc & 0x300)
	{
		result = 1;
		goto end;
	}
	addr = hdr->a3.a2;
	if (memcmp(addr,hw->dev_addr,WLAN_ADDR_LEN))
	{
		/* OK, we've been asked to leave the ess. Do we 
		 * ask to return or do we leave quietly? I'm 
		 * guessing that since we are still up and 
		 * running we should attempt to rejoin, from the 
		 * starting point. So:
		 */
		acx100_set_status(hw,ISTATUS_2_WAIT_AUTH);
		result = 0;
		goto end;
	}			

	client = acx_get_client_from_sta_hash_tab(addr);

	if (client == NULL) {
		result = 0;
		goto end;
	}

	do {
		if (!memcmp(addr, &client->address, WLAN_ADDR_LEN)) {
			resclt = client;
			goto end;
		}
	} while ((client = client->next));
	resclt = NULL;
      end:
	if (resclt) {
		resclt->used = 1;
		result = 0;
	} else
		result = 1;

	FN_EXIT(0, 0);
	return result;
}
/*----------------------------------------------------------------
* process_deauthenticate
* FIXME: rename to acx100_process_deauthenticate
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


/* process_deauthenticate()
 * STATUS: FINISHED, UNVERIFIED.
 */
int process_deauthenticate(wlan_fr_deauthen_t * req, wlandevice_t * hw)
{
	p80211_hdr_t *hdr;

	FN_ENTER;
	acxlog(L_STATE, "%s: UNVERIFIED.\n", __func__);
	acxlog(L_STD, "processing deauthenticate packet. Hmm, should this have happened?\n");
	hdr = req->hdr;
	if (hdr->a3.fc & 0x300)
		return 1;
	else {
		if (hw->macmode == WLAN_MACMODE_NONE /* 0, Ad-Hoc */ )
			return 1;
		if (acx100_is_mac_address_equal(hw->dev_addr, hdr->a3.a1)) {
			if (hw->iStatus > ISTATUS_2_WAIT_AUTH) {
				acx100_set_status(hw, ISTATUS_2_WAIT_AUTH);
				return 0;
			}
		}
	}
	FN_EXIT(0, 0);
	return 1;
}
/*----------------------------------------------------------------
* transmit_deauthen
* FIXME: rename to acx100_transmit_deauthen
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


/* transmit_deauthen()
 * STATUS: should be ok, but UNVERIFIED.
 */
int transmit_deauthen(char *a, client_t *clt, wlandevice_t *wlandev, int reason)
{
	TxData *hd;
	struct deauthen_frame_body *payload;
	struct txdescriptor *tx_desc;
	struct txhostdescriptor *hdesc_header;
	struct txhostdescriptor *hdesc_payload;

	FN_ENTER;
	acxlog(L_STATE, "%s: UNVERIFIED.\n", __func__);

	if ((tx_desc = acx100_get_tx_desc(wlandev)) == NULL) {
		return 0;
	}

	hdesc_header = tx_desc->host_desc;
	hdesc_payload = tx_desc->host_desc + 1;

	hd = (TxData *)hdesc_header->data;
	payload = (struct deauthen_frame_body *)hdesc_payload->data;

	hd->frame_control = WLAN_SET_FC_FTYPE(WLAN_FTYPE_MGMT) | WLAN_SET_FC_FSTYPE(WLAN_FSTYPE_DEAUTHEN);	/* 0xc0 */
	hd->duration_id = 0;

	if (clt) {
		clt->used = 1;
		memcpy(hd->da, clt->address, WLAN_ADDR_LEN);
	} else {
		memcpy(hd->da, a, WLAN_ADDR_LEN);
	}

	memcpy(hd->sa, wlandev->dev_addr, WLAN_ADDR_LEN);

	/* FIXME: this used to use dev_addr, but I think it should use
	 * the BSSID of the network we're associated to: wlandev->address */
	memcpy(hd->bssid, wlandev->address, WLAN_BSSID_LEN);
	hd->sequence_control = 0;

	acxlog(L_BINDEBUG | L_ASSOC | L_XFER,
	       "<transmit_deauthen>sta=%02X:%02X:%02X:%02X:%02X:%02X for %d\n",
	       hd->da[0x0], hd->da[0x1], hd->da[0x2],
	       hd->da[0x3], hd->da[0x4], hd->da[0x5],reason);

	hdesc_header->length = WLAN_HDR_A3_LEN;
	hdesc_header->val0x4 = 0;

	payload->reason = reason;

	hdesc_payload->length = sizeof(deauthen_frame_body_t);
	hdesc_payload->val0x4 = 0;

	tx_desc->total_length = hdesc_payload->length + hdesc_header->length;

	acx100_dma_tx_data(wlandev, tx_desc);

	FN_EXIT(0, 0);
	return 0;
}

/*----------------------------------------------------------------
* transmit_authen1
* FIXME: rename to acx100_transmit_authen1
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

/* transmit_authen1()
 * STATUS: UNVERIFIED
 */
int transmit_authen1(wlandevice_t *wlandev)
{
	struct auth_frame_body *payload;
	struct txdescriptor *tx_desc;
	struct txhostdescriptor *hdesc_header;
	struct txhostdescriptor *hdesc_payload;
	TxData *hd;

	acxlog(L_STATE, "%s: UNVERIFIED.\n", __func__);

	FN_ENTER;

	acxlog(L_BINSTD | L_ASSOC, "Sending authentication1 request, awaiting response!\n");

	if ((tx_desc = acx100_get_tx_desc(wlandev)) == NULL) {
		return 1;
	}

	hdesc_header = tx_desc->host_desc;
	hdesc_payload = tx_desc->host_desc + 1;

	hd = (TxData *)hdesc_header->data;
	payload = (struct auth_frame_body *)hdesc_payload->data;

	hd->frame_control = WLAN_SET_FC_FSTYPE(WLAN_FSTYPE_AUTHEN);	/* 0xb0 */
	hd->duration_id = host2ieee16(0x8000);

	memcpy(hd->da, wlandev->address, WLAN_ADDR_LEN);
	memcpy(hd->sa, wlandev->dev_addr, WLAN_ADDR_LEN);
	memcpy(hd->bssid, wlandev->address, WLAN_BSSID_LEN);

	hd->sequence_control = 0;

	hdesc_header->length = WLAN_HDR_A3_LEN;
	hdesc_header->val0x4 = 0;

	payload->auth_alg = wlandev->auth_alg;
	payload->auth_seq = 1;
	payload->status = 0;

	hdesc_payload->length = 2 + 2 + 2; /* 6 */
	hdesc_payload->val0x4 = 0;

	tx_desc->total_length = hdesc_payload->length + hdesc_header->length;

	acx100_dma_tx_data(wlandev, tx_desc);
	FN_EXIT(0, 0);
	return 0;
}

/*----------------------------------------------------------------
* transmit_authen2
* FIXME: rename to acx100_transmit_authen2
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

/* transmit_authen2()
 * STATUS: UNVERIFIED. (not binary compatible yet)
 */
int transmit_authen2(wlan_fr_authen_t * arg_0, client_t * sta_list,
		      wlandevice_t * hw)
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
		memcpy(sta_list->address, arg_0->hdr->a3.a2,
		       WLAN_ADDR_LEN);
		sta_list->val0x8 = (arg_0->hdr->a3.fc & 0x1000) >> 12;
		sta_list->auth_alg = *(arg_0->auth_alg);
		sta_list->val0xe = 2;
		sta_list->val0x98 = arg_0->hdr->a3.seq;

		if ((tx_desc = acx100_get_tx_desc(hw)) == NULL) {
			return 1;
		}

		hdesc_header = tx_desc->host_desc;
		hdesc_payload = tx_desc->host_desc + 1;

		hd = (TxData*)hdesc_header->data;
		payload = (struct auth_frame_body *)hdesc_payload->data;

		hd->frame_control = WLAN_SET_FC_FSTYPE(WLAN_FSTYPE_AUTHEN);	/* 0xb0 */
		hd->duration_id = arg_0->hdr->a3.dur;
		hd->sequence_control = arg_0->hdr->a3.seq;

		memcpy(hd->da, arg_0->hdr->a3.a2, WLAN_ADDR_LEN);
		memcpy(hd->sa, arg_0->hdr->a3.a1, WLAN_ADDR_LEN);
		memcpy(hd->bssid, arg_0->hdr->a3.a3, WLAN_BSSID_LEN);

		payload->auth_alg = *(arg_0->auth_alg);

		payload->auth_seq = 2;

		payload->status = 0;

		if (*(arg_0->auth_alg) == WLAN_AUTH_ALG_OPENSYSTEM) {
			sta_list->used = 2;
			packet_len += 2 + 2 + 2;
		} else {	/* shared key */
			gen_challenge(&payload->challenge);
			memcpy(&sta_list->val0x18, payload->challenge.text, 0x80);
			packet_len += 2 + 2 + 2 + 1+1+0x80;
		}

		hdesc_header->length = WLAN_HDR_A3_LEN;
		hdesc_header->val0x4 = 0;
		hdesc_payload->length = packet_len - WLAN_HDR_A3_LEN;
		hdesc_payload->val0x4 = 0;
		tx_desc->total_length = hdesc_payload->length + hdesc_header->length;

		acxlog(L_BINDEBUG | L_ASSOC | L_XFER,
		       "<transmit_auth2> BSSID=%02X:%02X:%02X:%02X:%02X:%02X\n",
		       hd->bssid[0], hd->bssid[1], hd->bssid[2],
		       hd->bssid[3], hd->bssid[4], hd->bssid[5]);

		acx100_dma_tx_data(hw, tx_desc);
	}
	FN_EXIT(0, 0);
	return 0;
}

/*----------------------------------------------------------------
* transmit_authen3
* FIXME: rename to acx100_transmit_authen3
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

/* transmit_authen3()
 * STATUS: UNVERIFIED.
 */
int transmit_authen3(wlan_fr_authen_t * arg_0, wlandevice_t * hw)
{
	UINT32 packet_len;
	struct txdescriptor *tx_desc;
	struct txhostdescriptor *hdesc_header;
	struct txhostdescriptor *hdesc_payload;
	TxData *hd;
	struct auth_frame_body *payload;

	FN_ENTER;
	acxlog(L_STATE, "%s: UNVERIFIED.\n", __func__);
	if ((tx_desc = acx100_get_tx_desc(hw)) == NULL) {
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
	memcpy(hd->da, hw->address, WLAN_ADDR_LEN);
	memcpy(hd->sa, hw->dev_addr, WLAN_ADDR_LEN);
	memcpy(hd->bssid, hw->address, WLAN_BSSID_LEN);

	hdesc_header->length = WLAN_HDR_A3_LEN;
	hdesc_header->val0x4 = 0;

	payload->auth_alg = *(arg_0->auth_alg);

	payload->auth_seq = 3;

	payload->status = 0;

	memcpy(&payload->challenge, arg_0->challenge, arg_0->challenge->len + 2);

	packet_len += 8 + arg_0->challenge->len;

	hdesc_payload->length = packet_len - WLAN_HDR_A3_LEN;
	hdesc_payload->val0x4 = 0;

	tx_desc->total_length = hdesc_payload->length + hdesc_header->length;

	acxlog(L_BINDEBUG | L_ASSOC | L_XFER, "transmit_auth3!\n");

	acx100_dma_tx_data(hw, tx_desc);
	FN_EXIT(0, 0);
	return 0;
}

/*----------------------------------------------------------------
* transmit_authen4
* FIXME: rename to acx100_transmit_authen4
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

/* transmit_authen4()
 * STATUS: UNVERIFIED.
 */
int transmit_authen4(wlan_fr_authen_t *arg_0, wlandevice_t *hw)
{
	struct txdescriptor *tx_desc;
	struct txhostdescriptor *hdesc_header;
	struct txhostdescriptor *hdesc_payload;
	TxData *hd;
	struct auth_frame_body *payload;

	FN_ENTER;
	acxlog(L_STATE, "%s: UNVERIFIED.\n", __func__);

	if ((tx_desc = acx100_get_tx_desc(hw)) == NULL) {
		return 1;
	}

	hdesc_header = tx_desc->host_desc;
	hdesc_payload = tx_desc->host_desc + 1;

	hd = (TxData *)hdesc_header->data;
	payload = (struct auth_frame_body *)hdesc_payload->data;

	hd->frame_control = WLAN_SET_FC_FSTYPE(WLAN_FSTYPE_AUTHEN);	/* 0xb0 */
	hd->duration_id = arg_0->hdr->a3.dur;
	hd->sequence_control = arg_0->hdr->a3.seq;

	memcpy(hd->da, arg_0->hdr->a3.a2, WLAN_ADDR_LEN);
	/* FIXME: huh? why is there no ¨sa"? */
	memcpy(hd->bssid, arg_0->hdr->a3.a3, WLAN_BSSID_LEN);

	hdesc_header->length = WLAN_HDR_A3_LEN;
	hdesc_header->val0x4 = 0;

	payload->auth_alg = *(arg_0->auth_alg);
	payload->auth_seq = 4;
	payload->status = 0;

	hdesc_payload->length = 6;
	hdesc_payload->val0x4 = 0;

	tx_desc->total_length = hdesc_payload->length + hdesc_header->length;

	acx100_dma_tx_data(hw, tx_desc);
	FN_EXIT(0, 0);
	return 0;
}

/*----------------------------------------------------------------
* transmit_assoc_req
* FIXME: rename to acx100_transmit_assoc_req
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

/* transmit_assoc_req()
 * STATUS: almost ok, but UNVERIFIED.
 */
int transmit_assoc_req(wlandevice_t *hw)
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
	if ((tx_desc = acx100_get_tx_desc(hw)) == NULL) {
		return 1;
	}

	packet_len = WLAN_HDR_A3_LEN;

	header = tx_desc->host_desc;      /* hostdescriptor for header */
	payload = tx_desc->host_desc + 1; /* hostdescriptor for payload */

	hd = (TxData *)header->data;
	pCurrPos = (UINT8 *)payload->data;

	hd->frame_control = WLAN_SET_FC_FSTYPE(WLAN_FSTYPE_ASSOCREQ);  /* 0x00 */;
	hd->duration_id = host2ieee16(0x8000);

	memcpy(hd->da, hw->address, WLAN_ADDR_LEN);
	memcpy(hd->sa, hw->dev_addr, WLAN_ADDR_LEN);
	memcpy(hd->bssid, hw->address, WLAN_BSSID_LEN);

	hd->sequence_control = 0;

	header->length = WLAN_HDR_A3_LEN;
	header->val0x4 = 0;

	/* now start filling the AssocReq frame body */
#if BROKEN
	*(UINT16 *)pCurrPos = host2ieee16(hw->capabilities & ~(WLAN_SET_MGMT_CAP_INFO_IBSS(1)));
#else
	/* FIXME: is it correct that we have to manually patc^H^H^H^Hadjust the
	 * Capabilities like that?
	 * I'd venture that hw->capabilities
	 * (acx100_update_capabilities()) should have set that
	 * beforehand maybe...
	 * Anyway, now Managed network association works properly
	 * without failing.
	 */
	/*
	*(UINT16 *)pCurrPos = host2ieee16((hw->capabilities & ~(WLAN_SET_MGMT_CAP_INFO_IBSS(1))) | WLAN_SET_MGMT_CAP_INFO_ESS(1));
	*/
	*(UINT16 *)pCurrPos = host2ieee16(WLAN_SET_MGMT_CAP_INFO_ESS(1));
	if (hw->wep_restricted)
		*(UINT16 *)pCurrPos |= host2ieee16(WLAN_SET_MGMT_CAP_INFO_PRIVACY(1));
	/* only ask for short preamble if the peer station supports it */
	if (WLAN_GET_MGMT_CAP_INFO_SHORT(hw->station_assoc.cap))
		*(UINT16 *)pCurrPos |= host2ieee16(WLAN_SET_MGMT_CAP_INFO_SHORT(1));
	/* only ask for PBCC support if the peer station supports it */
	if (WLAN_GET_MGMT_CAP_INFO_PBCC(hw->station_assoc.cap))
		*(UINT16 *)pCurrPos |= host2ieee16(WLAN_SET_MGMT_CAP_INFO_PBCC(1));
#endif
	acxlog(L_ASSOC, "association: requesting capabilities 0x%04X\n", *(UINT16 *)pCurrPos);
	pCurrPos += 2;

	*(UINT16 *)pCurrPos = host2ieee16(hw->listen_interval);
	pCurrPos += 2;

	*(UINT8 *)pCurrPos = 0; /* Element ID */
	pCurrPos += 1;
	*(UINT8 *)pCurrPos = strlen(hw->essid_for_assoc); /* Length */
	memcpy(&pCurrPos[1], hw->essid_for_assoc, pCurrPos[0]);
	pCurrPos += 1 + pCurrPos[0];

	*(UINT8 *)pCurrPos = 1; /* Element ID */
	pCurrPos += 1;
	*(UINT8 *)pCurrPos = hw->rate_spt_len; /* Length */
	pCurrPos += 1;
	memcpy(pCurrPos, hw->rate_support1, hw->rate_spt_len);
	pCurrPos += hw->rate_spt_len;

	packet_len += (int)pCurrPos - (int)payload->data;

	payload->length = packet_len - WLAN_HDR_A3_LEN;
	payload->val0x4 = 0;

	tx_desc->total_length = payload->length + header->length;

	acx100_dma_tx_data(hw, tx_desc);
	FN_EXIT(0, 0);
	return 0;
}

/*----------------------------------------------------------------
* transmit_disassoc
* FIXME: rename to acx100_transmit_disassoc
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

/* transmit_disassoc()
 * STATUS: almost ok, but UNVERIFIED.
 */
// FIXME: type of clt is a guess
// I'm not sure if clt is needed.
UINT32 transmit_disassoc(client_t *clt, wlandevice_t *hw)
{
	struct disassoc_frame_body *payload;
	struct txdescriptor *tx_desc;
	struct txhostdescriptor *hdesc_header;
	struct txhostdescriptor *hdesc_payload;
	TxData *hd;

	FN_ENTER;
	acxlog(L_STATE, "%s: UNVERIFIED.\n", __func__);
//	if (clt != NULL) {
		if ((tx_desc = acx100_get_tx_desc(hw)) == NULL) {
			return 1;
		}

		hdesc_header = tx_desc->host_desc;
		hdesc_payload = tx_desc->host_desc + 1;

		hd = (TxData *)hdesc_header->data;
		payload = (struct disassoc_frame_body *)hdesc_payload->data;

//		clt->used = 2;

		hd->frame_control = WLAN_SET_FC_FSTYPE(WLAN_FSTYPE_DISASSOC);	/* 0xa0 */
		hd->duration_id = 0;
		hd->sequence_control = 0;

		memcpy(hd->da, hw->address, WLAN_ADDR_LEN);
		memcpy(hd->sa, hw->dev_addr, WLAN_ADDR_LEN);
		memcpy(hd->bssid, hw->dev_addr, WLAN_BSSID_LEN);

		hdesc_header->length = WLAN_HDR_A3_LEN;
		hdesc_header->val0x4 = 0;

		payload->reason = 7;	/* "Class 3 frame received from nonassociated station." */

		hdesc_payload->length = hw->rate_spt_len + 8;
		hdesc_payload->val0x4 = 0;

		tx_desc->total_length = hdesc_payload->length + hdesc_header->length;

		/* FIXME: lengths missing! */
		acx100_dma_tx_data(hw, tx_desc);
		return 1;
//	}
	FN_EXIT(0, 0);
	return 0;
}

/*----------------------------------------------------------------
* gen_challenge
* FIXME: rename to acx100_gen_challenge
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

/* gen_challenge()
 * STATUS: FINISHED.
 */
void gen_challenge(challenge_text_t * d)
{
	d->element_ID = 0x10;
	d->length = 0x80;
	get_ran(d->text, 0x80);
}

/*----------------------------------------------------------------
* get_ran
* FIXME: rename to acx100_get_random
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

/* get_ran()
 * STATUS: UNVERIFIED. (not binary compatible yet)
 */
void get_ran(UINT8 *s, UINT32 stack)
{
	UINT32 var_4;
	UINT8 var_8[4];
	UINT8 seed[4];
	UINT32 count1;
	UINT32 count2;

	UINT16 count, len;
	UINT32 ran;

	acxlog(L_STATE, "%s: UNVERIFIED.\n", __func__);
	var_4 = 0;
	seed[0] = 0;
	seed[1] = 0;
	seed[2] = 0;
	seed[3] = 0;

	// FIXME: What is he doing here ???
	ran = 10000;
	count1 = 0;
	do {
		var_8[count1] = (0x03ff6010 / ran) & 0xff;
		ran = (ran * 0xCCCCCCCD) >> 3;

		count1 += 1;
	} while (count1 <= 3);

	// FIXME: Mmmm, strange ... is it really meant to just take some random part of the stack ??
	len = strlen(var_8);

	// generate a seed
	if (len != 0) {
		count2 = 0;
		if (len > 0) {
			do {
				seed[count2 & 3] =
				    seed[count2 & 3] ^ var_8[count2];
				count2 += 1;
			} while (count2 < len);
		}

		ran =
		    (seed[0]) | (seed[1] << 8) | (seed[2] << 0x10) |
		    (seed[3] << 0x18);

		// generate some random numbers
		count = 0;
		if (stack > 0) {
			do {
				// this is a standard random number generator
				// using "magic" numbers
				ran = (214013 * ran + 2531011);
				s[count] = (ran >> 0x10) & 0xff;
				count += 1;
			} while (count < stack);
		}
	}
}

/*----------------------------------------------------------------
* IBSSIDGen
* FIXME: rename to acx100_ibssid_gen
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

/* IBSSIDGen()
 * This generates a random BSSID to be used for ad-hoc networks
 * (in infrastructure BSS networks, the BSSID is the MAC of the access
 * point)
 * STATUS: should be ok.
 */
void IBSSIDGen(wlandevice_t * hw, unsigned char *p_out)
{
	UINT8 jifmod;
	int i;
	UINT8 oct;

	FN_ENTER;
	for (i = 0; i < 6; i++) {
		/* store jiffies modulo 0xff */
		jifmod = jiffies % 0xff;
		/* now XOR eax with this value */
		oct = hw->dev_addr[i] ^ jifmod;
		/* WLAN_LOG_NOTICE1("temp = %d\n", oct); */
		p_out[i] = oct;
	}

	p_out[0] = (p_out[0] & ~0x80) | 0x40;
	FN_EXIT(0, 0);
}

/*----------------------------------------------------------------
* d11CompleteScan
* FIXME: rename to acx100_complete_dot11_scan
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

/* d11CompleteScan()
 * STATUS: FINISHED.
 */
void d11CompleteScan(wlandevice_t *wlandev)
{
	UINT32 idx;
	UINT32 essid_len;
	UINT16 needed_cap;
	INT32 idx_found = -1;
	UINT32 found_station = 0;

	FN_ENTER;
	if (wlandev->mode == 2) {
		needed_cap = WLAN_SET_MGMT_CAP_INFO_ESS(1);	/* 1, we require Managed */
	} else {
		needed_cap = (wlandev->mode == 0) ? WLAN_SET_MGMT_CAP_INFO_IBSS(1)	/* 2, we require Ad-Hoc */
		    : WLAN_SET_MGMT_CAP_INFO_ESS(1) | WLAN_SET_MGMT_CAP_INFO_IBSS(1);	/* 3, Ad-Hoc or Managed */
	}
	essid_len = strlen(wlandev->essid);

	acxlog(L_BINDEBUG | L_ASSOC, "Radio scan found %d stations in this area.\n", wlandev->iStable);

	for (idx = 0; idx < wlandev->iStable; idx++) {
		/* V3CHANGE: dbg msg in V1 only */
		acxlog(L_BINDEBUG | L_ASSOC,
		       "<Scan Table> %d: SSID=\"%s\",CH=%d,SIR=%d,SNR=%d\n",
		       (int) idx,
		       wlandev->val0x126c[idx].essid,
		       (int) wlandev->val0x126c[idx].channel,
		       (int) wlandev->val0x126c[idx].sir,
		       (int) wlandev->val0x126c[idx].snr);

		if (!(wlandev->val0x126c[idx].cap & 3))
		{
			acxlog(L_ASSOC, "STRANGE: peer station has neither ESS (Managed) nor IBSS (Ad-Hoc) capability flag set: patching to assume Ad-Hoc!\n");
			wlandev->val0x126c[idx].cap |= WLAN_SET_MGMT_CAP_INFO_IBSS(1);
		}
		acxlog(L_ASSOC, "peer_cap 0x%02x, needed_cap 0x%02x\n",
		       wlandev->val0x126c[idx].cap, needed_cap);

		/* peer station doesn't support what we need? */
		if (!((wlandev->val0x126c[idx].cap & needed_cap) == needed_cap))
			continue; /* keep looking */

		if (!(wlandev->reg_dom_chanmask & (1 << (wlandev->val0x126c[idx].channel - 1) ) ))
		{
			acxlog(L_STD|L_ASSOC, "WARNING: peer station %ld is using channel %ld, which is outside the channel range of the regulatory domain the driver is currently configured for: couldn't join in case of matching settings, might want to adapt your config!\n", idx, wlandev->val0x126c[idx].channel);
			continue; /* keep looking */
		}

		if ((!wlandev->essid_active)
		 || (!memcmp(wlandev->val0x126c[idx].essid, wlandev->essid, essid_len)))
		{
			acxlog(L_ASSOC,
			       "ESSID matches: \"%s\" (station), \"%s\" (config)\n",
			       wlandev->val0x126c[idx].essid,
			       (wlandev->essid_active) ? wlandev->essid : "[any]");
			idx_found = idx;
			found_station = 1;
			acxlog(L_ASSOC, "matching station found!!\n");

			/* stop searching if this station is
			 * on the current channel, otherwise
			 * keep looking for an even better match */
			if (wlandev->val0x126c[idx].channel == wlandev->channel)
				break;
		}
		else
		if (wlandev->val0x126c[idx].essid[0] == '\0')
		{
			/* hmm, station with empty SSID:
			 * using hidden SSID broadcast?
			 */
			idx_found = idx;
			found_station = 1;
			acxlog(L_ASSOC, "found station with empty (hidden?) SSID, considering for association attempt.\n");
			/* ...and keep looking for better matches */
		}
	}
	if (found_station) {
		UINT8 *a;
		acx100_update_capabilities(wlandev);

		/* use ESSID we just found, but if it is empty (no broadcast),
		 * use user configured ESSID instead */
		memcpy(wlandev->essid_for_assoc,
			(wlandev->val0x126c[idx].essid[0] == '\0') ?
			wlandev->essid : wlandev->val0x126c[idx_found].essid,
			sizeof(wlandev->essid_for_assoc));
		wlandev->channel = wlandev->val0x126c[idx_found].channel;
		memcpy(wlandev->address,
		       wlandev->val0x126c[idx_found].address, WLAN_ADDR_LEN);

		a = wlandev->address;
		acxlog(L_STD | L_ASSOC,
		       "%s: matching station FOUND (idx %ld), JOINING (%02X %02X %02X %02X %02X %02X).\n",
		       __func__, idx_found, a[0], a[1], a[2], a[3], a[4], a[5]);
		acx100_join_bssid(wlandev);

		memcpy(&wlandev->station_assoc, &wlandev->val0x126c[idx_found], sizeof(struct iS));
		if (wlandev->preamble_mode == 2)
			/* if Auto mode, then use Preamble setting which
			 * the station supports */
			wlandev->preamble_flag = WLAN_GET_MGMT_CAP_INFO_SHORT(wlandev->station_assoc.cap);
		if (wlandev->mode != 0) {
			transmit_authen1(wlandev);
			acx100_set_status(wlandev, ISTATUS_2_WAIT_AUTH);
		} else {
			acx100_set_status(wlandev, ISTATUS_4_ASSOCIATED);
		}
	} else {		/* uh oh, no station found in range */
		if (wlandev->mode == 0) {	/* phew, we're safe: we intended to use Ad-Hoc mode */
			acxlog(L_STD | L_ASSOC,
			       "%s: no matching station found in range, CANNOT JOIN: generating our own IBSSID instead.\n",
			       __func__);
			IBSSIDGen(wlandev, wlandev->address);
			acx100_update_capabilities(wlandev);
			acx100_join_bssid(wlandev);
			acx100_set_status(wlandev, ISTATUS_4_ASSOCIATED);
		} else {
			acxlog(L_STD | L_ASSOC,
			       "%s: no matching station found in range and not in Ad-Hoc mode --> giving up scanning.\n",
			       __func__);
			acx100_set_status(wlandev, ISTATUS_0_STARTED);
		}
	}
	FN_EXIT(0, 0);
}

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
void ActivatePowerSaveMode(wlandevice_t * hw, int vala)
{
	memmap_t pm;

	FN_ENTER;
	acxlog(L_STATE, "%s: UNVERIFIED.\n", __func__);

	acx100_interrogate(hw, &pm, ACX100_RID_POWER_MGMT);
	if (pm.m.power.a != 0x81)
		return;
	pm.m.power.a = 0;
	pm.m.power.b = 0;
	pm.m.power.c = 0;
	acx100_configure(hw, &pm, ACX100_RID_POWER_MGMT);
	FN_EXIT(0, 0);
}

/*----------------------------------------------------------------
* acx100_timer
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
* STATUS:
*
* Comment:
*
*----------------------------------------------------------------*/

/* acx100_timer()
 * STATUS: should be ok, but UNVERIFIED.
 */
void acx100_timer(unsigned long a)
{
	netdevice_t *ndev = (netdevice_t *)a;
	wlandevice_t *hw = (wlandevice_t *)ndev->priv;
	unsigned long flags;
	
	FN_ENTER;
	acxlog(L_STATE, "%s: UNVERIFIED.\n", __func__);

	acxlog(L_BINDEBUG | L_ASSOC, "<acx_timer> iStatus = %d\n",
	       hw->iStatus);
	if (acx100_lock(hw, &flags))
		return;

	switch (hw->iStatus) {
	case ISTATUS_1_SCANNING:
		if (hw->scan_retries++ <= 4) {
			acx100_set_timer(hw, 2000000);
		}
		break;
	case ISTATUS_2_WAIT_AUTH:
		hw->scan_retries = 0;
		if (hw->auth_assoc_retries++ <= 9) {
			acxlog(L_ASSOC, "resend authen1 request (attempt %d).\n",
			       hw->auth_assoc_retries + 1);
			transmit_authen1(hw);
		} else {
			/* time exceeded: fall back to scanning mode */
			acxlog(L_ASSOC,
			       "authen1 request reply timeout, giving up.\n");
			/* simply set status back to scanning (DON'T start scan) */
			acx100_set_status(hw, ISTATUS_1_SCANNING);
		}
		acx100_set_timer(hw, 1500000);
		break;
	case ISTATUS_3_AUTHENTICATED:
		if (hw->auth_assoc_retries++ <= 9) {
			acxlog(L_ASSOC,
			       "resend association request (attempt %d).\n",
			       hw->auth_assoc_retries + 1);
			transmit_assoc_req(hw);
		} else {
			/* time exceeded: give up */
			acxlog(L_ASSOC,
			       "association request reply timeout, giving up.\n");
			/* simply set status back to scanning (DON'T start scan) */
			acx100_set_status(hw, ISTATUS_1_SCANNING);
		}
		acx100_set_timer(hw, 1500000);
		break;
	case ISTATUS_5_UNKNOWN:
		acx100_set_status(hw, hw->unknown0x2350);
		hw->unknown0x2350 = 0;
		break;
	case ISTATUS_0_STARTED:
	case ISTATUS_4_ASSOCIATED:
	default:
		break;
	}
	acx100_unlock(hw, &flags);
	FN_EXIT(0, 0);
}
