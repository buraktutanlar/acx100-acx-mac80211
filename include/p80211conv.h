/* include/p80211conv.h
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

#ifndef _LINUX_P80211CONV_H
#define _LINUX_P80211CONV_H

/*================================================================*/
/* Constants */

#define WLAN_ETHADDR_LEN	6
#define WLAN_IEEE_OUI_LEN	3

#define WLAN_ETHCONV_ENCAP	1
#define WLAN_ETHCONV_RFC1042	2
#define WLAN_ETHCONV_8021h	3

#define WLAN_MIN_ETHFRM_LEN	60
#define WLAN_MAX_ETHFRM_LEN	1518
#define WLAN_ETHHDR_LEN		18
	/* FIXME!!! These values differ between different wlan-ng 
	   versions!!! Make sure we use the ones our binary driver uses! */

#define P80211CAPTURE_VERSION	0x80211001

/*================================================================*/
/* Macros */


/*================================================================*/
/* Types */

/*
 * Frame capture header.  (See doc/capturefrm.txt)
 */
__WLAN_PRAGMA_PACK1__ typedef struct p80211_caphdr {
	UINT32 version __WLAN_ATTRIB_PACK__;
	UINT32 length __WLAN_ATTRIB_PACK__;
	UINT64 mactime __WLAN_ATTRIB_PACK__;
	UINT64 hosttime __WLAN_ATTRIB_PACK__;
	UINT32 phytype __WLAN_ATTRIB_PACK__;
	UINT32 channel __WLAN_ATTRIB_PACK__;
	UINT32 datarate __WLAN_ATTRIB_PACK__;
	UINT32 antenna __WLAN_ATTRIB_PACK__;
	UINT32 priority __WLAN_ATTRIB_PACK__;
	UINT32 ssi_type __WLAN_ATTRIB_PACK__;
	INT32 ssi_signal __WLAN_ATTRIB_PACK__;
	INT32 ssi_noise __WLAN_ATTRIB_PACK__;
	UINT32 preamble __WLAN_ATTRIB_PACK__;
	UINT32 encoding __WLAN_ATTRIB_PACK__;
} p80211_caphdr_t;
__WLAN_PRAGMA_PACKDFLT__
/* buffer free method pointer type */
typedef void (*freebuf_method_t) (void *buf, int size);

typedef struct p80211_metawep {
	char *data;
	UINT8 iv[4];
	UINT8 icv[4];
} p80211_metawep_t;

/* local ether header type */
typedef struct wlan_ethhdr {
	UINT8 daddr[WLAN_ETHADDR_LEN] __WLAN_ATTRIB_PACK__;
	UINT8 saddr[WLAN_ETHADDR_LEN] __WLAN_ATTRIB_PACK__;
	UINT16 type __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ wlan_ethhdr_t;

/* local llc header type */
typedef struct wlan_llc {
	UINT8 dsap __WLAN_ATTRIB_PACK__;
	UINT8 ssap __WLAN_ATTRIB_PACK__;
	UINT8 ctl __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ wlan_llc_t;

/* local snap header type */
typedef struct wlan_snap {
	UINT8 oui[WLAN_IEEE_OUI_LEN] __WLAN_ATTRIB_PACK__;
	UINT16 type __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ wlan_snap_t;

typedef struct wlan_pb {
	void (*ethfree) (struct sk_buff * skb);	/* 00 */
	unsigned short ethbuflen;	/* 04, using a wlan-ng name here! */
	unsigned short ethfrmlen;	/* 06 */
	wlan_ethhdr_t *eth_hdr;	/* 08, using a wlan-ng name here! */
	wlan_llc_t *eth_llc;	/* 0x0c, using a wlan-ng name here! */
	wlan_snap_t *eth_snap;	/* 0x10, using a wlan-ng name here! */
	UINT8 *eth_payload;	/* 0x14, using a wlan-ng name here! */
	unsigned char *ethbuf;	/* 0x18, using a wlan-ng name here! */
	UINT eth_payloadlen;	/* 0x1c, using a wlan-ng name here! */
	void (*p80211free) (void *);	/* 0x20; using a wlan-ng name here! */
	UINT16 p80211buflen;	/* 0x24, using a wlan-ng name here! */
	UINT16 p80211frmlen;	/* 0x26, using a wlan-ng name here! correct? */
	p80211_hdr_t *p80211_hdr;	/* 0x28, using a wlan-ng name here! */
	wlan_llc_t *p80211_llc;	/* 0x2c, using a wlan-ng name here! */
	wlan_snap_t *p80211_snap;	/* 0x30, using a wlan-ng name here! */
	UINT8 *p80211buf;	/* 0x34, using a wlan-ng name here! */
	UINT8 *p80211_payload;	/* 0x38, using a wlan-ng name! */
	UINT p80211_payloadlen;	/* 0x3c, using a wlan-ng name here! */
	UINT val0x40;		//spacer
	UINT val0x44;		//spacer
	struct sk_buff *ethhostbuf;	/* 0x48 */
	struct acx100_addr3 *p80211hostbuf;	/* 0x4c, using a wlan-ng name here! */
} wlan_pb_t;

/* Circular include trick */
struct wlandevice;
struct p80211;
struct p80211pb;

/*================================================================*/
/* Externs */

/*================================================================*/
/* Function Declarations */


int p80211pb_ether_to_p80211(struct wlandevice *wlandev,
			     struct wlan_pb *pb);
int p80211_stt_findproto(unsigned int proto);
int p80211_stt_addproto(UINT16 proto);

struct wlan_pb *p80211pb_alloc(void);
void p80211pb_freeskb(struct sk_buff *skb);
void p80211pb_free(struct wlan_pb *skb);

int p80211pb_p80211_to_ether(struct wlandevice *wlandev,
			     struct wlan_pb *pb);
void p80211pb_kfree_s(void *arg);
struct txdescriptor;
struct rxhostdescriptor;

int acx100_ether_to_txdesc(struct wlandevice *wlandev,struct txdescriptor *txdesc,struct sk_buff *skb);
struct sk_buff* acx100_rxdesc_to_ether(struct wlandevice *wlandev,struct rxhostdescriptor *rxdesc);
void acx100_rxdesc_to_txdesc(struct rxhostdescriptor *rxdesc,struct txdescriptor *txdesc);
#endif
