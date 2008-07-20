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
 *
 * This header contains the definitions of the Information Elements (IE)
 */
#ifndef _ACX_IE_H_
#define _ACX_IE_H_

/***********************************************************************
** Interrogate/Configure cmd constants
**
** NB: length includes JUST the data part of the IE
** (does not include size of the (type,len) pair)
**
** TODO: seems that acx100, acx100usb, acx111 have some differences,
** fix code with regard to this!
*/

/* Information Elements: Network Parameters, Static Configuration Entities */
/* these are handled by real_cfgtable in firmware "Rev 1.5.0" (FW150) */
#define ACX1xx_IE_UNKNOWN_00			(0x0000)	/* mapped to cfgInvalid in FW150 */
#define ACX1xx_IE_UNKNOWN_00_LEN		(-1)
#define ACX100_IE_ACX_TIMER			(0x0001)
#define ACX100_IE_ACX_TIMER_LEN 		(0x10)
#define ACX1xx_IE_POWER_MGMT			(0x0002)	/* TNETW1450: length 0x18!! */
#define ACX1xx_IE_POWER_MGMT_LEN 		(0x06)
#define ACX1xx_IE_QUEUE_CONFIG			(0x0003)
#define ACX1xx_IE_QUEUE_CONFIG_LEN		(0x1c)
#define ACX100_IE_BLOCK_SIZE			(0x0004)
#define ACX100_IE_BLOCK_SIZE_LEN		(0x02)
#define ACX1FF_IE_SLOT_TIME			(0x0004)	/* later firmware versions only? */
#define ACX1FF_IE_SLOT_TIME_LEN			(0x08)
#define ACX1xx_IE_MEMORY_CONFIG_OPTIONS		(0x0005)
#define ACX1xx_IE_MEMORY_CONFIG_OPTIONS_LEN	(0x14)
#define ACX1FF_IE_QUEUE_HEAD			(0x0005)
#define ACX1FF_IE_QUEUE_HEAD_LEN		(0x14 /* FIXME: length? */)
#define ACX1xx_IE_RATE_FALLBACK			(0x0006)	/* TNETW1450: length 2 */
#define ACX1xx_IE_RATE_FALLBACK_LEN		(0x01)
#define ACX100_IE_WEP_OPTIONS			(0x0007)
#define ACX100_IE_WEP_OPTIONS_LEN		(0x03)
#define ACX111_IE_RADIO_BAND			(0x0007)
#define ACX111_IE_RADIO_BAND_LEN		(-1)
#define ACX1FF_IE_TIMING_CFG			(0x0007)	/* later firmware versions; TNETW1450 only? */
#define ACX1FF_IE_TIMING_CFG_LEN		(-1)
#define ACX100_IE_SSID				(0x0008)	/* huh? */
#define ACX100_IE_SSID_LEN			(0x20)
#define ACX1xx_IE_MEMORY_MAP			(0x0008)	/* huh? TNETW1450 has length 0x40!! */
#define ACX1xx_IE_MEMORY_MAP_LEN		(0x28)
#define ACX1xx_IE_SCAN_STATUS			(0x0009)	/* mapped to cfgInvalid in FW150 */
#define ACX1xx_IE_SCAN_STATUS_LEN		(0x04)
#define ACX1xx_IE_ASSOC_ID			(0x000a)
#define ACX1xx_IE_ASSOC_ID_LEN			(0x02)
#define ACX1xx_IE_UNKNOWN_0B			(0x000B)	/* mapped to cfgInvalid in FW150 */
#define ACX1xx_IE_UNKNOWN_0B_LEN		(-1)
#define ACX1FF_IE_TX_POWER_LEVEL_TABLE		(0x000B)	/* later firmware versions; TNETW1450 only? */
#define ACX1FF_IE_TX_POWER_LEVEL_TABLE_LEN	(0x18)
#define ACX100_IE_UNKNOWN_0C			(0x000C)	/* very small implementation in FW150! */
#define ACX100_IE_UNKNOWN_0C_LEN		(-1)
/* ACX100 has an equivalent struct in the cmd mailbox directly after reset.
 * 0x14c seems extremely large, will trash stack on failure (memset!)
 * in case of small input struct --> OOPS! */
#define ACX111_IE_CONFIG_OPTIONS		(0x000c)
#define ACX111_IE_CONFIG_OPTIONS_LEN		(0x14c)
#define ACX1xx_IE_FWREV				(0x000d)
#define ACX1xx_IE_FWREV_LEN			(0x18)
#define ACX1xx_IE_FCS_ERROR_COUNT		(0x000e)
#define ACX1xx_IE_FCS_ERROR_COUNT_LEN		(0x04)
#define ACX1xx_IE_MEDIUM_USAGE			(0x000f)	/* AKA MEDIUM OCCUPANCY */
#define ACX1xx_IE_MEDIUM_USAGE_LEN		(0x08)
#define ACX1xx_IE_RXCONFIG			(0x0010)
#define ACX1xx_IE_RXCONFIG_LEN			(0x04)
#define ACX100_IE_UNKNOWN_11			(0x0011)	/* NONBINARY: large implementation in FW150! link quality readings or so? */
#define ACX100_IE_UNKNOWN_11_LEN		(-1)
#define ACX111_IE_QUEUE_THRESH			(0x0011)
#define ACX111_IE_QUEUE_THRESH_LEN		(-1)
#define ACX100_IE_UNKNOWN_12			(0x0012)	/* NONBINARY: VERY large implementation in FW150!! */
#define ACX100_IE_UNKNOWN_12_LEN		(-1)
#define ACX111_IE_BSS_POWER_SAVE		(0x0012)
#define ACX111_IE_BSS_POWER_SAVE_LEN		(/* -1 */ 2)
#define ACX1xx_IE_FIRMWARE_STATISTICS		(0x0013)	/* TNETW1450: length 0x134!! */
#define ACX1xx_IE_FIRMWARE_STATISTICS_LEN	(0x9c)
#define ACX1FF_IE_RX_INTR_CONFIG		(0x0014)	/* later firmware versions, TNETW1450 only? */
#define ACX1FF_IE_RX_INTR_CONFIG_LEN		(0x14)
#define ACX1xx_IE_FEATURE_CONFIG		(0x0015)
#define ACX1xx_IE_FEATURE_CONFIG_LEN		(0x08)
#define ACX111_IE_KEY_CHOOSE			(0x0016)	/* for rekeying. really len=4?? */
#define ACX111_IE_KEY_CHOOSE_LEN		(0x04)
#define ACX1FF_IE_MISC_CONFIG_TABLE		(0x0017)	/* later firmware versions, TNETW1450 only? */
#define ACX1FF_IE_MISC_CONFIG_TABLE_LEN		(0x04)
#define ACX1FF_IE_WONE_CONFIG			(0x0018)	/* later firmware versions, TNETW1450 only? */
#define ACX1FF_IE_WONE_CONFIG_LEN		(-1)
#define ACX1FF_IE_TID_CONFIG			(0x001a)	/* later firmware versions, TNETW1450 only? */
#define ACX1FF_IE_TID_CONFIG_LEN		(0x2c)
#define ACX1FF_IE_CALIB_ASSESSMENT		(0x001e)	/* later firmware versions, TNETW1450 only? */
#define ACX1FF_IE_CALIB_ASSESSMENT_LEN		(0x04)
#define ACX1FF_IE_BEACON_FILTER_OPTIONS		(0x001f)	/* later firmware versions, TNETW1450 only? */
#define ACX1FF_IE_BEACON_FILTER_OPTIONS_LEN	(0x02)
#define ACX1FF_IE_LOW_RSSI_THRESH_OPT		(0x0020)	/* later firmware versions, TNETW1450 only? */
#define ACX1FF_IE_LOW_RSSI_THRESH_OPT_LEN	(0x04)
#define ACX1FF_IE_NOISE_HISTOGRAM_RESULTS	(0x0021)	/* later firmware versions, TNETW1450 only? */
#define ACX1FF_IE_NOISE_HISTOGRAM_RESULTS_LEN	(0x30)
#define ACX1FF_IE_PACKET_DETECT_THRESH		(0x0023)	/* later firmware versions, TNETW1450 only? */
#define ACX1FF_IE_PACKET_DETECT_THRESH_LEN	(0x04)
#define ACX1FF_IE_TX_CONFIG_OPTIONS		(0x0024)	/* later firmware versions, TNETW1450 only? */
#define ACX1FF_IE_TX_CONFIG_OPTIONS_LEN		(0x04)
#define ACX1FF_IE_CCA_THRESHOLD			(0x0025)	/* later firmware versions, TNETW1450 only? */
#define ACX1FF_IE_CCA_THRESHOLD_LEN		(0x02)
#define ACX1FF_IE_EVENT_MASK			(0x0026)	/* later firmware versions, TNETW1450 only? */
#define ACX1FF_IE_EVENT_MASK_LEN		(0x08)
#define ACX1FF_IE_DTIM_PERIOD			(0x0027)	/* later firmware versions, TNETW1450 only? */
#define ACX1FF_IE_DTIM_PERIOD_LEN		(0x02)
#define ACX1FF_IE_ACI_CONFIG_SET		(0x0029)	/* later firmware versions; maybe TNETW1450 only? */
#define ACX1FF_IE_ACI_CONFIG_SET_LEN		(0x06)
#define ACX1FF_IE_EEPROM_VER			(0x0030)	/* later firmware versions; maybe TNETW1450 only? */
#define ACX1FF_IE_EEPROM_VER_LEN		(0x04)
#define ACX1xx_IE_DOT11_STATION_ID		(0x1001)
#define ACX1xx_IE_DOT11_STATION_ID_LEN		(0x06)
#define ACX100_IE_DOT11_UNKNOWN_1002		(0x1002)	/* mapped to cfgInvalid in FW150 */
#define ACX100_IE_DOT11_UNKNOWN_1002_LEN	(-1)
#define ACX111_IE_DOT11_FRAG_THRESH		(0x1002)	/* mapped to cfgInvalid in FW150; TNETW1450 has length 2!! */
#define ACX111_IE_DOT11_FRAG_THRESH_LEN		(-1)
#define ACX100_IE_DOT11_BEACON_PERIOD		(0x1003)	/* mapped to cfgInvalid in FW150 */
#define ACX100_IE_DOT11_BEACON_PERIOD_LEN	(0x02)
#define ACX1xx_IE_DOT11_DTIM_PERIOD		(0x1004)	/* mapped to cfgInvalid in FW150 */
#define ACX1xx_IE_DOT11_DTIM_PERIOD_LEN		(-1)
#define ACX1FF_IE_DOT11_MAX_RX_LIFETIME		(0x1004)	/* later firmware versions; maybe TNETW1450 only? */
#define ACX1FF_IE_DOT11_MAX_RX_LIFETIME_LEN	(-1)
#define ACX1xx_IE_DOT11_SHORT_RETRY_LIMIT	(0x1005)	/* TNETW1450: length 2 */
#define ACX1xx_IE_DOT11_SHORT_RETRY_LIMIT_LEN	(0x01)
#define ACX1xx_IE_DOT11_LONG_RETRY_LIMIT	(0x1006)	/* TNETW1450: length 2 */
#define ACX1xx_IE_DOT11_LONG_RETRY_LIMIT_LEN	(0x01)
#define ACX100_IE_DOT11_WEP_DEFAULT_KEY_WRITE	(0x1007)	/* configure default keys; TNETW1450 has length 0x24!! */
#define ACX100_IE_DOT11_WEP_DEFAULT_KEY_WRITE_LEN	(0x20)
#define ACX1xx_IE_DOT11_MAX_XMIT_MSDU_LIFETIME	(0x1008)
#define ACX1xx_IE_DOT11_MAX_XMIT_MSDU_LIFETIME_LEN	(0x04)
#define ACX1xx_IE_DOT11_GROUP_ADDR		(0x1009)
#define ACX1xx_IE_DOT11_GROUP_ADDR_LEN		(-1)
#define ACX1xx_IE_DOT11_CURRENT_REG_DOMAIN	(0x100A)
#define ACX1xx_IE_DOT11_CURRENT_REG_DOMAIN_LEN	(0x02)
/* It's harmless to have larger struct. Use USB case always. */
#define ACX1xx_IE_DOT11_CURRENT_ANTENNA		(0x100B)	/* in fact len=1 for PCI */
#define ACX1xx_IE_DOT11_CURRENT_ANTENNA_LEN	(0x02)
#define ACX1xx_IE_DOT11_UNKNOWN_100C		(0x100C)	/* mapped to cfgInvalid in FW150 */
#define ACX1xx_IE_DOT11_UNKNOWN_100C_LEN	(-1)
#define ACX1xx_IE_DOT11_TX_POWER_LEVEL		(0x100D)	/* TNETW1450 has length 2!! */
#define ACX1xx_IE_DOT11_TX_POWER_LEVEL_LEN	(0x01)
#define ACX1xx_IE_DOT11_CURRENT_CCA_MODE	(0x100E)	/* in fact len=1 for PCI */
#define ACX1xx_IE_DOT11_CURRENT_CCA_MODE_LEN	(0x02)
/* USB doesn't return anything - len==0?! */
#define ACX100_IE_DOT11_ED_THRESHOLD		(0x100f)
#define ACX100_IE_DOT11_ED_THRESHOLD_LEN	(0x04)
#define ACX1xx_IE_DOT11_WEP_DEFAULT_KEY_SET	(0x1010)	/* set default key ID; TNETW1450: length 2 */
#define ACX1xx_IE_DOT11_WEP_DEFAULT_KEY_SET_LEN	(0x01)
#define ACX100_IE_DOT11_UNKNOWN_1011		(0x1011)	/* mapped to cfgInvalid in FW150 */
#define ACX100_IE_DOT11_UNKNOWN_1011_LEN	(-1)
#define ACX1FF_IE_DOT11_CURR_5GHZ_REGDOM	(0x1011)	/* later firmware versions; maybe TNETW1450 only? */
#define ACX1FF_IE_DOT11_CURR_5GHZ_REGDOM_LEN	(-1)
#define ACX100_IE_DOT11_UNKNOWN_1012		(0x1012)	/* mapped to cfgInvalid in FW150 */
#define ACX100_IE_DOT11_UNKNOWN_1012_LEN	(-1)
#define ACX100_IE_DOT11_UNKNOWN_1013		(0x1013)	/* mapped to cfgInvalid in FW150 */
#define ACX100_IE_DOT11_UNKNOWN_1013_LEN	(-1)

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
#endif /* 0 */

#endif /* _ACX_IE_H_ */
