#ifndef _ACX_COMMANDS_H_
#define _ACX_COMMANDS_H_

/*
 * acx_commands.h: querying/configuration commands. Differ depending on the
 * chipset and bus type (PCI or USB - it appears that VLYNQ behaves like PCI
 * most of the time - most).
 *
 * Copyright (c) 2003, 2008 ACX100 driver project.
 *
 * This file is licensed under the GPLv2. See the README file for more
 * information.
 *
 * Part of the original comments can be seen between the dash lines below. I
 * just cannot make sense of the first part right now.
 *
 * See also the file README.unknowncommands: it contains other commands gathered
 * from snooping/anyothermeans of unhandled TI chipsets.
 *
 * ----------------------------------------------------------------------------
 * NB: length includes JUST the data part of the IE
 * (does not include size of the (type,len) pair)
 *
 * TODO: seems that acx100, acx100usb, acx111 have some differences,
 * fix code with regard to this!
 * ----------------------------------------------------------------------------
 */

/* 
 * ----------------------------------------------------------------------------
 *  can be found in the cmdTable table in the "Rev. 1.5.0" (FW150) firmware
 * ----------------------------------------------------------------------------
 */
#define ACX1xx_CMD_RESET		0x0000
#define ACX1xx_CMD_INTERROGATE		0x0001
#define ACX1xx_CMD_CONFIGURE		0x0002
#define ACX1xx_CMD_ENABLE_RX		0x0003
#define ACX1xx_CMD_ENABLE_TX		0x0004
#define ACX1xx_CMD_DISABLE_RX		0x0005
#define ACX1xx_CMD_DISABLE_TX		0x0006
#define ACX1xx_CMD_FLUSH_QUEUE		0x0007
#define ACX1xx_CMD_SCAN			0x0008
#define ACX1xx_CMD_STOP_SCAN		0x0009
#define ACX1xx_CMD_CONFIG_TIM		0x000a
#define ACX1xx_CMD_JOIN			0x000b
#define ACX1xx_CMD_WEP_MGMT		0x000c

/*
 * FIXME: get rid of this? We don't have any old firmware lying around, do we?
 */
#ifdef OLD_FIRMWARE_VERSIONS
/* mapped to unknownCMD in FW150 */
#define ACX100_CMD_HALT			0x000e
#else
#define ACX1xx_CMD_MEM_READ		0x000d
#define ACX1xx_CMD_MEM_WRITE		0x000e
#endif /* OLD_FIRMWARE_VERSIONS */
#define ACX1xx_CMD_SLEEP		0x000f
#define ACX1xx_CMD_WAKE			0x0010
/* mapped to unknownCMD in FW150 */
/* #define ACX1xx_CMD_UNKNOWN_11		0x0011 */ // not used by BSD driver too
#define ACX100_CMD_INIT_MEMORY		0x0012
#define ACX1FF_CMD_DISABLE_RADIO	0x0012	/* new firmware? TNETW1450? + NOT in BSD driver */
#define ACX1xx_CMD_CONFIG_BEACON	0x0013
#define ACX1xx_CMD_CONFIG_PROBE_RESPONSE	0x0014
#define ACX1xx_CMD_CONFIG_NULL_DATA	0x0015
#define ACX1xx_CMD_CONFIG_PROBE_REQUEST	0x0016
#define ACX1xx_CMD_FCC_TEST		0x0017	/* known as ACX_CMD_TEST in BSD driver */
#define ACX1xx_CMD_RADIOINIT		0x0018
#define ACX111_CMD_RADIOCALIB		0x0019
#define ACX1FF_CMD_NOISE_HISTOGRAM	0x001c	/* new firmware? TNETW1450? */
#define ACX1FF_CMD_RX_RESET		0x001d	/* new firmware? TNETW1450? */
#define ACX1FF_CMD_LNA_CONTROL		0x0020	/* new firmware? TNETW1450? */
#define ACX1FF_CMD_CONTROL_DBG_TRACE	0x0021	/* new firmware? TNETW1450? */


/* 'After Interrupt' Commands */
#define ACX_AFTER_IRQ_CMD_STOP_SCAN	0x01
#define ACX_AFTER_IRQ_CMD_ASSOCIATE	0x02
#define ACX_AFTER_IRQ_CMD_RADIO_RECALIB	0x04
#define ACX_AFTER_IRQ_UPDATE_CARD_CFG	0x08
#define ACX_AFTER_IRQ_TX_CLEANUP	0x10
#define ACX_AFTER_IRQ_COMPLETE_SCAN	0x20
#define ACX_AFTER_IRQ_RESTART_SCAN	0x40

#endif /* _ACX_COMMANDS_H_ */
