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

#include <wlan_compat.h>

#include <linux/ioport.h>
#include <linux/pci.h>

#include <linux/dcache.h>
#include <linux/highmem.h>
#include <linux/sched.h>
#include <linux/skbuff.h>
#include <linux/etherdevice.h>
#include <version.h>

#include <p80211hdr.h>
#include <p80211mgmt.h>
#include <acx100.h>
#include <acx100_conv.h>
#include <p80211types.h>
#include <acx100_helper.h>
#include <acx100_helper2.h>
#include <idma.h>
#include <ihw.h>

static UINT8 oui_rfc1042[] = { 0x00, 0x00, 0x00 };
static UINT8 oui_8021h[] = { 0x00, 0x00, 0xf8 };

/*----------------------------------------------------------------
* acx100_rxdesc_to_txdesc
*
* Converts a rx descriptor to a tx descriptor.
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
*	FINISHED
*
* Comment:
*
*----------------------------------------------------------------*/

void acx100_rxdesc_to_txdesc(struct rxhostdescriptor *rxdesc,
				struct txdescriptor *txdesc){
	struct txhostdescriptor *payload;
	struct txhostdescriptor *header;
	
	payload = txdesc->host_desc + 1;
	header = txdesc->host_desc;
	
	payload->val0x4 = 0;
	header->val0x4 = 0;
	
	memcpy(header->data, &rxdesc->data->buf, WLAN_HDR_A3_LEN);
	memcpy(payload->data, &rxdesc->data->val0x24, 
		rxdesc->data->status - WLAN_HDR_A3_LEN);

}

/*----------------------------------------------------------------
* acx100_ether_to_txdesc
*
* Uses the contents of the ether frame to build the elements of 
* the 802.11 frame.
*
* We don't actually set up the frame header here.  That's the 
* MAC's job.  We're only handling conversion of DIXII or 802.3+LLC 
* frames to something that works with 802.11.
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
*	FINISHED
*
* Comment: 
*	Based largely on p80211conv.c of the linux-wlan-ng project
*
*----------------------------------------------------------------*/

int acx100_ether_to_txdesc(wlandevice_t * wlandev,
			 struct txdescriptor * tx_desc,
			 struct sk_buff *skb)
{
	UINT16 proto;		/* protocol type or data length, depending on whether DIX or 802.3 ethernet format */
	UINT16 fc;
	struct txhostdescriptor *payload;
	struct txhostdescriptor *header;
	p80211_hdr_t * w_hdr;

	wlan_ethhdr_t *e_hdr; 
	struct wlan_llc *e_llc;
	struct wlan_snap *e_snap;

	UINT8 *a1 = NULL;
	UINT8 *a2 = NULL;
	UINT8 *a3 = NULL;
	
//	int i;

	FN_ENTER;

	payload = tx_desc->host_desc + 1;
	header = tx_desc->host_desc;
	e_hdr = (wlan_ethhdr_t *)skb->data;

	if (skb->len <= 0) {
		acxlog(L_DEBUG, "zero-length skb!\n");
		return 1;
	}

	/* step 1: classify ether frame, DIX or 802.3? */
	proto = ntohs(e_hdr->type);
	if (proto <= 1500) {
	        acxlog(L_DEBUG, "802.3 len: %d\n", skb->len);
                /* codes <= 1500 reserved for 802.3 lengths */
		/* it's 802.3, pass ether payload unchanged,  */

		/* trim off ethernet header and copy payload to tx_desc */
		payload->length = proto;
		memcpy(payload->data, skb->data + sizeof(wlan_ethhdr_t), payload->length);
	} else {
		/* it's DIXII, time for some conversion */
		/* Create 802.11 packet. Header also contains llc and snap. */

		acxlog(L_DEBUG, "<= DIXII len: %d\n", skb->len);

		/* size of header is 802.11 header + llc + snap */
		header->length = WLAN_HDR_A3_LEN + sizeof(wlan_llc_t) + sizeof(wlan_snap_t);
		/* llc is located behind the 802.11 header */
		e_llc = (wlan_llc_t*)(header->data +  WLAN_HDR_A3_LEN);
		/* snap is located behind the llc */
		e_snap = (wlan_snap_t*)((UINT8*)e_llc + sizeof(wlan_llc_t));
			
		/* setup the LLC header */
		e_llc->dsap = 0xAA;	/* SNAP, see IEEE 802 */
		e_llc->ssap = 0xAA;
		e_llc->ctl = 0x03;

		/* setup the SNAP header */
		e_snap->type = htons(proto);
		if (acx100_stt_findproto(proto)) {
			memcpy(e_snap->oui, oui_8021h, WLAN_IEEE_OUI_LEN);
		} else {
			memcpy(e_snap->oui, oui_rfc1042, WLAN_IEEE_OUI_LEN);
		}


		/* trim off ethernet header and copy payload to tx_desc */
		payload->length = skb->len - sizeof(wlan_ethhdr_t);
		memcpy(payload->data, skb->data + sizeof(wlan_ethhdr_t), payload->length);
	}
	
	/* FIXME: find out what these fields stand for */
	payload->val0x4 = 0;
	header->val0x4 = 0;
	
	/* calculate total tx_desc length */
	tx_desc->total_length = payload->length + header->length;

	/* Set up the 802.11 header */
	w_hdr = (p80211_hdr_t*)header->data;

	/* It's a data frame */
	fc = host2ieee16(WLAN_SET_FC_FTYPE(WLAN_FTYPE_DATA) |
			 WLAN_SET_FC_FSTYPE(WLAN_FSTYPE_DATAONLY));

	acxlog(L_XFER | L_DATA, "MODE: %ld\n", wlandev->macmode);
	switch (wlandev->macmode) {
	case WLAN_MACMODE_NONE:		/* 0 */
	case WLAN_MACMODE_IBSS_STA:	/* 1 */
		a1 = e_hdr->daddr;
		a2 = wlandev->netdev->dev_addr;
		a3 = wlandev->bssid;
		break;
	case WLAN_MACMODE_ESS_STA:	/* 2 */
		fc |= host2ieee16(WLAN_SET_FC_TODS(1));
		a1 = wlandev->bssid;
		a2 = wlandev->netdev->dev_addr;
		a3 = e_hdr->daddr;
		break;
	case WLAN_MACMODE_ESS_AP:	/* 3 */
		fc |= host2ieee16(WLAN_SET_FC_FROMDS(1));
		a1 = e_hdr->daddr;
		a2 = wlandev->bssid;
		a3 = e_hdr->saddr;
		break;
	default:			/* fall through */
		acxlog(L_DEBUG, "Error: Converting eth to wlan in unknown mode.\n");
	}
	memcpy(w_hdr->a3.a1, a1, WLAN_ADDR_LEN);
	memcpy(w_hdr->a3.a2, a2, WLAN_ADDR_LEN);
	memcpy(w_hdr->a3.a3, a3, WLAN_ADDR_LEN);

	if (wlandev->wep_enabled)
		fc |= host2ieee16(WLAN_SET_FC_ISWEP(1));
		
	w_hdr->a3.fc = fc;
	w_hdr->a3.dur = 0;
	w_hdr->a3.seq = 0;

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
* Uses the contents of a received 802.11 frame to build an ether 
* frame.
*
* This function extracts the src and dest address from the 802.11
* frame to use in the construction of the eth frame.
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
*	FINISHED
*
* Comment:  
*	Based largely on p80211conv.c of the linux-wlan-ng project
*
*----------------------------------------------------------------*/

struct sk_buff *acx100_rxdesc_to_ether(wlandevice_t * wlandev, struct
		rxhostdescriptor * rx_desc)
{
	UINT8 *daddr = NULL;
	UINT8 *saddr = NULL;
	wlan_ethhdr_t *e_hdr;
	wlan_llc_t *e_llc;
	wlan_snap_t *e_snap;
	UINT8 *e_payload;
	struct sk_buff *skb = NULL;
	p80211_hdr_t *w_hdr;
	UINT buflen;
	UINT16 fc;
	UINT payload_length;
	UINT payload_offset;

//	int i;

	FN_ENTER;

	payload_length = (rx_desc->data->status & 0xfff) - WLAN_HDR_A3_LEN;
	payload_offset = WLAN_HDR_A3_LEN;

	w_hdr = (p80211_hdr_t*)&rx_desc->data->buf;
	
	/* check if additional header is included */
	if (wlandev->rx_config_1 & RX_CFG1_INCLUDE_ADDIT_HDR) {
		/* Mmm, strange, when receiving a packet, 4 bytes precede the packet. Is it the CRC ? */
		w_hdr = (p80211_hdr_t*)(((UINT8*)w_hdr) + WLAN_CRC_LEN);
		payload_length -= WLAN_CRC_LEN;
	}

	/* setup some vars for convenience */
	fc = ieee2host16(w_hdr->a3.fc);
	if ((WLAN_GET_FC_TODS(fc) == 0) && (WLAN_GET_FC_FROMDS(fc) == 0)) {
		daddr = w_hdr->a3.a1;
		saddr = w_hdr->a3.a2;
	} else if ((WLAN_GET_FC_TODS(fc) == 0) && (WLAN_GET_FC_FROMDS(fc) == 1)) {
		daddr = w_hdr->a3.a1;
		saddr = w_hdr->a3.a3;
	} else if ((WLAN_GET_FC_TODS(fc) == 1) && (WLAN_GET_FC_FROMDS(fc) == 0)) {
		daddr = w_hdr->a3.a3;
		saddr = w_hdr->a3.a2;
	} else {
		payload_offset = WLAN_HDR_A4_LEN;
		payload_length -= ( WLAN_HDR_A4_LEN - WLAN_HDR_A3_LEN );
		if (payload_length < 0 ) {
			acxlog(L_STD, "A4 frame too short!\n");
			return NULL;
		}
		daddr = w_hdr->a4.a3;
		saddr = w_hdr->a4.a4;
	}
	
	if (WLAN_GET_FC_ISWEP(fc)){
		/* chop off the IV+ICV WEP header and footer */
		acxlog(L_DATA | L_DEBUG, "It's a WEP packet, chopping off IV and ICV.\n");
		payload_length -= 8;
		payload_offset += 4;
	}
	
	e_hdr = (wlan_ethhdr_t *) ((UINT8*) w_hdr + payload_offset);

	e_llc = (wlan_llc_t *) ((UINT8*) w_hdr + payload_offset);
	e_snap = (wlan_snap_t *) (((UINT8 *) e_llc) + sizeof(wlan_llc_t));
	e_payload = ((UINT8 *) e_snap) + sizeof(wlan_snap_t);

	acxlog(L_DATA, "payload_offset %i, payload_length %i\n", payload_offset, payload_length);
	acxlog(L_XFER | L_DATA,
	       "Frame info: llc.dsap %X, llc.ssap %X, llc.ctl %X, snap.oui %X%X%X, snap.type %X\n",
	       e_llc->dsap, e_llc->ssap,
	       e_llc->ctl, e_snap->oui[0],
	       e_snap->oui[1], e_snap->oui[2],
	       e_snap->type);

	/* Test for the various encodings */
	if ( (payload_length >= sizeof(wlan_ethhdr_t)) &&
	     ( e_llc->dsap != 0xaa || e_llc->ssap != 0xaa ) &&
	     ((memcmp(daddr, e_hdr->daddr, WLAN_ETHADDR_LEN) == 0) ||
	     (memcmp(saddr, e_hdr->saddr, WLAN_ETHADDR_LEN) == 0))) {
		acxlog(L_DEBUG | L_DATA, "802.3 ENCAP len: %d\n", payload_length);
		/* 802.3 Encapsulated */
		/* Test for an overlength frame */

		if ( payload_length > WLAN_MAX_ETHFRM_LEN) {
			/* A bogus length ethfrm has been encap'd. */
			/* Is someone trying an oflow attack? */
			acxlog(L_STD, "ENCAP frame too large (%d > %d)\n", 
				payload_length, WLAN_MAX_ETHFRM_LEN);
			return NULL;
		}

		/* allocate space and setup host buffer */
		buflen = payload_length;
		skb = dev_alloc_skb(buflen + 2);	/* +2 is attempt to align IP header */
		if (skb == NULL) {
			acxlog(L_STD, "failed to allocate skb\n");
			return NULL;
		}
		skb_reserve(skb, 2);
		skb_put(skb, buflen);		/* make room */

		/* now copy the data from the 80211 frame */
		memcpy(skb->data, e_hdr, payload_length);	/* copy the data */

	} else if ((payload_length >= sizeof(wlan_llc_t) + sizeof(wlan_snap_t)) &&
		   (e_llc->dsap == 0xaa) &&
		   (e_llc->ssap == 0xaa) &&
		   (e_llc->ctl == 0x03) && 
		   (((memcmp( e_snap->oui, oui_rfc1042, WLAN_IEEE_OUI_LEN)==0) &&
//		    (ethconv == WLAN_ETHCONV_8021h) && 
		    (acx100_stt_findproto(ieee2host16(e_snap->type)))) || 
		    (memcmp( e_snap->oui, oui_rfc1042, WLAN_IEEE_OUI_LEN)!=0)))
	{
		acxlog(L_DEBUG | L_DATA, "SNAP+RFC1042 len: %d\n", payload_length);
		/* it's a SNAP + RFC1042 frame && protocol is in STT */
		/* build 802.3 + RFC1042 */

		/* Test for an overlength frame */
		if ( payload_length + WLAN_ETHHDR_LEN > WLAN_MAX_ETHFRM_LEN ) {
			/* A bogus length ethfrm has been sent. */
			/* Is someone trying an oflow attack? */
			acxlog(L_STD, "SNAP frame too large (%d > %d)\n", 
				payload_length, WLAN_MAX_ETHFRM_LEN);
			return NULL;
		}
		
		/* allocate space and setup host buffer */
		buflen = payload_length + WLAN_ETHHDR_LEN;
		skb = dev_alloc_skb(buflen + 2);	/* +2 is attempt to align IP header */
		if (skb == NULL) {
			acxlog(L_STD, "failed to allocate skb\n");
			return NULL;
		}
		skb_reserve(skb, 2);
		skb_put(skb, buflen);		/* make room */

		/* create 802.3 header */
		e_hdr = (wlan_ethhdr_t *) skb->data;
		memcpy(e_hdr->daddr, daddr, WLAN_ETHADDR_LEN);
		memcpy(e_hdr->saddr, saddr, WLAN_ETHADDR_LEN); 
		e_hdr->type = htons(payload_length);

		/* Now copy the data from the 80211 frame.
		   Make room in front for the eth header, and keep the 
		   llc and snap from the 802.11 payload */
		memcpy(skb->data + WLAN_ETHHDR_LEN, 
		       e_llc, 
		       payload_length);
		       
	}  else if ((payload_length >= sizeof(wlan_llc_t) + sizeof(wlan_snap_t)) &&
		    (e_llc->dsap == 0xaa) &&
		    (e_llc->ssap == 0xaa) &&
		    (e_llc->ctl == 0x03) ) {
		acxlog(L_DEBUG | L_DATA, "802.1h/RFC1042 len: %d\n", payload_length);
		/* it's an 802.1h frame || (an RFC1042 && protocol is not in STT) */
		/* build a DIXII + RFC894 */
		
		/* Test for an overlength frame */
		if ( payload_length - sizeof(wlan_llc_t) - sizeof(wlan_snap_t) + WLAN_ETHHDR_LEN > WLAN_MAX_ETHFRM_LEN) {
			/* A bogus length ethfrm has been sent. */
			/* Is someone trying an oflow attack? */
			acxlog(L_STD, "DIXII frame too large (%d > %d)\n",
					payload_length - sizeof(wlan_llc_t) - sizeof(wlan_snap_t),
					WLAN_MAX_ETHFRM_LEN - WLAN_ETHHDR_LEN);
			return NULL;
		}

		/* allocate space and setup host buffer */
		buflen = payload_length + WLAN_ETHHDR_LEN - sizeof(wlan_llc_t) - sizeof(wlan_snap_t);
		skb = dev_alloc_skb(buflen + 2);	/* +2 is attempt to align IP header */
		if (skb == NULL) {
			acxlog(L_STD, "failed to allocate skb\n");
			return NULL;
		}
		skb_reserve(skb, 2);
		skb_put(skb, buflen);		/* make room */

		/* create 802.3 header */
		e_hdr = (wlan_ethhdr_t *) skb->data;
		memcpy(e_hdr->daddr, daddr, WLAN_ETHADDR_LEN);
		memcpy(e_hdr->saddr, saddr, WLAN_ETHADDR_LEN); 
		e_hdr->type = e_snap->type;

		/* Now copy the data from the 80211 frame.
		   Make room in front for the eth header, and cut off the 
		   llc and snap from the 802.11 payload */
		memcpy(skb->data + WLAN_ETHHDR_LEN, 
		       e_payload, 
		       payload_length - sizeof(wlan_llc_t) - sizeof(wlan_snap_t));
	} else {
		acxlog(L_DEBUG | L_DATA, "NON-ENCAP len: %d\n", payload_length);
		/* any NON-ENCAP */
		/* it's a generic 80211+LLC or IPX 'Raw 802.3' */
		/*  build an 802.3 frame */
		/* allocate space and setup hostbuf */

		/* Test for an overlength frame */
		if (payload_length + WLAN_ETHHDR_LEN > WLAN_MAX_ETHFRM_LEN) {
			/* A bogus length ethfrm has been sent. */
			/* Is someone trying an oflow attack? */
			acxlog(L_STD, "OTHER frame too large (%d > %d)\n",
				payload_length,
				WLAN_MAX_ETHFRM_LEN - WLAN_ETHHDR_LEN);
			return NULL;
		}

		/* allocate space and setup host buffer */
		buflen = payload_length + WLAN_ETHHDR_LEN;
		skb = dev_alloc_skb(buflen + 2);	/* +2 is attempt to align IP header */
		if (skb == NULL) {
			acxlog(L_STD, "failed to allocate skb\n");
			return NULL;
		}
		skb_reserve(skb, 2);
		skb_put(skb, buflen);		/* make room */

		/* set up the 802.3 header */
		e_hdr = (wlan_ethhdr_t *) skb->data;
		memcpy(e_hdr->daddr, daddr, WLAN_ETHADDR_LEN);
		memcpy(e_hdr->saddr, saddr, WLAN_ETHADDR_LEN);
		e_hdr->type = htons(payload_length);
		
		/* now copy the data from the 80211 frame */
		memcpy(skb->data + WLAN_ETHHDR_LEN, e_llc, payload_length);
	}

//	skb->protocol = eth_type_trans(skb, wlandev->netdev);
	
	/* the "<6>" output is from the KERN_INFO channel value */
// Can be used to debug conversion process
/*	acxlog(L_DATA, "p802.11 frame [%d]: ", (rx_desc->data->status & 0xfff));
	for (i = 0; i < (rx_desc->data->status & 0xfff); i++)
		acxlog(L_DATA, "%02x ", ((UINT8 *) w_hdr)[i]);
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
* acx100_stt_findproto
*
* Searches the 802.1h Selective Translation Table for a given 
* protocol.
*
* Arguments:
*	prottype	protocl number (in host order) to search for.
*
* Returns:
*	1 - if the table is empty or a match is found.
*	0 - if the table is non-empty and a match is not found.
*
* Side effects:
*
* Call context:
*	May be called in interrupt or non-interrupt context
*
* STATUS:
*
* Comment:
*	Based largely on p80211conv.c of the linux-wlan-ng project
*
*----------------------------------------------------------------*/

int acx100_stt_findproto(unsigned int proto)
{
	/* Always return found for now.  This is the behavior used by the */
	/*  Zoom Win95 driver when 802.1h mode is selected */
	/* TODO: If necessary, add an actual search we'll probably
		 need this to match the CMAC's way of doing things.
		 Need to do some testing to confirm.
	*/

	if (proto == 0x80f3)  /* APPLETALK */
		return 1;

	return 0;	
//	return ((prottype == ETH_P_AARP) || (prottype == ETH_P_IPX));
}


