/* include/p80211hdr.h
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

/*================================================================*/
/* Constants */

/*--- Sizes -----------------------------------------------*/
#define WLAN_CRC_LEN			4
#define WLAN_BSS_TS_LEN			8
#define WLAN_HDR_A3_LEN			24
#define WLAN_HDR_A4_LEN			30
#define WLAN_SSID_MAXLEN		32
#define WLAN_DATA_MAXLEN		2312
#define WLAN_A3FR_MAXLEN		(WLAN_HDR_A3_LEN + WLAN_DATA_MAXLEN + WLAN_CRC_LEN)
#define WLAN_A4FR_MAXLEN		(WLAN_HDR_A4_LEN + WLAN_DATA_MAXLEN + WLAN_CRC_LEN)
#define WLAN_BEACON_FR_MAXLEN		(WLAN_HDR_A3_LEN + 334)
#define WLAN_ATIM_FR_MAXLEN		(WLAN_HDR_A3_LEN + 0)
#define WLAN_DISASSOC_FR_MAXLEN		(WLAN_HDR_A3_LEN + 2)
#define WLAN_ASSOCREQ_FR_MAXLEN		(WLAN_HDR_A3_LEN + 48)
#define WLAN_ASSOCRESP_FR_MAXLEN	(WLAN_HDR_A3_LEN + 16)
#define WLAN_REASSOCREQ_FR_MAXLEN	(WLAN_HDR_A3_LEN + 54)
#define WLAN_REASSOCRESP_FR_MAXLEN	(WLAN_HDR_A3_LEN + 16)
#define WLAN_PROBEREQ_FR_MAXLEN		(WLAN_HDR_A3_LEN + 44)
#define WLAN_PROBERESP_FR_MAXLEN	(WLAN_HDR_A3_LEN + 78)
#define WLAN_AUTHEN_FR_MAXLEN		(WLAN_HDR_A3_LEN + 261)
#define WLAN_DEAUTHEN_FR_MAXLEN		(WLAN_HDR_A3_LEN + 2)
#define WLAN_WEP_NKEYS			4
#define WLAN_WEP_MAXKEYLEN		13
#define WLAN_CHALLENGE_IE_LEN		130
#define WLAN_CHALLENGE_LEN		128
#define WLAN_WEP_IV_LEN			4
#define WLAN_WEP_ICV_LEN		4

/*--- Frame Control Field -------------------------------------*/
/* Frame Types */
#define WLAN_FTYPE_MGMT			0x00
#define WLAN_FTYPE_CTL			0x01
#define WLAN_FTYPE_DATA			0x02

/* Frame subtypes */
/* Management */
#define WLAN_FSTYPE_ASSOCREQ		0x00
#define WLAN_FSTYPE_ASSOCRESP		0x01
#define WLAN_FSTYPE_REASSOCREQ		0x02
#define WLAN_FSTYPE_REASSOCRESP		0x03
#define WLAN_FSTYPE_PROBEREQ		0x04
#define WLAN_FSTYPE_PROBERESP		0x05
#define WLAN_FSTYPE_BEACON		0x08
#define WLAN_FSTYPE_ATIM		0x09
#define WLAN_FSTYPE_DISASSOC		0x0a
#define WLAN_FSTYPE_AUTHEN		0x0b
#define WLAN_FSTYPE_DEAUTHEN		0x0c

/* Control */
#define WLAN_FSTYPE_PSPOLL		0x0a
#define WLAN_FSTYPE_RTS			0x0b
#define WLAN_FSTYPE_CTS			0x0c
#define WLAN_FSTYPE_ACK			0x0d
#define WLAN_FSTYPE_CFEND		0x0e
#define WLAN_FSTYPE_CFENDCFACK		0x0f

/* Data */
#define WLAN_FSTYPE_DATAONLY		0x00
#define WLAN_FSTYPE_DATA_CFACK		0x01
#define WLAN_FSTYPE_DATA_CFPOLL		0x02
#define WLAN_FSTYPE_DATA_CFACK_CFPOLL	0x03
#define WLAN_FSTYPE_NULL		0x04
#define WLAN_FSTYPE_CFACK		0x05
#define WLAN_FSTYPE_CFPOLL		0x06
#define WLAN_FSTYPE_CFACK_CFPOLL	0x07


/*================================================================*/
/* Macros */

/*--- FC Macros ----------------------------------------------*/
/* Macros to get/set the bitfields of the Frame Control Field */
/*  GET_FC_??? - takes the host byte-order value of an FC     */
/*               and retrieves the value of one of the        */
/*               bitfields and moves that value so its lsb is */
/*               in bit 0.                                    */
/*  SET_FC_??? - takes a host order value for one of the FC   */
/*               bitfields and moves it to the proper bit     */
/*               location for ORing into a host order FC.     */
/*               To send the FC produced from SET_FC_???,     */
/*               one must put the bytes in IEEE order.        */
/*  e.g.                                                      */
/*     printf("the frame subtype is %x",                      */
/*                 GET_FC_FTYPE( ieee2host( rx.fc )))         */
/*                                                            */
/*     tx.fc = host2ieee( SET_FC_FTYPE(WLAN_FTYP_CTL) |       */
/*                        SET_FC_FSTYPE(WLAN_FSTYPE_RTS) );   */
/*------------------------------------------------------------*/

/* Always 0 for current 802.11 standards */
#define WLAN_GET_FC_PVER(n)	 (((u16)(n)) & (BIT0 | BIT1))
#define WLAN_GET_FC_FTYPE(n)	((((u16)(n)) & (BIT2 | BIT3)) >> 2)
#define WLAN_GET_FC_FSTYPE(n)	((((u16)(n)) & (BIT4|BIT5|BIT6|BIT7)) >> 4)
#define WLAN_GET_FC_TODS(n) 	((((u16)(n)) & (BIT8)) >> 8)
#define WLAN_GET_FC_FROMDS(n)	((((u16)(n)) & (BIT9)) >> 9)
#define WLAN_GET_FC_MOREFRAG(n) ((((u16)(n)) & (BIT10)) >> 10)
#define WLAN_GET_FC_RETRY(n)	((((u16)(n)) & (BIT11)) >> 11)
/* Indicates PS mode in which STA will be after successful completion
** of current frame exchange sequence. Always 0 for AP frames */
#define WLAN_GET_FC_PWRMGT(n)	((((u16)(n)) & (BIT12)) >> 12)
/* What MoreData=1 means:
** From AP to STA in PS mode: don't sleep yet, I have more frames for you
** From Contention-Free (CF) Pollable STA in response to a CF-Poll:
**   STA has buffered frames for transmission in response to next CF-Poll
** Bcast/mcast frames transmitted from AP:
**   when additional bcast/mcast frames remain to be transmitted by AP
**   during this beacon interval
** In all other cases MoreData=0 */
#define WLAN_GET_FC_MOREDATA(n) ((((u16)(n)) & (BIT13)) >> 13)
#define WLAN_GET_FC_ISWEP(n)	((((u16)(n)) & (BIT14)) >> 14)
#define WLAN_GET_FC_ORDER(n)	((((u16)(n)) & (BIT15)) >> 15)

#define WLAN_SET_FC_PVER(n)	((u16)(n))
#define WLAN_SET_FC_FTYPE(n)	(((u16)(n)) << 2)
#define WLAN_SET_FC_FSTYPE(n)	(((u16)(n)) << 4)
#define WLAN_SET_FC_TODS(n) 	(((u16)(n)) << 8)
#define WLAN_SET_FC_FROMDS(n)	(((u16)(n)) << 9)
#define WLAN_SET_FC_MOREFRAG(n) (((u16)(n)) << 10)
#define WLAN_SET_FC_RETRY(n)	(((u16)(n)) << 11)
#define WLAN_SET_FC_PWRMGT(n)	(((u16)(n)) << 12)
#define WLAN_SET_FC_MOREDATA(n) (((u16)(n)) << 13)
#define WLAN_SET_FC_ISWEP(n)	(((u16)(n)) << 14)
#define WLAN_SET_FC_ORDER(n)	(((u16)(n)) << 15)

/*--- Duration Macros ----------------------------------------*/
/* Macros to get/set the bitfields of the Duration Field      */
/*  - the duration value is only valid when bit15 is zero     */
/*  - the firmware handles these values, so I'm not going     */
/*    these macros right now.                                 */
/*------------------------------------------------------------*/

/*--- Sequence Control  Macros -------------------------------*/
/* Macros to get/set the bitfields of the Sequence Control    */
/* Field.                                                     */
/*------------------------------------------------------------*/
#define WLAN_GET_SEQ_FRGNUM(n) (((u16)(n)) & (BIT0|BIT1|BIT2|BIT3))
#define WLAN_GET_SEQ_SEQNUM(n) ((((u16)(n)) & (~(BIT0|BIT1|BIT2|BIT3))) >> 4)

/*--- Data ptr macro -----------------------------------------*/
/* Creates a u8* to the data portion of a frame               */
/* Assumes you're passing in a ptr to the beginning of the hdr*/
/*------------------------------------------------------------*/
#define WLAN_HDR_A3_DATAP(p) (((u8*)(p)) + WLAN_HDR_A3_LEN)
#define WLAN_HDR_A4_DATAP(p) (((u8*)(p)) + WLAN_HDR_A4_LEN)

#define DOT11_RATE5_ISBASIC_GET(r)     (((u8)(r)) & BIT7)

/*--- FC Macros v. 2.0 ---------------------------------------*/
/* Each constant is defined twice: WF_CONST is in host        */
/* byteorder, WF_CONSTi is in ieee byteorder.                 */
/* Usage:                                                     */
/* printf("the frame subtype is %x", WF_FC_FTYPEi & rx.fc);   */
/* tx.fc = WF_FTYPE_CTLi | WF_FSTYPE_RTSi;                    */
/*------------------------------------------------------------*/

enum {
/*--- Frame Control Field -------------------------------------*/
IEEE16(WF_FC_PVER,			0x0003)
IEEE16(WF_FC_FTYPE,			0x000c)
IEEE16(WF_FC_FSTYPE,			0x00f0)
IEEE16(WF_FC_TODS,			0x0100)
IEEE16(WF_FC_FROMDS,			0x0200)
IEEE16(WF_FC_FROMTODS,			0x0300)
IEEE16(WF_FC_MOREFRAG,			0x0400)
IEEE16(WF_FC_RETRY,			0x0800)
IEEE16(WF_FC_PWRMGT,			0x1000)
IEEE16(WF_FC_MOREDATA,			0x2000)
IEEE16(WF_FC_ISWEP,			0x4000)
IEEE16(WF_FC_ORDER,			0x8000)

/* Frame Types */
IEEE16(WF_FTYPE_MGMT,			0x00)
IEEE16(WF_FTYPE_CTL,			0x04)
IEEE16(WF_FTYPE_DATA,			0x08)

/* Frame subtypes */
/* Management */
IEEE16(WF_FSTYPE_ASSOCREQ,		0x00)
IEEE16(WF_FSTYPE_ASSOCRESP,		0x10)
IEEE16(WF_FSTYPE_REASSOCREQ,		0x20)
IEEE16(WF_FSTYPE_REASSOCRESP,		0x30)
IEEE16(WF_FSTYPE_PROBEREQ,		0x40)
IEEE16(WF_FSTYPE_PROBERESP,		0x50)
IEEE16(WF_FSTYPE_BEACON,		0x80)
IEEE16(WF_FSTYPE_ATIM,			0x90)
IEEE16(WF_FSTYPE_DISASSOC,		0xa0)
IEEE16(WF_FSTYPE_AUTHEN,		0xb0)
IEEE16(WF_FSTYPE_DEAUTHEN,		0xc0)

/* Control */
IEEE16(WF_FSTYPE_PSPOLL,		0xa0)
IEEE16(WF_FSTYPE_RTS,			0xb0)
IEEE16(WF_FSTYPE_CTS,			0xc0)
IEEE16(WF_FSTYPE_ACK,			0xd0)
IEEE16(WF_FSTYPE_CFEND,			0xe0)
IEEE16(WF_FSTYPE_CFENDCFACK,		0xf0)

/* Data */
IEEE16(WF_FSTYPE_DATAONLY,		0x00)
IEEE16(WF_FSTYPE_DATA_CFACK,		0x10)
IEEE16(WF_FSTYPE_DATA_CFPOLL,		0x20)
IEEE16(WF_FSTYPE_DATA_CFACK_CFPOLL,	0x30)
IEEE16(WF_FSTYPE_NULL,			0x40)
IEEE16(WF_FSTYPE_CFACK,			0x50)
IEEE16(WF_FSTYPE_CFPOLL,		0x60)
IEEE16(WF_FSTYPE_CFACK_CFPOLL,		0x70)
};

/*================================================================*/
/* Types */

/* BSS Timestamp */
typedef u8 wlan_bss_ts_t[WLAN_BSS_TS_LEN];

/* Generic 802.11 Header types */
__WLAN_PRAGMA_PACK1__ typedef struct p80211_hdr_a3 {
	u16 fc __WLAN_ATTRIB_PACK__;
	u16 dur __WLAN_ATTRIB_PACK__;
	u8 a1[ETH_ALEN] __WLAN_ATTRIB_PACK__;
	u8 a2[ETH_ALEN] __WLAN_ATTRIB_PACK__;
	u8 a3[ETH_ALEN] __WLAN_ATTRIB_PACK__;
	u16 seq __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ p80211_hdr_a3_t;

__WLAN_PRAGMA_PACKDFLT__ __WLAN_PRAGMA_PACK1__ typedef struct p80211_hdr_a4 {
	u16 fc __WLAN_ATTRIB_PACK__;
	u16 dur __WLAN_ATTRIB_PACK__;
	u8 a1[ETH_ALEN] __WLAN_ATTRIB_PACK__;
	u8 a2[ETH_ALEN] __WLAN_ATTRIB_PACK__;
	u8 a3[ETH_ALEN] __WLAN_ATTRIB_PACK__;
	u16 seq __WLAN_ATTRIB_PACK__;
	u8 a4[ETH_ALEN] __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ p80211_hdr_a4_t;

__WLAN_PRAGMA_PACKDFLT__ typedef union p80211_hdr {
	p80211_hdr_a3_t a3 __WLAN_ATTRIB_PACK__;
	p80211_hdr_a4_t a4 __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ p80211_hdr_t;

/* 802.11 header type v 2.0 */
typedef struct wlan_hdr {
	u16	fc __WLAN_ATTRIB_PACK__;
	u16	dur __WLAN_ATTRIB_PACK__;
	union { /* Note the following:
		** a1 *always* is receiver's mac or bcast/mcast
		** a2 *always* is transmitter's mac, if a2 exists
		** seq: [0:3] frag#, [4:15] seq# - used for dup detection
		** (dups from retries have same seq#) */
		struct {
			u8	ra[ETH_ALEN] __WLAN_ATTRIB_PACK__;
			u8	ta[ETH_ALEN] __WLAN_ATTRIB_PACK__;
		} std;
		struct {
			u8	a1[ETH_ALEN] __WLAN_ATTRIB_PACK__;
			u8	a2[ETH_ALEN] __WLAN_ATTRIB_PACK__;
			u8	a3[ETH_ALEN] __WLAN_ATTRIB_PACK__;
			u16	seq __WLAN_ATTRIB_PACK__;
			u8	a4[ETH_ALEN] __WLAN_ATTRIB_PACK__;
		} numbered;
		struct { /* ad-hoc peer->peer (to/from DS = 0/0) */
			u8	da[ETH_ALEN] __WLAN_ATTRIB_PACK__;
			u8	sa[ETH_ALEN] __WLAN_ATTRIB_PACK__;
			u8	bssid[ETH_ALEN] __WLAN_ATTRIB_PACK__;
			u16	seq __WLAN_ATTRIB_PACK__;
		} ibss;
		struct { /* ap->sta (to/from DS = 0/1) */
			u8	da[ETH_ALEN] __WLAN_ATTRIB_PACK__;
			u8	bssid[ETH_ALEN] __WLAN_ATTRIB_PACK__;
			u8	sa[ETH_ALEN] __WLAN_ATTRIB_PACK__;
			u16	seq __WLAN_ATTRIB_PACK__;
		} fromap;
		struct { /* sta->ap (to/from DS = 1/0) */
			u8	bssid[ETH_ALEN] __WLAN_ATTRIB_PACK__;
			u8	sa[ETH_ALEN] __WLAN_ATTRIB_PACK__;
			u8	da[ETH_ALEN] __WLAN_ATTRIB_PACK__;
			u16	seq __WLAN_ATTRIB_PACK__;
		} toap;
		struct { /* wds->wds (to/from DS = 1/1), the only 4addr pkt */
			u8	ra[ETH_ALEN] __WLAN_ATTRIB_PACK__;
			u8	ta[ETH_ALEN] __WLAN_ATTRIB_PACK__;
			u8	da[ETH_ALEN] __WLAN_ATTRIB_PACK__;
			u16	seq __WLAN_ATTRIB_PACK__;
			u8	sa[ETH_ALEN] __WLAN_ATTRIB_PACK__;
		} wds;
		struct { /* all management packets */
			u8	da[ETH_ALEN] __WLAN_ATTRIB_PACK__;
			u8	sa[ETH_ALEN] __WLAN_ATTRIB_PACK__;
			u8	bssid[ETH_ALEN] __WLAN_ATTRIB_PACK__;
			u16	seq __WLAN_ATTRIB_PACK__;
		} mgmt;
		struct { /* has no body, just a FCS */
			u8	ra[ETH_ALEN] __WLAN_ATTRIB_PACK__;
			u8	ta[ETH_ALEN] __WLAN_ATTRIB_PACK__;
		} rts;
		struct { /* has no body, just a FCS */
			u8	ra[ETH_ALEN] __WLAN_ATTRIB_PACK__;
		} cts;
		struct { /* has no body, just a FCS */
			u8	ra[ETH_ALEN] __WLAN_ATTRIB_PACK__;
		} ack;
		struct { /* has no body, just a FCS */
			/* NB: this one holds Assoc ID in dur field */
			u8	bssid[ETH_ALEN] __WLAN_ATTRIB_PACK__;
			u8	ta[ETH_ALEN] __WLAN_ATTRIB_PACK__;
		} pspoll;
		struct { /* has no body, just a FCS */
			u8	ra[ETH_ALEN] __WLAN_ATTRIB_PACK__;
			u8	bssid[ETH_ALEN] __WLAN_ATTRIB_PACK__;
		} cfend;
		struct { /* has no body, just a FCS */
			u8	ra[ETH_ALEN] __WLAN_ATTRIB_PACK__;
			u8	bssid[ETH_ALEN] __WLAN_ATTRIB_PACK__;
		} cfendcfack;
	} addr;
} wlan_hdr_t;

/* Separate structs for use if frame type is known */
typedef struct wlan_hdr_mgmt {
	u16	fc __WLAN_ATTRIB_PACK__;
	u16	dur __WLAN_ATTRIB_PACK__;
	u8	da[ETH_ALEN] __WLAN_ATTRIB_PACK__;
	u8	sa[ETH_ALEN] __WLAN_ATTRIB_PACK__;
	u8	bssid[ETH_ALEN] __WLAN_ATTRIB_PACK__;
	u16	seq __WLAN_ATTRIB_PACK__;
} wlan_hdr_mgmt_t;

typedef struct wlan_hdr_a3 {
	u16	fc __WLAN_ATTRIB_PACK__;
	u16	dur __WLAN_ATTRIB_PACK__;
	u8	a1[ETH_ALEN] __WLAN_ATTRIB_PACK__;
	u8	a2[ETH_ALEN] __WLAN_ATTRIB_PACK__;
	u8	a3[ETH_ALEN] __WLAN_ATTRIB_PACK__;
	u16	seq __WLAN_ATTRIB_PACK__;
} wlan_hdr_a3_t;
