#ifndef _ACXPCI_H_
#define _ACXPCI_H_

/*
 * acxpci.h: PCI related constants and structures.
 *
 * Copyright (c) 2008 Francis Galiegue <fgaliegue@gmail.com> for the ACX100
 * driver project.
 *
 * This file is licensed under the GPLv2. See the README file for details.
 */

/*
 * MMIO registers
 */

/* Register I/O offsets */
#define ACX100_EEPROM_ID_OFFSET	0x380

/*
 * Please add further ACX hardware register definitions only when
 * it turns out you need them in the driver, and please try to use firmware
 * functionality instead, since using direct I/O access instead of letting the
 * firmware do it might confuse the firmware's state machine.
 */

/* ***** ABSOLUTELY ALWAYS KEEP OFFSETS IN SYNC WITH THE INITIALIZATION
** OF THE I/O ARRAYS!!!! (grep for '^IO_ACX') ***** */

/*
 * NOTE about ACX_IO_IRQ_REASON: this register is CLEARED ON READ.
 */
#define	ACX_IO_SOFT_RESET,		0
#define	ACX_IO_SLV_MEM_ADDR 		1
#define	ACX_IO_SLV_MEM_DATA 		2
#define	ACX_IO_SLV_MEM_CTL 		3
#define	ACX_IO_SLV_END_CTL 		4
/*
 * Original code said that the following is the "function event mask". Whatever
 * that means.
 */
#define	ACX_IO_FEMR 			5
#define	ACX_IO_INT_TRIG 		6
#define	ACX_IO_IRQ_MASK 		7
#define	ACX_IO_IRQ_STATUS_NON_DES 	8
#define	ACX_IO_IRQ_REASON 		9
#define	ACX_IO_IRQ_ACK 			10
#define	ACX_IO_HINT_TRIG 		11
#define	ACX_IO_ENABLE 			12
#define	ACX_IO_EEPROM_CTL 		13
#define	ACX_IO_EEPROM_ADDR 		14
#define	ACX_IO_EEPROM_DATA 		15
#define	ACX_IO_EEPROM_CFG 		16
#define	ACX_IO_PHY_ADDR 		17
#define	ACX_IO_PHY_DATA 		18
#define	ACX_IO_PHY_CTL 			19
#define	ACX_IO_GPIO_OE 			20
#define	ACX_IO_GPIO_OUT 		21
#define	ACX_IO_CMD_MAILBOX_OFFS 	22
#define	ACX_IO_INFO_MAILBOX_OFFS 	23
#define	ACX_IO_EEPROM_INFORMATION 	24
#define	ACX_IO_EE_START 		25
#define	ACX_IO_SOR_CFG 			26
#define	ACX_IO_ECPU_CTRL 		27
/* ***** ABSOLUTELY ALWAYS KEEP OFFSETS IN SYNC WITH THE INITIALIZATION
** OF THE I/O ARRAYS!!!! (grep for '^IO_ACX') ***** */

/* Values for ACX_IO_INT_TRIG register: */
/* inform hw that rxdesc in queue needs processing */
#define INT_TRIG_RXPRC		0x08
/* inform hw that txdesc in queue needs processing */
#define INT_TRIG_TXPRC		0x04
/* ack that we received info from info mailbox */
#define INT_TRIG_INFOACK	0x02
/* inform hw that we have filled command mailbox */
#define INT_TRIG_CMD		0x01

/*
 * In-hardware TX/RX structures
 */

struct txhostdesc {
	acx_ptr	data_phy;			/* 0x00 [u8 *] */
	u16	data_offset;			/* 0x04 */
	u16	reserved;			/* 0x06 */
	u16	Ctl_16;	/* 16bit value, endianness!! */
	u16	length;			/* 0x0a */
	acx_ptr	desc_phy_next;		/* 0x0c [txhostdesc *] */
	acx_ptr	pNext;			/* 0x10 [txhostdesc *] */
	u32	Status;			/* 0x14, unused on Tx */
/* From here on you can use this area as you want (variable length, too!) */
	u8	*data;
	struct ieee80211_tx_status txstatus;
	struct sk_buff *skb;	

} __attribute__ ((packed));

struct rxhostdesc {
	acx_ptr	data_phy;			/* 0x00 [rxbuffer_t *] */
	u16	data_offset;			/* 0x04 */
	u16	reserved;			/* 0x06 */
	u16	Ctl_16;			/* 0x08; 16bit value, endianness!! */
	u16	length;			/* 0x0a */
	acx_ptr	desc_phy_next;		/* 0x0c [rxhostdesc_t *] */
	acx_ptr	pNext;			/* 0x10 [rxhostdesc_t *] */
	u32	Status;			/* 0x14 */
/* From here on you can use this area as you want (variable length, too!) */
	rxbuffer_t *data;
} __attribute__ ((packed));


#endif /* _ACXPCI_H_ */
