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
#ifndef __ACX_ACX100_H
#define __ACX_ACX100_H


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
#define L_BUFR		0x1000	/* debug rx buffer mgmt (ring buffer etc.) */
#define L_XFER_BEACON	0x2000	/* also log beacon packets */
#define L_BUFT		0x4000	/* debug tx buffer mgmt (ring buffer etc.) */
#define L_BUF (L_BUFR+L_BUFT)	/* debug buffer mgmt (ring buffer etc.) */

#define L_BINDEBUG	(L_BIN | L_DEBUG)
#define L_BINSTD	(L_BIN | L_STD)

#define L_ALL		(L_STD | L_INIT | L_IRQ | L_ASSOC | L_BIN | L_FUNC | \
			 L_STATE | L_XFER | L_DATA | L_DEBUG | L_IOCTL | L_CTL)

extern int debug;
extern int acx_debug_func_indent;

#define acxlog(chan, args...) \
	do { \
		if (debug & (chan)) \
			printk(KERN_WARNING args); \
	} while (0)

void log_fn_enter(const char *funcname);
void log_fn_exit(const char *funcname);
void log_fn_exit_v(const char *funcname, int v);

#define FN_ENTER \
	do { \
		if (unlikely(debug & L_FUNC)) { \
			log_fn_enter(__func__); \
		} \
	} while (0)

#define FN_EXIT(p, v) \
	do { \
		if (unlikely(debug & L_FUNC)) { \
			if (p) { \
				log_fn_exit_v(__func__, v); \
			} else { \
				log_fn_exit(__func__); \
			} \
		} \
	} while (0)

#else /* ACX_DEBUG */

#define acxlog(chan, args...)
#define FN_ENTER
#define FN_EXIT(p, v)

#endif /* ACX_DEBUG */

/* Use worker_queues for 2.5/2.6 Kernels and queue tasks for 2.4 Kernels 
   (used for the 'bottom half' of the interrupt routine) */

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
#define USE_QUEUE_TASKS
#else
/* #define NEWER_KERNELS_ONLY 1 */
#define USE_WORKER_TASKS
#endif

#define MAC_COPY(dst, src) \
	*(UINT32 *)dst = *(UINT32 *)src; \
	*(UINT16 *)(((UINT8 *)dst)+4) = *(UINT16 *)(((UINT8 *)src)+4); \

#define MAC_COPY_UNUSED1(dst, src) \
{ \
	int i; \
	for (i = 0; i < ETH_ALEN; i++) \
		*(((UINT8 *)dst)+i) = *(((UINT8 *)src)+i); \
}

#define MAC_COPY_UNUSED2(dst, src) memcpy(dst, src, ETH_ALEN);

#define MAC_FILL(dst, val) \
{ \
	int i; \
	for (i = 0; i < ETH_ALEN; i++) \
		*(((UINT8 *)dst)+i) = val; \
}

#define MAC_BCAST(dst)	MAC_FILL(dst, 0xff)

#define ACX_PACKED __WLAN_ATTRIB_PACK__

/*============================================================================*
 * Constants                                                                  *
 *============================================================================*/
/* Driver defaults */
#define DEFAULT_DTIM_INTERVAL	10
#define DEFAULT_MSDU_LIFETIME	4096	/* used to be 2048, but FreeBSD driver changed it to 4096 to work properly in noisy wlans */
#define DEFAULT_RTS_THRESHOLD	2312	/* max. size: disable RTS mechanism */
#define DEFAULT_BEACON_INTERVAL	100


#define CLEAR_BIT(val, mask) ((val) &= ~(mask))
#define SET_BIT(val, mask) ((val) |= (mask))

#define OK	0x0
#define NOT_OK	0x1

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

/* The supported chip models */
#define CHIPTYPE_ACX100		1
#define CHIPTYPE_ACX111		2

/*--- Support Constants ------------------------------------------------------*/
#define ACX_PCI		0
#define ACX_CARDBUS	1

/* Radio type names, found in Win98 driver's TIACXLN.INF */
#define RADIO_MAXIM_0D		0x0d
#define RADIO_RFMD_11		0x11
#define RADIO_RALINK_15		0x15
#define RADIO_RADIA_16		0x16	/* used in ACX111 cards (WG311v2, WL-121, ...) */
#define RADIO_UNKNOWN_17	0x17	/* most likely *sometimes* used in ACX111 cards */

/*--- IRQ Constants ----------------------------------------------------------*/
#define HOST_INT_RX_DATA	0x0001
#define HOST_INT_TX_COMPLETE	0x0002
#define HOST_INT_TX_XFER	0x0004
#define HOST_INT_RX_COMPLETE	0x0008
#define HOST_INT_DTIM		0x0010
#define HOST_INT_BEACON		0x0020
#define HOST_INT_TIMER		0x0040
#define HOST_INT_KEY_NOT_FOUND	0x0080
#define HOST_INT_IV_ICV_FAILURE	0x0100
#define HOST_INT_CMD_COMPLETE	0x0200
#define HOST_INT_INFO		0x0400
#define HOST_INT_OVERFLOW	0x0800
#define HOST_INT_PROCESS_ERROR	0x1000
#define HOST_INT_SCAN_COMPLETE	0x2000 /* official name */
#define HOST_INT_FCS_THRESHOLD	0x4000
#define HOST_INT_UNKNOWN	0x8000

/*--- Register I/O offsets ---------------------------------------------------*/

/* please add further ACX1XX hardware register definitions only when
   it turns out you need them in the driver, and please try to use
   firmware functionality instead, since using direct I/O access instead
   of letting the firmware do it might confuse the firmware's state 
   machine */

typedef enum {
	IO_ACX_SOFT_RESET = 0,

	IO_ACX_SLV_MEM_ADDR,
	IO_ACX_SLV_MEM_DATA,
	IO_ACX_SLV_MEM_CTL,
	IO_ACX_SLV_END_CTL,

	IO_ACX_FEMR,		/* Function Event Mask */

	IO_ACX_INT_TRIG,
	IO_ACX_IRQ_MASK,
	IO_ACX_IRQ_STATUS_NON_DES,
	IO_ACX_IRQ_STATUS_CLEAR, /* CLEAR = clear on read */
	IO_ACX_IRQ_ACK,
	IO_ACX_HINT_TRIG,

	IO_ACX_ENABLE,

	IO_ACX_EEPROM_CTL,
	IO_ACX_EEPROM_ADDR,
	IO_ACX_EEPROM_DATA,
	IO_ACX_EEPROM_CFG,

	IO_ACX_PHY_ADDR,
	IO_ACX_PHY_DATA,
	IO_ACX_PHY_CTL,

	IO_ACX_GPIO_OE,

	IO_ACX_GPIO_OUT,

	IO_ACX_CMD_MAILBOX_OFFS,
	IO_ACX_INFO_MAILBOX_OFFS,
	IO_ACX_EEPROM_INFORMATION,

	IO_ACX_EE_START,
	IO_ACX_SOR_CFG,
	IO_ACX_ECPU_CTRL,

	END_OF_IO_ENUM /* LEAVE THIS AT THE END, USED TO FIGURE OUT THE LENGTH */

} IO_INDICES;

/* Values for IO_ACX_INT_TRIG register */
#define INT_TRIG_RXPRC		0x08	/* inform hw that rx descriptor in queue needs processing */
#define INT_TRIG_TXPRC		0x04	/* inform hw that tx descriptor in queue needs processing */
#define INT_TRIG_INFOACK	0x02	/* ack that we received info from info mailbox */
#define INT_TRIG_CMD		0x01	/* inform hw that we have filled command mailbox */

#define IO_INDICES_SIZE END_OF_IO_ENUM * sizeof(UINT16)

/*--- EEPROM offsets ---------------------------------------------------------*/
#define ACX100_EEPROM_ID_OFFSET		0x380

/*--- Command Code Constants -------------------------------------------------*/

/*--- Controller Commands ----------------------------------------------------*/
/* can be found in table cmdTable in firmware "Rev. 1.5.0" (FW150) */
#define ACX1xx_CMD_RESET		0x00
#define ACX1xx_CMD_INTERROGATE		0x01
#define ACX1xx_CMD_CONFIGURE		0x02
#define ACX1xx_CMD_ENABLE_RX		0x03
#define ACX1xx_CMD_ENABLE_TX		0x04
#define ACX1xx_CMD_DISABLE_RX		0x05
#define ACX1xx_CMD_DISABLE_TX		0x06
#define ACX1xx_CMD_FLUSH_QUEUE		0x07
#define ACX1xx_CMD_SCAN			0x08
#define ACX1xx_CMD_STOP_SCAN		0x09
#define ACX1xx_CMD_CONFIG_TIM		0x0a
#define ACX1xx_CMD_JOIN			0x0b
#define ACX1xx_CMD_WEP_MGMT		0x0c
#if OLD_FIRMWARE_VERSIONS
#define ACX100_CMD_HALT			0x0e	/* mapped to unknownCMD in FW150 */
#else
#define ACX1xx_CMD_MEM_READ		0x0d
#define ACX1xx_CMD_MEM_WRITE		0x0e
#endif
#define ACX1xx_CMD_SLEEP		0x0f
#define ACX1xx_CMD_WAKE			0x10
#define ACX1xx_CMD_UNKNOWN_11		0x11	/* mapped to unknownCMD in FW150 */
#define ACX100_CMD_INIT_MEMORY		0x12
#define ACX1xx_CMD_CONFIG_BEACON	0x13
#define ACX1xx_CMD_CONFIG_PROBE_RESPONSE	0x14
#define ACX1xx_CMD_CONFIG_NULL_DATA	0x15
#define ACX1xx_CMD_CONFIG_PROBE_REQUEST	0x16
#define ACX1xx_CMD_TEST			0x17
#define ACX1xx_CMD_RADIOINIT		0x18
#define ACX111_CMD_RADIOCALIB		0x19

/*--- After Interrupt Commands -----------------------------------------------*/
#define ACX_AFTER_IRQ_CMD_STOP_SCAN		0x01
#define ACX_AFTER_IRQ_CMD_ASSOCIATE		0x02
#define ACX_AFTER_IRQ_CMD_RADIO_RECALIB		0x04

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

/*--- Information Elements: Network Parameters, Static Configuration Entities --*/
/* these are handled by real_cfgtable in firmware "Rev 1.5.0" (FW150) */
#define ACX1xx_IE_UNKNOWN_00			0x0000	/* mapped to cfgInvalid in FW150 */
#define ACX100_IE_ACX_TIMER			0x0001
#define ACX1xx_IE_POWER_MGMT			0x0002
#define ACX1xx_IE_QUEUE_CONFIG			0x0003
#define ACX100_IE_BLOCK_SIZE			0x0004
#define ACX1xx_IE_MEMORY_CONFIG_OPTIONS		0x0005
#define ACX1xx_IE_RATE_FALLBACK			0x0006
#define ACX100_IE_WEP_OPTIONS			0x0007
#define ACX111_IE_RADIO_BAND			0x0007
#define ACX1xx_IE_MEMORY_MAP			0x0008	/* huh? */
#define ACX100_IE_SSID				0x0008	/* huh? */
#define ACX1xx_IE_SCAN_STATUS			0x0009	/* mapped to cfgInvalid in FW150 */
#define ACX1xx_IE_ASSOC_ID			0x000a
#define ACX1xx_IE_UNKNOWN_0B			0x000b	/* mapped to cfgInvalid in FW150 */
#define ACX100_IE_UNKNOWN_0C			0x000c	/* very small implementation in FW150! */
#define ACX111_IE_CONFIG_OPTIONS		0x000c
#define ACX1xx_IE_FWREV				0x000d
#define ACX1xx_IE_FCS_ERROR_COUNT		0x000e
#define ACX1xx_IE_MEDIUM_USAGE			0x000f
#define ACX1xx_IE_RXCONFIG			0x0010
#define ACX100_IE_UNKNOWN_11			0x0011	/* NONBINARY: large implementation in FW150! link quality readings or so? */
#define ACX111_IE_QUEUE_THRESH			0x0011
#define ACX100_IE_UNKNOWN_12			0x0012	/* NONBINARY: VERY large implementation in FW150!! */
#define ACX111_IE_BSS_POWER_SAVE		0x0012
#define ACX1xx_IE_FIRMWARE_STATISTICS		0x0013
#define ACX1xx_IE_FEATURE_CONFIG		0x0015
#define ACX111_IE_KEY_CHOOSE			0x0016 /* for rekeying */
#define ACX1xx_IE_DOT11_STATION_ID		0x1001
#define ACX100_IE_DOT11_UNKNOWN_1002		0x1002	/* mapped to cfgInvalid in FW150 */
#define ACX111_IE_DOT11_FRAG_THRESH		0x1002	/* mapped to cfgInvalid in FW150 */
#define ACX100_IE_DOT11_BEACON_PERIOD		0x1003	/* mapped to cfgInvalid in FW150 */
#define ACX1xx_IE_DOT11_DTIM_PERIOD		0x1004	/* mapped to cfgInvalid in FW150 */
#define ACX1xx_IE_DOT11_SHORT_RETRY_LIMIT	0x1005
#define ACX1xx_IE_DOT11_LONG_RETRY_LIMIT	0x1006
#define ACX100_IE_DOT11_WEP_DEFAULT_KEY_WRITE	0x1007 /* configure default keys */
#define ACX1xx_IE_DOT11_MAX_XMIT_MSDU_LIFETIME	0x1008
#define ACX1xx_IE_DOT11_UNKNOWN_1009		0x1009	/* mapped to some very boring binary table in FW150 */
#define ACX1xx_IE_DOT11_CURRENT_REG_DOMAIN	0x100a
#define ACX1xx_IE_DOT11_CURRENT_ANTENNA		0x100b
#define ACX1xx_IE_DOT11_UNKNOWN_100C		0x100c	/* mapped to cfgInvalid in FW150 */
#define ACX1xx_IE_DOT11_TX_POWER_LEVEL		0x100d
#define ACX1xx_IE_DOT11_CURRENT_CCA_MODE	0x100e
#define ACX1xx_IE_DOT11_ED_THRESHOLD		0x100f
#define ACX1xx_IE_DOT11_WEP_DEFAULT_KEY_SET	0x1010	/* set default key ID */
#define ACX100_IE_DOT11_UNKNOWN_1011		0x1011	/* mapped to cfgInvalid in FW150 */
#define ACX100_IE_DOT11_UNKNOWN_1012		0x1012	/* mapped to cfgInvalid in FW150 */
#define ACX100_IE_DOT11_UNKNOWN_1013		0x1013	/* mapped to cfgInvalid in FW150 */

/*--- Configuration RID Lengths: Network Params, Static Config Entities -----*/
/* This is the length of JUST the DATA part of the RID (does not include the
 * len or code fields) */

/* TODO: fill in the rest of these */
#define ACX100_IE_ACX_TIMER_LEN				0x10
#define ACX1xx_IE_POWER_MGMT_LEN			0x06
#define ACX1xx_IE_QUEUE_CONFIG_LEN			0x1c
#define ACX100_IE_BLOCK_SIZE_LEN			0x02
#define ACX1xx_IE_MEMORY_CONFIG_OPTIONS_LEN		0x14
#define ACX1xx_IE_RATE_FALLBACK_LEN			0x01
#define ACX100_IE_WEP_OPTIONS_LEN			0x03
#define ACX1xx_IE_MEMORY_MAP_LEN			0x28
#define ACX100_IE_SSID_LEN				0x20
#define ACX1xx_IE_SCAN_STATUS_LEN			0x04
#define ACX1xx_IE_ASSOC_ID_LEN				0x02
#define ACX1xx_IE_CONFIG_OPTIONS_LEN			0x14C
#define ACX1xx_IE_FWREV_LEN				0x18
#define ACX1xx_IE_FCS_ERROR_COUNT_LEN			0x04
#define ACX1xx_IE_MEDIUM_USAGE_LEN			0x08
#define ACX1xx_IE_RXCONFIG_LEN				0x04
#define ACX1xx_IE_FIRMWARE_STATISTICS_LEN		0x9c
#define ACX1xx_IE_FEATURE_CONFIG_LEN			0x08
#define ACX111_IE_KEY_CHOOSE_LEN			0x04	/* really 4?? */
#define ACX1xx_IE_DOT11_STATION_ID_LEN			0x06
#define ACX100_IE_DOT11_BEACON_PERIOD_LEN		0x02
#define ACX1xx_IE_DOT11_DTIM_PERIOD_LEN			0x01
#define ACX1xx_IE_DOT11_SHORT_RETRY_LIMIT_LEN		0x01
#define ACX1xx_IE_DOT11_LONG_RETRY_LIMIT_LEN		0x01
#define ACX100_IE_DOT11_WEP_DEFAULT_KEY_LEN		0x20
#define ACX1xx_IE_DOT11_MAX_XMIT_MSDU_LIFETIME_LEN	0x04
#define ACX1xx_IE_DOT11_CURRENT_REG_DOMAIN_LEN		0x02
#if (WLAN_HOSTIF==WLAN_USB)
#define ACX1xx_IE_DOT11_CURRENT_ANTENNA_LEN		0x02
#else
#define ACX1xx_IE_DOT11_CURRENT_ANTENNA_LEN		0x01
#endif
#define ACX1xx_IE_DOT11_TX_POWER_LEVEL_LEN		0x01
#define ACX1xx_IE_DOT11_CURRENT_CCA_MODE_LEN		0x01
#define ACX1xx_IE_DOT11_ED_THRESHOLD_LEN		0x04
#define ACX1xx_IE_DOT11_WEP_DEFAULT_KEY_SET_LEN		0x01

/*============================================================================*
 * Types and their related constants                                          *
 *============================================================================*/

/*--- Commonly used basic types ----------------------------------------------*/
typedef struct acx100_bytestr {
	UINT16 len ACX_PACKED;
	UINT8 data[0] ACX_PACKED;
} acx100_bytestr_t;

typedef struct acx100_bytestr32 {
	UINT16 len ACX_PACKED;
	UINT8 data[32] ACX_PACKED;
} acx100_bytestr32_t;

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
	UINT16 reclen ACX_PACKED;
	UINT16 rid ACX_PACKED;
} acx100_rec_t;

typedef struct acx100_record16 {
	UINT16 reclen ACX_PACKED;
	UINT16 rid ACX_PACKED;
	UINT16 val ACX_PACKED;
} acx100_rec16_t;

typedef struct acx100_record32 {
	UINT16 reclen ACX_PACKED;
	UINT16 rid ACX_PACKED;
	UINT32 val ACX_PACKED;
} acx100_rec32;

/*-- Hardware/Firmware Component Information --*/
typedef struct acx100_compident {
	UINT16 id ACX_PACKED;
	UINT16 variant ACX_PACKED;
	UINT16 major ACX_PACKED;
	UINT16 minor ACX_PACKED;
} acx100_compident_t;

typedef struct acx100_caplevel {
	UINT16 role ACX_PACKED;
	UINT16 id ACX_PACKED;
	UINT16 variant ACX_PACKED;
	UINT16 bottom ACX_PACKED;
	UINT16 top ACX_PACKED;
} acx100_caplevel_t;

/*--- Communication Frames: Receive Frame Structure --------------------------*/
typedef struct acx100_rx_frame {
	/*-- MAC RX descriptor (acx100 byte order) --*/
	UINT16 status ACX_PACKED;
	UINT32 time ACX_PACKED;
	UINT8 silence ACX_PACKED;
	UINT8 signal ACX_PACKED;
	UINT8 rate ACX_PACKED;
	UINT8 rx_flow ACX_PACKED;
	UINT16 reserved1 ACX_PACKED;
	UINT16 reserved2 ACX_PACKED;

	/*-- 802.11 Header Information (802.11 byte order) --*/
	UINT16 frame_control ACX_PACKED;
	UINT16 duration_id ACX_PACKED;
	UINT8 address1[6] ACX_PACKED;
	UINT8 address2[6] ACX_PACKED;
	UINT8 address3[6] ACX_PACKED;
	UINT16 sequence_control ACX_PACKED;
	UINT8 address4[6] ACX_PACKED;
	UINT16 data_len ACX_PACKED;	/* acx100 (little endian) format */

	/*-- 802.3 Header Information --*/
	UINT8 dest_addr[6] ACX_PACKED;
	UINT8 src_addr[6] ACX_PACKED;
	UINT16 data_length ACX_PACKED;	/* IEEE? (big endian) format */
} acx100_rx_frame_t;


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
	UINT16 framelen ACX_PACKED;
	UINT16 infotype ACX_PACKED;
	UINT8 handover_addr[WLAN_BSSID_LEN] ACX_PACKED;
} acx100_HandoverAddr_t;

/*-- Inquiry Frame, Diagnose: Communication Tallies --*/
typedef struct acx100_CommTallies16 {
	UINT16 txunicastframes ACX_PACKED;
	UINT16 txmulticastframes ACX_PACKED;
	UINT16 txfragments ACX_PACKED;
	UINT16 txunicastoctets ACX_PACKED;
	UINT16 txmulticastoctets ACX_PACKED;
	UINT16 txdeferredtrans ACX_PACKED;
	UINT16 txsingleretryframes ACX_PACKED;
	UINT16 txmultipleretryframes ACX_PACKED;
	UINT16 txretrylimitexceeded ACX_PACKED;
	UINT16 txdiscards ACX_PACKED;
	UINT16 rxunicastframes ACX_PACKED;
	UINT16 rxmulticastframes ACX_PACKED;
	UINT16 rxfragments ACX_PACKED;
	UINT16 rxunicastoctets ACX_PACKED;
	UINT16 rxmulticastoctets ACX_PACKED;
	UINT16 rxfcserrors ACX_PACKED;
	UINT16 rxdiscardsnobuffer ACX_PACKED;
	UINT16 txdiscardswrongsa ACX_PACKED;
	UINT16 rxdiscardswepundecr ACX_PACKED;
	UINT16 rxmsginmsgfrag ACX_PACKED;
	UINT16 rxmsginbadmsgfrag ACX_PACKED;
} acx100_CommTallies16_t;

typedef struct acx100_CommTallies32 {
	UINT32 txunicastframes ACX_PACKED;
	UINT32 txmulticastframes ACX_PACKED;
	UINT32 txfragments ACX_PACKED;
	UINT32 txunicastoctets ACX_PACKED;
	UINT32 txmulticastoctets ACX_PACKED;
	UINT32 txdeferredtrans ACX_PACKED;
	UINT32 txsingleretryframes ACX_PACKED;
	UINT32 txmultipleretryframes ACX_PACKED;
	UINT32 txretrylimitexceeded ACX_PACKED;
	UINT32 txdiscards ACX_PACKED;
	UINT32 rxunicastframes ACX_PACKED;
	UINT32 rxmulticastframes ACX_PACKED;
	UINT32 rxfragments ACX_PACKED;
	UINT32 rxunicastoctets ACX_PACKED;
	UINT32 rxmulticastoctets ACX_PACKED;
	UINT32 rxfcserrors ACX_PACKED;
	UINT32 rxdiscardsnobuffer ACX_PACKED;
	UINT32 txdiscardswrongsa ACX_PACKED;
	UINT32 rxdiscardswepundecr ACX_PACKED;
	UINT32 rxmsginmsgfrag ACX_PACKED;
	UINT32 rxmsginbadmsgfrag ACX_PACKED;
} acx100_CommTallies32_t;

/*-- Inquiry Frame, Diagnose: Scan Results & Subfields --*/
typedef struct acx100_ScanResultSub {
	UINT16 chid ACX_PACKED;
	UINT16 anl ACX_PACKED;
	UINT16 sl ACX_PACKED;
	UINT8 bssid[WLAN_BSSID_LEN] ACX_PACKED;
	UINT16 bcnint ACX_PACKED;
	UINT16 capinfo ACX_PACKED;
	acx100_bytestr32_t ssid ACX_PACKED;
	UINT8 supprates[10] ACX_PACKED;	/* 802.11 info element */
	UINT16 proberesp_rate ACX_PACKED;
} acx100_ScanResultSub_t;

typedef struct acx100_ScanResult {
	UINT16 rsvd ACX_PACKED;
	UINT16 scanreason ACX_PACKED;
	 acx100_ScanResultSub_t
	    result[ACX100_SCANRESULT_MAX] ACX_PACKED;
} acx100_ScanResult_t;

/*-- Inquiry Frame, Diagnose: ChInfo Results & Subfields --*/
typedef struct acx100_ChInfoResultSub {
	UINT16 chid ACX_PACKED;
	UINT16 anl ACX_PACKED;
	UINT16 pnl ACX_PACKED;
	UINT16 active ACX_PACKED;
} acx100_ChInfoResultSub_t;

#define ACX100_CHINFORESULT_BSSACTIVE	BIT0
#define ACX100_CHINFORESULT_PCFACTIVE	BIT1

typedef struct acx100_ChInfoResult {
	UINT16 scanchannels ACX_PACKED;
	 acx100_ChInfoResultSub_t
	    result[ACX100_CHINFORESULT_MAX] ACX_PACKED;
} acx100_ChInfoResult_t;

/*-- Inquiry Frame, Diagnose: Host Scan Results & Subfields --*/
typedef struct acx100_HScanResultSub {
	UINT16 chid ACX_PACKED;
	UINT16 anl ACX_PACKED;
	UINT16 sl ACX_PACKED;
	UINT8 bssid[WLAN_BSSID_LEN] ACX_PACKED;
	UINT16 bcnint ACX_PACKED;
	UINT16 capinfo ACX_PACKED;
	acx100_bytestr32_t ssid ACX_PACKED;
	UINT8 supprates[10] ACX_PACKED;	/* 802.11 info element */
	UINT16 proberesp_rate ACX_PACKED;
	UINT16 atim ACX_PACKED;
} acx100_HScanResultSub_t;

typedef struct acx100_HScanResult {
	UINT16 nresult ACX_PACKED;
	UINT16 rsvd ACX_PACKED;
	 acx100_HScanResultSub_t
	    result[ACX100_HSCANRESULT_MAX] ACX_PACKED;
} acx100_HScanResult_t;

/*-- Unsolicited Frame, MAC Mgmt: LinkStatus --*/
#define ACX100_LINK_NOTCONNECTED	((UINT16)0)
#define ACX100_LINK_CONNECTED		((UINT16)1)
#define ACX100_LINK_DISCONNECTED	((UINT16)2)
#define ACX100_LINK_AP_CHANGE		((UINT16)3)
#define ACX100_LINK_AP_OUTOFRANGE	((UINT16)4)
#define ACX100_LINK_AP_INRANGE		((UINT16)5)
#define ACX100_LINK_ASSOCFAIL		((UINT16)6)

typedef struct acx100_LinkStatus {
	UINT16 linkstatus ACX_PACKED;
} acx100_LinkStatus_t;

/*-- Unsolicited Frame, MAC Mgmt: AssociationStatus --*/
#define ACX100_ASSOCSTATUS_STAASSOC	((UINT16)1)
#define ACX100_ASSOCSTATUS_REASSOC	((UINT16)2)
#define ACX100_ASSOCSTATUS_DISASSOC	((UINT16)3)
#define ACX100_ASSOCSTATUS_ASSOCFAIL	((UINT16)4)
#define ACX100_ASSOCSTATUS_AUTHFAIL	((UINT16)5)

typedef struct acx100_AssocStatus {
	UINT16 assocstatus ACX_PACKED;
	UINT8 sta_addr[WLAN_ADDR_LEN] ACX_PACKED;
	/* old_ap_addr is only valid if assocstatus == 2 */
	UINT8 old_ap_addr[WLAN_ADDR_LEN] ACX_PACKED;
	UINT16 reason ACX_PACKED;
	UINT16 reserved ACX_PACKED;
} acx100_AssocStatus_t;

/*-- Unsolicited Frame, MAC Mgmt: AuthRequest (AP Only) --*/
typedef struct acx100_AuthRequest {
	UINT8 sta_addr[WLAN_ADDR_LEN] ACX_PACKED;
	UINT16 algorithm ACX_PACKED;
} acx100_AuthReq_t;

/*-- Unsolicited Frame, MAC Mgmt: PSUserCount (AP Only) --*/
typedef struct acx100_PSUserCount {
	UINT16 usercnt ACX_PACKED;
} acx100_PSUserCount_t;


/*-- Collection of all Inf frames --*/
typedef union acx100_infodata {
	acx100_CommTallies16_t commtallies16 ACX_PACKED;
	acx100_CommTallies32_t commtallies32 ACX_PACKED;
	acx100_ScanResult_t scanresult ACX_PACKED;
	acx100_ChInfoResult_t chinforesult ACX_PACKED;
	acx100_HScanResult_t hscanresult ACX_PACKED;
	acx100_LinkStatus_t linkstatus ACX_PACKED;
	acx100_AssocStatus_t assocstatus ACX_PACKED;
	acx100_AuthReq_t authreq ACX_PACKED;
	acx100_PSUserCount_t psusercnt ACX_PACKED;
} acx100_infodata_t;

typedef struct acx100_InfFrame {
	UINT16 framelen ACX_PACKED;
	UINT16 infotype ACX_PACKED;
	acx100_infodata_t info ACX_PACKED;
} acx100_InfFrame_t;


/* Descriptor Control Bits */

#define ACX100_CTL_PREAMBLE   0x01	/* Preable type: 0 = long; 1 = short */
#define ACX100_CTL_FIRSTFRAG  0x02	/* This is the 1st frag of the frame */
#define ACX100_CTL_AUTODMA    0x04
#define ACX100_CTL_RECLAIM    0x08	/* ready to reuse */
#define ACX100_CTL_HOSTDONE   0x20	/* host has finished processing */
#define ACX100_CTL_ACXDONE    0x40	/* acx100 has finished processing */
#define ACX100_CTL_OWN        0x80	/* host owns the desc [has to be released last, AFTER modifying all other desc fields!] */

/* Used in beacon frames and the like */
#define DOT11RATEBYTE_1		(1*2)
#define DOT11RATEBYTE_2		(2*2)
#define DOT11RATEBYTE_5_5	(5*2+1)
#define DOT11RATEBYTE_11	(11*2)
#define DOT11RATEBYTE_22	(22*2)
#define DOT11RATEBYTE_6_G	(6*2)
#define DOT11RATEBYTE_9_G	(9*2)
#define DOT11RATEBYTE_12_G	(12*2)
#define DOT11RATEBYTE_18_G	(18*2)
#define DOT11RATEBYTE_24_G	(24*2)
#define DOT11RATEBYTE_36_G	(36*2)
#define DOT11RATEBYTE_48_G	(48*2)
#define DOT11RATEBYTE_54_G	(54*2)
#define DOT11RATEBYTE_BASIC	0x80	/* flags rates included in basic rate set */

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

/*--- Request (bulk OUT) packet contents -------------------------------------*/

typedef struct acx100_usb_txhdr {
  UINT16	desc ACX_PACKED;
  UINT16 MPDUlen ACX_PACKED;
  UINT8 index ACX_PACKED;
  UINT8 txRate ACX_PACKED;
  UINT32 hostData ACX_PACKED;
  UINT8  ctrl1 ACX_PACKED;
  UINT8  ctrl2 ACX_PACKED;
  UINT16 dataLength ACX_PACKED;
} acx100_usb_txhdr_t;


typedef struct acx100_usb_txfrm {
  acx100_usb_txhdr_t hdr ACX_PACKED;
  UINT8 data[WLAN_DATA_MAXLEN] ACX_PACKED;
} acx100_usb_txfrm_t;

typedef struct acx100_usb_scan {
	UINT16 unk1 ACX_PACKED;
	UINT16 unk2 ACX_PACKED;
} acx100_usb_scan_t;

typedef struct acx100_usb_scan_status {
	UINT16 rid ACX_PACKED;
	UINT16 length ACX_PACKED;
	UINT32	status ACX_PACKED;
} acx100_usb_scan_status_t;

typedef struct acx100_usb_cmdreq {
	UINT16 parm0 ACX_PACKED;
	UINT16 parm1 ACX_PACKED;
	UINT16 parm2 ACX_PACKED;
	UINT8 pad[54] ACX_PACKED;
} acx100_usb_cmdreq_t;

typedef struct acx100_usb_wridreq {
	UINT16 rid ACX_PACKED;
	UINT16 frmlen ACX_PACKED;
	UINT8 data[ACX100_RIDDATA_MAXLEN] ACX_PACKED;
} acx100_usb_wridreq_t;

typedef struct acx100_usb_rridreq {
	UINT16 rid ACX_PACKED;
	UINT16 frmlen ACX_PACKED;
	UINT8 pad[56] ACX_PACKED;
} acx100_usb_rridreq_t;

typedef struct acx100_usb_wmemreq {
	/*
  UINT16 offset ACX_PACKED;
	UINT16 page ACX_PACKED;
  */
	UINT8 data[ACX100_USB_RWMEM_MAXLEN] ACX_PACKED;
} acx100_usb_wmemreq_t;

typedef struct acx100_usb_rxtx_ctrl {
	UINT8 data ACX_PACKED;
} acx100_usb_rxtx_ctrl_t;

typedef struct acx100_usb_rmemreq {
	UINT8 data[ACX100_USB_RWMEM_MAXLEN] ACX_PACKED;
} acx100_usb_rmemreq_t;

/*--- Response (bulk IN) packet contents -------------------------------------*/

typedef struct acx100_usb_rxfrm {
	acx100_rx_frame_t desc ACX_PACKED;
	UINT8 data[WLAN_DATA_MAXLEN] ACX_PACKED;
} acx100_usb_rxfrm_t;

typedef struct acx100_usb_infofrm {
	UINT16 type ACX_PACKED;
	acx100_InfFrame_t info ACX_PACKED;
} acx100_usb_infofrm_t;

typedef struct acx100_usb_cmdresp {
	UINT16 resp0 ACX_PACKED;
	UINT16 resp1 ACX_PACKED;
	UINT16 resp2 ACX_PACKED;
} acx100_usb_cmdresp_t;

typedef struct acx100_usb_wridresp {
	UINT16 resp0 ACX_PACKED;
	UINT16 resp1 ACX_PACKED;
	UINT16 resp2 ACX_PACKED;
} acx100_usb_wridresp_t;

typedef struct acx100_usb_rridresp {
	UINT16 rid ACX_PACKED;
	UINT16 frmlen ACX_PACKED;
	UINT8 data[ACX100_RIDDATA_MAXLEN] ACX_PACKED;
} acx100_usb_rridresp_t;

typedef struct acx100_usb_wmemresp {
	UINT16 resp0 ACX_PACKED;
	UINT16 resp1 ACX_PACKED;
	UINT16 resp2 ACX_PACKED;
} acx100_usb_wmemresp_t;

typedef struct acx100_usb_rmemresp {
	UINT8 data[ACX100_USB_RWMEM_MAXLEN] ACX_PACKED;
} acx100_usb_rmemresp_t;

typedef struct acx100_usb_bufavail {
	UINT16 type ACX_PACKED;
	UINT16 frmlen ACX_PACKED;
} acx100_usb_bufavail_t;

typedef struct acx100_usb_error {
	UINT16 type ACX_PACKED;
	UINT16 errortype ACX_PACKED;
} acx100_usb_error_t;

/*--- Unions for packaging all the known packet types together ---------------*/

#if OLD
typedef union acx100_usbout {
        UINT16 type ACX_PACKED;
        acx100_usb_txfrm_t txfrm ACX_PACKED;
        acx100_usb_cmdreq_t cmdreq ACX_PACKED;
        acx100_usb_wridreq_t wridreq ACX_PACKED;
        acx100_usb_rridreq_t rridreq ACX_PACKED;
        acx100_usb_wmemreq_t wmemreq ACX_PACKED;
        acx100_usb_rmemreq_t rmemreq ACX_PACKED;
        acx100_usb_rxtx_ctrl_t rxtx ACX_PACKED;
        acx100_usb_scan_t scan ACX_PACKED;
} acx100_usbout_t;
#else
typedef struct acx100_bulkout {
	acx100_usb_txfrm_t txfrm ACX_PACKED;
} acx100_bulkout_t;

typedef struct acx100_usbout {
  UINT16 cmd ACX_PACKED;
  UINT16 status ACX_PACKED;
  union {
	acx100_usb_cmdreq_t cmdreq ACX_PACKED;
	acx100_usb_wridreq_t wridreq ACX_PACKED;
	acx100_usb_rridreq_t rridreq ACX_PACKED;
	acx100_usb_wmemreq_t wmemreq ACX_PACKED;
	acx100_usb_rmemreq_t rmemreq ACX_PACKED;
	acx100_usb_rxtx_ctrl_t rxtx ACX_PACKED;
	acx100_usb_scan_t scan ACX_PACKED;
  } u ACX_PACKED;
} acx100_usbout_t;
#endif

#if OLD
typedef union acx100_usbin {
	UINT16 type ACX_PACKED;
	acx100_usb_rxfrm_t rxfrm ACX_PACKED;
	acx100_usb_txfrm_t txfrm ACX_PACKED;
	acx100_usb_infofrm_t infofrm ACX_PACKED;
	acx100_usb_cmdresp_t cmdresp ACX_PACKED;
	acx100_usb_wridresp_t wridresp ACX_PACKED;
	acx100_usb_rridresp_t rridresp ACX_PACKED;
	acx100_usb_wmemresp_t wmemresp ACX_PACKED;
	acx100_usb_rmemresp_t rmemresp ACX_PACKED;
	acx100_usb_bufavail_t bufavail ACX_PACKED;
	acx100_usb_error_t usberror ACX_PACKED;
	UINT8 boguspad[3000] ACX_PACKED;
} acx100_usbin_t;
#else
typedef struct acx100_usbin {
  UINT16 cmd ACX_PACKED;
  UINT16 status ACX_PACKED;
  union {
	acx100_usb_rxfrm_t rxfrm ACX_PACKED;
	acx100_usb_txfrm_t txfrm ACX_PACKED;
	acx100_usb_infofrm_t infofrm ACX_PACKED;
	acx100_usb_cmdresp_t cmdresp ACX_PACKED;
	acx100_usb_wridresp_t wridresp ACX_PACKED;
	acx100_usb_rridresp_t rridresp ACX_PACKED;
	acx100_usb_wmemresp_t wmemresp ACX_PACKED;
	acx100_usb_rmemresp_t rmemresp ACX_PACKED;
	acx100_usb_bufavail_t bufavail ACX_PACKED;
	acx100_usb_error_t usberror ACX_PACKED;
	UINT8 boguspad[3000] ACX_PACKED;
  } u ACX_PACKED;
} acx100_usbin_t;
#endif

#endif /* WLAN_HOSTIF == WLAN_USB */

/*--- Firmware statistics ----------------------------------------------------*/
typedef struct fw_stats {
	UINT val0x0 ACX_PACKED;		/* hdr; */
	UINT tx_desc_of ACX_PACKED;
	UINT rx_oom ACX_PACKED;
	UINT rx_hdr_of ACX_PACKED;
	UINT rx_hdr_use_next ACX_PACKED;
	UINT rx_dropped_frame ACX_PACKED;
	UINT rx_frame_ptr_err ACX_PACKED;
	UINT rx_xfr_hint_trig ACX_PACKED;

	UINT rx_dma_req ACX_PACKED;	/* val0x1c */
	UINT rx_dma_err ACX_PACKED;	/* val0x20 */
	UINT tx_dma_req ACX_PACKED;
	UINT tx_dma_err ACX_PACKED;	/* val0x28 */

	UINT cmd_cplt ACX_PACKED;
	UINT fiq ACX_PACKED;
	UINT rx_hdrs ACX_PACKED;		/* val0x34 */
	UINT rx_cmplt ACX_PACKED;		/* val0x38 */
	UINT rx_mem_of ACX_PACKED;		/* val0x3c */
	UINT rx_rdys ACX_PACKED;
	UINT irqs ACX_PACKED;
	UINT acx_trans_procs ACX_PACKED;
	UINT decrypt_done ACX_PACKED;	/* val0x48 */
	UINT dma_0_done ACX_PACKED;
	UINT dma_1_done ACX_PACKED;
	UINT tx_exch_complet ACX_PACKED;
	UINT commands ACX_PACKED;
	UINT acx_rx_procs ACX_PACKED;
	UINT hw_pm_mode_changes ACX_PACKED;
	UINT host_acks ACX_PACKED;
	UINT pci_pm ACX_PACKED;
	UINT acm_wakeups ACX_PACKED;

	UINT wep_key_count ACX_PACKED;
	UINT wep_default_key_count ACX_PACKED;
	UINT dot11_def_key_mib ACX_PACKED;
	UINT wep_key_not_found ACX_PACKED;
	UINT wep_decrypt_fail ACX_PACKED;
} fw_stats_t;

/* Firmware version struct */

typedef struct fw_ver {
	UINT16 vala ACX_PACKED;
	UINT16 valb ACX_PACKED;
	char fw_id[20] ACX_PACKED;
	UINT32 hw_id ACX_PACKED;
} fw_ver_t;

/*--- IEEE 802.11 header -----------------------------------------------------*/
/* FIXME: acx_addr3_t should probably actually be discarded in favour of the
 * identical linux-wlan-ng p80211_hdr_t. An even better choice would be to use
 * the kernel's struct ieee80_11_hdr from driver/net/wireless/ieee802_11.h */
typedef struct acx_addr3 {
	/* IEEE 802.11-1999.pdf chapter 7 might help */
	UINT16 frame_control ACX_PACKED;	/* 0x00, wlan-ng name */
	UINT16 duration_id ACX_PACKED;	/* 0x02, wlan-ng name */
	char address1[0x6] ACX_PACKED;	/* 0x04, wlan-ng name */
	char address2[0x6] ACX_PACKED;	/* 0x0a */
	char address3[0x6] ACX_PACKED;	/* 0x10 */
	UINT16 sequence_control ACX_PACKED;	/* 0x16 */
	UINT8 *val0x18 ACX_PACKED;
	struct sk_buff *val0x1c ACX_PACKED;
	struct sk_buff *val0x20 ACX_PACKED;
} acx_addr3_t;

/*--- WEP stuff --------------------------------------------------------------*/
#define DOT11_MAX_DEFAULT_WEP_KEYS	4
#define MAX_KEYLEN			32

#define HOSTWEP_DEFAULTKEY_MASK		(BIT1 | BIT0)
#define HOSTWEP_DECRYPT			BIT4
#define HOSTWEP_ENCRYPT			BIT5
#define HOSTWEP_PRIVACYINVOKED		BIT6
#define HOSTWEP_EXCLUDEUNENCRYPTED	BIT7

/* non-firmware struct --> no packing necessary */
typedef struct wep_key {
	UINT8 index;
	size_t size;
	UINT8 key[29];
	UINT16 strange_filler;
} wep_key_t;			/* size = 264 bytes (33*8) */
/* FIXME: We don't have size 264! Or is there 2 bytes beyond the key
 * (strange_filler)? */

/* non-firmware struct --> no packing necessary */
typedef struct key_struct {
	UINT8 addr[ETH_ALEN];	/* 0x00 */
	UINT16 filler1;		/* 0x06 */
	UINT32 filler2;		/* 0x08 */
	UINT32 index;		/* 0x0c */
	UINT16 len;		/* 0x10 */
	UINT8 key[29];		/* 0x12; is this long enough??? */
} key_struct_t;			/* size = 276. FIXME: where is the remaining space?? */


/* non-firmware struct --> no packing necessary */
typedef struct client {
	UINT16 aid;		/* association ID */
	char address[ETH_ALEN];	/* 0x2 */
	UINT8 val0x8;
	UINT8 used;		/* 0x9 */
	UINT16 val0xa;
	UINT16 auth_alg;
	UINT16 val0xe;
	UINT8 *val0x10;		/* points to some data, don't know what yet */
	UINT32 unkn0x14;
	UINT8 val0x18[0x80];	/* 0x18, used by acx_process_authen() */
	UINT16 val0x98;
	UINT16 val0x9a;
	UINT8 pad5[8];		/* 0x9c */
	struct client *next;	/* 0xa4 */
} client_t;

/*--- Tx and Rx descriptor ring buffer administration ------------------------*/
typedef struct TIWLAN_DC {	/* V3 version */
	struct	wlandevice 	*priv;
	/* This is the pointer to the beginning of the cards tx queue pool.
	   The Adress is relative to the internal memory mapping of the card! */
	UINT32		ui32ACXTxQueueStart;	/* 0x8, official name */
	/* This is the pointer to the beginning of the cards rx queue pool.
	   The Adress is relative to the internal memory mapping of the card! */
	UINT32		ui32ACXRxQueueStart;	/* 0xc */
	UINT8		*pTxBufferPool;		/* 0x10 */
	UINT32		TxBufferPoolSize;	/* 0x18 */
	dma_addr_t	TxBufferPoolPhyAddr;	/* 0x14 */
	UINT16		TxDescrSize;		/* size per tx descr; ACX111 = ACX100 + 4 */
	struct	txdescriptor	*pTxDescQPool;	/* V13POS 0x1c, official name */
	spinlock_t	tx_lock;
	UINT32		tx_pool_count;		/* 0x20 indicates # of ring buffer pool entries */
	UINT32		tx_head;		/* 0x24 current ring buffer pool member index */
	UINT32		tx_tail;		/* 0x34,pool_idx2 is not correct, I'm
						 * just using it as a ring watch
						 * official name */
	struct	framehdr	*pFrameHdrQPool;/* 0x28 */
	UINT32		FrameHdrQPoolSize;	/* 0x2c */
	dma_addr_t	FrameHdrQPoolPhyAddr;	/* 0x30 */
	/* UINT32 		val0x38; */	/* 0x38, NOT USED */

	/* This is the pointer to the beginning of the hosts tx queue pool.
	   The address is relative to the cards internal memory mapping */
	struct txhostdescriptor *pTxHostDescQPool;	/* V3POS 0x3c, V1POS 0x60 */
	UINT		TxHostDescQPoolSize;	/* 0x40 */
	/* This is the pointer to the beginning of the hosts tx queue pool.
	   The address is relative to the host memory mapping */
	dma_addr_t	TxHostDescQPoolPhyAddr;	/* 0x44 */
	/* UINT32		val0x48; */	/* 0x48, NOT USED */
	/* UINT32		val0x4c; */	/* 0x4c, NOT USED */

	/* This is the pointer to the beginning of the cards rx queue pool.
	   The Adress is relative to the host memory mapping!! */
	struct	rxdescriptor	*pRxDescQPool;	/* V1POS 0x74, V3POS 0x50 */
	spinlock_t	rx_lock;
	UINT32		rx_pool_count;		/* V1POS 0x78, V3POS 0X54 */
	UINT32		rx_tail;		/* 0x6c */
	/* UINT32		val0x50; */	/* V1POS:0x50, some size NOT USED */
	/* UINT32		val0x54; */	/* 0x54, official name NOT USED */

	/* This is the pointer to the beginning of the hosts rx queue pool.
	   The address is relative to the card internal memory mapping */
	struct rxhostdescriptor *pRxHostDescQPool;	/* 0x58, is it really rxdescriptor? */
	UINT32		RxHostDescQPoolSize;	/* 0x5c */
	/* This is the pointer to the beginning of the hosts rx queue pool.
	   The address is relative to the host memory mapping */
	dma_addr_t	RxHostDescQPoolPhyAddr;	/* 0x60, official name. */
	/* UINT32		val0x64; */	/* 0x64, some size */
	UINT8		*pRxBufferPool;		/* this is supposed to be [rxbuffer *], but it's not defined here, so let's define it as [UINT8 *] */
	UINT32		RxBufferPoolSize;
	dma_addr_t	RxBufferPoolPhyAddr;	/* *rxdescq2; 0x74 */
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
/* non-firmware struct --> no packing necessary */
typedef struct bss_info {
	UINT8 bssid[ETH_ALEN];	/* BSSID (network ID of the device) */
	UINT8 mac_addr[ETH_ALEN];	/* MAC address of the station's device */
	UINT8 essid[IW_ESSID_MAX_SIZE + 1];	/* ESSID and trailing '\0'  */
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

#define ACX_STATE_FW_LOADED	0x01
#define ACX_STATE_IFACE_UP	0x02

/* MAC mode (BSS type) defines for ACX100.
 * Note that they shouldn't be redefined, since they are also used
 * during communication with firmware */
#define ACX_MODE_0_IBSS_ADHOC	0
#define ACX_MODE_1_UNUSED	1
#define ACX_MODE_2_MANAGED_STA	2
#define ACX_MODE_3_MANAGED_AP	3
#define ACX_MODE_FF_AUTO	0xff	/* pseudo mode - not ACX100 related! (accept both Ad-Hoc and Managed stations for association) */

/* non-firmware struct --> no packing necessary */
struct txrate_ctrl {
	UINT16  cfg;		      /* Tx rate from iwconfig */
	UINT16  cur;		      /* the Tx rate we currently use */
	UINT8   pbcc511;		  /* Use PBCC at 5 and 11Mbit? (else CCK) */
	UINT8   do_auto;		      /* Auto adjust Tx rates? */
	UINT8   fallback_threshold;     /* 0-100 */
	UINT8   fallback_count;
	UINT8   stepup_threshold;	/* 0-100 */
	UINT8   stepup_count;
	//TODO: unsigned long txcnt[];
};

/* non-firmware struct --> no packing necessary */
struct peer {
	struct txrate_ctrl txbase;      /* For basic rates */
	struct txrate_ctrl txrate;      /* For operational rates */
	UINT8   shortpre;		 /* 0 == Long Preamble, 1 == Short */
};

/* FIXME: this should be named something like struct acx_priv (typedef'd to
 * acx_priv_t) */

/* non-firmware struct --> no packing necessary */
typedef struct wlandevice {
	/*** Device chain ***/
	struct wlandevice	*next;		/* link for list of devices */

	/*** Linux network device ***/
	struct net_device *netdev;		/* pointer to linux netdevice */
	struct net_device *prev_nd;		/* FIXME: We should not chain via our
						 * private struct wlandevice _and_
						 * the struct net_device. */

	/*** Device statistics ***/
	struct net_device_stats stats;		/* net device statistics */
#ifdef WIRELESS_EXT
	struct iw_statistics wstats;		/* wireless statistics */
#endif
/*	struct p80211_frmrx_t rx; */		/* 802.11 frame rx statistics */

	/*** Power managment ***/
	struct pm_dev *pm;			/* PM crap */

	/*** USB stuff ***/
#if (WLAN_HOSTIF==WLAN_USB)
	struct	usb_device	*usbdev;
	acx100_usbin_t		usbin;
	acx100_usbin_t		bulkin;
	acx100_usbout_t		usbout;
	acx100_bulkout_t	bulkout;
	int			usb_txoffset;
	int			usb_txsize;
	int			usb_max_bulkout;
	int			usb_tx_mutex;
	struct txdescriptor *	currentdesc;
	spinlock_t		usb_ctrl_lock;
	spinlock_t		usb_tx_lock;
	struct	urb		*ctrl_urb;
	struct	urb		*bulkrx_urb;
	struct	urb		*bulktx_urb;
	unsigned char	usb_setup[8];
#endif

	/*** Management timer ***/
	struct	timer_list	mgmt_timer;

	/*** Locking ***/
	spinlock_t	lock;			/* mutex for concurrent accesses to structure */

	/*** Hardware identification ***/
	UINT8		form_factor;
	UINT8		radio_type;
	UINT8		eeprom_version;

	/*** Firmware identification ***/
	char		firmware_version[20];
	UINT32		firmware_numver;
	UINT32		firmware_id;

	/*** Hardware resources ***/
	UINT16		irq_mask;		/* interrupts types to mask out (not wanted) with many IRQs activated */
	UINT16		irq_mask_off;		/* interrupts types to mask out (not wanted) with IRQs off */

	unsigned long	membase;		/* 10 */
	unsigned long	membase2;		/* 14 */
	void 		*iobase;		/* 18 */
	void		*iobase2;		/* 1c */
	UINT		chip_type;
	const char	*chip_name;
	UINT8		bus_type;
	const UINT16		*io;		/* points to ACX100 or ACX111 I/O register address set */

	/*** Device state ***/
	u32		pci_state[16];		/* saved PCI state for suspend/resume */
	int		hw_unavailable;		/* indicates whether the hardware has been
						 * suspended or ejected. actually a counter. */
	UINT16		dev_state_mask;
	int		monitor;		/* whether the device is in monitor mode */
	int		monitor_setting;
	UINT8		led_power;		/* power LED status */
	UINT32		get_mask;		/* mask of settings to fetch from the card */
	UINT32		set_mask;		/* mask of settings to write to the card */

	UINT8		irqs_active;	/* whether irq sending is activated */
	UINT16		irq_status;
	UINT8		after_interrupt_jobs; /* mini job list for doing actions after an interrupt occurred */
#ifdef USE_QUEUE_TASKS
	struct tq_struct after_interrupt_task; /* our task for after interrupt actions */
#else
	struct work_struct after_interrupt_task; /* our task for after interrupt actions */
#endif

	/*** scanning ***/
	UINT16		scan_count;		/* number of times to do channel scan */
	UINT8		scan_mode;		/* 0 == active, 1 == passive, 2 == background */
	UINT16		scan_duration;
	UINT16		scan_probe_delay;
#if WIRELESS_EXT > 15
	struct iw_spy_data	spy_data;	/* FIXME: needs to be implemented! */
#endif
			
	/*** Wireless network settings ***/
	UINT8		dev_addr[MAX_ADDR_LEN]; /* copy of the device address (ifconfig hw ether) that we actually use for 802.11; copied over from the network device's MAC address (ifconfig) when it makes sense only */
	UINT8		address[ETH_ALEN];	/* the BSSID before joining */
	UINT8		bssid[ETH_ALEN];	/* the BSSID after having joined */
	UINT8		ap[ETH_ALEN];		/* The AP we want, FF:FF:FF:FF:FF:FF is any */
	UINT16		aid;			/* The Association ID send from the AP */
	UINT16		macmode_wanted;		/* That's the MAC mode we want (iwconfig) */
	UINT16		macmode_chosen;		/* That's the MAC mode we chose after browsing the station list */
	UINT16		macmode_joined;		/* This is the MAC mode we're currently in */
	UINT8		essid_active;		/* specific ESSID active, or select any? */
	UINT8		essid_len;		/* to avoid dozens of strlen() */
	char		essid[IW_ESSID_MAX_SIZE+1];	/* V3POS 84; essid; INCLUDES \0 termination for easy printf - but many places simply want the string data memcpy'd plus a length indicator! Keep that in mind... */
	char		essid_for_assoc[IW_ESSID_MAX_SIZE+1];	/* the ESSID we are going to use for association, in case of "essid 'any'" and in case of hidden ESSID (use configured ESSID then) */
	char		nick[IW_ESSID_MAX_SIZE+1]; /* see essid! */
	UINT16		channel;		/* V3POS 22f0, V1POS b8 */
	UINT8		reg_dom_id;		/* reg domain setting */
	UINT16		reg_dom_chanmask;
	UINT16		status;			/* 802.11 association status */
	UINT16		auth_or_assoc_retries;	/* V3POS 2827, V1POS 27ff */

	UINT16		bss_table_count;	/* # of active BSS scan table entries */
	struct		bss_info bss_table[32];	/* BSS scan table */
	struct		bss_info station_assoc;	/* the station we're currently associated to */
	UINT8		scan_running;
	unsigned long	scan_start;		/* FIXME type?? */
	UINT16		scan_retries;		/* V3POS 2826, V1POS 27fe */

	client_t	sta_list[32];		/* should those two be of */
	client_t	*sta_hash_tab[64];	/* equal size? */

	/* 802.11 power save mode */
	UINT8		ps_wakeup_cfg;
	UINT8		ps_listen_interval;
	UINT8		ps_options;
	UINT8		ps_hangover_period;
	UINT16		ps_enhanced_transition_time;

	/*** PHY settings ***/
	struct peer	defpeer;
	struct peer	ap_peer;

	UINT8		tx_disabled;
	UINT8		tx_level_dbm;
	UINT8		tx_level_val;
	UINT8		tx_level_auto;		/* whether to do automatic power adjustment */
	unsigned long	time_last_recalib;

	unsigned long	brange_time_last_state_change;	/* time the power LED was last changed */
	UINT8		brange_last_state;					/* last state of the LED */
	UINT8		brange_max_quality;					/* maximum quality that equates to full speed */

	UINT8		preamble_mode;		/* 0 == Long Preamble, 1 == Short, 2 == Auto */

	UINT8		sensitivity;
	UINT8		antenna;		/* antenna settings */
	UINT8		ed_threshold;		/* energy detect threshold */
	UINT8		cca;			/* clear channel assessment */

	UINT16		rts_threshold;
	UINT32		short_retry;		/* V3POS 204, V1POS 20c */
	UINT32		long_retry;		/* V3POS 208, V1POS 210 */
	UINT16		msdu_lifetime;		/* V3POS 20c, V1POS 214 */
	UINT16		listen_interval;	/* V3POS 250, V1POS 258, given in units of beacon interval */
	UINT32		beacon_interval;	/* V3POS 2300, V1POS c8 */

	UINT16		capabilities;		/* V3POS b0 */
	UINT8		capab_short;		/* V3POS 1ec, V1POS 1f4 */
	UINT8		capab_pbcc;		/* V3POS 1f0, V1POS 1f8 */
	UINT8		capab_agility;		/* V3POS 1f4, V1POS 1fc */
	UINT8		rate_supported_len;	/* V3POS 1243, V1POS 124b */
	UINT8		rate_supported[13];	/* V3POS 1244, V1POS 124c */

	/*** Encryption settings (WEP) ***/
	UINT32		auth_alg;		/* V3POS 228, V3POS 230, used in transmit_authen1 */
	UINT8		wep_enabled;
	UINT8		wep_restricted;		/* V3POS c0 */
	UINT8		wep_current_index;	/* V3POS 254, V1POS 25c not sure about this */
	wep_key_t	wep_keys[DOT11_MAX_DEFAULT_WEP_KEYS];	/* the default WEP keys */
	key_struct_t	wep_key_struct[10];	/* V3POS 688 */

	/*** Card Rx/Tx management ***/
	UINT32		promiscuous;
	UINT32		mc_count;		/* multicast count */
	UINT16		rx_config_1;		/* V3POS 2820, V1POS 27f8 */
	UINT16		rx_config_2;		/* V3POS 2822, V1POS 27fa */
	TIWLAN_DC	dc;			/* V3POS 2380, V1POS 2338 */
	UINT32		TxQueueCnt;		/* V3POS 24dc, V1POS 24b4 */
	UINT32		RxQueueCnt;		/* V3POS 24f4, V1POS 24cc */
	UINT32		TxQueueFree;
	struct	rxhostdescriptor *RxHostDescPoolStart;	/* V3POS 24f8, V1POS 24d0 */
	UINT32		tx_cnt_done;
	UINT16		memblocksize;		/* V3POS 2354, V1POS 230c */
	UINT32		RxBlockNum;		/* V3POS 24e4, V1POS 24bc */
	UINT32		TotalRxBlockSize;	/* V3POS 24e8, V1POS 24c0 */
	UINT32		TxBlockNum;		/* V3POS 24fc, V1POS 24d4 */
	UINT32		TotalTxBlockSize;	/* V3POS 2500, V1POS 24d8 */

	/*** ACX100 command interface ***/
	UINT16		cmd_type;		/* V3POS 2508, V1POS 24e0 */
	UINT16		cmd_status;		/* V3POS 250a, V1POS 24e2 */
	void		*CommandParameters;	/* FIXME: used to be an array UINT*0x88,
						 * but it should most likely be *one*
						 * UINT32 instead, pointing to the cmd
						 * param memory. V3POS 268c, V1POS 2664 */

	UINT16		info_type;		/* V3POS 2508, V1POS 24e0 */
	UINT16		info_status;		/* V3POS 250a, V1POS 24e2 */
	void		*InfoParameters;		/* V3POS 2814, V1POS 27ec */

	/*** Unknown ***/
	UINT8		dtim_interval;		/* V3POS 2302 */
#if UNUSED
	UINT8		val0x2324_0;		/* V3POS 2324 */
	UINT8		val0x2324_2;
	UINT8		val0x2324_4;
	UINT8		val0x2324_5;
	UINT8		val0x2324_6;
	UINT8		val0x2324_7;
#endif
} wlandevice_t;

/*-- MAC modes --*/
#define WLAN_MACMODE_NONE	0
#define WLAN_MACMODE_IBSS_STA	1
#define WLAN_MACMODE_ESS_STA	2
#define WLAN_MACMODE_ESS_AP	3

/*-- rx_config_1 bitfield --*/
/*  bit     description
 *    13   include additional header (length etc.) *required*
 *    10   receive frames from own SSID only
 *     9   discard broadcast (01:xx:xx:xx:xx:xx in mac)
 *     8   receive packets for multicast address 1
 *     7   receive packets for multicast address 0
 *     6   discard all multicast packets
 *     5   discard frames from foreign BSSID
 *     4   discard frames with foreign destination MAC address
 *     3   promiscuous mode (receive ALL frames, disable filter)
 *     2   include FCS
 *     1   include additional header (802.11 phy?)
 *     0   ???
 */
#define RX_CFG1_PLUS_ADDIT_HDR		0x2000
#define RX_CFG1_FILTER_SSID		0x0400
#define RX_CFG1_FILTER_BCAST		0x0200
#define RX_CFG1_RCV_MC_ADDR1		0x0100
#define RX_CFG1_RCV_MC_ADDR0		0x0080
#define RX_CFG1_FILTER_ALL_MULTI	0x0040
#define RX_CFG1_FILTER_BSSID		0x0020
#define RX_CFG1_FILTER_MAC		0x0010
#define RX_CFG1_RCV_PROMISCUOUS		0x0008
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
#define GETSET_LED_POWER	0x00000001L
#define GETSET_STATION_ID	0x00000002L
#define SET_TEMPLATES		0x00000004L
#define SET_STA_LIST		0x00000008L
#define GETSET_TX		0x00000010L
#define GETSET_RX		0x00000020L
#define SET_RXCONFIG		0x00000040L
#define GETSET_ANTENNA		0x00000080L
#define GETSET_SENSITIVITY	0x00000100L
#define GETSET_TXPOWER		0x00000200L
#define GETSET_ED_THRESH	0x00000400L
#define GETSET_CCA		0x00000800L
#define GETSET_POWER_80211	0x00001000L
#define GETSET_RETRY		0x00002000L
#define GETSET_REG_DOMAIN	0x00004000L
#define GETSET_CHANNEL		0x00008000L
#define GETSET_ESSID		0x00010000L
#define GETSET_MODE		0x00020000L
#define GETSET_WEP		0x00040000L
#define SET_WEP_OPTIONS		0x00080000L
#define SET_MSDU_LIFETIME	0x00100000L
#define SET_RATE_FALLBACK	0x00200000L
#define GETSET_ALL		0x80000000L

void acx_schedule_after_interrupt_task(wlandevice_t *priv);
void acx_rx(struct rxhostdescriptor *rxdesc, wlandevice_t *priv);

/*============================================================================*
 * Locking and synchronization functions                                      *
 *============================================================================*/

/* These functions *must* be inline or they will break horribly on SPARC, due
 * to its weird semantics for save/restore flags. extern inline should prevent
 * the kernel from linking or module from loading if they are not inlined. */

#ifdef BROKEN_LOCKING
extern inline int acx_lock(wlandevice_t *priv, unsigned long *flags)
{
	local_irq_save(*flags);
	if (!spin_trylock(&priv->lock)) {
		printk("ARGH! Lock already taken in %s\n", __func__);
		local_irq_restore(*flags);
		return -EFAULT;
	} else {
		printk("Lock given out in %s\n", __func__);
	}
	if (priv->hw_unavailable) {
		printk(KERN_WARNING
		       "acx_lock() called with hw_unavailable (dev=%p)\n",
		       priv->netdev);
		spin_unlock_irqrestore(&priv->lock, *flags);
		return -EBUSY;
	}
	return OK;
}

extern inline void acx_unlock(wlandevice_t *priv, unsigned long *flags)
{
	/* printk(KERN_WARNING "unlock\n"); */
	spin_unlock_irqrestore(&priv->lock, *flags);
	/* printk(KERN_WARNING "/unlock\n"); */
}

#else /* BROKEN_LOCKING */

extern inline int acx_lock(wlandevice_t *priv, unsigned long *flags)
{
	/* do nothing and be quiet */
	/*@-noeffect@*/
	(void)*priv;
	(void)*flags;
	/*@=noeffect@*/
	return OK;
}

extern inline void acx_unlock(wlandevice_t *priv, unsigned long *flags)
{
	/* do nothing and be quiet */
	/*@-noeffect@*/
	(void)*priv;
	(void)*flags;
	/*@=noeffect@*/
}
#endif /* BROKEN_LOCKING */

/* FIXME: LINUX_VERSION_CODE < KERNEL_VERSION(2,4,XX) ?? (not defined in XX=10
 * defined in XX=21, someone care to do a binary search of that range to find
 * the exact version this went in? */
#ifndef ARPHRD_IEEE80211_PRISM
#define ARPHRD_IEEE80211_PRISM 802
#endif

#endif /* __ACX_ACX100_H */
