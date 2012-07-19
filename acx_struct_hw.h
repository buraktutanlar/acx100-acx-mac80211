/*
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008
 * The ACX100 Open Source Project <acx100-devel@lists.sourceforge.net>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef _ACX_STRUCT_HW_H_
#define _ACX_STRUCT_HW_H_

#include <linux/version.h>
#include <linux/if_ether.h>
#include <linux/ieee80211.h>
#include <linux/wireless.h>

/***********************************************************************
** BOM Forward declarations of types
*/
typedef struct tx tx_t;
typedef struct acx_device acx_device_t;
typedef struct rxdesc rxdesc_t;
typedef struct txdesc txdesc_t;
typedef struct rxhostdesc rxhostdesc_t;
typedef struct txhostdesc txhostdesc_t;


/***********************************************************************
** BOM Random helpers
*/
#define ACX_PACKED __attribute__ ((packed))

/***********************************************************************
** BOM Constants
*/

/* Support Constants */
/* Radio type names, found in Win98 driver's TIACXLN.INF */
/* 0D: used in DWL-120+ USB cards (side-antenna and flip-antenna versions) */
#define RADIO_0D_MAXIM_MAX2820	0x0d
#define RADIO_11_RFMD		0x11
#define RADIO_15_RALINK		0x15
/* 16: Radia RC2422, used in ACX111 cards (WG311v2, WL-121, ...): */
#define RADIO_16_RADIA_RC2422	0x16
/* most likely *sometimes* used in ACX111 cards: */
#define RADIO_17_UNKNOWN	0x17
/* FwRad19.bin was found in a Safecom driver; must be an ACX111 radio: */
#define RADIO_19_UNKNOWN	0x19
/* 1B: radio in SafeCom SWLUT-54125 TNETW1450 USB adapter,
   label: G3 55ZCT27 TNETW3422 */
#define RADIO_1B_TI_TNETW3422	0x1b

/*
 * BOM WLAN header constants previously in wlan_hdr.h
 *
 * They used in acx-device buffer-structure definitions.
 * Keeping them as documenting elements of the previous driver.
 */

#define WLAN_HDR_A3_LEN			24
#define WLAN_HDR_A4_LEN			30
/* IV structure:
** 3 bytes: Initialization Vector (24 bits)
** 1 byte: 0..5: padding, must be 0; 6..7: key selector (0-3)
*/
#define WLAN_WEP_IV_LEN			4
/* 802.11 says 2312 but looks like 2312 is a max size of _WEPed data_ */
#define WLAN_DATA_MAXLEN		2304
#define WLAN_WEP_ICV_LEN		4
#define WLAN_FCS_LEN			4
#define WLAN_A3FR_MAXLEN		(WLAN_HDR_A3_LEN + WLAN_DATA_MAXLEN)
#define WLAN_A4FR_MAXLEN		(WLAN_HDR_A4_LEN + WLAN_DATA_MAXLEN)
#define WLAN_A3FR_MAXLEN_FCS		(WLAN_HDR_A3_LEN + WLAN_DATA_MAXLEN + 4)
#define WLAN_A4FR_MAXLEN_FCS		(WLAN_HDR_A4_LEN + WLAN_DATA_MAXLEN + 4)
#define WLAN_A3FR_MAXLEN_WEP		(WLAN_A3FR_MAXLEN + 8)
#define WLAN_A4FR_MAXLEN_WEP		(WLAN_A4FR_MAXLEN + 8)
#define WLAN_A3FR_MAXLEN_WEP_FCS	(WLAN_A3FR_MAXLEN_FCS + 8)
#define WLAN_A4FR_MAXLEN_WEP_FCS	(WLAN_A4FR_MAXLEN_FCS + 8)


/* BOM Controller Commands
 * Can be found in the cmdTable table on the "Rev. 1.5.0" (FW150) firmware
 */
#define ACX1xx_CMD_RESET		    0x00
#define ACX1xx_CMD_INTERROGATE		0x01
#define ACX1xx_CMD_CONFIGURE		0x02
#define ACX1xx_CMD_ENABLE_RX		0x03
#define ACX1xx_CMD_ENABLE_TX		0x04
#define ACX1xx_CMD_DISABLE_RX		0x05
#define ACX1xx_CMD_DISABLE_TX		0x06
#define ACX1xx_CMD_FLUSH_QUEUE		0x07
#define ACX1xx_CMD_SCAN			    0x08
#define ACX1xx_CMD_STOP_SCAN		0x09
#define ACX1xx_CMD_CONFIG_TIM		0x0A
#define ACX1xx_CMD_JOIN			    0x0B
#define ACX1xx_CMD_WEP_MGMT		    0x0C
#ifdef OLD_FIRMWARE_VERSIONS
#define ACX100_CMD_HALT             0x0E    /* mapped to unknownCMD in FW150 */
#else
#define ACX1xx_CMD_MEM_READ         0x0D
#define ACX1xx_CMD_MEM_WRITE        0x0E
#endif /* OLD_FIRMWARE_VERSIONS */
#define ACX1xx_CMD_SLEEP		    0x0F
#define ACX1xx_CMD_WAKE			    0x10
#define ACX1xx_CMD_UNKNOWN_11		0x11	/* mapped to unknownCMD in FW150 */
#define ACX100_CMD_INIT_MEMORY		0x12
#define ACX1FF_CMD_DISABLE_RADIO	0x12	/* new firmware? TNETW1450? + NOT in BSD driver */
#define ACX1xx_CMD_CONFIG_BEACON	0x13
#define ACX1xx_CMD_CONFIG_PROBE_RESPONSE 0x14
#define ACX1xx_CMD_CONFIG_NULL_DATA	0x15
#define ACX1xx_CMD_CONFIG_PROBE_REQUEST 0x16
#define ACX1xx_CMD_FCC_TEST		    0x17
#define ACX1xx_CMD_RADIOINIT		0x18
#define ACX111_CMD_RADIOCALIB		0x19
#define ACX1FF_CMD_NOISE_HISTOGRAM	0x1c	/* new firmware? TNETW1450? */
#define ACX1FF_CMD_RX_RESET		    0x1d	/* new firmware? TNETW1450? */
#define ACX1FF_CMD_LNA_CONTROL		0x20	/* new firmware? TNETW1450? */
#define ACX1FF_CMD_CONTROL_DBG_TRACE	0x21	/* new firmware? TNETW1450? */

/*
 * BOM Interrogate/Configure cmd constants
 *
 * NB: length includes JUST the data part of the IE
 * (does not include size of the (type,len) pair)
 *
 * TODO: seems that acx100, acx100usb, acx111 have some differences,
 * fix code with regard to this!
 *
 * OW, 20100703: acx IE definitions here checked against definitions in acx-20080210: OK
 */

/* Information Elements: Network Parameters, Static Configuration Entities */
/* these are handled by real_cfgtable in firmware "Rev 1.5.0" (FW150) */
#define ACX1xx_IE_UNKNOWN_00					(0x0000)	/* mapped to cfgInvalid in FW150 */
#define ACX1xx_IE_UNKNOWN_00_LEN				(-1)
#define ACX100_IE_ACX_TIMER						(0x0001)
#define ACX100_IE_ACX_TIMER_LEN 				(0x10)
#define ACX1xx_IE_POWER_MGMT					(0x0002)	/* TNETW1450: length 0x18!! */
#define ACX1xx_IE_POWER_MGMT_LEN 				(0x06)
#define ACX1xx_IE_QUEUE_CONFIG					(0x0003)
#define ACX1xx_IE_QUEUE_CONFIG_LEN				(0x1c)
#define ACX100_IE_BLOCK_SIZE					(0x0004)
#define ACX100_IE_BLOCK_SIZE_LEN				(0x02)
#define ACX1FF_IE_SLOT_TIME						(0x0004)	/* later firmware versions only? */
#define ACX1FF_IE_SLOT_TIME_LEN					(0x08)
#define ACX1xx_IE_MEMORY_CONFIG_OPTIONS			(0x0005)
#define ACX1xx_IE_MEMORY_CONFIG_OPTIONS_LEN		(0x14)
#define ACX1FF_IE_QUEUE_HEAD					(0x0005)
#define ACX1FF_IE_QUEUE_HEAD_LEN				(0x14 /* FIXME: length? */)
#define ACX1xx_IE_RATE_FALLBACK					(0x0006)	/* TNETW1450: length 2 */
#define ACX1xx_IE_RATE_FALLBACK_LEN				(0x01)
#define ACX100_IE_WEP_OPTIONS					(0x0007)
#define ACX100_IE_WEP_OPTIONS_LEN				(0x03)
#define ACX111_IE_RADIO_BAND					(0x0007)
#define ACX111_IE_RADIO_BAND_LEN				(-1)
#define ACX1FF_IE_TIMING_CFG					(0x0007)	/* later firmware versions; TNETW1450 only? */
#define ACX1FF_IE_TIMING_CFG_LEN				(-1)
#define ACX100_IE_SSID							(0x0008)	/* huh? */
#define ACX100_IE_SSID_LEN						(0x20)
#define ACX1xx_IE_MEMORY_MAP					(0x0008)	/* huh? TNETW1450 has length 0x40!! */
#define ACX1xx_IE_MEMORY_MAP_LEN				(0x28)
#define ACX1xx_IE_SCAN_STATUS					(0x0009)	/* mapped to cfgInvalid in FW150 */
#define ACX1xx_IE_SCAN_STATUS_LEN				(0x04)
#define ACX1xx_IE_ASSOC_ID						(0x000a)
#define ACX1xx_IE_ASSOC_ID_LEN					(0x02)
#define ACX1xx_IE_UNKNOWN_0B					(0x000B)	/* mapped to cfgInvalid in FW150 */
#define ACX1xx_IE_UNKNOWN_0B_LEN				(-1)
#define ACX1FF_IE_TX_POWER_LEVEL_TABLE			(0x000B)	/* later firmware versions; TNETW1450 only? */
#define ACX1FF_IE_TX_POWER_LEVEL_TABLE_LEN		(0x18)
#define ACX100_IE_UNKNOWN_0C					(0x000C)	/* very small implementation in FW150! */
#define ACX100_IE_UNKNOWN_0C_LEN				(-1)
/* ACX100 has an equivalent struct in the cmd mailbox directly after reset.
 * 0x14c seems extremely large, will trash stack on failure (memset!)
 * in case of small input struct --> OOPS! */
#define ACX111_IE_CONFIG_OPTIONS				(0x000c)
#define ACX111_IE_CONFIG_OPTIONS_LEN			(0x14c)
#define ACX1xx_IE_FWREV							(0x000d)
#define ACX1xx_IE_FWREV_LEN						(0x18)
#define ACX1xx_IE_FCS_ERROR_COUNT				(0x000e)
#define ACX1xx_IE_FCS_ERROR_COUNT_LEN			(0x04)
#define ACX1xx_IE_MEDIUM_USAGE					(0x000f)	/* AKA MEDIUM OCCUPANCY */
#define ACX1xx_IE_MEDIUM_USAGE_LEN				(0x08)
#define ACX1xx_IE_RXCONFIG						(0x0010)
#define ACX1xx_IE_RXCONFIG_LEN					(0x04)
#define ACX100_IE_UNKNOWN_11					(0x0011)	/* NONBINARY: large implementation in FW150! link quality readings or so? */
#define ACX100_IE_UNKNOWN_11_LEN				(-1)
#define ACX111_IE_QUEUE_THRESH					(0x0011)
#define ACX111_IE_QUEUE_THRESH_LEN				(-1)
#define ACX100_IE_UNKNOWN_12					(0x0012)	/* NONBINARY: VERY large implementation in FW150!! */
#define ACX100_IE_UNKNOWN_12_LEN				(-1)
#define ACX111_IE_BSS_POWER_SAVE				(0x0012)
#define ACX111_IE_BSS_POWER_SAVE_LEN			(/* -1 */ 2)
#define ACX1xx_IE_FIRMWARE_STATISTICS			(0x0013)	/* TNETW1450: length 0x134!! */
#define ACX1xx_IE_FIRMWARE_STATISTICS_LEN		(0x9c)
#define ACX1FF_IE_RX_INTR_CONFIG				(0x0014)	/* later firmware versions, TNETW1450 only? */
#define ACX1FF_IE_RX_INTR_CONFIG_LEN			(0x14)
#define ACX1xx_IE_FEATURE_CONFIG				(0x0015)
#define ACX1xx_IE_FEATURE_CONFIG_LEN			(0x08)
#define ACX111_IE_KEY_CHOOSE					(0x0016)	/* for rekeying. really len=4?? */
#define ACX111_IE_KEY_CHOOSE_LEN				(0x04)
#define ACX1FF_IE_MISC_CONFIG_TABLE				(0x0017)	/* later firmware versions, TNETW1450 only? */
#define ACX1FF_IE_MISC_CONFIG_TABLE_LEN			(0x04)
#define ACX1FF_IE_WONE_CONFIG					(0x0018)	/* later firmware versions, TNETW1450 only? */
#define ACX1FF_IE_WONE_CONFIG_LEN				(-1)
#define ACX1FF_IE_TID_CONFIG					(0x001a)	/* later firmware versions, TNETW1450 only? */
#define ACX1FF_IE_TID_CONFIG_LEN				(0x2c)
#define ACX1FF_IE_CALIB_ASSESSMENT				(0x001e)	/* later firmware versions, TNETW1450 only? */
#define ACX1FF_IE_CALIB_ASSESSMENT_LEN			(0x04)
#define ACX1FF_IE_BEACON_FILTER_OPTIONS			(0x001f)	/* later firmware versions, TNETW1450 only? */
#define ACX1FF_IE_BEACON_FILTER_OPTIONS_LEN		(0x02)
#define ACX1FF_IE_LOW_RSSI_THRESH_OPT			(0x0020)	/* later firmware versions, TNETW1450 only? */
#define ACX1FF_IE_LOW_RSSI_THRESH_OPT_LEN		(0x04)
#define ACX1FF_IE_NOISE_HISTOGRAM_RESULTS		(0x0021)	/* later firmware versions, TNETW1450 only? */
#define ACX1FF_IE_NOISE_HISTOGRAM_RESULTS_LEN	(0x30)
#define ACX1FF_IE_PACKET_DETECT_THRESH			(0x0023)	/* later firmware versions, TNETW1450 only? */
#define ACX1FF_IE_PACKET_DETECT_THRESH_LEN		(0x04)
#define ACX1FF_IE_TX_CONFIG_OPTIONS				(0x0024)	/* later firmware versions, TNETW1450 only? */
#define ACX1FF_IE_TX_CONFIG_OPTIONS_LEN			(0x04)
#define ACX1FF_IE_CCA_THRESHOLD					(0x0025)	/* later firmware versions, TNETW1450 only? */
#define ACX1FF_IE_CCA_THRESHOLD_LEN				(0x02)
#define ACX1FF_IE_EVENT_MASK					(0x0026)	/* later firmware versions, TNETW1450 only? */
#define ACX1FF_IE_EVENT_MASK_LEN				(0x08)
#define ACX1FF_IE_DTIM_PERIOD					(0x0027)	/* later firmware versions, TNETW1450 only? */
#define ACX1FF_IE_DTIM_PERIOD_LEN				(0x02)
#define ACX1FF_IE_ACI_CONFIG_SET				(0x0029)	/* later firmware versions; maybe TNETW1450 only? */
#define ACX1FF_IE_ACI_CONFIG_SET_LEN			(0x06)
#define ACX1FF_IE_EEPROM_VER					(0x0030)	/* later firmware versions; maybe TNETW1450 only? */
#define ACX1FF_IE_EEPROM_VER_LEN				(0x04)
#define ACX1xx_IE_DOT11_STATION_ID				(0x1001)
#define ACX1xx_IE_DOT11_STATION_ID_LEN			(0x06)
#define ACX100_IE_DOT11_UNKNOWN_1002			(0x1002)	/* mapped to cfgInvalid in FW150 */
#define ACX100_IE_DOT11_UNKNOWN_1002_LEN		(-1)
#define ACX111_IE_DOT11_FRAG_THRESH				(0x1002)	/* mapped to cfgInvalid in FW150; TNETW1450 has length 2!! */
#define ACX111_IE_DOT11_FRAG_THRESH_LEN			(-1)
#define ACX100_IE_DOT11_BEACON_PERIOD			(0x1003)	/* mapped to cfgInvalid in FW150 */
#define ACX100_IE_DOT11_BEACON_PERIOD_LEN		(0x02)
#define ACX1xx_IE_DOT11_DTIM_PERIOD				(0x1004)	/* mapped to cfgInvalid in FW150 */
#define ACX1xx_IE_DOT11_DTIM_PERIOD_LEN			(-1)
#define ACX1FF_IE_DOT11_MAX_RX_LIFETIME			(0x1004)	/* later firmware versions; maybe TNETW1450 only? */
#define ACX1FF_IE_DOT11_MAX_RX_LIFETIME_LEN		(-1)
#define ACX1xx_IE_DOT11_SHORT_RETRY_LIMIT		(0x1005)	/* TNETW1450: length 2 */
#define ACX1xx_IE_DOT11_SHORT_RETRY_LIMIT_LEN	(0x01)
#define ACX1xx_IE_DOT11_LONG_RETRY_LIMIT		(0x1006)	/* TNETW1450: length 2 */
#define ACX1xx_IE_DOT11_LONG_RETRY_LIMIT_LEN	(0x01)
#define ACX100_IE_DOT11_WEP_DEFAULT_KEY_WRITE	(0x1007)	/* configure default keys; TNETW1450 has length 0x24!! */
#define ACX100_IE_DOT11_WEP_DEFAULT_KEY_WRITE_LEN	(0x20)
#define ACX1xx_IE_DOT11_MAX_XMIT_MSDU_LIFETIME	(0x1008)
#define ACX1xx_IE_DOT11_MAX_XMIT_MSDU_LIFETIME_LEN	(0x04)
#define ACX1xx_IE_DOT11_GROUP_ADDR				(0x1009)
#define ACX1xx_IE_DOT11_GROUP_ADDR_LEN			(-1)
#define ACX1xx_IE_DOT11_CURRENT_REG_DOMAIN		(0x100A)
#define ACX1xx_IE_DOT11_CURRENT_REG_DOMAIN_LEN	(0x02)
/* It's harmless to have larger struct. Use USB case always. */
#define ACX1xx_IE_DOT11_CURRENT_ANTENNA			(0x100B)	/* in fact len=1 for PCI */
#define ACX1xx_IE_DOT11_CURRENT_ANTENNA_LEN		(0x02)
#define ACX1xx_IE_DOT11_UNKNOWN_100C			(0x100C)	/* mapped to cfgInvalid in FW150 */
#define ACX1xx_IE_DOT11_UNKNOWN_100C_LEN		(-1)
#define ACX1xx_IE_DOT11_TX_POWER_LEVEL			(0x100D)	/* TNETW1450 has length 2!! */
#define ACX1xx_IE_DOT11_TX_POWER_LEVEL_LEN		(0x01)
#define ACX1xx_IE_DOT11_CURRENT_CCA_MODE		(0x100E)	/* in fact len=1 for PCI */
#define ACX1xx_IE_DOT11_CURRENT_CCA_MODE_LEN	(0x02)
/* USB doesn't return anything - len==0?! */
#define ACX100_IE_DOT11_ED_THRESHOLD			(0x100f)
#define ACX100_IE_DOT11_ED_THRESHOLD_LEN		(0x04)
#define ACX1xx_IE_DOT11_WEP_DEFAULT_KEY_SET		(0x1010)	/* set default key ID; TNETW1450: length 2 */
#define ACX1xx_IE_DOT11_WEP_DEFAULT_KEY_SET_LEN	(0x01)
#define ACX100_IE_DOT11_UNKNOWN_1011			(0x1011)	/* mapped to cfgInvalid in FW150 */
#define ACX100_IE_DOT11_UNKNOWN_1011_LEN		(-1)
#define ACX1FF_IE_DOT11_CURR_5GHZ_REGDOM		(0x1011)	/* later firmware versions; maybe TNETW1450 only? */
#define ACX1FF_IE_DOT11_CURR_5GHZ_REGDOM_LEN	(-1)
#define ACX100_IE_DOT11_UNKNOWN_1012			(0x1012)	/* mapped to cfgInvalid in FW150 */
#define ACX100_IE_DOT11_UNKNOWN_1012_LEN		(-1)
#define ACX100_IE_DOT11_UNKNOWN_1013			(0x1013)	/* mapped to cfgInvalid in FW150 */
#define ACX100_IE_DOT11_UNKNOWN_1013_LEN		(-1)

#if 0
#define DEF_IE(name, val, len) enum { ACX##name=val, ACX##name##_LEN=len }

/* Experimentally obtained on acx100, fw 1.9.8.b
** -1 means that fw returned 'invalid IE'
** 0200 FC00 nnnn... are test read contents: u16 type, u16 len, data
** (AA are poison bytes marking bytes not written by fw)
**
** Looks like acx100 fw does not update len field (thus len=256-4=FC here)
** A number of IEs seem to trash type,len fields
** IEs marked 'huge' return gobs of data (no poison bytes remain)
*/
DEF_IE(100_IE_INVAL_00,			0x0000, -1);
DEF_IE(100_IE_INVAL_01,			0x0001, -1);	/* IE_ACX_TIMER, len=16 on older fw */
DEF_IE(100_IE_POWER_MGMT,		0x0002, 4);	/* 0200FC00 00040000 AAAAAAAA */
DEF_IE(100_IE_QUEUE_CONFIG,		0x0003, 28);	/* 0300FC00 48060000 9CAD0000 0101AAAA DCB00000 E4B00000 9CAA0000 00AAAAAA */
DEF_IE(100_IE_BLOCK_SIZE,		0x0004, 2);	/* 0400FC00 0001AAAA AAAAAAAA AAAAAAAA */
/* write only: */
DEF_IE(100_IE_MEMORY_CONFIG_OPTIONS,	0x0005, 20);
DEF_IE(100_IE_RATE_FALLBACK,		0x0006, 1);	/* 0600FC00 00AAAAAA AAAAAAAA AAAAAAAA */
/* write only: */
DEF_IE(100_IE_WEP_OPTIONS,		0x0007, 3);
DEF_IE(100_IE_MEMORY_MAP,		0x0008, 40);	/* huge: 0800FC00 30000000 6CA20000 70A20000... */
/* gives INVAL on read: */
DEF_IE(100_IE_SCAN_STATUS,		0x0009, -1);
DEF_IE(100_IE_ASSOC_ID,			0x000a, 2);	/* huge: 0A00FC00 00000000 01040800 00000000... */
DEF_IE(100_IE_INVAL_0B,			0x000b, -1);
/* 'command rejected': */
DEF_IE(100_IE_CONFIG_OPTIONS,		0x000c, -3);
DEF_IE(100_IE_FWREV,			0x000d, 24);	/* 0D00FC00 52657620 312E392E 382E6200 AAAAAAAA AAAAAAAA 05050201 AAAAAAAA */
DEF_IE(100_IE_FCS_ERROR_COUNT,		0x000e, 4);
DEF_IE(100_IE_MEDIUM_USAGE,		0x000f, 8);	/* E41F0000 2D780300 FCC91300 AAAAAAAA */
DEF_IE(100_IE_RXCONFIG,			0x0010, 4);	/* 1000FC00 00280000 AAAAAAAA AAAAAAAA */
DEF_IE(100_IE_QUEUE_THRESH,		0x0011, 12);	/* 1100FC00 AAAAAAAA 00000000 00000000 */
DEF_IE(100_IE_BSS_POWER_SAVE,		0x0012, 1);	/* 1200FC00 00AAAAAA AAAAAAAA AAAAAAAA */
/* read only, variable len */
DEF_IE(100_IE_FIRMWARE_STATISTICS,	0x0013, 256); /* 0000AC00 00000000 ... */
DEF_IE(100_IE_INT_CONFIG,		0x0014, 20);	/* 00000000 00000000 00000000 00000000 5D74D105 00000000 AAAAAAAA AAAAAAAA */
DEF_IE(100_IE_FEATURE_CONFIG,		0x0015, 8);	/* 1500FC00 16000000 AAAAAAAA AAAAAAAA */
/* returns 'invalid MAC': */
DEF_IE(100_IE_KEY_CHOOSE,		0x0016, -4);
DEF_IE(100_IE_INVAL_17,			0x0017, -1);
DEF_IE(100_IE_UNKNOWN_18,		0x0018, 0);	/* null len?! 1800FC00 AAAAAAAA AAAAAAAA AAAAAAAA */
DEF_IE(100_IE_UNKNOWN_19,		0x0019, 256);	/* huge: 1900FC00 9C1F00EA FEFFFFEA FEFFFFEA... */
DEF_IE(100_IE_INVAL_1A,			0x001A, -1);

DEF_IE(100_IE_DOT11_INVAL_1000,			0x1000, -1);
DEF_IE(100_IE_DOT11_STATION_ID,			0x1001, 6);	/* huge: 0110FC00 58B10E2F 03000000 00000000... */
DEF_IE(100_IE_DOT11_INVAL_1002,			0x1002, -1);
DEF_IE(100_IE_DOT11_INVAL_1003,			0x1003, -1);
DEF_IE(100_IE_DOT11_INVAL_1004,			0x1004, -1);
DEF_IE(100_IE_DOT11_SHORT_RETRY_LIMIT,		0x1005, 1);
DEF_IE(100_IE_DOT11_LONG_RETRY_LIMIT,		0x1006, 1);
/* write only: */
DEF_IE(100_IE_DOT11_WEP_DEFAULT_KEY_WRITE,	0x1007, 32);
DEF_IE(100_IE_DOT11_MAX_XMIT_MSDU_LIFETIME,	0x1008, 4);	/* huge: 0810FC00 00020000 F4010000 00000000... */
/* undoc but returns something */
DEF_IE(100_IE_DOT11_GROUP_ADDR,			0x1009, 12);	/* huge: 0910FC00 00000000 00000000 00000000... */
DEF_IE(100_IE_DOT11_CURRENT_REG_DOMAIN,		0x100a, 1);	/* 0A10FC00 30AAAAAA AAAAAAAA AAAAAAAA */
DEF_IE(100_IE_DOT11_CURRENT_ANTENNA,		0x100b, 1);	/* 0B10FC00 8FAAAAAA AAAAAAAA AAAAAAAA */
DEF_IE(100_IE_DOT11_INVAL_100C,			0x100c, -1);
DEF_IE(100_IE_DOT11_TX_POWER_LEVEL,		0x100d, 2);	/* 00000000 0100AAAA AAAAAAAA AAAAAAAA */
DEF_IE(100_IE_DOT11_CURRENT_CCA_MODE,		0x100e, 1);	/* 0E10FC00 0DAAAAAA AAAAAAAA AAAAAAAA */
DEF_IE(100_IE_DOT11_ED_THRESHOLD,		0x100f, 4);	/* 0F10FC00 70000000 AAAAAAAA AAAAAAAA */
/* set default key ID  */
DEF_IE(100_IE_DOT11_WEP_DEFAULT_KEY_SET,	0x1010, 1);	/* 1010FC00 00AAAAAA AAAAAAAA AAAAAAAA */
DEF_IE(100_IE_DOT11_INVAL_1011,			0x1011, -1);
DEF_IE(100_IE_DOT11_INVAL_1012,			0x1012, -1);
DEF_IE(100_IE_DOT11_INVAL_1013,			0x1013, -1);
DEF_IE(100_IE_DOT11_UNKNOWN_1014,		0x1014, 256);	/* huge */
DEF_IE(100_IE_DOT11_UNKNOWN_1015,		0x1015, 256);	/* huge */
DEF_IE(100_IE_DOT11_UNKNOWN_1016,		0x1016, 256);	/* huge */
DEF_IE(100_IE_DOT11_UNKNOWN_1017,		0x1017, 256);	/* huge */
DEF_IE(100_IE_DOT11_UNKNOWN_1018,		0x1018, 256);	/* huge */
DEF_IE(100_IE_DOT11_UNKNOWN_1019,		0x1019, 256);	/* huge */

/* Experimentally obtained on PCI acx111 Xterasys XN-2522g, fw 1.2.1.34
** -1 means that fw returned 'invalid IE'
** 0400 0800 nnnn... are test read contents: u16 type, u16 len, data
** (AA are poison bytes marking bytes not written by fw)
**
** Looks like acx111 fw reports real len!
*/
DEF_IE(111_IE_INVAL_00,			0x0000, -1);
DEF_IE(111_IE_INVAL_01,			0x0001, -1);
DEF_IE(111_IE_POWER_MGMT,		0x0002, 12);
/* write only, variable len: 12 + rxqueue_cnt*8 + txqueue_cnt*4: */
DEF_IE(111_IE_MEMORY_CONFIG,		0x0003, 24);
DEF_IE(111_IE_BLOCK_SIZE,		0x0004, 8); /* 04000800 AA00AAAA AAAAAAAA */
/* variable len: 8 + rxqueue_cnt*8 + txqueue_cnt*8: */
DEF_IE(111_IE_QUEUE_HEAD,		0x0005, 24);
DEF_IE(111_IE_RATE_FALLBACK,		0x0006, 1);
/* acx100 name:WEP_OPTIONS */
/* said to have len:1 (not true, actually returns 12 bytes): */
DEF_IE(111_IE_RADIO_BAND,		0x0007, 12); /* 07000C00 AAAA1F00 FF03AAAA AAAAAAAA */
DEF_IE(111_IE_MEMORY_MAP,		0x0008, 48);
/* said to have len:4, but gives INVAL on read: */
DEF_IE(111_IE_SCAN_STATUS,		0x0009, -1);
DEF_IE(111_IE_ASSOC_ID,			0x000a, 2);
/* write only, len is not known: */
DEF_IE(111_IE_UNKNOWN_0B,		0x000b, 0);
/* read only, variable len. I see 67 byte reads: */
DEF_IE(111_IE_CONFIG_OPTIONS,		0x000c, 67); /* 0C004300 01160500 ... */
DEF_IE(111_IE_FWREV,			0x000d, 24);
DEF_IE(111_IE_FCS_ERROR_COUNT,		0x000e, 4);
DEF_IE(111_IE_MEDIUM_USAGE,		0x000f, 8);
DEF_IE(111_IE_RXCONFIG,			0x0010, 4);
DEF_IE(111_IE_QUEUE_THRESH,		0x0011, 12);
DEF_IE(111_IE_BSS_POWER_SAVE,		0x0012, 1);
/* read only, variable len. I see 240 byte reads: */
DEF_IE(111_IE_FIRMWARE_STATISTICS,	0x0013, 240); /* 1300F000 00000000 ... */
/* said to have len=17. looks like fw pads it to 20: */
DEF_IE(111_IE_INT_CONFIG,		0x0014, 20); /* 14001400 00000000 00000000 00000000 00000000 00000000 */
DEF_IE(111_IE_FEATURE_CONFIG,		0x0015, 8);
/* said to be name:KEY_INDICATOR, len:4, but gives INVAL on read: */
DEF_IE(111_IE_KEY_CHOOSE,		0x0016, -1);
/* said to have len:4, but in fact returns 8: */
DEF_IE(111_IE_MAX_USB_XFR,		0x0017, 8); /* 17000800 00014000 00000000 */
DEF_IE(111_IE_INVAL_18,			0x0018, -1);
DEF_IE(111_IE_INVAL_19,			0x0019, -1);
/* undoc but returns something: */
/* huh, fw indicates len=20 but uses 4 more bytes in buffer??? */
DEF_IE(111_IE_UNKNOWN_1A,		0x001A, 20); /* 1A001400 AA00AAAA 0000020F FF030000 00020000 00000007 04000000 */

DEF_IE(111_IE_DOT11_INVAL_1000,			0x1000, -1);
DEF_IE(111_IE_DOT11_STATION_ID,			0x1001, 6);
DEF_IE(111_IE_DOT11_FRAG_THRESH,		0x1002, 2);
/* acx100 only? gives INVAL on read: */
DEF_IE(111_IE_DOT11_BEACON_PERIOD,		0x1003, -1);
/* said to be MAX_RECV_MSDU_LIFETIME: */
DEF_IE(111_IE_DOT11_DTIM_PERIOD,		0x1004, 4);
DEF_IE(111_IE_DOT11_SHORT_RETRY_LIMIT,		0x1005, 1);
DEF_IE(111_IE_DOT11_LONG_RETRY_LIMIT,		0x1006, 1);
/* acx100 only? gives INVAL on read: */
DEF_IE(111_IE_DOT11_WEP_DEFAULT_KEY_WRITE,	0x1007, -1);
DEF_IE(111_IE_DOT11_MAX_XMIT_MSDU_LIFETIME,	0x1008, 4);
/* undoc but returns something. maybe it's 2 multicast MACs to listen to? */
DEF_IE(111_IE_DOT11_GROUP_ADDR,			0x1009, 12); /* 09100C00 00000000 00000000 00000000 */
DEF_IE(111_IE_DOT11_CURRENT_REG_DOMAIN,		0x100a, 1);
DEF_IE(111_IE_DOT11_CURRENT_ANTENNA,		0x100b, 2);
DEF_IE(111_IE_DOT11_INVAL_100C,			0x100c, -1);
DEF_IE(111_IE_DOT11_TX_POWER_LEVEL,		0x100d, 1);
/* said to have len=1 but gives INVAL on read: */
DEF_IE(111_IE_DOT11_CURRENT_CCA_MODE,		0x100e, -1);
/* said to have len=4 but gives INVAL on read: */
DEF_IE(111_IE_DOT11_ED_THRESHOLD,		0x100f, -1);
/* set default key ID. write only: */
DEF_IE(111_IE_DOT11_WEP_DEFAULT_KEY_SET,	0x1010, 1);
/* undoc but returns something: */
DEF_IE(111_IE_DOT11_UNKNOWN_1011,		0x1011, 1); /* 11100100 20 */
DEF_IE(111_IE_DOT11_INVAL_1012,			0x1012, -1);
DEF_IE(111_IE_DOT11_INVAL_1013,			0x1013, -1);
#endif



/***********************************************************************
** BOM Information Frames Structures
*/

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


/***********************************************************************
** BOM rxbuffer_t
**
** This is the format of rx data returned by acx
*/

/* I've hoped it's a 802.11 PHY header, but no...
 * so far, I've seen on acx111:
 * 0000 3a00 0000 0000 IBSS Beacons
 * 0000 3c00 0000 0000 ESS Beacons
 * 0000 2700 0000 0000 Probe requests
 * --vda
 */
typedef struct phy_hdr {
	u8	unknown[4];
	u8	acx111_unknown[4];
} ACX_PACKED phy_hdr_t;

/* seems to be a bit similar to hfa384x_rx_frame.
 * These fields are still not quite obvious, though.
 * Some seem to have different meanings... */

#define RXBUF_HDRSIZE 12
#define RXBUF_BYTES_RCVD(adev, rxbuf) \
		((le16_to_cpu((rxbuf)->mac_cnt_rcvd) & 0xfff) - (adev)->phy_header_len)
#define RXBUF_BYTES_USED(rxbuf) \
		((le16_to_cpu((rxbuf)->mac_cnt_rcvd) & 0xfff) + RXBUF_HDRSIZE)
/* USBism */
#define RXBUF_IS_TXSTAT(rxbuf) (le16_to_cpu((rxbuf)->mac_cnt_rcvd) & 0x8000)
/*
mac_cnt_rcvd:
    12 bits: length of frame from control field to first byte of FCS
    3 bits: reserved
    1 bit: 1 = it's a tx status info, not a rx packet (USB only)

mac_cnt_mblks:
    6 bits: number of memory block used to store frame in adapter memory
    1 bit: Traffic Indicator bit in TIM of received Beacon was set

mac_status: 1 byte (bitmap):
    7 Matching BSSID
    6 Matching SSID
    5 BDCST	Address 1 field is a broadcast
    4 VBM	received beacon frame has more than one set bit (?!)
    3 TIM Set	bit representing this station is set in TIM of received beacon
    2 GROUP	Address 1 is a multicast
    1 ADDR1	Address 1 matches our MAC
    0 FCSGD	FSC is good

phy_stat_baseband: 1 byte (bitmap):
    7 Preamble		frame had a long preamble
    6 PLCP Error	CRC16 error in PLCP header
    5 Unsup_Mod		unsupported modulation
    4 Selected Antenna	antenna 1 was used to receive this frame
    3 PBCC/CCK		frame used: 1=PBCC, 0=CCK modulation
    2 OFDM		frame used OFDM modulation
    1 TI Protection	protection frame was detected
    0 Reserved

phy_plcp_signal: 1 byte:
    Receive PLCP Signal field from the Baseband Processor

phy_level: 1 byte:
    receive AGC gain level (can be used to measure receive signal strength)

phy_snr: 1 byte:
    estimated noise power of equalized receive signal
    at input of FEC decoder (can be used to measure receive signal quality)

time: 4 bytes:
    timestamp sampled from either the Access Manager TSF counter
    or free-running microsecond counter when the MAC receives
    first byte of PLCP header.
*/

typedef struct rxbuffer {
	u16	mac_cnt_rcvd;		/* only 12 bits are len! (0xfff) */
	u8	mac_cnt_mblks;
	u8	mac_status;
	u8	phy_stat_baseband;	/* bit 0x80: used LNA (Low-Noise Amplifier) */
	u8	phy_plcp_signal;
	u8	phy_level;		/* PHY stat */
	u8	phy_snr;		/* PHY stat */
	u32	time;			/* timestamp upon MAC rcv first byte */

	/* 4-byte (acx100) or 8-byte (acx111) phy header will be here
	 * if RX_CFG1_INCLUDE_PHY_HDR is in effect:
	 */
	/* phy_hdr_t	phy; */

	struct ieee80211_hdr hdr_a3;
	/* maximally sized data part of wlan packet */
	/* OW 20100513 u8	data_a3[30 + 2312 + 4 - 24];
	// WLAN_A4FR_MAXLEN_WEP_FCS - WLAN_HDR_A3_LEN]
	*/
	u8	data_a3[WLAN_A4FR_MAXLEN_WEP_FCS - WLAN_HDR_A3_LEN];
	/* can add hdr/data_a4 if needed */
} ACX_PACKED rxbuffer_t;


/*--- Firmware statistics ----------------------------------------------------*/

/* Define a random 100 bytes more to catch firmware versions which
 * provide a bigger struct */
#define FW_STATS_FUTURE_EXTENSION	100

typedef struct firmware_image {
	u32	chksum;
	u32	size;
	u8	data[1]; /* the byte array of the actual firmware... */
} ACX_PACKED firmware_image_t;

typedef struct fw_stats_tx {
	u32	tx_desc_of;
} ACX_PACKED fw_stats_tx_t;

typedef struct fw_stats_rx {
	u32	rx_oom;
	u32	rx_hdr_of;
	u32	rx_hw_stuck; /* old: u32	rx_hdr_use_next */
	u32	rx_dropped_frame;
	u32	rx_frame_ptr_err;
	u32	rx_xfr_hint_trig;
	u32	rx_aci_events; /* later versions only */
	u32	rx_aci_resets; /* later versions only */
} ACX_PACKED fw_stats_rx_t;

typedef struct fw_stats_dma {
	u32	rx_dma_req;
	u32	rx_dma_err;
	u32	tx_dma_req;
	u32	tx_dma_err;
} ACX_PACKED fw_stats_dma_t;

typedef struct fw_stats_irq {
	u32	cmd_cplt;
	u32	fiq;
	u32	rx_hdrs;
	u32	rx_cmplt;
	u32	rx_mem_of;
	u32	rx_rdys;
	u32	irqs;
	u32	tx_procs;
	u32	decrypt_done;
	u32	dma_0_done;
	u32	dma_1_done;
	u32	tx_exch_complet;
	u32	commands;
	u32	rx_procs;
	u32	hw_pm_mode_changes;
	u32	host_acks;
	u32	pci_pm;
	u32	acm_wakeups;
} ACX_PACKED fw_stats_irq_t;

typedef struct fw_stats_wep {
	u32	wep_key_count;
	u32	wep_default_key_count;
	u32	dot11_def_key_mib;
	u32	wep_key_not_found;
	u32	wep_decrypt_fail;
	u32	wep_pkt_decrypt;
	u32	wep_decrypt_irqs;
} ACX_PACKED fw_stats_wep_t;

typedef struct fw_stats_pwr {
	u32	tx_start_ctr;
	u32	no_ps_tx_too_short;
	u32	rx_start_ctr;
	u32	no_ps_rx_too_short;
	u32	lppd_started;
	u32	no_lppd_too_noisy;
	u32	no_lppd_too_short;
	u32	no_lppd_matching_frame;
} ACX_PACKED fw_stats_pwr_t;

typedef struct fw_stats_mic {
	u32	mic_rx_pkts;
	u32	mic_calc_fail;
} ACX_PACKED fw_stats_mic_t;

typedef struct fw_stats_aes {
	u32	aes_enc_fail;
	u32	aes_dec_fail;
	u32	aes_enc_pkts;
	u32	aes_dec_pkts;
	u32	aes_enc_irq;
	u32	aes_dec_irq;
} ACX_PACKED fw_stats_aes_t;

typedef struct fw_stats_event {
	u32	heartbeat;
	u32	calibration;
	u32	rx_mismatch;
	u32	rx_mem_empty;
	u32	rx_pool;
	u32	oom_late;
	u32	phy_tx_err;
	u32	tx_stuck;
} ACX_PACKED fw_stats_event_t;

/* Mainly for size calculation only */
typedef struct fw_stats {
	u16			type;
	u16			len;
	fw_stats_tx_t		tx;
	fw_stats_rx_t		rx;
	fw_stats_dma_t		dma;
	fw_stats_irq_t		irq;
	fw_stats_wep_t		wep;
	fw_stats_pwr_t		pwr;
	fw_stats_mic_t		mic;
	fw_stats_aes_t		aes;
	fw_stats_event_t	evt;
	u8			_padding[FW_STATS_FUTURE_EXTENSION];
} fw_stats_t;

/* Firmware version struct */
typedef struct fw_ver {
	u16	cmd;
	u16	size;
	char	fw_id[20];
	u32	hw_id;
} ACX_PACKED fw_ver_t;

#define FW_ID_SIZE 20


/*--- WEP stuff --------------------------------------------------------------*/
#define DOT11_MAX_DEFAULT_WEP_KEYS	4

/* non-firmware struct, no packing necessary */
typedef struct wep_key {
	size_t	size; /* most often used member first */
	u8	index;
	u8	key[29];
	u16	strange_filler;
} wep_key_t;			/* size = 264 bytes (33*8) */
/* FIXME: We don't have size 264! Or is there 2 bytes beyond the key
 * (strange_filler)? */

/* non-firmware struct, no packing necessary */
typedef struct key_struct {
	u8	addr[ETH_ALEN];	/* 0x00 */
	u16	filler1;	/* 0x06 */
	u32	filler2;	/* 0x08 */
	u32	index;		/* 0x0c */
	u16	len;		/* 0x10 */
	u8	key[29];	/* 0x12; is this long enough??? */
} key_struct_t;			/* size = 276. FIXME: where is the remaining space?? */


/***********************************************************************
** BOM Hardware structures
*/

/* An opaque typesafe helper type
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
 * Attempts to use acx_ptr without macros result in compile-time errors */

typedef struct {
	u32	v;
} ACX_PACKED acx_ptr;

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


/***********************************************************************
** BOM PCI structures
*/

/* IRQ Constants
** (outside of "#ifdef PCI" because USB (mis)uses HOST_INT_SCAN_COMPLETE)
** descriptions taken from BSD driver */
#define HOST_INT_RX_DATA	0x0001	/* IN:  packet transferred from remote host to device */
#define HOST_INT_TX_COMPLETE	0x0002	/* OUT: packet transferred from device      to remote host */
#define HOST_INT_TX_XFER	0x0004	/* OUT: packet transferred from host        to device */
#define HOST_INT_RX_COMPLETE	0x0008	/* IN:  packet transferred from device      to host */
#define HOST_INT_DTIM		0x0010	/* not documented yet */
#define HOST_INT_BEACON		0x0020	/* not documented yet */
#define HOST_INT_TIMER		0x0040	/* not documented yet - in BSD driver: ACX_DEV_INTF_UNKNOWN1 */
#define HOST_INT_KEY_NOT_FOUND	0x0080	/* not documented yet - in BSD driver: ACX_DEV_INTF_UNKNOWN2 */
#define HOST_INT_IV_ICV_FAILURE	0x0100	/* not documented yet */
#define HOST_INT_CMD_COMPLETE	0x0200	/* not documented yet */
#define HOST_INT_INFO		0x0400	/* not documented yet */
#define HOST_INT_OVERFLOW	0x0800	/* not documented yet - in BSD driver: ACX_DEV_INTF_UNKNOWN3 */
#define HOST_INT_PROCESS_ERROR	0x1000	/* not documented yet - in BSD driver: ACX_DEV_INTF_UNKNOWN4 */
#define HOST_INT_SCAN_COMPLETE	0x2000	/* not documented yet */
#define HOST_INT_FCS_THRESHOLD	0x4000	/* not documented yet - in BSD driver: ACX_DEV_INTF_BOOT ??? */
#define HOST_INT_UNKNOWN	0x8000	/* not documented yet - in BSD driver: ACX_DEV_INTF_UNKNOWN5 */

#define HOST_INT_MASK_ALL	0xffff

/* Outside of "#ifdef PCI" because USB needs to know sizeof()
** of txdesc and rxdesc: */
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
		} ACX_PACKED r1;
		struct {
			u16	rate111;	/* 0x2a */
		} ACX_PACKED r2;
	} ACX_PACKED u;
	u32	queue_info;			/* 0x2c (acx100, reserved on acx111) */
} ACX_PACKED;		/* size : 48 = 0x30 */
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
} ACX_PACKED;		/* size 52 = 0x34 */


#if defined(CONFIG_ACX_MAC80211_PCI) || defined(CONFIG_ACX_MAC80211_MEM)

/* Register I/O offsets */
#define ACX100_EEPROM_ID_OFFSET	0x380

/* please add further ACX hardware register definitions only when
   it turns out you need them in the driver, and please try to use
   firmware functionality instead, since using direct I/O access instead
   of letting the firmware do it might confuse the firmware's state
   machine */

/* ***** ABSOLUTELY ALWAYS KEEP OFFSETS IN SYNC WITH THE INITIALIZATION
** OF THE I/O ARRAYS!!!! (grep for '^IO_ACX') ***** */

/*
 * NOTE about IO_ACX_IRQ_REASON: this register is CLEARED ON READ.
 */
enum {
	IO_ACX_SOFT_RESET = 0,

	IO_ACX_SLV_MEM_ADDR,
	IO_ACX_SLV_MEM_DATA,
	IO_ACX_SLV_MEM_CTL,
	IO_ACX_SLV_END_CTL,

	IO_ACX_FEMR,		/* Function Event Mask */

	IO_ACX_INT_TRIG,
	IO_ACX_IRQ_MASK,
	IO_ACX_IRQ_STATUS_NON_DES,
	IO_ACX_IRQ_REASON,
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
	IO_ACX_ECPU_CTRL
};
/* ***** ABSOLUTELY ALWAYS KEEP OFFSETS IN SYNC WITH THE INITIALIZATION
 * OF THE I/O ARRAYS!!!! (grep for '^IO_ACX') ***** */

/* Values for IO_ACX_INT_TRIG register: */
/* inform hw that rxdesc in queue needs processing */
#define INT_TRIG_RXPRC		0x08
/* inform hw that txdesc in queue needs processing */
#define INT_TRIG_TXPRC		0x04
/* ack that we received info from info mailbox */
#define INT_TRIG_INFOACK	0x02
/* inform hw that we have filled command mailbox */
#define INT_TRIG_CMD		0x01

struct hostdesc {
	acx_ptr	data_phy;			/* 0x00 [u8 *] */
	u16	data_offset;			/* 0x04 */
	u16	reserved;			/* 0x06 */
	u16	Ctl_16;	/* 16bit value, endianness!! */
	u16	length;			/* 0x0a */
	acx_ptr	desc_phy_next;		/* 0x0c [txhostdesc *] */
	acx_ptr	pNext;			/* 0x10 [txhostdesc *] */
} ACX_PACKED;

struct txhostdesc {
	struct hostdesc hd;

/* From here on you can use this area as you want (variable length, too!) */
	u8	*data;

	/* OW ieee80211_tx_status not really required here
	 * struct ieee80211_tx_status txstatus;
	 */
	struct sk_buff *skb;

} ACX_PACKED;

struct rxhostdesc {
	struct hostdesc hd;
	u32	Status;			/* 0x14, unused on Tx */
	
/* From here on you can use this area as you want (variable length, too!) */
	rxbuffer_t *data;
} ACX_PACKED;

#endif /* ACX_PCI */


/***********************************************************************
 * BOM USB structures and constants
 */
#ifdef CONFIG_ACX_MAC80211_USB

/* Used for usb_txbuffer.desc field */
#define USB_TXBUF_TXDESC	0xA
/* Size of header (everything up to data[]) */
#define USB_TXBUF_HDRSIZE	14
typedef struct usb_txbuffer {
	u16	desc;
	u16	mpdu_len;
	u8	queue_index;
	u8	rate;
	u32	hostdata;
	u8	ctrl1;
	u8	ctrl2;
	u16	data_len;
	/* wlan packet content is placed here: */
	/* OW 20100513 u8	data[30 + 2312 + 4]; // WLAN_A4FR_MAXLEN_WEP_FCS] */
	u8	data[WLAN_A4FR_MAXLEN_WEP_FCS];
} ACX_PACKED usb_txbuffer_t;

/* USB returns either rx packets (see rxbuffer) or
 * these "tx status" structs: */
typedef struct usb_txstatus {
	u16	mac_cnt_rcvd;		/* only 12 bits are len! (0xfff) */
	u8	queue_index;
	u8	mac_status;		/* seen 0x20 on tx failure */
	u32	hostdata;
	u8	rate;
	u8	ack_failures;
	u8	rts_failures;
	u8	rts_ok;
} ACX_PACKED usb_txstatus_t;

typedef struct usb_tx {
	unsigned	busy:1;
	struct urb	*urb;
	acx_device_t	*adev;
	struct sk_buff *skb;
	/* actual USB bulk output data block is here: */
	usb_txbuffer_t	bulkout;
} usb_tx_t;

struct usb_rx_plain {
	unsigned	busy:1;
	struct urb	*urb;
	acx_device_t	*adev;
	rxbuffer_t	bulkin;
};

typedef struct usb_rx {
	unsigned	busy:1;
	struct urb	*urb;
	acx_device_t	*adev;
	rxbuffer_t	bulkin;
	/* Make entire structure 4k */
	u8 padding[4*1024 - sizeof(struct usb_rx_plain)];
} usb_rx_t;
#endif /* ACX_USB */


/* BOM Config Option structs */

typedef struct co_antennas {
	u8	type;
	u8	len;
	u8	list[2];
} ACX_PACKED co_antennas_t;

typedef struct co_powerlevels {
	u8	type;
	u8	len;
	u16	list[8];
} ACX_PACKED co_powerlevels_t;

typedef struct co_datarates {
	u8	type;
	u8	len;
	u8	list[8];
} ACX_PACKED co_datarates_t;

typedef struct co_domains {
	u8	type;
	u8	len;
	u8	list[6];
} ACX_PACKED co_domains_t;

typedef struct co_product_id {
	u8	type;
	u8	len;
	u8	list[128];
} ACX_PACKED co_product_id_t;

typedef struct co_manuf_id {
	u8	type;
	u8	len;
	u8	list[128];
} ACX_PACKED co_manuf_t;

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
} ACX_PACKED co_fixed_t;

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
} ACX_PACKED acx111_ie_configoption_t;

/* Misc TODO Move elsewhere */
typedef struct shared_queueindicator {
        u32     indicator;
        u16     host_lock;
        u16     fw_lock;
} ACX_PACKED queueindicator_t;


/* For use with ACX1xx_IE_RXCONFIG */
/*  bit     description
 *    13   include additional header (length etc.) *required*
 *		struct is defined in 'struct rxbuffer'
 *		is this bit acx100 only? does acx111 always put the header,
 *		and bit setting is irrelevant? --vda
 *    10   receive frames only with SSID used in last join cmd
 *     9   discard broadcast
 *     8   receive packets for multicast address 1
 *     7   receive packets for multicast address 0
 *     6   discard all multicast packets
 *     5   discard frames from foreign BSSID
 *     4   discard frames with foreign destination MAC address
 *     3   promiscuous mode (receive ALL frames, disable filter)
 *     2   include FCS
 *     1   include phy header
 *     0   ???
 */
#define RX_CFG1_INCLUDE_RXBUF_HDR	0x2000 /* ACX100 only */
#define RX_CFG1_FILTER_SSID		0x0400
#define RX_CFG1_FILTER_BCAST		0x0200
#define RX_CFG1_RCV_MC_ADDR1		0x0100
#define RX_CFG1_RCV_MC_ADDR0		0x0080
#define RX_CFG1_FILTER_ALL_MULTI	0x0040
#define RX_CFG1_FILTER_BSSID		0x0020
#define RX_CFG1_FILTER_MAC		0x0010
#define RX_CFG1_RCV_PROMISCUOUS		0x0008
#define RX_CFG1_INCLUDE_FCS		0x0004
#define RX_CFG1_INCLUDE_PHY_HDR		(WANT_PHY_HDR ? 0x0002 : 0)
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
 *     1   receive RTS/CTS/ACK frames
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

/* For use with ACX1xx_IE_FEATURE_CONFIG */
#define FEATURE1_80MHZ_CLOCK	0x00000040L
#define FEATURE1_4X		0x00000020L
#define FEATURE1_LOW_RX		0x00000008L
#define FEATURE1_EXTRA_LOW_RX	0x00000001L

#define FEATURE2_SNIFFER	0x00000080L
#define FEATURE2_NO_TXCRYPT	0x00000001L

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

/* keep in sync with the above */
#define GETSET_ALL	(0 \
/* GETSET_LED_POWER */	| 0x00000001L \
/* GETSET_STATION_ID */	| 0x00000002L \
/* SET_TEMPLATES */	| 0x00000004L \
/* SET_STA_LIST */	| 0x00000008L \
/* GETSET_TX */		| 0x00000010L \
/* GETSET_RX */		| 0x00000020L \
/* SET_RXCONFIG */	| 0x00000040L \
/* GETSET_ANTENNA */	| 0x00000080L \
/* GETSET_SENSITIVITY */| 0x00000100L \
/* GETSET_TXPOWER */	| 0x00000200L \
/* GETSET_ED_THRESH */	| 0x00000400L \
/* GETSET_CCA */	| 0x00000800L \
/* GETSET_POWER_80211 */| 0x00001000L \
/* GETSET_RETRY */	| 0x00002000L \
/* GETSET_REG_DOMAIN */	| 0x00004000L \
/* GETSET_CHANNEL */	| 0x00008000L \
/* GETSET_RESCAN */	| 0x00010000L \
/* GETSET_MODE */	| 0x00020000L \
/* GETSET_WEP */	| 0x00040000L \
/* SET_WEP_OPTIONS */	| 0x00080000L \
/* SET_MSDU_LIFETIME */	| 0x00100000L \
/* SET_RATE_FALLBACK */	| 0x00200000L \
			)

/***********************************************************************
*/
typedef struct acx100_ie_memblocksize {
	u16	type;
	u16	len;
	u16	size;
} ACX_PACKED acx100_ie_memblocksize_t;

typedef struct acx100_ie_queueconfig {
	u16	type;
	u16	len;
	u32	AreaSize;
	u32	RxQueueStart;
	u8	QueueOptions;
	u8	NumTxQueues;
	u8	NumRxDesc;	 /* for USB only */
	u8	pad1;
	u32	QueueEnd;
	u32	HostQueueEnd; /* QueueEnd2 */
	u32	TxQueueStart;
	u8	TxQueuePri;
	u8	NumTxDesc;
	u16	pad2;
} ACX_PACKED acx100_ie_queueconfig_t;

typedef struct acx111_ie_queueconfig {
	u16	type;
	u16	len;
	u32	tx_memory_block_address;
	u32	rx_memory_block_address;
	u32	rx1_queue_address;
	u32	reserved1;
	u32	tx1_queue_address;
	u8	tx1_attributes;
	u16	reserved2;
	u8	reserved3;
} ACX_PACKED acx111_ie_queueconfig_t;

typedef struct acx100_ie_memconfigoption {
	u16	type;
	u16	len;
	u32	DMA_config;
	acx_ptr	pRxHostDesc;
	u32	rx_mem;
	u32	tx_mem;
	u16	RxBlockNum;
	u16	TxBlockNum;
} ACX_PACKED acx100_ie_memconfigoption_t;

typedef struct acx111_ie_memoryconfig {
	u16	type;
	u16	len;
	u16	no_of_stations;
	u16	memory_block_size;
	u8	tx_rx_memory_block_allocation;
	u8	count_rx_queues;
	u8	count_tx_queues;
	u8	options;
	u8	fragmentation;
	u16	reserved1;
	u8	reserved2;

	/* start of rx1 block */
	u8	rx_queue1_count_descs;
	u8	rx_queue1_reserved1;
	u8	rx_queue1_type; /* must be set to 7 */
	u8	rx_queue1_prio; /* must be set to 0 */
	acx_ptr	rx_queue1_host_rx_start;
	/* end of rx1 block */

	/* start of tx1 block */
	u8	tx_queue1_count_descs;
	u8	tx_queue1_reserved1;
	u8	tx_queue1_reserved2;
	u8	tx_queue1_attributes;
	/* end of tx1 block */
} ACX_PACKED acx111_ie_memoryconfig_t;

typedef struct acx_ie_memmap {
	u16	type;
	u16	len;
	u32	CodeStart;
	u32	CodeEnd;
	u32	WEPCacheStart;
	u32	WEPCacheEnd;
	u32	PacketTemplateStart;
	u32	PacketTemplateEnd;
	u32	QueueStart;
	u32	QueueEnd;
	u32	PoolStart;
	u32	PoolEnd;
} ACX_PACKED acx_ie_memmap_t;

typedef struct acx111_ie_feature_config {
	u16	type;
	u16	len;
	u32	feature_options;
	u32	data_flow_options;
} ACX_PACKED acx111_ie_feature_config_t;

typedef struct acx1xx_ie_tx_level {
	u16	type;
	u16	len;
	u8	level;
} ACX_PACKED acx1xx_ie_tx_level_t;

#define TX_CFG_ACX100_NUM_POWER_LEVELS 2
#define TX_CFG_ACX111_NUM_POWER_LEVELS 5

#define PS_CFG_ENABLE		0x80
#define PS_CFG_PENDING		0x40 /* status flag when entering PS */
#define PS_CFG_WAKEUP_MODE_MASK	0x07
#define PS_CFG_WAKEUP_BY_HOST	0x03
#define PS_CFG_WAKEUP_EACH_ITVL	0x02
#define PS_CFG_WAKEUP_ON_DTIM	0x01
#define PS_CFG_WAKEUP_ALL_BEAC	0x00

/* Enhanced PS mode: sleep until Rx Beacon w/ the STA's AID bit set
** in the TIM; newer firmwares only(?) */
#define PS_OPT_ENA_ENHANCED_PS	0x04
#define PS_OPT_TX_PSPOLL	0x02 /* send PSPoll frame to fetch waiting frames from AP (on frame with matching AID) */
#define PS_OPT_STILL_RCV_BCASTS	0x01

typedef struct acx100_ie_powersave {
	u16	type;
	u16	len;
	u8	wakeup_cfg;
	u8	listen_interval; /* for EACH_ITVL: wake up every "beacon units" interval */
	u8	options;
	u8	hangover_period; /* remaining wake time after Tx MPDU w/ PS bit, in values of 1/1024 seconds */
	u16	enhanced_ps_transition_time; /* rem. wake time for Enh. PS */
} ACX_PACKED acx100_ie_powersave_t;

typedef struct acx111_ie_powersave {
	u16	type;
	u16	len;
	u8	wakeup_cfg;
	u8	listen_interval; /* for EACH_ITVL: wake up every "beacon units" interval */
	u8	options;
	u8	hangover_period; /* remaining wake time after Tx MPDU w/ PS bit, in values of 1/1024 seconds */
	u32	beacon_rx_time;
	u32	enhanced_ps_transition_time; /* rem. wake time for Enh. PS */
} ACX_PACKED acx111_ie_powersave_t;


/***********************************************************************
** BOM Commands and template structures
*/

/*
** SCAN command structure
**
** even though acx100 scan rates match RATE100 constants,
** acx111 ones do not match! Therefore we do not use RATE100 #defines */
#define ACX_SCAN_RATE_1		10
#define ACX_SCAN_RATE_2		20
#define ACX_SCAN_RATE_5		55
#define ACX_SCAN_RATE_11	110
#define ACX_SCAN_RATE_22	220
#define ACX_SCAN_RATE_PBCC	0x80	/* OR with this if needed */
#define ACX_SCAN_OPT_ACTIVE	0x00	/* a bit mask */
#define ACX_SCAN_OPT_PASSIVE	0x01
/* Background scan: we go into Power Save mode (by transmitting
** NULL data frame to AP with the power mgmt bit set), do the scan,
** and then exit Power Save mode. A plus is that AP buffers frames
** for us while we do background scan. Thus we avoid frame losses.
** Background scan can be active or passive, just like normal one */
#define ACX_SCAN_OPT_BACKGROUND	0x02
typedef struct acx100_scan {
	u16	count;	/* number of scans to do, 0xffff == continuous */
	u16	start_chan;
	u16	flags;	/* channel list mask; 0x8000 == all channels? */
	u8	max_rate;	/* max. probe rate */
	u8	options;	/* bit mask, see defines above */
	u16	chan_duration;
	u16	max_probe_delay;
} ACX_PACKED acx100_scan_t;			/* length 0xc */

#define ACX111_SCAN_RATE_6	0x0B
#define ACX111_SCAN_RATE_9	0x0F
#define ACX111_SCAN_RATE_12	0x0A
#define ACX111_SCAN_RATE_18	0x0E
#define ACX111_SCAN_RATE_24	0x09
#define ACX111_SCAN_RATE_36	0x0D
#define ACX111_SCAN_RATE_48	0x08
#define ACX111_SCAN_RATE_54	0x0C
#define ACX111_SCAN_OPT_5GHZ    0x04	/* else 2.4GHZ */
#define ACX111_SCAN_MOD_SHORTPRE 0x01	/* you can combine SHORTPRE and PBCC */
#define ACX111_SCAN_MOD_PBCC	0x80
#define ACX111_SCAN_MOD_OFDM	0x40
typedef struct acx111_scan {
	u16	count;		/* number of scans to do */
	u8	channel_list_select; /* 0: scan all channels, 1: from chan_list only */
	u16	reserved1;
	u8	reserved2;
	u8	rate;		/* rate for probe requests (if active scan) */
	u8	options;		/* bit mask, see defines above */
	u16	chan_duration;	/* min time to wait for reply on one channel (in TU) */
						/* (active scan only) (802.11 section 11.1.3.2.2) */
	u16	max_probe_delay;	/* max time to wait for reply on one channel (active scan) */
						/* time to listen on a channel (passive scan) */
	u8	modulation;
	u8	channel_list[26];	/* bits 7:0 first byte: channels 8:1 */
						/* bits 7:0 second byte: channels 16:9 */
						/* 26 bytes is enough to cover 802.11a */
} ACX_PACKED acx111_scan_t;

/*
** Radio calibration command structure
*/
typedef struct acx111_cmd_radiocalib {
/* 0x80000000 == automatic calibration by firmware, according to interval;
 * bits 0..3: select calibration methods to go through:
 * calib based on DC, AfeDC, Tx mismatch, Tx equilization */
	u32	methods;
	u32	interval;
} ACX_PACKED acx111_cmd_radiocalib_t;

/*
** Packet template structures
**
** Packet templates store contents of Beacon, Probe response, Probe request,
** Null data frame, and TIM data frame. Firmware automatically transmits
** contents of template at appropriate time:
** - Beacon: when configured as AP or Ad-hoc
** - Probe response: when configured as AP or Ad-hoc, whenever
**   a Probe request frame is received
** - Probe request: when host issues SCAN command (active)
** - Null data frame: when entering 802.11 power save mode
** - TIM data: at the end of Beacon frames (if no TIM template
**   is configured, then transmits default TIM)
** NB:
** - size field must be set to size of actual template
**   (NOT sizeof(struct) - templates are variable in length),
**   size field is not itself counted.
** - members flagged with an asterisk must be initialized with host,
**   rest must be zero filled.
** - variable length fields shown only in comments */
typedef struct acx_template_tim {
	u16	size;
	u8	tim_eid;	/* 00 1 TIM IE ID * */
	u8	len;		/* 01 1 Length * */
	u8	dtim_cnt;	/* 02 1 DTIM Count */
	u8	dtim_period;	/* 03 1 DTIM Period */
	u8	bitmap_ctrl;	/* 04 1 Bitmap Control * (except bit0) */
					/* 05 n Partial Virtual Bitmap * */
	u8	variable[0x100 - 1-1-1-1-1];
} ACX_PACKED acx_template_tim_t;

typedef struct acx_template_probereq {
	u16	size;
	u16	fc;		/* 00 2 fc * */
	u16	dur;		/* 02 2 Duration */
	u8	da[6];  	/* 04 6 Destination Address * */
	u8	sa[6];   	/* 0A 6 Source Address * */
	u8	bssid[6];	/* 10 6 BSSID * */
	u16	seq;		/* 16 2 Sequence Control */
				/* 18 n SSID * */
				/* nn n Supported Rates * */
	u8	variable[0x44 - 2-2-6-6-6-2];
} ACX_PACKED acx_template_probereq_t;

typedef struct acx_template_proberesp {
	u16	size;
	u16	fc;		/* 00 2 fc * (bits [15:12] and [10:8] per 802.11 section 7.1.3.1) */
	u16	dur;		/* 02 2 Duration */
	u8	da[6];	/* 04 6 Destination Address */
	u8	sa[6];	/* 0A 6 Source Address */
	u8	bssid[6];	/* 10 6 BSSID */
	u16	seq;		/* 16 2 Sequence Control */
	u8	timestamp[8];/* 18 8 Timestamp */
	u16	beacon_interval; /* 20 2 Beacon Interval * */
	u16	cap;		/* 22 2 Capability Information * */
					/* 24 n SSID * */
					/* nn n Supported Rates * */
					/* nn 1 DS Parameter Set * */
/* OW 20100514	u8	variable[0x54 - 2-2-6-6-6-2-8-2-2]; */
	u8	variable[0x154 - 2-2-6-6-6-2-8-2-2];
} ACX_PACKED acx_template_proberesp_t;
#define acx_template_beacon_t acx_template_proberesp_t
#define acx_template_beacon acx_template_proberesp

typedef struct acx_template_nullframe {
	u16	size;
	struct ieee80211_hdr hdr;
	/* OW, 20080210 code: 	struct wlan_hdr_a3 hdr;
	 * maybe better user: ieee80211_hdr_3addr
	 */
} ACX_PACKED acx_template_nullframe_t;


/*
** JOIN command structure
**
** as opposed to acx100, acx111 dtim interval is AFTER rates_basic111.
** NOTE: took me about an hour to get !@#$%^& packing right --> struct packing is eeeeevil... */

typedef struct acx_joinbss {
	u8	bssid[ETH_ALEN];
	u16	beacon_interval;
	union {
		struct {
			u8	dtim_interval;
			u8	rates_basic;
			u8	rates_supported;
		} ACX_PACKED acx100;
		struct {
			u16	rates_basic;
			u8	dtim_interval;
		} ACX_PACKED acx111;
	} ACX_PACKED u;
	u8	genfrm_txrate;	/* generated frame (bcn, proberesp, RTS, PSpoll) tx rate */
	u8	genfrm_mod_pre;	/* generated frame modulation/preamble:
						** bit7: PBCC, bit6: OFDM (else CCK/DQPSK/DBPSK)
						** bit5: short pre */
	u8	macmode;	/* BSS Type, must be one of ACX_MODE_xxx */
	u8	channel;
	u8	essid_len;
	char	essid[IW_ESSID_MAX_SIZE];
} ACX_PACKED acx_joinbss_t;

#define JOINBSS_RATES_1		0x01
#define JOINBSS_RATES_2		0x02
#define JOINBSS_RATES_5		0x04
#define JOINBSS_RATES_11	0x08
#define JOINBSS_RATES_22	0x10

/* Looks like missing bits are used to indicate 11g rates!
** (it follows from the fact that constants below match 1:1 to RATE111_nn)
** This was actually seen! Look at that Assoc Request sent by acx111,
** it _does_ contain 11g rates in basic set:
01:30:20.070772 Beacon (xxx) [1.0* 2.0* 5.5* 11.0* 6.0* 9.0* 12.0* 18.0* 24.0* 36.0* 48.0* 54.0* Mbit] ESS CH: 1
01:30:20.074425 Authentication (Open System)-1: Succesful
01:30:20.076539 Authentication (Open System)-2:
01:30:20.076620 Acknowledgment
01:30:20.088546 Assoc Request (xxx) [1.0* 2.0* 5.5* 6.0* 9.0* 11.0* 12.0* 18.0* 24.0* 36.0* 48.0* 54.0* Mbit]
01:30:20.122413 Assoc Response AID(1) :: Succesful
01:30:20.122679 Acknowledgment
01:30:20.173204 Beacon (xxx) [1.0* 2.0* 5.5* 11.0* 6.0* 9.0* 12.0* 18.0* 24.0* 36.0* 48.0* 54.0* Mbit] ESS CH: 1
*/
#define JOINBSS_RATES_BASIC111_1	0x0001
#define JOINBSS_RATES_BASIC111_2	0x0002
#define JOINBSS_RATES_BASIC111_5	0x0004
#define JOINBSS_RATES_BASIC111_11	0x0020
#define JOINBSS_RATES_BASIC111_22	0x0100


/***********************************************************************
*/
typedef struct mem_read_write {
	u16	addr;
	u16	type; /* 0x0 int. RAM / 0xffff MAC reg. / 0x81 PHY RAM / 0x82 PHY reg.; or maybe it's actually 0x30 for MAC? Better verify it by writing and reading back and checking whether the value holds! */
	u32	len;
	u32	data;
} ACX_PACKED mem_read_write_t;


typedef struct acx_cmd_radioinit {
	u32	offset;
	u32	len;
} ACX_PACKED acx_cmd_radioinit_t;

typedef struct acx100_ie_wep_options {
	u16	type;
	u16	len;
	u16	NumKeys;	/* max # of keys */
	u8	WEPOption;	/* 0 == decrypt default key only, 1 == override decrypt */
	u8	Pad;		/* used only for acx111 */
} ACX_PACKED acx100_ie_wep_options_t;

typedef struct ie_dot11WEPDefaultKey {
	u16	type;
	u16	len;
	u8	action;
	u8	keySize;
	u8	defaultKeyNum;
	u8	key[29];	/* check this! was Key[19] */
} ACX_PACKED ie_dot11WEPDefaultKey_t;

typedef struct acx111WEPDefaultKey {
	u8	MacAddr[ETH_ALEN];
	u16	action;	/* NOTE: this is a u16, NOT a u8!! */
	u16	reserved;
	u8	keySize;
	u8	type;
	u8	index;
	u8	defaultKeyNum;
	u8	counter[6];
	u8	key[32];	/* up to 32 bytes (for TKIP!) */
} ACX_PACKED acx111WEPDefaultKey_t;

typedef struct ie_dot11WEPDefaultKeyID {
	u16	type;
	u16	len;
	u8	KeyID;
} ACX_PACKED ie_dot11WEPDefaultKeyID_t;

typedef struct acx100_cmd_wep_mgmt {
	u8	MacAddr[ETH_ALEN];
	u16	Action;
	u16	KeySize;
	u8	Key[29]; /* 29*8 == 232bits == WEP256 */
} ACX_PACKED acx100_cmd_wep_mgmt_t;

typedef struct acx_ie_generic {
	u16	type;
	u16	len;
	union {
		/* Association ID IE: just a 16bit value: */
		u16	aid;
		/* generic member for quick implementation of commands */
		u8	bytes[32];
	} ACX_PACKED m;
} ACX_PACKED acx_ie_generic_t;

/* OW TODO This could be cleanup actually.
 * Code for WEP key setting in HW should be taken from 20080210 version.
 */
#define ACX_SEC_KEYSIZE                     16
/* Security algorithms. */
enum {
        ACX_SEC_ALG,
        ACX_SEC_ALGO_NONE = 0, /* unencrypted, as of TX header. */
        ACX_SEC_ALGO_WEP,
        ACX_SEC_ALGO_UNKNOWN,
        ACX_SEC_ALGO_AES,
        ACX_SEC_ALGO_WEP104,
        ACX_SEC_ALGO_TKIP,
};

/***********************************************************************
*/
#define CHECK_SIZEOF(type,size) { \
	extern void BUG_bad_size_for_##type(void); \
	if (sizeof(type)!=(size)) BUG_bad_size_for_##type(); \
}

static inline void
acx_struct_size_check(void)
{
	CHECK_SIZEOF(txdesc_t, 0x30);
	CHECK_SIZEOF(acx100_ie_memconfigoption_t, 24);
	CHECK_SIZEOF(acx100_ie_queueconfig_t, 0x20);
	CHECK_SIZEOF(acx_joinbss_t, 0x30);
	/* IEs need 4 bytes for (type,len) tuple */
	CHECK_SIZEOF(acx111_ie_configoption_t, ACX111_IE_CONFIG_OPTIONS_LEN + 4);
}

#endif /* _ACX_STRUCT_H_ */
