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
int acx_delete_dma_regions(wlandevice_t *priv);
void acx_dma_tx_data(wlandevice_t *wlandev, struct txdescriptor *txdesc);
void acx_clean_tx_desc(wlandevice_t *priv);
void acx_clean_tx_desc_emergency(wlandevice_t *priv);
u8 acx_signal_to_winlevel(u8 rawlevel);
void acx_process_rx_desc(wlandevice_t *priv);
struct txdescriptor *acx_get_tx_desc(wlandevice_t *priv);

/* seems to be a bit similar to hfa384x_rx_frame.
 * These fields are still not quite obvious, though.
 * Some seem to have different meanings... */

#define ACX100_RXBUF_HDRSIZE 12

#define MAC_CNT_RCVD(desc) (le16_to_cpu(desc->mac_cnt_rcvd) & 0xfff)
typedef struct rxbuffer {
	u16	mac_cnt_rcvd ACX_PACKED;	/* 0x0, only 12 bits are len! (0xfff) */
	u8	mac_cnt_mblks ACX_PACKED;	/* 0x2 */
	u8	mac_status ACX_PACKED;		/* 0x3 */
	u8	phy_stat_baseband ACX_PACKED;	/* 0x4 bit 0x80: used LNA (Low-Noise Amplifier) */
	u8	phy_plcp_signal ACX_PACKED;	/* 0x5 */
	u8	phy_level ACX_PACKED;		/* 0x6 PHY stat */
	u8	phy_snr ACX_PACKED;		/* 0x7 PHY stat */
	u32	time ACX_PACKED;		/* 0x8 timestamp upon MAC rcv first byte */
	acx_addr3_t buf ACX_PACKED;	/* 0x0c 0x18 */
	u8	data[ACX100_BAP_DATALEN_MAX] ACX_PACKED;
} rxb_t;	/* 0x956 */

typedef struct txbuffer {
	u8 data[WLAN_MAX_ETHFRM_LEN-WLAN_ETHHDR_LEN] ACX_PACKED;
} txb_t;

/* This struct must contain the header of a packet. A header can maximally
 * contain a type 4 802.11 header + a LLC + a SNAP, amounting to 38 bytes */
typedef struct framehdr {
	char data[0x26] ACX_PACKED;
} frmhdr_t;

/* figure out tx descriptor pointer, depending on different acx100 or acx111
 * tx descriptor length */
#define GET_TX_DESC_PTR(dc, index) \
	(struct txdescriptor *) (((u8 *)(dc)->pTxDescQPool) + ((index) * dc->TxDescrSize))
#define GET_NEXT_TX_DESC_PTR(dc, txdesc) \
	(struct txdescriptor *) (((u8 *)(txdesc)) + (dc)->TxDescrSize)

/* flags:
 * init value is 0x8e, "idle" value is 0x82 (in idle tx descs)
 */
#define DESC_CTL_SHORT_PREAMBLE 0x01	/* Preamble type: 0 = long; 1 = short */
#define DESC_CTL_FIRSTFRAG      0x02	/* This is the 1st frag of the frame */
#define DESC_CTL_AUTODMA        0x04	
#define DESC_CTL_RECLAIM        0x08	/* ready to reuse */
#define DESC_CTL_HOSTDONE       0x20	/* host has finished processing */
#define DESC_CTL_ACXDONE        0x40    /* acx1xx has finished processing */
#define DESC_CTL_HOSTOWN        0x80    /* host owns the desc [has to be released last, AFTER modifying all other desc fields!] */

#define	DESC_CTL_INIT		(DESC_CTL_HOSTOWN | DESC_CTL_RECLAIM | \
				 DESC_CTL_AUTODMA | DESC_CTL_FIRSTFRAG)

#define	DESC_CTL_DONE		(DESC_CTL_ACXDONE | DESC_CTL_HOSTOWN)

#define DESC_CTL2_SEQ		0x01	/* don't increase sequence field */
#define DESC_CTL2_FCS		0x02	/* don't add the FCS */
#define DESC_CTL2_MORE_FRAG	0x04
#define DESC_CTL2_RETRY		0x08	/* don't increase retry field */
#define DESC_CTL2_POWER		0x10	/* don't increase power mgmt. field */
#define DESC_CTL2_RTS		0x20	/* do RTS/CTS magic before sending */
#define DESC_CTL2_WEP		0x40	/* encrypt this frame */
#define DESC_CTL2_DUR		0x80	/* don't increase duration field */

/* Values for rate field (acx100 only) */
#define RATE100_1              10
#define RATE100_2              20
#define RATE100_5              55
#define RATE100_11             110
#define RATE100_22             220
/* This bit denotes use of PBCC:
** (PBCC encoding usable with 11 and 22 Mbps speeds only) */
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
/* These bits denote 802.11b compatible settings */
#define RATE111_80211B_COMPAT	0x0027

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

/* some fields here are actually pointers,
 * but they have to remain u32, since using ptr instead
 * (8 bytes on 64bit systems!!) would disrupt the fixed descriptor
 * format the acx firmware expects in the non-user area.
 * Since we need to cram an 8 byte ptr into 4 bytes, this probably
 * means that ACX related data needs to remain in low memory
 * (address value needs <= 4 bytes) on 64bit
 * (alternatively we need to cope with the shorted value somehow) */
typedef u32 ACX_PTR;
typedef struct txdescriptor {
	ACX_PTR	pNextDesc ACX_PACKED;		/* pointer to next txdescriptor */
	ACX_PTR	HostMemPtr ACX_PACKED;
	ACX_PTR	AcxMemPtr ACX_PACKED;
	u32	tx_time ACX_PACKED;
	u16	total_length ACX_PACKED;
	u16	Reserved ACX_PACKED;
	/* the following 16 bytes do not change when acx100 owns the descriptor */
	union { /* we need to add a union here with a *fixed* size of 16, since ptrlen AMD64 (8) != ptrlen x86 (4) */
		struct {
			struct  txrate_ctrl *txc ACX_PACKED;
			struct txhostdescriptor *host_desc ACX_PACKED;
		} s ACX_PACKED;
		struct {
			u32 d1 ACX_PACKED;
			u32 d2 ACX_PACKED;
			u32 d3 ACX_PACKED;
			u32 d4 ACX_PACKED;
		} dummy ACX_PACKED;
	} fixed_size ACX_PACKED;
	u8	Ctl_8 ACX_PACKED;			/* 0x24, 8bit value */
	u8	Ctl2_8 ACX_PACKED;			/* 0x25, 8bit value */
	u8	error ACX_PACKED;			/* 0x26 */
	u8	ack_failures ACX_PACKED;		/* 0x27 */
	u8	rts_failures ACX_PACKED;		/* 0x28 */
	u8	rts_ok ACX_PACKED;			/* 0x29 */
	union {
    		struct {
			u8	rate ACX_PACKED;	/* 0x2a */
			u8	queue_ctrl ACX_PACKED;	/* 0x2b */
    		} r1 ACX_PACKED;
    		struct {
			u16  rate111 ACX_PACKED;
    		} r2 ACX_PACKED;
	} u ACX_PACKED;
	u32	queue_info ACX_PACKED;			/* 0x2c (acx100, 'reserved' on acx111) */
} txdesc_t;		/* size : 48 = 0x30 */
/* NOTE: The acx111 txdescriptor structure is 4 byte larger */
/* There are 4 more 'reserved' bytes. tx alloc code takes this into account */

typedef struct txhostdescriptor {
	ACX_PTR	data_phy ACX_PACKED;			/* 0x00 [u8 *] */
	u16	data_offset ACX_PACKED;			/* 0x04 */
	u16	reserved ACX_PACKED;			/* 0x06 */
	u16	Ctl_16 ACX_PACKED; /* 16bit value, endianness!! */
	u16	length ACX_PACKED;			/* 0x0a */
	ACX_PTR	desc_phy_next ACX_PACKED;		/* 0x0c [txhostdescriptor *] */
	ACX_PTR	pNext ACX_PACKED;			/* 0x10 [txhostdescriptor *] */
	u32	Status ACX_PACKED;			/* 0x14, unused on Tx */
/* From here on you can use this area as you want (variable length, too!) */
	struct	txhostdescriptor *desc_phy ACX_PACKED;	/* 0x18 [txhostdescriptor *] */
	u8	*data ACX_PACKED;
} txhostdesc_t;		/* size: variable, currently 0x20 */

typedef struct rxdescriptor {
	ACX_PTR	pNextDesc ACX_PACKED;			/* 0x00 */
	ACX_PTR	HostMemPtr ACX_PACKED;			/* 0x04 */
	ACX_PTR	ACXMemPtr ACX_PACKED;			/* 0x08 */
	u32	rx_time ACX_PACKED;			/* 0x0c */
	u16	total_length ACX_PACKED;		/* 0x10 */
	u16	WEP_length ACX_PACKED;			/* 0x12 */
	u32	WEP_ofs ACX_PACKED;			/* 0x14 */
	u8	driverWorkspace[16] ACX_PACKED;		/* 0x18 */
#if 0
	u32	val0x18 ACX_PACKED;			/* 0x18 the following 16 bytes do not change when acx100 owns the descriptor */
	u32	val0x1c ACX_PACKED;			/* 0x1c */
	u32	val0x20 ACX_PACKED;			/* 0x20 */
	struct	rxbuffer *val0x24 ACX_PACKED;		/* 0x24 */
#endif 

	u8	Ctl_8 ACX_PACKED;
	u8	rate ACX_PACKED;
	u8	error ACX_PACKED;
	u8	SNR ACX_PACKED;				/* modulation / preamble; FIXME: huh? SNR is Signal-to-Noise Ratio, which is something entirely different!! */
	u8   RxLevel ACX_PACKED;
	u8	queue_ctrl ACX_PACKED;
	u16	unknown ACX_PACKED;
	u32	val0x30 ACX_PACKED;
} rxdesc_t;		/* size 52 = 0x34 */

typedef struct rxhostdescriptor {
	ACX_PTR	data_phy ACX_PACKED;			/* 0x00 [struct rxbuffer *] */
	u16	data_offset ACX_PACKED;			/* 0x04 */
	u16	reserved ACX_PACKED;			/* 0x06 */
	u16	Ctl_16 ACX_PACKED;				/* 0x08; 16bit value, endianness!! */
	u16	length ACX_PACKED;				/* 0x0a */
	ACX_PTR	desc_phy_next ACX_PACKED;			/* 0x0c [struct rxhostdescriptor *] */
	ACX_PTR	pNext ACX_PACKED;				/* 0x10 [struct rxhostdescriptor *] */
	u32	Status ACX_PACKED;				/* 0x14 */
/* From here on you can use this area as you want (variable length, too!) */
	struct	rxhostdescriptor *desc_phy ACX_PACKED;	/* 0x18 */
	struct	rxbuffer *data ACX_PACKED;
} rxhostdesc_t;		/* size 44 = 0x2c */

typedef struct acx100_ie_memblocksize {
	u16 type ACX_PACKED;
	u16 len ACX_PACKED;
	u16 size ACX_PACKED;
} acx100_ie_memblocksize_t;

#define ETH_P_80211_RAW		(ETH_P_ECONET + 1)
#endif /* __ACX_IDMA_H */
