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
#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/if_arp.h>
#include <linux/rtnetlink.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/wireless.h>
#include <linux/pm.h>
#include <linux/vmalloc.h>
#include <linux/firmware.h>
//#include <net/iw_handler.h>
#include <linux/ethtool.h>
//#include <linux/utsrelease.h>
#include <linux/ctype.h>

#include "acx.h"

#define DEBUG_TSC 0
#if DEBUG_TSC && defined(CONFIG_X86)
#define TIMESTAMP(d) unsigned long d; rdtscl(d)
#else
#define TIMESTAMP(d) unsigned long d = jiffies
#endif

// OW Debugging
#include "wlan_compat.h"
#include "wlan_hdr.h"

/***********************************************************************
*/

static void acx_l_rx(acx_device_t * adev, rxbuffer_t * rxbuf);



/***********************************************************************
*/
#if ACX_DEBUG
unsigned int acx_debug /* will add __read_mostly later */  = ACX_DEFAULT_MSG;
/* parameter is 'debug', corresponding var is acx_debug */
module_param_named(debug, acx_debug, uint, 0);
MODULE_PARM_DESC(debug, "Debug level mask (see L_xxx constants)");
#endif

#ifdef MODULE_LICENSE
MODULE_LICENSE("Dual MPL/GPL");
#endif
/* USB had this: MODULE_AUTHOR("Martin Wawro <martin.wawro AT uni-dortmund.de>"); */
MODULE_AUTHOR("ACX100 Open Source Driver development team");
MODULE_DESCRIPTION
    ("Driver for TI ACX1xx based wireless cards (CardBus/PCI/USB)");

MODULE_VERSION(ACX_RELEASE);

/***********************************************************************
*/
/* Probably a number of acx's intermediate buffers for USB transfers,
** not to be confused with number of descriptors in tx/rx rings
** (which are not directly accessible to host in USB devices) */
#define USB_RX_CNT 10
#define USB_TX_CNT 10


/***********************************************************************
*/

/* minutes to wait until next radio recalibration: */
#define RECALIB_PAUSE	5

/* Please keep acx_reg_domain_ids_len in sync... */
const u8 acx_reg_domain_ids[acx_reg_domain_ids_len] =
    { 0x10, 0x20, 0x30, 0x31, 0x32, 0x40, 0x41, 0x51 };
static const u16 reg_domain_channel_masks[acx_reg_domain_ids_len] =
    { 0x07ff, 0x07ff, 0x1fff, 0x0600, 0x1e00, 0x2000, 0x3fff, 0x01fc };
const char *const
 acx_reg_domain_strings[] = {
	/* 0 */ " 1-11 FCC (USA)",
	/* 1 */ " 1-11 DOC/IC (Canada)",
/* BTW: WLAN use in ETSI is regulated by ETSI standard EN 300 328-2 V1.1.2 */
	/* 2 */ " 1-13 ETSI (Europe)",
	/* 3 */ "10-11 Spain",
	/* 4 */ "10-13 France",
	/* 5 */ "   14 MKK (Japan)",
	/* 6 */ " 1-14 MKK1",
	/* 7 */ "  3-9 Israel (not all firmware versions)",
	NULL			/* needs to remain as last entry */
};



/***********************************************************************
** Debugging support
*/
#ifdef PARANOID_LOCKING
static unsigned max_lock_time;
static unsigned max_sem_time;

/* Obvious or linux kernel specific derived code follows: */

void acx_lock_unhold()
{
	max_lock_time = 0;
}

void acx_sem_unhold()
{
	max_sem_time = 0;
}

static inline const char *sanitize_str(const char *s)
{
	const char *t = strrchr(s, '/');
	if (t)
		return t + 1;
	return s;
}

void acx_lock_debug(acx_device_t * adev, const char *where)
{
	unsigned int count = 100 * 1000 * 1000;
	TIMESTAMP(lock_start);
	where = sanitize_str(where);
	while (--count) {
		if (!spin_is_locked(&adev->spinlock))
			break;
		cpu_relax();
	}
	if (!count) {
		printk(KERN_EMERG "acx: LOCKUP: already taken at %s!\n",
		       adev->last_lock);
		BUG();
	}
	adev->last_lock = where;
	adev->lock_time = lock_start;
}

void acx_unlock_debug(acx_device_t * adev, const char *where)
{
#ifdef SMP
	if (!spin_is_locked(&adev->spinlock)) {
		where = sanitize_str(where);
		printk(KERN_EMERG "acx: STRAY UNLOCK at %s!\n", where);
		BUG();
	}
#endif
	if (acx_debug & L_LOCK) {
		TIMESTAMP(diff);
		diff -= adev->lock_time;
		if (diff > max_lock_time) {
			where = sanitize_str(where);
			printk("acx: max lock hold time %ld CPU ticks from %s "
			       "to %s\n", diff, adev->last_lock, where);
			max_lock_time = diff;
		}
	}
}
#endif /* PARANOID_LOCKING */


/***********************************************************************
*/
#if ACX_DEBUG > 1

static int acx_debug_func_indent;
#define FUNC_INDENT_INCREMENT 2

static const char spaces[] = "          " "          ";	/* Nx10 spaces */

void log_fn_enter(const char *funcname)
{
	int indent;
	TIMESTAMP(d);

	indent = acx_debug_func_indent;
	if (indent >= sizeof(spaces))
		indent = sizeof(spaces) - 1;

	printk("%08ld %s==> %s\n",
	       d % 100000000, spaces + (sizeof(spaces) - 1) - indent, funcname);

	acx_debug_func_indent += FUNC_INDENT_INCREMENT;
}
void log_fn_exit(const char *funcname)
{
	int indent;
	TIMESTAMP(d);

	// OW Handle underflow
	if (acx_debug_func_indent >= FUNC_INDENT_INCREMENT)
		acx_debug_func_indent -= FUNC_INDENT_INCREMENT;
	else
		acx_debug_func_indent = 0;

	indent = acx_debug_func_indent;
	if (indent >= sizeof(spaces))
		indent = sizeof(spaces) - 1;

	printk("%08ld %s<== %s\n",
	       d % 100000000, spaces + (sizeof(spaces) - 1) - indent, funcname);
}
void log_fn_exit_v(const char *funcname, int v)
{
	int indent;
	TIMESTAMP(d);

	acx_debug_func_indent -= FUNC_INDENT_INCREMENT;

	indent = acx_debug_func_indent;
	if (indent >= sizeof(spaces))
		indent = sizeof(spaces) - 1;

	printk("%08ld %s<== %s: %08X\n",
	       d % 100000000,
	       spaces + (sizeof(spaces) - 1) - indent, funcname, v);
}
#endif /* ACX_DEBUG > 1 */


/***********************************************************************
** Basically a mdelay/msleep with logging
*/
void acx_s_mwait(int ms)
{
	FN_ENTER;
	msleep(ms);
	FN_EXIT0;
}

/***********************************************************************
** Not inlined: it's larger than it seems
*/
char* acx_print_mac(char *buf, const u8 *mac)
{
	sprintf(buf, MACSTR, MAC(mac));
	return(buf);
}

void acx_print_mac2(const char *head, const u8 *mac, const char *tail)
{
	printk("acx: %s" MACSTR "%s", head, MAC(mac), tail);
}

/***********************************************************************
** acx_cmd_status_str
*/
const char *acx_cmd_status_str(unsigned int state)
{
	static const char *const cmd_error_strings[] = {
		"Idle",
		"Success",
		"Unknown Command",
		"Invalid Information Element",
		"Channel rejected",
		"Channel invalid in current regulatory domain",
		"MAC invalid",
		"Command rejected (read-only information element)",
		"Command rejected",
		"Already asleep",
		"TX in progress",
		"Already awake",
		"Write only",
		"RX in progress",
		"Invalid parameter",
		"Scan in progress",
		"Failed"
	};
	return state < ARRAY_SIZE(cmd_error_strings) ?
	    cmd_error_strings[state] : "?";
}

/***********************************************************************
*/
#if ACX_DEBUG
void acx_dump_bytes(const void *data, int num)
{
	const u8 *ptr = (const u8 *)data;

	FN_ENTER;

	if (num <= 0) {
		printk("\n");
		return;
	}

	while (num >= 16) {
		printk("%02X %02X %02X %02X %02X %02X %02X %02X "
		       "%02X %02X %02X %02X %02X %02X %02X %02X\n",
		       ptr[0], ptr[1], ptr[2], ptr[3],
		       ptr[4], ptr[5], ptr[6], ptr[7],
		       ptr[8], ptr[9], ptr[10], ptr[11],
		       ptr[12], ptr[13], ptr[14], ptr[15]);
		num -= 16;
		ptr += 16;
	}
	if (num > 0) {
		while (--num > 0)
			printk("%02X ", *ptr++);
		printk("%02X\n", *ptr);
	}

	FN_EXIT0;

}
#endif


/***********************************************************************
** acx_s_get_firmware_version
**
** Obvious
*/
void acx_s_get_firmware_version(acx_device_t * adev)
{
	fw_ver_t fw;
	u8 hexarr[4] = { 0, 0, 0, 0 };
	int hexidx = 0, val = 0;
	const char *num;
	char c;

	FN_ENTER;

	memset(fw.fw_id, 'E', FW_ID_SIZE);
	acx_s_interrogate(adev, &fw, ACX1xx_IE_FWREV);
	memcpy(adev->firmware_version, fw.fw_id, FW_ID_SIZE);
	adev->firmware_version[FW_ID_SIZE] = '\0';

	log(L_DEBUG, "acx: fw_ver: fw_id='%s' hw_id=%08X\n",
	    adev->firmware_version, fw.hw_id);

	if (strncmp(fw.fw_id, "Rev ", 4) != 0) {
		printk("acx: strange firmware version string "
		       "'%s', please report\n", adev->firmware_version);
		adev->firmware_numver = 0x01090407;	/* assume 1.9.4.7 */
	} else {
		num = &fw.fw_id[4];
		while (1) {
			c = *num++;
			if ((c == '.') || (c == '\0')) {
				hexarr[hexidx++] = val;
				if ((hexidx > 3) || (c == '\0'))	/* end? */
					break;
				val = 0;
				continue;
			}
			if ((c >= '0') && (c <= '9'))
				c -= '0';
			else
				c = c - 'a' + (char)10;
			val = val * 16 + c;
		}

		adev->firmware_numver = (u32) ((hexarr[0] << 24) |
					       (hexarr[1] << 16)
					       | (hexarr[2] << 8) | hexarr[3]);
		log(L_DEBUG, "acx: firmware_numver 0x%08X\n", adev->firmware_numver);
	}
	if (IS_ACX111(adev)) {
		if (adev->firmware_numver == 0x00010011) {
			/* This one does not survive floodpinging */
			printk("acx: firmware '%s' is known to be buggy, "
			       "please upgrade\n", adev->firmware_version);
		}
	}

	adev->firmware_id = le32_to_cpu(fw.hw_id);

	/* we're able to find out more detailed chip names now */
	switch (adev->firmware_id & 0xffff0000) {
	case 0x01010000:
	case 0x01020000:
		adev->chip_name = "TNETW1100A";
		break;
	case 0x01030000:
		adev->chip_name = "TNETW1100B";
		break;
	case 0x03000000:
	case 0x03010000:
		adev->chip_name = "TNETW1130";
		break;
	case 0x04030000:	/* 0x04030101 is TNETW1450 */
		adev->chip_name = "TNETW1450";
		break;
	default:
		printk("acx: unknown chip ID 0x%08X, "
		       "please report\n", adev->firmware_id);
		break;
	}

	FN_EXIT0;
}


/***********************************************************************
** acx_display_hardware_details
**
** Displays hw/fw version, radio type etc...
**
** Obvious
*/
void acx_display_hardware_details(acx_device_t * adev)
{
	const char *radio_str, *form_str;

	FN_ENTER;

	switch (adev->radio_type) {
	case RADIO_MAXIM_0D:
		radio_str = "Maxim";
		break;
	case RADIO_RFMD_11:
		radio_str = "RFMD";
		break;
	case RADIO_RALINK_15:
		radio_str = "Ralink";
		break;
	case RADIO_RADIA_16:
		radio_str = "Radia";
		break;
	case RADIO_UNKNOWN_17:
		/* TI seems to have a radio which is
		 * additionally 802.11a capable, too */
		radio_str = "802.11a/b/g radio?! Please report";
		break;
	case RADIO_UNKNOWN_19:
		radio_str = "A radio used by Safecom cards?! Please report";
		break;
	case RADIO_UNKNOWN_1B:
		radio_str = "An unknown radio used by TNETW1450 USB adapters";
		break;
	default:
		radio_str = "UNKNOWN, please report radio type name!";
		break;
	}

	switch (adev->form_factor) {
	case 0x00:
		form_str = "unspecified";
		break;
	case 0x01:
		form_str = "(mini-)PCI / CardBus";
		break;
	case 0x02:
		form_str = "USB";
		break;
	case 0x03:
		form_str = "Compact Flash";
		break;
	default:
		form_str = "UNKNOWN, please report";
		break;
	}

	printk("acx: chipset %s, radio type 0x%02X (%s), "
	       "form factor 0x%02X (%s), EEPROM version 0x%02X, "
	       "uploaded firmware '%s'\n",
	       adev->chip_name, adev->radio_type, radio_str,
	       adev->form_factor, form_str, adev->eeprom_version,
	       adev->firmware_version);

	FN_EXIT0;
}


/***********************************************************************
** acx_e_get_stats, acx_e_get_wireless_stats
*/
int
acx_e_op_get_stats(struct ieee80211_hw *hw,
		struct ieee80211_low_level_stats *stats)
{
	acx_device_t *adev = ieee2adev(hw);
	unsigned long flags;

	FN_ENTER;
	acx_sem_lock(adev);

	acx_lock(adev, flags);
	memcpy(stats, &adev->ieee_stats, sizeof(*stats));
	acx_unlock(adev, flags);

	acx_sem_unlock(adev);
	FN_EXIT0;
	return 0;
}


/***********************************************************************
** maps acx111 tx descr rate field to acx100 one
*/
const u8 acx_bitpos2rate100[] = {
	RATE100_1,		/* 0 */
	RATE100_2,		/* 1 */
	RATE100_5,		/* 2 */
	RATE100_2,		/* 3, should not happen */
	RATE100_2,		/* 4, should not happen */
	RATE100_11,		/* 5 */
	RATE100_2,		/* 6, should not happen */
	RATE100_2,		/* 7, should not happen */
	RATE100_22,		/* 8 */
	RATE100_2,		/* 9, should not happen */
	RATE100_2,		/* 10, should not happen */
	RATE100_2,		/* 11, should not happen */
	RATE100_2,		/* 12, should not happen */
	RATE100_2,		/* 13, should not happen */
	RATE100_2,		/* 14, should not happen */
	RATE100_2,		/* 15, should not happen */
};

u8 acx_rate111to100(u16 r)
{
	return acx_bitpos2rate100[highest_bit(r)];
}


/***********************************************************************
** Calculate level like the feb 2003 windows driver seems to do
*
* Note: the FreeBSD and DragonFlyBSD drivers seems to use different
* so-called correction constants depending on the chip. They will be
* defined for now, but as it is still unknown whether they are correct
* or not, only the original value will be used. Something else to take
* into account is that the OpenBSD driver uses another approach and
* defines the maximum RSSI value depending on the chip, rather than
* using a value of 100 for all of them, as it is currently done here.
*/
#define ACX100_RSSI_CORR 8
#define ACX111_RSSI_CORR 5
static u8 acx_signal_to_winlevel(u8 rawlevel)
{
	/* u8 winlevel = (u8) (0.5 + 0.625 * rawlevel); */
	u8 winlevel = (((ACX100_RSSI_CORR / 2) + (rawlevel * 5)) /
			ACX100_RSSI_CORR);

	if (winlevel > 100)
		winlevel = 100;
	return winlevel;
}

u8 acx_signal_determine_quality(u8 signal, u8 noise)
{
	int qual;

	qual = (((signal - 30) * 100 / 70) + (100 - noise * 4)) / 2;

	if (qual > 100)
		return 100;
	if (qual < 0)
		return 0;
	return qual;
}


/***********************************************************************
** Interrogate/configure commands
*/

/* FIXME: the lengths given here probably aren't always correct.
 * They should be gradually replaced by proper "sizeof(acx1XX_ie_XXXX)-4",
 * unless the firmware actually expects a different length than the struct length */
static const u16 acx100_ie_len[] = {
	0,
	ACX100_IE_ACX_TIMER_LEN,
	sizeof(acx100_ie_powersave_t) - 4,	/* is that 6 or 8??? */
	ACX1xx_IE_QUEUE_CONFIG_LEN,
	ACX100_IE_BLOCK_SIZE_LEN,
	ACX1xx_IE_MEMORY_CONFIG_OPTIONS_LEN,
	ACX1xx_IE_RATE_FALLBACK_LEN,
	ACX100_IE_WEP_OPTIONS_LEN,
	ACX1xx_IE_MEMORY_MAP_LEN,	/*    ACX1xx_IE_SSID_LEN, */
	0,
	ACX1xx_IE_ASSOC_ID_LEN,
	0,
	ACX111_IE_CONFIG_OPTIONS_LEN,
	ACX1xx_IE_FWREV_LEN,
	ACX1xx_IE_FCS_ERROR_COUNT_LEN,
	ACX1xx_IE_MEDIUM_USAGE_LEN,
	ACX1xx_IE_RXCONFIG_LEN,
	0,
	0,
	sizeof(fw_stats_t) - 4,
	0,
	ACX1xx_IE_FEATURE_CONFIG_LEN,
	ACX111_IE_KEY_CHOOSE_LEN,
	ACX1FF_IE_MISC_CONFIG_TABLE_LEN,
	ACX1FF_IE_WONE_CONFIG_LEN,
	0,
	ACX1FF_IE_TID_CONFIG_LEN,
	0,
	0,
	0,
	ACX1FF_IE_CALIB_ASSESSMENT_LEN,
	ACX1FF_IE_BEACON_FILTER_OPTIONS_LEN,
	ACX1FF_IE_LOW_RSSI_THRESH_OPT_LEN,
	ACX1FF_IE_NOISE_HISTOGRAM_RESULTS_LEN,
	0,
	ACX1FF_IE_PACKET_DETECT_THRESH_LEN,
	ACX1FF_IE_TX_CONFIG_OPTIONS_LEN,
	ACX1FF_IE_CCA_THRESHOLD_LEN,
	ACX1FF_IE_EVENT_MASK_LEN,
	ACX1FF_IE_DTIM_PERIOD_LEN,
	0,
	ACX1FF_IE_ACI_CONFIG_SET_LEN,
	0,
	0,
	0,
	0,
	0,
	0,
	ACX1FF_IE_EEPROM_VER_LEN,
};

static const u16 acx100_ie_len_dot11[] = {
	0,
	ACX1xx_IE_DOT11_STATION_ID_LEN,
	0,
	ACX100_IE_DOT11_BEACON_PERIOD_LEN,
	ACX1xx_IE_DOT11_DTIM_PERIOD_LEN,
	ACX1xx_IE_DOT11_SHORT_RETRY_LIMIT_LEN,
	ACX1xx_IE_DOT11_LONG_RETRY_LIMIT_LEN,
	ACX100_IE_DOT11_WEP_DEFAULT_KEY_WRITE_LEN,
	ACX1xx_IE_DOT11_MAX_XMIT_MSDU_LIFETIME_LEN,
	0,
	ACX1xx_IE_DOT11_CURRENT_REG_DOMAIN_LEN,
	ACX1xx_IE_DOT11_CURRENT_ANTENNA_LEN,
	0,
	ACX1xx_IE_DOT11_TX_POWER_LEVEL_LEN,
	ACX1xx_IE_DOT11_CURRENT_CCA_MODE_LEN,
	ACX100_IE_DOT11_ED_THRESHOLD_LEN,
	ACX1xx_IE_DOT11_WEP_DEFAULT_KEY_SET_LEN,
	0,
	0,
	0,
};

static const u16 acx111_ie_len[] = {
	0,
	ACX100_IE_ACX_TIMER_LEN,
	sizeof(acx111_ie_powersave_t) - 4,
	ACX1xx_IE_QUEUE_CONFIG_LEN,
	ACX100_IE_BLOCK_SIZE_LEN,
	ACX1xx_IE_MEMORY_CONFIG_OPTIONS_LEN,
	ACX1xx_IE_RATE_FALLBACK_LEN,
	ACX100_IE_WEP_OPTIONS_LEN,
	ACX1xx_IE_MEMORY_MAP_LEN,	/*    ACX1xx_IE_SSID_LEN, */
	0,
	ACX1xx_IE_ASSOC_ID_LEN,
	0,
	ACX111_IE_CONFIG_OPTIONS_LEN,
	ACX1xx_IE_FWREV_LEN,
	ACX1xx_IE_FCS_ERROR_COUNT_LEN,
	ACX1xx_IE_MEDIUM_USAGE_LEN,
	ACX1xx_IE_RXCONFIG_LEN,
	0,
	0,
	sizeof(fw_stats_t) - 4,
	0,
	ACX1xx_IE_FEATURE_CONFIG_LEN,
	ACX111_IE_KEY_CHOOSE_LEN,
	ACX1FF_IE_MISC_CONFIG_TABLE_LEN,
	ACX1FF_IE_WONE_CONFIG_LEN,
	0,
	ACX1FF_IE_TID_CONFIG_LEN,
	0,
	0,
	0,
	ACX1FF_IE_CALIB_ASSESSMENT_LEN,
	ACX1FF_IE_BEACON_FILTER_OPTIONS_LEN,
	ACX1FF_IE_LOW_RSSI_THRESH_OPT_LEN,
	ACX1FF_IE_NOISE_HISTOGRAM_RESULTS_LEN,
	0,
	ACX1FF_IE_PACKET_DETECT_THRESH_LEN,
	ACX1FF_IE_TX_CONFIG_OPTIONS_LEN,
	ACX1FF_IE_CCA_THRESHOLD_LEN,
	ACX1FF_IE_EVENT_MASK_LEN,
	ACX1FF_IE_DTIM_PERIOD_LEN,
	0,
	ACX1FF_IE_ACI_CONFIG_SET_LEN,
	0,
	0,
	0,
	0,
	0,
	0,
	ACX1FF_IE_EEPROM_VER_LEN,
};

static const u16 acx111_ie_len_dot11[] = {
	0,
	ACX1xx_IE_DOT11_STATION_ID_LEN,
	0,
	ACX100_IE_DOT11_BEACON_PERIOD_LEN,
	ACX1xx_IE_DOT11_DTIM_PERIOD_LEN,
	ACX1xx_IE_DOT11_SHORT_RETRY_LIMIT_LEN,
	ACX1xx_IE_DOT11_LONG_RETRY_LIMIT_LEN,
	ACX100_IE_DOT11_WEP_DEFAULT_KEY_WRITE_LEN,
	ACX1xx_IE_DOT11_MAX_XMIT_MSDU_LIFETIME_LEN,
	0,
	ACX1xx_IE_DOT11_CURRENT_REG_DOMAIN_LEN,
	ACX1xx_IE_DOT11_CURRENT_ANTENNA_LEN,
	0,
	ACX1xx_IE_DOT11_TX_POWER_LEVEL_LEN,
	ACX1xx_IE_DOT11_CURRENT_CCA_MODE_LEN,
	ACX100_IE_DOT11_ED_THRESHOLD_LEN,
	ACX1xx_IE_DOT11_WEP_DEFAULT_KEY_SET_LEN,
	0,
	0,
	0,
};


#if !ACX_DEBUG
int acx_s_configure(acx_device_t * adev, void *pdr, int type)
{
#else
int
acx_s_configure_debug(acx_device_t *adev, void *pdr, int type,
		      const char *typestr)
{
#endif
	u16 len;
	int res;
	char msgbuf[255];

	FN_ENTER;

	if (type < 0x1000)
		len = adev->ie_len[type];
	else
		len = adev->ie_len_dot11[type - 0x1000];

	if (unlikely(!len)) {
		log(L_DEBUG, "acx: %s: zero-length type %s?!\n",
				__func__, typestr);
	}

	((acx_ie_generic_t *) pdr)->type = cpu_to_le16(type);
	((acx_ie_generic_t *) pdr)->len = cpu_to_le16(len);
	res = acx_s_issue_cmd(adev, ACX1xx_CMD_CONFIGURE, pdr, len + 4);

	sprintf(msgbuf, "acx: %s: %s: type=0x%04X, typestr=%s, len=%u",
			__func__, wiphy_name(adev->ieee->wiphy),
			type, typestr, len);
	if (likely(res == OK))
		log(L_CTL,  "%s: OK\n", msgbuf);
	 else
		log(L_ANY,  "%s: FAILED\n", msgbuf);

	FN_EXIT0;
	return res;
}

// OW TODO Replace the FUNC #defs by __func__ in logging
#undef FUNC
#define FUNC "interrogate"
#if !ACX_DEBUG
int acx_s_interrogate(acx_device_t * adev, void *pdr, int type)
{
#else
int
acx_s_interrogate_debug(acx_device_t * adev, void *pdr, int type,
			const char *typestr)
{
#endif
	u16 len;
	int res;

	FN_ENTER;

	/* FIXME: no check whether this exceeds the array yet.
	 * We should probably remember the number of entries... */
	if (type < 0x1000)
		len = adev->ie_len[type];
	else
		len = adev->ie_len_dot11[type - 0x1000];

	log(L_CTL, "acx: " FUNC "(type:%s,len:%u)\n", typestr, len);

	((acx_ie_generic_t *) pdr)->type = cpu_to_le16(type);
	((acx_ie_generic_t *) pdr)->len = cpu_to_le16(len);
	res = acx_s_issue_cmd(adev, ACX1xx_CMD_INTERROGATE, pdr, len + 4);
	if (unlikely(OK != res)) {
#if ACX_DEBUG
		printk("acx: %s: " FUNC "(type:%s) FAILED\n", wiphy_name(adev->ieee->wiphy),
		       typestr);
#else
		printk("acx: %s: " FUNC "(type:0x%X) FAILED\n", wiphy_name(adev->ieee->wiphy),
		       type);
#endif
		/* dump_stack() is already done in issue_cmd() */
	}

	FN_EXIT1(res);
	return res;
}

#if CMD_DISCOVERY
void great_inquisitor(acx_device_t * adev)
{
	static struct {
		u16 type;
		u16 len;
		/* 0x200 was too large here: */
		u8 data[0x100 - 4];
	} ACX_PACKED ie;
	u16 type;

	FN_ENTER;

	/* 0..0x20, 0x1000..0x1020 */
	for (type = 0; type <= 0x1020; type++) {
		if (type == 0x21)
			type = 0x1000;
		ie.type = cpu_to_le16(type);
		ie.len = cpu_to_le16(sizeof(ie) - 4);
		acx_s_issue_cmd(adev, ACX1xx_CMD_INTERROGATE, &ie, sizeof(ie));
	}
	FN_EXIT0;
}
#endif


#ifdef CONFIG_PROC_FS
/*
 * Procfs
 */

static int acx_e_proc_show_diag(struct seq_file *file, void *v)
{
	acx_device_t *adev = (acx_device_t *) file->private;

	unsigned long flags;
	ssize_t len = 0, partlen;
	u32 temp1, temp2;
	u8 *st, *st_end;
#ifdef __BIG_ENDIAN
	u8 *st2;
#endif
	fw_stats_t *fw_stats;
	char *part_str = NULL;
	fw_stats_tx_t *tx = NULL;
	fw_stats_rx_t *rx = NULL;
	fw_stats_dma_t *dma = NULL;
	fw_stats_irq_t *irq = NULL;
	fw_stats_wep_t *wep = NULL;
	fw_stats_pwr_t *pwr = NULL;
	fw_stats_mic_t *mic = NULL;
	fw_stats_aes_t *aes = NULL;
	fw_stats_event_t *evt = NULL;

	FN_ENTER;
	acx_sem_lock(adev);
	acx_lock(adev, flags);

	if (IS_PCI(adev))
		acxpci_s_proc_diag_output(file, adev);
	else if (IS_MEM(adev))
		acxmem_s_proc_diag_output(file, adev);

	seq_printf(file,
		     "\n"
		     "** network status **\n"
		     "dev_state_mask 0x%04X\n"
		     "mode %u, channel %u, "
		     "reg_dom_id 0x%02X, reg_dom_chanmask 0x%04X, ",
		     adev->dev_state_mask,
		     adev->mode, adev->channel,
		     adev->reg_dom_id, adev->reg_dom_chanmask);
	seq_printf(file,
		     "ESSID \"%s\", essid_active %d, essid_len %d, "
		     "essid_for_assoc \"%s\", nick \"%s\"\n"
		     "WEP ena %d, restricted %d, idx %d\n",
		     adev->essid, adev->essid_active, (int)adev->essid_len,
		     adev->essid_for_assoc, adev->nick,
		     adev->wep_enabled, adev->wep_restricted,
		     adev->wep_current_index);
	seq_printf(file, "dev_addr  " MACSTR "\n", MAC(adev->dev_addr));
	seq_printf(file, "bssid     " MACSTR "\n", MAC(adev->bssid));
	seq_printf(file, "ap_filter " MACSTR "\n", MAC(adev->ap));

	seq_printf(file, "\n" "** PHY status **\n"
		     "tx_disabled %d, tx_level_dbm %d\n"	/* "tx_level_val %d, tx_level_auto %d\n" */
		     "sensitivity %d, antenna 0x%02X, ed_threshold %d, cca %d, preamble_mode %d\n"
		     "rate_basic 0x%04X, rate_oper 0x%04X\n"
		     "rts_threshold %d, frag_threshold %d, short_retry %d, long_retry %d\n"
		     "msdu_lifetime %d, listen_interval %d, beacon_interval %d\n",
		     adev->tx_disabled, adev->tx_level_dbm,	/* adev->tx_level_val, adev->tx_level_auto, */
		     adev->sensitivity, adev->antenna, adev->ed_threshold,
		     adev->cca, adev->preamble_mode, adev->rate_basic, adev->rate_oper, adev->rts_threshold,
		     adev->frag_threshold, adev->short_retry, adev->long_retry,
		     adev->msdu_lifetime, adev->listen_interval,
		     adev->beacon_interval);

	acx_unlock(adev, flags);

	seq_printf(file,
		     "\n"
		     "** Firmware **\n"
		     "NOTE: version dependent statistics layout, "
		     "please report if you suspect wrong parsing!\n"
		     "\n" "version \"%s\"\n", adev->firmware_version);

	/* TODO: may replace kmalloc/memset with kzalloc once
	 * Linux 2.6.14 is widespread */
	fw_stats = kmalloc(sizeof(*fw_stats), GFP_KERNEL);
	if (!fw_stats) {
		FN_EXIT1(0);
		return 0;
	}
	memset(fw_stats, 0, sizeof(*fw_stats));

	st = (u8 *) fw_stats;

	part_str = "statistics query command";

	if (OK != acx_s_interrogate(adev, st, ACX1xx_IE_FIRMWARE_STATISTICS))
		goto fw_stats_end;

	st += sizeof(u16);
	len = *(u16 *) st;

	if (len > sizeof(*fw_stats)) {
		seq_printf(file,
			     "firmware version with bigger fw_stats struct detected\n"
			     "(%zu vs. %zu), please report\n", len, sizeof(fw_stats_t));
		if (len > sizeof(*fw_stats)) {
			seq_printf(file, "struct size exceeded allocation!\n");
			len = sizeof(*fw_stats);
		}
	}
	st += sizeof(u16);
	st_end = st - 2 * sizeof(u16) + len;

#ifdef __BIG_ENDIAN
	/* let's make one bold assumption here:
	 * (hopefully!) *all* statistics fields are u32 only,
	 * thus if we need to make endianness corrections
	 * we can simply do them in one go, in advance */
	st2 = (u8 *) fw_stats;
	for (temp1 = 0; temp1 < len; temp1 += 4, st2 += 4)
		*(u32 *) st2 = le32_to_cpu(*(u32 *) st2);
#endif

	part_str = "Rx/Tx";

	/* directly at end of a struct part? --> no error! */
	if (st == st_end)
		goto fw_stats_end;

	tx = (fw_stats_tx_t *) st;
	st += sizeof(fw_stats_tx_t);
	rx = (fw_stats_rx_t *) st;
	st += sizeof(fw_stats_rx_t);
	partlen = sizeof(fw_stats_tx_t) + sizeof(fw_stats_rx_t);

	if (IS_ACX100(adev)) {
		/* at least ACX100 PCI F/W 1.9.8.b
		 * and ACX100 USB F/W 1.0.7-USB
		 * don't have those two fields... */
		st -= 2 * sizeof(u32);

		/* our parsing doesn't quite match this firmware yet,
		 * log failure */
		if (st > st_end)
			goto fw_stats_fail;
		temp1 = temp2 = 999999999;
	} else {
		if (st > st_end)
			goto fw_stats_fail;
		temp1 = rx->rx_aci_events;
		temp2 = rx->rx_aci_resets;
	}

	seq_printf(file,
		     "%s:\n"
		     "  tx_desc_overfl %u\n"
		     "  rx_OutOfMem %u, rx_hdr_overfl %u, rx_hw_stuck %u\n"
		     "  rx_dropped_frame %u, rx_frame_ptr_err %u, rx_xfr_hint_trig %u\n"
		     "  rx_aci_events %u, rx_aci_resets %u\n",
		     part_str,
		     tx->tx_desc_of,
		     rx->rx_oom,
		     rx->rx_hdr_of,
		     rx->rx_hw_stuck,
		     rx->rx_dropped_frame,
		     rx->rx_frame_ptr_err, rx->rx_xfr_hint_trig, temp1, temp2);

	part_str = "DMA";

	if (st == st_end)
		goto fw_stats_end;

	dma = (fw_stats_dma_t *) st;
	partlen = sizeof(fw_stats_dma_t);
	st += partlen;

	if (st > st_end)
		goto fw_stats_fail;

	seq_printf(file,
		     "%s:\n"
		     "  rx_dma_req %u, rx_dma_err %u, tx_dma_req %u, tx_dma_err %u\n",
		     part_str,
		     dma->rx_dma_req,
		     dma->rx_dma_err, dma->tx_dma_req, dma->tx_dma_err);

	part_str = "IRQ";

	if (st == st_end)
		goto fw_stats_end;

	irq = (fw_stats_irq_t *) st;
	partlen = sizeof(fw_stats_irq_t);
	st += partlen;

	if (st > st_end)
		goto fw_stats_fail;

	seq_printf(file,
		     "%s:\n"
		     "  cmd_cplt %u, fiq %u\n"
		     "  rx_hdrs %u, rx_cmplt %u, rx_mem_overfl %u, rx_rdys %u\n"
		     "  irqs %u, tx_procs %u, decrypt_done %u\n"
		     "  dma_0_done %u, dma_1_done %u, tx_exch_complet %u\n"
		     "  commands %u, rx_procs %u, hw_pm_mode_changes %u\n"
		     "  host_acks %u, pci_pm %u, acm_wakeups %u\n",
		     part_str,
		     irq->cmd_cplt,
		     irq->fiq,
		     irq->rx_hdrs,
		     irq->rx_cmplt,
		     irq->rx_mem_of,
		     irq->rx_rdys,
		     irq->irqs,
		     irq->tx_procs,
		     irq->decrypt_done,
		     irq->dma_0_done,
		     irq->dma_1_done,
		     irq->tx_exch_complet,
		     irq->commands,
		     irq->rx_procs,
		     irq->hw_pm_mode_changes,
		     irq->host_acks, irq->pci_pm, irq->acm_wakeups);

	part_str = "WEP";

	if (st == st_end)
		goto fw_stats_end;

	wep = (fw_stats_wep_t *) st;
	partlen = sizeof(fw_stats_wep_t);
	st += partlen;

	if (IS_ACX100(adev)) {
		/* at least ACX100 PCI F/W 1.9.8.b
		 * and ACX100 USB F/W 1.0.7-USB
		 * don't have those two fields... */
		st -= 2 * sizeof(u32);
		if (st > st_end)
			goto fw_stats_fail;
		temp1 = temp2 = 999999999;
	} else {
		if (st > st_end)
			goto fw_stats_fail;
		temp1 = wep->wep_pkt_decrypt;
		temp2 = wep->wep_decrypt_irqs;
	}

	seq_printf(file,
		     "%s:\n"
		     "  wep_key_count %u, wep_default_key_count %u, dot11_def_key_mib %u\n"
		     "  wep_key_not_found %u, wep_decrypt_fail %u\n"
		     "  wep_pkt_decrypt %u, wep_decrypt_irqs %u\n",
		     part_str,
		     wep->wep_key_count,
		     wep->wep_default_key_count,
		     wep->dot11_def_key_mib,
		     wep->wep_key_not_found,
		     wep->wep_decrypt_fail, temp1, temp2);

	part_str = "power";

	if (st == st_end)
		goto fw_stats_end;

	pwr = (fw_stats_pwr_t *) st;
	partlen = sizeof(fw_stats_pwr_t);
	st += partlen;

	if (st > st_end)
		goto fw_stats_fail;

	seq_printf(file,
		     "%s:\n"
		     "  tx_start_ctr %u, no_ps_tx_too_short %u\n"
		     "  rx_start_ctr %u, no_ps_rx_too_short %u\n"
		     "  lppd_started %u\n"
		     "  no_lppd_too_noisy %u, no_lppd_too_short %u, no_lppd_matching_frame %u\n",
		     part_str,
		     pwr->tx_start_ctr,
		     pwr->no_ps_tx_too_short,
		     pwr->rx_start_ctr,
		     pwr->no_ps_rx_too_short,
		     pwr->lppd_started,
		     pwr->no_lppd_too_noisy,
		     pwr->no_lppd_too_short, pwr->no_lppd_matching_frame);

	part_str = "MIC";

	if (st == st_end)
		goto fw_stats_end;

	mic = (fw_stats_mic_t *) st;
	partlen = sizeof(fw_stats_mic_t);
	st += partlen;

	if (st > st_end)
		goto fw_stats_fail;

	seq_printf(file,
		     "%s:\n"
		     "  mic_rx_pkts %u, mic_calc_fail %u\n",
		     part_str, mic->mic_rx_pkts, mic->mic_calc_fail);

	part_str = "AES";

	if (st == st_end)
		goto fw_stats_end;

	aes = (fw_stats_aes_t *) st;
	partlen = sizeof(fw_stats_aes_t);
	st += partlen;

	if (st > st_end)
		goto fw_stats_fail;

	seq_printf(file,
		     "%s:\n"
		     "  aes_enc_fail %u, aes_dec_fail %u\n"
		     "  aes_enc_pkts %u, aes_dec_pkts %u\n"
		     "  aes_enc_irq %u, aes_dec_irq %u\n",
		     part_str,
		     aes->aes_enc_fail,
		     aes->aes_dec_fail,
		     aes->aes_enc_pkts,
		     aes->aes_dec_pkts, aes->aes_enc_irq, aes->aes_dec_irq);

	part_str = "event";

	if (st == st_end)
		goto fw_stats_end;

	evt = (fw_stats_event_t *) st;
	partlen = sizeof(fw_stats_event_t);
	st += partlen;

	if (st > st_end)
		goto fw_stats_fail;

	seq_printf(file,
		     "%s:\n"
		     "  heartbeat %u, calibration %u\n"
		     "  rx_mismatch %u, rx_mem_empty %u, rx_pool %u\n"
		     "  oom_late %u\n"
		     "  phy_tx_err %u, tx_stuck %u\n",
		     part_str,
		     evt->heartbeat,
		     evt->calibration,
		     evt->rx_mismatch,
		     evt->rx_mem_empty,
		     evt->rx_pool,
		     evt->oom_late, evt->phy_tx_err, evt->tx_stuck);

	if (st < st_end)
		goto fw_stats_bigger;

	goto fw_stats_end;

	fw_stats_fail:
	st -= partlen;
	seq_printf(file,
			"failed at %s part (size %zu), offset %zu (struct size %zu), "
				"please report\n", part_str, partlen, ((void *) st
					- (void *) fw_stats), len);

	fw_stats_bigger:
	for (; st < st_end; st += 4)
		seq_printf(file, "UNKN%3d: %u\n", (int) ((void *) st
				- (void *) fw_stats), *(u32 *) st);

	fw_stats_end:
	kfree(fw_stats);

	acx_sem_unlock(adev);
	FN_EXIT0;
	return 0;
}


/***********************************************************************
** acx_e_read_proc_XXXX
** Handle our /proc entry
**
** Arguments:
**	standard kernel read_proc interface
** Returns:
**	number of bytes written to buf
** Side effects:
**	none
*/
static int acx_e_proc_show_acx(struct seq_file *file, void *v)
{
	acx_device_t *adev = (acx_device_t*) file->private;

	FN_ENTER;
	acx_sem_lock(adev);

	seq_printf(file,
		     "acx driver version:\t\t" ACX_RELEASE "\n"
		     "Wireless extension version:\t" STRING(WIRELESS_EXT) "\n"
		     "chip name:\t\t\t%s (0x%08X)\n"
		     "radio type:\t\t\t0x%02X\n"
		     "form factor:\t\t\t0x%02X\n"
		     "EEPROM version:\t\t\t0x%02X\n"
		     "firmware version:\t\t%s (0x%08X)\n",
		     adev->chip_name, adev->firmware_id,
		     adev->radio_type,
		     adev->form_factor,
		     adev->eeprom_version,
		     adev->firmware_version, adev->firmware_numver);

	acx_sem_unlock(adev);
	FN_EXIT0;
	return 0;
}

static int acx_e_proc_show_eeprom(struct seq_file *file, void *v)
{
	acx_device_t *adev = (acx_device_t *) file->private;

	int length;
	char *buf, *buf2;
	// OW Hopefully enough
	const int buf_size = 1024*64;

	FN_ENTER;
	acx_sem_lock(adev);

	buf = kmalloc(buf_size, GFP_KERNEL);

	/* fill buf */
	length = 0;
	if (IS_PCI(adev))
		length = acxpci_proc_eeprom_output(buf, adev);
	else if (IS_MEM(adev))
		length = acxmem_proc_eeprom_output(buf, adev);
	else
		goto out;

	for (buf2 = buf; buf2 < buf + length; seq_putc(file, *(buf2++)))
		;

	out:
	kfree(buf);

	acx_sem_unlock(adev);
	FN_EXIT0;
	return 0;

}

static int acx_e_proc_show_phy(struct seq_file *file, void *v)
{
	acx_device_t *adev = (acx_device_t *) file->private;

	int i;
	char *buf, *p;
	// OW Hopefully enough
	const int buf_size = 1024*64;

	FN_ENTER;
	acx_sem_lock(adev);

	buf = kmalloc(buf_size, GFP_KERNEL);
	/*
	   if (RADIO_RFMD_11 != adev->radio_type) {
	   printk("acx: sorry, not yet adapted for radio types "
	   "other than RFMD, please verify "
	   "PHY size etc. first!\n");
	   goto end;
	   }
	 */

	/* The PHY area is only 0x80 bytes long; further pages after that
	 * only have some page number registers with altered value,
	 * all other registers remain the same. */
	p = buf;
	for (i = 0; i < 0x80; i++) {
		acx_s_read_phy_reg(adev, i, p++);
		seq_putc(file, *p);
	}

	kfree(buf);

	acx_sem_unlock(adev);
	FN_EXIT0;

	return 0;
}

static int acx_e_proc_show_debug(struct seq_file *file, void *v)
{
	FN_ENTER;
	// No sem locking required, since debug is global for all devices

	seq_printf(file, "acx_debug: 0x%04x\n", acx_debug);

	FN_EXIT0;

	return 0;
}

static ssize_t acx_e_proc_write_debug(struct file *file, const char __user *buf,
				   size_t count, loff_t *ppos)
{
	ssize_t ret = -EINVAL;
	char *after;
	unsigned long val = simple_strtoul(buf, &after, 0);
	size_t size = after - buf + 1;

	FN_ENTER;
	// No sem locking required, since debug is global for all devices

	if (count == size) {
		ret = count;
		acx_debug = val;
	}

	log(L_ANY, "%s: acx_debug=0x%04x\n", __func__, acx_debug);

	FN_EXIT0;
	return ret;

}

/*
 * proc files registration
 */
static const char *const
 proc_files[] = { "acx", "acx_diag", "acx_eeprom", "acx_phy", "acx_debug" };

typedef int acx_proc_show_t(struct seq_file *file, void *v);
typedef ssize_t (acx_proc_write_t)(struct file *, const char __user *, size_t, loff_t *);

static acx_proc_show_t *const
 acx_proc_show_funcs[] = {
	acx_e_proc_show_acx,
	acx_e_proc_show_diag,
	acx_e_proc_show_eeprom,
	acx_e_proc_show_phy,
	acx_e_proc_show_debug,
};

static acx_proc_write_t *const
 acx_proc_write_funcs[] = {
	NULL,
	NULL,
	NULL,
	NULL,
	acx_e_proc_write_debug,
};

static struct file_operations acx_e_proc_ops[5] ;

static int acx_e_proc_open(struct inode *inode, struct file *file)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(proc_files); i++) {
		if (strcmp(proc_files[i], file->f_path.dentry->d_name.name) == 0) break;
	}
	// log(L_ANY, "%s: proc filename=%s\n", __func__, proc_files[i]);

	return single_open(file, acx_proc_show_funcs[i], PDE(inode)->data);
}

static int acx_manage_proc_entries(struct ieee80211_hw *hw, int num, int remove)
{
	acx_device_t *adev = ieee2adev(hw);
	char procbuf[80];
	char procbuf2[80];
	int i;
	struct proc_dir_entry *pe;
	struct proc_dir_entry *ppe;

	FN_ENTER;

	// Create a subdir for this acx instance
	snprintf(procbuf2, sizeof(procbuf), "driver/acx%i", num);

	if (!remove) {

		ppe = proc_mkdir(procbuf2, NULL);

		for (i = 0; i < ARRAY_SIZE(proc_files); i++) {
			snprintf(procbuf, sizeof(procbuf), "%s/%s", procbuf2, proc_files[i]);
			log(L_INIT, "acx: %sing /proc entry %s\n",
					remove ? "remov" : "creat", procbuf);

			acx_e_proc_ops[i].owner = THIS_MODULE;
			acx_e_proc_ops[i].open = acx_e_proc_open;
			acx_e_proc_ops[i].read = seq_read;
			acx_e_proc_ops[i].llseek = seq_lseek;
			acx_e_proc_ops[i].release = single_release;
			acx_e_proc_ops[i].write = acx_proc_write_funcs[i];

			// Read-only
			if (acx_proc_write_funcs[i] == NULL)
				pe = proc_create(procbuf, 0444, NULL, &acx_e_proc_ops[i]);
			// Read-Write
			else
				pe = proc_create(procbuf, 0644, NULL, &acx_e_proc_ops[i]);

			if (!pe) {
				printk("acx: cannot register /proc entry %s\n", procbuf);
				return NOT_OK;
			}
			pe->data = adev;

		}

	} else {

		for (i = 0; i < ARRAY_SIZE(proc_files); i++) {
			snprintf(procbuf, sizeof(procbuf), "%s/%s", procbuf2, proc_files[i]);
			remove_proc_entry(procbuf, NULL);
		}
		remove_proc_entry(procbuf2, NULL);

	}

	FN_EXIT0;
	return OK;
}

int acx_proc_register_entries(struct ieee80211_hw *ieee, int num)
{
	return acx_manage_proc_entries(ieee, num, 0);
}

int acx_proc_unregister_entries(struct ieee80211_hw *ieee, int num)
{
	return acx_manage_proc_entries(ieee, num, 1);
}
#endif /* CONFIG_PROC_FS */

/****
** Gathered From rt2x00 and bcm43xx_mac80211 projects
**/
void acx_free_modes(acx_device_t * adev)
{

//        kfree(adev->modes);
//        adev->modes = NULL;
}

/*
#define RATETAB_ENT(_rate, _rateid, _flags) \
	{							\
		.rate	= (_rate),				\
		.val	= (_rateid),				\
		.val2   = (_rateid),				\
		.flags  = (_flags),				\
	}
*/

static struct ieee80211_rate __acx_rates[] = {
	{ .bitrate = 10, .hw_value = 0, .flags = IEEE80211_RATE_SHORT_PREAMBLE },
	{ .bitrate = 20, .hw_value = 1, .flags = IEEE80211_RATE_SHORT_PREAMBLE },
	{ .bitrate = 55, .hw_value = 2, .flags = IEEE80211_RATE_SHORT_PREAMBLE },
	{ .bitrate = 110, .hw_value = 3, .flags = IEEE80211_RATE_SHORT_PREAMBLE },
	{ .bitrate = 60, .hw_value = 4, },
	{ .bitrate = 90, .hw_value = 5, },
	{ .bitrate = 120, .hw_value = 6, },
	{ .bitrate = 180, .hw_value = 7, },
	{ .bitrate = 240, .hw_value = 8, },
	{ .bitrate = 360, .hw_value = 9, },
	{ .bitrate = 480, .hw_value = 10, },
	{ .bitrate = 540, .hw_value = 11, },
};

static struct ieee80211_channel channels[] = {
	{ .center_freq = 2412, .hw_value = 1, },
	{ .center_freq = 2417, .hw_value = 2, },
	{ .center_freq = 2422, .hw_value = 3, },
	{ .center_freq = 2427, .hw_value = 4, },
	{ .center_freq = 2432, .hw_value = 5, },
	{ .center_freq = 2437, .hw_value = 6, },
	{ .center_freq = 2442, .hw_value = 7, },
	{ .center_freq = 2447, .hw_value = 8, },
	{ .center_freq = 2452, .hw_value = 9, },
	{ .center_freq = 2457, .hw_value = 10, },
	{ .center_freq = 2462, .hw_value = 11, },
	{ .center_freq = 2467, .hw_value = 12, },
	{ .center_freq = 2472, .hw_value = 13, },
	{ .center_freq = 2484, .hw_value = 14, },
};

static struct ieee80211_supported_band g_band_2GHz = {
	.channels = channels,
	.n_channels = ARRAY_SIZE(channels),
	.bitrates = __acx_rates,
	.n_bitrates = 12,
};

static struct ieee80211_supported_band b_band_2GHz = {
	.channels = channels,
	.n_channels = ARRAY_SIZE(channels),
	.bitrates = __acx_rates,
	.n_bitrates = 4,
};

int acx_setup_modes(acx_device_t *adev)
{
	struct ieee80211_hw *hw = adev->ieee;

	FN_ENTER;

	if (IS_ACX111(adev) || IS_ACX100(adev)) {
/*
		adev->modes = kzalloc(sizeof(struct ieee80211_hw_mode) * 2, GFP_KERNEL);
        	err = acx_setup_modes_gphy(adev);
*/
		hw->wiphy->bands[IEEE80211_BAND_2GHZ] = &g_band_2GHz;
	}
	// OX FIXME This is probably not very logic ?!
	else
	{
/*
		adev->modes = kzalloc(sizeof(struct ieee80211_hw_mode), GFP_KERNEL);
		err = acx_setup_modes_bphy(adev);
*/
		hw->wiphy->bands[IEEE80211_BAND_2GHZ] = &b_band_2GHz;
	}
/*	if (err && adev->modes)
		kfree(adev->modes);*/

	FN_EXIT0;
	return 0;

}

/***********************************************************************
** acx_fill_beacon_or_proberesp_template
**
** Origin: derived from rt2x00 project
*/
static int
acx_fill_beacon_or_proberesp_template(acx_device_t *adev,
                                        struct acx_template_beacon *templ,
                                        struct sk_buff* skb /* in host order! */)
{
        FN_ENTER;

        memcpy(templ,skb->data, skb->len);
        FN_EXIT1(skb->len);
        return skb->len;
}

/***********************************************************************
** acx_s_set_beacon_template
**
**
*/
static int
acx_s_set_beacon_template(acx_device_t *adev, struct sk_buff *skb)
{
        struct acx_template_beacon bcn;
        int len, result;

        FN_ENTER;
	printk("acx: Size of template: %08zX, Size of beacon: %08X\n", sizeof(struct acx_template_beacon),skb->len);
        len = acx_fill_beacon_or_proberesp_template(adev, &bcn, skb);
        result = acx_s_issue_cmd(adev, ACX1xx_CMD_CONFIG_BEACON, &bcn, len);

        FN_EXIT1(result);
        return result;
}

/***********************************************************************
** acx_cmd_join_bssid
**
** Common code for both acx100 and acx111.
*/
/* NB: does NOT match RATE100_nn but matches ACX[111]_SCAN_RATE_n */
static const u8 bitpos2genframe_txrate[] = {
	10,			/*  0.  1 Mbit/s */
	20,			/*  1.  2 Mbit/s */
	55,			/*  2.  5.5 Mbit/s */
	0x0B,			/*  3.  6 Mbit/s */
	0x0F,			/*  4.  9 Mbit/s */
	110,			/*  5. 11 Mbit/s */
	0x0A,			/*  6. 12 Mbit/s */
	0x0E,			/*  7. 18 Mbit/s */
	220,			/*  8. 22 Mbit/s */
	0x09,			/*  9. 24 Mbit/s */
	0x0D,			/* 10. 36 Mbit/s */
	0x08,			/* 11. 48 Mbit/s */
	0x0C,			/* 12. 54 Mbit/s */
	10,			/* 13.  1 Mbit/s, should never happen */
	10,			/* 14.  1 Mbit/s, should never happen */
	10,			/* 15.  1 Mbit/s, should never happen */
};

/* Looks scary, eh?
** Actually, each one compiled into one AND and one SHIFT,
** 31 bytes in x86 asm (more if uints are replaced by u16/u8) */
static inline unsigned int rate111to5bits(unsigned int rate)
{
	return (rate & 0x7)
	    | ((rate & RATE111_11) / (RATE111_11 / JOINBSS_RATES_11))
	    | ((rate & RATE111_22) / (RATE111_22 / JOINBSS_RATES_22));
}


void acx_s_cmd_join_bssid(acx_device_t *adev, const u8 *bssid)
{
        acx_joinbss_t tmp;
        int dtim_interval;
        int i;

        if (mac_is_zero(bssid))
                return;

        FN_ENTER;

        dtim_interval = (ACX_MODE_0_ADHOC == adev->mode) ?
                        1 : adev->dtim_interval;

        memset(&tmp, 0, sizeof(tmp));

        for (i = 0; i < ETH_ALEN; i++) {
                tmp.bssid[i] = bssid[ETH_ALEN-1 - i];
        }

        tmp.beacon_interval = cpu_to_le16(adev->beacon_interval);

        /* Basic rate set. Control frame responses (such as ACK or CTS frames)
        ** are sent with one of these rates */
        if (IS_ACX111(adev)) {
                /* It was experimentally determined that rates_basic
                ** can take 11g rates as well, not only rates
                ** defined with JOINBSS_RATES_BASIC111_nnn.
                ** Just use RATE111_nnn constants... */
                tmp.u.acx111.dtim_interval = dtim_interval;
                tmp.u.acx111.rates_basic = cpu_to_le16(adev->rate_basic);
                log(L_ASSOC, "acx: rates_basic:%04X, rates_supported:%04X\n",
                        adev->rate_basic, adev->rate_oper);
        } else {
                tmp.u.acx100.dtim_interval = dtim_interval;
                tmp.u.acx100.rates_basic = rate111to5bits(adev->rate_basic);
                tmp.u.acx100.rates_supported = rate111to5bits(adev->rate_oper);
                log(L_ASSOC, "acx: rates_basic:%04X->%02X, "
                        "rates_supported:%04X->%02X\n",
                        adev->rate_basic, tmp.u.acx100.rates_basic,
                        adev->rate_oper, tmp.u.acx100.rates_supported);
        }

        /* Setting up how Beacon, Probe Response, RTS, and PS-Poll frames
        ** will be sent (rate/modulation/preamble) */
        tmp.genfrm_txrate = bitpos2genframe_txrate[lowest_bit(adev->rate_basic)];
        tmp.genfrm_mod_pre = 0; /* FIXME: was = adev->capab_short (which was always 0); */
        /* we can use short pre *if* all peers can understand it */
        /* FIXME #2: we need to correctly set PBCC/OFDM bits here too */

        /* we switch fw to STA mode in MONITOR mode, it seems to be
        ** the only mode where fw does not emit beacons by itself
        ** but allows us to send anything (we really want to retain
        ** ability to tx arbitrary frames in MONITOR mode)
        */
        tmp.macmode = (adev->mode != ACX_MODE_MONITOR ? adev->mode : ACX_MODE_2_STA);
        tmp.channel = adev->channel;
        tmp.essid_len = adev->essid_len;

        memcpy(tmp.essid, adev->essid, tmp.essid_len);
        acx_s_issue_cmd(adev, ACX1xx_CMD_JOIN, &tmp, tmp.essid_len + 0x11);

        log(L_ASSOC|L_DEBUG, "acx: BSS_Type = %u\n", tmp.macmode);
        acxlog_mac(L_ASSOC|L_DEBUG, "acx: JoinBSSID MAC:", adev->bssid, "\n");

/*        acx_update_capabilities(adev); */
        FN_EXIT0;
}

/***********************************************************************
** acxpci_i_set_multicast_list
** **
** FIXME: most likely needs refinement
*/

void acx_i_op_configure_filter(struct ieee80211_hw *hw,
		unsigned int changed_flags, unsigned int *total_flags, u64 multicast) {

	acx_device_t *adev = ieee2adev(hw);
	unsigned long flags;

	FN_ENTER;

	acx_lock(adev, flags);

	changed_flags &= (FIF_PROMISC_IN_BSS | FIF_ALLMULTI | FIF_FCSFAIL
			| FIF_CONTROL | FIF_OTHER_BSS);
	*total_flags &= (FIF_PROMISC_IN_BSS | FIF_ALLMULTI | FIF_FCSFAIL
			| FIF_CONTROL | FIF_OTHER_BSS);
	/*        if ((changed_flags & (FIF_PROMISC_IN_BSS | FIF_ALLMULTI)) == 0)
	 return; */

	if (*total_flags) {
		SET_BIT(adev->rx_config_1, RX_CFG1_RCV_PROMISCUOUS);
		CLEAR_BIT(adev->rx_config_1, RX_CFG1_FILTER_ALL_MULTI);
		SET_BIT(adev->set_mask, SET_RXCONFIG);
		/* let kernel know in case *we* needed to set promiscuous */
	} else {
		CLEAR_BIT(adev->rx_config_1, RX_CFG1_RCV_PROMISCUOUS);
		SET_BIT(adev->rx_config_1, RX_CFG1_FILTER_ALL_MULTI);
		SET_BIT(adev->set_mask, SET_RXCONFIG);
	}

	/* cannot update card settings directly here, atomic context */
	// TODO This is one point to check for better cmd and interrupt handling
	acx_schedule_task(adev, ACX_AFTER_IRQ_UPDATE_CARD_CFG);

	acx_unlock(adev, flags);

	//acx_s_update_card_settings(adev);

	FN_EXIT0;
}

/***********************************************************************
** acx111 feature config
**
** Obvious
*/
static int
acx111_s_get_feature_config(acx_device_t * adev,
			    u32 * feature_options, u32 * data_flow_options)
{
	struct acx111_ie_feature_config feat;

	FN_ENTER;

	if (!IS_ACX111(adev)) {
		return NOT_OK;
	}

	memset(&feat, 0, sizeof(feat));

	if (OK != acx_s_interrogate(adev, &feat, ACX1xx_IE_FEATURE_CONFIG)) {
		FN_EXIT1(NOT_OK);
		return NOT_OK;
	}
	log(L_DEBUG,
	    "acx: got Feature option:0x%X, DataFlow option: 0x%X\n",
	    feat.feature_options, feat.data_flow_options);

	if (feature_options)
		*feature_options = le32_to_cpu(feat.feature_options);
	if (data_flow_options)
		*data_flow_options = le32_to_cpu(feat.data_flow_options);

	FN_EXIT0;
	return OK;
}


static int
acx111_s_set_feature_config(acx_device_t * adev,
			    u32 feature_options, u32 data_flow_options,
			    unsigned int mode
			    /* 0 == remove, 1 == add, 2 == set */ )
{
	struct acx111_ie_feature_config feat;

	int i;

	FN_ENTER;

	if (!IS_ACX111(adev)) {
		FN_EXIT1(NOT_OK);
		return NOT_OK;
	}

	if ((mode < 0) || (mode > 2)) {
		FN_EXIT1(NOT_OK);
		return NOT_OK;
	}

	if (mode != 2)	{
		/* need to modify old data */
		i = acx111_s_get_feature_config(adev, &feat.feature_options,
				&feat.data_flow_options);
		if (i != OK)
		{
			printk("%s: acx111_s_get_feature_config: NOT_OK\n", __FUNCTION__);
			return i;
		}
	}
	else {
		/* need to set a completely new value */
		feat.feature_options = 0;
		feat.data_flow_options = 0;
	}

	if (mode == 0) {	/* remove */
		CLEAR_BIT(feat.feature_options, cpu_to_le32(feature_options));
		CLEAR_BIT(feat.data_flow_options,
			  cpu_to_le32(data_flow_options));
	} else {		/* add or set */
		SET_BIT(feat.feature_options, cpu_to_le32(feature_options));
		SET_BIT(feat.data_flow_options, cpu_to_le32(data_flow_options));
	}

	log(L_DEBUG,
	    "acx: old: feature 0x%08X dataflow 0x%08X. mode: %u\n"
	    "acx: new: feature 0x%08X dataflow 0x%08X\n",
	    feature_options, data_flow_options, mode,
	    le32_to_cpu(feat.feature_options),
	    le32_to_cpu(feat.data_flow_options));

	if (OK != acx_s_configure(adev, &feat, ACX1xx_IE_FEATURE_CONFIG)) {
		FN_EXIT1(NOT_OK);
		return NOT_OK;
	}

	FN_EXIT0;
	return OK;
}

static inline int acx111_s_feature_off(acx_device_t * adev, u32 f, u32 d)
{
	return acx111_s_set_feature_config(adev, f, d, 0);
}
static inline int acx111_s_feature_on(acx_device_t * adev, u32 f, u32 d)
{
	return acx111_s_set_feature_config(adev, f, d, 1);
}
static inline int acx111_s_feature_set(acx_device_t * adev, u32 f, u32 d)
{
	return acx111_s_set_feature_config(adev, f, d, 2);
}


/***********************************************************************
** acx100_s_init_memory_pools
*/
static int
acx100_s_init_memory_pools(acx_device_t * adev, const acx_ie_memmap_t * mmt)
{
	acx100_ie_memblocksize_t MemoryBlockSize;
	acx100_ie_memconfigoption_t MemoryConfigOption;
	int TotalMemoryBlocks;
	int RxBlockNum;
	int TotalRxBlockSize;
	int TxBlockNum;
	int TotalTxBlockSize;

	FN_ENTER;

	/* Let's see if we can follow this:
	   first we select our memory block size (which I think is
	   completely arbitrary) */
	MemoryBlockSize.size = cpu_to_le16(adev->memblocksize);

	/* Then we alert the card to our decision of block size */
	if (OK != acx_s_configure(adev, &MemoryBlockSize, ACX100_IE_BLOCK_SIZE)) {
		goto bad;
	}

	/* We figure out how many total blocks we can create, using
	   the block size we chose, and the beginning and ending
	   memory pointers, i.e.: end-start/size */
	TotalMemoryBlocks =
	    (le32_to_cpu(mmt->PoolEnd) -
	     le32_to_cpu(mmt->PoolStart)) / adev->memblocksize;

	log(L_ANY, "acx: TotalMemoryBlocks=%u (%u bytes)\n",
	    TotalMemoryBlocks, TotalMemoryBlocks * adev->memblocksize);

	/* MemoryConfigOption.DMA_config bitmask:
	   access to ACX memory is to be done:
	   0x00080000   using PCI conf space?!
	   0x00040000   using IO instructions?
	   0x00000000   using memory access instructions
	   0x00020000   using local memory block linked list (else what?)
	   0x00010000   using host indirect descriptors (else host must access ACX memory?)
	 */
	if (IS_PCI(adev)) {
		MemoryConfigOption.DMA_config = cpu_to_le32(0x30000);
		/* Declare start of the Rx host pool */
		MemoryConfigOption.pRxHostDesc =
		    cpu2acx(adev->rxhostdesc_startphy);
		log(L_DEBUG, "acx: pRxHostDesc 0x%08X, rxhostdesc_startphy 0x%lX\n",
		    acx2cpu(MemoryConfigOption.pRxHostDesc),
		    (long)adev->rxhostdesc_startphy);
	}
	else if(IS_MEM(adev)) {
		/*
		 * ACX ignores DMA_config for generic slave mode.
		 */
		MemoryConfigOption.DMA_config = 0;
		/* Declare start of the Rx host pool */
		MemoryConfigOption.pRxHostDesc = cpu2acx(0);
		log(L_DEBUG, "pRxHostDesc 0x%08X, rxhostdesc_startphy 0x%lX\n",
			acx2cpu(MemoryConfigOption.pRxHostDesc),
			(long)adev->rxhostdesc_startphy);
	}
	else {
		MemoryConfigOption.DMA_config = cpu_to_le32(0x20000);
	}

	/* 50% of the allotment of memory blocks go to tx descriptors */
	TxBlockNum = TotalMemoryBlocks / 2;
	MemoryConfigOption.TxBlockNum = cpu_to_le16(TxBlockNum);

	/* and 50% go to the rx descriptors */
	RxBlockNum = TotalMemoryBlocks - TxBlockNum;
	MemoryConfigOption.RxBlockNum = cpu_to_le16(RxBlockNum);

	/* size of the tx and rx descriptor queues */
	TotalTxBlockSize = TxBlockNum * adev->memblocksize;
	TotalRxBlockSize = RxBlockNum * adev->memblocksize;
	log(L_DEBUG, "acx: TxBlockNum %u RxBlockNum %u TotalTxBlockSize %u "
	    "TotalTxBlockSize %u\n", TxBlockNum, RxBlockNum,
	    TotalTxBlockSize, TotalRxBlockSize);


	/* align the tx descriptor queue to an alignment of 0x20 (32 bytes) */
	MemoryConfigOption.rx_mem =
	    cpu_to_le32((le32_to_cpu(mmt->PoolStart) + 0x1f) & ~0x1f);

	/* align the rx descriptor queue to units of 0x20
	 * and offset it by the tx descriptor queue */
	MemoryConfigOption.tx_mem =
	    cpu_to_le32((le32_to_cpu(mmt->PoolStart) + TotalRxBlockSize +
			 0x1f) & ~0x1f);
	log(L_DEBUG, "acx: rx_mem %08X rx_mem %08X\n", MemoryConfigOption.tx_mem,
	    MemoryConfigOption.rx_mem);

	/* alert the device to our decision */
	if (OK !=
	    acx_s_configure(adev, &MemoryConfigOption,
			    ACX1xx_IE_MEMORY_CONFIG_OPTIONS)) {
		goto bad;
	}

	/* and tell the device to kick it into gear */
	if (OK != acx_s_issue_cmd(adev, ACX100_CMD_INIT_MEMORY, NULL, 0)) {
		goto bad;
	}

#ifdef CONFIG_ACX_MAC80211_MEM
	/*
	 * slave memory interface has to manage the transmit pools for the ACX,
	 * so it needs to know what we chose here.
	 */
	adev->acx_txbuf_start = MemoryConfigOption.tx_mem;
	adev->acx_txbuf_numblocks = MemoryConfigOption.TxBlockNum;
#endif


	FN_EXIT1(OK);
	return OK;
      bad:
	FN_EXIT1(NOT_OK);
	return NOT_OK;
}


/***********************************************************************
** acx100_s_create_dma_regions
**
** Note that this fn messes up heavily with hardware, but we cannot
** lock it (we need to sleep). Not a problem since IRQs can't happen
*/
/* OLD CODE? - let's rewrite it! */
static int acx100_s_create_dma_regions(acx_device_t * adev)
{
	acx100_ie_queueconfig_t queueconf;
	acx_ie_memmap_t memmap;
	int res = NOT_OK;
	u32 tx_queue_start, rx_queue_start;

	FN_ENTER;

	/* read out the acx100 physical start address for the queues */
	if (OK != acx_s_interrogate(adev, &memmap, ACX1xx_IE_MEMORY_MAP)) {
		goto fail;
	}

	tx_queue_start = le32_to_cpu(memmap.QueueStart);
	rx_queue_start = tx_queue_start + TX_CNT * sizeof(txdesc_t);

	log(L_DEBUG, "acx: initializing Queue Indicator\n");

	memset(&queueconf, 0, sizeof(queueconf));

	/* Not needed for PCI, so we can avoid setting them altogether */
	if (IS_USB(adev)) {
		queueconf.NumTxDesc = USB_TX_CNT;
		queueconf.NumRxDesc = USB_RX_CNT;
	}

	/* calculate size of queues */
	queueconf.AreaSize = cpu_to_le32(TX_CNT * sizeof(txdesc_t) +
					 RX_CNT * sizeof(rxdesc_t) + 8);
	queueconf.NumTxQueues = 1;	/* number of tx queues */
	/* sets the beginning of the tx descriptor queue */
	queueconf.TxQueueStart = memmap.QueueStart;
	/* done by memset: queueconf.TxQueuePri = 0; */
	queueconf.RxQueueStart = cpu_to_le32(rx_queue_start);
	queueconf.QueueOptions = 1;	/* auto reset descriptor */
	/* sets the end of the rx descriptor queue */
	queueconf.QueueEnd =
	    cpu_to_le32(rx_queue_start + RX_CNT * sizeof(rxdesc_t)
	    );
	/* sets the beginning of the next queue */
	queueconf.HostQueueEnd =
	    cpu_to_le32(le32_to_cpu(queueconf.QueueEnd) + 8);
	if (OK != acx_s_configure(adev, &queueconf, ACX1xx_IE_QUEUE_CONFIG)) {
		goto fail;
	}

	if (IS_PCI(adev)) {
		/* sets the beginning of the rx descriptor queue, after the tx descrs */
		if (OK != acxpci_s_create_hostdesc_queues(adev))
			goto fail;
		acxpci_create_desc_queues(adev, tx_queue_start, rx_queue_start);
	}
#ifdef CONFIG_ACX_MAC80211_MEM
	else if (IS_MEM(adev)) {
		/* sets the beginning of the rx descriptor queue, after the tx descrs */
		adev->acx_queue_indicator = (queueindicator_t *) le32_to_cpu (queueconf.QueueEnd);

		if (OK != acxmem_s_create_hostdesc_queues(adev))
			goto fail;

		acxmem_create_desc_queues(adev, tx_queue_start, rx_queue_start);
	}
#endif

	if (OK != acx_s_interrogate(adev, &memmap, ACX1xx_IE_MEMORY_MAP)) {
		goto fail;
	}

	memmap.PoolStart = cpu_to_le32((le32_to_cpu(memmap.QueueEnd) + 4 +
					0x1f) & ~0x1f);

	if (OK != acx_s_configure(adev, &memmap, ACX1xx_IE_MEMORY_MAP)) {
		goto fail;
	}

	if (OK != acx100_s_init_memory_pools(adev, &memmap)) {
		goto fail;
	}

	res = OK;
	goto end;

      fail:
	acx_s_mwait(1000);	/* ? */
	if (IS_PCI(adev))
		acxpci_free_desc_queues(adev);
	else if (IS_MEM(adev))
		acxmem_free_desc_queues(adev);
      end:
	FN_EXIT1(res);
	return res;
}


/***********************************************************************
** acx111_s_create_dma_regions
**
** Note that this fn messes heavily with hardware, but we cannot
** lock it (we need to sleep). Not a problem since IRQs can't happen
*/
#define ACX111_PERCENT(percent) ((percent)/5)

static int acx111_s_create_dma_regions(acx_device_t * adev)
{
	struct acx111_ie_memoryconfig memconf;
	struct acx111_ie_queueconfig queueconf;
	u32 tx_queue_start, rx_queue_start;

	FN_ENTER;

	/* Calculate memory positions and queue sizes */

	/* Set up our host descriptor pool + data pool */
	if (IS_PCI(adev)) {
		if (OK != acxpci_s_create_hostdesc_queues(adev))
			goto fail;
	}
	else if (IS_MEM(adev)) {
		if (OK != acxmem_s_create_hostdesc_queues(adev))
			goto fail;
	}


	memset(&memconf, 0, sizeof(memconf));
	/* the number of STAs (STA contexts) to support
	 ** NB: was set to 1 and everything seemed to work nevertheless... */
	memconf.no_of_stations = 1;	//cpu_to_le16(VEC_SIZE(adev->sta_list));
	/* specify the memory block size. Default is 256 */
	memconf.memory_block_size = cpu_to_le16(adev->memblocksize);
	/* let's use 50%/50% for tx/rx (specify percentage, units of 5%) */
	memconf.tx_rx_memory_block_allocation = ACX111_PERCENT(50);
	/* set the count of our queues
	 ** NB: struct acx111_ie_memoryconfig shall be modified
	 ** if we ever will switch to more than one rx and/or tx queue */
	memconf.count_rx_queues = 1;
	memconf.count_tx_queues = 1;
	/* 0 == Busmaster Indirect Memory Organization, which is what we want
	 * (using linked host descs with their allocated mem).
	 * 2 == Generic Bus Slave */
	/* done by memset: memconf.options = 0; */
	/* let's use 25% for fragmentations and 75% for frame transfers
	 * (specified in units of 5%) */
	memconf.fragmentation = ACX111_PERCENT(75);
	/* Rx descriptor queue config */
	memconf.rx_queue1_count_descs = RX_CNT;
	memconf.rx_queue1_type = 7;	/* must be set to 7 */
	/* done by memset: memconf.rx_queue1_prio = 0; low prio */
	if (IS_PCI(adev)) {
		memconf.rx_queue1_host_rx_start =
		    cpu2acx(adev->rxhostdesc_startphy);
	}
	else if (IS_MEM(adev))
	{
		memconf.rx_queue1_host_rx_start = cpu2acx(adev->rxhostdesc_startphy);
	}

	/* Tx descriptor queue config */
	memconf.tx_queue1_count_descs = TX_CNT;
	/* done by memset: memconf.tx_queue1_attributes = 0; lowest priority */

	/* NB1: this looks wrong: (memconf,ACX1xx_IE_QUEUE_CONFIG),
	 ** (queueconf,ACX1xx_IE_MEMORY_CONFIG_OPTIONS) look swapped, eh?
	 ** But it is actually correct wrt IE numbers.
	 ** NB2: sizeof(memconf) == 28 == 0x1c but configure(ACX1xx_IE_QUEUE_CONFIG)
	 ** writes 0x20 bytes (because same IE for acx100 uses struct acx100_ie_queueconfig
	 ** which is 4 bytes larger. what a mess. TODO: clean it up) */
	if (OK != acx_s_configure(adev, &memconf, ACX1xx_IE_QUEUE_CONFIG)) {
		goto fail;
	}

	acx_s_interrogate(adev, &queueconf, ACX1xx_IE_MEMORY_CONFIG_OPTIONS);

	tx_queue_start = le32_to_cpu(queueconf.tx1_queue_address);
	rx_queue_start = le32_to_cpu(queueconf.rx1_queue_address);

	log(L_INIT, "acx: dump queue head (from card):\n"
	    "acx: len: %u\n"
	    "acx: tx_memory_block_address: %X\n"
	    "acx: rx_memory_block_address: %X\n"
	    "acx: tx1_queue address: %X\n"
	    "acx: rx1_queue address: %X\n",
	    le16_to_cpu(queueconf.len),
	    le32_to_cpu(queueconf.tx_memory_block_address),
	    le32_to_cpu(queueconf.rx_memory_block_address),
	    tx_queue_start, rx_queue_start);

	if (IS_PCI(adev))
		acxpci_create_desc_queues(adev, tx_queue_start, rx_queue_start);
	else if (IS_MEM(adev))
		acxmem_create_desc_queues(adev, tx_queue_start, rx_queue_start);

	FN_EXIT1(OK);
	return OK;

      fail:
	if (IS_PCI(adev))
		acxpci_free_desc_queues(adev);
	else if (IS_MEM(adev))
		acxmem_free_desc_queues(adev);

	FN_EXIT1(NOT_OK);
	return NOT_OK;
}


/***********************************************************************
*/
static void acx_s_initialize_rx_config(acx_device_t * adev)
{
	struct {
		u16 id;
		u16 len;
		u16 rx_cfg1;
		u16 rx_cfg2;
	} ACX_PACKED cfg;

	switch (adev->mode) {
	case ACX_MODE_MONITOR:
		adev->rx_config_1 = (u16) (0
					   /* | RX_CFG1_INCLUDE_RXBUF_HDR  */
					   /* | RX_CFG1_FILTER_SSID        */
					   /* | RX_CFG1_FILTER_BCAST       */
					   /* | RX_CFG1_RCV_MC_ADDR1       */
					   /* | RX_CFG1_RCV_MC_ADDR0       */
					   /* | RX_CFG1_FILTER_ALL_MULTI   */
					   /* | RX_CFG1_FILTER_BSSID       */
					   /* | RX_CFG1_FILTER_MAC         */
					   | RX_CFG1_RCV_PROMISCUOUS
					   | RX_CFG1_INCLUDE_FCS
					   /* | RX_CFG1_INCLUDE_PHY_HDR    */
		    );
		adev->rx_config_2 = (u16) (0
					   | RX_CFG2_RCV_ASSOC_REQ
					   | RX_CFG2_RCV_AUTH_FRAMES
					   | RX_CFG2_RCV_BEACON_FRAMES
					   | RX_CFG2_RCV_CONTENTION_FREE
					   | RX_CFG2_RCV_CTRL_FRAMES
					   | RX_CFG2_RCV_DATA_FRAMES
					   | RX_CFG2_RCV_BROKEN_FRAMES
					   | RX_CFG2_RCV_MGMT_FRAMES
					   | RX_CFG2_RCV_PROBE_REQ
					   | RX_CFG2_RCV_PROBE_RESP
					   | RX_CFG2_RCV_ACK_FRAMES
					   | RX_CFG2_RCV_OTHER);
		break;
	default:
		adev->rx_config_1 = (u16) (0
					   /* | RX_CFG1_INCLUDE_RXBUF_HDR  */
					   /* | RX_CFG1_FILTER_SSID        */
					   /* | RX_CFG1_FILTER_BCAST       */
					   /* | RX_CFG1_RCV_MC_ADDR1       */
					   /* | RX_CFG1_RCV_MC_ADDR0       */
					   /* | RX_CFG1_FILTER_ALL_MULTI   */
					   /* | RX_CFG1_FILTER_BSSID       */
					   /* | RX_CFG1_FILTER_MAC         */
					    | RX_CFG1_RCV_PROMISCUOUS
					   /* | RX_CFG1_INCLUDE_FCS */
					   /* | RX_CFG1_INCLUDE_PHY_HDR   */
		    );
		adev->rx_config_2 = (u16) (0
					   | RX_CFG2_RCV_ASSOC_REQ
					   | RX_CFG2_RCV_AUTH_FRAMES
					   | RX_CFG2_RCV_BEACON_FRAMES
					   | RX_CFG2_RCV_CONTENTION_FREE
					   | RX_CFG2_RCV_CTRL_FRAMES
					   | RX_CFG2_RCV_DATA_FRAMES
					   /*| RX_CFG2_RCV_BROKEN_FRAMES   */
					   | RX_CFG2_RCV_MGMT_FRAMES
					   | RX_CFG2_RCV_PROBE_REQ
					   | RX_CFG2_RCV_PROBE_RESP
					   | RX_CFG2_RCV_ACK_FRAMES
					   | RX_CFG2_RCV_OTHER);
		break;
	}
	adev->rx_config_1 |= RX_CFG1_INCLUDE_RXBUF_HDR;

	if ((adev->rx_config_1 & RX_CFG1_INCLUDE_PHY_HDR)
	    || (adev->firmware_numver >= 0x02000000))
		adev->phy_header_len = IS_ACX111(adev) ? 8 : 4;
	else
		adev->phy_header_len = 0;

	log(L_DEBUG, "acx: setting RXconfig to %04X:%04X\n",
	    adev->rx_config_1, adev->rx_config_2);
	cfg.rx_cfg1 = cpu_to_le16(adev->rx_config_1);
	cfg.rx_cfg2 = cpu_to_le16(adev->rx_config_2);
	acx_s_configure(adev, &cfg, ACX1xx_IE_RXCONFIG);
}


/***********************************************************************
** FIXME: this should be solved in a general way for all radio types
** by decoding the radio firmware module,
** since it probably has some standard structure describing how to
** set the power level of the radio module which it controls.
** Or maybe not, since the radio module probably has a function interface
** instead which then manages Tx level programming :-\
**
** Obvious
*/
static int acx111_s_set_tx_level(acx_device_t * adev, u8 level_dbm)
{
	struct acx111_ie_tx_level tx_level;

	/* my acx111 card has two power levels in its configoptions (== EEPROM):
	 * 1 (30mW) [15dBm]
	 * 2 (10mW) [10dBm]
	 * For now, just assume all other acx111 cards have the same.
	 * FIXME: Ideally we would query it here, but we first need a
	 * standard way to query individual configoptions easily.
	 * Well, now we have proper cfgopt txpower variables, but this still
	 * hasn't been done yet, since it also requires dBm <-> mW conversion here... */
	if (level_dbm <= 12) {
		tx_level.level = 2;	/* 10 dBm */
		adev->tx_level_dbm = 10;
	} else {
		tx_level.level = 1;	/* 15 dBm */
		adev->tx_level_dbm = 15;
	}
	if (level_dbm != adev->tx_level_dbm)
		log(L_INIT, "acx: only predefined transmission "
		    "power levels are supported at this time: "
		    "adjusted %d dBm to %d dBm\n", level_dbm,
		    adev->tx_level_dbm);

	return acx_s_configure(adev, &tx_level, ACX1xx_IE_DOT11_TX_POWER_LEVEL);
}

static int acx_s_set_tx_level(acx_device_t *adev, u8 level_dbm)
{
	if (IS_ACX111(adev)) {
		return acx111_s_set_tx_level(adev, level_dbm);
	}
	if (IS_PCI(adev)) {
		return acx100pci_s_set_tx_level(adev, level_dbm);
	}
	if (IS_MEM(adev)) {
		return acx100mem_s_set_tx_level(adev, level_dbm);
	}

	return OK;
}


/***********************************************************************
** acx_s_set_defaults
*/
void acx_s_set_defaults(acx_device_t * adev)
{
	struct ieee80211_conf *conf = &adev->ieee->conf;
	unsigned long flags;

	FN_ENTER;

	// OW FIXME - review locking
	acx_lock(adev, flags);

	/* do it before getting settings, prevent bogus channel 0 warning */
	adev->channel = 1;

	/* query some settings from the card.
	 * NOTE: for some settings, e.g. CCA and ED (ACX100!), an initial
	 * query is REQUIRED, otherwise the card won't work correctly! */
	adev->get_mask =
	    GETSET_ANTENNA | GETSET_SENSITIVITY | GETSET_STATION_ID |
	    GETSET_REG_DOMAIN;
	/* Only ACX100 supports ED and CCA */
	if (IS_ACX100(adev))
		adev->get_mask |= GETSET_CCA | GETSET_ED_THRESH;

	// OW FIXME - review locking
	acx_unlock(adev, flags);
	acx_s_update_card_settings(adev);
	acx_lock(adev, flags);

	/* set our global interrupt mask */
	if (IS_PCI(adev))
		acxpci_set_interrupt_mask(adev);
	else if (IS_MEM(adev))
		acxmem_set_interrupt_mask(adev);

	adev->led_power = 1;	/* LED is active on startup */
	adev->brange_max_quality = 60;	/* LED blink max quality is 60 */
	adev->brange_time_last_state_change = jiffies;

	/* copy the MAC address we just got from the card
	 * into our MAC address used during current 802.11 session */
	SET_IEEE80211_PERM_ADDR(adev->ieee, adev->dev_addr);
	MAC_BCAST(adev->ap);

	adev->essid_len =
	    snprintf(adev->essid, sizeof(adev->essid), "STA%02X%02X%02X",
		     adev->dev_addr[3], adev->dev_addr[4], adev->dev_addr[5]);
	adev->essid_active = 1;

	/* we have a nick field to waste, so why not abuse it
	 * to announce the driver version? ;-) */
	strncpy(adev->nick, "acx " ACX_RELEASE, IW_ESSID_MAX_SIZE);

	if (IS_PCI(adev)) {	/* FIXME: this should be made to apply to USB, too! */
		/* first regulatory domain entry in EEPROM == default reg. domain */
		adev->reg_dom_id = adev->cfgopt_domains.list[0];
	} else if(IS_MEM(adev)){
		/* first regulatory domain entry in EEPROM == default reg. domain */
		adev->reg_dom_id = adev->cfgopt_domains.list[0];
	}

	/* 0xffff would be better, but then we won't get a "scan complete"
	 * interrupt, so our current infrastructure will fail: */
	adev->scan_count = 1;
	adev->scan_mode = ACX_SCAN_OPT_ACTIVE;
	adev->scan_duration = 100;
	adev->scan_probe_delay = 200;
	/* reported to break scanning: adev->scan_probe_delay = adev->cfgopt_probe_delay; */
	adev->scan_rate = ACX_SCAN_RATE_1;


	adev->mode = ACX_MODE_2_STA;
	adev->listen_interval = 100;
	adev->beacon_interval = DEFAULT_BEACON_INTERVAL;
	adev->dtim_interval = DEFAULT_DTIM_INTERVAL;

	adev->msdu_lifetime = DEFAULT_MSDU_LIFETIME;

	adev->rts_threshold = DEFAULT_RTS_THRESHOLD;
	adev->frag_threshold = 2346;

	/* use standard default values for retry limits */
	adev->short_retry = 7;	/* max. retries for (short) non-RTS packets */
	adev->long_retry = 4;	/* max. retries for long (RTS) packets */

	adev->preamble_mode = 2;	/* auto */
	adev->fallback_threshold = 3;
	adev->stepup_threshold = 10;
	adev->rate_bcast = RATE111_1;
	adev->rate_bcast100 = RATE100_1;
	adev->rate_basic = RATE111_1 | RATE111_2;
	adev->rate_auto = 1;
	if (IS_ACX111(adev)) {
		adev->rate_oper = RATE111_ALL;
	} else {
		adev->rate_oper = RATE111_ACX100_COMPAT;
	}

	/* Supported Rates element - the rates here are given in units of
	 * 500 kbit/s, plus 0x80 added. See 802.11-1999.pdf item 7.3.2.2 */
	acx_l_update_ratevector(adev);

	/* set some more defaults */
	if (IS_ACX111(adev)) {
		/* 30mW (15dBm) is default, at least in my acx111 card: */
		adev->tx_level_dbm = 15;
		conf->power_level = adev->tx_level_dbm;
		acx_unlock(adev, flags);
		acx_s_set_tx_level(adev, adev->tx_level_dbm);
		SET_BIT(adev->set_mask, GETSET_TXPOWER);
		acx_lock(adev, flags);
	} else {
		/* don't use max. level, since it might be dangerous
		 * (e.g. WRT54G people experience
		 * excessive Tx power damage!) */
		adev->tx_level_dbm = 18;
		conf->power_level = adev->tx_level_dbm;
		acx_unlock(adev, flags);
		acx_s_set_tx_level(adev, adev->tx_level_dbm);
		SET_BIT(adev->set_mask, GETSET_TXPOWER);
		acx_lock(adev, flags);
	}

	/* adev->tx_level_auto = 1; */
	if (IS_ACX111(adev)) {
		/* start with sensitivity level 1 out of 3: */
		adev->sensitivity = 1;
	}

/* #define ENABLE_POWER_SAVE */
#ifdef ENABLE_POWER_SAVE
	adev->ps_wakeup_cfg = PS_CFG_ENABLE | PS_CFG_WAKEUP_ALL_BEAC;
	adev->ps_listen_interval = 1;
	adev->ps_options =
	    PS_OPT_ENA_ENHANCED_PS | PS_OPT_TX_PSPOLL | PS_OPT_STILL_RCV_BCASTS;
	adev->ps_hangover_period = 30;
	adev->ps_enhanced_transition_time = 0;
#else
	adev->ps_wakeup_cfg = 0;
	adev->ps_listen_interval = 0;
	adev->ps_options = 0;
	adev->ps_hangover_period = 0;
	adev->ps_enhanced_transition_time = 0;
#endif

	/* These settings will be set in fw on ifup */
	adev->set_mask = 0 | GETSET_RETRY | SET_MSDU_LIFETIME
	    /* configure card to do rate fallback when in auto rate mode */
	    | SET_RATE_FALLBACK | SET_RXCONFIG | GETSET_TXPOWER
	    /* better re-init the antenna value we got above */
	    | GETSET_ANTENNA
#if POWER_SAVE_80211
	    | GETSET_POWER_80211
#endif
	    ;

	acx_unlock(adev, flags);
	acx_lock_unhold();	/* hold time 844814 CPU ticks @2GHz */

	acx_s_initialize_rx_config(adev);

	FN_EXIT0;
}


/***********************************************************************
** acx_l_process_rxbuf
**
** NB: used by USB code also
*/
void acx_l_process_rxbuf(acx_device_t * adev, rxbuffer_t * rxbuf)
{
	struct ieee80211_hdr *hdr;
	u16 fc, buf_len;

	FN_ENTER;

	hdr = acx_get_wlan_hdr(adev, rxbuf);
	fc = le16_to_cpu(hdr->frame_control);
	/* length of frame from control field to first byte of FCS */
	buf_len = RXBUF_BYTES_RCVD(adev, rxbuf);

	// For debugging
	if (
			((WF_FC_FSTYPE & fc) != WF_FSTYPE_BEACON) &&
			(acx_debug & (L_XFER|L_DATA))
	){
		printk(	"acx: rx: %s "
				"time:%u len:%u signal:%u SNR:%u macstat:%02X "
				"phystat:%02X phyrate:%u status:%u\n",
				acx_get_packet_type_string(fc),

				le32_to_cpu(rxbuf->time),
				buf_len,
				acx_signal_to_winlevel(rxbuf->phy_level),
				acx_signal_to_winlevel(rxbuf->phy_snr),
				rxbuf->mac_status,

				rxbuf->phy_stat_baseband,
				rxbuf->phy_plcp_signal,
				adev->status);
	}

	if (unlikely(acx_debug & L_DATA)) {
		printk("acx: rx: 802.11 buf[%u]: \n", buf_len);
		acx_dump_bytes(hdr, buf_len);
	}

	acx_l_rx(adev, rxbuf);
	/* Now check Rx quality level, AFTER processing packet.
	 * I tried to figure out how to map these levels to dBm
	 * values, but for the life of me I really didn't
	 * manage to get it. Either these values are not meant to
	 * be expressed in dBm, or it's some pretty complicated
	 * calculation. */

	/* TODO: only the RSSI seems to be reported */
	adev->rx_status.signal = acx_signal_to_winlevel(rxbuf->phy_level);

	FN_EXIT0;
}


/***********************************************************************
** acx_l_handle_txrate_auto
**
** Theory of operation:
** client->rate_cap is a bitmask of rates client is capable of.
** client->rate_cfg is a bitmask of allowed (configured) rates.
** It is set as a result of iwconfig rate N [auto]
** or iwpriv set_rates "N,N,N N,N,N" commands.
** It can be fixed (e.g. 0x0080 == 18Mbit only),
** auto (0x00ff == 18Mbit or any lower value),
** and code handles any bitmask (0x1081 == try 54Mbit,18Mbit,1Mbit _only_).
**
** client->rate_cur is a value for rate111 field in tx descriptor.
** It is always set to txrate_cfg sans zero or more most significant
** bits. This routine handles selection of new rate_cur value depending on
** outcome of last tx event.
**
** client->rate_100 is a precalculated rate value for acx100
** (we can do without it, but will need to calculate it on each tx).
**
** You cannot configure mixed usage of 5.5 and/or 11Mbit rate
** with PBCC and CCK modulation. Either both at CCK or both at PBCC.
** In theory you can implement it, but so far it is considered not worth doing.
**
** 22Mbit, of course, is PBCC always. */

/* maps acx100 tx descr rate field to acx111 one */
/*
static u16 rate100to111(u8 r)
{
	switch (r) {
	case RATE100_1:
		return RATE111_1;
	case RATE100_2:
		return RATE111_2;
	case RATE100_5:
	case (RATE100_5 | RATE100_PBCC511):
		return RATE111_5;
	case RATE100_11:
	case (RATE100_11 | RATE100_PBCC511):
		return RATE111_11;
	case RATE100_22:
		return RATE111_22;
	default:
		printk("acx: unexpected acx100 txrate: %u! "
		       "Please report\n", r);
		return RATE111_1;
	}
}

*/

int acx_i_op_tx(struct ieee80211_hw *hw, struct sk_buff *skb) {
	acx_device_t *adev = ieee2adev(hw);
	tx_t *tx;
	void *txbuf;
	unsigned long flags;

	struct ieee80211_tx_info *ctl = IEEE80211_SKB_CB(skb);
	int txresult = NOT_OK;

	FN_ENTER;

	if (unlikely(!skb)) {
		/* indicate success */
		txresult = OK;
		goto out;
	}

	if (unlikely(!adev)) {
		goto out;
	}

	if (unlikely(!(adev->dev_state_mask & ACX_STATE_IFACE_UP))) {
		goto out;
	}
	if (unlikely(!adev->initialized)) {
		goto out;
	}

	acx_lock(adev, flags);

	tx = acx_l_alloc_tx(adev, skb->len);

	if (unlikely(!tx)) {
		log(L_BUFT, "acx: %s: start_xmit: txdesc ring is full, " "dropping tx\n", wiphy_name(adev->ieee->wiphy));
		txresult = NOT_OK;
		goto out_unlock;
	}

	txbuf = acx_l_get_txbuf(adev, tx);

	if (unlikely(!txbuf)) {
		/* Card was removed */
		txresult = NOT_OK;
		// OW only USB implemented
		acx_l_dealloc_tx(adev, tx);
		goto out_unlock;
	}
	memcpy(txbuf, skb->data, skb->len);

	acx_l_tx_data(adev, tx, skb->len, ctl, skb);

	txresult = OK;
	adev->stats.tx_packets++;
	adev->stats.tx_bytes += skb->len;

out_unlock:
	acx_unlock(adev, flags);

out:
	FN_EXIT1(txresult);
	return txresult;
}
/***********************************************************************
** acx_l_update_ratevector
**
** Updates adev->rate_supported[_len] according to rate_{basic,oper}
*/
const u8 acx_bitpos2ratebyte[] = {
	DOT11RATEBYTE_1,
	DOT11RATEBYTE_2,
	DOT11RATEBYTE_5_5,
	DOT11RATEBYTE_6_G,
	DOT11RATEBYTE_9_G,
	DOT11RATEBYTE_11,
	DOT11RATEBYTE_12_G,
	DOT11RATEBYTE_18_G,
	DOT11RATEBYTE_22,
	DOT11RATEBYTE_24_G,
	DOT11RATEBYTE_36_G,
	DOT11RATEBYTE_48_G,
	DOT11RATEBYTE_54_G,
};

void acx_l_update_ratevector(acx_device_t * adev)
{
	u16 bcfg = adev->rate_basic;
	u16 ocfg = adev->rate_oper;
	u8 *supp = adev->rate_supported;
	const u8 *dot11 = acx_bitpos2ratebyte;

	FN_ENTER;

	while (ocfg) {
		if (ocfg & 1) {
			*supp = *dot11;
			if (bcfg & 1) {
				*supp |= 0x80;
			}
			supp++;
		}
		dot11++;
		ocfg >>= 1;
		bcfg >>= 1;
	}
	adev->rate_supported_len = supp - adev->rate_supported;
	if (acx_debug & L_ASSOC) {
		printk("acx: new ratevector: ");
		acx_dump_bytes(adev->rate_supported, adev->rate_supported_len);
	}
	FN_EXIT0;
}

/***********************************************************************
** acx_i_timer
**
** Fires up periodically. Used to kick scan/auth/assoc if something goes wrong
**
** Obvious
*/
void acx_i_timer(unsigned long address)
{
	unsigned long flags;
	acx_device_t *adev = (acx_device_t *) address;

	FN_ENTER;

	acx_lock(adev, flags);

	FIXME();
	/* We need calibration and stats gather tasks to perform here */

	acx_unlock(adev, flags);

	FN_EXIT0;
}


/***********************************************************************
** acx_set_timer
**
** Sets the 802.11 state management timer's timeout.
**
** Linux derived
*/
void acx_set_timer(acx_device_t * adev, int timeout_us)
{
	FN_ENTER;

	log(L_DEBUG | L_IRQ, "acx: %s(%u ms)\n", __func__, timeout_us / 1000);
	if (!(adev->dev_state_mask & ACX_STATE_IFACE_UP)) {
		printk("acx: attempt to set the timer "
		       "when the card interface is not up!\n");
		goto end;
	}

	/* first check if the timer was already initialized, THEN modify it */
	if (adev->mgmt_timer.function) {
		mod_timer(&adev->mgmt_timer,
			  jiffies + (timeout_us * HZ / 1000000));
	}
      end:
	FN_EXIT0;
}

/** acx_plcp_get_bitrate_cck
 **
 ** Obvious
*/
static u8 acx_plcp_get_bitrate_cck(u8 plcp)
{
        switch (plcp) {
        case 0x0A:
                return ACX_CCK_RATE_1MB;
        case 0x14:
                return ACX_CCK_RATE_2MB;
        case 0x37:
                return ACX_CCK_RATE_5MB;
        case 0x6E:
                return ACX_CCK_RATE_11MB;
        }
        return 0;
}

/* Extract the bitrate out of an OFDM PLCP header. */
/** Obvious **/
static u8 acx_plcp_get_bitrate_ofdm(u8 plcp)
{
        switch (plcp & 0xF) {
        case 0xB:
                return ACX_OFDM_RATE_6MB;
        case 0xF:
                return ACX_OFDM_RATE_9MB;
        case 0xA:
                return ACX_OFDM_RATE_12MB;
        case 0xE:
                return ACX_OFDM_RATE_18MB;
        case 0x9:
                return ACX_OFDM_RATE_24MB;
        case 0xD:
                return ACX_OFDM_RATE_36MB;
        case 0x8:
                return ACX_OFDM_RATE_48MB;
        case 0xC:
                return ACX_OFDM_RATE_54MB;
        }
        return 0;
}


/***********************************************************************
** acx_l_rx
**
** The end of the Rx path. Pulls data from a rxhostdesc into a socket
** buffer and feeds it to the network stack via netif_rx().
**
** Look to bcm43xx or p54
** FIXME Remove documentation references to bcm43xx and p54 ...
*/

static void acx_l_rx(acx_device_t *adev, rxbuffer_t *rxbuf)
{

	struct ieee80211_rx_status *status;

	struct ieee80211_hdr *w_hdr;
	struct sk_buff *skb;
	int buflen;
	FN_ENTER;

	if (unlikely(!(adev->dev_state_mask & ACX_STATE_IFACE_UP))) {
		printk("acx: asked to receive a packet but the interface is down??\n");
		goto out;
	}

	w_hdr = acx_get_wlan_hdr(adev, rxbuf);
	buflen = RXBUF_BYTES_USED(rxbuf) - ((u8*)w_hdr - (u8*)rxbuf);

	/*
	 * Allocate our skb
	 */
	skb = dev_alloc_skb(buflen + 2);
	if (!skb) {
		printk("acx: skb allocation FAILED\n");
		goto out;
	}

	skb_reserve(skb, 2);
	skb_put(skb, buflen);
	memcpy(skb->data, w_hdr, buflen);

	status=IEEE80211_SKB_RXCB(skb);
	memset(status, 0, sizeof(*status));

	status->mactime = rxbuf->time;
	status->signal = acx_signal_to_winlevel(rxbuf->phy_level);

	/* TODO: they do not seem to be reported, at least on the acx111
	 * (and TNETW1450?), therefore commenting them out
	status->signal = acx_signal_to_winlevel(rxbuf->phy_level);
	status->noise = acx_signal_to_winlevel(rxbuf->phy_snr); */

	status->flag = 0;

	status->freq = adev->rx_status.freq;
	status->band = adev->rx_status.band;

	status->antenna = 1;
	if (rxbuf->phy_stat_baseband & (1 << 3)) { /* Uses OFDM */
		status->rate_idx = acx_plcp_get_bitrate_ofdm(rxbuf->phy_plcp_signal);
	}
	else {
		status->rate_idx = acx_plcp_get_bitrate_cck(rxbuf->phy_plcp_signal);
	}

	/*
	 * FIXME: should it really be done here??
	 */
	ieee80211_rx_irqsafe(adev->ieee, skb);
	adev->stats.rx_packets++;
	adev->stats.rx_bytes += skb->len;

out:
	FN_EXIT0;
}



/***********************************************************************
** acx_s_read_fw
**
** Loads a firmware image
**
** Returns:
**  0				unable to load file
**  pointer to firmware		success
*/
firmware_image_t *acx_s_read_fw(struct device *dev, const char *file,
				u32 * size)
{
	firmware_image_t *res;
	const struct firmware *fw_entry;

	res = NULL;
	log(L_INIT, "acx: requesting firmware image '%s'\n", file);
	if (!request_firmware(&fw_entry, file, dev)) {
		*size = 8;
		if (fw_entry->size >= 8)
			*size = 8 + le32_to_cpu(*(u32 *) (fw_entry->data + 4));
		if (fw_entry->size != *size) {
			printk("acx: firmware size does not match "
			       "firmware header: %d != %d, "
			       "aborting fw upload\n",
			       (int)fw_entry->size, (int)*size);
			goto release_ret;
		}
		res = vmalloc(*size);
		if (!res) {
			printk("acx: no memory for firmware "
			       "(%u bytes)\n", *size);
			goto release_ret;
		}
		memcpy(res, fw_entry->data, fw_entry->size);
	      release_ret:
		release_firmware(fw_entry);
		return res;
	}
	printk("acx: firmware image '%s' was not provided. "
	       "Check your hotplug scripts\n", file);

	/* checksum will be verified in write_fw, so don't bother here */
	return res;
}


/***********************************************************************
** acx_s_set_wepkey
*/
static void acx100_s_set_wepkey(acx_device_t * adev)
{
	ie_dot11WEPDefaultKey_t dk;
	int i;

	for (i = 0; i < DOT11_MAX_DEFAULT_WEP_KEYS; i++) {
		if (adev->wep_keys[i].size != 0) {
			log(L_INIT, "acx: setting WEP key: %d with "
			    "total size: %d\n", i, (int)adev->wep_keys[i].size);
			dk.action = 1;
			dk.keySize = adev->wep_keys[i].size;
			dk.defaultKeyNum = i;
			memcpy(dk.key, adev->wep_keys[i].key, dk.keySize);
			acx_s_configure(adev, &dk,
					ACX100_IE_DOT11_WEP_DEFAULT_KEY_WRITE);
		}
	}
}

static void acx111_s_set_wepkey(acx_device_t * adev)
{
	acx111WEPDefaultKey_t dk;
	int i;

	for (i = 0; i < DOT11_MAX_DEFAULT_WEP_KEYS; i++) {
		if (adev->wep_keys[i].size != 0) {
			log(L_INIT, "acx: setting WEP key: %d with "
			    "total size: %d\n", i, (int)adev->wep_keys[i].size);
			memset(&dk, 0, sizeof(dk));
			dk.action = cpu_to_le16(1);	/* "add key"; yes, that's a 16bit value */
			dk.keySize = adev->wep_keys[i].size;

			/* are these two lines necessary? */
			dk.type = 0;	/* default WEP key */
			dk.index = 0;	/* ignored when setting default key */

			dk.defaultKeyNum = i;
			memcpy(dk.key, adev->wep_keys[i].key, dk.keySize);
			acx_s_issue_cmd(adev, ACX1xx_CMD_WEP_MGMT, &dk,
					sizeof(dk));
		}
	}
}
/* Obvious */
static void acx_s_set_wepkey(acx_device_t * adev)
{
	if (IS_ACX111(adev))
		acx111_s_set_wepkey(adev);
	else
		acx100_s_set_wepkey(adev);
}


/***********************************************************************
** acx100_s_init_wep
**
** FIXME: this should probably be moved into the new card settings
** management, but since we're also modifying the memory map layout here
** due to the WEP key space we want, we should take care...
*/
static int acx100_s_init_wep(acx_device_t * adev)
{
	// acx100_ie_wep_options_t options;
	// ie_dot11WEPDefaultKeyID_t dk;
	acx_ie_memmap_t pt;
	int res = NOT_OK;

	FN_ENTER;

	if (OK != acx_s_interrogate(adev, &pt, ACX1xx_IE_MEMORY_MAP)) {
		goto fail;
	}

	log(L_DEBUG, "acx: CodeEnd:%X\n", pt.CodeEnd);

	pt.WEPCacheStart = cpu_to_le32(le32_to_cpu(pt.CodeEnd) + 0x4);
	pt.WEPCacheEnd = cpu_to_le32(le32_to_cpu(pt.CodeEnd) + 0x4);

	if (OK != acx_s_configure(adev, &pt, ACX1xx_IE_MEMORY_MAP)) {
		goto fail;
	}

/* OW: This disables WEP by not configuring the WEP cache and leaving
 * WEPCacheStart=WEPCacheEnd.
 *
 * When doing the crypto by mac80211 it is required, that the acx is not doing
 * any WEP crypto himself. Otherwise TX "WEP key not found" errors occure.
 *
 * By disabling WEP using WEPCacheStart=WEPCacheStart the acx not trying any
 * own crypto anymore. All crypto (including WEP) is pushed to mac80211 for the
 * moment.
 *
 */
#if 0

	/* let's choose maximum setting: 4 default keys, plus 10 other keys: */
	options.NumKeys = cpu_to_le16(DOT11_MAX_DEFAULT_WEP_KEYS + 10);
	options.WEPOption = 0x00;

	log(L_ASSOC, "acx: writing WEP options\n");
	acx_s_configure(adev, &options, ACX100_IE_WEP_OPTIONS);

	acx100_s_set_wepkey(adev);

	if (adev->wep_keys[adev->wep_current_index].size != 0) {
		log(L_ASSOC, "acx: setting active default WEP key number: %d\n",
		    adev->wep_current_index);
		dk.KeyID = adev->wep_current_index;
		acx_s_configure(adev, &dk, ACX1xx_IE_DOT11_WEP_DEFAULT_KEY_SET);	/* 0x1010 */
	}
	/* FIXME!!! wep_key_struct is filled nowhere! But adev
	 * is initialized to 0, and we don't REALLY need those keys either */
/*		for (i = 0; i < 10; i++) {
		if (adev->wep_key_struct[i].len != 0) {
			MAC_COPY(wep_mgmt.MacAddr, adev->wep_key_struct[i].addr);
			wep_mgmt.KeySize = cpu_to_le16(adev->wep_key_struct[i].len);
			memcpy(&wep_mgmt.Key, adev->wep_key_struct[i].key, le16_to_cpu(wep_mgmt.KeySize));
			wep_mgmt.Action = cpu_to_le16(1);
			log(L_ASSOC, "acx: writing WEP key %d (len %d)\n", i, le16_to_cpu(wep_mgmt.KeySize));
			if (OK == acx_s_issue_cmd(adev, ACX1xx_CMD_WEP_MGMT, &wep_mgmt, sizeof(wep_mgmt))) {
				adev->wep_key_struct[i].index = i;
			}
		}
	}
*/

	/* now retrieve the updated WEPCacheEnd pointer... */
	if (OK != acx_s_interrogate(adev, &pt, ACX1xx_IE_MEMORY_MAP)) {
		printk("acx: %s: ACX1xx_IE_MEMORY_MAP read #2 FAILED\n",
		       wiphy_name(adev->ieee->wiphy));
		goto fail;
	}
#endif

	/* ...and tell it to start allocating templates at that location */
	/* (no endianness conversion needed) */
	pt.PacketTemplateStart = pt.WEPCacheEnd;

	if (OK != acx_s_configure(adev, &pt, ACX1xx_IE_MEMORY_MAP)) {
		printk("acx: %s: ACX1xx_IE_MEMORY_MAP write #2 FAILED\n",
		       wiphy_name(adev->ieee->wiphy));
		goto fail;
	}
	res = OK;

      fail:
	FN_EXIT1(res);
	return res;
}


static int
acx_s_init_max_template_generic(acx_device_t * adev, unsigned int len,
				unsigned int cmd)
{
	int res;
	union {
		acx_template_nullframe_t null;
		acx_template_beacon_t b;
		acx_template_tim_t tim;
		acx_template_probereq_t preq;
		acx_template_proberesp_t presp;
	} templ;

	memset(&templ, 0, len);
	templ.null.size = cpu_to_le16(len - 2);
	res = acx_s_issue_cmd(adev, cmd, &templ, len);
	return res;
}

static inline int acx_s_init_max_null_data_template(acx_device_t * adev)
{
	/* OW
	 * hh version:       issue_cmd(cmd:cmd,buflen:26,timeout:50ms,type:0x0018)
	 * mac80211 version: issue_cmd: begin: (cmd:cmd,buflen:32,timeout:50ms,type:0x001E)
	 *
	 * diff with hh is struct ieee80211_hdr included in acx_template_nullframe_t,
	 * which is bigger, thus size if bigger
	 */
	return acx_s_init_max_template_generic(adev,
			sizeof(acx_template_nullframe_t), ACX1xx_CMD_CONFIG_NULL_DATA);
}

static inline int acx_s_init_max_beacon_template(acx_device_t * adev)
{
	return acx_s_init_max_template_generic(adev,
					       sizeof(acx_template_beacon_t),
					       ACX1xx_CMD_CONFIG_BEACON);
}

static inline int acx_s_init_max_tim_template(acx_device_t * adev)
{
	return acx_s_init_max_template_generic(adev, sizeof(acx_template_tim_t),
					       ACX1xx_CMD_CONFIG_TIM);
}

static inline int acx_s_init_max_probe_response_template(acx_device_t * adev)
{
	return acx_s_init_max_template_generic(adev,
					       sizeof(acx_template_proberesp_t),
					       ACX1xx_CMD_CONFIG_PROBE_RESPONSE);
}

static inline int acx_s_init_max_probe_request_template(acx_device_t * adev)
{
	return acx_s_init_max_template_generic(adev,
					       sizeof(acx_template_probereq_t),
					       ACX1xx_CMD_CONFIG_PROBE_REQUEST);
}

/***********************************************************************
** acx_s_set_tim_template
**
** FIXME: In full blown driver we will regularly update partial virtual bitmap
** by calling this function
** (it can be done by irq handler on each DTIM irq or by timer...)

[802.11 7.3.2.6] TIM information element:
- 1 EID
- 1 Length
1 1 DTIM Count
    indicates how many beacons (including this) appear before next DTIM
    (0=this one is a DTIM)
2 1 DTIM Period
    number of beacons between successive DTIMs
    (0=reserved, 1=all TIMs are DTIMs, 2=every other, etc)
3 1 Bitmap Control
    bit0: Traffic Indicator bit associated with Assoc ID 0 (Bcast AID?)
    set to 1 in TIM elements with a value of 0 in the DTIM Count field
    when one or more broadcast or multicast frames are buffered at the AP.
    bit1-7: Bitmap Offset (logically Bitmap_Offset = Bitmap_Control & 0xFE).
4 n Partial Virtual Bitmap
    Visible part of traffic-indication bitmap.
    Full bitmap consists of 2008 bits (251 octets) such that bit number N
    (0<=N<=2007) in the bitmap corresponds to bit number (N mod 8)
    in octet number N/8 where the low-order bit of each octet is bit0,
    and the high order bit is bit7.
    Each set bit in virtual bitmap corresponds to traffic buffered by AP
    for a specific station (with corresponding AID?).
    Partial Virtual Bitmap shows a part of bitmap which has non-zero.
    Bitmap Offset is a number of skipped zero octets (see above).
    'Missing' octets at the tail are also assumed to be zero.
    Example: Length=6, Bitmap_Offset=2, Partial_Virtual_Bitmap=55 55 55
    This means that traffic-indication bitmap is:
    00000000 00000000 01010101 01010101 01010101 00000000 00000000...
    (is bit0 in the map is always 0 and real value is in Bitmap Control bit0?)
*/
static int acx_s_set_tim_template(acx_device_t * adev)
{
/* For now, configure smallish test bitmap, all zero ("no pending data") */
	enum { bitmap_size = 5 };

	acx_template_tim_t t;
	int result;

	FN_ENTER;

	memset(&t, 0, sizeof(t));
	t.size = 5 + bitmap_size;	/* eid+len+count+period+bmap_ctrl + bmap */
	t.tim_eid = WLAN_EID_TIM;
	t.len = 3 + bitmap_size;	/* count+period+bmap_ctrl + bmap */
	result = acx_s_issue_cmd(adev, ACX1xx_CMD_CONFIG_TIM, &t, sizeof(t));
	FN_EXIT1(result);
	return result;
}




#if POWER_SAVE_80211
/***********************************************************************
** acx_s_set_null_data_template
*/
static int acx_s_set_null_data_template(acx_device_t * adev)
{
	struct acx_template_nullframe b;
	int result;

	FN_ENTER;

	/* memset(&b, 0, sizeof(b)); not needed, setting all members */

	b.size = cpu_to_le16(sizeof(b) - 2);
	b.hdr.fc = WF_FTYPE_MGMTi | WF_FSTYPE_NULLi;
	b.hdr.dur = 0;
	MAC_BCAST(b.hdr.a1);
	MAC_COPY(b.hdr.a2, adev->dev_addr);
	MAC_COPY(b.hdr.a3, adev->bssid);
	b.hdr.seq = 0;

	result =
	    acx_s_issue_cmd(adev, ACX1xx_CMD_CONFIG_NULL_DATA, &b, sizeof(b));

	FN_EXIT1(result);
	return result;
}
#endif






/***********************************************************************
** acx_s_init_packet_templates()
**
** NOTE: order is very important here, to have a correct memory layout!
** init templates: max Probe Request (station mode), max NULL data,
** max Beacon, max TIM, max Probe Response.
*/
static int acx_s_init_packet_templates(acx_device_t * adev)
{
	acx_ie_memmap_t mm;	/* ACX100 only */
	int result = NOT_OK;

	FN_ENTER;

	log(L_DEBUG | L_INIT, "acx: initializing max packet templates\n");

	if (OK != acx_s_init_max_probe_request_template(adev))
		goto failed;

	if (OK != acx_s_init_max_null_data_template(adev))
		goto failed;

	if (OK != acx_s_init_max_beacon_template(adev))
		goto failed;

	if (OK != acx_s_init_max_tim_template(adev))
		goto failed;

	if (OK != acx_s_init_max_probe_response_template(adev))
		goto failed;

	if (IS_ACX111(adev)) {
		/* ACX111 doesn't need the memory map magic below,
		 * and the other templates will be set later (acx_start) */
		result = OK;
		goto success;
	}

	/* ACX100 will have its TIM template set,
	 * and we also need to update the memory map */

	if (OK != acx_s_set_tim_template(adev))
		goto failed_acx100;

	log(L_DEBUG, "acx: sizeof(memmap) = %d bytes\n", (int)sizeof(mm));

	if (OK != acx_s_interrogate(adev, &mm, ACX1xx_IE_MEMORY_MAP))
		goto failed_acx100;

	mm.QueueStart = cpu_to_le32(le32_to_cpu(mm.PacketTemplateEnd) + 4);
	if (OK != acx_s_configure(adev, &mm, ACX1xx_IE_MEMORY_MAP))
		goto failed_acx100;

	result = OK;
	goto success;

      failed_acx100:
	log(L_DEBUG | L_INIT,
	    /* "cb=0x%X\n" */
	    "acx: ACXMemoryMap:\n"
	    "acx: .CodeStart=0x%X\n"
	    "acx: .CodeEnd=0x%X\n"
	    "acx: .WEPCacheStart=0x%X\n"
	    "acx: .WEPCacheEnd=0x%X\n"
	    "acx: .PacketTemplateStart=0x%X\n" ".PacketTemplateEnd=0x%X\n",
	    /* len, */
	    le32_to_cpu(mm.CodeStart),
	    le32_to_cpu(mm.CodeEnd),
	    le32_to_cpu(mm.WEPCacheStart),
	    le32_to_cpu(mm.WEPCacheEnd),
	    le32_to_cpu(mm.PacketTemplateStart),
	    le32_to_cpu(mm.PacketTemplateEnd));

      failed:
	printk("acx: %s: %s() FAILED\n", wiphy_name(adev->ieee->wiphy), __func__);

      success:
	FN_EXIT1(result);
	return result;
}



/***********************************************************************
** acx_s_init_mac
*/
int acx_s_init_mac(acx_device_t * adev)
{
	int result = NOT_OK;

	FN_ENTER;

	if (IS_ACX111(adev)) {
		adev->ie_len = acx111_ie_len;
		adev->ie_len_dot11 = acx111_ie_len_dot11;
	} else {
		adev->ie_len = acx100_ie_len;
		adev->ie_len_dot11 = acx100_ie_len_dot11;
	}

	if (IS_PCI(adev)) {
		adev->memblocksize = 256;	/* 256 is default */
		/* try to load radio for both ACX100 and ACX111, since both
		 * chips have at least some firmware versions making use of an
		 * external radio module */
		acxpci_s_upload_radio(adev);
	}
	else if (IS_MEM(adev)){
		adev->memblocksize = 256; /* 256 is default */
		/* try to load radio for both ACX100 and ACX111, since both
		 * chips have at least some firmware versions making use of an
		 * external radio module */
		acxmem_s_upload_radio(adev);
	}
	else {
		adev->memblocksize = 128;
	}

	if (IS_ACX111(adev)) {
		/* for ACX111, the order is different from ACX100
		   1. init packet templates
		   2. create station context and create dma regions
		   3. init wep default keys
		 */
		if (OK != acx_s_init_packet_templates(adev))
			goto fail;
		if (OK != acx111_s_create_dma_regions(adev)) {
			printk("acx: %s: acx111_create_dma_regions FAILED\n",
			       wiphy_name(adev->ieee->wiphy));
			goto fail;
		}
	} else {
		if (OK != acx100_s_init_wep(adev))
			goto fail;
		if (OK != acx_s_init_packet_templates(adev))
			goto fail;
		if (OK != acx100_s_create_dma_regions(adev)) {
			printk("acx: %s: acx100_create_dma_regions FAILED\n",
			       wiphy_name(adev->ieee->wiphy));
			goto fail;
		}
	}

	SET_IEEE80211_PERM_ADDR(adev->ieee, adev->dev_addr);
	result = OK;

      fail:
	if (result)
		printk("acx: init_mac() FAILED\n");
	FN_EXIT1(result);
	return result;
}



#if POWER_SAVE_80211
static void acx_s_update_80211_powersave_mode(acx_device_t * adev)
{
	/* merge both structs in a union to be able to have common code */
	union {
		acx111_ie_powersave_t acx111;
		acx100_ie_powersave_t acx100;
	} pm;

	/* change 802.11 power save mode settings */
	log(L_INIT, "acx: updating 802.11 power save mode settings: "
	    "wakeup_cfg 0x%02X, listen interval %u, "
	    "options 0x%02X, hangover period %u, "
	    "enhanced_ps_transition_time %u\n",
	    adev->ps_wakeup_cfg, adev->ps_listen_interval,
	    adev->ps_options, adev->ps_hangover_period,
	    adev->ps_enhanced_transition_time);
	acx_s_interrogate(adev, &pm, ACX1xx_IE_POWER_MGMT);
	log(L_INIT, "acx: Previous PS mode settings: wakeup_cfg 0x%02X, "
	    "listen interval %u, options 0x%02X, "
	    "hangover period %u, "
	    "enhanced_ps_transition_time %u, beacon_rx_time %u\n",
	    pm.acx111.wakeup_cfg,
	    pm.acx111.listen_interval,
	    pm.acx111.options,
	    pm.acx111.hangover_period,
	    IS_ACX111(adev) ?
	    pm.acx111.enhanced_ps_transition_time
	    : pm.acx100.enhanced_ps_transition_time,
	    IS_ACX111(adev) ? pm.acx111.beacon_rx_time : (u32) - 1);
	pm.acx111.wakeup_cfg = adev->ps_wakeup_cfg;
	pm.acx111.listen_interval = adev->ps_listen_interval;
	pm.acx111.options = adev->ps_options;
	pm.acx111.hangover_period = adev->ps_hangover_period;
	if (IS_ACX111(adev)) {
		pm.acx111.beacon_rx_time = cpu_to_le32(adev->ps_beacon_rx_time);
		pm.acx111.enhanced_ps_transition_time =
		    cpu_to_le32(adev->ps_enhanced_transition_time);
	} else {
		pm.acx100.enhanced_ps_transition_time =
		    cpu_to_le16(adev->ps_enhanced_transition_time);
	}
	acx_s_configure(adev, &pm, ACX1xx_IE_POWER_MGMT);
	acx_s_interrogate(adev, &pm, ACX1xx_IE_POWER_MGMT);
	log(L_INIT, "acx: wakeup_cfg: 0x%02X\n", pm.acx111.wakeup_cfg);
	acx_s_mwait(40);
	acx_s_interrogate(adev, &pm, ACX1xx_IE_POWER_MGMT);
	log(L_INIT, "acx: wakeup_cfg: 0x%02X\n", pm.acx111.wakeup_cfg);
	log(L_INIT, "acx: power save mode change %s\n",
	    (pm.acx111.
	     wakeup_cfg & PS_CFG_PENDING) ? "FAILED" : "was successful");
	/* FIXME: maybe verify via PS_CFG_PENDING bit here
	 * that power save mode change was successful. */
	/* FIXME: we shouldn't trigger a scan immediately after
	 * fiddling with power save mode (since the firmware is sending
	 * a NULL frame then). */
}
#endif


/***********************************************************************
** acx_s_update_card_settings
**
** Applies accumulated changes in various adev->xxxx members
** Called by ioctl commit handler, acx_start, acx_set_defaults,
** acx_s_after_interrupt_task (if IRQ_CMD_UPDATE_CARD_CFG),
*/
void acx_s_set_sane_reg_domain(acx_device_t *adev, int do_set)
{
	unsigned mask;

	unsigned int i;

	for (i = 0; i < sizeof(acx_reg_domain_ids); i++)
		if (acx_reg_domain_ids[i] == adev->reg_dom_id)
			break;

	if (sizeof(acx_reg_domain_ids) == i) {
		log(L_INIT, "acx: Invalid or unsupported regulatory domain"
			       " 0x%02X specified, falling back to FCC (USA)!"
			       " Please report if this sounds fishy!\n",
				adev->reg_dom_id);
		i = 0;
		adev->reg_dom_id = acx_reg_domain_ids[i];

		/* since there was a mismatch, we need to force updating */
		do_set = 1;
	}

	if (do_set) {
		acx_ie_generic_t dom;
		dom.m.bytes[0] = adev->reg_dom_id;
		acx_s_configure(adev, &dom, ACX1xx_IE_DOT11_CURRENT_REG_DOMAIN);
	}

	adev->reg_dom_chanmask = reg_domain_channel_masks[i];

	mask = (1 << (adev->channel - 1));
	if (!(adev->reg_dom_chanmask & mask)) {
	/* hmm, need to adjust our channel to reside within domain */
		mask = 1;
		for (i = 1; i <= 14; i++) {
			if (adev->reg_dom_chanmask & mask) {
				printk("acx: %s: adjusting the selected channel from %d "
					"to %d due to the new regulatory domain\n",
					wiphy_name(adev->ieee->wiphy), adev->channel, i);
				adev->channel = i;
				break;
			}
			mask <<= 1;
		}
	}
}

static void acx111_s_sens_radio_16_17(acx_device_t * adev)
{
	u32 feature1, feature2;

	if ((adev->sensitivity < 1) || (adev->sensitivity > 3)) {
		printk("acx: %s: invalid sensitivity setting (1..3), "
		       "setting to 1\n", wiphy_name(adev->ieee->wiphy));
		adev->sensitivity = 1;
	}
	acx111_s_get_feature_config(adev, &feature1, &feature2);
	CLEAR_BIT(feature1, FEATURE1_LOW_RX | FEATURE1_EXTRA_LOW_RX);
	if (adev->sensitivity > 1)
		SET_BIT(feature1, FEATURE1_LOW_RX);
	if (adev->sensitivity > 2)
		SET_BIT(feature1, FEATURE1_EXTRA_LOW_RX);
	acx111_s_feature_set(adev, feature1, feature2);
}


void acx_s_update_card_settings(acx_device_t *adev)
{
	unsigned long flags;
	unsigned int start_scan = 0;
	int i;

	FN_ENTER;

	log(L_DEBUG, "acx: %s: get_mask 0x%08X, set_mask 0x%08X\n",
	    __func__, adev->get_mask, adev->set_mask);

	/* Track dependencies between various settings */

	if (adev->set_mask & (GETSET_MODE | GETSET_RESCAN | GETSET_WEP)) {
		log(L_INIT, "acx: an important setting has been changed. "
		    "The packet templates must also be updated\n");
		SET_BIT(adev->set_mask, SET_TEMPLATES);
	}

	if (adev->set_mask & GETSET_CHANNEL) {
		/* This will actually tune RX/TX to the channel */
		SET_BIT(adev->set_mask, GETSET_RX | GETSET_TX);
		switch (adev->mode) {
		case ACX_MODE_0_ADHOC:
		case ACX_MODE_3_AP:
			/* Beacons contain channel# - update them */
			SET_BIT(adev->set_mask, SET_TEMPLATES);
		}

		switch (adev->mode) {
		case ACX_MODE_0_ADHOC:
		case ACX_MODE_2_STA:
			start_scan = 1;
		}
	}

	/* Apply settings */

	if (adev->get_mask & GETSET_STATION_ID) {
		u8 stationID[4 + ACX1xx_IE_DOT11_STATION_ID_LEN];
		const u8 *paddr;

		acx_s_interrogate(adev, &stationID, ACX1xx_IE_DOT11_STATION_ID);
		paddr = &stationID[4];
//		memcpy(adev->dev_addr, adev->ndev->dev_addr, ETH_ALEN);
		for (i = 0; i < ETH_ALEN; i++) {
			/* we copy the MAC address (reversed in
			 * the card) to the netdevice's MAC
			 * address, and on ifup it will be
			 * copied into iwadev->dev_addr */
			adev->dev_addr[ETH_ALEN - 1 - i] = paddr[i];
		}
		SET_IEEE80211_PERM_ADDR(adev->ieee,adev->dev_addr);
		CLEAR_BIT(adev->get_mask, GETSET_STATION_ID);
	}

	if (adev->get_mask & GETSET_SENSITIVITY) {
		if ((RADIO_RFMD_11 == adev->radio_type)
		    || (RADIO_MAXIM_0D == adev->radio_type)
		    || (RADIO_RALINK_15 == adev->radio_type)) {
			acx_s_read_phy_reg(adev, 0x30, &adev->sensitivity);
		} else {
			log(L_INIT, "acx: don't know how to get sensitivity "
			    "for radio type 0x%02X\n", adev->radio_type);
			adev->sensitivity = 0;
		}
		log(L_INIT, "acx: got sensitivity value %u\n", adev->sensitivity);

		CLEAR_BIT(adev->get_mask, GETSET_SENSITIVITY);
	}

	if (adev->get_mask & GETSET_ANTENNA) {
		u8 antenna[4 + ACX1xx_IE_DOT11_CURRENT_ANTENNA_LEN];

		memset(antenna, 0, sizeof(antenna));
		acx_s_interrogate(adev, antenna,
				  ACX1xx_IE_DOT11_CURRENT_ANTENNA);
		adev->antenna = antenna[4];
		log(L_INIT, "acx: got antenna value 0x%02X\n", adev->antenna);
		CLEAR_BIT(adev->get_mask, GETSET_ANTENNA);
	}

	if (adev->get_mask & GETSET_ED_THRESH) {
		if (IS_ACX100(adev)) {
			u8 ed_threshold[4 + ACX100_IE_DOT11_ED_THRESHOLD_LEN];

			memset(ed_threshold, 0, sizeof(ed_threshold));
			acx_s_interrogate(adev, ed_threshold,
					  ACX100_IE_DOT11_ED_THRESHOLD);
			adev->ed_threshold = ed_threshold[4];
		} else {
			log(L_INIT, "acx: acx111 doesn't support ED\n");
			adev->ed_threshold = 0;
		}
		log(L_INIT, "acx: got Energy Detect (ED) threshold %u\n",
		    adev->ed_threshold);
		CLEAR_BIT(adev->get_mask, GETSET_ED_THRESH);
	}

	if (adev->get_mask & GETSET_CCA) {
		if (IS_ACX100(adev)) {
			u8 cca[4 + ACX1xx_IE_DOT11_CURRENT_CCA_MODE_LEN];

			memset(cca, 0, sizeof(adev->cca));
			acx_s_interrogate(adev, cca,
					  ACX1xx_IE_DOT11_CURRENT_CCA_MODE);
			adev->cca = cca[4];
		} else {
			log(L_INIT, "acx: acx111 doesn't support CCA\n");
			adev->cca = 0;
		}
		log(L_INIT, "acx: got Channel Clear Assessment (CCA) value %u\n",
		    adev->cca);
		CLEAR_BIT(adev->get_mask, GETSET_CCA);
	}

	if (adev->get_mask & GETSET_REG_DOMAIN) {
		acx_ie_generic_t dom;

		acx_s_interrogate(adev, &dom,
				  ACX1xx_IE_DOT11_CURRENT_REG_DOMAIN);
		adev->reg_dom_id = dom.m.bytes[0];
		acx_s_set_sane_reg_domain(adev, 0);
		log(L_INIT, "acx: got regulatory domain 0x%02X\n", adev->reg_dom_id);
		CLEAR_BIT(adev->get_mask, GETSET_REG_DOMAIN);
	}

	if (adev->set_mask & GETSET_STATION_ID) {
		u8 stationID[4 + ACX1xx_IE_DOT11_STATION_ID_LEN];
		u8 *paddr;

		paddr = &stationID[4];
		MAC_COPY(adev->dev_addr, adev->ieee->wiphy->perm_addr);
		for (i = 0; i < ETH_ALEN; i++) {
			/* copy the MAC address we obtained when we noticed
			 * that the ethernet iface's MAC changed
			 * to the card (reversed in
			 * the card!) */
			paddr[i] = adev->dev_addr[ETH_ALEN - 1 - i];
		}
		acx_s_configure(adev, &stationID, ACX1xx_IE_DOT11_STATION_ID);
		CLEAR_BIT(adev->set_mask, GETSET_STATION_ID);
	}

	if (adev->set_mask & SET_STA_LIST) {
		CLEAR_BIT(adev->set_mask, SET_STA_LIST);
	}
	if (adev->set_mask & SET_RATE_FALLBACK) {
		u8 rate[4 + ACX1xx_IE_RATE_FALLBACK_LEN];

		/* configure to not do fallbacks when not in auto rate mode */
		rate[4] =
		    (adev->
		     rate_auto) ? /* adev->txrate_fallback_retries */ 1 : 0;
		log(L_INIT, "acx: updating Tx fallback to %u retries\n", rate[4]);
		acx_s_configure(adev, &rate, ACX1xx_IE_RATE_FALLBACK);
		CLEAR_BIT(adev->set_mask, SET_RATE_FALLBACK);
	}
	if (adev->set_mask & GETSET_TXPOWER) {
		log(L_INIT, "acx: updating the transmit power: %u dBm\n",
		    adev->tx_level_dbm);
		acx_s_set_tx_level(adev, adev->tx_level_dbm);
		CLEAR_BIT(adev->set_mask, GETSET_TXPOWER);
	}

	if (adev->set_mask & GETSET_SENSITIVITY) {
		log(L_INIT, "acx: updating sensitivity value: %u\n",
		    adev->sensitivity);
		switch (adev->radio_type) {
		case RADIO_RFMD_11:
		case RADIO_MAXIM_0D:
		case RADIO_RALINK_15:
			acx_s_write_phy_reg(adev, 0x30, adev->sensitivity);
			break;
		case RADIO_RADIA_16:
		case RADIO_UNKNOWN_17:
			acx111_s_sens_radio_16_17(adev);
			break;
		default:
			log(L_INIT, "acx: don't know how to modify the sensitivity "
			    "for radio type 0x%02X\n", adev->radio_type);
		}
		CLEAR_BIT(adev->set_mask, GETSET_SENSITIVITY);
	}

	if (adev->set_mask & GETSET_ANTENNA) {
		/* antenna */
		u8 antenna[4 + ACX1xx_IE_DOT11_CURRENT_ANTENNA_LEN];

		memset(antenna, 0, sizeof(antenna));
		antenna[4] = adev->antenna;
		log(L_INIT, "acx: updating antenna value: 0x%02X\n", adev->antenna);
		acx_s_configure(adev, &antenna,
				ACX1xx_IE_DOT11_CURRENT_ANTENNA);
		CLEAR_BIT(adev->set_mask, GETSET_ANTENNA);
	}

	if (adev->set_mask & GETSET_ED_THRESH) {
		/* ed_threshold */
		log(L_INIT, "acx: pdating the Energy Detect (ED) threshold: %u\n",
		    adev->ed_threshold);
		if (IS_ACX100(adev)) {
			u8 ed_threshold[4 + ACX100_IE_DOT11_ED_THRESHOLD_LEN];

			memset(ed_threshold, 0, sizeof(ed_threshold));
			ed_threshold[4] = adev->ed_threshold;
			acx_s_configure(adev, &ed_threshold,
					ACX100_IE_DOT11_ED_THRESHOLD);
		} else
			log(L_INIT, "acx: acx111 doesn't support ED\n");
		CLEAR_BIT(adev->set_mask, GETSET_ED_THRESH);
	}

	if (adev->set_mask & GETSET_CCA) {
		/* CCA value */
		log(L_INIT, "acx: updating the Channel Clear Assessment "
		    "(CCA) value: 0x%02X\n", adev->cca);
		if (IS_ACX100(adev)) {
			u8 cca[4 + ACX1xx_IE_DOT11_CURRENT_CCA_MODE_LEN];

			memset(cca, 0, sizeof(cca));
			cca[4] = adev->cca;
			acx_s_configure(adev, &cca,
					ACX1xx_IE_DOT11_CURRENT_CCA_MODE);
		} else
			log(L_INIT, "acx: acx111 doesn't support CCA\n");
		CLEAR_BIT(adev->set_mask, GETSET_CCA);
	}

	if (adev->set_mask & GETSET_LED_POWER) {
		/* Enable Tx */
		log(L_INIT, "acx: updating the power LED status: %u\n", adev->led_power);

		acx_lock(adev, flags); /* acxpci_l_power_led expects that the lock is already taken! */
		if (IS_PCI(adev))
			acxpci_l_power_led(adev, adev->led_power);
		else if (IS_MEM(adev))
			acxmem_l_power_led(adev, adev->led_power);

		CLEAR_BIT(adev->set_mask, GETSET_LED_POWER);
		acx_unlock(adev, flags);
	}

	if (adev->set_mask & GETSET_POWER_80211) {
#if POWER_SAVE_80211
		acx_s_update_80211_powersave_mode(adev);
#endif
		CLEAR_BIT(adev->set_mask, GETSET_POWER_80211);
	}

	if (adev->set_mask & GETSET_CHANNEL) {
		/* channel */
		log(L_INIT, "acx: updating channel to: %u\n", adev->channel);
		CLEAR_BIT(adev->set_mask, GETSET_CHANNEL);
	}

	if (adev->set_mask & GETSET_TX) {
		/* set Tx */
		log(L_DEBUG, "acx: updating TX: %s\n",
		    adev->tx_disabled ? "disable" : "enable");
		if (adev->tx_disabled)
			acx_s_issue_cmd(adev, ACX1xx_CMD_DISABLE_TX, NULL, 0);
		else {
			acx_s_issue_cmd(adev, ACX1xx_CMD_ENABLE_TX,
					&adev->channel, 1);

			/* OW 20091231:
			 * For the acx111 encryption needs to be turned off
			 * using FEATURE2_NO_TXCRYPT | FEATURE2_SNIFFER.
			 * Maybe redundant with GETSET_MODE below.
			 */
			if (IS_ACX111(adev)) {
				acx111_s_feature_on(adev, 0, FEATURE2_NO_TXCRYPT | FEATURE2_SNIFFER);
			}

			acx_wake_queue(adev->ieee, NULL);
		}
		CLEAR_BIT(adev->set_mask, GETSET_TX);
	}

	if (adev->set_mask & GETSET_RX) {
		// OW TODO Add Disable Rx
		/* Enable Rx */
		log(L_DEBUG, "acx: updating: enable Rx on channel: %u\n",
		    adev->channel);
		acx_s_issue_cmd(adev, ACX1xx_CMD_ENABLE_RX, &adev->channel, 1);
		CLEAR_BIT(adev->set_mask, GETSET_RX);
	}

	if (adev->set_mask & GETSET_RETRY) {
		u8 short_retry[4 + ACX1xx_IE_DOT11_SHORT_RETRY_LIMIT_LEN];
		u8 long_retry[4 + ACX1xx_IE_DOT11_LONG_RETRY_LIMIT_LEN];

		log(L_INIT,
		    "acx: updating the short retry limit: %u, long retry limit: %u\n",
		    adev->short_retry, adev->long_retry);
		short_retry[0x4] = adev->short_retry;
		long_retry[0x4] = adev->long_retry;
		acx_s_configure(adev, &short_retry,
				ACX1xx_IE_DOT11_SHORT_RETRY_LIMIT);
		acx_s_configure(adev, &long_retry,
				ACX1xx_IE_DOT11_LONG_RETRY_LIMIT);
		CLEAR_BIT(adev->set_mask, GETSET_RETRY);
	}

	if (adev->set_mask & SET_MSDU_LIFETIME) {
		u8 xmt_msdu_lifetime[4 +
				     ACX1xx_IE_DOT11_MAX_XMIT_MSDU_LIFETIME_LEN];

		log(L_INIT, "acx: updating the tx MSDU lifetime: %u\n",
		    adev->msdu_lifetime);
		*(u32 *) & xmt_msdu_lifetime[4] =
		    cpu_to_le32((u32) adev->msdu_lifetime);
		acx_s_configure(adev, &xmt_msdu_lifetime,
				ACX1xx_IE_DOT11_MAX_XMIT_MSDU_LIFETIME);
		CLEAR_BIT(adev->set_mask, SET_MSDU_LIFETIME);
	}

	if (adev->set_mask & GETSET_REG_DOMAIN) {
		log(L_INIT, "acx: updating the regulatory domain: 0x%02X\n",
		    adev->reg_dom_id);
		acx_s_set_sane_reg_domain(adev, 1);
		CLEAR_BIT(adev->set_mask, GETSET_REG_DOMAIN);
	}

	if (adev->set_mask & GETSET_MODE ) {

		switch (adev->mode) {
		case ACX_MODE_3_AP:
			adev->aid = 0;
			//acx111_s_feature_off(adev, 0,
			//	    FEATURE2_NO_TXCRYPT | FEATURE2_SNIFFER);
			MAC_COPY(adev->bssid, adev->dev_addr);
			acx_s_cmd_join_bssid(adev, adev->dev_addr);
			break;
		case ACX_MODE_MONITOR:
			SET_BIT(adev->set_mask, SET_RXCONFIG | SET_WEP_OPTIONS);
			break;
		case ACX_MODE_0_ADHOC:
		case ACX_MODE_2_STA:

			/* OW 20091231:
			 * For the acx111 encryption needs to be turned off
			 * using FEATURE2_NO_TXCRYPT | FEATURE2_SNIFFER.
			 */
			if (IS_ACX111(adev)) {
				acx111_s_feature_on(adev, 0, FEATURE2_NO_TXCRYPT | FEATURE2_SNIFFER);
			}
			break;
		case ACX_MODE_OFF:
			/* disable both Tx and Rx to shut radio down properly */
			acx_s_issue_cmd(adev, ACX1xx_CMD_DISABLE_TX, NULL, 0);
			acx_s_issue_cmd(adev, ACX1xx_CMD_DISABLE_RX, NULL, 0);

		default:
			break;
		}
		CLEAR_BIT(adev->set_mask, GETSET_MODE);
	}

	if (adev->set_mask & SET_TEMPLATES) {
		switch (adev->mode)
		{
			case ACX_MODE_3_AP:
				acx_s_set_tim_template(adev);
				break;
			default:
				break;
		}
		if (adev->beacon_cache)
		{
			acx_s_set_beacon_template(adev, adev->beacon_cache);
			dev_kfree_skb(adev->beacon_cache);
			adev->beacon_cache = NULL;
		}
		CLEAR_BIT(adev->set_mask, SET_TEMPLATES);
	}

	if (adev->set_mask & SET_RXCONFIG) {
		acx_s_initialize_rx_config(adev);
		CLEAR_BIT(adev->set_mask, SET_RXCONFIG);
	}

	if (adev->set_mask & GETSET_RESCAN) {
		/*		switch (adev->mode) {
		 case ACX_MODE_0_ADHOC:
		 case ACX_MODE_2_STA:
		 start_scan = 1;
		 break;
		 }
		 */
		CLEAR_BIT(adev->set_mask, GETSET_RESCAN);
	}

	if (adev->set_mask & GETSET_WEP) {
		/* encode */

		ie_dot11WEPDefaultKeyID_t dkey;
#ifdef DEBUG_WEP
		struct {
			u16 type;
			u16 len;
			u8 val;
		} ACX_PACKED keyindic;
#endif
		log(L_INIT, "acx: updating WEP key settings\n");

		acx_s_set_wepkey(adev);
		if (adev->wep_enabled) {
			dkey.KeyID = adev->wep_current_index;
			log(L_INIT, "acx: setting WEP key %u as default\n",
			    dkey.KeyID);
			acx_s_configure(adev, &dkey,
					ACX1xx_IE_DOT11_WEP_DEFAULT_KEY_SET);
#ifdef DEBUG_WEP
			keyindic.val = 3;
			acx_s_configure(adev, &keyindic, ACX111_IE_KEY_CHOOSE);
#endif
		}

//		start_scan = 1;
		CLEAR_BIT(adev->set_mask, GETSET_WEP);
	}

	if (adev->set_mask & SET_WEP_OPTIONS) {
		acx100_ie_wep_options_t options;

		if (IS_ACX111(adev)) {
			log(L_DEBUG,
			    "acx: setting WEP Options for acx111 is not supported\n");
		} else {
			log(L_INIT, "acx: setting WEP Options\n");

			/* let's choose maximum setting: 4 default keys,
			 * plus 10 other keys: */
			options.NumKeys =
			    cpu_to_le16(DOT11_MAX_DEFAULT_WEP_KEYS + 10);
			/* don't decrypt default key only,
			 * don't override decryption: */
			options.WEPOption = 0;
			if (adev->mode == ACX_MODE_3_AP) {
				/* don't decrypt default key only,
				 * override decryption mechanism: */
				options.WEPOption = 2;
			}

			acx_s_configure(adev, &options, ACX100_IE_WEP_OPTIONS);
		}
		CLEAR_BIT(adev->set_mask, SET_WEP_OPTIONS);
	}


	/* debug, rate, and nick don't need any handling */
	/* what about sniffing mode?? */

/*	log(L_INIT, "acx: get_mask 0x%08X, set_mask 0x%08X - after update\n",
	    adev->get_mask, adev->set_mask);
*/
/* end: */
	FN_EXIT0;
}

#if 0
/*
 *
 */
static int acx_s_recalib_radio(acx_device_t * adev)
{
	if (IS_ACX111(adev)) {
		acx111_cmd_radiocalib_t cal;

		/* automatic recalibration, choose all methods: */
		cal.methods = cpu_to_le32(0x8000000f);
		/* automatic recalibration every 60 seconds (value in TUs)
		 * I wonder what the firmware default here is? */
		cal.interval = cpu_to_le32(58594);
		return acx_s_issue_cmd_timeo(adev, ACX111_CMD_RADIOCALIB,
					     &cal, sizeof(cal),
					     CMD_TIMEOUT_MS(100));
	} else {
		/* On ACX100, we need to recalibrate the radio
		 * by issuing a GETSET_TX|GETSET_RX */
		if (		/* (OK == acx_s_issue_cmd(adev, ACX1xx_CMD_DISABLE_TX, NULL, 0)) &&
				   (OK == acx_s_issue_cmd(adev, ACX1xx_CMD_DISABLE_RX, NULL, 0)) && */
			   (OK ==
			    acx_s_issue_cmd(adev, ACX1xx_CMD_ENABLE_TX,
					    &adev->channel, 1))
			   && (OK ==
			       acx_s_issue_cmd(adev, ACX1xx_CMD_ENABLE_RX,
					       &adev->channel, 1)))
			return OK;
		return NOT_OK;
	}
}
#endif // if 0
#if 0
static void acx_s_after_interrupt_recalib(acx_device_t * adev)
{
	int res;

	/* this helps with ACX100 at least;
	 * hopefully ACX111 also does a
	 * recalibration here */

	/* clear flag beforehand, since we want to make sure
	 * it's cleared; then only set it again on specific circumstances */
	CLEAR_BIT(adev->after_interrupt_jobs, ACX_AFTER_IRQ_CMD_RADIO_RECALIB);

	/* better wait a bit between recalibrations to
	 * prevent overheating due to torturing the card
	 * into working too long despite high temperature
	 * (just a safety measure) */
	if (adev->recalib_time_last_success
	    && time_before(jiffies, adev->recalib_time_last_success
			   + RECALIB_PAUSE * 60 * HZ)) {
		if (adev->recalib_msg_ratelimit <= 4) {
			printk("acx: %s: less than " STRING(RECALIB_PAUSE)
			       " minutes since last radio recalibration, "
			       "not recalibrating (maybe the card is too hot?)\n",
			       wiphy_name(adev->ieee->wiphy));
			adev->recalib_msg_ratelimit++;
			if (adev->recalib_msg_ratelimit == 5)
				printk("acx: disabling the above message until next recalib\n");
		}
		return;
	}

	adev->recalib_msg_ratelimit = 0;

	/* note that commands sometimes fail (card busy),
	 * so only clear flag if we were fully successful */
	res = acx_s_recalib_radio(adev);
	if (res == OK) {
		printk("acx: %s: successfully recalibrated radio\n",
		       wiphy_name(adev->ieee->wiphy));
		adev->recalib_time_last_success = jiffies;
		adev->recalib_failure_count = 0;
	} else {
		/* failed: resubmit, but only limited
		 * amount of times within some time range
		 * to prevent endless loop */

		adev->recalib_time_last_success = 0;	/* we failed */

		/* if some time passed between last
		 * attempts, then reset failure retry counter
		 * to be able to do next recalib attempt */
		if (time_after
		    (jiffies, adev->recalib_time_last_attempt + 5 * HZ))
			adev->recalib_failure_count = 0;

		if (adev->recalib_failure_count < 5) {
			/* increment inside only, for speedup of outside path */
			adev->recalib_failure_count++;
			adev->recalib_time_last_attempt = jiffies;
			acx_schedule_task(adev,
					  ACX_AFTER_IRQ_CMD_RADIO_RECALIB);
		}
	}
}
#endif // if 0

void acx_e_after_interrupt_task(acx_device_t *adev)
{
	unsigned long flags;

	// OW 20090722 TODO Is all the locking required ???

	FN_ENTER;

	if (!adev->after_interrupt_jobs || !adev->initialized)
		goto end_no_lock;	/* no jobs to do */

	acx_sem_lock(adev);
	acx_lock(adev, flags);

	/* we see lotsa tx errors */
	if (adev->after_interrupt_jobs & ACX_AFTER_IRQ_CMD_RADIO_RECALIB) {
		printk("acx: too many TX errors?\n");
		printk("acx: %s: TODO: ACX_AFTER_IRQ_CMD_RADIO_RECALIB\n", __func__);
//		acx_s_after_interrupt_recalib(adev);
	}

	/* a poor interrupt code wanted to do update_card_settings() */
	if (adev->after_interrupt_jobs & ACX_AFTER_IRQ_UPDATE_CARD_CFG) {
		if (ACX_STATE_IFACE_UP & adev->dev_state_mask) {
			acx_unlock(adev, flags);
			acx_s_update_card_settings(adev);
			acx_lock(adev, flags);
		}
		CLEAR_BIT(adev->after_interrupt_jobs,
			  ACX_AFTER_IRQ_UPDATE_CARD_CFG);
	}

	/* 1) we detected that no Scan_Complete IRQ came from fw, or
	 ** 2) we found too many STAs */
	if (adev->after_interrupt_jobs & ACX_AFTER_IRQ_CMD_STOP_SCAN) {
		log(L_IRQ, "acx: sending a stop scan cmd...\n");

		// OW Scanning is done by mac80211
#if 0
		acx_unlock(adev, flags);
		acx_s_issue_cmd(adev, ACX1xx_CMD_STOP_SCAN, NULL, 0);
		acx_lock(adev, flags);
		/* HACK: set the IRQ bit, since we won't get a
		 * scan complete IRQ any more on ACX111 (works on ACX100!),
		 * since _we_, not a fw, have stopped the scan */
		SET_BIT(adev->irq_status, HOST_INT_SCAN_COMPLETE);
#endif
		CLEAR_BIT(adev->after_interrupt_jobs,
			  ACX_AFTER_IRQ_CMD_STOP_SCAN);
	}

	/* either fw sent Scan_Complete or we detected that
	 ** no Scan_Complete IRQ came from fw. Finish scanning,
	 ** pick join partner if any */
	if (adev->after_interrupt_jobs & ACX_AFTER_IRQ_COMPLETE_SCAN) {
		/* + scan kills current join status - restore it
		 **   (do we need it for STA?) */
		/* + does it happen only with active scans?
		 **   active and passive scans? ALL scans including
		 **   background one? */
		/* + was not verified that everything is restored
		 **   (but at least we start to emit beacons again) */
		CLEAR_BIT(adev->after_interrupt_jobs,
			  ACX_AFTER_IRQ_COMPLETE_SCAN);
	}

	/* STA auth or assoc timed out, start over again */

	if (adev->after_interrupt_jobs & ACX_AFTER_IRQ_RESTART_SCAN) {
		log(L_IRQ, "acx: sending a start_scan cmd...\n");
		CLEAR_BIT(adev->after_interrupt_jobs,
			  ACX_AFTER_IRQ_RESTART_SCAN);
	}

	/* whee, we got positive assoc response! 8) */
	if (adev->after_interrupt_jobs & ACX_AFTER_IRQ_CMD_ASSOCIATE) {
		CLEAR_BIT(adev->after_interrupt_jobs,
			  ACX_AFTER_IRQ_CMD_ASSOCIATE);
	}

	/* others */
	if(adev->after_interrupt_jobs)
	{
		printk("acx: %s: Jobs still to be run: 0x%02X\n",__func__, adev->after_interrupt_jobs);
		adev->after_interrupt_jobs = 0;
	}

	acx_unlock(adev, flags);
	acx_sem_unlock(adev);

	end_no_lock:

	FN_EXIT0;
}


/***********************************************************************
** acx_schedule_task
**
** Schedule the call of the after_interrupt method after leaving
** the interrupt context.
*/
void acx_schedule_task(acx_device_t *adev, unsigned int set_flag) {

	// OW TODO This has currently no effect
	// OW TODO Interrupt handling ...

	//	if (!adev->after_interrupt_jobs)
	//	{
	SET_BIT(adev->after_interrupt_jobs, set_flag);

	// OW Use mac80211 workqueue
	ieee80211_queue_work(adev->ieee, &adev->after_interrupt_task);

	//	}
}


/***********************************************************************
*/
void acx_init_task_scheduler(acx_device_t *adev) {

	/* configure task scheduler */
#if defined(CONFIG_ACX_MAC80211_PCI)
	if (IS_PCI(adev)) {
		INIT_WORK(&adev->after_interrupt_task, acxpci_interrupt_tasklet);
		return;
	}
#endif
#if defined(CONFIG_ACX_MAC80211_USB)
	if (IS_USB(adev)) {
		INIT_WORK(&adev->after_interrupt_task, acxusb_interrupt_tasklet);
		return;
	}
#endif
#if defined(CONFIG_ACX_MAC80211_MEM)
	if (IS_MEM(adev)) {
		INIT_WORK(&adev->after_interrupt_task, acxmem_i_interrupt_tasklet);
		return;
	}
#endif

	logf0(L_ANY, "Unhandled adev device type!\n");
	BUG();

	// OW TODO Interrupt handling ...
	/* OW In case of of tasklet ... but workqueues seem to be prefered
	 tasklet_init(&adev->interrupt_tasklet,
	 (void(*)(unsigned long)) acx_interrupt_tasklet,
	 (unsigned long) adev);
	 */

}


/***********************************************************************
** acx_s_start
*/
void acx_s_start(acx_device_t * adev)
{
	FN_ENTER;

	/*
	 * Ok, now we do everything that can possibly be done with ioctl
	 * calls to make sure that when it was called before the card
	 * was up we get the changes asked for
	 */

	SET_BIT(adev->set_mask, SET_TEMPLATES | SET_STA_LIST | GETSET_WEP
		| GETSET_TXPOWER | GETSET_ANTENNA | GETSET_ED_THRESH |
		GETSET_CCA | GETSET_REG_DOMAIN | GETSET_MODE | GETSET_CHANNEL |
		GETSET_TX | GETSET_RX | GETSET_STATION_ID);

	log(L_INIT, "acx: updating initial settings on iface activation\n");
	acx_s_update_card_settings(adev);

	FN_EXIT0;
}


/***********************************************************************
** acx_update_capabilities
*//*
void acx_update_capabilities(acx_device_t * adev)
{
	u16 cap = 0;

	switch (adev->mode) {
	case ACX_MODE_3_AP:
		SET_BIT(cap, WF_MGMT_CAP_ESS);
		break;
	case ACX_MODE_0_ADHOC:
		SET_BIT(cap, WF_MGMT_CAP_IBSS);
		break;
*/		/* other types of stations do not emit beacons */
/*	}

	if (adev->wep_restricted) {
		SET_BIT(cap, WF_MGMT_CAP_PRIVACY);
	}
	if (adev->cfgopt_dot11ShortPreambleOption) {
		SET_BIT(cap, WF_MGMT_CAP_SHORT);
	}
	if (adev->cfgopt_dot11PBCCOption) {
		SET_BIT(cap, WF_MGMT_CAP_PBCC);
	}
	if (adev->cfgopt_dot11ChannelAgility) {
		SET_BIT(cap, WF_MGMT_CAP_AGILITY);
	}
	log(L_DEBUG, "acx: caps updated from 0x%04X to 0x%04X\n",
	    adev->capabilities, cap);
	adev->capabilities = cap;
}
*/
/**
** Derived from mac80211 code, p54, bcm43xx_mac80211
**
*/
static void acx_s_select_opmode(acx_device_t *adev)
{
	int changed = 0;
	FN_ENTER;

	if (adev->interface.operating) {
		switch (adev->interface.type) {
		case NL80211_IFTYPE_AP:
			if (adev->mode != ACX_MODE_3_AP) {
				adev->mode = ACX_MODE_3_AP;
				changed = 1;
			}
			break;
		case NL80211_IFTYPE_ADHOC:
			if (adev->mode != ACX_MODE_0_ADHOC) {
				adev->mode = ACX_MODE_0_ADHOC;
				changed = 1;
			}
			break;
		case NL80211_IFTYPE_STATION:
			if (adev->mode != ACX_MODE_2_STA) {
				adev->mode = ACX_MODE_2_STA;
				changed = 1;
			}
			break;
		case NL80211_IFTYPE_WDS:
		default:
			if (adev->mode != ACX_MODE_OFF) {
				adev->mode = ACX_MODE_OFF;
				changed = 1;
			}
			break;
		}
	} else {
		if (adev->interface.type == NL80211_IFTYPE_MONITOR) {
			if (adev->mode != ACX_MODE_MONITOR) {
				adev->mode = ACX_MODE_MONITOR;
				changed = 1;
			}
		} else {
			if (adev->mode != ACX_MODE_OFF) {
				adev->mode = ACX_MODE_OFF;
				changed = 1;
			}
		}
	}

	if (changed) {
		SET_BIT(adev->set_mask, GETSET_MODE);
		acx_s_update_card_settings(adev);
		//	acx_schedule_task(adev,	ACX_AFTER_IRQ_UPDATE_CARD_CFG);
	}

	FN_EXIT0;
}

int acx_e_op_add_interface(struct ieee80211_hw *ieee,
		      struct ieee80211_if_init_conf *conf)
{
	acx_device_t *adev = ieee2adev(ieee);
	unsigned long flags;
	int err = -EOPNOTSUPP;

	char mac[] = MACSTR; // approximate max length

	FN_ENTER;
	acx_sem_lock(adev);
	acx_lock(adev, flags);

	if (conf->type == NL80211_IFTYPE_MONITOR) {
		adev->interface.monitor++;
	} else {
		if (adev->interface.operating)
			goto out_unlock;
		adev->interface.operating = 1;
		adev->interface.mac_addr = conf->mac_addr;
		adev->interface.type = conf->type;
	}
//	adev->mode = conf->type;

	acx_unlock(adev, flags);

	if (adev->initialized)
		acx_s_select_opmode(adev);

	acx_lock(adev, flags);

	err = 0;

	printk(KERN_INFO "acx: Virtual interface added "
	       "(type: 0x%08X, MAC: %s)\n",
	       conf->type,
	       acx_print_mac(mac, conf->mac_addr));

    out_unlock:
	acx_unlock(adev, flags);
	acx_sem_unlock(adev);

	FN_EXIT0;
	return err;
}

void acx_e_op_remove_interface(struct ieee80211_hw *hw,
			  struct ieee80211_if_init_conf *conf)
{
	acx_device_t *adev = ieee2adev(hw);

	char mac[] = MACSTR; // approximate max length

	FN_ENTER;
	acx_sem_lock(adev);

	if (conf->type == NL80211_IFTYPE_MONITOR) {
		adev->interface.monitor--;
//      assert(bcm->interface.monitor >= 0);
	} else {
		adev->interface.operating = 0;
	}

	log(L_DEBUG, "acx: %s: interface.operating=%d, conf->type=%d\n",
			__func__,
			adev->interface.operating, conf->type);

	if (adev->initialized)
		acx_s_select_opmode(adev);

	log(L_ANY, "acx: Virtual interface removed: "
	       "type=%d, MAC=%s\n",
	       conf->type, acx_print_mac(mac, conf->mac_addr));

	acx_sem_unlock(adev);

	// OW 20100131 Flush of mac80211 normally done by mac80211
	// flush_scheduled_work();

	FN_EXIT0;
}

int acx_net_reset(struct ieee80211_hw *ieee)
{
	acx_device_t *adev = ieee2adev(ieee);
	FN_ENTER;
	if (IS_PCI(adev))
		acxpci_s_reset_dev(adev);
	if (IS_MEM(adev))
		acxmem_s_reset_dev(adev);
	else
		TODO();

	FN_EXIT0;
	return 0;
}

int acx_selectchannel(acx_device_t *adev, u8 channel, int freq)
{
	int result;

	FN_ENTER;

	adev->rx_status.freq = freq;
	adev->rx_status.band = IEEE80211_BAND_2GHZ;

	adev->channel = channel;
	/* hmm, the following code part is strange, but this is how
	 * it was being done before... */
	log(L_IOCTL, "acx: Changing to channel %d\n", channel);
	SET_BIT(adev->set_mask, GETSET_CHANNEL);
	result = -EINPROGRESS;	/* need to call commit handler */

	FN_EXIT1(result);
	return result;
}

int acx_e_op_config(struct ieee80211_hw *hw, u32 changed) {
	acx_device_t *adev = ieee2adev(hw);
	struct ieee80211_conf *conf = &hw->conf;
	unsigned long flags;

	FN_ENTER;
	acx_sem_lock(adev);

	//FIXME();
	if (!adev->initialized)
		goto end_sem_unlock;

	acx_lock(adev, flags);

	if (conf->channel->hw_value != adev->channel) {

		acx_unlock(adev, flags);
		acx_selectchannel(adev, conf->channel->hw_value,
				conf->channel->center_freq);
		acx_lock(adev, flags);

		/*		acx_schedule_task(adev,
		 ACX_AFTER_IRQ_UPDATE_CARD_CFG
		 *//*+ ACX_AFTER_IRQ_RESTART_SCAN *//*);*/

	}
	/*
	 if (conf->short_slot_time != adev->short_slot) {
	 //                assert(phy->type == BCM43xx_PHYTYPE_G);
	 if (conf->short_slot_time)
	 acx_short_slot_timing_enable(adev);
	 else
	 acx_short_slot_timing_disable(adev);
	 acx_schedule_task(adev, ACX_AFTER_IRQ_UPDATE_CARD_CFG);
	 }
	 */

	// OW FIXME mac80211 depreciates radio_enabled. Perhaps related to rfkill changes ?
	//adev->tx_disabled = !conf->radio_enabled;
	adev->tx_disabled = 0;

	/*	if (conf->power_level != 0){
	 adev->tx_level_dbm = conf->power_level;
	 acx_s_set_tx_level(adev, adev->tx_level_dbm);
	 SET_BIT(adev->set_mask,GETSET_TXPOWER);
	 //acx_schedule_task(adev, ACX_AFTER_IRQ_UPDATE_CARD_CFG);
	 }
	 */
	//FIXME: This does not seem to wake up:
#if 0
	if (conf->power_level == 0)
	{
		if (radio->enabled)
		bcm43xx_radio_turn_off(bcm);
	}
	else
	{
		if (!radio->enabled)
		bcm43xx_radio_turn_on(bcm);
	}
#endif

	//TODO: phymode
	//TODO: antennas
	acx_unlock(adev, flags);

	if (adev->set_mask > 0) {
		acx_s_update_card_settings(adev);
	}

	end_sem_unlock:

	acx_sem_unlock(adev);
	FN_EXIT0;

	return 0;
}

extern void acx_e_op_bss_info_changed(struct ieee80211_hw *hw,
		struct ieee80211_vif *vif, struct ieee80211_bss_conf *info, u32 changed) {
	acx_device_t *adev = ieee2adev(hw);

	unsigned long flags;
	int err = -ENODEV;

	struct sk_buff *skb_tmp;

	FN_ENTER;
	acx_sem_lock(adev);

	if (!adev->interface.operating)
		goto end_sem_unlock;

	if (adev->initialized)
		acx_s_select_opmode(adev);

	acx_lock(adev, flags);

	if ((vif->type != NL80211_IFTYPE_MONITOR) && (adev->vif == vif)) {
		if (info->bssid) {
			adev->interface.bssid = info->bssid;
			MAC_COPY(adev->bssid, info->bssid);
		}
	}
	if ((vif->type == NL80211_IFTYPE_AP) && (adev->vif == vif)) {

		if (info->bssid && (strlen(info->bssid) > 0)) {
			adev->essid_len = strlen(info->bssid);
			memcpy(adev->essid, info->bssid, strlen(info->bssid));
			SET_BIT(adev->set_mask, SET_TEMPLATES);
		}
	}

	// OW: Beacon update condition, as seen in b43/main.c
	if (info->enable_beacon) {

		skb_tmp = ieee80211_beacon_get(hw, vif);
		if (skb_tmp != 0) {
			adev->beacon_interval = DEFAULT_BEACON_INTERVAL;
			// OW adev->beacon_cache = conf->beacon;
			adev->beacon_cache = skb_tmp;
			SET_BIT(adev->set_mask, SET_TEMPLATES);
		}

		if (info->beacon_int != adev->beacon_interval)
			adev->beacon_interval = info->beacon_int;

	}

	acx_unlock(adev, flags);

	if (adev->set_mask != 0)
		acx_s_update_card_settings(adev);
	//		acx_schedule_task(adev, ACX_AFTER_IRQ_UPDATE_CARD_CFG);
	err = 0;

	end_sem_unlock:

	acx_sem_unlock(adev);
	FN_EXIT1(err);

	return;
}

int acx_e_op_get_tx_stats(struct ieee80211_hw *hw,
			 struct ieee80211_tx_queue_stats *stats)
{
	acx_device_t *adev = ieee2adev(hw);
	int err = -ENODEV;

	FN_ENTER;
	acx_sem_lock(adev);

	stats->len = 0;
	stats->limit = TX_CNT;
	stats->count = 0;

	acx_sem_unlock(adev);
	FN_EXIT0;
	return err;
}

int acx_e_conf_tx(struct ieee80211_hw *hw,
		u16 queue, const struct ieee80211_tx_queue_params *params)
{
	acx_device_t *adev = ieee2adev(hw);
	FN_ENTER;
	acx_sem_lock(adev);
    // TODO
  	acx_sem_unlock(adev);
	FN_EXIT0;
	return 0;
}

// OW TODO Refactor to "acx_"keymac_write, even if not used currently
// Or it could actually go out, since crypto and keys are handeled by mac80211
static void keymac_write(acx_device_t * adev, u16 index, const u32 * addr)
{
	/* for keys 0-3 there is no associated mac address */
	if (index < 4)
		return;

	index -= 4;
	if (1) {
		TODO();
/*
                bcm43xx_shm_write32(bcm,
                                    BCM43xx_SHM_HWMAC,
                                    index * 2,
                                    cpu_to_be32(*addr));
                bcm43xx_shm_write16(bcm,
                                    BCM43xx_SHM_HWMAC,
                                    (index * 2) + 1,
                                    cpu_to_be16(*((u16 *)(addr + 1))));
*/
	} else {
		if (index < 8) {
			TODO();	/* Put them in the macaddress filter */
		} else {
			TODO();
			/* Put them BCM43xx_SHM_SHARED, stating index 0x0120.
			   Keep in mind to update the count of keymacs in 0x003 */
		}
	}
}

/**
** Derived from mac80211 code, p54, bcm43xx_mac80211
**
*/

int acx_clear_keys(acx_device_t * adev)
{
	static const u32 zero_mac[2] = { 0 };
	unsigned int i, j, nr_keys = 54;
	u16 offset;

	/* FixMe:Check for Number of Keys available */

//        assert(nr_keys <= ARRAY_SIZE(adev->key));

	for (i = 0; i < nr_keys; i++) {
		adev->key[i].enabled = 0;
		/* returns for i < 4 immediately */
		keymac_write(adev, i, zero_mac);
/*
                bcm43xx_shm_write16(adev, BCM43xx_SHM_SHARED,
                                    0x100 + (i * 2), 0x0000);
*/
		for (j = 0; j < 8; j++) {
			offset =
			    adev->security_offset + (j * 4) +
			    (i * ACX_SEC_KEYSIZE);
/*
                        bcm43xx_shm_write16(bcm, BCM43xx_SHM_SHARED,
                                            offset, 0x0000);
*/
		}
	}
	return 1;
}

/**
** Derived from mac80211 code, p54, bcm43xx_mac80211
**
*/

int acx_key_write(acx_device_t *adev,
		  u16 index, u8 algorithm,
		  const struct ieee80211_key_conf *key, const u8 *mac_addr)
{
	int result;

	FN_ENTER;

//       log(L_IOCTL, "acx: set encoding flags=0x%04X, size=%d, key: %s\n",
//                       dwrq->flags, dwrq->length, extra ? "set" : "No key");

//	acx_sem_lock(adev);

	// index = (dwrq->flags & IW_ENCODE_INDEX) - 1;
	if (key->keylen > 0) {

		/* if index is 0 or invalid, use default key */
		if (index > 3)
			index = (int) adev->wep_current_index;

		if ((algorithm == ACX_SEC_ALGO_WEP) ||
			  (algorithm == ACX_SEC_ALGO_WEP104)) {

			switch (key->keylen) {
			case 40 / 8:
				/* WEP 40-bit =
				 40-bit  entered key + 24 bit IV = 64-bit */
				//adev->wep_keys[index].size = 13;

				adev->wep_keys[index].size = 5;
				break;

			case 104 / 8:
				/* WEP 104-bit =
				 104-bit entered key + 24-bit IV = 128-bit */
				// adev->wep_keys[index].size = 29;
				adev->wep_keys[index].size = 13;
				break;

			case 128 / 8:
				/* WEP 128-bit =
				 128-bit entered key + 24 bit IV = 152-bit */
				adev->wep_keys[index].size = 29;
				break;

			default:
				adev->wep_keys[index].size = 0;
				return -EINVAL; /* shouldn't happen */
			}

			memset(adev->wep_keys[index].key, 0,
					sizeof(adev->wep_keys[index].key));
			memcpy(adev->wep_keys[index].key, key->key, key->keylen);

			adev->wep_current_index = index;

		} else {
			/* set transmit key */
			if (index <= 3)
				adev->wep_current_index = index;
		}

	}

	adev->wep_enabled = ((algorithm == ACX_SEC_ALGO_WEP) ||	(algorithm == ACX_SEC_ALGO_WEP104));

/*
        adev->wep_enabled = !(dwrq->flags & IW_ENCODE_DISABLED);

        if (algorithm & IW_ENCODE_OPEN) {
                adev->auth_alg = WLAN_AUTH_ALG_OPENSYSTEM;
                adev->wep_restricted = 0;

        } else if (algorithm & IW_ENCODE_RESTRICTED) {
                adev->auth_alg = WLAN_AUTH_ALG_SHAREDKEY;
                adev->wep_restricted = 1;
        }
*/
//	adev->auth_alg = algorithm;
	/* set flag to make sure the card WEP settings get updated */

	/* OW
	 if (adev->wep_enabled) {
		SET_BIT(adev->set_mask, GETSET_WEP);
		acx_s_update_card_settings(adev);
		acx_schedule_task(adev, ACX_AFTER_IRQ_UPDATE_CARD_CFG);
	 }
  */

	/*
        log(L_IOCTL, "acx: len=%d, key at 0x%p, flags=0x%X\n",
                dwrq->length, extra, dwrq->flags);
        for (index = 0; index <= 3; index++) {
                if (adev->wep_keys[index].size) {
                        log(L_IOCTL,    "acx: index=%d, size=%d, key at 0x%p\n",
                                adev->wep_keys[index].index,
                                (int) adev->wep_keys[index].size,
                                adev->wep_keys[index].key);
                }
        }
*/

	result = -EINPROGRESS;
//	acx_sem_unlock(adev);

	FN_EXIT1(result);
	return result;


}

int acx_e_op_set_key	(struct ieee80211_hw *hw, enum set_key_cmd cmd,
			       struct ieee80211_vif *vif, struct ieee80211_sta *sta,
			       struct ieee80211_key_conf *key) {

	struct acx_device *adev = ieee2adev(hw);
	// unsigned long flags;
	u8 algorithm;
	// u16 index;
	int err = -EINVAL;

	FN_ENTER;
	acx_sem_lock(adev);

	/* OW Mac80211 SW crypto support:
	 *
	 * For the moment we do all crypto in sw with mac80211.
	 * Cpu cycles are cheap, and acx100 can do only WEP in hw anyway.
	 * TODO WEP hw support can still be added later, if required.
	 */

	switch (key->alg) {
	case ALG_WEP:
		if (key->keylen == 5) {
			algorithm = ACX_SEC_ALGO_WEP;
			log(L_INIT, "acx: %s: algorithm=%i: %s\n", __func__, algorithm, "ACX_SEC_ALGO_WEP");
		} else {
			algorithm = ACX_SEC_ALGO_WEP104;
			log(L_INIT, "acx: %s: algorithm=%i: %s\n", __func__, algorithm, "ACX_SEC_ALGO_WEP104");
		}
		// OW Let's try WEP in mac80211 sw
		err = -EOPNOTSUPP;
		break;

	case ALG_TKIP:
		algorithm = ACX_SEC_ALGO_TKIP;
		log(L_INIT, "acx: %s: algorithm=%i: %s\n", __func__, algorithm, "ACX_SEC_ALGO_TKIP");
		err = -EOPNOTSUPP;
		break;

		break;
	case ALG_CCMP:
		algorithm = ACX_SEC_ALGO_AES;
		log(L_INIT, "acx: %s: algorithm=%i: %s\n", __func__, algorithm, "ACX_SEC_ALGO_AES");
		err = -EOPNOTSUPP;
		break;

	default:
		FIXME();
		algorithm = ACX_SEC_ALGO_NONE;
		err = -EOPNOTSUPP;
	}

	acx_sem_unlock(adev);
	FN_EXIT0;
	return err;

	// OW Everything below this lines, doesn't matter anymore for the moment.
#if 0
	index = (u8) (key->keyidx);
	if (index >= ARRAY_SIZE(adev->key))
		goto out;

	acx_lock(adev, flags);

	switch (cmd) {
	case SET_KEY:

		err = acx_key_write(adev, index, algorithm, key, addr);
		if (err != -EINPROGRESS)
			goto out_unlock;

		key->hw_key_idx = index;

		/*		CLEAR_BIT(key->flags, IEEE80211_KEY_FORCE_SW_ENCRYPT);*/
		/*		if (CHECK_BIT(key->flags, IEEE80211_KEY_DEFAULT_TX_KEY))
		 adev->default_key_idx = index;*/

		SET_BIT(key->flags, IEEE80211_KEY_FLAG_GENERATE_IV);
		adev->key[index].enabled = 1;
		err = 0;

		break;

	case DISABLE_KEY:
		adev->key[index].enabled = 0;
		err = 0;
		break;
		/* case ENABLE_COMPRESSION:
		 case DISABLE_COMPRESSION:
		 err = 0;
		 break; */
	}

	out_unlock:
			acx_unlock(adev, flags);

	if (adev->wep_enabled) {
			SET_BIT(adev->set_mask, GETSET_WEP);
			acx_s_update_card_settings(adev);
			//acx_schedule_task(adev, ACX_AFTER_IRQ_UPDATE_CARD_CFG);
	}

	out:

	FN_EXIT0;

	return err;
#endif

}



/***********************************************************************
** Common function to parse ALL configoption struct formats
** (ACX100 and ACX111; FIXME: how to make it work with ACX100 USB!?!?).
** FIXME: logging should be removed here and added to a /proc file instead
**
** Look into bcm43xx
*/
void
acx_s_parse_configoption(acx_device_t * adev,
			 const acx111_ie_configoption_t * pcfg)
{
	const u8 *pEle;
	int i;
	int is_acx111 = IS_ACX111(adev);

	if (acx_debug & L_DEBUG) {
		printk("acx: configoption struct content:\n");
		acx_dump_bytes(pcfg, sizeof(*pcfg));
	}

	if ((is_acx111 && (adev->eeprom_version == 5))
	    || (!is_acx111 && (adev->eeprom_version == 4))
	    || (!is_acx111 && (adev->eeprom_version == 5))) {
		/* these versions are known to be supported */
	} else {
		printk("acx: unknown chip and EEPROM version combination (%s, v%d), "
		       "don't know how to parse config options yet. "
		       "Please report\n", is_acx111 ? "ACX111" : "ACX100",
		       adev->eeprom_version);
		return;
	}

	/* first custom-parse the first part which has chip-specific layout */

	pEle = (const u8 *)pcfg;

	pEle += 4;		/* skip (type,len) header */

	memcpy(adev->cfgopt_NVSv, pEle, sizeof(adev->cfgopt_NVSv));
	pEle += sizeof(adev->cfgopt_NVSv);

	if (is_acx111) {
		adev->cfgopt_NVS_vendor_offs = le16_to_cpu(*(u16 *) pEle);
		pEle += sizeof(adev->cfgopt_NVS_vendor_offs);

		adev->cfgopt_probe_delay = 200;	/* good default value? */
		pEle += 2;	/* FIXME: unknown, value 0x0001 */
	} else {
		memcpy(adev->cfgopt_MAC, pEle, sizeof(adev->cfgopt_MAC));
		pEle += sizeof(adev->cfgopt_MAC);

		adev->cfgopt_probe_delay = le16_to_cpu(*(u16 *) pEle);
		pEle += sizeof(adev->cfgopt_probe_delay);
		if ((adev->cfgopt_probe_delay < 100)
		    || (adev->cfgopt_probe_delay > 500)) {
			printk("acx: strange probe_delay value %d, "
			       "tweaking to 200\n", adev->cfgopt_probe_delay);
			adev->cfgopt_probe_delay = 200;
		}
	}

	adev->cfgopt_eof_memory = le32_to_cpu(*(u32 *) pEle);
	pEle += sizeof(adev->cfgopt_eof_memory);

	printk("acx: NVS_vendor_offs:%04X probe_delay:%d eof_memory:%d\n",
	       adev->cfgopt_NVS_vendor_offs,
	       adev->cfgopt_probe_delay, adev->cfgopt_eof_memory);

	adev->cfgopt_dot11CCAModes = *pEle++;
	adev->cfgopt_dot11Diversity = *pEle++;
	adev->cfgopt_dot11ShortPreambleOption = *pEle++;
	adev->cfgopt_dot11PBCCOption = *pEle++;
	adev->cfgopt_dot11ChannelAgility = *pEle++;
	adev->cfgopt_dot11PhyType = *pEle++;
	adev->cfgopt_dot11TempType = *pEle++;
	printk("acx: CCAModes:%02X Diversity:%02X ShortPreOpt:%02X "
	       "PBCC:%02X ChanAgil:%02X PHY:%02X Temp:%02X\n",
	       adev->cfgopt_dot11CCAModes,
	       adev->cfgopt_dot11Diversity,
	       adev->cfgopt_dot11ShortPreambleOption,
	       adev->cfgopt_dot11PBCCOption,
	       adev->cfgopt_dot11ChannelAgility,
	       adev->cfgopt_dot11PhyType, adev->cfgopt_dot11TempType);

	/* then use common parsing for next part which has common layout */

	pEle++;			/* skip table_count (6) */

	if (IS_MEM(adev) && IS_ACX100(adev))
	{
	/*
	 * For iPaq hx4700 Generic Slave F/W 1.10.7.K.  I'm not sure if these
	 * 4 extra bytes are before the dot11 things above or after, so I'm just
	 * going to guess after.  If someone sees these aren't reasonable numbers,
	 * please fix this.
	 * The area from which the dot11 values above are read contains:
	 * 04 01 01 01 00 05 01 06 00 02 01 02
	 * the 8 dot11 reads above take care of 8 of them, but which 8...
	 */
		pEle += 4;
	}

	adev->cfgopt_antennas.type = pEle[0];
	adev->cfgopt_antennas.len = pEle[1];
	printk("acx: AntennaID:%02X Len:%02X Data:",
	       adev->cfgopt_antennas.type, adev->cfgopt_antennas.len);
	for (i = 0; i < pEle[1]; i++) {
		adev->cfgopt_antennas.list[i] = pEle[i + 2];
		printk("%02X ", pEle[i + 2]);
	}
	printk("\n");

	pEle += pEle[1] + 2;
	adev->cfgopt_power_levels.type = pEle[0];
	adev->cfgopt_power_levels.len = pEle[1];
	printk("acx: PowerLevelID:%02X Len:%02X Data:",
	       adev->cfgopt_power_levels.type, adev->cfgopt_power_levels.len);
	for (i = 0; i < pEle[1]; i++) {
		adev->cfgopt_power_levels.list[i] =
		    le16_to_cpu(*(u16 *) & pEle[i * 2 + 2]);
		printk("%04X ", adev->cfgopt_power_levels.list[i]);
	}
	printk("\n");

	pEle += pEle[1] * 2 + 2;
	adev->cfgopt_data_rates.type = pEle[0];
	adev->cfgopt_data_rates.len = pEle[1];
	printk("acx: DataRatesID:%02X Len:%02X Data:",
	       adev->cfgopt_data_rates.type, adev->cfgopt_data_rates.len);
	for (i = 0; i < pEle[1]; i++) {
		adev->cfgopt_data_rates.list[i] = pEle[i + 2];
		printk("%02X ", pEle[i + 2]);
	}
	printk("\n");

	pEle += pEle[1] + 2;
	adev->cfgopt_domains.type = pEle[0];
	adev->cfgopt_domains.len = pEle[1];

	if (IS_MEM(adev) && IS_ACX100(adev))
	{
	/*
	   * For iPaq hx4700 Generic Slave F/W 1.10.7.K.
	 * There's an extra byte between this structure and the next
	 * that is not accounted for with this structure's length.  It's
	 * most likely a bug in the firmware, but we can fix it here
	 * by bumping the length of this field by 1.
	 */
		adev->cfgopt_domains.len++;
	}

	printk("acx: DomainID:%02X Len:%02X Data:",
	       adev->cfgopt_domains.type, adev->cfgopt_domains.len);
	for (i = 0; i < adev->cfgopt_domains.len; i++) {
		adev->cfgopt_domains.list[i] = pEle[i + 2];
		printk("%02X ", pEle[i + 2]);
	}
	printk("\n");

	pEle += adev->cfgopt_domains.len + 2;
	adev->cfgopt_product_id.type = pEle[0];
	adev->cfgopt_product_id.len = pEle[1];
	for (i = 0; i < pEle[1]; i++) {
		adev->cfgopt_product_id.list[i] = pEle[i + 2];
	}
	printk("acx: ProductID:%02X Len:%02X Data:%.*s\n",
	       adev->cfgopt_product_id.type, adev->cfgopt_product_id.len,
	       adev->cfgopt_product_id.len,
	       (char *)adev->cfgopt_product_id.list);

	pEle += pEle[1] + 2;
	adev->cfgopt_manufacturer.type = pEle[0];
	adev->cfgopt_manufacturer.len = pEle[1];
	for (i = 0; i < pEle[1]; i++) {
		adev->cfgopt_manufacturer.list[i] = pEle[i + 2];
	}
	printk("acx: ManufacturerID:%02X Len:%02X Data:%.*s\n",
	       adev->cfgopt_manufacturer.type, adev->cfgopt_manufacturer.len,
	       adev->cfgopt_manufacturer.len,
	       (char *)adev->cfgopt_manufacturer.list);
/*
	printk("acx: EEPROM part:\n");
	for (i=0; i<58; i++) {
		printk("%02X =======>  0x%02X\n",
			i, (u8 *)adev->cfgopt_NVSv[i-2]);
	}
*/
}

/*
 * OW Debugging
 */

/***********************************************************************
** acx_get_packet_type_string
*/
const char* acx_get_packet_type_string(u16 fc)
{
	static const char * const mgmt_arr[] = {
		"MGMT/AssocReq", "MGMT/AssocResp", "MGMT/ReassocReq",
		"MGMT/ReassocResp", "MGMT/ProbeReq", "MGMT/ProbeResp",
		"MGMT/UNKNOWN", "MGMT/UNKNOWN", "MGMT/Beacon", "MGMT/ATIM",
		"MGMT/Disassoc", "MGMT/Authen", "MGMT/Deauthen"
	};
	static const char * const ctl_arr[] = {
		"CTL/PSPoll", "CTL/RTS", "CTL/CTS", "CTL/Ack", "CTL/CFEnd",
		"CTL/CFEndCFAck"
	};
	static const char * const data_arr[] = {
		"DATA/DataOnly", "DATA/Data CFAck", "DATA/Data CFPoll",
		"DATA/Data CFAck/CFPoll", "DATA/Null", "DATA/CFAck",
		"DATA/CFPoll", "DATA/CFAck/CFPoll"
	};
	const char *str;
	u8 fstype = (WF_FC_FSTYPE & fc) >> 4;
	u8 ctl;

	switch (WF_FC_FTYPE & fc) {
	case WF_FTYPE_MGMT:
		if (fstype < VEC_SIZE(mgmt_arr))
			str = mgmt_arr[fstype];
		else
			str = "MGMT/UNKNOWN";
		break;
	case WF_FTYPE_CTL:
		ctl = fstype - 0x0a;
		if (ctl < VEC_SIZE(ctl_arr))
			str = ctl_arr[ctl];
		else
			str = "CTL/UNKNOWN";
		break;
	case WF_FTYPE_DATA:
		if (fstype < VEC_SIZE(data_arr))
			str = data_arr[fstype];
		else
			str = "DATA/UNKNOWN";
		break;
	default:
		str = "UNKNOWN";
		break;
	}
	return str;
}


/***********************************************************************
** Linux Kernel Specific
*/
static int __init acx_e_init_module(void)
{
	int r1, r2, r3;

	acx_struct_size_check();

	printk("acx: this driver is still EXPERIMENTAL\n"
	       "acx: please read the README file and/or "
	       "go to http://acx100.sourceforge.net/wiki for "
	       "further information\n");

#if defined(CONFIG_ACX_MAC80211_PCI)
	r1 = acxpci_e_init_module();
#else
	r1 = -EINVAL;
#endif

#if defined(CONFIG_ACX_MAC80211_USB)
	r2 = acxusb_e_init_module();
#else
	r2 = -EINVAL;
#endif

#if defined(CONFIG_ACX_MAC80211_MEM)
	r3 = acxmem_e_init_module();
#else
	r3 = -EINVAL;
#endif

	if (r3 && r2 && r1)		/* all three failed! */
	{
		printk ("acx: r1_pci=%i, r2_usb=%i, r3_mem=%i\n", r1, r2, r3);
		return -EINVAL;
	}

	/* return success if at least one succeeded */
	return 0;
}

static void __exit acx_e_cleanup_module(void)
{
#if defined(CONFIG_ACX_MAC80211_PCI)
	acxpci_e_cleanup_module();
#endif

#if defined(CONFIG_ACX_MAC80211_USB)
	acxusb_e_cleanup_module();
#endif

#if defined(CONFIG_ACX_MAC80211_MEM)
	acxmem_e_cleanup_module();
#endif

}

module_init(acx_e_init_module)
    module_exit(acx_e_cleanup_module)
