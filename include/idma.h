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
int acx100_delete_dma_regions(wlandevice_t *priv);
void acx100_dma_tx_data(wlandevice_t *wlandev, struct txdescriptor *txdesc);
void acx100_clean_tx_desc(wlandevice_t *priv);
UINT8 acx_signal_to_winlevel(UINT8 rawlevel);
void acx100_process_rx_desc(wlandevice_t *priv);
struct txdescriptor *acx100_get_tx_desc(wlandevice_t *priv);

char *acx100_get_packet_type_string(UINT16 fc);

/* seems to be a bit similar to hfa384x_rx_frame.
 * These fields are still not quite obvious, though.
 * Some seem to have different meanings... */

#define ACX100_RXBUF_HDRSIZE 12

typedef struct rxbuffer {
	UINT16	mac_cnt_rcvd;	/* 0x0, only 12 bits are len! (0xfff) */
	UINT8	mac_cnt_mblks;	/* 0x2 */
	UINT8	mac_status;	/* 0x3 */
	UINT8	phy_stat_baseband;	/* 0x4 bit 0x80: used LNA (Low-Noise Amplifier) */
	UINT8	phy_plcp_signal;	/* 0x5 */
	UINT8	phy_level;		/* 0x6 PHY stat */
	UINT8	phy_snr;		/* 0x7  PHY stat */
	UINT32	time;		/* 0x8  timestamp upon MAC rcv first byte */
	acx100_addr3_t buf;	/* 0x0c 0x18 */
	UINT8	data[ACX100_BAP_DATALEN_MAX];
} rxb_t;			/* 0x956 */

typedef struct txbuffer {
	UINT8 data[WLAN_MAX_ETHFRM_LEN-WLAN_ETHHDR_LEN];
} txb_t;

/* This struct must contain the header of a packet. A header can maximally
 * contain a type 4 802.11 header + a LLC + a SNAP, amounting to 38 bytes */
typedef struct framehdr {
	char data[0x26];
} frmhdr_t;

/* figure out tx descriptor pointer, depending on different acx100 or acx111
 * tx descriptor length */
#define GET_TX_DESC_PTR(dc, index) \
	(struct txdescriptor *) (((UINT32)dc->pTxDescQPool) + (index * dc->TxDescrSize))
#define GET_NEXT_TX_DESC_PTR(dc, txdesc) \
	(struct txdescriptor *) (((UINT32)txdesc) + dc->TxDescrSize)

/* flags:
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

#define DESC_CTL2_FCS		0x02
#define DESC_CTL2_MORE_FRAG	0x04
#define DESC_CTL2_RTS		0x20	/* do RTS/CTS magic before sending */

/* Values for rate field (acx100 only) */
#define RATE100_1              10
#define RATE100_2              20
#define RATE100_5              55
#define RATE100_11             110
#define RATE100_22             220
/* This bit denotes use of PBCC:
** (it is unknown whether it works with 1 and 2Mbit rates, probably not) */
#define RATE100_PBCC511                0x80
/* Where did these come from? */
#define RATE100_6_G            0x0D
#define RATE100_9_G            0x0F
#define RATE100_12_G           0x05
#define RATE100_18_G           0x07
#define RATE100_24_G           0x09
#define RATE100_36_G           0x0B
#define RATE100_48_G           0x01
#define RATE100_54_G           0x03

/* Bit values for rate111 field */
#define RATE111_1		0x0001	/* DBPSK */
#define RATE111_2		0x0002	/* DQPSK */
#define RATE111_5		0x0004	/* CCK or PBCC */
#define RATE111_6		0x0008	/* CCK-OFDM or OFDM */
#define RATE111_9		0x0010	/* CCK-OFDM or OFDM */
#define RATE111_11		0x0020	/* CCK or PBCC */
#define RATE111_12		0x0040	/* CCK-OFDM or OFDM */
#define RATE111_18		0x0080	/* CCK-OFDM or OFDM */
#define RATE111_22		0x0100	/* PBCC */
#define RATE111_24		0x0200	/* CCK-OFDM or OFDM */
#define RATE111_36		0x0400	/* CCK-OFDM or OFDM */
#define RATE111_48		0x0800	/* CCK-OFDM or OFDM */
#define RATE111_54		0x1000	/* CCK-OFDM or OFDM */
#define RATE111_RESERVED	0x2000
#define RATE111_PBCC511		0x4000  /* PBCC mod at 5.5 or 11Mbit (else CCK) */
#define RATE111_SHORTPRE	0x8000  /* short preamble */
/* Special 'try everything' value */
#define RATE111_ALL		0x1fff
/* These bits denote acx100 compatible settings */
#define RATE111_ACX100_COMPAT	0x0127

/* For the sake of humanity, here are all 11b/11g/11a rates and modulations:
     11b 11g 11a
     --- --- ---
 1  |B  |B  |
 2  |Q  |Q  |
 5.5|Cp |C p|
 6  |   |Od |O
 9  |   |od |o
11  |Cp |C p|
12  |   |Od |O
18  |   |od |o
22  |   |  p|
24  |   |Od |O
33  |   |  p|
36  |   |od |o
48  |   |od |o
54  |   |od |o

Mandatory:
 B - DBPSK
 Q - DQPSK
 C - CCK
 O - OFDM
Optional:
 o - OFDM
 d - CCK-OFDM (also known as DSSS-OFDM)
 p - PBCC

DBPSK = Differential Binary Phase Shift Keying
DQPSK = Differential Quaternary Phase Shift Keying
DSSS = Direct Sequence Spread Spectrum
CCK = Complementary Code Keying, a form of DSSS
PBCC = Packet Binary Convolutional Coding
OFDM = Orthogonal Frequency Division Multiplexing

The term CCK-OFDM may be used interchangeably with DSSS-OFDM
(the IEEE 802.11g-2003 standard uses the latter terminology).
In the CCK-OFDM, the PLCP header of the frame uses the CCK form of DSSS,
while the PLCP payload (the MAC frame) is modulated using OFDM.

Basically, you must use CCK-OFDM if you have mixed 11b/11g environment,
or else (pure OFDM) 11b equipment may not realize that AP
is sending a packet and start sending its own one.
Sadly, looks like acx111 does not support CCK-OFDM, only pure OFDM.

Re PBCC: avoid using it. It makes sense only if you have
TI "11b+" hardware. You _must_ use PBCC in order to reach 22Mbps on it.

Preambles:

Long preamble (at 1Mbit rate, takes 144 us):
    16 bytes	ones
     2 bytes	0xF3A0 (lsb sent first)
PLCP header follows (at 1Mbit also):
     1 byte	Signal (speed, in 0.1Mbit units)
     1 byte	Service
	0,1,4:	reserved
	2:	1=locked clock
	3:	1=PBCC
	5:	Length Extension (PBCC 22,33Mbit (11g only))  <-
	6:	Length Extension (PBCC 22,33Mbit (11g only))  <- BLACK MAGIC HERE
	7:	Length Extension                              <-
     2 bytes	Length (time needed to tx this frame)
     2 bytes	CRC
PSDU follows (up to 2346 bytes at selected rate)

While Signal value alone is not enough to determine rate and modulation,
Signal+Service is always sufficient.

Short preamble (at 1Mbit rate, takes 72 us):
     7 bytes	zeroes
     2 bytes	0x05CF (lsb sent first)
PLCP header follows *at 2Mbit/s*. Format is the same as in long preamble.
PSDU follows (up to 2346 bytes at selected rate)

OFDM preamble is completely different, uses OFDM
modulation from the start and thus easily identifiable.
Not shown here.
*/

typedef struct txdescriptor {
	UINT32	pNextDesc;		/* pointer to the next txdescriptor */
	UINT32	HostMemPtr;
	UINT32	AcxMemPtr;
	UINT32	tx_time;
	UINT16	total_length;
	UINT16	Reserved;
	/* the following 16 bytes do not change when acx100 owns the descriptor */
	union { /* we need to add a union here with a *fixed* size of 16, since ptrlen AMD64 (8) != ptrlen x86 (4) */
		struct {
			struct  txrate_ctrl *txc;
			struct txhostdescriptor *host_desc;
		} s;
		struct {
			UINT32 d1;
			UINT32 d2;
			UINT32 d3;
			UINT32 d4;
		} dummy;
	} fixed_size;
	UINT8	Ctl_8;				/* 0x24, 8bit value */
	UINT8	Ctl2_8;				/* 0x25, 8bit value */
	UINT8	error;				/* 0x26 */
	UINT8	ack_failures;			/* 0x27 */
	UINT8	rts_failures;			/* 0x28 */
	UINT8	rts_ok;				/* 0x29 */
	union {
    		struct {
			UINT8	rate;		/* 0x2a */
			UINT8	queue_ctrl;	/* 0x2b */
    		} r1 __attribute__((packed));
    		struct {
			UINT16  rate111;
    		} r2 __attribute__((packed));
	} u __attribute__((packed));
	UINT32	queue_info;			/* 0x2c (acx100, 'reserved' on acx111) */

} txdesc_t __attribute__((packed));		/* size : 48 = 0x30 */
/* NOTE: The acx111 txdescriptor structure is 4 byte larger */
/* There are 4 more 'reserved' bytes. tx alloc code takes this into account */

/* some fields here are actually pointers,
 * but they have to remain UINT32, since using ptr instead
 * (8 bytes on 64bit systems!!) would disrupt the fixed descriptor
 * format the acx firmware expects in the non-user area.
 * Since we need to cram an 8 byte ptr into 4 bytes, this probably
 * means that ACX related data needs to remain in low memory
 * (address value needs <= 4 bytes) on 64bit
 * (alternatively we need to cope with the shorted value somehow) */
typedef UINT32 ACX_PTR;
typedef struct txhostdescriptor {
	ACX_PTR	data_phy;			/* 0x00 [UINT8 *] */
	UINT16	data_offset;			/* 0x04 */
	UINT16	reserved;			/* 0x06 */
	UINT16	Ctl_16; /* 16bit value, endianness!! */
	UINT16	length;				/* 0x0a */
	ACX_PTR	desc_phy_next;			/* 0x0c [txhostdescriptor *] */
	ACX_PTR	pNext;				/* 0x10 [txhostdescriptor *] */
	UINT32	Status;				/* 0x14, unused on Tx */
/* From here on you can use this area as you want (variable length, too!) */
	struct	txhostdescriptor *desc_phy;	/* 0x18 [txhostdescriptor *] */
	UINT8	*data;
} txhostdesc_t;			/* size: 0x2c */

typedef struct rxdescriptor {
	UINT32	pNextDesc;			/* 0x00 */
	UINT32	HostMemPtr;			/* 0x04 */
	UINT32	ACXMemPtr;			/* 0x08 */
	UINT32	rx_time;			/* 0x0c */
	UINT16	total_length;			/* 0x10 */
	UINT16	WEP_length;			/* 0x12 */
	UINT32	WEP_ofs;			/* 0x14 */
	UINT8	driverWorkspace[16]; 		/* 0x18 */
#if 0
	UINT32	val0x18;			/* 0x18 the following 16 bytes do not change when acx100 owns the descriptor */
	UINT32	val0x1c;			/* 0x1c */
	UINT32	val0x20;			/* 0x20 */
	struct	rxbuffer *val0x24;		/* 0x24 */
#endif 

	UINT8	Ctl_8;
	UINT8	rate;
	UINT8	error;
	UINT8	SNR;				/* modulation / preamble */
	UINT8   RxLevel;
	UINT8	queue_ctrl;
	UINT16	unknown;
	UINT32	val0x30;
} rxdesc_t;			/* size 52 = 0x34 */

typedef struct rxhostdescriptor {
	ACX_PTR	data_phy;			/* 0x00 [struct rxbuffer *] */
	UINT16	data_offset;			/* 0x04 */
	UINT16	reserved;			/* 0x06 */
	UINT16	Ctl_16;				/* 0x08; 16bit value, endianness!! */
	UINT16	length;				/* 0x0a */
	ACX_PTR	desc_phy_next;			/* 0x0c [struct  rxhostdescriptor *] */
	ACX_PTR	pNext;				/* 0x10 [struct  rxhostdescriptor *] */
	UINT32	Status;				/* 0x14 */
/* From here on you can use this area as you want (variable length, too!) */
	struct	rxhostdescriptor *desc_phy;	/* 0x18 */
	struct	rxbuffer *data;
} rxhostdesc_t;			/* size 44 = 0x2c */

typedef struct MemoryBlockSizeStruct {
	UINT16 rid;
	UINT16 len;
	UINT16 size;
} acx100_memblocksize_t;

#define ETH_P_80211_RAW		(ETH_P_ECONET + 1)
#endif /* __ACX_IDMA_H */
