/* src/p80211conv.c - conversion between 802.11 and ethernet
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
 * This code is based on elements which are
 * Copyright (C) 1999 AbsoluteValue Systems, Inc.  All Rights Reserved.
 * info@linux-wlan.com
 * http://www.linux-wlan.com
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
#define WLAN_DBVAR	prism2_debug
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

#include <wlan_compat.h>

#include <linux/ioport.h>
#include <linux/pci.h>

#include <asm/pci.h>
#include <linux/dcache.h>
#include <linux/highmem.h>
#include <linux/sched.h>
#include <linux/skbuff.h>
#include <linux/etherdevice.h>
#include <version.h>
#include <p80211hdr.h>
#include <p80211mgmt.h>
#include <p80211msg.h>
#include <p80211ioctl.h>
#include <acx100.h>
#include <p80211conv.h>
#include <p80211netdev.h>
#include <p80211req.h>
#include <p80211types.h>
#include <acx100_helper.h>
#include <acx100_helper2.h>
#include <idma.h>
#include <ihw.h>
#include <acx100mgmt.h>


static UINT8 oui_rfc1042[] = { 0x00, 0x00, 0x00 };
static UINT8 oui_8021h[] = { 0x00, 0x00, 0xf8 };
/*----------------------------------------------------------------
* acx100_rxdesc_to_txdesc
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



void acx100_rxdesc_to_txdesc(struct rxhostdescriptor *rxdesc,
				struct txdescriptor *txdesc){
	struct txhostdescriptor *payload;
	struct txhostdescriptor *header;
	payload = txdesc->val0x1c + 1;
	header = txdesc->val0x1c;
	payload->val0x4 = 0;
	header->val0x4 = 0;
	memcpy(header->data,&rxdesc->pThisBuf->buf,WLAN_HDR_A3_LEN);
	memcpy(payload->data,&rxdesc->pThisBuf->val0x24, 
		rxdesc->pThisBuf->status - WLAN_HDR_A3_LEN);

}

/*----------------------------------------------------------------
* acx100_ether_to_txdesc
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

int acx100_ether_to_txdesc(wlandevice_t * hw,
			 struct txdescriptor * tx_desc /* ebx */ ,
			 struct sk_buff *skb)
{
	UINT16 proto;		/* protocol type or data length, depending on whether DIX or 802.3 ethernet format */
	UINT16 fc;
	struct txhostdescriptor *payload;
	struct txhostdescriptor *header;
	p80211_hdr_t* hdr;
	struct wlan_llc *llc;
	struct wlan_snap *snap;
	UINT8 *a1 = NULL;
	UINT8 *a2 = NULL;
	UINT8 *a3 = NULL;
//	int i;

	FN_ENTER;

	payload = tx_desc->val0x1c + 1;
	header = tx_desc->val0x1c;
	proto = ntohs(((wlan_ethhdr_t*)skb->data)->type);
	if (proto <= 1500) {
		/* codes <= 1500 reserved for 802.3 lengths */
		/* it's 802.3, pass ether payload unchanged,  */
		/*   leave off any PAD octets.  */

		acxlog(L_DEBUG, "<= 1500\n");

		payload->length = ntohs(((wlan_ethhdr_t*)skb->data)->type);
		
		memcpy(payload->data, skb->data+sizeof(wlan_ethhdr_t), payload->length);
	} else {
		/* it's DIXII, time for some conversion */
		/* Create 802.11 packet. Header also contains llc and snap. */

		acxlog(L_DEBUG, "<= DIXII\n");

		/* size of header is 802.11 header + llc + snap */
		header->length =
		    WLAN_HDR_A3_LEN + sizeof(wlan_llc_t) +
		    sizeof(wlan_snap_t);
		llc = (wlan_llc_t*)(header->data +  WLAN_HDR_A3_LEN);
		snap = (wlan_snap_t*)(header->data + WLAN_HDR_A3_LEN
			+ sizeof(wlan_llc_t));
		/* setup the LLC header */
		llc->dsap = 0xAA;	/* SNAP, see IEEE 802 */
		llc->ssap = 0xAA;
		llc->ctl = 0x03;

		/* setup the SNAP header */
		snap->type = htons(proto);
		if (p80211_stt_findproto(proto)) {
			memcpy(snap->oui, oui_8021h,
			       WLAN_IEEE_OUI_LEN);
		} else {
			memcpy(snap->oui, oui_rfc1042,
			       WLAN_IEEE_OUI_LEN);
		}

		payload->length = skb->len - sizeof(wlan_ethhdr_t);
		memcpy(payload->data, skb->data + sizeof(wlan_ethhdr_t),
			payload->length);
	}
	payload->val0x4 = 0;
	header->val0x4 = 0;
	tx_desc->total_length = payload->length + header->length;
	/* Set up the 802.11 header */
	/* It's a data frame */
	hdr = (p80211_hdr_t*)header->data;
	fc = host2ieee16(WLAN_SET_FC_FTYPE(WLAN_FTYPE_DATA) |
			 WLAN_SET_FC_FSTYPE(WLAN_FSTYPE_DATAONLY));
	hdr->a3.dur = 0;
	hdr->a3.seq = 0;

	acxlog(L_DEBUG, "MODE: %ld\n", hw->macmode);
	switch (hw->macmode) {
	case WLAN_MACMODE_NONE:	/* 0 */
		a1 = ((wlan_ethhdr_t*)skb->data)->daddr;
		a2 = ((wlan_ethhdr_t*)skb->data)->saddr;
		a3 = hw->bssid;
		break;
	case WLAN_MACMODE_ESS_STA:	/* 2 */
		fc |= host2ieee16(WLAN_SET_FC_TODS(1));
		a1 = hw->bssid;	/*bssid */
		a2 = ((wlan_ethhdr_t*)skb->data)->saddr;	/*src */
		a3 = ((wlan_ethhdr_t*)skb->data)->daddr;	/*dest */
		break;
	case WLAN_MACMODE_ESS_AP:	/* 3 */
		fc |= host2ieee16(WLAN_SET_FC_FROMDS(1));
		a1 = ((wlan_ethhdr_t*)skb->data)->daddr;
		a2 = hw->bssid;
		a3 = ((wlan_ethhdr_t*)skb->data)->saddr;
		break;
	default:
		acxlog(L_DEBUG,
		       "Error: Converting eth to wlan in unknown mode.\n");
		/* fall through */
	case WLAN_MACMODE_IBSS_STA:
		a1 = NULL;
		a2 = NULL;
		a3 = NULL;
		break;
	}
	hdr->a3.fc = fc;
	memcpy(hdr->a3.a1, a1, WLAN_ADDR_LEN);
	memcpy(hdr->a3.a2, a2, WLAN_ADDR_LEN);
	memcpy(hdr->a3.a3, a3, WLAN_ADDR_LEN);

	/* the "<6>" output is from the KERN_INFO channel value */
// Can be used to debug conversion process
/*	acxlog(L_DATA, "Original eth frame [%d]: ", skb->len);
	for (i = 0; i < skb->len; i++)
		acxlog(L_DATA, "%02x ", ((UINT8 *) skb->data)[i]);
	acxlog(L_DATA, "\n");

	acxlog(L_DATA, "802.11 header [%d]: ", header->length);
	for (i = 0; i < header->length; i++)
		acxlog(L_DATA, "%02x ", ((UINT8 *) header->data)[i]);
	acxlog(L_DATA, "\n");

	acxlog(L_DATA, "802.11 payload [%d]: ", payload->length);
	for (i = 0; i < payload->length; i++)
		acxlog(L_DATA, "%02x ", ((UINT8 *) payload->data)[i]);
	acxlog(L_DATA, "\n");
*/
	FN_EXIT(0, 0);
	return 0;
}

/*----------------------------------------------------------------
* acx100_rxdesc_to_ether
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

struct sk_buff *acx100_rxdesc_to_ether(wlandevice_t * hw, struct
		rxhostdescriptor * rx_desc)
{
	UINT8 *daddr = NULL;
	UINT8 *saddr = NULL;
	UINT8 *payload = NULL;
	int some_wep_calc_offs;	/* some added frame length (4) in case of WEP; is this for conditional WLAN_CRC_LEN inclusion? */
	int p80211frmlen;
	int p80211_payloadlen;
	wlan_ethhdr_t *ethhdr;
	wlan_llc_t *p80211_llc;
	wlan_snap_t *p80211_snap;
	struct sk_buff *skb = NULL;
	p80211_hdr_t *hdr;
	UINT llclen;		/* 802 LLC+data length */
	UINT buflen;		/* full frame length, including PAD */
	UINT16 fc;
//	int i;
#if DO_SOFTWARE_WEP
	UINT payload_length;
#endif

	FN_ENTER;
	acxlog(L_STATE, "%s: UNVERIFIED.\n", __func__);

#if DO_SOFTWARE_WEP
	payload_length = pb->len - WLAN_HDR_A3_LEN - WLAN_CRC_LEN;
#endif
	hdr = (p80211_hdr_t*)&rx_desc->pThisBuf->buf;
	if (hw->rx_config_1 & 0x2){
		hdr = (p80211_hdr_t*)(((UINT8*)hdr)+ 0x4);
	}

	/* setup some vars for convenience */
	fc = ieee2host16(hdr->a3.fc);
	some_wep_calc_offs = WLAN_GET_FC_ISWEP(fc) << 2;
	if ((WLAN_GET_FC_TODS(fc) == 0) && (WLAN_GET_FC_FROMDS(fc) == 0)) {
		daddr = hdr->a3.a1;
		saddr = hdr->a3.a2;
	} else if ((WLAN_GET_FC_TODS(fc) == 0)
		   && (WLAN_GET_FC_FROMDS(fc) == 1)) {
		daddr = hdr->a3.a1;
		saddr = hdr->a3.a3;
	} else if ((WLAN_GET_FC_TODS(fc) == 1)
		   && (WLAN_GET_FC_FROMDS(fc) == 0)) {
		daddr = hdr->a3.a3;
		saddr = hdr->a3.a2;
	} else {
		acxlog(L_DEBUG,
		       "HDR_A4 detected! A4 currently not supported\n");
		/* set some bogus pointers so at least we won't crash */
		daddr = hdr->a3.a1;
		saddr = hdr->a3.a2;
	}
#if DO_SOFTWARE_WEP
	/* perform de-wep if necessary.. */
	if ((hw->hostwep & HOSTWEP_PRIVACYINVOKED)
	    && WLAN_GET_FC_ISWEP(fc)
	    && (hw->hostwep & HOSTWEP_DECRYPT)) {

		if ((foo =
		     wep_decrypt(hw, pb->data + WLAN_HDR_A3_LEN + 4,
				 payload_length - 8, -1,
				 pb->data + WLAN_HDR_A3_LEN,
				 pb->data + WLAN_HDR_A3_LEN +
				 payload_length - 4))) {
			/* de-wep failed, drop pb. */
			acxlog(L_DEBUG,
			       "Host de-WEP failed, dropping frame (%d)\n",
			       foo);
			hw->rx.decrypt_err++;
			return 2;
		}
		/* subtract the IV+ICV length off the payload */
		payload_length -= 8;
		/* chop off the IV */
		//pb_pull(pb, 4);
		/* chop off the ICV. */
		//pb_trim(pb, pb->len - 4);
		hw->rx.decrypt++;
	}
#endif

	/* The frame is preprocessed by dmaRxXfrISR. The 802.11 header is in */
	/* p80211_hdr, but the LLC and SNAP are in the p80211_payload. */
	/* Nevermind, let's do it here */
	p80211_payloadlen = rx_desc->pThisBuf->status&0xfff;
	if (WLAN_GET_FC_ISWEP(rx_desc->pThisBuf->buf.frame_control)){
		if(WLAN_GET_FC_FTYPE(rx_desc->pThisBuf->buf.frame_control) == WLAN_FTYPE_DATA) {
			p80211frmlen = p80211_payloadlen - 8;
			p80211_payloadlen -= 0x20;
		} else {
			p80211frmlen = p80211_payloadlen - 0x10;
			p80211_payloadlen -= 0x1c;
		}
	} else {
		p80211frmlen = p80211_payloadlen;
		p80211_payloadlen -= 0x18;
	}

	if (hw->rx_config_1 & 0x2){
		p80211_payloadlen -= 4;
	}		

	if (WLAN_GET_FC_ISWEP(fc)){
		if (WLAN_GET_FC_TODS(fc) &&
			WLAN_GET_FC_FROMDS(fc))
			{
				payload = (UINT8*)hdr + 0x20;
			} else {
				payload = (UINT8*)hdr + 0x1c;
			}
	} else {
		payload = (UINT8*)hdr + WLAN_HDR_A3_LEN /* 0x18 */;
	}
	
	/* HU, what is this ethhdr stuff ?? It is used for ethhdr->daddr but it points to the LLC or the start of the payload. */
	ethhdr = (wlan_ethhdr_t *) (payload);
	/* LLC is at start of payload */
	p80211_llc = (wlan_llc_t *) (payload);
	/* then there's the SNAP */
	p80211_snap = (wlan_snap_t *) (((UINT8 *) p80211_llc) +
			     sizeof(wlan_llc_t));

	acxlog(L_XFER | L_DATA,
	       "Frame info: llc.dsap %X, llc.ssap %X, llc.ctl %X, snap.oui %X%X%X, snap.type %X\n",
	       p80211_llc->dsap, p80211_llc->ssap,
	       p80211_llc->ctl, p80211_snap->oui[0],
	       p80211_snap->oui[1], p80211_snap->oui[2],
	       p80211_snap->type);

	/* Test for the various encodings */
	if (memcmp(daddr, ethhdr->daddr, WLAN_ETHADDR_LEN) == 0 &&
	    memcmp(saddr, ethhdr->saddr, WLAN_ETHADDR_LEN) == 0) {
		/* ENCAP */
		/* Test for an overlength frame */
		acxlog(L_DEBUG | L_DATA, "ENCAP content?\n");
		if (p80211frmlen >
		    WLAN_HDR_A3_LEN + WLAN_CRC_LEN + WLAN_MAX_ETHFRM_LEN) {
			/* A bogus length ethfrm has been encap'd. */
			/* Is someone trying an oflow attack? */
			acxlog(L_DEBUG, "frame too large!\n");
			return NULL;
		}

		/* allocate space and setup host buffer */
		buflen = llclen =
		    p80211frmlen - WLAN_HDR_A3_LEN -
		    some_wep_calc_offs;
		skb = dev_alloc_skb(buflen + 2);	/* +2 is attempt to align IP header */

		if (skb == NULL)
			return skb;
		skb_reserve(skb, 2);
		skb_put(((struct sk_buff *) skb), buflen);	/* make room */

		/* now copy the data from the 80211 frame */
		memcpy(skb->data, payload, buflen);	/* copy the data */
	} else if (p80211_llc->dsap == 0xaa && p80211_llc->ssap == 0xaa && p80211_llc->ctl == 0x03) {	//&&
//
// These test are wrong, look at linux-wlan for correct implementation
//
//                 (memcmp( pb->p80211_snap->oui,
//                         oui_8021h, WLAN_IEEE_OUI_LEN) == 0 ||
//                  ((memcmp( pb->p80211_snap->oui,
//                         oui_rfc1042, WLAN_IEEE_OUI_LEN) == 0) &&
//
//p80211_stt_findproto(ieee2host16(pb->p80211_snap->type)))) ) {

		acxlog(L_DEBUG | L_DATA, "802.1h or RFC1042 content.\n");

		/* Test for an overlength frame */
		if (p80211_payloadlen - 8 + 0xe > 1518) {	/* FIXME */
			/* A bogus length ethfrm has been sent. */
			/* Is someone trying an oflow attack? */
			acxlog(L_STD, "SNAP frame too large (1518) %d\n",
			       p80211_payloadlen - 8 + 0xe);
			return NULL;
		}

		/* FIXME: this used to be pb->p80211_payloadlen - 8,
		 * but it distorted some web traffic.
		 * Changing to - 4 improved it and worked for both
		 * Managed and Ad-Hoc, but I didn't test WEP.
		 */
		llclen = p80211_payloadlen - 4;
		buflen = llclen + 14;	//wlan_max( llclen + sizeof(wlan_ethhdr_t), WLAN_MIN_ETHFRM_LEN);
		skb = dev_alloc_skb(buflen + 2);	/* +2 is attempt to align IP header */
		if (skb == NULL) {
			acxlog(L_STD, "failed to allocate skb\n");
			return NULL;
		}
		skb_reserve(skb, 2);
		skb_put(skb, buflen);
		/* make room */
		ethhdr = (wlan_ethhdr_t *) skb->data;

		p80211_llc = (wlan_llc_t *) payload;
		p80211_snap =		    (wlan_snap_t *) (((UINT8 *) p80211_llc) +
				     sizeof(wlan_llc_t));

		/* set up the DIXII header */
		ethhdr->type = p80211_snap->type;
		memcpy(ethhdr->daddr, daddr, WLAN_ETHADDR_LEN);
		memcpy(ethhdr->saddr, saddr, WLAN_ETHADDR_LEN);

		/* now copy the data from the 80211 frame */
		/* + 14 to skip the eth header */
		/* + 8 to skip llc and snap */
		memcpy(skb->data+14, payload + 8, llclen);	/* copy the data */
	} else {
		/* any NON-ENCAP */
		/* it's a generic 80211+LLC or IPX 'Raw 802.3' */
		/*  build an 802.3 frame */
		/* allocate space and setup hostbuf */

		acxlog(L_DEBUG | L_DATA, "NON-ENCAP content.\n");
		/* Test for an overlength frame */
		if (p80211_payloadlen + WLAN_ETHHDR_LEN >
		    WLAN_MAX_ETHFRM_LEN) {
			/* A bogus length ethfrm has been sent. */
			/* Is someone trying an oflow attack? */
			return NULL;
		}

		llclen = p80211_payloadlen;
		buflen =
		    wlan_max(llclen + sizeof(wlan_ethhdr_t),
			     WLAN_MIN_ETHFRM_LEN);
		skb = dev_alloc_skb(buflen + 2);	/* +2 is attempt to align IP header */
		if (skb == NULL)
			return skb;
		skb_reserve((struct sk_buff *) skb, 2);
		skb_put(skb, buflen);	/* make room */

		/* set up the pointers */
		ethhdr = (wlan_ethhdr_t *) skb->data;

		/* set up the 802.3 header */
		ethhdr->type = htons(llclen);
		memcpy(ethhdr->daddr, daddr, WLAN_ETHADDR_LEN);
		memcpy(ethhdr->saddr, saddr, WLAN_ETHADDR_LEN);
		/* now copy the data from the 80211 frame */
		memcpy((UINT8*)skb->data + sizeof(wlan_ethhdr_t),
			payload, p80211_payloadlen);
	}
	/* the "<6>" output is from the KERN_INFO channel value */
// Can be used to debug conversion process
/*	acxlog(L_DATA, "p802.11 header [%d]: ", p80211frmlen-p80211_payloadlen);
	for (i = 0; i < p80211frmlen-p80211_payloadlen; i++)
		acxlog(L_DATA, "%02x ", ((UINT8 *) hdr)[i]);
	acxlog(L_DATA, "\n");

	acxlog(L_DATA, "p802.11 payload [%d]: ", p80211_payloadlen);
	for (i = 0; i < p80211_payloadlen; i++)
		acxlog(L_DATA, "%02x ", ((UINT8 *) payload)[i]);
	acxlog(L_DATA, "\n");

	acxlog(L_DATA, "eth frame [%d]: ", skb->len);
	for (i = 0; i < skb->len; i++)
		acxlog(L_DATA, "%02x ", ((UINT8 *) skb->data)[i]);
	acxlog(L_DATA, "\n");
*/
	FN_EXIT(0, 0);
	return skb;
}
/*----------------------------------------------------------------
* p80211_stt_findproto
* FIXME: rename to acx100_stt_findproto
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

int p80211_stt_findproto(unsigned int prottype)
{
	return ((prottype == ETH_P_AARP) || (prottype == ETH_P_IPX));
}


