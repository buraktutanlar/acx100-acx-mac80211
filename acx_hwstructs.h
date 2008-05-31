#ifndef _ACX_HWSTRUCTS_H_
#define _ACX_HWSTRUCTS_H_

/*
 * acx_hwstructs.h: hardware specific data structures and constants.
 *
 * Copyright (c) 2003-2008, the ACX100 driver project.
 *
 * This file is licensed under GPL version 2. See the README file for more
 * information.
 */

#include "acx_debug.h"

/* 
 * Following is the original comment - which explains why acx_debug.h is
 * included here:
 *
 * ----------------------------------------------------------------------------
 * An opaque typesafe helper type.
 *
 * Some hardware fields are actually pointers,
 * but they have to remain u32, since using ptr instead
 * (8 bytes on 64bit systems!) would disrupt the fixed descriptor
 * format the acx firmware expects in the non-user area.
 * Since we cannot cram an 8 byte ptr into 4 bytes, we need to
 * enforce that pointed to data remains in low memory
 * (address value needs to fit in 4 bytes) on 64bit systems.
 *
 * This is easy to get wrong, thus we are using a small struct
 * and special macros to access it. Macros will check for
 * attempts to overflow an acx_ptr with value > 0xffffffff.
 *
 * Attempts to use acx_ptr without macros result in compile-time errors
 * ----------------------------------------------------------------------------
 *
 * Ok well, I'll try and do without later. Shouldn't be that hard.
 *
 * The main point of the macro below, though is that they highlight the fact
 * that these pointers are always little endian! THAT is the thing to remember.
 *
 */

typedef struct {
	u32	v;
} __attribute__ ((packed)) acx_ptr;

#if ACX_DEBUG
#define CHECK32(n) BUG_ON(sizeof(n)>4 && (long)(n)>0xffffff00)
#else
#define CHECK32(n) ((void)0)
#endif

/* acx_ptr <-> integer conversion */
#define cpu2acx(n) ({ CHECK32(n); ((acx_ptr){ .v = cpu_to_le32(n) }); })
#define acx2cpu(a) (le32_to_cpu(a.v))

/* acx_ptr <-> pointer conversion */
#define ptr2acx(p) ({ CHECK32(p); ((acx_ptr){ .v = cpu_to_le32((u32)(long)(p)) }); })
#define acx2ptr(a) ((void*)le32_to_cpu(a.v))

/* Values for rate field (acx100 only) */
#define RATE100_1		10
#define RATE100_2		20
#define RATE100_5		55
#define RATE100_11		110
#define RATE100_22		220
/* This bit denotes use of PBCC:
** (PBCC encoding is usable with 11 and 22 Mbps speeds only) */
#define RATE100_PBCC511		0x80

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

/* Descriptor Ctl field bits
 * init value is 0x8e, "idle" value is 0x82 (in idle tx descs)
 */
#define DESC_CTL_SHORT_PREAMBLE	0x01	/* preamble type: 0 = long; 1 = short */
#define DESC_CTL_FIRSTFRAG	0x02	/* this is the 1st frag of the frame */
#define DESC_CTL_AUTODMA	0x04
#define DESC_CTL_RECLAIM	0x08	/* ready to reuse */
#define DESC_CTL_HOSTDONE	0x20	/* host has finished processing */
#define DESC_CTL_ACXDONE	0x40	/* acx has finished processing */
/* host owns the desc [has to be released last, AFTER modifying all other desc fields!] */
#define DESC_CTL_HOSTOWN	0x80
#define	DESC_CTL_ACXDONE_HOSTOWN (DESC_CTL_ACXDONE | DESC_CTL_HOSTOWN)

/* Descriptor Status field
 */
#define	DESC_STATUS_FULL	(1 << 31)

/* NB: some bits may be interesting for Monitor mode tx (aka Raw tx): */
#define DESC_CTL2_SEQ		0x01	/* don't increase sequence field */
#define DESC_CTL2_FCS		0x02	/* don't add the FCS */
#define DESC_CTL2_MORE_FRAG	0x04
#define DESC_CTL2_RETRY		0x08	/* don't increase retry field */
#define DESC_CTL2_POWER		0x10	/* don't increase power mgmt. field */
#define DESC_CTL2_RTS		0x20	/* do RTS/CTS magic before sending */
#define DESC_CTL2_WEP		0x40	/* encrypt this frame */
#define DESC_CTL2_DUR		0x80	/* don't increase duration field */

/*
 * PCI specific structures
 */

/*
 * Here is the original comment:
 *
 * ----------------------------------------------------------------------------
 * Outside of "#ifdef PCI" because USB needs to know sizeof()
 * of txdesc and rxdesc.
 * ----------------------------------------------------------------------------
 *
 * These seem to be hardware tx/rx structures... If they are PCI specific, why
 * does USB need to know their size at all??
 *
 */
struct txdesc {
	acx_ptr	pNextDesc;	/* pointer to next txdesc */
	acx_ptr	HostMemPtr;			/* 0x04 */
	acx_ptr	AcxMemPtr;			/* 0x08 */
	u32	tx_time;			/* 0x0c */
	u16	total_length;			/* 0x10 */
	u16	Reserved;			/* 0x12 */

/* The following 16 bytes do not change when acx100 owns the descriptor */
/* BUG: fw clears last byte of this area which is supposedly reserved
** for driver use. amd64 blew up. We dare not use it now */
	u32	dummy[4];

	u8	Ctl_8;			/* 0x24, 8bit value */
	u8	Ctl2_8;			/* 0x25, 8bit value */
	u8	error;			/* 0x26 */
	u8	ack_failures;		/* 0x27 */
	u8	rts_failures;		/* 0x28 */
	u8	rts_ok;			/* 0x29 */
	union {
		struct {
			u8	rate;		/* 0x2a */
			u8	queue_ctrl;	/* 0x2b */
		} __attribute__ ((packed)) r1;
		struct {
			u16	rate111;	/* 0x2a */
		} __attribute__ ((packed)) r2;
	} __attribute__ ((packed)) u;
	u32	queue_info;			/* 0x2c (acx100, reserved on acx111) */
} __attribute__ ((packed));		/* size : 48 = 0x30 */

typedef struct rxdesc rxdesc_t;
/* NB: acx111 txdesc structure is 4 byte larger */
/* All these 4 extra bytes are reserved. tx alloc code takes them into account */

struct rxdesc {
	acx_ptr	pNextDesc;			/* 0x00 */
	acx_ptr	HostMemPtr;			/* 0x04 */
	acx_ptr	ACXMemPtr;			/* 0x08 */
	u32	rx_time;			/* 0x0c */
	u16	total_length;			/* 0x10 */
	u16	WEP_length;			/* 0x12 */
	u32	WEP_ofs;			/* 0x14 */

/* the following 16 bytes do not change when acx100 owns the descriptor */
	u8	driverWorkspace[16];		/* 0x18 */

	u8	Ctl_8;
	u8	rate;
	u8	error;
	u8	SNR;				/* Signal-to-Noise Ratio */
	u8	RxLevel;
	u8	queue_ctrl;
	u16	unknown;
	u32	unknown2;
} __attribute__ ((packed));		/* size 52 = 0x34 */

typedef struct txdesc txdesc_t;

/*
 * FIXME: this is typedef'ed, but doesn't match any existing structure???
 */

typedef struct tx tx_t;

/* 
 * "Config Option structs", says the original comment.
 * 
 * FIXME: I don't know their exact use yet. They seem to be only used in the PCI
 * case.
 */

typedef struct co_antennas {
	u8	type;
	u8	len;
	u8	list[2];
} __attribute__ ((packed)) co_antennas_t;

typedef struct co_powerlevels {
	u8	type;
	u8	len;
	u16	list[8];
} __attribute__ ((packed)) co_powerlevels_t;

typedef struct co_datarates {
	u8	type;
	u8	len;
	u8	list[8];
} __attribute__ ((packed)) co_datarates_t;

typedef struct co_domains {
	u8	type;
	u8	len;
	u8	list[6];
} __attribute__ ((packed)) co_domains_t;

typedef struct co_product_id {
	u8	type;
	u8	len;
	u8	list[128];
} __attribute__ ((packed)) co_product_id_t;

typedef struct co_manuf_id {
	u8	type;
	u8	len;
	u8	list[128];
} __attribute__ ((packed)) co_manuf_t;

typedef struct co_fixed {
	char	NVSv[8];
/*	u16	NVS_vendor_offs;	ACX111-only */
/*	u16	unknown;		ACX111-only */
	u8	MAC[6];	/* ACX100-only */
	u16	probe_delay;	/* ACX100-only */
	u32	eof_memory;
	u8	dot11CCAModes;
	u8	dot11Diversity;
	u8	dot11ShortPreambleOption;
	u8	dot11PBCCOption;
	u8	dot11ChannelAgility;
	u8	dot11PhyType; /* FIXME: does 802.11 call it "dot11PHYType"? */
	u8	dot11TempType;
	u8	table_count;
} __attribute__ ((packed)) co_fixed_t;

typedef struct acx111_ie_configoption {
	u16			type;
	u16			len;
/* Do not access below members directly, they are in fact variable length */
	co_fixed_t		fixed;
	co_antennas_t		antennas;
	co_powerlevels_t	power_levels;
	co_datarates_t		data_rates;
	co_domains_t		domains;
	co_product_id_t		product_id;
	co_manuf_t		manufacturer;
	u8			_padding[4];
} __attribute__ ((packed)) acx111_ie_configoption_t;

#endif /* _ACX_HWSTRUCTS_H_ */
