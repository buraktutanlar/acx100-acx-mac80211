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

int acx100_create_dma_regions(wlandevice_t *priv);
int acx111_create_dma_regions(wlandevice_t *priv);
int acx100_delete_dma_region(wlandevice_t *wlandev);
void acx100_dma_tx_data(wlandevice_t *wlandev, struct txdescriptor *txdesc);
void acx100_clean_tx_desc(wlandevice_t *priv);
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
typedef struct rxbuffer {
	UINT32 status;		/* 0x0 */
	UINT16 stat;		/* 0x4 */
	UINT8 level;		/* 0x6 */
	UINT8 snr;		/* 0x7 */
	UINT32 time;		/* 0x8 */
	acx100_addr3_t buf;	/* 0x0c 0x18 */
	UINT8 val0x24[0x922];

} rxb_t;			/* 0x956 */

typedef struct txbuffer {
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

#define	DESC_CTL_INIT		(ACX100_CTL_OWN | ACX100_CTL_RECLAIM | \
				 ACX100_CTL_AUTODMA | ACX100_CTL_FIRSTFRAG)

#define	DESC_CTL_DONE		(ACX100_CTL_ACXDONE | ACX100_CTL_OWN)

#define DESC_CTL2_RTS		0x20

typedef struct txdescriptor {
	UINT pNextDesc;		/* 0x00 I think this is a pointer to the next txdescriptor */
	UINT HostMemPtr;	/* V1_3POS 0x04 */
	UINT AcxMemPtr;		/* 0x08 */
	UINT tx_time;
	UINT total_length;
	UINT val0x14;
	UINT val0x18;
	struct txhostdescriptor *host_desc;
	UINT val0x20;

	unsigned char Ctl;	/* V1_3POS 0x24 */
	unsigned char Ctl2;
	unsigned char error;	/* V1_3POS 0x26 */
	unsigned char AckFailures;	/* 0x27 */
	UINT16 val0x28;
	UINT8 rate;		/* 0x2a */
	UINT8 val0x2b;
	UINT32 val0x2c;		/* hmm, what's that? */
} txdesc_t;			/* size : 48 = 0x30 */

typedef struct txhostdescriptor {
	UINT8 *data_phy;	/* 0x00 */
	UINT16 val0x4;		/* V1_3POS 0x04 */
	UINT16 val0x6;
	UINT16 Ctl;		/* V1_3POS 0x08 */
	UINT16 length;
	struct txhostdescriptor *desc_phy_next;	/* 0x0c */
	struct txhostdescriptor *val0x10;
	UINT val0x14;
	struct txhostdescriptor *desc_phy;
	struct txdescriptor *val0x1c;
	UINT val0x20;
	UINT8 *data;		/* 0x24 */
	UINT16 val0x28;
	UINT8 rate;		/* 0x2a */
	UINT8 val0x2b;
} txhostdesc_t;			/* size: 0x2c */

typedef struct rxdescriptor {
	UINT pNextDesc;
	UINT HostMemPtr;	/* 0x04 */
	char val0x8;		/* 0x08 */
	UINT8 val0x9;
	UINT16 val0xa;
	UINT val0xc;
	UINT val0x10;
	int val0x14;
	UINT val0x18;
	UINT val0x1c;
	int val0x20;

	struct rxbuffer *val0x24;
	UINT val0x28;
	UINT val0x2c;
	UINT val0x30;
} rxdesc_t;			/* size 52 = 0x34 */

typedef struct rxhostdescriptor {
	struct rxbuffer *data_phy;	/* 0x00 */
	UINT HostMemPtr;	/* 0x04 */
	UINT16 Ctl;		/* 0x08 */
	UINT16 length;		/* 0x0a */
	struct rxhostdescriptor *desc_phy_next;	/* 0x0c */

	UINT val0x10;		/* 0x10 */
	int val0x14;		/* 0x14 */

	struct rxhostdescriptor *desc_phy;	/* 0x18 */

	UINT val0x1c;		/* 0x1c */
	int val0x20;		/* 0x20 */

	struct rxbuffer *data;	/* 0x24 */

	char val0x28;		/* 0x28 */
	char val0x29;		/* 0x29 */
	char val0x2a;		/* 0x2a; rate */
	char val0x2b;		/* 0x2b */
} rxhostdesc_t;			/* size 44 = 0x2c */

typedef struct MemoryBlockSizeStruct {
	UINT16 val0x0;
	UINT16 val0x2;
	UINT16 size;
} memblocksize_t;

#define ETH_P_80211_RAW		(ETH_P_ECONET + 1)
