/*
 * Copyright (C) 2003-2008 The ACX100 Open Source Project
 * <acx100-devel@lists.sourceforge.net>
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

/* Information Elements: Network Parameters, Static Configuration Entities */
/* these are handled by real_cfgtable in firmware "Rev 1.5.0" (FW150) */
//DEF_IE(1xx_IE_UNKNOWN_00		,0x0000, -1);	/* mapped to cfgInvalid in FW150 */
#define ACX100_IE_ACX_TIMER			(0x0001)
#define ACX100_IE_ACX_TIMER_LEN 		(0x10)
/* TNETW1450: length 0x18!! */
#define ACX1xx_IE_POWER_MGMT			(0x0002)
#define ACX1xx_IE_POWER_MGMT_LEN 		(0x06)
#define ACX1xx_IE_QUEUE_CONFIG			(0x0003)
#define ACX1xx_IE_QUEUE_CONFIG_LEN		(0x1c)
#define ACX100_IE_BLOCK_SIZE			(0x0004)
#define ACX100_IE_BLOCK_SIZE_LEN		(0x02)
/* later firmware versions only? */
#define ACX1FF_IE_SLOT_TIME			(0x0004)
#define ACX1FF_IE_SLOT_TIME_LEN			(0x08)
#define ACX1xx_IE_MEMORY_CONFIG_OPTIONS		(0x0005)
#define ACX1xx_IE_MEMORY_CONFIG_OPTIONS_LEN	(0x14)
#define ACX1FF_IE_QUEUE_HEAD			(0x0005)
#define ACX1FF_IE_QUEUE_HEAD_LEN		(0x14 /* FIXME: length? */)
/* TNETW1450: length 2 */
#define ACX1xx_IE_RATE_FALLBACK			(0x0006)
#define ACX1xx_IE_RATE_FALLBACK_LEN		(0x01)
#define ACX100_IE_WEP_OPTIONS			(0x0007)
#define ACX100_IE_WEP_OPTIONS_LEN		(0x03)
#define ACX111_IE_RADIO_BAND			(0x0007)
#define ACX111_IE_RADIO_BAND_LEN		(-1)
/* later firmware versions; TNETW1450 only? */
#define ACX1FF_IE_TIMING_CFG			(0x0007)
#define ACX1FF_IE_TIMING_CFG_LEN		(-1)
/* huh? */
#define ACX100_IE_SSID				(0x0008)
#define ACX100_IE_SSID_LEN			(0x20)
/* huh? TNETW1450 has length 0x40!! */
#define ACX1xx_IE_MEMORY_MAP			(0x0008)
#define ACX1xx_IE_MEMORY_MAP_LEN		(0x28)
/* mapped to cfgInvalid in FW150 */
#define ACX1xx_IE_SCAN_STATUS			(0x0009)
#define ACX1xx_IE_SCAN_STATUS_LEN		(0x04)
#define ACX1xx_IE_ASSOC_ID			(0x000a)
#define ACX1xx_IE_ASSOC_ID_LEN			(0x02)
/* mapped to cfgInvalid in FW150 */
#define ACX1xx_IE_UNKNOWN_0B			(0x000b)
#define ACX1xx_IE_UNKNOWN_0B_LEN		(-1)
/* later firmware versions; TNETW1450 only? */
#define ACX1FF_IE_TX_POWER_LEVEL_TABLE		(0x000b)
#define ACX1FF_IE_TX_POWER_LEVEL_TABLE_LEN	(0x18)
/* very small implementation in FW150! */
#define ACX100_IE_UNKNOWN_0C			(0x000c)
#define ACX100_IE_UNKNOWN_0C_LEN		(-1)
/*
 * ACX100 has an equivalent struct in the cmd mailbox directly after reset.
 * 0x14c seems extremely large, will trash stack on failure (memset!)
 * in case of small input struct --> OOPS!
 */
#define ACX111_IE_CONFIG_OPTIONS		(0x000c)
#define ACX111_IE_CONFIG_OPTIONS_LEN		(0x14c)
#define ACX1xx_IE_FWREV				(0x000d)
#define ACX1xx_IE_FWREV_LEN			(0x18)
#define ACX1xx_IE_FCS_ERROR_COUNT		(0x000e)
#define ACX1xx_IE_FCS_ERROR_COUNT_LEN		(0x04)
#define ACX1xx_IE_MEDIUM_USAGE			(0x000f)
#define ACX1xx_IE_MEDIUM_USAGE_LEN		(0x08)
#define ACX1xx_IE_RXCONFIG			(0x0010)
#define ACX1xx_IE_RXCONFIG_LEN			(0x04)
/* NONBINARY: large implementation in FW150! link quality readings or so? */
#define ACX100_IE_UNKNOWN_11			(0x0011)
#define ACX100_IE_UNKNOWN_11_LEN		(-1)
#define ACX111_IE_QUEUE_THRESH			(0x0011)
#define ACX111_IE_QUEUE_THRESH_LEN		(-1)
/* NONBINARY: VERY large implementation in FW150!! */
#define ACX100_IE_UNKNOWN_12			(0x0012)
#define ACX100_IE_UNKNOWN_12_LEN		(-1)
#define ACX111_IE_BSS_POWER_SAVE		(0x0012)
#define ACX111_IE_BSS_POWER_SAVE_LEN		(/* -1 */ 2)
/* TNETW1450: length 0x134!! */
#define ACX1xx_IE_FIRMWARE_STATISTICS		(0x0013)
#define ACX1xx_IE_FIRMWARE_STATISTICS_LEN	(0x9c)
/* later firmware versions, TNETW1450 only? */
#define ACX1FF_IE_RX_INTR_CONFIG		(0x0014)
#define ACX1FF_IE_RX_INTR_CONFIG_LEN		(0x14)
#define ACX1xx_IE_FEATURE_CONFIG		(0x0015)
#define ACX1xx_IE_FEATURE_CONFIG_LEN		(0x08)
/* for rekeying. really len=4?? */
#define ACX111_IE_KEY_CHOOSE			(0x0016)
#define ACX111_IE_KEY_CHOOSE_LEN		(0x04)
/* later firmware versions, TNETW1450 only? */
#define ACX1FF_IE_MISC_CONFIG_TABLE		(0x0017)
#define ACX1FF_IE_MISC_CONFIG_TABLE_LEN		(0x04)
/* later firmware versions, TNETW1450 only? */
#define ACX1FF_IE_WONE_CONFIG			(0x0018)
#define ACX1FF_IE_WONE_CONFIG_LEN		(-1)
/* later firmware versions, TNETW1450 only? */
#define ACX1FF_IE_TID_CONFIG			(0x001a)
#define ACX1FF_IE_TID_CONFIG_LEN		(0x2c)
/* later firmware versions, TNETW1450 only? */
#define ACX1FF_IE_CALIB_ASSESSMENT		(0x001e)
#define ACX1FF_IE_CALIB_ASSESSMENT_LEN		(0x04)
/* later firmware versions, TNETW1450 only? */
#define ACX1FF_IE_BEACON_FILTER_OPTIONS		(0x001f)
#define ACX1FF_IE_BEACON_FILTER_OPTIONS_LEN	(0x02)
/* later firmware versions, TNETW1450 only? */
#define ACX1FF_IE_LOW_RSSI_THRESH_OPT		(0x0020)
#define ACX1FF_IE_LOW_RSSI_THRESH_OPT_LEN	(0x04)
/* later firmware versions, TNETW1450 only? */
#define ACX1FF_IE_NOISE_HISTOGRAM_RESULTS	(0x0021)
#define ACX1FF_IE_NOISE_HISTOGRAM_RESULTS_LEN	(0x30)
/* later firmware versions, TNETW1450 only? */
#define ACX1FF_IE_PACKET_DETECT_THRESH		(0x0023)
#define ACX1FF_IE_PACKET_DETECT_THRESH_LEN	(0x04)
/* later firmware versions, TNETW1450 only? */
#define ACX1FF_IE_TX_CONFIG_OPTIONS		(0x0024)
#define ACX1FF_IE_TX_CONFIG_OPTIONS_LEN		(0x04)
/* later firmware versions, TNETW1450 only? */
#define ACX1FF_IE_CCA_THRESHOLD			(0x0025)
#define ACX1FF_IE_CCA_THRESHOLD_LEN		(0x02)
/* later firmware versions, TNETW1450 only? */
#define ACX1FF_IE_EVENT_MASK			(0x0026)
#define ACX1FF_IE_EVENT_MASK_LEN		(0x08)
/* later firmware versions, TNETW1450 only? */
#define ACX1FF_IE_DTIM_PERIOD			(0x0027)
#define ACX1FF_IE_DTIM_PERIOD_LEN		(0x02)
/* later firmware versions; maybe TNETW1450 only? */
#define ACX1FF_IE_ACI_CONFIG_SET		(0x0029)
#define ACX1FF_IE_ACI_CONFIG_SET_LEN		(0x06)
/* later firmware versions; maybe TNETW1450 only? */
#define ACX1FF_IE_EEPROM_VER			(0x0030)
#define ACX1FF_IE_EEPROM_VER_LEN		(0x04)
#define ACX1xx_IE_DOT11_STATION_ID		(0x1001)
#define ACX1xx_IE_DOT11_STATION_ID_LEN		(0x06)
/* mapped to cfgInvalid in FW150 */
#define ACX100_IE_DOT11_UNKNOWN_1002		(0x1002)
#define ACX100_IE_DOT11_UNKNOWN_1002_LEN	(-1)
/* mapped to cfgInvalid in FW150; TNETW1450 has length 2!! */
#define ACX111_IE_DOT11_FRAG_THRESH		(0x1002)
#define ACX111_IE_DOT11_FRAG_THRESH_LEN		(-1)
/* mapped to cfgInvalid in FW150 */
#define ACX100_IE_DOT11_BEACON_PERIOD		(0x1003)
#define ACX100_IE_DOT11_BEACON_PERIOD_LEN	(0x02)
/* mapped to cfgInvalid in FW150 */
#define ACX1xx_IE_DOT11_DTIM_PERIOD		(0x1004)
#define ACX1xx_IE_DOT11_DTIM_PERIOD_LEN		(-1)
/* later firmware versions; maybe TNETW1450 only? */
#define ACX1FF_IE_DOT11_MAX_RX_LIFETIME		(0x1004)
#define ACX1FF_IE_DOT11_MAX_RX_LIFETIME_LEN	(-1)
/* TNETW1450: length 2 */
#define ACX1xx_IE_DOT11_SHORT_RETRY_LIMIT	(0x1005)
#define ACX1xx_IE_DOT11_SHORT_RETRY_LIMIT_LEN	(0x01)
/* TNETW1450: length 2 */
#define ACX1xx_IE_DOT11_LONG_RETRY_LIMIT	(0x1006)
#define ACX1xx_IE_DOT11_LONG_RETRY_LIMIT_LEN	(0x01)
/* configure default keys; TNETW1450 has length 0x24!! */
#define ACX100_IE_DOT11_WEP_DEFAULT_KEY_WRITE	(0x1007)
#define ACX100_IE_DOT11_WEP_DEFAULT_KEY_WRITE_LEN	(0x20)
#define ACX1xx_IE_DOT11_MAX_XMIT_MSDU_LIFETIME	(0x1008)
#define ACX1xx_IE_DOT11_MAX_XMIT_MSDU_LIFETIME_LEN	(0x04)
#define ACX1xx_IE_DOT11_GROUP_ADDR		(0x1009)
#define ACX1xx_IE_DOT11_GROUP_ADDR_LEN		(-1)
#define ACX1xx_IE_DOT11_CURRENT_REG_DOMAIN	(0x100a)
#define ACX1xx_IE_DOT11_CURRENT_REG_DOMAIN_LEN	(0x02)
/* It's harmless to have larger struct. Use USB case always. */
/* in fact len=1 for PCI */
#define ACX1xx_IE_DOT11_CURRENT_ANTENNA		(0x100b)
#define ACX1xx_IE_DOT11_CURRENT_ANTENNA_LEN	(0x02)
/* mapped to cfgInvalid in FW150 */
#define ACX1xx_IE_DOT11_UNKNOWN_100C		(0x100c)
#define ACX1xx_IE_DOT11_UNKNOWN_100C_LEN	(-1)
/* TNETW1450 has length 2!! */
#define ACX1xx_IE_DOT11_TX_POWER_LEVEL		(0x100d)
#define ACX1xx_IE_DOT11_TX_POWER_LEVEL_LEN	(0x01)
/* in fact len=1 for PCI */
#define ACX1xx_IE_DOT11_CURRENT_CCA_MODE	(0x100e)
#define ACX1xx_IE_DOT11_CURRENT_CCA_MODE_LEN	(0x02)
/* USB doesn't return anything - len==0?! */
#define ACX100_IE_DOT11_ED_THRESHOLD		(0x100f)
#define ACX100_IE_DOT11_ED_THRESHOLD_LEN	(0x04)
/* set default key ID; TNETW1450: length 2 */
#define ACX1xx_IE_DOT11_WEP_DEFAULT_KEY_SET	(0x1010)
#define ACX1xx_IE_DOT11_WEP_DEFAULT_KEY_SET_LEN	(0x01)
/* mapped to cfgInvalid in FW150 */
#define ACX100_IE_DOT11_UNKNOWN_1011		(0x1011)
#define ACX100_IE_DOT11_UNKNOWN_1011_LEN	(-1)
/* later firmware versions; maybe TNETW1450 only? */
#define ACX1FF_IE_DOT11_CURR_5GHZ_REGDOM	(0x1011)
#define ACX1FF_IE_DOT11_CURR_5GHZ_REGDOM_LEN	(-1)
/* mapped to cfgInvalid in FW150 */
#define ACX100_IE_DOT11_UNKNOWN_1012		(0x1012)
#define ACX100_IE_DOT11_UNKNOWN_1012_LEN	(-1)
/* mapped to cfgInvalid in FW150 */
#define ACX100_IE_DOT11_UNKNOWN_1013		(0x1013)
#define ACX100_IE_DOT11_UNKNOWN_1013_LEN	(-1)

#endif /* _ACX_IE_H_ */
