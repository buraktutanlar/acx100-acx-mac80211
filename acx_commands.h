#ifndef _ACX_COMMANDS_H_
#define _ACX_COMMANDS_H_

/*
 * acx_commands.h: querying/configuration commands and on-chip (EEPROM?)
 * registers. Differ depending on the chipset and bus type (PCI or USB - it
 * appears that VLYNQ behaves like PCI most of the time - most).
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
 * from snooping/anyothermeans of unhandled TI chipsets. Note that what was
 * called ACX.._IE_ before is now called ACX.._REG_, because what we talk about
 * here are really registers.
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
 * COMMANDS
 */

/* 
 * Original comment:
 *
 * ----------------------------------------------------------------------------
 *  can be found in table cmdTable in firmware "Rev. 1.5.0" (FW150)
 * ----------------------------------------------------------------------------
 *
 */
#define ACX1xx_CMD_RESET		0x00
#define ACX1xx_CMD_QUERY		0x01
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

/*
 * FIXME: get rid of this? We don't have any old firmware lying around, do we?
 */
#ifdef OLD_FIRMWARE_VERSIONS
/* mapped to unknownCMD in FW150 */
#define ACX100_CMD_HALT			0x0e
#else
#define ACX1xx_CMD_MEM_READ		0x0d
#define ACX1xx_CMD_MEM_WRITE		0x0e
#endif
#define ACX1xx_CMD_SLEEP		0x0f
#define ACX1xx_CMD_WAKE			0x10
/* mapped to unknownCMD in FW150 */
#define ACX1xx_CMD_UNKNOWN_11		0x11
#define ACX100_CMD_INIT_MEMORY		0x12
/* new firmware? TNETW1450? */
#define ACX1FF_CMD_DISABLE_RADIO	0x12
#define ACX1xx_CMD_CONFIG_BEACON	0x13
#define ACX1xx_CMD_CONFIG_PROBE_RESPONSE	0x14
#define ACX1xx_CMD_CONFIG_NULL_DATA	0x15
#define ACX1xx_CMD_CONFIG_PROBE_REQUEST	0x16
#define ACX1xx_CMD_FCC_TEST		0x17
#define ACX1xx_CMD_RADIOINIT		0x18
#define ACX111_CMD_RADIOCALIB		0x19
/* new firmware? TNETW1450? */
#define ACX1FF_CMD_NOISE_HISTOGRAM	0x1c
/* new firmware? TNETW1450? */
#define ACX1FF_CMD_RX_RESET		0x1d
/* new firmware? TNETW1450? */
#define ACX1FF_CMD_LNA_CONTROL		0x20
/* new firmware? TNETW1450? */
#define ACX1FF_CMD_CONTROL_DBG_TRACE	0x21


/*
 * REGISTERS
 *
 * Note that some of them are read/write, and others are read-only.
 *
 * Original comment, whatever it means:
 *
 * ----------------------------------------------------------------------------
 * these are handled by real_cfgtable in firmware "Rev 1.5.0" (FW150)
 * ----------------------------------------------------------------------------
 */
//DEF_IE(1xx_IE_UNKNOWN_00		,0x0000, -1);	/* mapped to cfgInvalid in FW150 */
#define ACX100_REG_ACX_TIMER			(0x0001)
#define ACX100_REG_ACX_TIMER_LEN 		(0x10)
/* TNETW1450: length 0x18!! */
#define ACX1xx_REG_POWER_MGMT			(0x0002)
#define ACX1xx_REG_POWER_MGMT_LEN 		(0x06)
#define ACX1xx_REG_QUEUE_CONFIG			(0x0003)
#define ACX1xx_REG_QUEUE_CONFIG_LEN		(0x1c)
#define ACX100_REG_BLOCK_SIZE			(0x0004)
#define ACX100_REG_BLOCK_SIZE_LEN		(0x02)
/* later firmware versions only? */
#define ACX1FF_REG_SLOT_TIME			(0x0004)
#define ACX1FF_REG_SLOT_TIME_LEN			(0x08)
#define ACX1xx_REG_MEMORY_CONFIG_OPTIONS		(0x0005)
#define ACX1xx_REG_MEMORY_CONFIG_OPTIONS_LEN	(0x14)
#define ACX1FF_REG_QUEUE_HEAD			(0x0005)
/* FIXME: length? */
#define ACX1FF_REG_QUEUE_HEAD_LEN		(0x14)
/* TNETW1450: length 2 */
#define ACX1xx_REG_RATE_FALLBACK			(0x0006)
#define ACX1xx_REG_RATE_FALLBACK_LEN		(0x01)
#define ACX100_REG_WEP_OPTIONS			(0x0007)
#define ACX100_REG_WEP_OPTIONS_LEN		(0x03)
#define ACX111_REG_RADIO_BAND			(0x0007)
#define ACX111_REG_RADIO_BAND_LEN		(-1)
/* later firmware versions; TNETW1450 only? */
#define ACX1FF_REG_TIMING_CFG			(0x0007)
#define ACX1FF_REG_TIMING_CFG_LEN		(-1)
/* huh? */
#define ACX100_REG_SSID				(0x0008)
#define ACX100_REG_SSID_LEN			(0x20)
/* huh? TNETW1450 has length 0x40!! */
#define ACX1xx_REG_MEMORY_MAP			(0x0008)
#define ACX1xx_REG_MEMORY_MAP_LEN		(0x28)
/* mapped to cfgInvalid in FW150 */
#define ACX1xx_REG_SCAN_STATUS			(0x0009)
#define ACX1xx_REG_SCAN_STATUS_LEN		(0x04)
#define ACX1xx_REG_ASSOC_ID			(0x000a)
#define ACX1xx_REG_ASSOC_ID_LEN			(0x02)
/* mapped to cfgInvalid in FW150 */
#define ACX1xx_REG_UNKNOWN_0B			(0x000b)
#define ACX1xx_REG_UNKNOWN_0B_LEN		(-1)
/* later firmware versions; TNETW1450 only? */
#define ACX1FF_REG_TX_POWER_LEVEL_TABLE		(0x000b)
#define ACX1FF_REG_TX_POWER_LEVEL_TABLE_LEN	(0x18)
/* very small implementation in FW150! */
#define ACX100_REG_UNKNOWN_0C			(0x000c)
#define ACX100_REG_UNKNOWN_0C_LEN		(-1)
/*
 * ACX100 has an equivalent struct in the cmd mailbox directly after reset.
 * 0x14c seems extremely large, will trash stack on failure (memset!)
 * in case of small input struct --> OOPS!
 */
#define ACX111_REG_CONFIG_OPTIONS		(0x000c)
#define ACX111_REG_CONFIG_OPTIONS_LEN		(0x14c)
#define ACX1xx_REG_FWREV				(0x000d)
#define ACX1xx_REG_FWREV_LEN			(0x18)
#define ACX1xx_REG_FCS_ERROR_COUNT		(0x000e)
#define ACX1xx_REG_FCS_ERROR_COUNT_LEN		(0x04)
#define ACX1xx_REG_MEDIUM_USAGE			(0x000f)
#define ACX1xx_REG_MEDIUM_USAGE_LEN		(0x08)
#define ACX1xx_REG_RXCONFIG			(0x0010)
#define ACX1xx_REG_RXCONFIG_LEN			(0x04)
/* NONBINARY: large implementation in FW150! link quality readings or so? */
#define ACX100_REG_UNKNOWN_11			(0x0011)
#define ACX100_REG_UNKNOWN_11_LEN		(-1)
#define ACX111_REG_QUEUE_THRESH			(0x0011)
#define ACX111_REG_QUEUE_THRESH_LEN		(-1)
/* NONBINARY: VERY large implementation in FW150!! */
#define ACX100_REG_UNKNOWN_12			(0x0012)
#define ACX100_REG_UNKNOWN_12_LEN		(-1)
#define ACX111_REG_BSS_POWER_SAVE		(0x0012)
#define ACX111_REG_BSS_POWER_SAVE_LEN		(/* -1 */ 2)
/* TNETW1450: length 0x134!! */
#define ACX1xx_REG_FIRMWARE_STATISTICS		(0x0013)
#define ACX1xx_REG_FIRMWARE_STATISTICS_LEN	(0x9c)
/* later firmware versions, TNETW1450 only? */
#define ACX1FF_REG_RX_INTR_CONFIG		(0x0014)
#define ACX1FF_REG_RX_INTR_CONFIG_LEN		(0x14)
#define ACX1xx_REG_FEATURE_CONFIG		(0x0015)
#define ACX1xx_REG_FEATURE_CONFIG_LEN		(0x08)
/* for rekeying. really len=4?? */
#define ACX111_REG_KEY_CHOOSE			(0x0016)
#define ACX111_REG_KEY_CHOOSE_LEN		(0x04)
/* later firmware versions, TNETW1450 only? */
#define ACX1FF_REG_MISC_CONFIG_TABLE		(0x0017)
#define ACX1FF_REG_MISC_CONFIG_TABLE_LEN		(0x04)
/* later firmware versions, TNETW1450 only? */
#define ACX1FF_REG_WONE_CONFIG			(0x0018)
#define ACX1FF_REG_WONE_CONFIG_LEN		(-1)
/* later firmware versions, TNETW1450 only? */
#define ACX1FF_REG_TID_CONFIG			(0x001a)
#define ACX1FF_REG_TID_CONFIG_LEN		(0x2c)
/* later firmware versions, TNETW1450 only? */
#define ACX1FF_REG_CALIB_ASSESSMENT		(0x001e)
#define ACX1FF_REG_CALIB_ASSESSMENT_LEN		(0x04)
/* later firmware versions, TNETW1450 only? */
#define ACX1FF_REG_BEACON_FILTER_OPTIONS		(0x001f)
#define ACX1FF_REG_BEACON_FILTER_OPTIONS_LEN	(0x02)
/* later firmware versions, TNETW1450 only? */
#define ACX1FF_REG_LOW_RSSI_THRESH_OPT		(0x0020)
#define ACX1FF_REG_LOW_RSSI_THRESH_OPT_LEN	(0x04)
/* later firmware versions, TNETW1450 only? */
#define ACX1FF_REG_NOISE_HISTOGRAM_RESULTS	(0x0021)
#define ACX1FF_REG_NOISE_HISTOGRAM_RESULTS_LEN	(0x30)
/* later firmware versions, TNETW1450 only? */
#define ACX1FF_REG_PACKET_DETECT_THRESH		(0x0023)
#define ACX1FF_REG_PACKET_DETECT_THRESH_LEN	(0x04)
/* later firmware versions, TNETW1450 only? */
#define ACX1FF_REG_TX_CONFIG_OPTIONS		(0x0024)
#define ACX1FF_REG_TX_CONFIG_OPTIONS_LEN		(0x04)
/* later firmware versions, TNETW1450 only? */
#define ACX1FF_REG_CCA_THRESHOLD			(0x0025)
#define ACX1FF_REG_CCA_THRESHOLD_LEN		(0x02)
/* later firmware versions, TNETW1450 only? */
#define ACX1FF_REG_EVENT_MASK			(0x0026)
#define ACX1FF_REG_EVENT_MASK_LEN		(0x08)
/* later firmware versions, TNETW1450 only? */
#define ACX1FF_REG_DTIM_PERIOD			(0x0027)
#define ACX1FF_REG_DTIM_PERIOD_LEN		(0x02)
/* later firmware versions; maybe TNETW1450 only? */
#define ACX1FF_REG_ACI_CONFIG_SET		(0x0029)
#define ACX1FF_REG_ACI_CONFIG_SET_LEN		(0x06)
/* later firmware versions; maybe TNETW1450 only? */
#define ACX1FF_REG_EEPROM_VER			(0x0030)
#define ACX1FF_REG_EEPROM_VER_LEN		(0x04)
#define ACX1xx_REG_DOT11_STATION_ID		(0x1001)
#define ACX1xx_REG_DOT11_STATION_ID_LEN		(0x06)
/* mapped to cfgInvalid in FW150 */
#define ACX100_REG_DOT11_UNKNOWN_1002		(0x1002)
#define ACX100_REG_DOT11_UNKNOWN_1002_LEN	(-1)
/* mapped to cfgInvalid in FW150; TNETW1450 has length 2!! */
#define ACX111_REG_DOT11_FRAG_THRESH		(0x1002)
#define ACX111_REG_DOT11_FRAG_THRESH_LEN		(-1)
/* mapped to cfgInvalid in FW150 */
#define ACX100_REG_DOT11_BEACON_PERIOD		(0x1003)
#define ACX100_REG_DOT11_BEACON_PERIOD_LEN	(0x02)
/* mapped to cfgInvalid in FW150 */
#define ACX1xx_REG_DOT11_DTIM_PERIOD		(0x1004)
#define ACX1xx_REG_DOT11_DTIM_PERIOD_LEN		(-1)
/* later firmware versions; maybe TNETW1450 only? */
#define ACX1FF_REG_DOT11_MAX_RX_LIFETIME		(0x1004)
#define ACX1FF_REG_DOT11_MAX_RX_LIFETIME_LEN	(-1)
/* TNETW1450: length 2 */
#define ACX1xx_REG_DOT11_SHORT_RETRY_LIMIT	(0x1005)
#define ACX1xx_REG_DOT11_SHORT_RETRY_LIMIT_LEN	(0x01)
/* TNETW1450: length 2 */
#define ACX1xx_REG_DOT11_LONG_RETRY_LIMIT	(0x1006)
#define ACX1xx_REG_DOT11_LONG_RETRY_LIMIT_LEN	(0x01)
/* configure default keys; TNETW1450 has length 0x24!! */
#define ACX100_REG_DOT11_WEP_DEFAULT_KEY_WRITE	(0x1007)
#define ACX100_REG_DOT11_WEP_DEFAULT_KEY_WRITE_LEN	(0x20)
#define ACX1xx_REG_DOT11_MAX_XMIT_MSDU_LIFETIME	(0x1008)
#define ACX1xx_REG_DOT11_MAX_XMIT_MSDU_LIFETIME_LEN	(0x04)
#define ACX1xx_REG_DOT11_GROUP_ADDR		(0x1009)
#define ACX1xx_REG_DOT11_GROUP_ADDR_LEN		(-1)
#define ACX1xx_REG_DOT11_CURRENT_REG_DOMAIN	(0x100a)
#define ACX1xx_REG_DOT11_CURRENT_REG_DOMAIN_LEN	(0x02)
/* It's harmless to have larger struct. Use USB case always. */
/* in fact len=1 for PCI */
#define ACX1xx_REG_DOT11_CURRENT_ANTENNA		(0x100b)
#define ACX1xx_REG_DOT11_CURRENT_ANTENNA_LEN	(0x02)
/* mapped to cfgInvalid in FW150 */
#define ACX1xx_REG_DOT11_UNKNOWN_100C		(0x100c)
#define ACX1xx_REG_DOT11_UNKNOWN_100C_LEN	(-1)
/* TNETW1450 has length 2!! */
#define ACX1xx_REG_DOT11_TX_POWER_LEVEL		(0x100d)
#define ACX1xx_REG_DOT11_TX_POWER_LEVEL_LEN	(0x01)
/* in fact len=1 for PCI */
#define ACX1xx_REG_DOT11_CURRENT_CCA_MODE	(0x100e)
#define ACX1xx_REG_DOT11_CURRENT_CCA_MODE_LEN	(0x02)
/* USB doesn't return anything - len==0?! */
#define ACX100_REG_DOT11_ED_THRESHOLD		(0x100f)
#define ACX100_REG_DOT11_ED_THRESHOLD_LEN	(0x04)
/* set default key ID; TNETW1450: length 2 */
#define ACX1xx_REG_DOT11_WEP_DEFAULT_KEY_SET	(0x1010)
#define ACX1xx_REG_DOT11_WEP_DEFAULT_KEY_SET_LEN	(0x01)
/* mapped to cfgInvalid in FW150 */
#define ACX100_REG_DOT11_UNKNOWN_1011		(0x1011)
#define ACX100_REG_DOT11_UNKNOWN_1011_LEN	(-1)
/* later firmware versions; maybe TNETW1450 only? */
#define ACX1FF_REG_DOT11_CURR_5GHZ_REGDOM	(0x1011)
#define ACX1FF_REG_DOT11_CURR_5GHZ_REGDOM_LEN	(-1)
/* mapped to cfgInvalid in FW150 */
#define ACX100_REG_DOT11_UNKNOWN_1012		(0x1012)
#define ACX100_REG_DOT11_UNKNOWN_1012_LEN	(-1)
/* mapped to cfgInvalid in FW150 */
#define ACX100_REG_DOT11_UNKNOWN_1013		(0x1013)
#define ACX100_REG_DOT11_UNKNOWN_1013_LEN	(-1)


#endif /* _ACX_COMMANDS_H_ */
