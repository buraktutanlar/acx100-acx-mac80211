/* include/acx100.h
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

#ifndef _ACX100_H
#define _ACX100_H


#if (WLAN_HOSTIF==WLAN_USB)
#include <linux/usb.h>
#endif

/*============================================================================*
 * Debug / log functionality                                                  *
 *============================================================================*/

/* NOTE: If we still want basic logging of driver info if ACX_DEBUG is not
 * defined, we should provide an acxlog variant that is never turned off. We
 * should make a acx_msg(), and rename acxlog() to acx_debug() to make the
 * difference very clear.
 */

#ifdef ACX_DEBUG

#define L_STD		0x01	/* standard logging that really should be there,
				   like error messages etc. */
#define L_INIT		0x02	/* special card initialisation logging */
#define L_IRQ		0x04	/* interrupt stuff */
#define L_ASSOC		0x08	/* assocation (network join) and station log */
#define L_BIN		0x10	/* original messages from binary drivers */
#define L_FUNC		0x20	/* logging of function enter / leave */
#define L_STATE		0x40	/* log of function implementation state */
#define L_XFER		0x80	/* logging of transfers and mgmt */
#define L_DATA		0x100	/* logging of transfer data */
#define L_DEBUG		0x200	/* log of debug info */
#define L_IOCTL		0x400	/* log ioctl calls */
#define L_CTL		0x800	/* log of low-level ctl commands */
#define L_BUF		0x1000	/* debug buffer mgmt (ring buffer etc.) */
#define L_XFER_BEACON	0x2000	/* also log beacon packets */

#define L_BINDEBUG	(L_BIN | L_DEBUG)
#define L_BINSTD	(L_BIN | L_STD)

#define L_ALL		(L_STD | L_INIT | L_IRQ | L_ASSOC | L_BIN | L_FUNC | \
			 L_STATE | L_XFER | L_DATA | L_DEBUG | L_IOCTL | L_CTL)

extern int debug;
extern int acx100_debug_func_indent;

#define acxlog(chan, args...) \
	if (debug & (chan)) \
		printk(KERN_WARNING args)

#define FUNC_INDENT_INCREMENT 2

#define FN_ENTER \
	do { \
		if (debug & L_FUNC) { \
			int i; \
			for (i = 0; i < acx100_debug_func_indent; i++) \
				printk(" "); \
			printk("==> %s\n", __func__); \
			acx100_debug_func_indent += FUNC_INDENT_INCREMENT; \
		} \
	} while (0)

#define FN_EXIT(p, v) \
	do { \
		if (debug & L_FUNC) { \
			int i; \
			acx100_debug_func_indent -= FUNC_INDENT_INCREMENT; \
			for (i = 0; i < acx100_debug_func_indent; i++) \
				printk(" "); \
			if (p) { \
				printk("<== %s: %08x\n", __func__, v); \
			} else { \
				printk("<== %s\n", __func__); \
			} \
		} \
	} while (0)

#else /* ACX_DEBUG */

#define acxlog(chan, args...)
#define FN_ENTER
#define FN_EXIT(p, v)

#endif /* ACX_DEBUG */

/*============================================================================*
 * Constants                                                                  *
 *============================================================================*/

/*--- Mins & Maxs ------------------------------------------------------------*/
#define ACX100_CMD_ALLOC_LEN_MIN	((UINT16)4)
#define ACX100_CMD_ALLOC_LEN_MAX	((UINT16)2400)
#define ACX100_BAP_DATALEN_MAX		((UINT16)4096)
#define ACX100_BAP_OFFSET_MAX		((UINT16)4096)
#define ACX100_PORTID_MAX		((UINT16)7)
#define ACX100_NUMPORTS_MAX		((UINT16)(ACX100_PORTID_MAX+1))
#define ACX100_PDR_LEN_MAX		((UINT16)512)	/* in bytes, from EK */
#define ACX100_PDA_RECS_MAX		((UINT16)200)	/* a guess */
#define ACX100_PDA_LEN_MAX		((UINT16)1024)	/* in bytes, from EK */
#define ACX100_SCANRESULT_MAX		((UINT16)31)
#define ACX100_HSCANRESULT_MAX		((UINT16)31)
#define ACX100_CHINFORESULT_MAX		((UINT16)16)
#define ACX100_DRVR_FIDSTACKLEN_MAX	10
#define ACX100_DRVR_TXBUF_MAX		(sizeof(acx100_tx_frame_t) \
						+ WLAN_DATA_MAXLEN \
						- WLAN_WEP_IV_LEN \
						- WLAN_WEP_ICV_LEN \
						+ 2)
#define ACX100_DRVR_MAGIC		0x4a2d
#define ACX100_INFODATA_MAXLEN		sizeof(acx100_infodata_t)
#define ACX100_INFOFRM_MAXLEN		sizeof(acx100_InfFrame_t)
#define ACX100_RID_GUESSING_MAXLEN	2048	/* I'm not really sure */
#define ACX100_RIDDATA_MAXLEN		ACX100_RID_GUESSING_MAXLEN
#define ACX100_USB_RWMEM_MAXLEN		2048

/*--- Support Constants ------------------------------------------------------*/
#define ACX100_BAP_PROC				((UINT16)0)
#define ACX100_BAP_INT				((UINT16)1)
#define ACX100_PORTTYPE_IBSS			((UINT16)0)
#define ACX100_PORTTYPE_BSS			((UINT16)1)
#define ACX100_PORTTYPE_WDS			((UINT16)2)
#define ACX100_WEPFLAGS_PRIVINVOKED		((UINT16)BIT0)
#define ACX100_WEPFLAGS_EXCLUDE			((UINT16)BIT1)
#define ACX100_WEPFLAGS_DISABLE_TXCRYPT		((UINT16)BIT4)
#define ACX100_WEPFLAGS_DISABLE_RXCRYPT		((UINT16)BIT7)
#define ACX100_WEPFLAGS_IV_INTERVAL1		((UINT16)0)
#define ACX100_WEPFLAGS_IV_INTERVAL10		((UINT16)BIT5)
#define ACX100_WEPFLAGS_IV_INTERVAL50		((UINT16)BIT6)
#define ACX100_WEPFLAGS_IV_INTERVAL100		((UINT16)(BIT5 | BIT6))
#define ACX100_ROAMMODE_FWSCAN_FWROAM		((UINT16)1)
#define ACX100_ROAMMODE_FWSCAN_HOSTROAM		((UINT16)2)
#define ACX100_ROAMMODE_HOSTSCAN_HOSTROAM	((UINT16)3)
#define ACX100_PORTSTATUS_DISABLED		((UINT16)1)
#define ACX100_PORTSTATUS_INITSRCH		((UINT16)2)
#define ACX100_PORTSTATUS_CONN_IBSS		((UINT16)3)
#define ACX100_PORTSTATUS_CONN_ESS		((UINT16)4)
#define ACX100_PORTSTATUS_OOR_ESS		((UINT16)5)
#define ACX100_PORTSTATUS_CONN_WDS		((UINT16)6)
#define ACX100_PORTSTATUS_HOSTAP		((UINT16)8)
#define ACX100_RATEBIT_1			((UINT16)1)
#define ACX100_RATEBIT_2			((UINT16)2)
#define ACX100_RATEBIT_5dot5			((UINT16)4)
#define ACX100_RATEBIT_11			((UINT16)8)


#define RADIO_MAXIM_0D		0x0d
#define RADIO_RFMD_11		0x11
#define RADIO_UNKNOWN_15	0x15

/*--- IRQ Constants ----------------------------------------------------------*/
#define HOST_INT_TIMER			0x40
#define HOST_INT_SCAN_COMPLETE		0x2000	/* official name */

/*--- MAC Internal memory constants and macros -------------------------------*/
/* Masks and macros used to manipulate MAC internal memory addresses.  MAC
 * internal memory addresses are 23 bit quantities.  The MAC uses a paged
 * address space where the upper 16 bits are the page number and the lower 7
 * bits are the offset.  There are various Host API elements that require two
 * 16-bit quantities to specify a MAC internal memory address.  Unfortunately,
 * some of the API's use a page/offset format where the offset value is JUST
 * the lower 7 bits and the page is the remaining 16 bits.  Some of the APIs
 * assume that the 23 bit address has been split at the 16th bit.  We refer to
 * these two formats as AUX format and CMD format.  The macros below help
 * handle some of this.
 */

/* Handy constant */
#define ACX100_ADDR_AUX_OFF_MAX		((UINT16)0x007f)

/* Mask bits for discarding unwanted pieces in a flat address */
#define ACX100_ADDR_FLAT_AUX_PAGE_MASK	0x007fff80
#define ACX100_ADDR_FLAT_AUX_OFF_MASK	0x0000007f
#define ACX100_ADDR_FLAT_CMD_PAGE_MASK	0xffff0000
#define ACX100_ADDR_FLAT_CMD_OFF_MASK	0x0000ffff

/* Mask bits for discarding unwanted pieces in AUX format 16-bit address parts */
#define ACX100_ADDR_AUX_PAGE_MASK	0xffff
#define ACX100_ADDR_AUX_OFF_MASK	0x007f

/* Mask bits for discarding unwanted pieces in CMD format 16-bit address parts */
#define ACX100_ADDR_CMD_PAGE_MASK	0x007f
#define ACX100_ADDR_CMD_OFF_MASK	0xffff

/* Make a 32-bit flat address from AUX format 16-bit page and offset */
#define ACX100_ADDR_AUX_MKFLAT(p,o) \
	(((UINT32)(((UINT16)(p)) & ACX100_ADDR_AUX_PAGE_MASK)) << 7) \
	| ((UINT32)(((UINT16)(o)) & ACX100_ADDR_AUX_OFF_MASK))

/* Make a 32-bit flat address from CMD format 16-bit page and offset */
#define ACX100_ADDR_CMD_MKFLAT(p,o) \
	(((UINT32)(((UINT16)(p)) & ACX100_ADDR_CMD_PAGE_MASK)) << 16) \
	| ((UINT32)(((UINT16)(o)) & ACX100_ADDR_CMD_OFF_MASK))

/* Make AUX format offset and page from a 32-bit flat address */
#define ACX100_ADDR_AUX_MKPAGE(f) \
	((UINT16)((((UINT32)(f)) & ACX100_ADDR_FLAT_AUX_PAGE_MASK) >> 7))
#define ACX100_ADDR_AUX_MKOFF(f) \
	((UINT16)(((UINT32)(f)) & ACX100_ADDR_FLAT_AUX_OFF_MASK))

/* Make CMD format offset and page from a 32-bit flat address */
#define ACX100_ADDR_CMD_MKPAGE(f) \
	((UINT16)((((UINT32)(f)) & ACX100_ADDR_FLAT_CMD_PAGE_MASK) >> 16))
#define ACX100_ADDR_CMD_MKOFF(f) \
	((UINT16)(((UINT32)(f)) & ACX100_ADDR_FLAT_CMD_OFF_MASK))

/*--- Aux register masks/tests -----------------------------------------------*/
/* Some of the upper bits of the AUX offset register are used to select address
 * space. */
#define ACX100_AUX_CTL_EXTDS		0x00
#define ACX100_AUX_CTL_NV		0x01
#define ACX100_AUX_CTL_PHY		0x02
#define ACX100_AUX_CTL_ICSRAM		0x03

/* Make AUX register offset and page values from a flat address */
#define ACX100_AUX_MKOFF(f, c) \
		(ACX100_ADDR_AUX_MKOFF(f) | (((UINT16)(c)) << 12))
#define ACX100_AUX_MKPAGE(f)	ACX100_ADDR_AUX_MKPAGE(f)

/*--- Controller Memory addresses --------------------------------------------*/
#define HFA3842_PDA_BASE		0x007f0000UL
#define HFA3841_PDA_BASE		0x003f0000UL
#define HFA3841_PDA_BOGUS_BASE		0x00390000UL

/*--- Driver Download states  ------------------------------------------------*/
#define ACX100_DLSTATE_DISABLED			0
#define ACX100_DLSTATE_RAMENABLED		1
#define ACX100_DLSTATE_FLASHENABLED		2
#define ACX100_DLSTATE_FLASHWRITTEN		3
#define ACX100_DLSTATE_FLASHWRITEPENDING	4

/*--- Register I/O offsets ---------------------------------------------------*/
#define ACX100_REG0			0x00

#define ACX100_FW_0			0x14
#define ACX100_FW_1			0x16
#define ACX100_DATA_LO			0x18
#define ACX100_DATA_HI			0x1a
#define ACX100_FW_2			0x1c
#define ACX100_FW_3			0x1e
#define ACX100_FW_4			0x20
#define ACX100_FW_5			0x22

#define ACX100_IRQ_34			0x34

/* 7C seems to be used to trigger some action processing by the ACX100 */
#define ACX100_REG_7C			0x7c

#define ACX100_IRQ_MASK			0x98
#define ACX100_STATUS			0xa4
#define ACX100_IRQ_STATUS		0xa8
#define ACX100_IRQ_ACK			0xac
#define ACX100_IRQ_SOMETHING		0xb0

#define ACX100_REG_104			0x104

#define ACX100_REG_124			0x124

#define ACX100_EEPROM_0			0x250
#define ACX100_EEPROM_1			0x252
#define ACX100_EEPROM_ADDR		0x254
#define ACX100_EEPROM_2			0x256
#define ACX100_EEPROM_DATA		0x258
#define ACX100_EEPROM_3			0x25c
#define ACX100_EEPROM_4			0x25e

#define ACX100_CMD_MAILBOX_OFFS		0x2a4
#define ACX100_INFO_MAILBOX_OFFS	0x2a8

#define ACX100_RESET_0			0x2d0
#define ACX100_RESET_1			0x2d4
#define ACX100_RESET_2			0x2d8

/*--- Register Field Masks ---------------------------------------------------*/

/*--- EEPROM offsets ---------------------------------------------------------*/
#define ACX100_EEPROM_ID_OFFSET		0x380

/*--- Command Code Constants -------------------------------------------------*/

/*--- Controller Commands ----------------------------------------------------*/
/* can be found in table cmdTable in firmware "Rev. 1.5.0" (FW150) */
#define ACX100_CMD_RESET		0x00
#define ACX100_CMD_INTERROGATE		0x01
#define ACX100_CMD_CONFIGURE		0x02
#define ACX100_CMD_ENABLE_RX		0x03
#define ACX100_CMD_ENABLE_TX		0x04
#define ACX100_CMD_DISABLE_RX		0x05
#define ACX100_CMD_DISABLE_TX		0x06
#define ACX100_CMD_FLUSH_QUEUE		0x07
#define ACX100_CMD_SCAN			0x08
#define ACX100_CMD_STOP_SCAN		0x09
#define ACX100_CMD_CONFIG_TIM		0x0a
#define ACX100_CMD_JOIN			0x0b
#define ACX100_CMD_WEP_MGMT		0x0c
#define ACX100_CMD_HALT			0x0e	/* mapped to unknownCMD in FW150 */
#define ACX100_CMD_SLEEP		0x0f
#define ACX100_CMD_WAKE			0x10
#define ACX100_CMD_UNKNOWN_11		0x11	/* mapped to unknownCMD in FW150 */
#define ACX100_CMD_INIT_MEMORY		0x12
#define ACX100_CMD_CONFIG_BEACON	0x13
#define ACX100_CMD_CONFIG_PROBE_RESPONSE	0x14
#define ACX100_CMD_CONFIG_NULL_DATA	0x15
#define ACX100_CMD_CONFIG_PROBE_REQUEST	0x16
#define ACX100_CMD_TEST			0x17
#define ACX100_CMD_RADIOINIT		0x18

/*--- Buffer Management Commands ---------------------------------------------*/

/*--- Regulate Commands ------------------------------------------------------*/

/*--- Configure Commands -----------------------------------------------------*/

/*--- Debugging Commands -----------------------------------------------------*/

/*--- Result Codes -----------------------------------------------------------*/

/*--- Programming Modes ------------------------------------------------------*/
/* MODE 0: Disable programming
 * MODE 1: Enable volatile memory programming
 * MODE 2: Enable non-volatile memory programming
 * MODE 3: Program non-volatile memory section
 */

/*--- AUX register enable ----------------------------------------------------*/

/*============================================================================*
 * Record ID Constants                                                        *
 *============================================================================*/

/*--- Configuration RIDs: Network Parameters, Static Configuration Entities --*/
/* these are handled by real_cfgtable in firmware "Rev 1.5.0" (FW150) */
#define ACX100_RID_UNKNOWN_00			0x00	/* mapped to cfgInvalid in FW150 */
#define ACX100_RID_ACX_TIMER			0x01
#define ACX100_RID_POWER_MGMT			0x02
#define ACX100_RID_QUEUE_CONFIG			0x03
#define ACX100_RID_BLOCK_SIZE			0x04
#define ACX100_RID_MEMORY_CONFIG_OPTIONS	0x05
#define ACX100_RID_RATE_FALLBACK		0x06
#define ACX100_RID_WEP_OPTIONS			0x07
#define ACX100_RID_MEMORY_MAP			0x08	/* huh? */
#define ACX100_RID_SSID				0x08	/* huh? */
#define ACX100_RID_UNKNOWN_09			0x09	/* mapped to cfgInvalid in FW150 */
#define ACX100_RID_ASSOC_ID			0x0a
#define ACX100_RID_UNKNOWN_0B			0x0b	/* mapped to cfgInvalid in FW150 */
#define ACX100_RID_UNKNOWN_0C			0x0c	/* very small implementation in FW150! */
#define ACX100_RID_FWREV			0x0d
#define ACX100_RID_FCS_ERROR_COUNT		0x0e
#define ACX100_RID_MEDIUM_USAGE			0x0f
#define ACX100_RID_RXCONFIG			0x10
#define ACX100_RID_UNKNOWN_11			0x11	/* NONBINARY: large implementation in FW150! link quality readings or so? */
#define ACX100_RID_UNKNOWN_12			0x12	/* NONBINARY: VERY large implementation in FW150!! maybe monitor mode??? :-) */
#define ACX100_RID_FIRMWARE_STATISTICS		0x13
#define ACX100_RID_DOT11_STATION_ID		0x1001
#define ACX100_RID_DOT11_UNKNOWN_1002		0x1002	/* mapped to cfgInvalid in FW150 */
#define ACX100_RID_DOT11_BEACON_PERIOD		0x1003	/* mapped to cfgInvalid in FW150 */
#define ACX100_RID_DOT11_DTIM_PERIOD		0x1004	/* mapped to cfgInvalid in FW150 */
#define ACX100_RID_DOT11_SHORT_RETRY_LIMIT	0x1005
#define ACX100_RID_DOT11_LONG_RETRY_LIMIT	0x1006
#define ACX100_RID_DOT11_WEP_KEY		0x1007
#define ACX100_RID_DOT11_MAX_XMIT_MSDU_LIFETIME	0x1008
#define ACX100_RID_DOT11_UNKNOWN_1009		0x1009	/* mapped to some very boring binary table in FW150 */
#define ACX100_RID_DOT11_CURRENT_REG_DOMAIN	0x100a
#define ACX100_RID_DOT11_CURRENT_ANTENNA	0x100b
#define ACX100_RID_DOT11_UNKNOWN_100C		0x100c	/* mapped to cfgInvalid in FW150 */
#define ACX100_RID_DOT11_TX_POWER_LEVEL		0x100d
#define ACX100_RID_DOT11_CURRENT_CCA_MODE	0x100e
#define ACX100_RID_DOT11_ED_THRESHOLD		0x100f
#define ACX100_RID_DOT11_WEP_DEFAULT_KEY_SET	0x1010
#define ACX100_RID_DOT11_UNKNOWN_1011		0x1011	/* mapped to cfgInvalid in FW150 */
#define ACX100_RID_DOT11_UNKNOWN_1012		0x1012	/* mapped to cfgInvalid in FW150 */
#define ACX100_RID_DOT11_UNKNOWN_1013		0x1013	/* mapped to cfgInvalid in FW150 */

/*--- Configuration RID Lengths: Network Params, Static Config Entities -----*/
/* This is the length of JUST the DATA part of the RID (does not include the
 * len or code fields) */

/* TODO: fill in the rest of these */
#define ACX100_RID_ACX_TIMER_LEN			0x10
#define ACX100_RID_POWER_MGMT_LEN			0x06
#define ACX100_RID_QUEUE_CONFIG_LEN			0x1c
#define ACX100_RID_BLOCK_SIZE_LEN			0x02
#define ACX100_RID_MEMORY_CONFIG_OPTIONS_LEN		0x14
#define ACX100_RID_RATE_FALLBACK_LEN			0x01
#define ACX100_RID_WEP_OPTIONS_LEN			0x03
#define ACX100_RID_MEMORY_MAP_LEN			0x28
#define ACX100_RID_SSID_LEN				0x20
#define ACX100_RID_ASSOC_ID_LEN				0x02
#define ACX100_RID_FWREV_LEN				0x18
#define ACX100_RID_FCS_ERROR_COUNT_LEN			0x04
#define ACX100_RID_MEDIUM_USAGE_LEN			0x08
#define ACX100_RID_RXCONFIG_LEN				0x04
#define ACX100_RID_FIRMWARE_STATISTICS_LEN		0x9c
#define ACX100_RID_DOT11_STATION_ID_LEN			0x06
#define ACX100_RID_DOT11_BEACON_PERIOD_LEN		0x02
#define ACX100_RID_DOT11_DTIM_PERIOD_LEN		0x01
#define ACX100_RID_DOT11_SHORT_RETRY_LIMIT_LEN		0x01
#define ACX100_RID_DOT11_LONG_RETRY_LIMIT_LEN		0x01
#define ACX100_RID_DOT11_WEP_KEY_LEN			0x09
#define ACX100_RID_DOT11_WEP_DEFAULT_KEY_LEN		0x20
#define ACX100_RID_DOT11_MAX_XMIT_MSDU_LIFETIME_LEN	0x04
#define ACX100_RID_DOT11_CURRENT_REG_DOMAIN_LEN		0x01
#define ACX100_RID_DOT11_CURRENT_ANTENNA_LEN		0x01
#define ACX100_RID_DOT11_TX_POWER_LEVEL_LEN		0x01
#define ACX100_RID_DOT11_CURRENT_CCA_MODE_LEN		0x01
#define ACX100_RID_DOT11_ED_THRESHOLD_LEN		0x04
#define ACX100_RID_DOT11_WEP_DEFAULT_KEY_SET_LEN	0x01

/*--- Configuration RIDs: Network Parameters, Dynamic Configuration Entities -*/
#define ACX100_RID_GROUPADDR		((UINT16)0xFC80)
#define ACX100_RID_CREATEIBSS		((UINT16)0xFC81)
#define ACX100_RID_FRAGTHRESH		((UINT16)0xFC82)
#define ACX100_RID_RTSTHRESH		((UINT16)0xFC83)
#define ACX100_RID_TXRATECNTL		((UINT16)0xFC84)
#define ACX100_RID_PROMISCMODE		((UINT16)0xFC85)
#define ACX100_RID_FRAGTHRESH0		((UINT16)0xFC90)
#define ACX100_RID_FRAGTHRESH1		((UINT16)0xFC91)
#define ACX100_RID_FRAGTHRESH2		((UINT16)0xFC92)
#define ACX100_RID_FRAGTHRESH3		((UINT16)0xFC93)
#define ACX100_RID_FRAGTHRESH4		((UINT16)0xFC94)
#define ACX100_RID_FRAGTHRESH5		((UINT16)0xFC95)
#define ACX100_RID_FRAGTHRESH6		((UINT16)0xFC96)
#define ACX100_RID_RTSTHRESH0		((UINT16)0xFC97)
#define ACX100_RID_RTSTHRESH1		((UINT16)0xFC98)
#define ACX100_RID_RTSTHRESH2		((UINT16)0xFC99)
#define ACX100_RID_RTSTHRESH3		((UINT16)0xFC9A)
#define ACX100_RID_RTSTHRESH4		((UINT16)0xFC9B)
#define ACX100_RID_RTSTHRESH5		((UINT16)0xFC9C)
#define ACX100_RID_RTSTHRESH6		((UINT16)0xFC9D)
#define ACX100_RID_TXRATECNTL0		((UINT16)0xFC9E)
#define ACX100_RID_TXRATECNTL1		((UINT16)0xFC9F)
#define ACX100_RID_TXRATECNTL2		((UINT16)0xFCA0)
#define ACX100_RID_TXRATECNTL3		((UINT16)0xFCA1)
#define ACX100_RID_TXRATECNTL4		((UINT16)0xFCA2)
#define ACX100_RID_TXRATECNTL5		((UINT16)0xFCA3)
#define ACX100_RID_TXRATECNTL6		((UINT16)0xFCA4)

/*--- Configuration RID Lengths: Network Params, Dynamic Config Entities -----*/
/* This is the length of JUST the DATA part of the RID (does not include the
 * len or code fields) */

/* TODO: fill in the rest of these */
#define ACX100_RID_GROUPADDR_LEN	((UINT16)16 * WLAN_ADDR_LEN)
#define ACX100_RID_CREATEIBSS_LEN	((UINT16)0)
#define ACX100_RID_FRAGTHRESH_LEN	((UINT16)0)
#define ACX100_RID_RTSTHRESH_LEN	((UINT16)0)
#define ACX100_RID_TXRATECNTL_LEN	((UINT16)4)
#define ACX100_RID_PROMISCMODE_LEN	((UINT16)2)
#define ACX100_RID_FRAGTHRESH0_LEN	((UINT16)0)
#define ACX100_RID_FRAGTHRESH1_LEN	((UINT16)0)
#define ACX100_RID_FRAGTHRESH2_LEN	((UINT16)0)
#define ACX100_RID_FRAGTHRESH3_LEN	((UINT16)0)
#define ACX100_RID_FRAGTHRESH4_LEN	((UINT16)0)
#define ACX100_RID_FRAGTHRESH5_LEN	((UINT16)0)
#define ACX100_RID_FRAGTHRESH6_LEN	((UINT16)0)
#define ACX100_RID_RTSTHRESH0_LEN	((UINT16)0)
#define ACX100_RID_RTSTHRESH1_LEN	((UINT16)0)
#define ACX100_RID_RTSTHRESH2_LEN	((UINT16)0)
#define ACX100_RID_RTSTHRESH3_LEN	((UINT16)0)
#define ACX100_RID_RTSTHRESH4_LEN	((UINT16)0)
#define ACX100_RID_RTSTHRESH5_LEN	((UINT16)0)
#define ACX100_RID_RTSTHRESH6_LEN	((UINT16)0)
#define ACX100_RID_TXRATECNTL0_LEN	((UINT16)0)
#define ACX100_RID_TXRATECNTL1_LEN	((UINT16)0)
#define ACX100_RID_TXRATECNTL2_LEN	((UINT16)0)
#define ACX100_RID_TXRATECNTL3_LEN	((UINT16)0)
#define ACX100_RID_TXRATECNTL4_LEN	((UINT16)0)
#define ACX100_RID_TXRATECNTL5_LEN	((UINT16)0)
#define ACX100_RID_TXRATECNTL6_LEN	((UINT16)0)

/*--- Configuration RIDs: Behavior Parameters --------------------------------*/
#define ACX100_RID_ITICKTIME		((UINT16)0xFCE0)

/*--- Configuration RID Lengths: Behavior Parameters -------------------------*/
/* This is the length of JUST the DATA part of the RID (does not include the
 * len or code fields) */
#define ACX100_RID_ITICKTIME_LEN	((UINT16)2)

/*--- Information RIDs: NIC Information --------------------------------------*/
#define ACX100_RID_MAXLOADTIME		((UINT16)0xFD00)
#define ACX100_RID_DOWNLOADBUFFER	((UINT16)0xFD01)
#define ACX100_RID_PRIIDENTITY		((UINT16)0xFD02)
#define ACX100_RID_PRISUPRANGE		((UINT16)0xFD03)
#define ACX100_RID_PRI_CFIACTRANGES	((UINT16)0xFD04)
#define ACX100_RID_NICSERIALNUMBER	((UINT16)0xFD0A)
#define ACX100_RID_NICIDENTITY		((UINT16)0xFD0B)
#define ACX100_RID_MFISUPRANGE		((UINT16)0xFD0C)
#define ACX100_RID_CFISUPRANGE		((UINT16)0xFD0D)
#define ACX100_RID_CHANNELLIST		((UINT16)0xFD10)
#define ACX100_RID_REGULATORYDOMAINS	((UINT16)0xFD11)
#define ACX100_RID_TEMPTYPE		((UINT16)0xFD12)
#define ACX100_RID_CIS			((UINT16)0xFD13)
#define ACX100_RID_STAIDENTITY		((UINT16)0xFD20)
#define ACX100_RID_STASUPRANGE		((UINT16)0xFD21)
#define ACX100_RID_STA_MFIACTRANGES	((UINT16)0xFD22)
#define ACX100_RID_STA_CFIACTRANGES	((UINT16)0xFD23)
#define ACX100_RID_BUILDSEQ		((UINT16)0xFFFE)
#define ACX100_RID_FWID			((UINT16)0xFFFF)

/*--- Information RID Lengths: NIC Information -------------------------------*/
/* This is the length of JUST the DATA part of the RID (does not include the
 * len or code fields) */
#define ACX100_RID_MAXLOADTIME_LEN	((UINT16)0)
#define ACX100_RID_DOWNLOADBUFFER_LEN	((UINT16)sizeof(acx100_downloadbuffer_t))
#define ACX100_RID_PRIIDENTITY_LEN	((UINT16)8)
#define ACX100_RID_PRISUPRANGE_LEN	((UINT16)10)
#define ACX100_RID_CFIACTRANGES_LEN	((UINT16)10)
#define ACX100_RID_NICSERIALNUMBER_LEN	((UINT16)12)
#define ACX100_RID_NICIDENTITY_LEN	((UINT16)8)
#define ACX100_RID_MFISUPRANGE_LEN	((UINT16)10)
#define ACX100_RID_CFISUPRANGE_LEN	((UINT16)10)
#define ACX100_RID_CHANNELLIST_LEN	((UINT16)0)
#define ACX100_RID_REGULATORYDOMAINS_LEN	((UINT16)12)
#define ACX100_RID_TEMPTYPE_LEN		((UINT16)0)
#define ACX100_RID_CIS_LEN		((UINT16)480)
#define ACX100_RID_STAIDENTITY_LEN	((UINT16)8)
#define ACX100_RID_STASUPRANGE_LEN	((UINT16)10)
#define ACX100_RID_MFIACTRANGES_LEN	((UINT16)10)
#define ACX100_RID_CFIACTRANGES2_LEN	((UINT16)10)
#define ACX100_RID_BUILDSEQ_LEN		((UINT16)sizeof(acx100_BuildSeq_t))
#define ACX100_RID_FWID_LEN		((UINT16)sizeof(acx100_FWID_t))

/*--- Information RIDs: MAC Information --------------------------------------*/
#define ACX100_RID_PORTSTATUS		((UINT16)0xFD40)
#define ACX100_RID_CURRENTSSID		((UINT16)0xFD41)
#define ACX100_RID_CURRENTBSSID		((UINT16)0xFD42)
#define ACX100_RID_COMMSQUALITY		((UINT16)0xFD43)
#define ACX100_RID_CURRENTTXRATE	((UINT16)0xFD44)
#define ACX100_RID_CURRENTBCNINT	((UINT16)0xFD45)
#define ACX100_RID_CURRENTSCALETHRESH	((UINT16)0xFD46)
#define ACX100_RID_PROTOCOLRSPTIME	((UINT16)0xFD47)
#define ACX100_RID_SHORTRETRYLIMIT	((UINT16)0xFD48)
#define ACX100_RID_LONGRETRYLIMIT	((UINT16)0xFD49)
#define ACX100_RID_MAXTXLIFETIME	((UINT16)0xFD4A)
#define ACX100_RID_MAXRXLIFETIME	((UINT16)0xFD4B)
#define ACX100_RID_CFPOLLABLE		((UINT16)0xFD4C)
#define ACX100_RID_AUTHALGORITHMS	((UINT16)0xFD4D)
#define ACX100_RID_PRIVACYOPTIMP	((UINT16)0xFD4F)
#define ACX100_RID_DBMCOMMSQUALITY	((UINT16)0xFD51)
#define ACX100_RID_CURRENTTXRATE1	((UINT16)0xFD80)
#define ACX100_RID_CURRENTTXRATE2	((UINT16)0xFD81)
#define ACX100_RID_CURRENTTXRATE3	((UINT16)0xFD82)
#define ACX100_RID_CURRENTTXRATE4	((UINT16)0xFD83)
#define ACX100_RID_CURRENTTXRATE5	((UINT16)0xFD84)
#define ACX100_RID_CURRENTTXRATE6	((UINT16)0xFD85)
#define ACX100_RID_OWNMACADDRESS	((UINT16)0xFD86)
/* #define ACX100_RID_PCFINFO		((UINT16)0xFD87) */
#define ACX100_RID_SCANRESULTS       	((UINT16)0xFD88)	/* NEW */
#define ACX100_RID_HOSTSCANRESULTS   	((UINT16)0xFD89)	/* NEW */
#define ACX100_RID_AUTHENTICATIONUSED	((UINT16)0xFD8A)	/* NEW */

/*--- Information RID Lengths: MAC Information -------------------------------*/
/* This is the length of JUST the DATA part of the RID (does not include the
 * len or code fields) */
#define ACX100_RID_PORTSTATUS_LEN		((UINT16)0)
#define ACX100_RID_CURRENTSSID_LEN		((UINT16)34)
#define ACX100_RID_CURRENTBSSID_LEN		((UINT16)WLAN_BSSID_LEN)
#define ACX100_RID_COMMSQUALITY_LEN		((UINT16)sizeof(acx100_commsquality_t))
#define ACX100_RID_DBMCOMMSQUALITY_LEN		((UINT16)sizeof(acx100_dbmcommsquality_t))
#define ACX100_RID_CURRENTTXRATE_LEN		((UINT16)0)
#define ACX100_RID_CURRENTBCNINT_LEN		((UINT16)0)
#define ACX100_RID_STACURSCALETHRESH_LEN	((UINT16)12)
#define ACX100_RID_APCURSCALETHRESH_LEN		((UINT16)6)
#define ACX100_RID_PROTOCOLRSPTIME_LEN		((UINT16)0)
#define ACX100_RID_SHORTRETRYLIMIT_LEN		((UINT16)0)
#define ACX100_RID_LONGRETRYLIMIT_LEN		((UINT16)0)
#define ACX100_RID_MAXTXLIFETIME_LEN		((UINT16)0)
#define ACX100_RID_MAXRXLIFETIME_LEN		((UINT16)0)
#define ACX100_RID_CFPOLLABLE_LEN		((UINT16)0)
#define ACX100_RID_AUTHALGORITHMS_LEN		((UINT16)4)
#define ACX100_RID_PRIVACYOPTIMP_LEN		((UINT16)0)
#define ACX100_RID_CURRENTTXRATE1_LEN		((UINT16)0)
#define ACX100_RID_CURRENTTXRATE2_LEN		((UINT16)0)
#define ACX100_RID_CURRENTTXRATE3_LEN		((UINT16)0)
#define ACX100_RID_CURRENTTXRATE4_LEN		((UINT16)0)
#define ACX100_RID_CURRENTTXRATE5_LEN		((UINT16)0)
#define ACX100_RID_CURRENTTXRATE6_LEN		((UINT16)0)
#define ACX100_RID_OWNMACADDRESS_LEN		((UINT16)6)
#define ACX100_RID_PCFINFO_LEN			((UINT16)6)
#define ACX100_RID_CNFAPPCFINFO_LEN		((UINT16)sizeof(acx100_PCFInfo_data_t))
#define ACX100_RID_SCANREQUEST_LEN		((UINT16)sizeof(acx100_ScanRequest_data_t))
#define ACX100_RID_JOINREQUEST_LEN		((UINT16)sizeof(acx100_JoinRequest_data_t))
#define ACX100_RID_AUTHENTICATESTA_LEN		((UINT16)sizeof(acx100_authenticateStation_data_t))
#define ACX100_RID_CHANNELINFOREQUEST_LEN	((UINT16)sizeof(acx100_ChannelInfoRequest_data_t))

/*--- Information RIDs: Modem Information ------------------------------------*/
#define ACX100_RID_PHYTYPE		((UINT16)0xFDC0)
#define ACX100_RID_CURRENTCHANNEL	((UINT16)0xFDC1)
#define ACX100_RID_CURRENTPOWERSTATE	((UINT16)0xFDC2)
#define ACX100_RID_CCAMODE		((UINT16)0xFDC3)
#define ACX100_RID_SUPPORTEDDATARATES	((UINT16)0xFDC6)

/*--- Information RID Lengths: Modem Information -----------------------------*/
/* This is the length of JUST the DATA part of the RID (does not include the
 * len or code fields) */
#define ACX100_RID_PHYTYPE_LEN			((UINT16)0)
#define ACX100_RID_CURRENTCHANNEL_LEN		((UINT16)0)
#define ACX100_RID_CURRENTPOWERSTATE_LEN	((UINT16)0)
#define ACX100_RID_CCAMODE_LEN			((UINT16)0)
#define ACX100_RID_SUPPORTEDDATARATES_LEN	((UINT16)10)

/*--- API Enhancements (not yet implemented) ---------------------------------*/
#define ACX100_RID_CNFWEPDEFAULTKEYID	((UINT16)0xFC23)
#define ACX100_RID_CNFWEPDEFAULTKEY0	((UINT16)0xFC24)
#define ACX100_RID_CNFWEPDEFAULTKEY1	((UINT16)0xFC25)
#define ACX100_RID_CNFWEPDEFAULTKEY2	((UINT16)0xFC26)
#define ACX100_RID_CNFWEPDEFAULTKEY3	((UINT16)0xFC27)
#define ACX100_RID_CNFWEPFLAGS		((UINT16)0xFC28)
#define ACX100_RID_CNFWEPKEYMAPTABLE	((UINT16)0xFC29)
#define ACX100_RID_CNFAUTHENTICATION	((UINT16)0xFC2A)
#define ACX100_RID_CNFMAXASSOCSTATIONS	((UINT16)0xFC2B)
#define ACX100_RID_CNFTXCONTROL		((UINT16)0xFC2C)
#define ACX100_RID_CNFROAMINGMODE	((UINT16)0xFC2D)
#define ACX100_RID_CNFHOSTAUTH		((UINT16)0xFC2E)
#define ACX100_RID_CNFRCVCRCERROR	((UINT16)0xFC30)
/* #define ACX100_RID_CNFMMLIFE		((UINT16)0xFC31) */
#define ACX100_RID_CNFALTRETRYCNT	((UINT16)0xFC32)
#define ACX100_RID_CNFAPBCNINT		((UINT16)0xFC33)
#define ACX100_RID_CNFAPPCFINFO		((UINT16)0xFC34)
#define ACX100_RID_CNFSTAPCFINFO	((UINT16)0xFC35)
#define ACX100_RID_CNFPRIORITYQUSAGE	((UINT16)0xFC37)
#define ACX100_RID_CNFTIMCTRL		((UINT16)0xFC40)
#define ACX100_RID_CNFTHIRTY2TALLY	((UINT16)0xFC42)
#define ACX100_RID_CNFENHSECURITY	((UINT16)0xFC43)
#define ACX100_RID_CNFDBMADJUST  	((UINT16)0xFC46)	/* NEW */
#define ACX100_RID_CNFSHORTPREAMBLE	((UINT16)0xFCB0)
#define ACX100_RID_CNFEXCLONGPREAMBLE	((UINT16)0xFCB1)
#define ACX100_RID_CNFAUTHRSPTIMEOUT	((UINT16)0xFCB2)
#define ACX100_RID_CNFBASICRATES	((UINT16)0xFCB3)
#define ACX100_RID_CNFSUPPRATES		((UINT16)0xFCB4)
#define ACX100_RID_CNFFALLBACKCTRL	((UINT16)0xFCB5)	/* NEW */
#define ACX100_RID_WEPKEYDISABLE  	((UINT16)0xFCB6)	/* NEW */
#define ACX100_RID_WEPKEYMAPINDEX 	((UINT16)0xFCB7)	/* NEW AP */
#define ACX100_RID_BROADCASTKEYID 	((UINT16)0xFCB8)	/* NEW AP */
#define ACX100_RID_ENTSECFLAGEYID 	((UINT16)0xFCB9)	/* NEW AP */
#define ACX100_RID_CNFPASSIVESCANCTRL	((UINT16)0xFCBA)	/* NEW STA */
#define ACX100_RID_SCANREQUEST		((UINT16)0xFCE1)
#define ACX100_RID_JOINREQUEST		((UINT16)0xFCE2)
#define ACX100_RID_AUTHENTICATESTA	((UINT16)0xFCE3)
#define ACX100_RID_CHANNELINFOREQUEST	((UINT16)0xFCE4)
#define ACX100_RID_HOSTSCAN          	((UINT16)0xFCE5)	/* NEW STA */

#define ACX100_RID_CNFWEPDEFAULTKEY_LEN		((UINT16)6)
#define ACX100_RID_CNFWEP128DEFAULTKEY_LEN	((UINT16)14)
#define ACX100_RID_CNFPRIOQUSAGE_LEN		((UINT16)4)

/*============================================================================*
 * PD Record codes                                                            *
 *============================================================================*/

#define ACX100_PDR_PCB_PARTNUM			((UINT16)0x0001)
#define ACX100_PDR_PDAVER			((UINT16)0x0002)
#define ACX100_PDR_NIC_SERIAL			((UINT16)0x0003)
#define ACX100_PDR_MKK_MEASUREMENTS		((UINT16)0x0004)
#define ACX100_PDR_NIC_RAMSIZE			((UINT16)0x0005)
#define ACX100_PDR_MFISUPRANGE			((UINT16)0x0006)
#define ACX100_PDR_CFISUPRANGE			((UINT16)0x0007)
#define ACX100_PDR_NICID			((UINT16)0x0008)
#define ACX100_PDR_REFDAC_MEASUREMENTS		((UINT16)0x0010)
#define ACX100_PDR_VGDAC_MEASUREMENTS		((UINT16)0x0020)
#define ACX100_PDR_LEVEL_COMP_MEASUREMENTS	((UINT16)0x0030)
#define ACX100_PDR_MODEM_TRIMDAC_MEASUREMENTS	((UINT16)0x0040)
#define ACX100_PDR_COREGA_HACK			((UINT16)0x00ff)
#define ACX100_PDR_MAC_ADDRESS			((UINT16)0x0101)
#define ACX100_PDR_MKK_CALLNAME			((UINT16)0x0102)
#define ACX100_PDR_REGDOMAIN			((UINT16)0x0103)
#define ACX100_PDR_ALLOWED_CHANNEL		((UINT16)0x0104)
#define ACX100_PDR_DEFAULT_CHANNEL		((UINT16)0x0105)
#define ACX100_PDR_PRIVACY_OPTION		((UINT16)0x0106)
#define ACX100_PDR_TEMPTYPE			((UINT16)0x0107)
#define ACX100_PDR_REFDAC_SETUP			((UINT16)0x0110)
#define ACX100_PDR_VGDAC_SETUP			((UINT16)0x0120)
#define ACX100_PDR_LEVEL_COMP_SETUP		((UINT16)0x0130)
#define ACX100_PDR_TRIMDAC_SETUP		((UINT16)0x0140)
#define ACX100_PDR_IFR_SETTING			((UINT16)0x0200)
#define ACX100_PDR_RFR_SETTING			((UINT16)0x0201)
#define ACX100_PDR_HFA3861_BASELINE		((UINT16)0x0202)
#define ACX100_PDR_HFA3861_SHADOW		((UINT16)0x0203)
#define ACX100_PDR_HFA3861_IFRF			((UINT16)0x0204)
#define ACX100_PDR_HFA3861_CHCALSP		((UINT16)0x0300)
#define ACX100_PDR_HFA3861_CHCALI		((UINT16)0x0301)
#define ACX100_PDR_3842_NIC_CONFIG		((UINT16)0x0400)
#define ACX100_PDR_USB_ID			((UINT16)0x0401)
#define ACX100_PDR_PCI_ID			((UINT16)0x0402)
#define ACX100_PDR_PCI_IFCONF			((UINT16)0x0403)
#define ACX100_PDR_PCI_PMCONF			((UINT16)0x0404)
#define ACX100_PDR_RFENRGY			((UINT16)0x0406)
#define ACX100_PDR_UNKNOWN407			((UINT16)0x0407)
#define ACX100_PDR_UNKNOWN408			((UINT16)0x0408)
#define ACX100_PDR_UNKNOWN409			((UINT16)0x0409)
#define ACX100_PDR_HFA3861_MANF_TESTSP		((UINT16)0x0900)
#define ACX100_PDR_HFA3861_MANF_TESTI		((UINT16)0x0901)
#define ACX100_PDR_END_OF_PDA			((UINT16)0x0000)

/*============================================================================*
 * Macros                                                                     *
 *============================================================================*/

/*--- Register ID macros -----------------------------------------------------*/
#define ACX100_CMD		ACX100_CMD_OFF
#define ACX100_PARAM0		ACX100_PARAM0_OFF
#define ACX100_PARAM1		ACX100_PARAM1_OFF
#define ACX100_PARAM2		ACX100_PARAM2_OFF
#define ACX100_RESP0		ACX100_RESP0_OFF
#define ACX100_RESP1		ACX100_RESP1_OFF
#define ACX100_RESP2		ACX100_RESP2_OFF
#define ACX100_INFOFID		ACX100_INFOFID_OFF
#define ACX100_RXFID		ACX100_RXFID_OFF
#define ACX100_ALLOCFID		ACX100_ALLOCFID_OFF
#define ACX100_TXCOMPLFID	ACX100_TXCOMPLFID_OFF
#define ACX100_SELECT0		ACX100_SELECT0_OFF
#define ACX100_OFFSET0		ACX100_OFFSET0_OFF
#define ACX100_DATA0		ACX100_DATA0_OFF
#define ACX100_SELECT1		ACX100_SELECT1_OFF
#define ACX100_OFFSET1		ACX100_OFFSET1_OFF
#define ACX100_DATA1		ACX100_DATA1_OFF
#define ACX100_EVSTAT		ACX100_EVSTAT_OFF
#define ACX100_INTEN		ACX100_INTEN_OFF
#define ACX100_EVACK		ACX100_EVACK_OFF
#define ACX100_CONTROL		ACX100_CONTROL_OFF
#define ACX100_SWSUPPORT0	ACX100_SWSUPPORT0_OFF
#define ACX100_SWSUPPORT1	ACX100_SWSUPPORT1_OFF
#define ACX100_SWSUPPORT2	ACX100_SWSUPPORT2_OFF
#define ACX100_AUXPAGE		ACX100_AUXPAGE_OFF
#define ACX100_AUXOFFSET	ACX100_AUXOFFSET_OFF
#define ACX100_AUXDATA		ACX100_AUXDATA_OFF
#define ACX100_PCICOR		ACX100_PCICOR_OFF


/*--- Register Test/Get/Set Field macros -------------------------------------*/
#define ACX100_CMD_ISBUSY(value)	((UINT16)((UINT16)(value) & ACX100_CMD_BUSY))
#define ACX100_CMD_AINFO_GET(value)	((UINT16)(((UINT16)(value) & ACX100_CMD_AINFO) >> 8))
#define ACX100_CMD_AINFO_SET(value)	((UINT16)((UINT16)(value) << 8))
#define ACX100_CMD_MACPORT_GET(value)	((UINT16)(ACX100_CMD_AINFO_GET((UINT16)(value) & ACX100_CMD_MACPORT)))
#define ACX100_CMD_MACPORT_SET(value)	((UINT16)ACX100_CMD_AINFO_SET(value))
#define ACX100_CMD_ISRECL(value)	((UINT16)(ACX100_CMD_AINFO_GET((UINT16)(value) & ACX100_CMD_RECL)))
#define ACX100_CMD_RECL_SET(value)	((UINT16)ACX100_CMD_AINFO_SET(value))
#define ACX100_CMD_QOS_GET(value)	((UINT16)(((UINT16)(value) & 0x3000) >> 12))
#define ACX100_CMD_QOS_SET(value)	((UINT16)((((UINT16)(value)) << 12) & 0x3000))
#define ACX100_CMD_ISWRITE(value)	((UINT16)(ACX100_CMD_AINFO_GET((UINT16)(value) & ACX100_CMD_WRITE)))
#define ACX100_CMD_WRITE_SET(value)	((UINT16)ACX100_CMD_AINFO_SET((UINT16)value))
#define ACX100_CMD_PROGMODE_GET(value)	((UINT16)(ACX100_CMD_AINFO_GET((UINT16)(value) & ACX100_CMD_PROGMODE)))
#define ACX100_CMD_PROGMODE_SET(value)	((UINT16)ACX100_CMD_AINFO_SET((UINT16)value))
#define ACX100_CMD_CMDCODE_GET(value)	((UINT16)(((UINT16)(value)) & ACX100_CMD_CMDCODE))
#define ACX100_CMD_CMDCODE_SET(value)	((UINT16)(value))

#define ACX100_STATUS_RESULT_GET(value)	((UINT16)((((UINT16)(value)) & ACX100_STATUS_RESULT) >> 8))
#define ACX100_STATUS_RESULT_SET(value)	(((UINT16)(value)) << 8)
#define ACX100_STATUS_CMDCODE_GET(value)	(((UINT16)(value)) & ACX100_STATUS_CMDCODE)
#define ACX100_STATUS_CMDCODE_SET(value)	((UINT16)(value))

#define ACX100_OFFSET_ISBUSY(value)	((UINT16)(((UINT16)(value)) & ACX100_OFFSET_BUSY))
#define ACX100_OFFSET_ISERR(value)	((UINT16)(((UINT16)(value)) & ACX100_OFFSET_ERR))
#define ACX100_OFFSET_DATAOFF_GET(value)	((UINT16)(((UINT16)(value)) & ACX100_OFFSET_DATAOFF))
#define ACX100_OFFSET_DATAOFF_SET(value)	((UINT16)(value))

#define ACX100_EVSTAT_ISTICK(value)	((UINT16)(((UINT16)(value)) & ACX100_EVSTAT_TICK))
#define ACX100_EVSTAT_ISWTERR(value)	((UINT16)(((UINT16)(value)) & ACX100_EVSTAT_WTERR))
#define ACX100_EVSTAT_ISINFDROP(value)	((UINT16)(((UINT16)(value)) & ACX100_EVSTAT_INFDROP))
#define ACX100_EVSTAT_ISINFO(value)	((UINT16)(((UINT16)(value)) & ACX100_EVSTAT_INFO))
#define ACX100_EVSTAT_ISDTIM(value)	((UINT16)(((UINT16)(value)) & ACX100_EVSTAT_DTIM))
#define ACX100_EVSTAT_ISCMD(value)	((UINT16)(((UINT16)(value)) & ACX100_EVSTAT_CMD))
#define ACX100_EVSTAT_ISALLOC(value)	((UINT16)(((UINT16)(value)) & ACX100_EVSTAT_ALLOC))
#define ACX100_EVSTAT_ISTXEXC(value)	((UINT16)(((UINT16)(value)) & ACX100_EVSTAT_TXEXC))
#define ACX100_EVSTAT_ISTX(value)	((UINT16)(((UINT16)(value)) & ACX100_EVSTAT_TX))
#define ACX100_EVSTAT_ISRX(value)	((UINT16)(((UINT16)(value)) & ACX100_EVSTAT_RX))

#define ACX100_INTEN_ISTICK(value)	((UINT16)(((UINT16)(value)) & ACX100_INTEN_TICK))
#define ACX100_INTEN_TICK_SET(value)	((UINT16)(((UINT16)(value)) << 15))
#define ACX100_INTEN_ISWTERR(value)	((UINT16)(((UINT16)(value)) & ACX100_INTEN_WTERR))
#define ACX100_INTEN_WTERR_SET(value)	((UINT16)(((UINT16)(value)) << 14))
#define ACX100_INTEN_ISINFDROP(value)	((UINT16)(((UINT16)(value)) & ACX100_INTEN_INFDROP))
#define ACX100_INTEN_INFDROP_SET(value)	((UINT16)(((UINT16)(value)) << 13))
#define ACX100_INTEN_ISINFO(value)	((UINT16)(((UINT16)(value)) & ACX100_INTEN_INFO))
#define ACX100_INTEN_INFO_SET(value)	((UINT16)(((UINT16)(value)) << 7))
#define ACX100_INTEN_ISDTIM(value)	((UINT16)(((UINT16)(value)) & ACX100_INTEN_DTIM))
#define ACX100_INTEN_DTIM_SET(value)	((UINT16)(((UINT16)(value)) << 5))
#define ACX100_INTEN_ISCMD(value)	((UINT16)(((UINT16)(value)) & ACX100_INTEN_CMD))
#define ACX100_INTEN_CMD_SET(value)	((UINT16)(((UINT16)(value)) << 4))
#define ACX100_INTEN_ISALLOC(value)	((UINT16)(((UINT16)(value)) & ACX100_INTEN_ALLOC))
#define ACX100_INTEN_ALLOC_SET(value)	((UINT16)(((UINT16)(value)) << 3))
#define ACX100_INTEN_ISTXEXC(value)	((UINT16)(((UINT16)(value)) & ACX100_INTEN_TXEXC))
#define ACX100_INTEN_TXEXC_SET(value)	((UINT16)(((UINT16)(value)) << 2))
#define ACX100_INTEN_ISTX(value)	((UINT16)(((UINT16)(value)) & ACX100_INTEN_TX))
#define ACX100_INTEN_TX_SET(value)	((UINT16)(((UINT16)(value)) << 1))
#define ACX100_INTEN_ISRX(value)	((UINT16)(((UINT16)(value)) & ACX100_INTEN_RX))
#define ACX100_INTEN_RX_SET(value)	((UINT16)(((UINT16)(value)) << 0))

#define ACX100_EVACK_ISTICK(value)	((UINT16)(((UINT16)(value)) & ACX100_EVACK_TICK))
#define ACX100_EVACK_TICK_SET(value)	((UINT16)(((UINT16)(value)) << 15))
#define ACX100_EVACK_ISWTERR(value)	((UINT16)(((UINT16)(value)) & ACX100_EVACK_WTERR))
#define ACX100_EVACK_WTERR_SET(value)	((UINT16)(((UINT16)(value)) << 14))
#define ACX100_EVACK_ISINFDROP(value)	((UINT16)(((UINT16)(value)) & ACX100_EVACK_INFDROP))
#define ACX100_EVACK_INFDROP_SET(value)	((UINT16)(((UINT16)(value)) << 13))
#define ACX100_EVACK_ISINFO(value)	((UINT16)(((UINT16)(value)) & ACX100_EVACK_INFO))
#define ACX100_EVACK_INFO_SET(value)	((UINT16)(((UINT16)(value)) << 7))
#define ACX100_EVACK_ISDTIM(value)	((UINT16)(((UINT16)(value)) & ACX100_EVACK_DTIM))
#define ACX100_EVACK_DTIM_SET(value)	((UINT16)(((UINT16)(value)) << 5))
#define ACX100_EVACK_ISCMD(value)	((UINT16)(((UINT16)(value)) & ACX100_EVACK_CMD))
#define ACX100_EVACK_CMD_SET(value)	((UINT16)(((UINT16)(value)) << 4))
#define ACX100_EVACK_ISALLOC(value)	((UINT16)(((UINT16)(value)) & ACX100_EVACK_ALLOC))
#define ACX100_EVACK_ALLOC_SET(value)	((UINT16)(((UINT16)(value)) << 3))
#define ACX100_EVACK_ISTXEXC(value)	((UINT16)(((UINT16)(value)) & ACX100_EVACK_TXEXC))
#define ACX100_EVACK_TXEXC_SET(value)	((UINT16)(((UINT16)(value)) << 2))
#define ACX100_EVACK_ISTX(value)	((UINT16)(((UINT16)(value)) & ACX100_EVACK_TX))
#define ACX100_EVACK_TX_SET(value)	((UINT16)(((UINT16)(value)) << 1))
#define ACX100_EVACK_ISRX(value)	((UINT16)(((UINT16)(value)) & ACX100_EVACK_RX))
#define ACX100_EVACK_RX_SET(value)	((UINT16)(((UINT16)(value)) << 0))

#define ACX100_CONTROL_AUXEN_SET(value)	((UINT16)(((UINT16)(value)) << 14))
#define ACX100_CONTROL_AUXEN_GET(value)	((UINT16)(((UINT16)(value)) >> 14))

/*--- Host Maintained State Info ---------------------------------------------*/
#define ACX100_STATE_PREINIT	0
#define ACX100_STATE_INIT	1
#define ACX100_STATE_RUNNING	2

/*============================================================================*
 * Types and their related constants                                          *
 *============================================================================*/

/*--- Commonly used basic types ----------------------------------------------*/
typedef struct acx100_bytestr {
	UINT16 len __WLAN_ATTRIB_PACK__;
	UINT8 data[0] __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_bytestr_t;

typedef struct acx100_bytestr32 {
	UINT16 len __WLAN_ATTRIB_PACK__;
	UINT8 data[32] __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_bytestr32_t;

/*--- Configuration Record Structures: ---------------------------------------*
 *--- Network Parameters, Static Configuration Entities ----------------------*/

/*-- Prototype structure --*/
/* all configuration record structures start with these members */
typedef struct acx100_mem_t {
	UINT base;
	UINT len;
	long time;
} mem_t;

typedef struct acx100_record {
	UINT16 reclen __WLAN_ATTRIB_PACK__;
	UINT16 rid __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_rec_t;

typedef struct acx100_record16 {
	UINT16 reclen __WLAN_ATTRIB_PACK__;
	UINT16 rid __WLAN_ATTRIB_PACK__;
	UINT16 val __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_rec16_t;

typedef struct acx100_record32 {
	UINT16 reclen __WLAN_ATTRIB_PACK__;
	UINT16 rid __WLAN_ATTRIB_PACK__;
	UINT32 val __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_rec32;

/*-- Hardware/Firmware Component Information --*/
typedef struct acx100_compident {
	UINT16 id __WLAN_ATTRIB_PACK__;
	UINT16 variant __WLAN_ATTRIB_PACK__;
	UINT16 major __WLAN_ATTRIB_PACK__;
	UINT16 minor __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_compident_t;

typedef struct acx100_caplevel {
	UINT16 role __WLAN_ATTRIB_PACK__;
	UINT16 id __WLAN_ATTRIB_PACK__;
	UINT16 variant __WLAN_ATTRIB_PACK__;
	UINT16 bottom __WLAN_ATTRIB_PACK__;
	UINT16 top __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_caplevel_t;

/*-- Configuration Record: cnfPortType --*/
typedef struct acx100_cnfPortType {
	UINT16 cnfPortType __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_cnfPortType_t;

/*-- Configuration Record: cnfOwnMACAddress --*/
typedef struct acx100_cnfOwnMACAddress {
	UINT8 cnfOwnMACAddress[6] __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_cnfOwnMACAddress_t;

/*-- Configuration Record: cnfDesiredSSID --*/
typedef struct acx100_cnfDesiredSSID {
	UINT8 cnfDesiredSSID[34] __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_cnfDesiredSSID_t;

/*-- Configuration Record: cnfOwnChannel --*/
typedef struct acx100_cnfOwnChannel {
	UINT16 cnfOwnChannel __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_cnfOwnChannel_t;

/*-- Configuration Record: cnfOwnSSID --*/
typedef struct acx100_cnfOwnSSID {
	UINT8 cnfOwnSSID[34] __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_cnfOwnSSID_t;

/*-- Configuration Record: cnfOwnATIMWindow --*/
typedef struct acx100_cnfOwnATIMWindow {
	UINT16 cnfOwnATIMWindow __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_cnfOwnATIMWindow_t;

/*-- Configuration Record: cnfSystemScale --*/
typedef struct acx100_cnfSystemScale {
	UINT16 cnfSystemScale __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_cnfSystemScale_t;

/*-- Configuration Record: cnfMaxDataLength --*/
typedef struct acx100_cnfMaxDataLength {
	UINT16 cnfMaxDataLength __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_cnfMaxDataLength_t;

/*-- Configuration Record: cnfWDSAddress --*/
typedef struct acx100_cnfWDSAddress {
	UINT8 cnfWDSAddress[6] __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_cnfWDSAddress_t;

/*-- Configuration Record: cnfPMEnabled --*/
typedef struct acx100_cnfPMEnabled {
	UINT16 cnfPMEnabled __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_cnfPMEnabled_t;

/*-- Configuration Record: cnfPMEPS --*/
typedef struct acx100_cnfPMEPS {
	UINT16 cnfPMEPS __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_cnfPMEPS_t;

/*-- Configuration Record: cnfMulticastReceive --*/
typedef struct acx100_cnfMulticastReceive {
	UINT16 cnfMulticastReceive __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_cnfMulticastReceive_t;

/*-- Configuration Record: cnfAuthentication --*/
#define ACX100_CNFAUTHENTICATION_OPENSYSTEM	0x0001
#define ACX100_CNFAUTHENTICATION_SHAREDKEY	0x0002

/*-- Configuration Record: cnfMaxSleepDuration --*/
typedef struct acx100_cnfMaxSleepDuration {
	UINT16 cnfMaxSleepDuration __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_cnfMaxSleepDuration_t;

/*-- Configuration Record: cnfPMHoldoverDuration --*/
typedef struct acx100_cnfPMHoldoverDuration {
	UINT16 cnfPMHoldoverDuration __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_cnfPMHoldoverDuration_t;

/*-- Configuration Record: cnfOwnName --*/
typedef struct acx100_cnfOwnName {
	UINT8 cnfOwnName[34] __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_cnfOwnName_t;

/*-- Configuration Record: cnfOwnDTIMPeriod --*/
typedef struct acx100_cnfOwnDTIMPeriod {
	UINT16 cnfOwnDTIMPeriod __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_cnfOwnDTIMPeriod_t;

/*-- Configuration Record: cnfWDSAddress --*/
typedef struct acx100_cnfWDSAddressN {
	UINT8 cnfWDSAddress[6] __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_cnfWDSAddressN_t;

/*-- Configuration Record: cnfMulticastPMBuffering --*/
typedef struct acx100_cnfMulticastPMBuffering {
	UINT16 cnfMulticastPMBuffering __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_cnfMulticastPMBuffering_t;

/*--- Configuration Record Structures: ---------------------------------------*
 *--- Network Parameters, Dynamic Configuration Entities ---------------------*/

/*-- Configuration Record: GroupAddresses --*/
typedef struct acx100_GroupAddresses {
	UINT8 MACAddress[16][6] __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_GroupAddresses_t;

/*-- Configuration Record: CreateIBSS --*/
typedef struct acx100_CreateIBSS {
	UINT16 CreateIBSS __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_CreateIBSS_t;

#define ACX100_CREATEIBSS_JOINCREATEIBSS          0
#define ACX100_CREATEIBSS_JOINESS_JOINCREATEIBSS  1
#define ACX100_CREATEIBSS_JOINIBSS                2
#define ACX100_CREATEIBSS_JOINESS_JOINIBSS        3

/*-- Configuration Record: FragmentationThreshold --*/
typedef struct acx100_FragmentationThreshold {
	UINT16 FragmentationThreshold __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_FragmentationThreshold_t;

/*-- Configuration Record: RTSThreshold --*/
typedef struct acx100_RTSThreshold {
	UINT16 RTSThreshold __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_RTSThreshold_t;

/*-- Configuration Record: TxRateControl --*/
typedef struct acx100_TxRateControl {
	UINT16 TxRateControl __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_TxRateControl_t;

/*-- Configuration Record: PromiscuousMode --*/
typedef struct acx100_PromiscuousMode {
	UINT16 PromiscuousMode __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_PromiscuousMode_t;

/*-- Configuration Record: ScanRequest (data portion only) --*/
typedef struct acx100_ScanRequest_data {
	UINT16 channelList __WLAN_ATTRIB_PACK__;
	UINT16 txRate __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_ScanRequest_data_t;

/*-- Configuration Record: HostScanRequest (data portion only) --*/
typedef struct acx100_HostScanRequest_data {
	UINT16 channelList __WLAN_ATTRIB_PACK__;
	UINT16 txRate __WLAN_ATTRIB_PACK__;
	acx100_bytestr32_t ssid __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_HostScanRequest_data_t;

/*-- Configuration Record: JoinRequest (data portion only) --*/
typedef struct acx100_JoinRequest_data {
	UINT8 bssid[WLAN_BSSID_LEN] __WLAN_ATTRIB_PACK__;
	UINT16 channel __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_JoinRequest_data_t;

/*-- Configuration Record: authenticateStation (data portion only) --*/
typedef struct acx100_authenticateStation_data {
	UINT8 address[WLAN_ADDR_LEN] __WLAN_ATTRIB_PACK__;
	UINT16 status __WLAN_ATTRIB_PACK__;
	UINT16 algorithm __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_authenticateStation_data_t;

/*-- Configuration Record: ChannelInfoRequest (data portion only) --*/
typedef struct acx100_ChannelInfoRequest_data {
	UINT16 channelList __WLAN_ATTRIB_PACK__;
	UINT16 channelDwellTime __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_ChannelInfoRequest_data_t;

/*--- Configuration Record Structures: Behavior Parameters -------------------*/

/*-- Configuration Record: TickTime --*/
typedef struct acx100_TickTime {
	UINT16 TickTime __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_TickTime_t;

/*============================================================================*
 * Information Record Structures: NIC Information                             *
 *============================================================================*/

/*-- Information Record: MaxLoadTime --*/
typedef struct acx100_MaxLoadTime {
	UINT16 MaxLoadTime __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_MaxLoadTime_t;

/*-- Information Record: DownLoadBuffer --*/
/* NOTE: The page and offset are in AUX format */
typedef struct acx100_downloadbuffer {
	UINT16 page __WLAN_ATTRIB_PACK__;
	UINT16 offset __WLAN_ATTRIB_PACK__;
	UINT16 len __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_downloadbuffer_t;

/*-- Information Record: PRIIdentity --*/
typedef struct acx100_PRIIdentity {
	UINT16 PRICompID __WLAN_ATTRIB_PACK__;
	UINT16 PRIVariant __WLAN_ATTRIB_PACK__;
	UINT16 PRIMajorVersion __WLAN_ATTRIB_PACK__;
	UINT16 PRIMinorVersion __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_PRIIdentity_t;

/*-- Information Record: PRISupRange --*/
typedef struct acx100_PRISupRange {
	UINT16 PRIRole __WLAN_ATTRIB_PACK__;
	UINT16 PRIID __WLAN_ATTRIB_PACK__;
	UINT16 PRIVariant __WLAN_ATTRIB_PACK__;
	UINT16 PRIBottom __WLAN_ATTRIB_PACK__;
	UINT16 PRITop __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_PRISupRange_t;

/*-- Information Record: CFIActRanges --*/
typedef struct acx100_CFIActRanges {
	UINT16 CFIRole __WLAN_ATTRIB_PACK__;
	UINT16 CFIID __WLAN_ATTRIB_PACK__;
	UINT16 CFIVariant __WLAN_ATTRIB_PACK__;
	UINT16 CFIBottom __WLAN_ATTRIB_PACK__;
	UINT16 CFITop __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_CFIActRanges_t;

/*-- Information Record: NICSerialNumber --*/
typedef struct acx100_NICSerialNumber {
	UINT8 NICSerialNumber[12] __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_NICSerialNumber_t;

/*-- Information Record: NICIdentity --*/
typedef struct acx100_NICIdentity {
	UINT16 NICCompID __WLAN_ATTRIB_PACK__;
	UINT16 NICVariant __WLAN_ATTRIB_PACK__;
	UINT16 NICMajorVersion __WLAN_ATTRIB_PACK__;
	UINT16 NICMinorVersion __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_NICIdentity_t;

/*-- Information Record: MFISupRange --*/
typedef struct acx100_MFISupRange {
	UINT16 MFIRole __WLAN_ATTRIB_PACK__;
	UINT16 MFIID __WLAN_ATTRIB_PACK__;
	UINT16 MFIVariant __WLAN_ATTRIB_PACK__;
	UINT16 MFIBottom __WLAN_ATTRIB_PACK__;
	UINT16 MFITop __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_MFISupRange_t;

/*-- Information Record: CFISupRange --*/
typedef struct acx100_CFISupRange {
	UINT16 CFIRole __WLAN_ATTRIB_PACK__;
	UINT16 CFIID __WLAN_ATTRIB_PACK__;
	UINT16 CFIVariant __WLAN_ATTRIB_PACK__;
	UINT16 CFIBottom __WLAN_ATTRIB_PACK__;
	UINT16 CFITop __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_CFISupRange_t;

/*-- Information Record: BUILDSEQ:BuildSeq --*/
typedef struct acx100_BuildSeq {
	UINT16 primary __WLAN_ATTRIB_PACK__;
	UINT16 secondary __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_BuildSeq_t;

/*-- Information Record: FWID --*/
#define ACX100_FWID_LEN	14
typedef struct acx100_FWID {
	UINT8 primary[ACX100_FWID_LEN] __WLAN_ATTRIB_PACK__;
	UINT8 secondary[ACX100_FWID_LEN] __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_FWID_t;

/*-- Information Record: ChannelList --*/
typedef struct acx100_ChannelList {
	UINT16 ChannelList __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_ChannelList_t;

/*-- Information Record: RegulatoryDomains --*/
typedef struct acx100_RegulatoryDomains {
	UINT8 RegulatoryDomains[12] __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_RegulatoryDomains_t;

/*-- Information Record: TempType --*/
typedef struct acx100_TempType {
	UINT16 TempType __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_TempType_t;

/*-- Information Record: CIS --*/
typedef struct acx100_CIS {
	UINT8 CIS[480] __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_CIS_t;

/*-- Information Record: STAIdentity --*/
typedef struct acx100_STAIdentity {
	UINT16 STACompID __WLAN_ATTRIB_PACK__;
	UINT16 STAVariant __WLAN_ATTRIB_PACK__;
	UINT16 STAMajorVersion __WLAN_ATTRIB_PACK__;
	UINT16 STAMinorVersion __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_STAIdentity_t;

/*-- Information Record: STASupRange --*/
typedef struct acx100_STASupRange {
	UINT16 STARole __WLAN_ATTRIB_PACK__;
	UINT16 STAID __WLAN_ATTRIB_PACK__;
	UINT16 STAVariant __WLAN_ATTRIB_PACK__;
	UINT16 STABottom __WLAN_ATTRIB_PACK__;
	UINT16 STATop __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_STASupRange_t;

/*-- Information Record: MFIActRanges --*/
typedef struct acx100_MFIActRanges {
	UINT16 MFIRole __WLAN_ATTRIB_PACK__;
	UINT16 MFIID __WLAN_ATTRIB_PACK__;
	UINT16 MFIVariant __WLAN_ATTRIB_PACK__;
	UINT16 MFIBottom __WLAN_ATTRIB_PACK__;
	UINT16 MFITop __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_MFIActRanges_t;

/*-- Information Record: PortStatus --*/
typedef struct acx100_PortStatus {
	UINT16 PortStatus __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_PortStatus_t;

#define ACX100_PSTATUS_DISABLED		((UINT16)1)
#define ACX100_PSTATUS_SEARCHING	((UINT16)2)
#define ACX100_PSTATUS_CONN_IBSS	((UINT16)3)
#define ACX100_PSTATUS_CONN_ESS		((UINT16)4)
#define ACX100_PSTATUS_OUTOFRANGE	((UINT16)5)
#define ACX100_PSTATUS_CONN_WDS		((UINT16)6)

/*-- Information Record: CurrentSSID --*/
typedef struct acx100_CurrentSSID {
	UINT8 CurrentSSID[34] __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_CurrentSSID_t;

/*-- Information Record: CurrentBSSID --*/
typedef struct acx100_CurrentBSSID {
	UINT8 CurrentBSSID[6] __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_CurrentBSSID_t;

/*-- Information Record: commsquality --*/
typedef struct acx100_commsquality {
	UINT16 CQ_currBSS __WLAN_ATTRIB_PACK__;
	UINT16 ASL_currBSS __WLAN_ATTRIB_PACK__;
	UINT16 ANL_currFC __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_commsquality_t;

/*-- Information Record: dmbcommsquality --*/
typedef struct acx100_dbmcommsquality {
	UINT16 CQdbm_currBSS __WLAN_ATTRIB_PACK__;
	UINT16 ASLdbm_currBSS __WLAN_ATTRIB_PACK__;
	UINT16 ANLdbm_currFC __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_dbmcommsquality_t;

/*-- Information Record: CurrentTxRate --*/
typedef struct acx100_CurrentTxRate {
	UINT16 CurrentTxRate __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_CurrentTxRate_t;

/*-- Information Record: CurrentBeaconInterval --*/
typedef struct acx100_CurrentBeaconInterval {
	UINT16 CurrentBeaconInterval __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_CurrentBeaconInterval_t;

/*-- Information Record: CurrentScaleThresholds --*/
typedef struct acx100_CurrentScaleThresholds {
	UINT16 EnergyDetectThreshold __WLAN_ATTRIB_PACK__;
	UINT16 CarrierDetectThreshold __WLAN_ATTRIB_PACK__;
	UINT16 DeferDetectThreshold __WLAN_ATTRIB_PACK__;
	UINT16 CellSearchThreshold __WLAN_ATTRIB_PACK__;	/* Stations only */
	UINT16 DeadSpotThreshold __WLAN_ATTRIB_PACK__;	/* Stations only */
} __WLAN_ATTRIB_PACK__ acx100_CurrentScaleThresholds_t;

/*-- Information Record: ProtocolRspTime --*/
typedef struct acx100_ProtocolRspTime {
	UINT16 ProtocolRspTime __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_ProtocolRspTime_t;

/*-- Information Record: ShortRetryLimit --*/
typedef struct acx100_ShortRetryLimit {
	UINT16 ShortRetryLimit __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_ShortRetryLimit_t;

/*-- Information Record: LongRetryLimit --*/
typedef struct acx100_LongRetryLimit {
	UINT16 LongRetryLimit __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_LongRetryLimit_t;

/*-- Information Record: MaxTransmitLifetime --*/
typedef struct acx100_MaxTransmitLifetime {
	UINT16 MaxTransmitLifetime __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_MaxTransmitLifetime_t;

/*-- Information Record: MaxReceiveLifetime --*/
typedef struct acx100_MaxReceiveLifetime {
	UINT16 MaxReceiveLifetime __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_MaxReceiveLifetime_t;

/*-- Information Record: CFPollable --*/
typedef struct acx100_CFPollable {
	UINT16 CFPollable __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_CFPollable_t;

/*-- Information Record: AuthenticationAlgorithms --*/
typedef struct acx100_AuthenticationAlgorithms {
	UINT16 AuthenticationType __WLAN_ATTRIB_PACK__;
	UINT16 TypeEnabled __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_AuthenticationAlgorithms_t;

/*-- Information Record: AuthenticationAlgorithms (data only) --*/
typedef struct acx100_AuthenticationAlgorithms_data {
	UINT16 AuthenticationType __WLAN_ATTRIB_PACK__;
	UINT16 TypeEnabled __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_AuthenticationAlgorithms_data_t;

/*-- Information Record: PrivacyOptionImplemented --*/
typedef struct acx100_PrivacyOptionImplemented {
	UINT16 PrivacyOptionImplemented __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_PrivacyOptionImplemented_t;

/*-- Information Record: OwnMACAddress --*/
typedef struct acx100_OwnMACAddress {
	UINT8 OwnMACAddress[6] __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_OwnMACAddress_t;

/*-- Information Record: PCFInfo --*/
typedef struct acx100_PCFInfo {
	UINT16 MediumOccupancyLimit __WLAN_ATTRIB_PACK__;
	UINT16 CFPPeriod __WLAN_ATTRIB_PACK__;
	UINT16 CFPMaxDuration __WLAN_ATTRIB_PACK__;
	UINT16 CFPFlags __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_PCFInfo_t;

/*-- Information Record: PCFInfo (data portion only) --*/
typedef struct acx100_PCFInfo_data {
	UINT16 MediumOccupancyLimit __WLAN_ATTRIB_PACK__;
	UINT16 CFPPeriod __WLAN_ATTRIB_PACK__;
	UINT16 CFPMaxDuration __WLAN_ATTRIB_PACK__;
	UINT16 CFPFlags __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_PCFInfo_data_t;

/*============================================================================*
 * Information Record Structures: Modem Information Records                   *
 *============================================================================*/

/*-- Information Record: PHYType --*/
typedef struct acx100_PHYType {
	UINT16 PHYType __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_PHYType_t;

/*-- Information Record: CurrentChannel --*/
typedef struct acx100_CurrentChannel {
	UINT16 CurrentChannel __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_CurrentChannel_t;

/*-- Information Record: CurrentPowerState --*/
typedef struct acx100_CurrentPowerState {
	UINT16 CurrentPowerState __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_CurrentPowerState_t;

/*-- Information Record: CCAMode --*/
typedef struct acx100_CCAMode {
	UINT16 CCAMode __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_CCAMode_t;

/*-- Information Record: SupportedDataRates --*/
typedef struct acx100_SupportedDataRates {
	UINT8 SupportedDataRates[10] __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_SupportedDataRates_t;

/*============================================================================*
 *                   FRAME DESCRIPTORS AND FRAME STRUCTURES                   *
 *============================================================================*/

/*============================================================================*
 * Frame Descriptors: Offsets                                                 *
 *============================================================================*/
/*--- Control Info (offset 44-51) --------------------------------------------*/
#define ACX100_FD_STATUS_OFF		((UINT16)0x44)
#define ACX100_FD_TIME_OFF		((UINT16)0x46)
#define ACX100_FD_SWSUPPORT_OFF		((UINT16)0x4A)
#define ACX100_FD_SILENCE_OFF		((UINT16)0x4A)
#define ACX100_FD_SIGNAL_OFF		((UINT16)0x4B)
#define ACX100_FD_RATE_OFF		((UINT16)0x4C)
#define ACX100_FD_RXFLOW_OFF		((UINT16)0x4D)
#define ACX100_FD_RESERVED_OFF		((UINT16)0x4E)
#define ACX100_FD_TXCONTROL_OFF		((UINT16)0x50)
/*--- 802.11 Header (offset 52-6B) -------------------------------------------*/
#define ACX100_FD_FRAMECONTROL_OFF	((UINT16)0x52)
#define ACX100_FD_DURATIONID_OFF	((UINT16)0x54)
#define ACX100_FD_ADDRESS1_OFF		((UINT16)0x56)
#define ACX100_FD_ADDRESS2_OFF		((UINT16)0x5C)
#define ACX100_FD_ADDRESS3_OFF		((UINT16)0x62)
#define ACX100_FD_SEQCONTROL_OFF	((UINT16)0x68)
#define ACX100_FD_ADDRESS4_OFF		((UINT16)0x6A)
#define ACX100_FD_DATALEN_OFF		((UINT16)0x70)
/*--- 802.3 Header (offset 72-7F) --------------------------------------------*/
#define ACX100_FD_DESTADDRESS_OFF	((UINT16)0x72)
#define ACX100_FD_SRCADDRESS_OFF	((UINT16)0x78)
#define ACX100_FD_DATALENGTH_OFF	((UINT16)0x7E)

/*============================================================================*
 * FRAME STRUCTURES: Communication Frames                                     *
 *============================================================================*/

/*--- Communication Frames: Transmit Frame Structure -------------------------*/
typedef struct acx100_tx_frame {
	/*-- Control information --*/
	UINT16 status __WLAN_ATTRIB_PACK__;
	UINT16 reserved1 __WLAN_ATTRIB_PACK__;
	UINT16 reserved2 __WLAN_ATTRIB_PACK__;
	UINT32 sw_support __WLAN_ATTRIB_PACK__;
	UINT16 reserved3 __WLAN_ATTRIB_PACK__;
	UINT16 tx_control __WLAN_ATTRIB_PACK__;

	/*-- 802.11 Header Information --*/
	UINT16 frame_control __WLAN_ATTRIB_PACK__;
	UINT16 duration_id __WLAN_ATTRIB_PACK__;
	UINT8 address1[6] __WLAN_ATTRIB_PACK__;
	UINT8 address2[6] __WLAN_ATTRIB_PACK__;
	UINT8 address3[6] __WLAN_ATTRIB_PACK__;
	UINT16 sequence_control __WLAN_ATTRIB_PACK__;
	UINT8 address4[6] __WLAN_ATTRIB_PACK__;
	UINT16 data_len __WLAN_ATTRIB_PACK__;	/* little endian format */

	/*-- 802.3 Header Information --*/
	UINT8 dest_addr[6] __WLAN_ATTRIB_PACK__;
	UINT8 src_addr[6] __WLAN_ATTRIB_PACK__;
	UINT16 data_length __WLAN_ATTRIB_PACK__;	/* big endian format */
} __WLAN_ATTRIB_PACK__ acx100_tx_frame_t;

/*--- Communication Frames: Field Masks for Transmit Frames ------------------*/
/*-- Status Field --*/
#define ACX100_TXSTATUS_ACKERR		((UINT16)BIT5)
#define ACX100_TXSTATUS_FORMERR		((UINT16)BIT3)
#define ACX100_TXSTATUS_DISCON		((UINT16)BIT2)
#define ACX100_TXSTATUS_AGEDERR		((UINT16)BIT1)
#define ACX100_TXSTATUS_RETRYERR	((UINT16)BIT0)
/*-- Transmit Control Field --*/
#define ACX100_TX_CFPOLL		((UINT16)BIT12)
#define ACX100_TX_PRST			((UINT16)BIT11)
#define ACX100_TX_MACPORT		((UINT16)(BIT10 | BIT9 | BIT8))
#define ACX100_TX_NOENCRYPT		((UINT16)BIT7)
#define ACX100_TX_RETRYSTRAT		((UINT16)(BIT6 | BIT5))
#define ACX100_TX_STRUCTYPE		((UINT16)(BIT4 | BIT3))
#define ACX100_TX_TXEX			((UINT16)BIT2)
#define ACX100_TX_TXOK			((UINT16)BIT1)

/*--- Communication Frames: Test/Get/Set Field Values for Transmit Frames ----*/
/*-- Status Field --*/
#define ACX100_TXSTATUS_ISERROR(v) \
		(((UINT16)(v)) & \
		 (ACX100_TXSTATUS_ACKERR | ACX100_TXSTATUS_FORMERR | \
		  ACX100_TXSTATUS_DISCON | ACX100_TXSTATUS_AGEDERR | \
		  ACX100_TXSTATUS_RETRYERR))

#define ACX100_TXSTATUS_ISACKERR(v) \
		((UINT16)(((UINT16)(v)) & ACX100_TXSTATUS_ACKERR))
#define ACX100_TXSTATUS_ISFORMERR(v) \
		((UINT16)(((UINT16)(v)) & ACX100_TXSTATUS_FORMERR))
#define ACX100_TXSTATUS_ISDISCON(v) \
		((UINT16)(((UINT16)(v)) & ACX100_TXSTATUS_DISCON))
#define ACX100_TXSTATUS_ISAGEDERR(v) \
		((UINT16)(((UINT16)(v)) & ACX100_TXSTATUS_AGEDERR))
#define ACX100_TXSTATUS_ISRETRYERR(v) \
		((UINT16)(((UINT16)(v)) & ACX100_TXSTATUS_RETRYERR))

#define ACX100_TX_GET(v,m,s) \
		((((UINT16)(v)) & ((UINT16)(m))) >> ((UINT16)(s)))
#define ACX100_TX_SET(v,m,s) \
		((((UINT16)(v)) << ((UINT16)(s))) & ((UINT16)(m)))

#define ACX100_TX_CFPOLL_GET(v)		ACX100_TX_GET(v, ACX100_TX_CFPOLL, 12)
#define ACX100_TX_CFPOLL_SET(v)		ACX100_TX_SET(v, ACX100_TX_CFPOLL, 12)
#define ACX100_TX_PRST_GET(v)		ACX100_TX_GET(v, ACX100_TX_PRST, 11)
#define ACX100_TX_PRST_SET(v)		ACX100_TX_SET(v, ACX100_TX_PRST, 11)
#define ACX100_TX_MACPORT_GET(v)	ACX100_TX_GET(v, ACX100_TX_MACPORT, 8)
#define ACX100_TX_MACPORT_SET(v)	ACX100_TX_SET(v, ACX100_TX_MACPORT, 8)
#define ACX100_TX_NOENCRYPT_GET(v)	ACX100_TX_GET(v, ACX100_TX_NOENCRYPT, 7)
#define	ACX100_TX_NOENCRYPT_SET(v)	ACX100_TX_SET(v, ACX100_TX_NOENCRYPT, 7)
#define	ACX100_TX_RETRYSTRAT_GET(v)	ACX100_TX_GET(v, ACX100_TX_RETRYSTRAT, 5)
#define	ACX100_TX_RETRYSTRAT_SET(v)	ACX100_TX_SET(v, ACX100_TX_RETRYSTRAT, 5)
#define	ACX100_TX_STRUCTYPE_GET(v)	ACX100_TX_GET(v, ACX100_TX_STRUCTYPE, 3)
#define	ACX100_TX_STRUCTYPE_SET(v)	ACX100_TX_SET(v, ACX100_TX_STRUCTYPE, 3)
#define	ACX100_TX_TXEX_GET(v)		ACX100_TX_GET(v, ACX100_TX_TXEX, 2)
#define	ACX100_TX_TXEX_SET(v)		ACX100_TX_SET(v, ACX100_TX_TXEX, 2)
#define	ACX100_TX_TXOK_GET(v)		ACX100_TX_GET(v, ACX100_TX_TXOK, 1)
#define	ACX100_TX_TXOK_SET(v)		ACX100_TX_SET(v, ACX100_TX_TXOK, 1)

/*--- Communication Frames: Receive Frame Structure --------------------------*/
typedef struct acx100_rx_frame {
	/*-- MAC RX descriptor (acx100 byte order) --*/
	UINT16 status __WLAN_ATTRIB_PACK__;
	UINT32 time __WLAN_ATTRIB_PACK__;
	UINT8 silence __WLAN_ATTRIB_PACK__;
	UINT8 signal __WLAN_ATTRIB_PACK__;
	UINT8 rate __WLAN_ATTRIB_PACK__;
	UINT8 rx_flow __WLAN_ATTRIB_PACK__;
	UINT16 reserved1 __WLAN_ATTRIB_PACK__;
	UINT16 reserved2 __WLAN_ATTRIB_PACK__;

	/*-- 802.11 Header Information (802.11 byte order) --*/
	UINT16 frame_control __WLAN_ATTRIB_PACK__;
	UINT16 duration_id __WLAN_ATTRIB_PACK__;
	UINT8 address1[6] __WLAN_ATTRIB_PACK__;
	UINT8 address2[6] __WLAN_ATTRIB_PACK__;
	UINT8 address3[6] __WLAN_ATTRIB_PACK__;
	UINT16 sequence_control __WLAN_ATTRIB_PACK__;
	UINT8 address4[6] __WLAN_ATTRIB_PACK__;
	UINT16 data_len __WLAN_ATTRIB_PACK__;	/* acx100 (little endian) format */

	/*-- 802.3 Header Information --*/
	UINT8 dest_addr[6] __WLAN_ATTRIB_PACK__;
	UINT8 src_addr[6] __WLAN_ATTRIB_PACK__;
	UINT16 data_length __WLAN_ATTRIB_PACK__;	/* IEEE? (big endian) format */
} __WLAN_ATTRIB_PACK__ acx100_rx_frame_t;

/*--- Communication Frames: Field Masks for Receive Frames -------------------*/
/*-- Offsets --*/
#define ACX100_RX_DATA_LEN_OFF		((UINT16)44)
#define ACX100_RX_80211HDR_OFF		((UINT16)14)
#define ACX100_RX_DATA_OFF		((UINT16)60)

/*-- Status Fields --*/
#define ACX100_RXSTATUS_MSGTYPE		((UINT16)(BIT15 | BIT14 | BIT13))
#define ACX100_RXSTATUS_MACPORT		((UINT16)(BIT10 | BIT9 | BIT8))
#define ACX100_RXSTATUS_UNDECR		((UINT16)BIT1)
#define ACX100_RXSTATUS_FCSERR		((UINT16)BIT0)

/*--- Communication Frames: Test/Get/Set Field Values for Receive Frames -----*/
#define ACX100_RXSTATUS_MSGTYPE_GET(value) \
		((UINT16)((((UINT16)(value)) & ACX100_RXSTATUS_MSGTYPE) >> 13))
#define ACX100_RXSTATUS_MSGTYPE_SET(value) \
		((UINT16)(((UINT16)(value)) << 13))
#define ACX100_RXSTATUS_MACPORT_GET(value) \
		((UINT16)((((UINT16)(value)) & ACX100_RXSTATUS_MACPORT) >> 8))
#define ACX100_RXSTATUS_MACPORT_SET(value) \
		((UINT16)(((UINT16)(value)) << 8))
#define ACX100_RXSTATUS_ISUNDECR(value)	 \
		((UINT16)(((UINT16)(value)) & ACX100_RXSTATUS_UNDECR))
#define ACX100_RXSTATUS_ISFCSERR(value)	 \
		((UINT16)(((UINT16)(value)) & ACX100_RXSTATUS_FCSERR))

/*============================================================================*
 * Information Frames Structures                                              *
 *============================================================================*/

/*--- Information Types ------------------------------------------------------*/
#define ACX100_IT_HANDOVERADDR		((UINT16)0xF000UL)
#define ACX100_IT_COMMTALLIES		((UINT16)0xF100UL)
#define ACX100_IT_SCANRESULTS		((UINT16)0xF101UL)
#define ACX100_IT_CHINFORESULTS		((UINT16)0xF102UL)
#define ACX100_IT_HOSTSCANRESULTS	((UINT16)0xF103UL)	/* NEW */
#define ACX100_IT_LINKSTATUS		((UINT16)0xF200UL)
#define ACX100_IT_ASSOCSTATUS		((UINT16)0xF201UL)
#define ACX100_IT_AUTHREQ		((UINT16)0xF202UL)
#define ACX100_IT_PSUSERCNT		((UINT16)0xF203UL)
#define ACX100_IT_KEYIDCHANGED		((UINT16)0xF204UL)	/* NEW AP */

/*-- Notification Frame, MAC Mgmt: Handover Address --*/
typedef struct acx100_HandoverAddr {
	UINT16 framelen __WLAN_ATTRIB_PACK__;
	UINT16 infotype __WLAN_ATTRIB_PACK__;
	UINT8 handover_addr[WLAN_BSSID_LEN] __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_HandoverAddr_t;

/*-- Inquiry Frame, Diagnose: Communication Tallies --*/
typedef struct __WLAN_ATTRIB_PACK__ acx100_CommTallies16 {
	UINT16 txunicastframes __WLAN_ATTRIB_PACK__;
	UINT16 txmulticastframes __WLAN_ATTRIB_PACK__;
	UINT16 txfragments __WLAN_ATTRIB_PACK__;
	UINT16 txunicastoctets __WLAN_ATTRIB_PACK__;
	UINT16 txmulticastoctets __WLAN_ATTRIB_PACK__;
	UINT16 txdeferredtrans __WLAN_ATTRIB_PACK__;
	UINT16 txsingleretryframes __WLAN_ATTRIB_PACK__;
	UINT16 txmultipleretryframes __WLAN_ATTRIB_PACK__;
	UINT16 txretrylimitexceeded __WLAN_ATTRIB_PACK__;
	UINT16 txdiscards __WLAN_ATTRIB_PACK__;
	UINT16 rxunicastframes __WLAN_ATTRIB_PACK__;
	UINT16 rxmulticastframes __WLAN_ATTRIB_PACK__;
	UINT16 rxfragments __WLAN_ATTRIB_PACK__;
	UINT16 rxunicastoctets __WLAN_ATTRIB_PACK__;
	UINT16 rxmulticastoctets __WLAN_ATTRIB_PACK__;
	UINT16 rxfcserrors __WLAN_ATTRIB_PACK__;
	UINT16 rxdiscardsnobuffer __WLAN_ATTRIB_PACK__;
	UINT16 txdiscardswrongsa __WLAN_ATTRIB_PACK__;
	UINT16 rxdiscardswepundecr __WLAN_ATTRIB_PACK__;
	UINT16 rxmsginmsgfrag __WLAN_ATTRIB_PACK__;
	UINT16 rxmsginbadmsgfrag __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_CommTallies16_t;

typedef struct __WLAN_ATTRIB_PACK__ acx100_CommTallies32 {
	UINT32 txunicastframes __WLAN_ATTRIB_PACK__;
	UINT32 txmulticastframes __WLAN_ATTRIB_PACK__;
	UINT32 txfragments __WLAN_ATTRIB_PACK__;
	UINT32 txunicastoctets __WLAN_ATTRIB_PACK__;
	UINT32 txmulticastoctets __WLAN_ATTRIB_PACK__;
	UINT32 txdeferredtrans __WLAN_ATTRIB_PACK__;
	UINT32 txsingleretryframes __WLAN_ATTRIB_PACK__;
	UINT32 txmultipleretryframes __WLAN_ATTRIB_PACK__;
	UINT32 txretrylimitexceeded __WLAN_ATTRIB_PACK__;
	UINT32 txdiscards __WLAN_ATTRIB_PACK__;
	UINT32 rxunicastframes __WLAN_ATTRIB_PACK__;
	UINT32 rxmulticastframes __WLAN_ATTRIB_PACK__;
	UINT32 rxfragments __WLAN_ATTRIB_PACK__;
	UINT32 rxunicastoctets __WLAN_ATTRIB_PACK__;
	UINT32 rxmulticastoctets __WLAN_ATTRIB_PACK__;
	UINT32 rxfcserrors __WLAN_ATTRIB_PACK__;
	UINT32 rxdiscardsnobuffer __WLAN_ATTRIB_PACK__;
	UINT32 txdiscardswrongsa __WLAN_ATTRIB_PACK__;
	UINT32 rxdiscardswepundecr __WLAN_ATTRIB_PACK__;
	UINT32 rxmsginmsgfrag __WLAN_ATTRIB_PACK__;
	UINT32 rxmsginbadmsgfrag __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_CommTallies32_t;

/*-- Inquiry Frame, Diagnose: Scan Results & Subfields --*/
typedef struct acx100_ScanResultSub {
	UINT16 chid __WLAN_ATTRIB_PACK__;
	UINT16 anl __WLAN_ATTRIB_PACK__;
	UINT16 sl __WLAN_ATTRIB_PACK__;
	UINT8 bssid[WLAN_BSSID_LEN] __WLAN_ATTRIB_PACK__;
	UINT16 bcnint __WLAN_ATTRIB_PACK__;
	UINT16 capinfo __WLAN_ATTRIB_PACK__;
	acx100_bytestr32_t ssid __WLAN_ATTRIB_PACK__;
	UINT8 supprates[10] __WLAN_ATTRIB_PACK__;	/* 802.11 info element */
	UINT16 proberesp_rate __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_ScanResultSub_t;

typedef struct acx100_ScanResult {
	UINT16 rsvd __WLAN_ATTRIB_PACK__;
	UINT16 scanreason __WLAN_ATTRIB_PACK__;
	 acx100_ScanResultSub_t
	    result[ACX100_SCANRESULT_MAX] __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_ScanResult_t;

/*-- Inquiry Frame, Diagnose: ChInfo Results & Subfields --*/
typedef struct acx100_ChInfoResultSub {
	UINT16 chid __WLAN_ATTRIB_PACK__;
	UINT16 anl __WLAN_ATTRIB_PACK__;
	UINT16 pnl __WLAN_ATTRIB_PACK__;
	UINT16 active __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_ChInfoResultSub_t;

#define ACX100_CHINFORESULT_BSSACTIVE	BIT0
#define ACX100_CHINFORESULT_PCFACTIVE	BIT1

typedef struct acx100_ChInfoResult {
	UINT16 scanchannels __WLAN_ATTRIB_PACK__;
	 acx100_ChInfoResultSub_t
	    result[ACX100_CHINFORESULT_MAX] __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_ChInfoResult_t;

/*-- Inquiry Frame, Diagnose: Host Scan Results & Subfields --*/
typedef struct acx100_HScanResultSub {
	UINT16 chid __WLAN_ATTRIB_PACK__;
	UINT16 anl __WLAN_ATTRIB_PACK__;
	UINT16 sl __WLAN_ATTRIB_PACK__;
	UINT8 bssid[WLAN_BSSID_LEN] __WLAN_ATTRIB_PACK__;
	UINT16 bcnint __WLAN_ATTRIB_PACK__;
	UINT16 capinfo __WLAN_ATTRIB_PACK__;
	acx100_bytestr32_t ssid __WLAN_ATTRIB_PACK__;
	UINT8 supprates[10] __WLAN_ATTRIB_PACK__;	/* 802.11 info element */
	UINT16 proberesp_rate __WLAN_ATTRIB_PACK__;
	UINT16 atim __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_HScanResultSub_t;

typedef struct acx100_HScanResult {
	UINT16 nresult __WLAN_ATTRIB_PACK__;
	UINT16 rsvd __WLAN_ATTRIB_PACK__;
	 acx100_HScanResultSub_t
	    result[ACX100_HSCANRESULT_MAX] __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_HScanResult_t;

/*-- Unsolicited Frame, MAC Mgmt: LinkStatus --*/
#define ACX100_LINK_NOTCONNECTED	((UINT16)0)
#define ACX100_LINK_CONNECTED		((UINT16)1)
#define ACX100_LINK_DISCONNECTED	((UINT16)2)
#define ACX100_LINK_AP_CHANGE		((UINT16)3)
#define ACX100_LINK_AP_OUTOFRANGE	((UINT16)4)
#define ACX100_LINK_AP_INRANGE		((UINT16)5)
#define ACX100_LINK_ASSOCFAIL		((UINT16)6)

typedef struct acx100_LinkStatus {
	UINT16 linkstatus __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_LinkStatus_t;

/*-- Unsolicited Frame, MAC Mgmt: AssociationStatus --*/
#define ACX100_ASSOCSTATUS_STAASSOC	((UINT16)1)
#define ACX100_ASSOCSTATUS_REASSOC	((UINT16)2)
#define ACX100_ASSOCSTATUS_DISASSOC	((UINT16)3)
#define ACX100_ASSOCSTATUS_ASSOCFAIL	((UINT16)4)
#define ACX100_ASSOCSTATUS_AUTHFAIL	((UINT16)5)

typedef struct acx100_AssocStatus {
	UINT16 assocstatus __WLAN_ATTRIB_PACK__;
	UINT8 sta_addr[WLAN_ADDR_LEN] __WLAN_ATTRIB_PACK__;
	/* old_ap_addr is only valid if assocstatus == 2 */
	UINT8 old_ap_addr[WLAN_ADDR_LEN] __WLAN_ATTRIB_PACK__;
	UINT16 reason __WLAN_ATTRIB_PACK__;
	UINT16 reserved __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_AssocStatus_t;

/*-- Unsolicited Frame, MAC Mgmt: AuthRequest (AP Only) --*/
typedef struct acx100_AuthRequest {
	UINT8 sta_addr[WLAN_ADDR_LEN] __WLAN_ATTRIB_PACK__;
	UINT16 algorithm __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_AuthReq_t;

/*-- Unsolicited Frame, MAC Mgmt: PSUserCount (AP Only) --*/
typedef struct acx100_PSUserCount {
	UINT16 usercnt __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_PSUserCount_t;

/*-- Collection of all Inf frames --*/
typedef union acx100_infodata {
	acx100_CommTallies16_t commtallies16 __WLAN_ATTRIB_PACK__;
	acx100_CommTallies32_t commtallies32 __WLAN_ATTRIB_PACK__;
	acx100_ScanResult_t scanresult __WLAN_ATTRIB_PACK__;
	acx100_ChInfoResult_t chinforesult __WLAN_ATTRIB_PACK__;
	acx100_HScanResult_t hscanresult __WLAN_ATTRIB_PACK__;
	acx100_LinkStatus_t linkstatus __WLAN_ATTRIB_PACK__;
	acx100_AssocStatus_t assocstatus __WLAN_ATTRIB_PACK__;
	acx100_AuthReq_t authreq __WLAN_ATTRIB_PACK__;
	acx100_PSUserCount_t psusercnt __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_infodata_t;

typedef struct acx100_InfFrame {
	UINT16 framelen __WLAN_ATTRIB_PACK__;
	UINT16 infotype __WLAN_ATTRIB_PACK__;
	acx100_infodata_t info __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_InfFrame_t;

#if (WLAN_HOSTIF == WLAN_USB)

/*============================================================================*
 * USB Packet structures and constants                                        *
 *============================================================================*/

/* Should be sent to the ctrlout endpoint */
#define ACX100_USB_ENBULKIN	6

/* Should be sent to the bulkout endpoint */
#define ACX100_USB_TXFRM	0
#define ACX100_USB_CMDREQ	1
#define ACX100_USB_WRIDREQ	2
#define ACX100_USB_RRIDREQ	3
#define ACX100_USB_WMEMREQ	4
#define ACX100_USB_RMEMREQ	5
#define ACX100_USB_UPLOAD_FW 0x10
#define ACX100_USB_ACK_CS 0x11
#define ACX100_USB_UNKNOWN_REQ1 0x12
#define ACX100_USB_TX_DESC 0xA

/* Received from the bulkin endpoint */
#define ACX100_USB_ISFRM(a)	((a) < 0x7fff)
#define ACX100_USB_ISTXFRM(a)	(ACX100_USB_ISFRM((a)) && ((a) & 0x1000))
#define ACX100_USB_ISRXFRM(a)	(ACX100_USB_ISFRM((a)) && !((a) & 0x1000))
#define ACX100_USB_INFOFRM	0x8000
#define ACX100_USB_CMDRESP	0x8001
#define ACX100_USB_WRIDRESP	0x8002
#define ACX100_USB_RRIDRESP	0x8003
#define ACX100_USB_WMEMRESP	0x8004
#define ACX100_USB_RMEMRESP	0x8005
#define ACX100_USB_BUFAVAIL	0x8006
#define ACX100_USB_ERROR	0x8007

#define ACX100_USB_TXHI_ISDATA     0x1
#define ACX100_USB_TXHI_DIRECTED   0x2
#define ACX100_USB_TXHI_BROADCAST  0x4

#define ACX100_USB_CTL_PREAMBLE   0x01
#define ACX100_USB_CTL_FIRSTFRAG  0x02
#define ACX100_USB_CTL_AUTODMA    0x04
#define ACX100_USB_CTL_RECLAIM    0x08
#define ACX100_USB_CTL_HOSTDONE   0x20
#define ACX100_USB_CTL_ACXDONE    0x40
#define ACX100_USB_CTL_OWN        0x80

#define ACX100_USB_CTL2_FCS       0x02
#define ACX100_USB_CTL2_MORE_FRAG 0x04
#define ACX100_USB_CTL2_RTS       0x20


/*--- Request (bulk OUT) packet contents -------------------------------------*/

typedef struct acx100_usb_txfrm {
  UINT16 desc __WLAN_ATTRIB_PACK__;
  UINT16 MPDUlen __WLAN_ATTRIB_PACK__;
  UINT16 index __WLAN_ATTRIB_PACK__;
  UINT16 txRate __WLAN_ATTRIB_PACK__;
  UINT32 hostData __WLAN_ATTRIB_PACK__;
  UINT8  ctrl1 __WLAN_ATTRIB_PACK__;
  UINT8  ctrl2 __WLAN_ATTRIB_PACK__;
  UINT16 dataLength __WLAN_ATTRIB_PACK__;
  UINT8 data[WLAN_DATA_MAXLEN];
} acx100_usb_txfrm_t __WLAN_ATTRIB_PACK__;

typedef struct acx100_usb_scan {
	UINT16 cmd __WLAN_ATTRIB_PACK__;
	UINT16 pad0 __WLAN_ATTRIB_PACK__;
	UINT16 unk1 __WLAN_ATTRIB_PACK__;
	UINT16 unk2 __WLAN_ATTRIB_PACK__;
} acx100_usb_scan_t __WLAN_ATTRIB_PACK__;


typedef struct acx100_usb_cmdreq {
	UINT16 type __WLAN_ATTRIB_PACK__;
	UINT16 cmd __WLAN_ATTRIB_PACK__;
	UINT16 parm0 __WLAN_ATTRIB_PACK__;
	UINT16 parm1 __WLAN_ATTRIB_PACK__;
	UINT16 parm2 __WLAN_ATTRIB_PACK__;
	UINT8 pad[54] __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_usb_cmdreq_t;

typedef struct acx100_usb_wridreq {
	UINT16 cmd __WLAN_ATTRIB_PACK__;
	UINT16 status  __WLAN_ATTRIB_PACK__;
	UINT16 rid __WLAN_ATTRIB_PACK__;
	UINT16 frmlen __WLAN_ATTRIB_PACK__;
	UINT8 data[ACX100_RIDDATA_MAXLEN] __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_usb_wridreq_t;

typedef struct acx100_usb_rridreq {
	UINT16 cmd __WLAN_ATTRIB_PACK__;
	UINT16 status __WLAN_ATTRIB_PACK__;
	UINT16 rid __WLAN_ATTRIB_PACK__;
	UINT16 frmlen __WLAN_ATTRIB_PACK__;
	UINT8 pad[56] __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_usb_rridreq_t;

typedef struct acx100_usb_wmemreq {
	UINT16 cmd __WLAN_ATTRIB_PACK__;
	UINT16 status __WLAN_ATTRIB_PACK__;
	/*
  UINT16 offset __WLAN_ATTRIB_PACK__;
	UINT16 page __WLAN_ATTRIB_PACK__;
  */
	UINT8 data[ACX100_USB_RWMEM_MAXLEN] __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_usb_wmemreq_t;

typedef struct acx100_usb_rxtx_ctrl {
	UINT16 cmd __WLAN_ATTRIB_PACK__;
	UINT16 status __WLAN_ATTRIB_PACK__;
	UINT8 data __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_usb_rxtx_ctrl_t;

typedef struct acx100_usb_rmemreq {
	UINT16 type __WLAN_ATTRIB_PACK__;
	UINT16 frmlen __WLAN_ATTRIB_PACK__;
	UINT16 offset __WLAN_ATTRIB_PACK__;
	UINT16 page __WLAN_ATTRIB_PACK__;
	UINT8 pad[56] __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_usb_rmemreq_t;

/*--- Response (bulk IN) packet contents -------------------------------------*/

typedef struct acx100_usb_rxfrm {
	acx100_rx_frame_t desc __WLAN_ATTRIB_PACK__;
	UINT8 data[WLAN_DATA_MAXLEN] __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_usb_rxfrm_t;

typedef struct acx100_usb_infofrm {
	UINT16 type __WLAN_ATTRIB_PACK__;
	acx100_InfFrame_t info __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_usb_infofrm_t;

typedef struct acx100_usb_cmdresp {
	UINT16 type __WLAN_ATTRIB_PACK__;
	UINT16 status __WLAN_ATTRIB_PACK__;
	UINT16 resp0 __WLAN_ATTRIB_PACK__;
	UINT16 resp1 __WLAN_ATTRIB_PACK__;
	UINT16 resp2 __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_usb_cmdresp_t;

typedef struct acx100_usb_wridresp {
	UINT16 type __WLAN_ATTRIB_PACK__;
	UINT16 status __WLAN_ATTRIB_PACK__;
	UINT16 resp0 __WLAN_ATTRIB_PACK__;
	UINT16 resp1 __WLAN_ATTRIB_PACK__;
	UINT16 resp2 __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_usb_wridresp_t;

typedef struct acx100_usb_rridresp {
	UINT16 cmd __WLAN_ATTRIB_PACK__;
	UINT16 status __WLAN_ATTRIB_PACK__;
	UINT16 rid __WLAN_ATTRIB_PACK__;
	UINT16 frmlen __WLAN_ATTRIB_PACK__;
	UINT8 data[ACX100_RIDDATA_MAXLEN] __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_usb_rridresp_t;

typedef struct acx100_usb_wmemresp {
	UINT16 type __WLAN_ATTRIB_PACK__;
	UINT16 status __WLAN_ATTRIB_PACK__;
	UINT16 resp0 __WLAN_ATTRIB_PACK__;
	UINT16 resp1 __WLAN_ATTRIB_PACK__;
	UINT16 resp2 __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_usb_wmemresp_t;

typedef struct acx100_usb_rmemresp {
	UINT16 type __WLAN_ATTRIB_PACK__;
	UINT16 frmlen __WLAN_ATTRIB_PACK__;
	UINT8 data[ACX100_USB_RWMEM_MAXLEN] __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_usb_rmemresp_t;

typedef struct acx100_usb_bufavail {
	UINT16 type __WLAN_ATTRIB_PACK__;
	UINT16 frmlen __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_usb_bufavail_t;

typedef struct acx100_usb_error {
	UINT16 type __WLAN_ATTRIB_PACK__;
	UINT16 errortype __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_usb_error_t;

/*--- Unions for packaging all the known packet types together ---------------*/

typedef union acx100_usbout {
  UINT16 type __WLAN_ATTRIB_PACK__;
	acx100_usb_txfrm_t txfrm __WLAN_ATTRIB_PACK__;
	acx100_usb_cmdreq_t cmdreq __WLAN_ATTRIB_PACK__;
	acx100_usb_wridreq_t wridreq __WLAN_ATTRIB_PACK__;
	acx100_usb_rridreq_t rridreq __WLAN_ATTRIB_PACK__;
	acx100_usb_wmemreq_t wmemreq __WLAN_ATTRIB_PACK__;
	acx100_usb_rmemreq_t rmemreq __WLAN_ATTRIB_PACK__;
	acx100_usb_rxtx_ctrl_t rxtx __WLAN_ATTRIB_PACK__;
	acx100_usb_scan_t scan __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_usbout_t;

typedef union acx100_usbin {
	UINT16 type __WLAN_ATTRIB_PACK__;
	acx100_usb_rxfrm_t rxfrm __WLAN_ATTRIB_PACK__;
	acx100_usb_txfrm_t txfrm __WLAN_ATTRIB_PACK__;
	acx100_usb_infofrm_t infofrm __WLAN_ATTRIB_PACK__;
	acx100_usb_cmdresp_t cmdresp __WLAN_ATTRIB_PACK__;
	acx100_usb_wridresp_t wridresp __WLAN_ATTRIB_PACK__;
	acx100_usb_rridresp_t rridresp __WLAN_ATTRIB_PACK__;
	acx100_usb_wmemresp_t wmemresp __WLAN_ATTRIB_PACK__;
	acx100_usb_rmemresp_t rmemresp __WLAN_ATTRIB_PACK__;
	acx100_usb_bufavail_t bufavail __WLAN_ATTRIB_PACK__;
	acx100_usb_error_t usberror __WLAN_ATTRIB_PACK__;
	UINT8 boguspad[3000] __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_usbin_t;

#endif /* WLAN_HOSTIF == WLAN_USB */

/*============================================================================*
 * PD record structures                                                       *
 *============================================================================*/

typedef struct acx100_pdr_pcb_partnum {
	UINT8 num[8] __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_pdr_pcb_partnum_t;

typedef struct acx100_pdr_pcb_tracenum {
	UINT8 num[8] __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_pdr_pcb_tracenum_t;

typedef struct acx100_pdr_nic_serial {
	UINT8 num[12] __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_pdr_nic_serial_t;

typedef struct acx100_pdr_mkk_measurements {
	double carrier_freq __WLAN_ATTRIB_PACK__;
	double occupied_band __WLAN_ATTRIB_PACK__;
	double power_density __WLAN_ATTRIB_PACK__;
	double tx_spur_f1 __WLAN_ATTRIB_PACK__;
	double tx_spur_f2 __WLAN_ATTRIB_PACK__;
	double tx_spur_f3 __WLAN_ATTRIB_PACK__;
	double tx_spur_f4 __WLAN_ATTRIB_PACK__;
	double tx_spur_l1 __WLAN_ATTRIB_PACK__;
	double tx_spur_l2 __WLAN_ATTRIB_PACK__;
	double tx_spur_l3 __WLAN_ATTRIB_PACK__;
	double tx_spur_l4 __WLAN_ATTRIB_PACK__;
	double rx_spur_f1 __WLAN_ATTRIB_PACK__;
	double rx_spur_f2 __WLAN_ATTRIB_PACK__;
	double rx_spur_l1 __WLAN_ATTRIB_PACK__;
	double rx_spur_l2 __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_pdr_mkk_measurements_t;

typedef struct acx100_pdr_nic_ramsize {
	UINT8 size[12] __WLAN_ATTRIB_PACK__;	/* units of KB */
} __WLAN_ATTRIB_PACK__ acx100_pdr_nic_ramsize_t;

typedef struct acx100_pdr_mfisuprange {
	UINT16 id __WLAN_ATTRIB_PACK__;
	UINT16 variant __WLAN_ATTRIB_PACK__;
	UINT16 bottom __WLAN_ATTRIB_PACK__;
	UINT16 top __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_pdr_mfisuprange_t;

typedef struct acx100_pdr_cfisuprange {
	UINT16 id __WLAN_ATTRIB_PACK__;
	UINT16 variant __WLAN_ATTRIB_PACK__;
	UINT16 bottom __WLAN_ATTRIB_PACK__;
	UINT16 top __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_pdr_cfisuprange_t;

typedef struct acx100_pdr_nicid {
	UINT16 id __WLAN_ATTRIB_PACK__;
	UINT16 variant __WLAN_ATTRIB_PACK__;
	UINT16 major __WLAN_ATTRIB_PACK__;
	UINT16 minor __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_pdr_nicid_t;

typedef struct acx100_pdr_refdac_measurements {
	UINT16 value[0] __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_pdr_refdac_measurements_t;

typedef struct acx100_pdr_vgdac_measurements {
	UINT16 value[0] __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_pdr_vgdac_measurements_t;

typedef struct acx100_pdr_level_comp_measurements {
	UINT16 value[0] __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_pdr_level_compc_measurements_t;

typedef struct acx100_pdr_mac_address {
	UINT8 addr[6] __WLAN_ATTRIB_PACK__;
	UINT short_addr[6] __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_pdr_mac_address_t;

typedef struct acx100_pdr_mkk_callname {
	UINT8 callname[8] __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_pdr_mkk_callname_t;

typedef struct acx100_pdr_regdomain {
	UINT16 numdomains __WLAN_ATTRIB_PACK__;
	UINT16 domain[5] __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_pdr_regdomain_t;

typedef struct acx100_pdr_allowed_channel {
	UINT16 ch_bitmap __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_pdr_allowed_channel_t;

typedef struct acx100_pdr_default_channel {
	UINT16 channel __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_pdr_default_channel_t;

typedef struct acx100_pdr_privacy_option {
	UINT16 available __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_pdr_privacy_option_t;

typedef struct acx100_pdr_temptype {
	UINT16 type __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_pdr_temptype_t;

typedef struct acx100_pdr_refdac_setup {
	UINT16 ch_value[14] __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_pdr_refdac_setup_t;

typedef struct acx100_pdr_vgdac_setup {
	UINT16 ch_value[14] __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_pdr_vgdac_setup_t;

typedef struct acx100_pdr_level_comp_setup {
	UINT16 ch_value[14] __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_pdr_level_comp_setup_t;

typedef struct acx100_pdr_trimdac_setup {
	UINT16 trimidac __WLAN_ATTRIB_PACK__;
	UINT16 trimqdac __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_pdr_trimdac_setup_t;

typedef struct acx100_pdr_ifr_setting {
	UINT16 value[3] __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_pdr_ifr_setting_t;

typedef struct acx100_pdr_rfr_setting {
	UINT16 value[3] __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_pdr_rfr_setting_t;

typedef struct acx100_pdr_hfa3861_baseline {
	UINT16 value[50] __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_pdr_hfa3861_baseline_t;

typedef struct acx100_pdr_hfa3861_shadow {
	UINT32 value[32] __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_pdr_hfa3861_shadow_t;

typedef struct acx100_pdr_hfa3861_ifrf {
	UINT32 value[20] __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_pdr_hfa3861_ifrf_t;

typedef struct acx100_pdr_hfa3861_chcalsp {
	UINT16 value[14] __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_pdr_hfa3861_chcalsp_t;

typedef struct acx100_pdr_hfa3861_chcali {
	UINT16 value[17] __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_pdr_hfa3861_chcali_t;

typedef struct acx100_pdr_hfa3861_nic_config {
	UINT16 config_bitmap __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_pdr_nic_config_t;

typedef struct acx100_pdr_hfa3861_manf_testsp {
	UINT16 value[30] __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_pdr_hfa3861_manf_testsp_t;

typedef struct acx100_pdr_hfa3861_manf_testi {
	UINT16 value[30] __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_pdr_hfa3861_manf_testi_t;

typedef struct acx100_end_of_pda {
	UINT16 crc __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_pdr_end_of_pda_t;

typedef struct acx100_pdrec {
	UINT16 len __WLAN_ATTRIB_PACK__;	/* in words */
	UINT16 code __WLAN_ATTRIB_PACK__;
	union pdr {
		acx100_pdr_pcb_partnum_t pcb_partnum __WLAN_ATTRIB_PACK__;
		acx100_pdr_pcb_tracenum_t pcb_tracenum __WLAN_ATTRIB_PACK__;
		acx100_pdr_nic_serial_t nic_serial __WLAN_ATTRIB_PACK__;
		acx100_pdr_mkk_measurements_t mkk_measurements
		    __WLAN_ATTRIB_PACK__;
		acx100_pdr_nic_ramsize_t nic_ramsize __WLAN_ATTRIB_PACK__;
		acx100_pdr_mfisuprange_t mfisuprange __WLAN_ATTRIB_PACK__;
		acx100_pdr_cfisuprange_t cfisuprange __WLAN_ATTRIB_PACK__;
		acx100_pdr_nicid_t nicid __WLAN_ATTRIB_PACK__;
		acx100_pdr_refdac_measurements_t refdac_measurements
		    __WLAN_ATTRIB_PACK__;
		acx100_pdr_vgdac_measurements_t vgdac_measurements
		    __WLAN_ATTRIB_PACK__;
		 acx100_pdr_level_compc_measurements_t
		    level_compc_measurements __WLAN_ATTRIB_PACK__;
		acx100_pdr_mac_address_t mac_address __WLAN_ATTRIB_PACK__;
		acx100_pdr_mkk_callname_t mkk_callname __WLAN_ATTRIB_PACK__;
		acx100_pdr_regdomain_t regdomain __WLAN_ATTRIB_PACK__;
		acx100_pdr_allowed_channel_t allowed_channel
		    __WLAN_ATTRIB_PACK__;
		acx100_pdr_default_channel_t default_channel
		    __WLAN_ATTRIB_PACK__;
		acx100_pdr_privacy_option_t privacy_option __WLAN_ATTRIB_PACK__;
		acx100_pdr_temptype_t temptype __WLAN_ATTRIB_PACK__;
		acx100_pdr_refdac_setup_t refdac_setup __WLAN_ATTRIB_PACK__;
		acx100_pdr_vgdac_setup_t vgdac_setup __WLAN_ATTRIB_PACK__;
		acx100_pdr_level_comp_setup_t level_comp_setup
		    __WLAN_ATTRIB_PACK__;
		acx100_pdr_trimdac_setup_t trimdac_setup __WLAN_ATTRIB_PACK__;
		acx100_pdr_ifr_setting_t ifr_setting __WLAN_ATTRIB_PACK__;
		acx100_pdr_rfr_setting_t rfr_setting __WLAN_ATTRIB_PACK__;
		acx100_pdr_hfa3861_baseline_t hfa3861_baseline
		    __WLAN_ATTRIB_PACK__;
		acx100_pdr_hfa3861_shadow_t hfa3861_shadow __WLAN_ATTRIB_PACK__;
		acx100_pdr_hfa3861_ifrf_t hfa3861_ifrf __WLAN_ATTRIB_PACK__;
		acx100_pdr_hfa3861_chcalsp_t hfa3861_chcalsp
		    __WLAN_ATTRIB_PACK__;
		acx100_pdr_hfa3861_chcali_t hfa3861_chcali __WLAN_ATTRIB_PACK__;
		acx100_pdr_nic_config_t nic_config __WLAN_ATTRIB_PACK__;
		acx100_pdr_hfa3861_manf_testsp_t hfa3861_manf_testsp
		    __WLAN_ATTRIB_PACK__;
		acx100_pdr_hfa3861_manf_testi_t hfa3861_manf_testi
		    __WLAN_ATTRIB_PACK__;
		acx100_pdr_end_of_pda_t end_of_pda __WLAN_ATTRIB_PACK__;
	} data __WLAN_ATTRIB_PACK__;
} __WLAN_ATTRIB_PACK__ acx100_pdrec_t;

/*--- Firmware statistics ----------------------------------------------------*/
typedef struct fw_stats {
	UINT val0x0;		/* hdr; */
	UINT val0x4;
	UINT tx_desc_of;
	UINT rx_oom;
	UINT rx_hdr_of;
	UINT rx_hdr_use_next;
	UINT rx_dropped_frame;
	UINT rx_frame_ptr_err;

	UINT rx_dma_req;	/* val0x1c */
	UINT rx_dma_err;	/* val0x20 */
	UINT tx_dma_req;
	UINT tx_dma_err;	/* val0x28 */

	UINT cmd_cplt;
	UINT fiq;
	UINT rx_hdrs;		/* val0x34 */
	UINT rx_cmplt;		/* val0x38 */
	UINT rx_mem_of;		/* val0x3c */
	UINT rx_rdys;
	UINT irqs;
	UINT acx_trans_procs;
	UINT decrypt_done;	/* val0x48 */
	UINT dma_0_done;
	UINT dma_1_done;
	UINT tx_exch_complet;
	UINT commands;
	UINT acx_rx_procs;
	UINT hw_pm_mode_changes;
	UINT host_acks;
	UINT pci_pm;
	UINT acm_wakeups;

	UINT wep_key_count;
	UINT wep_default_key_count;
	UINT dot11_def_key_mib;
	UINT wep_key_not_found;
	UINT wep_decrypt_fail;
} fw_stats_t;

/*--- IEEE 802.11 header -----------------------------------------------------*/
/* FIXME: acx100_addr3_t should probably actually be discarded in favour of the
 * identical linux-wlan-ng p80211_hdr_t. An even better choice would be to use
 * the kernel's struct ieee80_11_hdr from driver/net/wireless/ieee802_11.h */
typedef struct acx100_addr3 {
	/* IEEE 802.11-1999.pdf chapter 7 might help */
	UINT16 frame_control __WLAN_ATTRIB_PACK__;	/* 0x00, wlan-ng name */
	UINT16 duration_id __WLAN_ATTRIB_PACK__;	/* 0x02, wlan-ng name */
	char address1[0x6] __WLAN_ATTRIB_PACK__;	/* 0x04, wlan-ng name */
	char address2[0x6] __WLAN_ATTRIB_PACK__;	/* 0x0a */
	char address3[0x6] __WLAN_ATTRIB_PACK__;	/* 0x10 */
	UINT16 sequence_control __WLAN_ATTRIB_PACK__;	/* 0x16 */
	UINT8 *val0x18;
	struct sk_buff *val0x1c;
	struct sk_buff *val0x20;
} acx100_addr3_t;

/*--- WEP stuff --------------------------------------------------------------*/
#define NUM_WEPKEYS			4
#define MAX_KEYLEN			32

#define HOSTWEP_DEFAULTKEY_MASK		(BIT1 | BIT0)
#define HOSTWEP_DECRYPT			BIT4
#define HOSTWEP_ENCRYPT			BIT5
#define HOSTWEP_PRIVACYINVOKED		BIT6
#define HOSTWEP_EXCLUDEUNENCRYPTED	BIT7

typedef struct wep_key {
	UINT32 index;
	UINT16 size;
	UINT8 key[29];
	UINT16 strange_filler;
} wep_key_t;			/* size = 264 bytes (33*8) */
/* FIXME: We don't have size 264! Or is there 2 bytes beyond the key
 * (strange_filler)? */

typedef struct key_struct {
	UINT8 addr[WLAN_ADDR_LEN];	/* 0x00 */
	UINT16 filler1;		/* 0x06 */
	UINT32 filler2;		/* 0x08 */
	UINT32 index;		/* 0x0c */
	UINT16 len;		/* 0x10 */
	UINT8 key[29];		/* 0x12; is this long enough??? */
} key_struct_t;			/* size = 276. FIXME: where is the remaining space?? */

/*--- Tx and Rx descriptor ring buffer administration ------------------------*/
typedef struct TIWLAN_DC {	/* V3 version */
	struct wlandevice *wlandev;

	UINT val0x4;		//spacing

	UINT ui32ACXTxQueueStart;	/* 0x8, official name */
	UINT ui32ACXRxQueueStart;	/* 0xc */

	UINT8 *pTxBufferPool;	/* 0x10 */
	dma_addr_t TxBufferPoolPhyAddr;	/* 0x14 */
	UINT TxBufferPoolSize;	/* 0x18 */

	struct txdescriptor *pTxDescQPool;	/* V13POS 0x1c, official name */
	UINT tx_pool_count;	/* 0x20 indicates # of ring buffer pool entries */
	UINT tx_head;		/* 0x24 current ring buffer pool member index */
	UINT tx_tail;		/* 0x34,pool_idx2 is not correct, I'm
				 * just using it as a ring watch
				 * official name */

	struct framehdr *pFrameHdrQPool;	/* 0x28 */
	UINT FrameHdrQPoolSize;	/* 0x2c */
	dma_addr_t FrameHdrQPoolPhyAddr;	/* 0x30 */

	UINT val0x38;		/* 0x38, official name */

	struct txhostdescriptor *pTxHostDescQPool;	/* V3POS 0x3c, V1POS 0x60 */
	UINT TxHostDescQPoolSize;	/* 0x40 */
	UINT TxHostDescQPoolPhyAddr;	/* 0x44 */

	UINT val0x48;		/* 0x48 */
	UINT val0x4c;

	struct rxdescriptor *pRxDescQPool;	/* V1POS 0x74, V3POS 0x50 */
	UINT rx_pool_count;	/* V1POS 0x78, V3POS 0X54 */
	UINT rx_tail;		/* 0x6c */

	UINT val0x50;		/* V1POS:0x50, some size */
	UINT val0x54;		/* 0x54, official name */

	struct rxhostdescriptor *pRxHostDescQPool;	/* 0x58, is it really rxdescriptor? */
	UINT RxHostDescQPoolSize;	/* 0x5c */
	UINT RxHostDescQPoolPhyAddr;	/* 0x60, official name. */

	UINT val0x64;		/* 0x64, some size */

	UINT *pRxBufferPool;	/* *rxdescq1; 0x70 */
	UINT RxBufferPoolPhyAddr;	/* *rxdescq2; 0x74 */
	UINT RxBufferPoolSize;
} TIWLAN_DC;

/*--- 802.11 Management capabilities -----------------------------------------*/
#define IEEE802_11_MGMT_CAP_ESS		(1 << 0)
#define IEEE802_11_MGMT_CAP_IBSS	(1 << 1)
#define IEEE802_11_MGMT_CAP_CFP_ABLE	(1 << 2)
#define IEEE802_11_MGMT_CAP_CFP_REQ	(1 << 3)
#define IEEE802_11_MGMT_CAP_WEP		(1 << 4)
#define IEEE802_11_MGMT_CAP_SHORT_PRE	(1 << 5)
#define IEEE802_11_MGMT_CAP_PBCC	(1 << 6)
#define IEEE802_11_MGMT_CAP_CHAN_AGIL	(1 << 7)

/*--- 802.11 Basic Service Set info ------------------------------------------*/
typedef struct bss_info {
	u8 bssid[ETH_ALEN];	/* BSSID */
	u8 essid[IW_ESSID_MAX_SIZE + 1];	/* ESSID and trailing '\0'  */
	size_t essid_len;	/* Length of ESSID (FIXME: \0 included?) */
	UINT16 caps;		/* 802.11 capabilities information */
	UINT8 channel;		/* 802.11 channel */
	UINT16 wep;	/* WEP flag (FIXME: redundant, bit 4 in caps) */
	unsigned char supp_rates[64]; /* FIXME: 802.11b section 7.3.2.2 allows 8 rates, but 802.11g devices seem to allow for many more: how many exactly? */
	UINT32 sir;		/* 0x78; Standard IR */
	UINT32 snr;		/* 0x7c; Signal to Noise Ratio */
	UINT16 beacon_interval;	/* 802.11 beacon interval */
} bss_info_t;				/* 132 0x84 */

/*============================================================================*
 * Main acx100 per-device data structure (netdev->priv)                       *
 *============================================================================*/

/* FIXME: this should be named something like struct acx100_priv (typedef'd to
 * acx100_priv_t) */

typedef struct wlandevice {
	/*** Device chain ***/
	struct wlandevice *next;	/* link for list of devices */

	/*** Linux network device ***/
	struct net_device *netdev;	/* pointer to linux netdevice */
	struct net_device *prev_nd;	/* FIXME: We should not chain via our
					 * private struct wlandevice _and_
					 * the struct net_device. */

	/*** Device statistics ***/
	struct net_device_stats stats;	/* net device statistics */
#ifdef WIRELESS_EXT
	struct iw_statistics wstats;	/* wireless statistics */
#endif
/*	struct p80211_frmrx_t rx; *//* 802.11 frame rx statistics */

	/*** Power managment ***/
	struct pm_dev *pm;	/* PM crap */

	/*** USB stuff ***/
#if (WLAN_HOSTIF==WLAN_USB)
	struct usb_device *usbdev;
	acx100_usbout_t usbout;
	acx100_usbin_t usbin;
	acx100_usbin_t bulkin;
	struct urb *urb;
	struct urb *bulkrx_urb;
	struct usb_ctrlrequest usb_setup;
#endif

	/*** Procfs support ***/
#ifdef CONFIG_PROC_FS
	struct proc_dir_entry *proc_entry;
#endif
	/*** Management timer ***/
	struct timer_list mgmt_timer;

	/*** Locking ***/
	spinlock_t lock;	/* mutex for concurrent accesses to structure */

	/*** Hardware identification ***/
	UINT8 form_factor;
	UINT8 radio_type;
	unsigned char eeprom_version;

	/*** Firmware identification ***/
	unsigned char firmware_version[20];
	UINT32 firmware_numver;
	UINT32 firmware_id;

	/*** Hardware resources ***/
	UINT16 irq_mask;	/* interrupts types to mask out (not wanted) */

	UINT membase;		/* 10 */
	UINT membase2;		/* 14 */
	UINT iobase;		/* 18 */
	UINT iobase2;		/* 1c */

	/*** Device status ***/
	int hw_unavailable;	/* indicates whether the hardware has been
				 * suspended or ejected */
	int open;
	UINT8 ifup;		/* whether the device is up */
	int monitor;		/* whether the device is in monitor mode */
	int monitor_setting;
	char led_power;		/* power led status */
	UINT32 get_mask;	/* mask of settings to fetch from the card */
	UINT32 set_mask;	/* mask of settings to write to the card */

	/*** Wireless network settings ***/
	UINT8 dev_addr[MAX_ADDR_LEN];
	UINT8 address[WLAN_ADDR_LEN];
	UINT8 bssid[WLAN_ADDR_LEN];
	UINT8 ap[ETH_ALEN];	/* The AP we want, FF:FF:FF:FF:FF:FF is any */
	UINT32 macmode;		/* This is the mode we're currently in */
	UINT32 mode;		/* That's the MAC mode we want */
	char essid_active;	/* specific ESSID active, or select any? */
	size_t essid_len;		/* to avoid dozens of strlen() */
	char essid[IW_ESSID_MAX_SIZE+1];	/* V3POS 84; essid; INCLUDES \0 termination for easy printf - but many places simply want the string data memcpy'd plus a length indicator! Keep that in mind... */
	char essid_for_assoc[IW_ESSID_MAX_SIZE+1];	/* the ESSID we are going to use for association, in case of "essid 'any'" and in case of hidden ESSID (use configured ESSID then) */
	char nick[IW_ESSID_MAX_SIZE+1]; /* see essid! */
	UINT16 channel;		/* V3POS 22f0, V1POS b8 */
	UINT8 bitrateval;	/* V3POS b8, V1POS ba */
	char bitrate_auto;	/* whether to auto adjust bit rates */
	UINT8 rate_fallback_retries;
	unsigned char reg_dom_id;	/* reg domain setting */
	UINT16 reg_dom_chanmask;
	UINT status;		/* 802.11 association status */
	UINT unknown0x2350;	/* FIXME: old status ?? */
	UINT8 auth_assoc_retries;	/* V3POS 2827, V1POS 27ff */

	UINT8 bss_table_count;		/* # of active BSS scan table entries */
	struct bss_info bss_table[32];	/* BSS scan table */
	struct bss_info station_assoc;	/* the station we're currently associated to */
	char scan_running;
	unsigned long scan_start;
	UINT8 scan_retries;	/* V3POS 2826, V1POS 27fe */

	/* 802.11 power save mode */
	UINT8 ps_wakeup_cfg;
	UINT8 ps_listen_interval;
	UINT8 ps_options;
	UINT8 ps_hangover_period;
	UINT16 ps_enhanced_transition_time;

	/*** PHY settings ***/
	unsigned char tx_level_dbm;
	unsigned char tx_level_val;
	char tx_level_auto; /* whether to do automatic power adjustment */
	unsigned char antenna;	/* antenna settings */
	unsigned char ed_threshold;	/* energy detect threshold */
	unsigned char cca;	/* clear channel assessment */
	char preamble_mode;	/* 0 == Long Preamble, 1 == Short, 2 == Auto */
	char preamble_flag;	/* 0 == Long Preamble, 1 == Short */

	UINT32 short_retry;	/* V3POS 204, V1POS 20c */
	UINT32 long_retry;	/* V3POS 208, V1POS 210 */
	UINT16 msdu_lifetime;	/* V3POS 20c, V1POS 214 */
	UINT32 auth_alg;	/* V3POS 228, V3POS 230, used in transmit_authen1 */
	UINT16 listen_interval;	/* V3POS 250, V1POS 258, given in units of beacon interval */
	UINT32 beacon_interval;	/* V3POS 2300, V1POS c8 */

	UINT16 capabilities;	/* V3POS b0 */
	unsigned char capab_short;	/* V3POS 1ec, V1POS 1f4 */
	unsigned char capab_pbcc;	/* V3POS 1f0, V1POS 1f8 */
	unsigned char capab_agility;	/* V3POS 1f4, V1POS 1fc */
	char rate_spt_len;	/* V3POS 1243, V1POS 124b */
	char rate_support1[5];	/* V3POS 1244, V1POS 124c */
	char rate_support2[5];	/* V3POS 1254, V1POS 125c */

	/*** Encryption settings (WEP) ***/
	UINT32 wep_enabled;
	UINT32 wep_restricted;	/* V3POS c0 */
	UINT32 wep_current_index;	/* V3POS 254, V1POS 25c not sure about this */
	wep_key_t wep_keys[NUM_WEPKEYS];	/* V3POS 268 (it is NOT 260, but 260 plus offset 8!), V1POS 270 */
	key_struct_t wep_key_struct[10];	/* V3POS 688 */

	/*** Card Rx/Tx management ***/
	UINT16 rx_config_1;	/* V3POS 2820, V1POS 27f8 */
	UINT16 rx_config_2;	/* V3POS 2822, V1POS 27fa */
	TIWLAN_DC dc;		/* V3POS 2380, V1POS 2338 */
	UINT TxQueueNo;		/* V3POS 24dc, V1POS 24b4 */
	UINT RxQueueNo;		/* V3POS 24f4, V1POS 24cc */
	int TxQueueFree;
	struct rxhostdescriptor *RxHostDescPoolStart;	/* V3POS 24f8, V1POS 24d0 */
	UINT16 memblocksize;	/* V3POS 2354, V1POS 230c */
	UINT32 val0x24e4;	/* V3POS 24e4, V1POS 24bc */
	UINT32 val0x24e8;	/* V3POS 24e8, V1POS 24c0 */
	UINT32 val0x24fc;	/* V3POS 24fc, V1POS 24d4 */
	UINT32 val0x2500;	/* V3POS 2500, V1POS 24d8 */

	/*** ACX100 command interface ***/
	UINT16 cmd_type;	/* V3POS 2508, V1POS 24e0 */
	UINT16 cmd_status;	/* V3POS 250a, V1POS 24e2 */
	UINT32 CommandParameters;	/* FIXME: used to be an array UINT*0x88,
					 * but it should most likely be *one*
					 * UINT32 instead, pointing to the cmd
					 * param memory. V3POS 268c, V1POS 2664 */
	UINT16 info_type;	/* V3POS 2508, V1POS 24e0 */
	UINT16 info_status;	/* V3POS 250a, V1POS 24e2 */
	UINT InfoParameters;	/* V3POS 2814, V1POS 27ec */

	/*** Unknown ***/
	char dtim_interval;	/* V3POS 2302 */
	char val0x2324[0x8];	/* V3POS 2324 */
} wlandevice_t __WLAN_ATTRIB_PACK__;

/*-- MAC modes --*/
#define WLAN_MACMODE_NONE	0
#define WLAN_MACMODE_IBSS_STA	1
#define WLAN_MACMODE_ESS_STA	2
#define WLAN_MACMODE_ESS_AP	3

/*-- rx_config_1 bitfield --*/
/*  bit     description
 *    13   include additional header (length etc.) *required*
 *    10   receive only own beacon frames
 *     9   discard broadcast (01:xx:xx:xx:xx:xx in mac)
 * 8,7,6   ???
 *     5   BSSID filter
 *     4   promiscuous mode (aka. filter wrong mac addr)
 *     3   receive ALL frames (disable filter)
 *     2   include FCS
 *     1   include additional header (802.11 phy?)
 *     0   ???
 */
#define RX_CFG1_PLUS_ADDIT_HDR		0x2000
#define RX_CFG1_ONLY_OWN_BEACONS	0x0400
#define RX_CFG1_DISABLE_BCAST		0x0200
#define RX_CFG1_FILTER_BSSID		0x0020
#define RX_CFG1_PROMISCUOUS		0x0010
#define RX_CFG1_RCV_ALL_FRAMES		0x0008
#define RX_CFG1_INCLUDE_FCS		0x0004
#define RX_CFG1_INCLUDE_ADDIT_HDR	0x0002

/*-- rx_config_2 bitfield --*/
/*  bit     description
 *    11   receive association requests etc.
 *    10   receive authentication frames
 *     9   receive beacon frames
 *     8   ?? filter on some bit in 802.11 header ??
 *     7   receive control frames
 *     6   receive data frames
 *     5   receive broken frames
 *     4   receive management frames
 *     3   receive probe requests
 *     2   receive probe responses
 *     1   receive ack frames
 *     0   receive other
 */
#define RX_CFG2_RCV_ASSOC_REQ		0x0800
#define RX_CFG2_RCV_AUTH_FRAMES		0x0400
#define RX_CFG2_RCV_BEACON_FRAMES	0x0200
#define RX_CFG2_FILTER_ON_SOME_BIT	0x0100
#define RX_CFG2_RCV_CTRL_FRAMES		0x0080
#define RX_CFG2_RCV_DATA_FRAMES		0x0040
#define RX_CFG2_RCV_BROKEN_FRAMES	0x0020
#define RX_CFG2_RCV_MGMT_FRAMES		0x0010
#define RX_CFG2_RCV_PROBE_REQ		0x0008
#define RX_CFG2_RCV_PROBE_RESP		0x0004
#define RX_CFG2_RCV_ACK_FRAMES		0x0002
#define RX_CFG2_RCV_OTHER		0x0001

/*-- get and set mask values --*/
#define GETSET_WEP		0x00000001
#define GETSET_TXPOWER		0x00000002
#define GETSET_ANTENNA		0x00000004
#define GETSET_ED_THRESH	0x00000008
#define GETSET_CCA		0x00000010
#define GETSET_TX		0x00000020
#define GETSET_RX		0x00000040
#define GETSET_RETRY		0x00000080
#define GETSET_REG_DOMAIN	0x00000100
#define GETSET_CHANNEL		0x00000200
#define GETSET_ESSID		0x00000400
#define GETSET_MODE		0x00000800
#define SET_MSDU_LIFETIME	0x00001000
#define GET_STATION_ID		0x00002000
#define SET_RATE_FALLBACK	0x00004000
#define SET_RXCONFIG		0x00008000
#define SET_WEP_OPTIONS		0x00010000
#define GETSET_LED_POWER	0x00020000
#define GETSET_POWER_80211	0x00040000
#define GETSET_ALL		0x80000000

/*============================================================================*
 * Locking and synchronization functions                                      *
 *============================================================================*/

/* These functions *must* be inline or they will break horribly on SPARC, due
 * to its weird semantics for save/restore flags. extern inline should prevent
 * the kernel from linking or module from loading if they are not inlined. */

#ifdef BROKEN_LOCKING
extern inline int acx100_lock(wlandevice_t *wlandev, unsigned long *flags)
{
	local_irq_save(*flags);
	if (!spin_trylock(&wlandev->lock)) {
		printk("ARGH! Lock already taken in %s\n", __func__);
		local_irq_restore(*flags);
		return -EFAULT;
	} else {
		printk("Lock given out in %s\n", __func__);
	}
	if (wlandev->hw_unavailable) {
		printk(KERN_WARNING
		       "acx100_lock() called with hw_unavailable (dev=%p)\n",
		       wlandev->netdev);
		spin_unlock_irqrestore(&wlandev->lock, *flags);
		return -EBUSY;
	}
	return 0;
}

extern inline void acx100_unlock(wlandevice_t *wlandev, unsigned long *flags)
{
	/* printk(KERN_WARNING "unlock\n"); */
	spin_unlock_irqrestore(&wlandev->lock, *flags);
	/* printk(KERN_WARNING "/unlock\n"); */
}

#else /* BROKEN_LOCKING */

extern inline int acx100_lock(wlandevice_t *wlandev, unsigned long *flags)
{
	/* do nothing and be quiet */
	(void)*wlandev;
	(void)*flags;
	return 0;
}

extern inline void acx100_unlock(wlandevice_t *wlandev, unsigned long *flags)
{
	/* do nothing and be quiet */
	(void)*wlandev;
	(void)*flags;
}
#endif /* BROKEN_LOCKING */

/* FIXME: LINUX_VERSION_CODE < KERNEL_VERSION(2,4,XX) ?? (not defined in XX=10
 * defined in XX=21, someone care to do a binary search of that range to find
 * the exact version this went in? */
#ifndef ARPHRD_IEEE80211_PRISM
#define ARPHRD_IEEE80211_PRISM 802
#endif

#endif /* _ACX100_H */
