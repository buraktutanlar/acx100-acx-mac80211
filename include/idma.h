/* include/idma.h
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
#ifndef __ACX_IDMA_H
#define __ACX_IDMA_H

#include "acx100_conv.h"

int acx100_create_dma_regions(wlandevice_t *priv);
int acx111_create_dma_regions(wlandevice_t *priv);
int acx100_delete_dma_region(wlandevice_t *wlandev);
void acx100_dma_tx_data(wlandevice_t *wlandev, struct txdescriptor *txdesc);
void acx100_clean_tx_desc(wlandevice_t *priv);
inline UINT8 acx_signal_to_winlevel(UINT8 rawlevel);
void acx100_process_rx_desc(wlandevice_t *priv);
int acx100_create_tx_host_desc_queue(TIWLAN_DC *pDc);
int acx100_create_rx_host_desc_queue(TIWLAN_DC *pDc);
void acx100_create_tx_desc_queue(TIWLAN_DC *pDc);
void acx100_create_rx_desc_queue(TIWLAN_DC *pDc);
void acx100_free_desc_queues(TIWLAN_DC *pDc);
int acx100_init_memory_pools(wlandevice_t *priv, acx100_memmap_t *vala);
struct txdescriptor *acx100_get_tx_desc(wlandevice_t *priv);

char *acx100_get_packet_type_string(UINT16 fc);

/* seems to be a bit similar to hfa384x_rx_frame.
 * These fields are still not quite obvious, though.
 * Some seem to have different meanings... */

#define ACX100_RXBUF_HDRSIZE 12

typedef struct rxbuffer {
	/* UINT32	status;	*/	/* 0x0 MAC stat */
	UINT16	mac_cnt_rcvd;	/* 0x0, only 12 bits are len! (0xfff) */
	UINT8	mac_cnt_mblks;	/* 0x2 */
	UINT8	mac_status;	/* 0x3 */
	/* UINT16	stat; */		/* 0x4 PHY stat */
	UINT8	phy_stat_baseband;	/* 0x4 bit 0x80: used LNA (Low-Noise Amplifier) */
	UINT8	phy_plcp_signal;	/* 0x5 */
	UINT8	phy_level;		/* 0x6 PHY stat */
	UINT8	phy_snr;		/* 0x7  PHY stat */
	UINT32	time;		/* 0x8  timestamp upon MAC rcv first byte */
	acx100_addr3_t buf;	/* 0x0c 0x18 */
	UINT8	val0x24[0x922];

} rxb_t;			/* 0x956 */

typedef struct txbuffer {
	UINT8 data[WLAN_MAX_ETHFRM_LEN-WLAN_ETHHDR_LEN];
} txb_t;

/* This stuct must contain the header of a packet. A header can maximally
 * contain a type 4 802.11 header + a LLC + a SNAP, amounting to 38 bytes */
typedef struct framehdr {
	char data[0x26];
} frmhdr_t;

/* flags:
 * 0x01 - short preamble
 * 0x02 - first packet in a row?? fragmentation??? (sorry)
 * 0x40 - usable ?? (0 means: not used for tx)
 * 0x80 - free ? (0 : used, 1 : free)
 * init value is 0x8e, "idle" value is 0x82 (in idle tx descs)
 */

#define DESC_CTL_SHORT_PREAMBLE 0x01
#define DESC_CTL_FIRST_MPDU     0x02
#define DESC_CTL_AUTODMA        0x04
#define DESC_CTL_RECLAIM        0x08
#define DESC_CTL_USED_FOR_TX    0x40    /* owned */
#define DESC_CTL_FREE           0x80    /* tx finished */

#define	DESC_CTL_INIT		(ACX100_CTL_OWN | ACX100_CTL_RECLAIM | \
				 ACX100_CTL_AUTODMA | ACX100_CTL_FIRSTFRAG)

#define	DESC_CTL_DONE		(ACX100_CTL_ACXDONE | ACX100_CTL_OWN)

#define DESC_CTL2_RTS		0x20

typedef struct txdescriptor {
	UINT32	pNextDesc;		/* pointer to the next txdescriptor */
	UINT32	HostMemPtr;
	UINT32	AcxMemPtr;
	UINT32	tx_time;
	UINT16	total_length;
	UINT16	Reserved;
	UINT32	val0x14;		/* the following 16 bytes do not change when acx100 owns the descriptor */
	UINT32	val0x18;			/* 0x18 */
	struct	txhostdescriptor *host_desc;	/* 0x1c */
	UINT32	val0x20;			/* 0x20 */
	UINT8	Ctl;				/* 0x24 */
	UINT8	Ctl2;				/* 0x25 */
	UINT8	error;				/* 0x26 */
	UINT8	ack_failures;			/* 0x27 */
	UINT8	rts_failures;			/* 0x28 */
	UINT8	rts_ok;				/* 0x29 */
	UINT8	rate;				/* 0x2a */
	UINT8	queue_ctrl;			/* 0x2b */
	UINT32	queue_info;			/* 0x2c */
} txdesc_t;			/* size : 48 = 0x30 */

typedef struct txhostdescriptor {
	UINT8	*data_phy;
	UINT16	data_offset;
	UINT16	val0x6;
	UINT16	Ctl;
	UINT16	length;
	struct	txhostdescriptor *desc_phy_next;	
/* You can use this area as you want */
	struct	txhostdescriptor *pNext;	/* 0x10 */
	UINT32	Flags;				/* 0x14 */
	struct	txhostdescriptor *desc_phy;	/* 0x18 */
	struct	txdescriptor *val0x1c;		/* 0x1c */
	UINT32	val0x20;			/* 0x20 */
	UINT8	*data;
	UINT16	val0x28;
	UINT8	rate;
	UINT8	val0x2b;
} txhostdesc_t;			/* size: 0x2c */

typedef struct rxdescriptor {
	UINT32	pNextDesc;			/* 0x00 */
	UINT32	HostMemPtr;			/* 0x04 */
	UINT32	ACXMemPtr;			/* 0x08 */
	UINT32	rx_time;			/* 0x0c */
	UINT16	total_length;			/* 0x10 */
	UINT16	WEP_length;			/* 0x12 */
	UINT32	WEP_ofs;			/* 0x14 */
	UINT32	val0x18;			/* 0x18 */
	UINT32	val0x1c;			/* 0x1c */
	UINT32	val0x20;			/* 0x20 */
	struct	rxbuffer *val0x24;		/* 0x24 */
	UINT8	Ctl;
	UINT8	rate;
	UINT8	error;
	UINT8	SNR;
	UINT8   RxLevel;
	UINT8	queue_ctrl;
	UINT16	unknown;
	UINT32	val0x30;
} rxdesc_t;			/* size 52 = 0x34 */

typedef struct rxhostdescriptor {
	struct	rxbuffer *data_phy;
	UINT32	HostMemPtr;
	UINT16	Ctl;
	UINT16	length;
	struct	rxhostdescriptor *desc_phy_next;
	struct	rxhostdescriptor *pNext;
	UINT32	Status;
/* You can use this area as you want */
	struct	rxhostdescriptor *desc_phy;
	UINT32	val0x1c;
	UINT32	val0x20;
	struct	rxbuffer *data;
	UINT8	val0x28;
	UINT8	val0x29;
	UINT8	rate;
	UINT8	val0x2b;
} rxhostdesc_t;			/* size 44 = 0x2c */

typedef struct MemoryBlockSizeStruct {
	UINT16 rid;
	UINT16 len;
	UINT16 size;
} memblocksize_t;

#define ETH_P_80211_RAW		(ETH_P_ECONET + 1)
#endif /* __ACX_IDMA_H */
