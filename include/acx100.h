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

/* whether to do Tx descriptor cleanup in softirq (i.e. not in IRQ
 * handler) or not. Note that doing it later does slightly increase
 * system load, so still do that stuff in the IRQ handler for now,
 * even if that probably means worse latency */
#define TX_CLEANUP_IN_SOFTIRQ 0

/*============================================================================*
 * Debug / log functionality                                                  *
 *============================================================================*/

#if ACX_DEBUG

#define L_STD		0x01	/* standard logging that really should be there,
				   like error messages etc. */
#define L_INIT		0x02	/* special card initialisation logging */
#define L_IRQ		0x04	/* interrupt stuff */
#define L_ASSOC		0x08	/* assocation (network join) and station log */
#define L_BIN		0x10	/* original messages from binary drivers */
#define L_FUNC		0x20	/* logging of function enter / leave */
#define L_UNUSED1	0x40	/* unused */
#define L_XFER		0x80	/* logging of transfers and mgmt */
#define L_DATA		0x100	/* logging of transfer data */
#define L_DEBUG		0x200	/* log of debug info */
#define L_IOCTL		0x400	/* log ioctl calls */
#define L_CTL		0x800	/* log of low-level ctl commands */
#define L_BUFR		0x1000	/* debug rx buffer mgmt (ring buffer etc.) */
#define L_XFER_BEACON	0x2000	/* also log beacon packets */
#define L_BUFT		0x4000	/* debug tx buffer mgmt (ring buffer etc.) */
#define L_BUF (L_BUFR+L_BUFT)	/* debug buffer mgmt (ring buffer etc.) */
#define L_USBRXTX	0x8000  /* debug USB rx/tx operations */

#define L_BINDEBUG	(L_BIN | L_DEBUG)
#define L_BINSTD	(L_BIN | L_STD)

#define L_ALL		(L_STD | L_INIT | L_IRQ | L_ASSOC | L_BIN | L_FUNC | \
			 L_STATE | L_XFER | L_DATA | L_DEBUG | L_IOCTL | L_CTL)

extern unsigned int debug;

#else

/* We want if(debug & something) to be always false */
enum {
	L_STD		= 0,
	L_INIT		= 0,
	L_IRQ		= 0,
	L_ASSOC		= 0,
	L_BIN		= 0,
	L_FUNC		= 0,
	L_UNUSED1	= 0,
	L_XFER		= 0,
	L_DATA		= 0,
	L_DEBUG		= 0,
	L_IOCTL		= 0,
	L_CTL		= 0,
	L_BUFR		= 0,
	L_XFER_BEACON	= 0,
	L_BUFT		= 0,
	L_BUF 		= 0,
	L_USBRXTX	= 0,
	L_BINDEBUG	= 0,
	L_BINSTD	= 0,
	L_ALL		= 0,
	debug = 0
};

#endif /* ACX_DEBUG */

/* Use worker_queues for 2.5/2.6 Kernels and queue tasks for 2.4 Kernels 
   (used for the 'bottom half' of the interrupt routine) */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)

/* #define NEWER_KERNELS_ONLY 1 */
#define USE_WORKER_TASKS
#define WORK_STRUCT struct work_struct
#define SCHEDULE_WORK schedule_work
#define FLUSH_SCHEDULED_WORK flush_scheduled_work

#else

#define USE_QUEUE_TASKS
#define WORK_STRUCT struct tq_struct
#define SCHEDULE_WORK schedule_task
#define INIT_WORK(work, func, ndev) \
      do { \
              (work)->routine = (func); \
              (work)->data = (ndev); \
      } while(0)
#define FLUSH_SCHEDULED_WORK flush_scheduled_tasks

#endif



/*============================================================================*
 * Random helpers                                                             *
 *============================================================================*/
static inline void MAC_COPY(u8 *mac, const u8 *src)
{
	*(u32*)mac = *(u32*)src;
	((u16*)mac)[2] = ((u16*)src)[2];
	/* kernel's memcpy will do the same: memcpy(dst, src, ETH_ALEN); */
}

static inline void MAC_FILL(u8 *mac, u8 val)
{
	memset(mac, val, ETH_ALEN);
}

static inline void MAC_BCAST(u8 *mac)
{
        ((u16*)mac)[2] = *(u32*)mac = -1;
}

static inline void MAC_ZERO(u8 *mac)
{
        ((u16*)mac)[2] = *(u32*)mac = 0;
}

static inline int mac_is_equal(const u8 *a, const u8 *b)
{
	/* can't beat this */
	return memcmp(a, b, ETH_ALEN) == 0;
}

static inline int mac_is_bcast(const u8 *mac)
{
	/* AND together 4 first bytes with sign-entended 2 last bytes
	** Only bcast address gives 0xffffffff. +1 gives 0 */
	return ( *(s32*)mac & ((s16*)mac)[2] ) + 1 == 0;
}

static inline int mac_is_zero(const u8 *mac)
{
	return ( *(u32*)mac | ((u16*)mac)[2] ) == 0;
}

static inline int mac_is_directed(const u8 *mac)
{
	return (mac[0] & 1)==0;
}

static inline int mac_is_mcast(const u8 *mac)
{
	return (mac[0] & 1) && !mac_is_bcast(mac);
}

#define MACSTR "%02x:%02x:%02x:%02x:%02x:%02x"
#define MAC(bytevector) \
	((unsigned char *)bytevector)[0], \
	((unsigned char *)bytevector)[1], \
	((unsigned char *)bytevector)[2], \
	((unsigned char *)bytevector)[3], \
	((unsigned char *)bytevector)[4], \
	((unsigned char *)bytevector)[5]
				

#define ACX_PACKED __WLAN_ATTRIB_PACK__

#define VEC_SIZE(a) (sizeof(a)/sizeof(a[0]))

#define CLEAR_BIT(val, mask) ((val) &= ~(mask))
#define SET_BIT(val, mask) ((val) |= (mask))

/*============================================================================*
 * Constants                                                                  *
 *============================================================================*/
/* Driver defaults */
#define DEFAULT_DTIM_INTERVAL	10
#define DEFAULT_MSDU_LIFETIME	4096	/* used to be 2048, but FreeBSD driver changed it to 4096 to work properly in noisy wlans */
#define DEFAULT_RTS_THRESHOLD	2312	/* max. size: disable RTS mechanism */
#define DEFAULT_BEACON_INTERVAL	100


#define OK	0x0
#define NOT_OK	0x1

#define ACX100_BAP_DATALEN_MAX		((u16)4096)
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
#define RADIO_UNKNOWN_19	0x19	/* found FwRad19.bin in a Safecom driver; must be a ACX111 radio, does anyone know more?? */

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

#define IO_INDICES_SIZE END_OF_IO_ENUM * sizeof(u16)

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
#define ACX_AFTER_IRQ_CMD_UPDATE_CARD_CFG	0x08
#define ACX_AFTER_IRQ_CMD_TX_CLEANUP	0x10

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
#define ACX1xx_IE_DOT11_GROUP_ADDR		0x1009
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
#define ACX1xx_IE_CONFIG_OPTIONS_LEN			0x14c
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

/*--- Communication Frames: Receive Frame Structure --------------------------*/
typedef struct acx100_rx_frame {
	/*-- MAC RX descriptor (acx100 byte order) --*/
	u16 status ACX_PACKED;
	u32 time ACX_PACKED;
	u8 silence ACX_PACKED;
	u8 signal ACX_PACKED;
	u8 rate ACX_PACKED;
	u8 rx_flow ACX_PACKED;
	u16 reserved1 ACX_PACKED;
	u16 reserved2 ACX_PACKED;

	/*-- 802.11 Header Information (802.11 byte order) --*/
	u16 frame_control ACX_PACKED;
	u16 duration_id ACX_PACKED;
	u8 address1[6] ACX_PACKED;
	u8 address2[6] ACX_PACKED;
	u8 address3[6] ACX_PACKED;
	u16 sequence_control ACX_PACKED;
	u8 address4[6] ACX_PACKED;
	u16 data_len ACX_PACKED;	/* acx100 (little endian) format */

	/*-- 802.3 Header Information --*/
	u8 dest_addr[6] ACX_PACKED;
	u8 src_addr[6] ACX_PACKED;
	u16 data_length ACX_PACKED;	/* IEEE? (big endian) format */
} acx100_rx_frame_t;


/*============================================================================*
 * Information Frames Structures                                              *
 *============================================================================*/


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

/* The number of bulk URBs to use */

#define ACX100_USB_NUM_BULK_URBS 8

/* Should be sent to the bulkout endpoint */
#define ACX100_USB_TXFRM	0
#define ACX100_USB_CMDREQ	1
#define ACX100_USB_WRIDREQ	2
#define ACX100_USB_RRIDREQ	3
#define ACX100_USB_WMEMREQ	4
#define ACX100_USB_RMEMREQ	5
#define ACX100_USB_UPLOAD_FW	0x10
#define ACX100_USB_ACK_CS	0x11
#define ACX100_USB_UNKNOWN_REQ1	0x12
#define ACX100_USB_TX_DESC	0xA

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
  u16	desc ACX_PACKED;
  u16 MPDUlen ACX_PACKED;
  u8 index ACX_PACKED;
  u8 txRate ACX_PACKED;
  u32 hostData ACX_PACKED;
  u8  ctrl1 ACX_PACKED;
  u8  ctrl2 ACX_PACKED;
  u16 dataLength ACX_PACKED;
} acx100_usb_txhdr_t;


typedef struct acx100_usb_txfrm {
  acx100_usb_txhdr_t hdr ACX_PACKED;
  u8 data[WLAN_DATA_MAXLEN] ACX_PACKED;
} acx100_usb_txfrm_t;

typedef struct acx100_usb_scan {
	u16 unk1 ACX_PACKED;
	u16 unk2 ACX_PACKED;
} acx100_usb_scan_t;

typedef struct acx100_usb_scan_status {
	u16 rid ACX_PACKED;
	u16 length ACX_PACKED;
	u32	status ACX_PACKED;
} acx100_usb_scan_status_t;

typedef struct acx100_usb_cmdreq {
	u16 parm0 ACX_PACKED;
	u16 parm1 ACX_PACKED;
	u16 parm2 ACX_PACKED;
	u8 pad[54] ACX_PACKED;
} acx100_usb_cmdreq_t;

typedef struct acx100_usb_wridreq {
	u16 rid ACX_PACKED;
	u16 frmlen ACX_PACKED;
	u8 data[ACX100_RIDDATA_MAXLEN] ACX_PACKED;
} acx100_usb_wridreq_t;

typedef struct acx100_usb_rridreq {
	u16 rid ACX_PACKED;
	u16 frmlen ACX_PACKED;
	u8 pad[56] ACX_PACKED;
} acx100_usb_rridreq_t;

typedef struct acx100_usb_wmemreq {
	/*
  u16 offset ACX_PACKED;
	u16 page ACX_PACKED;
  */
	u8 data[ACX100_USB_RWMEM_MAXLEN] ACX_PACKED;
} acx100_usb_wmemreq_t;

typedef struct acx100_usb_rxtx_ctrl {
	u8 data ACX_PACKED;
} acx100_usb_rxtx_ctrl_t;

typedef struct acx100_usb_rmemreq {
	u8 data[ACX100_USB_RWMEM_MAXLEN] ACX_PACKED;
} acx100_usb_rmemreq_t;

/*--- Response (bulk IN) packet contents -------------------------------------*/

typedef struct acx100_usb_rxfrm {
	acx100_rx_frame_t desc ACX_PACKED;
	u8 data[WLAN_DATA_MAXLEN] ACX_PACKED;
} acx100_usb_rxfrm_t;

typedef struct acx100_usb_cmdresp {
	u16 resp0 ACX_PACKED;
	u16 resp1 ACX_PACKED;
	u16 resp2 ACX_PACKED;
} acx100_usb_cmdresp_t;

typedef struct acx100_usb_wridresp {
	u16 resp0 ACX_PACKED;
	u16 resp1 ACX_PACKED;
	u16 resp2 ACX_PACKED;
} acx100_usb_wridresp_t;

typedef struct acx100_usb_rridresp {
	u16 rid ACX_PACKED;
	u16 frmlen ACX_PACKED;
	u8 data[ACX100_RIDDATA_MAXLEN] ACX_PACKED;
} acx100_usb_rridresp_t;

typedef struct acx100_usb_wmemresp {
	u16 resp0 ACX_PACKED;
	u16 resp1 ACX_PACKED;
	u16 resp2 ACX_PACKED;
} acx100_usb_wmemresp_t;

typedef struct acx100_usb_rmemresp {
	u8 data[ACX100_USB_RWMEM_MAXLEN] ACX_PACKED;
} acx100_usb_rmemresp_t;

typedef struct acx100_usb_bufavail {
	u16 type ACX_PACKED;
	u16 frmlen ACX_PACKED;
} acx100_usb_bufavail_t;

typedef struct acx100_usb_error {
	u16 type ACX_PACKED;
	u16 errortype ACX_PACKED;
} acx100_usb_error_t;

/*--- Unions for packaging all the known packet types together ---------------*/

#if OLD
typedef union acx100_usbout {
        u16 type ACX_PACKED;
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
  u16 cmd ACX_PACKED;
  u16 status ACX_PACKED;
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
	u16 type ACX_PACKED;
	acx100_usb_rxfrm_t rxfrm ACX_PACKED;
	acx100_usb_txfrm_t txfrm ACX_PACKED;
	acx100_usb_cmdresp_t cmdresp ACX_PACKED;
	acx100_usb_wridresp_t wridresp ACX_PACKED;
	acx100_usb_rridresp_t rridresp ACX_PACKED;
	acx100_usb_wmemresp_t wmemresp ACX_PACKED;
	acx100_usb_rmemresp_t rmemresp ACX_PACKED;
	acx100_usb_bufavail_t bufavail ACX_PACKED;
	acx100_usb_error_t usberror ACX_PACKED;
	u8 boguspad[3000] ACX_PACKED;
} acx100_usbin_t;
#else
typedef struct acx100_usbin {
  u16 cmd ACX_PACKED;
  u16 status ACX_PACKED;
  union {
	acx100_usb_rxfrm_t rxfrm ACX_PACKED;
	acx100_usb_txfrm_t txfrm ACX_PACKED;
	acx100_usb_cmdresp_t cmdresp ACX_PACKED;
	acx100_usb_wridresp_t wridresp ACX_PACKED;
	acx100_usb_rridresp_t rridresp ACX_PACKED;
	acx100_usb_wmemresp_t wmemresp ACX_PACKED;
	acx100_usb_rmemresp_t rmemresp ACX_PACKED;
	acx100_usb_bufavail_t bufavail ACX_PACKED;
	acx100_usb_error_t usberror ACX_PACKED;
	u8 boguspad[3000] ACX_PACKED;
  } u ACX_PACKED;
} acx100_usbin_t;
#endif

#endif /* WLAN_HOSTIF == WLAN_USB */

/*--- Firmware statistics ----------------------------------------------------*/
typedef struct fw_stats {
	u32 val0x0 ACX_PACKED;		/* hdr; */
	u32 tx_desc_of ACX_PACKED;
	u32 rx_oom ACX_PACKED;
	u32 rx_hdr_of ACX_PACKED;
	u32 rx_hdr_use_next ACX_PACKED;
	u32 rx_dropped_frame ACX_PACKED;
	u32 rx_frame_ptr_err ACX_PACKED;
	u32 rx_xfr_hint_trig ACX_PACKED;

	u32 rx_dma_req ACX_PACKED;	/* val0x1c */
	u32 rx_dma_err ACX_PACKED;	/* val0x20 */
	u32 tx_dma_req ACX_PACKED;
	u32 tx_dma_err ACX_PACKED;	/* val0x28 */

	u32 cmd_cplt ACX_PACKED;
	u32 fiq ACX_PACKED;
	u32 rx_hdrs ACX_PACKED;		/* val0x34 */
	u32 rx_cmplt ACX_PACKED;		/* val0x38 */
	u32 rx_mem_of ACX_PACKED;		/* val0x3c */
	u32 rx_rdys ACX_PACKED;
	u32 irqs ACX_PACKED;
	u32 acx_trans_procs ACX_PACKED;
	u32 decrypt_done ACX_PACKED;	/* val0x48 */
	u32 dma_0_done ACX_PACKED;
	u32 dma_1_done ACX_PACKED;
	u32 tx_exch_complet ACX_PACKED;
	u32 commands ACX_PACKED;
	u32 acx_rx_procs ACX_PACKED;
	u32 hw_pm_mode_changes ACX_PACKED;
	u32 host_acks ACX_PACKED;
	u32 pci_pm ACX_PACKED;
	u32 acm_wakeups ACX_PACKED;

	u32 wep_key_count ACX_PACKED;
	u32 wep_default_key_count ACX_PACKED;
	u32 dot11_def_key_mib ACX_PACKED;
	u32 wep_key_not_found ACX_PACKED;
	u32 wep_decrypt_fail ACX_PACKED;
} fw_stats_t;

/* Firmware version struct */

typedef struct fw_ver {
	u16 vala ACX_PACKED;
	u16 valb ACX_PACKED;
	char fw_id[20] ACX_PACKED;
	u32 hw_id ACX_PACKED;
} fw_ver_t;

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
	u8 index;
	size_t size;
	u8 key[29];
	u16 strange_filler;
} wep_key_t;			/* size = 264 bytes (33*8) */
/* FIXME: We don't have size 264! Or is there 2 bytes beyond the key
 * (strange_filler)? */

/* non-firmware struct --> no packing necessary */
typedef struct key_struct {
	u8 addr[ETH_ALEN];	/* 0x00 */
	u16 filler1;		/* 0x06 */
	u32 filler2;		/* 0x08 */
	u32 index;		/* 0x0c */
	u16 len;		/* 0x10 */
	u8 key[29];		/* 0x12; is this long enough??? */
} key_struct_t;			/* size = 276. FIXME: where is the remaining space?? */


/* non-firmware struct --> no packing necessary */
enum {
	CLIENT_EMPTY_SLOT_0 = 0,
	CLIENT_EXIST_1 = 1,
	CLIENT_AUTHENTICATED_2 = 2,
	CLIENT_ASSOCIATED_3 = 3,
};
typedef struct client {
	u8	used;		/* misnamed, more like 'status' */
	u16	aid;		/* association ID */
	char	address[ETH_ALEN];
	u8	ps;		/* client's auth req had PWRMGT bit in fc */
	u16	cap_info;	/* from client's assoc req */
	u16	auth_alg;
	u16	auth_step;
	u8	*ratevec;	/* from client's assoc req */
	u32	unkn0x14;
	u8	challenge_text[WLAN_CHALLENGE_LEN];
	u16	seq;		/* from client's auth req */
	u16	ratemask;	/* bit 0=1Mbit..bit5=22Mbit */
	u8	pad5[8];	/* 0x9c */
	struct client *next;	/* 0xa4 */
} client_t;

/*--- Tx and Rx descriptor ring buffer administration ------------------------*/
typedef struct TIWLAN_DC {	/* V3 version */
	struct	wlandevice 	*priv;
	/* This is the pointer to the beginning of the card's tx queue pool.
	   The address is relative to the internal memory mapping of the card! */
	u32		ui32ACXTxQueueStart;	/* 0x8, official name */
	/* This is the pointer to the beginning of the card's rx queue pool.
	   The address is relative to the internal memory mapping of the card! */
	u32		ui32ACXRxQueueStart;	/* 0xc */
	u8		*pTxBufferPool;		/* 0x10 */
	u32		TxBufferPoolSize;	/* 0x18 */
	dma_addr_t	TxBufferPoolPhyAddr;	/* 0x14 */
	u16		TxDescrSize;		/* size per tx descr; ACX111 = ACX100 + 4 */
	struct	txdescriptor	*pTxDescQPool;	/* V13POS 0x1c, official name */
	spinlock_t	tx_lock;
	u32		tx_pool_count;		/* 0x20 indicates # of ring buffer pool entries */
	u32		tx_head;		/* 0x24 current ring buffer pool member index */
	u32		tx_tail;		/* 0x34,pool_idx2 is not correct, I'm
						 * just using it as a ring watch
						 * official name */
	struct	framehdr	*pFrameHdrQPool;/* 0x28 */
	unsigned int	FrameHdrQPoolSize;	/* 0x2c */
	dma_addr_t	FrameHdrQPoolPhyAddr;	/* 0x30 */
	/* u32 		val0x38; */	/* 0x38, NOT USED */

	/* This is the pointer to the beginning of the host's tx queue pool.
	   The address is relative to the cards internal memory mapping */
	struct txhostdescriptor *pTxHostDescQPool;	/* V3POS 0x3c, V1POS 0x60 */
	unsigned int	TxHostDescQPoolSize;	/* 0x40 */
	/* This is the pointer to the beginning of the hosts tx queue pool.
	   The address is relative to the host memory mapping */
	dma_addr_t	TxHostDescQPoolPhyAddr;	/* 0x44 */
	/* u32		val0x48; */	/* 0x48, NOT USED */
	/* u32		val0x4c; */	/* 0x4c, NOT USED */

	/* This is the pointer to the beginning of the cards rx queue pool.
	   The Adress is relative to the host memory mapping!! */
	struct	rxdescriptor	*pRxDescQPool;	/* V1POS 0x74, V3POS 0x50 */
	spinlock_t	rx_lock;
	u32		rx_pool_count;		/* V1POS 0x78, V3POS 0X54 */
	u32		rx_tail;		/* 0x6c */
	/* u32		val0x50; */	/* V1POS:0x50, some size NOT USED */
	/* u32		val0x54; */	/* 0x54, official name NOT USED */

	/* This is the pointer to the beginning of the hosts rx queue pool.
	   The address is relative to the card internal memory mapping */
	struct rxhostdescriptor *pRxHostDescQPool;	/* 0x58, is it really rxdescriptor? */
	unsigned int	RxHostDescQPoolSize;	/* 0x5c */
	/* This is the pointer to the beginning of the hosts rx queue pool.
	   The address is relative to the host memory mapping */
	dma_addr_t	RxHostDescQPoolPhyAddr;	/* 0x60, official name. */
	/* u32		val0x64; */	/* 0x64, some size */
	u8		*pRxBufferPool;		/* this is supposed to be [rxbuffer *], but it's not defined here, so let's define it as [u8 *] */
	u32		RxBufferPoolSize;
	dma_addr_t	RxBufferPoolPhyAddr;	/* *rxdescq2; 0x74 */
} TIWLAN_DC;

/*--- 802.11 Basic Service Set info ------------------------------------------*/
/* non-firmware struct --> no packing necessary */
typedef struct bss_info {
	u8 bssid[ETH_ALEN];	/* BSSID (network ID of the device) */
	u8 mac_addr[ETH_ALEN];	/* MAC address of the station's device */
	u8 essid[IW_ESSID_MAX_SIZE + 1];	/* ESSID and trailing '\0'  */
	size_t essid_len;	/* Length of ESSID (FIXME: \0 included?) */
	u16 caps;		/* 802.11 capabilities information */
	u8 channel;		/* 802.11 channel */
	u16 wep;		/* WEP flag (FIXME: redundant, bit 4 in caps) */
	unsigned char supp_rates[64]; /* FIXME: 802.11b section 7.3.2.2 allows 8 rates, but 802.11g devices seem to allow for many more: how many exactly? */
	u32 sir;		/* 0x78; Standard IR */
	u32 snr;		/* 0x7c; Signal to Noise Ratio */
	u16 beacon_interval;	/* 802.11 beacon interval */
} bss_info_t;				/* 132 0x84 */

/*============================================================================*
 * Main acx100 per-device data structure (netdev->priv)                       *
 *============================================================================*/
#define ACX_STATE_FW_LOADED	0x01
#define ACX_STATE_IFACE_UP	0x02

/* MAC mode (BSS type) defines
 * Note that they shouldn't be redefined, since they are also used
 * during communication with firmware */
#define ACX_MODE_0_ADHOC	0
#define ACX_MODE_1_UNUSED	1
#define ACX_MODE_2_STA		2
#define ACX_MODE_3_AP		3
/* These are our own inventions. Do not send such modes to firmware */
#define ACX_MODE_MONITOR	0xfe
#define ACX_MODE_OFF		0xff
/* 'Submode': identifies exact status of ADHOC/STA host */
#define ACX_STATUS_0_STOPPED		0
#define ACX_STATUS_1_SCANNING		1
#define ACX_STATUS_2_WAIT_AUTH		2
#define ACX_STATUS_3_AUTHENTICATED	3
#define ACX_STATUS_4_ASSOCIATED		4

/* non-firmware struct --> no packing necessary */
struct txrate_ctrl {
	u16  cfg;		      /* Tx rate from iwconfig */
	u16  cur;		      /* the Tx rate we currently use */
	u8   pbcc511;		  /* Use PBCC at 5 and 11Mbit? (else CCK) */
	u8   do_auto;		      /* Auto adjust Tx rates? */
	u8   ignore_count;	/* no. of pkts to ignore (old rate byte set) */
	u8   fallback_threshold;     /* 0-100 */
	u8   fallback_count;
	u8   stepup_threshold;	/* 0-100 */
	u8   stepup_count;
	/* TODO: unsigned long txcnt[]; */
};

/* non-firmware struct --> no packing necessary */
struct peer {
	struct txrate_ctrl txbase;      /* For basic rates */
	struct txrate_ctrl txrate;      /* For operational rates */
	u8   shortpre;		 /* 0 == Long Preamble, 1 == Short */
};

/* FIXME: this should be named something like struct acx_priv (typedef'd to
 * acx_priv_t) */

/* non-firmware struct --> no packing necessary */
typedef struct wlandevice {
	/* !!! keep most often accessed parameters at first position to avoid dereferencing penalty !!! */
	const u16		*io;		/* points to ACX100 or ACX111 I/O register address set */

	/*** Device chain ***/
	struct wlandevice	*next;		/* link for list of devices */

	struct pci_dev *pdev;	

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
	acx100_usbin_t		rxtruncbuf;
	acx100_usbin_t		bulkins[ACX100_USB_NUM_BULK_URBS];
	acx100_usbout_t		ctrlout;
	acx100_usbin_t		ctrlin;
	acx100_bulkout_t	bulkouts[ACX100_USB_NUM_BULK_URBS];
	int			usb_txoffset;
	int			usb_txsize;
	int			usb_free_tx;
	int			rxtruncsize;
	int			bulkinep;	/* bulk-in endpoint */
	int			bulkoutep;	/* bulk-out endpoint */
	struct txdescriptor *	currentdesc;
	spinlock_t		usb_ctrl_lock;
	spinlock_t		usb_tx_lock;
	struct	urb		*ctrl_urb;
	struct	urb		*bulkrx_urbs[ACX100_USB_NUM_BULK_URBS];
	struct	urb		*bulktx_urbs[ACX100_USB_NUM_BULK_URBS];
	unsigned char		bulktx_states[ACX100_USB_NUM_BULK_URBS];
	unsigned char		usb_setup[8];
	unsigned char		rxtruncation;
#endif

	/*** Management timer ***/
	struct	timer_list	mgmt_timer;

	/*** Locking ***/
	spinlock_t	lock;			/* mutex for concurrent accesses to structure */

	/*** Hardware identification ***/
	u8		form_factor;
	u8		radio_type;
	u8		eeprom_version;

	/*** Firmware identification ***/
	char		firmware_version[20];
	u32		firmware_numver;
	u32		firmware_id;

	/*** Hardware resources ***/
	u16		irq_mask;		/* interrupts types to mask out (not wanted) with many IRQs activated */
	u16		irq_mask_off;		/* interrupts types to mask out (not wanted) with IRQs off */

	unsigned long	membase;		/* 10 */
	unsigned long	membase2;		/* 14 */
	void 		*iobase;		/* 18 */
	void		*iobase2;		/* 1c */
	unsigned int	chip_type;
	const char	*chip_name;
	u8		bus_type;

	/*** Device state ***/
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 10)
	/* 2.6.9-rc3-mm2 (2.6.9-bk4, too) introduced a shorter API version,
	   then it made its way into 2.6.10 */
	u32		pci_state[16];		/* saved PCI state for suspend/resume */
#endif
	int		hw_unavailable;		/* indicates whether the hardware has been
						 * suspended or ejected. actually a counter. */
	u16		dev_state_mask;
	u8		led_power;		/* power LED status */
	u32		get_mask;		/* mask of settings to fetch from the card */
	u32		set_mask;		/* mask of settings to write to the card */

	u8		irqs_active;		/* whether irq sending is activated */
	u16		irq_status;
	u8		after_interrupt_jobs;	/* mini job list for doing actions after an interrupt occurred */
	WORK_STRUCT	after_interrupt_task;	/* our task for after interrupt actions */

	/*** scanning ***/
	u16		scan_count;		/* number of times to do channel scan */
	u8		scan_mode;		/* 0 == active, 1 == passive, 2 == background */
	u16		scan_duration;
	u16		scan_probe_delay;
	u8		scan_rate;
#if WIRELESS_EXT > 15
	struct iw_spy_data	spy_data;	/* FIXME: needs to be implemented! */
#endif
			
	/*** Wireless network settings ***/
	u8		dev_addr[MAX_ADDR_LEN]; /* copy of the device address (ifconfig hw ether) that we actually use for 802.11; copied over from the network device's MAC address (ifconfig) when it makes sense only */
	u8		bssid[ETH_ALEN];	/* the BSSID after having joined */
	u8		ap[ETH_ALEN];		/* The AP we want, FF:FF:FF:FF:FF:FF is any */
	u16		aid;			/* The Association ID sent from the AP */
	u16		mode;			/* mode from iwconfig */
	u16		status;			/* 802.11 association status */
	u8		essid_active;		/* specific ESSID active, or select any? */
	u8		essid_len;		/* to avoid dozens of strlen() */
	char		essid[IW_ESSID_MAX_SIZE+1];	/* V3POS 84; essid; INCLUDES \0 termination for easy printf - but many places simply want the string data memcpy'd plus a length indicator! Keep that in mind... */
	char		essid_for_assoc[IW_ESSID_MAX_SIZE+1];	/* the ESSID we are going to use for association, in case of "essid 'any'" and in case of hidden ESSID (use configured ESSID then) */
	char		nick[IW_ESSID_MAX_SIZE+1]; /* see essid! */
	u16		channel;		/* V3POS 22f0, V1POS b8 */
	u8		reg_dom_id;		/* reg domain setting */
	u16		reg_dom_chanmask;
	u16		auth_or_assoc_retries;	/* V3POS 2827, V1POS 27ff */

	u16		bss_table_count;	/* # of active BSS scan table entries */
	struct		bss_info bss_table[32];	/* BSS scan table */
	struct		bss_info station_assoc;	/* the station we're currently associated to */
	u8		scan_running;
	unsigned long	scan_start;		/* FIXME type?? */
	u16		scan_retries;		/* V3POS 2826, V1POS 27fe */

	/* stations known to us (if we're an ap) */
	client_t	sta_list[32];		/* tab is larger than list, so that */
	client_t	*sta_hash_tab[64];	/* hash collisions are not likely */

	u16		last_seq_ctrl;		/* duplicate packet detection */

	/* 802.11 power save mode */
	u8		ps_wakeup_cfg;
	u8		ps_listen_interval;
	u8		ps_options;
	u8		ps_hangover_period;
	u16		ps_enhanced_transition_time;

	/*** PHY settings ***/
	struct peer	defpeer;
	struct peer	ap_peer;

	u8		tx_disabled;
	u8		tx_level_dbm;
	u8		tx_level_val;
	u8		tx_level_auto;		/* whether to do automatic power adjustment */
	unsigned long	time_last_recalib_success;

	unsigned long	brange_time_last_state_change;	/* time the power LED was last changed */
	u8		brange_last_state;					/* last state of the LED */
	u8		brange_max_quality;					/* maximum quality that equates to full speed */

	u8		preamble_mode;		/* 0 == Long Preamble, 1 == Short, 2 == Auto */

	u8		sensitivity;
	u8		antenna;		/* antenna settings */
	u8		ed_threshold;		/* energy detect threshold */
	u8		cca;			/* clear channel assessment */

	u16		rts_threshold;
	u32		short_retry;		/* V3POS 204, V1POS 20c */
	u32		long_retry;		/* V3POS 208, V1POS 210 */
	u16		msdu_lifetime;		/* V3POS 20c, V1POS 214 */
	u16		listen_interval;	/* V3POS 250, V1POS 258, given in units of beacon interval */
	u32		beacon_interval;	/* V3POS 2300, V1POS c8 */

	u16		capabilities;		/* V3POS b0 */
	u8		capab_short;		/* V3POS 1ec, V1POS 1f4 */
	u8		capab_pbcc;		/* V3POS 1f0, V1POS 1f8 */
	u8		capab_agility;		/* V3POS 1f4, V1POS 1fc */
	u8		rate_supported_len;	/* V3POS 1243, V1POS 124b */
	u8		rate_supported[13];	/* V3POS 1244, V1POS 124c */

	/*** Encryption settings (WEP) ***/
	u32		auth_alg;		/* V3POS 228, V3POS 230, used in transmit_authen1 */
	u8		wep_enabled;
	u8		wep_restricted;		/* V3POS c0 */
	u8		wep_current_index;	/* V3POS 254, V1POS 25c not sure about this */
	wep_key_t	wep_keys[DOT11_MAX_DEFAULT_WEP_KEYS];	/* the default WEP keys */
	key_struct_t	wep_key_struct[10];	/* V3POS 688 */

	/*** Card Rx/Tx management ***/
	u32		promiscuous;
	u32		mc_count;		/* multicast count */
	u16		rx_config_1;		/* V3POS 2820, V1POS 27f8 */
	u16		rx_config_2;		/* V3POS 2822, V1POS 27fa */
	TIWLAN_DC	dc;			/* V3POS 2380, V1POS 2338 */
	u32		TxQueueCnt;		/* V3POS 24dc, V1POS 24b4 */
	u32		RxQueueCnt;		/* V3POS 24f4, V1POS 24cc */
	u32		TxQueueFree;
	struct	rxhostdescriptor *RxHostDescPoolStart;	/* V3POS 24f8, V1POS 24d0 */
	u32		tx_cnt_done;
	u16		memblocksize;		/* V3POS 2354, V1POS 230c */
	u32		RxBlockNum;		/* V3POS 24e4, V1POS 24bc */
	u32		TotalRxBlockSize;	/* V3POS 24e8, V1POS 24c0 */
	u32		TxBlockNum;		/* V3POS 24fc, V1POS 24d4 */
	u32		TotalTxBlockSize;	/* V3POS 2500, V1POS 24d8 */

	/*** ACX100 command interface ***/
	u16		cmd_type;		/* V3POS 2508, V1POS 24e0 */
	u16		cmd_status;		/* V3POS 250a, V1POS 24e2 */
	void		*CommandParameters;	/* FIXME: used to be an array unsigned int*0x88,
						 * but it should most likely be *one*
						 * u32 instead, pointing to the cmd
						 * param memory. V3POS 268c, V1POS 2664 */

	u16		info_type;		/* V3POS 2508, V1POS 24e0 */
	u16		info_status;		/* V3POS 250a, V1POS 24e2 */
	void		*InfoParameters;		/* V3POS 2814, V1POS 27ec */

	/*** Unknown ***/
	u8		dtim_interval;		/* V3POS 2302 */
#if UNUSED
	u8		val0x2324_0;		/* V3POS 2324 */
	u8		val0x2324_2;
	u8		val0x2324_4;
	u8		val0x2324_5;
	u8		val0x2324_6;
	u8		val0x2324_7;
#endif
} wlandevice_t;

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
#define RX_CFG1_PLUS_ADDIT_HDR		0x2000 /* ACX100 only!! */
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
 *     8   receive contention free packets
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
#define RX_CFG2_RCV_CONTENTION_FREE	0x0100
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
/* Used when ESSID changes etc and we need to scan for AP anew */
#define GETSET_RESCAN		0x00010000L	
#define GETSET_MODE		0x00020000L
#define GETSET_WEP		0x00040000L
#define SET_WEP_OPTIONS		0x00080000L
#define SET_MSDU_LIFETIME	0x00100000L
#define SET_RATE_FALLBACK	0x00200000L
#define GETSET_ALL		0x80000000L

void acx_rx(struct rxhostdescriptor *rxdesc, wlandevice_t *priv);

/*============================================================================*
 * Firmware loading                                                           *
 *============================================================================*/
#if defined(CONFIG_FW_LOADER) || defined(CONFIG_FW_LOADER_MODULE)
#define USE_FW_LOADER_26 1
#endif
#define USE_FW_LOADER_LEGACY 1

#ifdef USE_FW_LOADER_26
#include <linux/firmware.h>	/* request_firmware() */
#include <linux/pci.h>		/* struct pci_device */
#endif

#ifdef USE_FW_LOADER_LEGACY
extern char *firmware_dir;
#endif

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

#ifndef ARPHRD_IEEE80211_PRISM
#define ARPHRD_IEEE80211_PRISM 802
#endif

#endif /* __ACX_ACX100_H */
