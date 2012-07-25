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
#include "acx_debug.h"

#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/netdevice.h>
#include <linux/wireless.h>
#include <linux/pm.h>
#include <linux/vmalloc.h>
#include <linux/firmware.h>
#include <linux/ethtool.h>
#include <linux/pci.h>
#include <linux/nl80211.h>
#include <linux/ieee80211.h>
#include <linux/etherdevice.h>

#include <net/mac80211.h>

#include "acx.h"
#include "usb.h"
#include "merge.h"
#include "mem.h"
#include "pci.h"
#include "cmd.h"
#include "ie.h"
#include "utils.h"
#include "cardsetting.h"
#include "main.h"
#include "debug.h"

/*
 * BOM Config
 * ==================================================
 */

/*
 * BOM Prototypes
 * ... static and also none-static for overview reasons (maybe not best practice ...)
 * ==================================================
 */

/* Locking */
#ifdef OW_20100613_OBSELETE_ACXLOCK_REMOVE
void acx_lock_unhold(void);
void acx_sem_unhold(void);
void acx_lock_debug(acx_device_t *adev, const char *where);
void acx_unlock_debug(acx_device_t *adev, const char *where);
static inline const char *acx_sanitize_str(const char *s);
#endif

/* Logging */
void log_fn_enter(const char *funcname);
void log_fn_exit(const char *funcname);
void log_fn_exitv(const char *funcname, int v);
//-void acx_dump_bytes(const void *data, int num);
//-const char *acx_cmd_status_str(unsigned int state);

/* Firmware, EEPROM, Phy */
MODULE_FIRMWARE("tiacx111");
MODULE_FIRMWARE("tiacx111c16");
MODULE_FIRMWARE("tiacx111r16");

//-void acx_get_firmware_version(acx_device_t *adev);
//-void acx_display_hardware_details(acx_device_t *adev);
//-firmware_image_t *acx_read_fw(struct device *dev, const char *file, u32 * size);
//-void acx_parse_configoption(acx_device_t *adev, const acx111_ie_configoption_t * pcfg);
int acx_read_phy_reg(acx_device_t *adev, u32 reg, u8 *charbuf);
int acx_write_phy_reg(acx_device_t *adev, u32 reg, u8 value);

/* Rx Path */

/* Tx Path */
#if CONFIG_ACX_MAC80211_VERSION < KERNEL_VERSION(2, 6, 39)
int acx_op_tx(struct ieee80211_hw *hw, struct sk_buff *skb);
#else
void acx_op_tx(struct ieee80211_hw *hw, struct sk_buff *skb);
#endif

/* Crypto */
#ifdef UNUSED
static void acx100_set_wepkey(acx_device_t * adev);
static void acx111_set_wepkey(acx_device_t * adev);
static void acx_set_wepkey(acx_device_t * adev);
#endif

/* OW, 20100704, Obselete, TBC for cleanup */
#if 0
static void acx_keymac_write(acx_device_t * adev, u16 index, const u32 * addr);
int acx_clear_keys(acx_device_t * adev);
int acx_key_write(acx_device_t *adev, u16 index, u8 algorithm, const struct ieee80211_key_conf *key, const u8 *mac_addr);
#endif

/* Irq Handling, Timer */
//-void acx_init_task_scheduler(acx_device_t *adev);
//-void acx_after_interrupt_task(acx_device_t *adev);
//-void acx_schedule_task(acx_device_t *adev, unsigned int set_flag);
//-void acx_log_irq(u16 irqtype);
//-void acx_timer(unsigned long address);
void acx_set_timer(acx_device_t *adev, int timeout_us);

/* Mac80211 Ops */
//-int acx_op_config(struct ieee80211_hw *hw, u32 changed);
/* void acx_op_bss_info_changed(struct ieee80211_hw *hw,
			struct ieee80211_vif *vif,
			struct ieee80211_bss_conf *info, u32 changed);

int acx_op_set_key(struct ieee80211_hw *hw, enum set_key_cmd cmd,
		struct ieee80211_vif *vif, struct ieee80211_sta *sta,
		struct ieee80211_key_conf *key);
*/
/* void acx_op_configure_filter(struct ieee80211_hw *hw,
			unsigned int changed_flags,
			unsigned int *total_flags, u64 multicast);
*/

/*
#if CONFIG_ACX_MAC80211_VERSION >= KERNEL_VERSION(3, 2, 0)
int acx_conf_tx(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
		u16 queue, const struct ieee80211_tx_queue_params *params);
#else
int acx_conf_tx(struct ieee80211_hw *hw, u16 queue,
		const struct ieee80211_tx_queue_params *params);
#endif
*/

/* int acx_op_get_stats(struct ieee80211_hw *hw, struct ieee80211_low_level_stats *stats);

#if CONFIG_ACX_MAC80211_VERSION < KERNEL_VERSION(2, 6, 34)
int acx_e_op_get_tx_stats(struct ieee80211_hw *hw,
			struct ieee80211_tx_queue_stats *stats);
#endif


int acx_op_set_tim(struct ieee80211_hw *hw, struct ieee80211_sta *sta,
		bool set);
*/
static int acx_do_job_update_tim(acx_device_t *adev);

/* Helpers */
//-void acx_mwait(int ms);
/* void great_inquisitor(acx_device_t * adev); */

/* Driver, Module */
static int __init acx_init_module(void);
static void __exit acx_cleanup_module(void);


/*
 * BOM Defines, static vars, etc.
 * ==================================================
 */

/* BOM Rate and channel definition
 * ---
 */

/* We define rates without short-preamble support fo now */

static struct ieee80211_rate acx100_rates[] = {
	{ .bitrate = 10, .hw_value = RATE100_1, },
	{ .bitrate = 20, .hw_value = RATE100_2, },
	{ .bitrate = 55, .hw_value = RATE100_5, },
	{ .bitrate = 110, .hw_value = RATE100_11, },
	{ .bitrate = 220, .hw_value = RATE100_22, },
};

struct ieee80211_rate acx111_rates[] = {
	{ .bitrate = 10, .hw_value = RATE111_1, },
	{ .bitrate = 20, .hw_value = RATE111_2, },
	{ .bitrate = 55, .hw_value = RATE111_5, },
	{ .bitrate = 60, .hw_value = RATE111_6, },
	{ .bitrate = 90, .hw_value = RATE111_9, },
	{ .bitrate = 110, .hw_value = RATE111_11, },
	{ .bitrate = 120, .hw_value = RATE111_12, },
	{ .bitrate = 180, .hw_value = RATE111_18, },
	{ .bitrate = 240, .hw_value = RATE111_24, },
	{ .bitrate = 360, .hw_value = RATE111_36, },
	{ .bitrate = 480, .hw_value = RATE111_48, },
	{ .bitrate = 540, .hw_value = RATE111_54, },
};
const int acx111_rates_sizeof=ARRAY_SIZE(acx111_rates);

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

static struct ieee80211_supported_band acx100_band_2GHz = {
	.channels	= channels,
	.n_channels	= ARRAY_SIZE(channels),
	.bitrates	= acx100_rates,
	.n_bitrates	= ARRAY_SIZE(acx100_rates),
};

static struct ieee80211_supported_band acx111_band_2GHz = {
	.channels	= channels,
	.n_channels	= ARRAY_SIZE(channels),
	.bitrates	= acx111_rates,
	.n_bitrates	= ARRAY_SIZE(acx111_rates),
};

const u8 bitpos2genframe_txrate[] = {
	[0] = 10,		/*  1 Mbit/s */
	[1] = 20,		/*  2 Mbit/s */
	[2] = 55,		/*  5*5 Mbit/s */
	[3] = 0x0B,		/*  6 Mbit/s */
	[4] = 0x0F,		/*  9 Mbit/s */
	[5] = 110,		/* 11 Mbit/s */
	[6] = 0x0A,		/* 12 Mbit/s */
	[7] = 0x0E,		/* 18 Mbit/s */
	[8] = 220,		/* 22 Mbit/s */
	[9] = 0x09,		/* 24 Mbit/s */
	[10] = 0x0D,		/* 36 Mbit/s */
	[11] = 0x08,		/* 48 Mbit/s */
	[12] = 0x0C,		/* 54 Mbit/s */
	[13] = 10,		/*  1 Mbit/s, should never happen */
	[14] = 10,		/*  1 Mbit/s, should never happen */
	[15] = 10,		/*  1 Mbit/s, should never happen */
};

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

const u8 acx_bitpos2rate100[] = {
	[0] = RATE100_1,
	[1] = RATE100_2,
	[2] = RATE100_5,
	[3] = RATE100_2,	/* should not happen */
	[4] = RATE100_2,	/* should not happen */
	[5] = RATE100_11,
	[6] = RATE100_2,	/* should not happen */
	[7] = RATE100_2,	/* should not happen */
	[8] = RATE100_22,
	[9] = RATE100_2,	/* should not happen */
	[10] = RATE100_2,	/* should not happen */
	[11] = RATE100_2,	/* should not happen */
	[12] = RATE100_2,	/* should not happen */
	[13] = RATE100_2,	/* should not happen */
	[14] = RATE100_2,	/* should not happen */
	[15] = RATE100_2,	/* should not happen */
};
BUILD_BUG_DECL(Rates, ARRAY_SIZE(acx_bitpos2rate100)
		   != ARRAY_SIZE(bitpos2genframe_txrate));


/*
 * BOM Locking
 * ==================================================
 */
#define DEBUG_TSC 0
#if DEBUG_TSC && defined(CONFIG_X86)
#define TIMESTAMP(d) unsigned long d; rdtscl(d)
#else
#define TIMESTAMP(d) unsigned long d = jiffies
#endif

#ifdef OW_20100613_OBSELETE_ACXLOCK_REMOVE
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

static inline const char *acx_sanitize_str(const char *s)
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
	where = acx_sanitize_str(where);
	while (--count) {
		if (!spin_is_locked(&adev->spinlock))
			break;
		cpu_relax();
	}
	if (!count) {
		pr_emerg("LOCKUP: already taken at %s!\n",
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
		where = acx_sanitize_str(where);
		pr_emerg("STRAY UNLOCK at %s!\n", where);
		BUG();
	}
#endif
	if (acx_debug & L_LOCK) {
		TIMESTAMP(diff);
		diff -= adev->lock_time;
		if (diff > max_lock_time) {
			where = acx_sanitize_str(where);
			pr_notice("max lock hold time %ld CPU ticks from %s "
			       "to %s\n", diff, adev->last_lock, where);
			max_lock_time = diff;
		}
	}
}
#endif /* PARANOID_LOCKING */
#endif

/*
 * BOM Logging
 * ==================================================
 */

/* ----- */
#if ACX_DEBUG > 1

static int acx_debug_func_indent;
#define ACX_DEBUG_FUNC_INDENT_INCREMENT 2
static const char acx_debug_spaces[] = "          " "          ";	/* Nx10 spaces */

void log_fn_enter(const char *funcname)
{
	int indent;
	TIMESTAMP(d);

	indent = acx_debug_func_indent;
	if (indent >= sizeof(acx_debug_spaces))
		indent = sizeof(acx_debug_spaces) - 1;

	pr_info("%08ld %s==> %s\n", d % 100000000,
		acx_debug_spaces + (sizeof(acx_debug_spaces) - 1) - indent,
		funcname);

	acx_debug_func_indent += ACX_DEBUG_FUNC_INDENT_INCREMENT;
}

void log_fn_exit(const char *funcname)
{
	int indent;
	TIMESTAMP(d);

	/* OW Handle underflow */
	if (acx_debug_func_indent >= ACX_DEBUG_FUNC_INDENT_INCREMENT)
		acx_debug_func_indent -= ACX_DEBUG_FUNC_INDENT_INCREMENT;
	else
		acx_debug_func_indent = 0;

	indent = acx_debug_func_indent;
	if (indent >= sizeof(acx_debug_spaces))
		indent = sizeof(acx_debug_spaces) - 1;

	pr_info(" %08ld %s<== %s\n", d % 100000000,
		acx_debug_spaces + (sizeof(acx_debug_spaces) - 1) - indent,
		funcname);
}

void log_fn_exitv(const char *funcname, int v)
{
	int indent;
	TIMESTAMP(d);

	acx_debug_func_indent -= ACX_DEBUG_FUNC_INDENT_INCREMENT;

	indent = acx_debug_func_indent;
	if (indent >= sizeof(acx_debug_spaces))
		indent = sizeof(acx_debug_spaces) - 1;

	pr_info("%08ld %s<== %s: %08X\n", d % 100000000,
		acx_debug_spaces + (sizeof(acx_debug_spaces) - 1) - indent,
		funcname, v);
}
#endif /* ACX_DEBUG > 1 */

/*
 * BOM Firmware, EEPROM, Phy
 * ==================================================
 */

void acx_get_firmware_version(acx_device_t * adev)
{
	fw_ver_t fw;
	u8 hexarr[4] = { 0, 0, 0, 0 };
	int hexidx = 0, val = 0;
	const char *num;
	char c;

	FN_ENTER;

	memset(fw.fw_id, 'E', FW_ID_SIZE);
	acx_interrogate(adev, &fw, ACX1xx_IE_FWREV);
	memcpy(adev->firmware_version, fw.fw_id, FW_ID_SIZE);
	adev->firmware_version[FW_ID_SIZE] = '\0';

	log(L_INIT, "Firmware: firmware_version='%s' hw_id=%08X\n",
	    adev->firmware_version, fw.hw_id);

	if (strncmp(fw.fw_id, "Rev ", 4) != 0) {
		pr_info("strange firmware version string "
			"'%s', please report\n", adev->firmware_version);
		adev->firmware_numver = 0x01090407;	/* assume 1.9.4.7 */
	} else {
		num = &fw.fw_id[4];
		while (1) {
			c = *num++;
			if ((c == '.') || (c == '\0')) {
				hexarr[hexidx++] = val;
				if ((hexidx > 3) || (c == '\0')) /* end? */
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
		log(L_DEBUG, "firmware_numver 0x%08X\n",
			adev->firmware_numver);
	}
	if (IS_ACX111(adev)) {
		if (adev->firmware_numver == 0x00010011) {
			/* This one does not survive floodpinging */
			pr_info("firmware '%s' is known to be buggy, "
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
		pr_info("unknown chip ID 0x%08X, "
			"please report\n", adev->firmware_id);
		break;
	}
	FN_EXIT0;
}

/*
 * acx_display_hardware_details
 *
 * Displays hw/fw version, radio type etc...
 */
void acx_display_hardware_details(acx_device_t *adev)
{
	const char *radio_str, *form_str;

	FN_ENTER;

	switch (adev->radio_type) {
	case RADIO_0D_MAXIM_MAX2820:
		/* DWL-650+ B1: MAXIM MAX2820 EGM 236 A7NOCH */
		/* USB DWL-120+ flip-antenna version:
		   MAXIM MAX2820 EGM 243 A7NO10
		   (large G logo) W22 B003A P01
		   (reference: W22-P01-B003A) */
		radio_str = "Maxim (MAX2820)";
		break;
	case RADIO_11_RFMD:
		radio_str = "RFMD";
		break;
	case RADIO_15_RALINK:
		radio_str = "Ralink";
		break;
	case RADIO_16_RADIA_RC2422:
		/* WL311v2 indicates that it's a Radia,
                   semi-recognizable label: RC2422(?) */
		radio_str = "Radia (RC2422?)";
		break;
	case RADIO_17_UNKNOWN:
		/* TI seems to have a radio which is
		 * additionally 802.11a capable, too */
		radio_str = "802.11a/b/g radio?! Please report";
		break;
	case RADIO_19_UNKNOWN:
		radio_str = "A radio used by Safecom cards?! Please report";
		break;
	case RADIO_1B_TI_TNETW3422:
		/* ex-Radia (consumed by TI), i.e. likely a RC2422 successor */
		radio_str = "TI (TNETW3422)";
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

	pr_info("chipset %s, radio type 0x%02X (%s), "
	       "form factor 0x%02X (%s), EEPROM version 0x%02X, "
	       "uploaded firmware '%s'\n",
	       adev->chip_name, adev->radio_type, radio_str,
	       adev->form_factor, form_str, adev->eeprom_version,
	       adev->firmware_version);

	FN_EXIT0;
}

/*
 * acx_s_read_fw
 *
 * Loads a firmware image
 * Returns:
 *  0:						unable to load file
 *  pointer to firmware:	success
 */
firmware_image_t *acx_read_fw(struct device *dev, const char *file,
				u32 * size)
{
	firmware_image_t *res;
	const struct firmware *fw_entry;

	res = NULL;
	log(L_INIT, "requesting firmware image '%s'\n", file);
	if (!request_firmware(&fw_entry, file, dev)) {
		*size = 8;
		if (fw_entry->size >= 8)
			*size = 8 + le32_to_cpu(*(u32 *) (fw_entry->data + 4));
		if (fw_entry->size != *size) {
			pr_info("firmware size does not match "
				"firmware header: %d != %d, "
				"aborting fw upload\n",
				(int)fw_entry->size, (int)*size);
			goto release_ret;
		}
		res = vmalloc(*size);
		if (!res) {
			pr_info("no memory for firmware "
			       "(%u bytes)\n", *size);
			goto release_ret;
		}
		memcpy(res, fw_entry->data, fw_entry->size);
	      release_ret:
		release_firmware(fw_entry);
		return res;
	}
	pr_info("firmware image '%s' was not provided. "
	       "Check your hotplug scripts\n", file);

	/* checksum will be verified in write_fw, so don't bother here */
	return res;
}

/*
 * Common function to parse ALL configoption struct formats
 * (ACX100 and ACX111; FIXME: how to make it work with ACX100 USB!?!?).
 *
 * FIXME: logging should be removed here and added to a /proc file instead
 */
void acx_parse_configoption(acx_device_t *adev,
			 const acx111_ie_configoption_t *pcfg)
{
	const u8 *pEle;
	struct eeprom_cfg *acfg = &adev->cfgopt;
	int i;
	int is_acx111 = IS_ACX111(adev);

	if (acx_debug & L_DEBUG) {
		pr_info("configoption struct content:\n");
		acx_dump_bytes(pcfg, sizeof(*pcfg));
	}

	if ((is_acx111 && (adev->eeprom_version == 5))
	    || (!is_acx111 && (adev->eeprom_version == 4))
	    || (!is_acx111 && (adev->eeprom_version == 5))) {
		/* these versions are known to be supported */
	} else {
		pr_info("unknown chip and EEPROM version combination (%s, v%d), "
		       "don't know how to parse config options yet. "
		       "Please report\n", is_acx111 ? "ACX111" : "ACX100",
		       adev->eeprom_version);
		return;
	}

	/* first custom-parse the first part which has chip-specific layout */

	pEle = (const u8 *)pcfg;

	pEle += 4;		/* skip (type,len) header */

	memcpy(acfg->NVSv, pEle, sizeof(acfg->NVSv));
	pEle += sizeof(acfg->NVSv);

	pr_info("NVSv: ");
	for (i = 0; i < sizeof(acfg->NVSv); i++) {
		printk("%02X ", acfg->NVSv[i]);
	}
	printk("\n");

	if (is_acx111) {
		acfg->NVS_vendor_offs = le16_to_cpu(*(u16 *) pEle);
		pEle += sizeof(acfg->NVS_vendor_offs);

		acfg->probe_delay = 200;	/* good default value? */
		pEle += 2;	/* FIXME: unknown, value 0x0001 */
	} else {
		memcpy(acfg->MAC, pEle, sizeof(acfg->MAC));
		pEle += sizeof(acfg->MAC);

		acfg->probe_delay = le16_to_cpu(*(u16 *) pEle);
		pEle += sizeof(acfg->probe_delay);
		if ((acfg->probe_delay < 100)
		    || (acfg->probe_delay > 500)) {
			pr_info("strange probe_delay value %d, "
			       "tweaking to 200\n", acfg->probe_delay);
			acfg->probe_delay = 200;
		}
	}

	acfg->eof_memory = le32_to_cpu(*(u32 *) pEle);
	pEle += sizeof(acfg->eof_memory);

	pr_info("NVS_vendor_offs:%04X probe_delay:%d eof_memory:%d\n",
	       acfg->NVS_vendor_offs,
	       acfg->probe_delay, acfg->eof_memory);

	acfg->dot11CCAModes = *pEle++;
	acfg->dot11Diversity = *pEle++;
	acfg->dot11ShortPreambleOption = *pEle++;
	acfg->dot11PBCCOption = *pEle++;
	acfg->dot11ChannelAgility = *pEle++;
	acfg->dot11PhyType = *pEle++;
	acfg->dot11TempType = *pEle++;
	pr_info("CCAModes:%02X Diversity:%02X ShortPreOpt:%02X "
	       "PBCC:%02X ChanAgil:%02X PHY:%02X Temp:%02X\n",
	       acfg->dot11CCAModes,
	       acfg->dot11Diversity,
	       acfg->dot11ShortPreambleOption,
	       acfg->dot11PBCCOption,
	       acfg->dot11ChannelAgility,
	       acfg->dot11PhyType, acfg->dot11TempType);

	/* then use common parsing for next part which has common layout */

	pEle++;			/* skip table_count (6) */

	if (IS_MEM(adev) && IS_ACX100(adev)) {
		/*
		 * For iPaq hx4700 Generic Slave F/W 1.10.7.K.  I'm
		 * not sure if these 4 extra bytes are before the
		 * dot11 things above or after, so I'm just going to
		 * guess after.  If someone sees these aren't
		 * reasonable numbers, please fix this.
		 * The area from which the dot11 values above are read
		 * contains: 04 01 01 01 00 05 01 06 00 02 01 02 the 8
		 * dot11 reads above take care of 8 of them, but which
		 * 8...
		 */
		pEle += 4;
	}

	acfg->antennas.type = pEle[0];
	acfg->antennas.len = pEle[1];
	pr_info("AntennaID:%02X Len:%02X Data:",
	       acfg->antennas.type, acfg->antennas.len);
	for (i = 0; i < pEle[1]; i++) {
		acfg->antennas.list[i] = pEle[i + 2];
		printk("%02X ", pEle[i + 2]);
	}
	printk("\n");

	pEle += pEle[1] + 2;
	acfg->power_levels.type = pEle[0];
	acfg->power_levels.len = pEle[1];
	pr_info("PowerLevelID:%02X Len:%02X Data:",
	       acfg->power_levels.type, acfg->power_levels.len);
	for (i = 0; i < pEle[1]; i++) {
		acfg->power_levels.list[i] =
		    le16_to_cpu(*(u16 *) & pEle[i * 2 + 2]);
		printk("%04X ", acfg->power_levels.list[i]);
	}
	printk("\n");

	pEle += pEle[1] * 2 + 2;
	acfg->data_rates.type = pEle[0];
	acfg->data_rates.len = pEle[1];
	pr_info("DataRatesID:%02X Len:%02X Data:",
	       acfg->data_rates.type, acfg->data_rates.len);
	for (i = 0; i < pEle[1]; i++) {
		acfg->data_rates.list[i] = pEle[i + 2];
		printk("%02X ", pEle[i + 2]);
	}
	printk("\n");

	pEle += pEle[1] + 2;
	acfg->domains.type = pEle[0];
	acfg->domains.len = pEle[1];

	if (IS_MEM(adev) && IS_ACX100(adev)) {
		/*
		 * For iPaq hx4700 Generic Slave F/W 1.10.7.K.
		 * There's an extra byte between this structure and
		 * the next that is not accounted for with this
		 * structure's length.  It's most likely a bug in the
		 * firmware, but we can fix it here by bumping the
		 * length of this field by 1.
		 */
		acfg->domains.len++;
	}

	pr_info("DomainID:%02X Len:%02X Data:",
	       acfg->domains.type, acfg->domains.len);
	for (i = 0; i < acfg->domains.len; i++) {
		acfg->domains.list[i] = pEle[i + 2];
		printk("%02X ", pEle[i + 2]);
	}
	printk("\n");

	pEle += acfg->domains.len + 2;
	acfg->product_id.type = pEle[0];
	acfg->product_id.len = pEle[1];
	for (i = 0; i < pEle[1]; i++)
		acfg->product_id.list[i] = pEle[i + 2];

	pr_info("ProductID:%02X Len:%02X Data:%.*s\n",
	       acfg->product_id.type, acfg->product_id.len,
	       acfg->product_id.len,
	       (char *)acfg->product_id.list);

	pEle += pEle[1] + 2;
	acfg->manufacturer.type = pEle[0];
	acfg->manufacturer.len = pEle[1];
	for (i = 0; i < pEle[1]; i++)
		acfg->manufacturer.list[i] = pEle[i + 2];

	pr_info("ManufacturerID:%02X Len:%02X Data:%.*s\n",
	       acfg->manufacturer.type, acfg->manufacturer.len,
	       acfg->manufacturer.len,
	       (char *)acfg->manufacturer.list);
	/*
	pr_info("EEPROM part:\n");
	for (i = 0; i < 58; i++) {
		printk("%02X =======>  0x%02X\n",
			i, (u8 *)acfg->NVSv[i-2]);
	}
	*/
}

int acx_read_phy_reg(acx_device_t *adev, u32 reg, u8 *charbuf)
{
	if (IS_PCI(adev) || IS_MEM(adev))
		return _acx_read_phy_reg(adev, reg, charbuf);
	if (IS_USB(adev))
		return acxusb_read_phy_reg(adev, reg, charbuf);

	log(L_ANY, "Unsupported dev_type=%i\n", (adev)->dev_type);

	return (NOT_OK);
}

int acx_write_phy_reg(acx_device_t *adev, u32 reg, u8 value)
{
	if (IS_PCI(adev) || IS_MEM(adev))
		return _acx_write_phy_reg(adev, reg, value);
	if (IS_USB(adev))
		return acxusb_write_phy_reg(adev, reg, value);

	log(L_ANY, "Unsupported dev_type=%i\n", (adev)->dev_type);

	return (NOT_OK);
}

/*
 * BOM CMDs (Control Path)
 * ==================================================
 */



void acx_start(acx_device_t *adev)
{
	FN_ENTER;

	log(L_INIT, "Updating initial settings\n");

	acx1xx_update_station_id(adev);

	acx1xx_update_rate_fallback(adev);
	acx1xx_update_tx_level(adev);
	acx1xx_update_antenna(adev);

	acx1xx_update_ed_threshold(adev);
	acx1xx_update_cca(adev);

	acx1xx_update_tx(adev);
	acx1xx_update_rx(adev);

	acx1xx_update_retry(adev);
	acx1xx_update_msdu_lifetime(adev);
	acx_update_reg_domain(adev);

	acx_update_mode(adev);

	/* For the acx100, we leave the firmware sensitivity and it
	   doesn't support auto recalib, so don't set it */
	if (IS_ACX111(adev)) {
		acx_update_sensitivity(adev);
		acx111_set_recalib_auto(adev, 1);
	}

	FN_EXIT0;
}


int acx_net_reset(struct ieee80211_hw *ieee)
{
	acx_device_t *adev = ieee2adev(ieee);
	FN_ENTER;
	if (IS_PCI(adev) || IS_MEM(adev))
		acx_reset_dev(adev);
	else
		TODO();

	FN_EXIT0;
	return 0;
}


int acx_setup_modes(acx_device_t *adev)
{
	int i;

	FN_ENTER;

	for (i=0; i<ARRAY_SIZE(channels); i++)
		channels[i].max_power = TX_CFG_MAX_DBM_POWER;

	if (IS_ACX100(adev)) {
		adev->ieee->wiphy->bands[IEEE80211_BAND_2GHZ] =
			&acx100_band_2GHz;
	} else {
		if (IS_ACX111(adev))
			adev->ieee->wiphy->bands[IEEE80211_BAND_2GHZ] =
				&acx111_band_2GHz;
		else {
			logf0(L_ANY, "Error: Unknown device");
			return -1;
		}
	}
	FN_EXIT0;
	return 0;
}



static int acx_recalib_radio(acx_device_t *adev)
{
	if (IS_ACX100(adev)) {
		logf0(L_INIT, "acx100: Doing radio re-calibration.\n");
		/* On ACX100, we need to recalibrate the radio
		 * by issuing a GETSET_TX|GETSET_RX */

		/* (OK == acx_s_issue_cmd(adev, ACX1xx_CMD_DISABLE_TX,
		   NULL, 0)) && (OK == acx_s_issue_cmd(adev,
		   ACX1xx_CMD_DISABLE_RX, NULL, 0)) && */
		if ((acx_issue_cmd(adev, ACX1xx_CMD_ENABLE_TX, &adev->channel,
						1) == OK)
			&& (acx_issue_cmd(adev, ACX1xx_CMD_ENABLE_RX,
						&adev->channel, 1) == OK))
			return OK;

		return NOT_OK;
	} else {
		logf0(L_INIT, "acx111: Enabling auto radio re-calibration.\n");
		return(acx111_set_recalib_auto(adev, 1));
	}

}

static void acx_after_interrupt_recalib(acx_device_t *adev)
{
	int res;

	/* this helps with ACX100 at least; hopefully ACX111 also does
	 * a recalibration here */

	/* clear flag beforehand, since we want to make sure it's
	 * cleared; then only set it again on specific
	 * circumstances */
	CLEAR_BIT(adev->after_interrupt_jobs, ACX_AFTER_IRQ_CMD_RADIO_RECALIB);

	/* better wait a bit between recalibrations to prevent
	 * overheating due to torturing the card into working too long
	 * despite high temperature (just a safety measure) */
	if (adev->recalib_time_last_success
	    && time_before(jiffies, adev->recalib_time_last_success
			   + RECALIB_PAUSE * 60 * HZ)) {
		if (adev->recalib_msg_ratelimit <= 4) {
			logf1(L_ANY, "%s: less than " STRING(RECALIB_PAUSE)
			       " minutes since last radio recalibration, "
			       "not recalibrating (maybe the card is too hot?)\n",
			       wiphy_name(adev->ieee->wiphy));
			adev->recalib_msg_ratelimit++;
			if (adev->recalib_msg_ratelimit == 5)
				logf0(L_ANY, "disabling the above message until next recalib\n");
		}
		return;
	}

	adev->recalib_msg_ratelimit = 0;

	/* note that commands sometimes fail (card busy), so only
	 * clear flag if we were fully successful */
	res = acx_recalib_radio(adev);
	if (res == OK) {
		pr_info("%s: successfully recalibrated radio\n",
		       wiphy_name(adev->ieee->wiphy));
		adev->recalib_time_last_success = jiffies;
		adev->recalib_failure_count = 0;
	} else {
		/* failed: resubmit, but only limited amount of times
		 * within some time range to prevent endless loop */

		adev->recalib_time_last_success = 0;	/* we failed */

		/* if some time passed between last attempts, then
		 * reset failure retry counter to be able to do next
		 * recalib attempt */
		if (time_after
		    (jiffies, adev->recalib_time_last_attempt + 5 * HZ))
			adev->recalib_failure_count = 0;

		if (adev->recalib_failure_count < 5) {
			/* increment inside only, for speedup of
			 * outside path */
			adev->recalib_failure_count++;
			adev->recalib_time_last_attempt = jiffies;
			acx_schedule_task(adev,
					  ACX_AFTER_IRQ_CMD_RADIO_RECALIB);
		}
	}
}

/*
 * BOM Other (Control Path)
 * ==================================================
 */
#if POWER_SAVE_80211
static void acx_s_update_80211_powersave_mode(acx_device_t * adev)
{
	/* merge both structs in a union to be able to have common code */
	union {
		acx111_ie_powersave_t acx111;
		acx100_ie_powersave_t acx100;
	} pm;

	/* change 802.11 power save mode settings */
	log(L_INIT, "updating 802.11 power save mode settings: "
	    "wakeup_cfg 0x%02X, listen interval %u, "
	    "options 0x%02X, hangover period %u, "
	    "enhanced_ps_transition_time %u\n",
	    adev->ps_wakeup_cfg, adev->ps_listen_interval,
	    adev->ps_options, adev->ps_hangover_period,
	    adev->ps_enhanced_transition_time);
	acx_interrogate(adev, &pm, ACX1xx_IE_POWER_MGMT);
	log(L_INIT, "Previous PS mode settings: wakeup_cfg 0x%02X, "
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
	acx_configure(adev, &pm, ACX1xx_IE_POWER_MGMT);
	acx_interrogate(adev, &pm, ACX1xx_IE_POWER_MGMT);
	log(L_INIT, "wakeup_cfg: 0x%02X\n", pm.acx111.wakeup_cfg);
	acx_mwait(40);
	acx_interrogate(adev, &pm, ACX1xx_IE_POWER_MGMT);
	log(L_INIT, "wakeup_cfg: 0x%02X\n", pm.acx111.wakeup_cfg);
	log(L_INIT, "power save mode change %s\n",
	    (pm.acx111.
	     wakeup_cfg & PS_CFG_PENDING) ? "FAILED" : "was successful");
	/* FIXME: maybe verify via PS_CFG_PENDING bit here
	 * that power save mode change was successful. */
	/* FIXME: we shouldn't trigger a scan immediately after
	 * fiddling with power save mode (since the firmware is sending
	 * a NULL frame then). */
}
#endif

/* TODO Verify these functions: translation rxbuffer.phy_plcp_signal to rate_idx */
#if 0

/** Rate values **/
#define ACX_CCK_RATE_1MB            0
#define ACX_CCK_RATE_2MB            1
#define ACX_CCK_RATE_5MB            2
#define ACX_CCK_RATE_11MB           3
#define ACX_OFDM_RATE_6MB           4
#define ACX_OFDM_RATE_9MB           5
#define ACX_OFDM_RATE_12MB          6
#define ACX_OFDM_RATE_18MB          7
#define ACX_OFDM_RATE_24MB          8
#define ACX_OFDM_RATE_36MB          9
#define ACX_OFDM_RATE_48MB          10
#define ACX_OFDM_RATE_54MB          11

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
#endif



/*
 * BOM Tx Path
 * ==================================================
 */

/* TODO: consider defining OP_TX_RET_TYPE, OP_TX_RET_VAL in
 * acx_compat, and hiding this #if/else.  OTOH, inclusion doesnt care
 * about old kernels
 */
OP_TX_RET_TYPE acx_op_tx(struct ieee80211_hw *hw, struct sk_buff *skb)
{
	acx_device_t *adev = ieee2adev(hw);

	skb_queue_tail(&adev->tx_queue, skb);

	ieee80211_queue_work(adev->ieee, &adev->tx_work);

	if (skb_queue_len(&adev->tx_queue) >= ACX_TX_QUEUE_MAX_LENGTH)
		acx_stop_queue(adev->ieee, NULL);

	return OP_TX_RET_OK;
}


/*
 * BOM Irq Handling, Timer
 * ==================================================
 */
void acx_init_task_scheduler(acx_device_t *adev)
{
	/* configure task scheduler */
#if defined(CONFIG_ACX_MAC80211_PCI)
	if (IS_PCI(adev)) {
		pr_info("device IS_PCI\n");
		INIT_WORK(&adev->irq_work, acx_irq_work);
		return;
	}
#endif
#if defined(CONFIG_ACX_MAC80211_USB)
	if (IS_USB(adev)) {
		pr_info("device IS_USB\n");
		INIT_WORK(&adev->irq_work, acxusb_irq_work);
		return;
	}
#endif
#if defined(CONFIG_ACX_MAC80211_MEM)
	if (IS_MEM(adev)) {
		pr_info("device IS_MEM\n");
		INIT_WORK(&adev->irq_work, acx_irq_work);
		return;
	}
#endif

	logf0(L_ANY, "Unhandled adev device type!\n");
	BUG();

	/* OW TODO Interrupt handling ... */
	/* OW In case of of tasklet ... but workqueues seem to be prefered
	 tasklet_init(&adev->interrupt_tasklet,
	 (void(*)(unsigned long)) acx_interrupt_tasklet,
	 (unsigned long) adev);
	 */

}

void acx_after_interrupt_task(acx_device_t *adev)
{
	FN_ENTER;

	if (!adev->after_interrupt_jobs || !adev->initialized)
		goto end_no_lock;	/* no jobs to do */

	/* we see lotsa tx errors */
	if (adev->after_interrupt_jobs & ACX_AFTER_IRQ_CMD_RADIO_RECALIB) {
		logf0(L_DEBUG, "Schedule CMD_RADIO_RECALIB\n");
		acx_after_interrupt_recalib(adev);
	}

	/* 1) we detected that no Scan_Complete IRQ came from fw, or
	 * 2) we found too many STAs */
	if (adev->after_interrupt_jobs & ACX_AFTER_IRQ_CMD_STOP_SCAN) {
		log(L_IRQ, "sending a stop scan cmd...\n");

		/* OW Scanning is done by mac80211 */
#if 0
		acx_unlock(adev, flags);
		acx_issue_cmd(adev, ACX1xx_CMD_STOP_SCAN, NULL, 0);
		acx_lock(adev, flags);
		/* HACK: set the IRQ bit, since we won't get a scan
		 * complete IRQ any more on ACX111 (works on ACX100!),
		 * since _we_, not a fw, have stopped the scan */
		SET_BIT(adev->irq_status, HOST_INT_SCAN_COMPLETE);
#endif
		CLEAR_BIT(adev->after_interrupt_jobs,
			  ACX_AFTER_IRQ_CMD_STOP_SCAN);
	}

	/* either fw sent Scan_Complete or we detected that no
	 * Scan_Complete IRQ came from fw. Finish scanning, pick join
	 * partner if any */
	if (adev->after_interrupt_jobs & ACX_AFTER_IRQ_COMPLETE_SCAN) {
		/* + scan kills current join status - restore it
		 *   (do we need it for STA?) */
		/* + does it happen only with active scans?
		 *   active and passive scans? ALL scans including
		 *   background one? */
		/* + was not verified that everything is restored
		 *   (but at least we start to emit beacons again) */
		CLEAR_BIT(adev->after_interrupt_jobs,
			  ACX_AFTER_IRQ_COMPLETE_SCAN);
	}

	if (adev->after_interrupt_jobs & ACX_AFTER_IRQ_RESTART_SCAN) {
		log(L_IRQ, "sending a start_scan cmd...\n");
		CLEAR_BIT(adev->after_interrupt_jobs,
			  ACX_AFTER_IRQ_RESTART_SCAN);
	}

	if (adev->after_interrupt_jobs & ACX_AFTER_IRQ_UPDATE_TIM) {
		log(L_IRQ, "ACX_AFTER_IRQ_UPDATE_TIM\n");
		acx_do_job_update_tim(adev);
		CLEAR_BIT(adev->after_interrupt_jobs,
			ACX_AFTER_IRQ_UPDATE_TIM);
	}

	/* others */
	if(adev->after_interrupt_jobs)
	{
		pr_info("Jobs still to be run: 0x%02X\n",
			adev->after_interrupt_jobs);
		adev->after_interrupt_jobs = 0;
	}
end_no_lock:
	FN_EXIT0;
}

void acx_log_irq(u16 irqtype)
{
	pr_info("%s: got: ", __func__);

	if (irqtype & HOST_INT_RX_DATA)
		pr_info("Rx_Data,");

	if (irqtype & HOST_INT_TX_COMPLETE)
		pr_info("Tx_Complete,");

	if (irqtype & HOST_INT_TX_XFER)
		pr_info("Tx_Xfer,");

	if (irqtype & HOST_INT_RX_COMPLETE)
		pr_info("Rx_Complete,");

	if (irqtype & HOST_INT_DTIM)
		pr_info("DTIM,");

	if (irqtype & HOST_INT_BEACON)
		pr_info("Beacon,");

	if (irqtype & HOST_INT_TIMER)
		log(L_IRQ, "Timer,");

	if (irqtype & HOST_INT_KEY_NOT_FOUND)
		pr_info("Key_Not_Found,");

	if (irqtype & HOST_INT_IV_ICV_FAILURE)
		pr_info("IV_ICV_Failure (crypto),");

	if (irqtype & HOST_INT_CMD_COMPLETE)
		pr_info("Cmd_Complete,");

	if (irqtype & HOST_INT_INFO)
		pr_info("Info,");

	if (irqtype & HOST_INT_OVERFLOW)
		pr_info("Overflow,");

	if (irqtype & HOST_INT_PROCESS_ERROR)
		pr_info("Process_Error,");

	if (irqtype & HOST_INT_SCAN_COMPLETE)
		pr_info("Scan_Complete,");

	if (irqtype & HOST_INT_FCS_THRESHOLD)
		pr_info("FCS_Threshold,");

	if (irqtype & HOST_INT_UNKNOWN)
		pr_info("Unknown,");

	pr_info(": IRQ(s)\n");
}

/*
 * acx_schedule_task
 *
 * Schedule the call of the after_interrupt method after leaving
 * the interrupt context.
 */
void acx_schedule_task(acx_device_t *adev, unsigned int set_flag)
{
	SET_BIT(adev->after_interrupt_jobs, set_flag);
	ieee80211_queue_work(adev->ieee, &adev->irq_work);
}

/*
* acx_i_timer
*/
void acx_timer(unsigned long address)
{
	/* acx_device_t *adev = (acx_device_t *) address; */

	FN_ENTER;

	FIXME();
	/* We need calibration and stats gather tasks to perform here */

	FN_EXIT0;
}

/*
 * acx_set_timer
 *
 * Sets the 802.11 state management timer's timeout.
 *
 */
void acx_set_timer(acx_device_t *adev, int timeout_us)
{
	FN_ENTER;

	log(L_DEBUG | L_IRQ, "(%u ms)\n", timeout_us / 1000);
	if (!(adev->dev_state_mask & ACX_STATE_IFACE_UP)) {
		pr_info("attempt to set the timer "
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

/*
 * BOM Mac80211 Ops
 * ==================================================
 */

int acx_op_add_interface(struct ieee80211_hw *ieee, struct ieee80211_VIF *vif)
{
	acx_device_t *adev = ieee2adev(ieee);
	int err = -EOPNOTSUPP;

	u8 *mac_vif;
	char mac[MACSTR_SIZE];

	int vif_type;

	FN_ENTER;
	acx_sem_lock(adev);

	vif_type = vif->type;
	adev->vif_type = vif_type;
	log(L_ANY, "vif_type=%04X\n", vif_type);

	if (vif_type == NL80211_IFTYPE_MONITOR)
		adev->vif_monitor++;
	else if (adev->vif_operating)
		goto out_unlock;

	adev->vif_operating = 1;
	adev->vif = VIF_vif(vif);
	mac_vif = VIF_addr(vif);

	switch (adev->vif_type) {
	case NL80211_IFTYPE_AP:
		log(L_ANY, "NL80211_IFTYPE_AP\n");
		adev->mode = ACX_MODE_3_AP;
		break;

	case NL80211_IFTYPE_ADHOC:
		log(L_ANY, "NL80211_IFTYPE_ADHOC\n");
		adev->mode = ACX_MODE_0_ADHOC;
		break;

	case NL80211_IFTYPE_STATION:
		log(L_ANY, "NL80211_IFTYPE_STATION\n");
		adev->mode = ACX_MODE_2_STA;
		break;

	case NL80211_IFTYPE_MONITOR:
		logf0(L_ANY, "NL80211_IFTYPE_MONITOR\n");
		break;

	case NL80211_IFTYPE_WDS:
		logf0(L_ANY, "NL80211_IFTYPE_WDS: Not implemented\n");
		goto out_unlock;

	default:
		logf1(L_ANY, "Unknown adev->vif_type=%d\n", adev->vif_type);

		goto out_unlock;
		break;
	}

	/* Reconfigure mac-address globally, affecting all vifs */
	if (!mac_is_equal(mac_vif, adev->dev_addr)) {
		memcpy(adev->dev_addr, mac_vif, ETH_ALEN);
		memcpy(adev->bssid, mac_vif, ETH_ALEN);
		acx1xx_set_station_id(adev, mac_vif);
		SET_IEEE80211_PERM_ADDR(adev->ieee, adev->dev_addr);
	}

	acx_update_mode(adev);

	logf0(L_ANY, "Redoing cmd_join_bssid() after add_interface\n");
	acx_cmd_join_bssid(adev, adev->bssid);

	acx_debugfs_add_adev(adev);

	pr_info("Virtual interface added (type: 0x%08X, MAC: %s)\n",
		adev->vif_type,	acx_print_mac(mac, mac_vif));

	err = 0;

out_unlock:
	acx_sem_unlock(adev);
	FN_EXIT0;
	return err;
}

void acx_op_remove_interface(struct ieee80211_hw *hw, struct ieee80211_VIF *vif)
{
	acx_device_t *adev = ieee2adev(hw);

	char mac[MACSTR_SIZE];
	u8 *mac_vif;

	FN_ENTER;
	acx_sem_lock(adev);
	acx_debugfs_remove_adev(adev);

	mac_vif = VIF_addr(vif);

	if (vif->type == NL80211_IFTYPE_MONITOR)
		adev->vif_monitor--;
	else {
		adev->vif_operating = 0;
		adev->vif = NULL;
	}

	acx_set_mode(adev, ACX_MODE_OFF);

	log(L_DEBUG, "vif_operating=%d, vif->type=%d\n",
		adev->vif_operating, vif->type);

	log(L_ANY, "Virtual interface removed: type=%d, MAC=%s\n",
		vif->type, acx_print_mac(mac, mac_vif));

	acx_sem_unlock(adev);
	FN_EXIT0;
}

/* FUNCTION_GREP_RESET
 * The function_grep script can get confused with multiple "{"" opening braces
 * due e.g. due to #ifdefs. This tag reset the parser state of the script.
 */

int acx_op_config(struct ieee80211_hw *hw, u32 changed)
{
	acx_device_t *adev = ieee2adev(hw);
	struct ieee80211_conf *conf = &hw->conf;

	u32 changed_not_done = changed;

	FN_ENTER;
	acx_sem_lock(adev);

	if (!adev->initialized)
		goto end_sem_unlock;

	logf1(L_DEBUG, "changed=%08X\n", changed);

	/* Tx-Power
	 * power_level: requested transmit power (in dBm)
	 */
	if (changed & IEEE80211_CONF_CHANGE_POWER) {
		logf1(L_DEBUG, "IEEE80211_CONF_CHANGE_POWER: %d\n",
			conf->power_level);
		acx1xx_set_tx_level_dbm(adev, conf->power_level);
	}

	/* IEEE80211_CONF_CHANGE_CHANNEL */
	if (changed & IEEE80211_CONF_CHANGE_CHANNEL) {
		changed_not_done &= ~IEEE80211_CONF_CHANGE_CHANNEL;

		logf1(L_DEBUG, "IEEE80211_CONF_CHANGE_CHANNEL, "
			"channel->hw_value=%i\n", conf->channel->hw_value);

		if (conf->channel->hw_value == adev->channel)
			goto change_channel_done;

		acx_selectchannel(adev, conf->channel->hw_value,
				conf->channel->center_freq);
	}
change_channel_done:
	if (changed_not_done)
		logf1(L_DEBUG, "changed_not_done=%08X\n", changed_not_done);

end_sem_unlock:
	acx_sem_unlock(adev);
	FN_EXIT0;
	return 0;
}

void acx_op_bss_info_changed(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
			struct ieee80211_bss_conf *info, u32 changed)
{
	acx_device_t *adev = ieee2adev(hw);

	int err = -ENODEV;

	struct sk_buff *beacon;

	FN_ENTER;
	acx_sem_lock(adev);

	logf1(L_DEBUG, "changed=%04X\n", changed);

	if (!adev->vif_operating)
		goto end_sem_unlock;

	if (changed & BSS_CHANGED_BSSID) {
		MAC_COPY(adev->bssid, info->bssid);

		logf0(L_ANY, "Redoing join following bssid update\n");
		acx_cmd_join_bssid(adev, adev->bssid);
	}

	/* BOM BSS_CHANGED_BEACON */
	if (changed & BSS_CHANGED_BEACON) {

		/* TODO Use ieee80211_beacon_get_tim instead */
		beacon = ieee80211_beacon_get(hw, vif);
		if (!beacon) {
			logf0(L_ANY,
				"Error: BSS_CHANGED_BEACON: skb_tmp==NULL");
			return;
		}

		adev->beacon_interval = info->beacon_int;
		acx_set_beacon(adev, beacon);

		dev_kfree_skb(beacon);
	}
	err = 0;

end_sem_unlock:
	acx_sem_unlock(adev);
	FN_EXIT1(err);
	return;
}

int acx111_set_default_key(acx_device_t *adev, u8 key_id)
{
	int res;
	ie_dot11WEPDefaultKeyID_t dkey;

#if defined DEBUG_WEP
	struct {
		u16 type;
		u16 len;
		u8 val;
	}ACX_PACKED keyindic;
#endif

	dkey.KeyID = key_id;
	log(L_INIT, "Setting key %u as default\n", dkey.KeyID);
	res = acx_configure(adev, &dkey,
		ACX1xx_IE_DOT11_WEP_DEFAULT_KEY_SET);

#if defined DEBUG_WEP
	keyindic.val = key_id;
	acx_configure(adev, &keyindic, ACX111_IE_KEY_CHOOSE);
#endif

	adev->default_key = key_id;

	return res;
}

static int acx111_set_key_type(acx_device_t *adev, acx111WEPDefaultKey_t *key,
                               struct ieee80211_key_conf *mac80211_key,
                               const u8 *addr)
{
	switch (mac80211_key->cipher) {
#if 0
	case WLAN_CIPHER_SUITE_WEP40:
	case WLAN_CIPHER_SUITE_WEP104:
		if (is_broadcast_ether_addr(addr))
			key->type = KEY_WEP_DEFAULT;
		else
			key->type = KEY_WEP_ADDR;

		mac80211_key->hw_key_idx = mac80211_key->keyidx;
		break;
	case WLAN_CIPHER_SUITE_TKIP:
		if (is_broadcast_ether_addr(addr))
			key->type = KEY_TKIP_MIC_GROUP;
		else
			key->type = KEY_TKIP_MIC_PAIRWISE;

		mac80211_key->hw_key_idx = mac80211_key->keyidx;
		break;
#endif
	case WLAN_CIPHER_SUITE_CCMP:
		if (is_broadcast_ether_addr(addr))
			key->type = KEY_AES_GROUP;
		else
			key->type = KEY_AES_PAIRWISE;

		break;
	default:
		log(L_INIT, "Unknown key cipher 0x%x", mac80211_key->cipher);
		return -EOPNOTSUPP;
	}

	return 0;
}


static int acx111_set_key(acx_device_t *adev, enum set_key_cmd cmd,
                          const u8 *addr, struct ieee80211_key_conf *key)
{
	int ret = -1;
	acx111WEPDefaultKey_t dk;

	memset(&dk, 0, sizeof(dk));

	switch (cmd) {
	case SET_KEY:
		dk.action = cpu_to_le16(KEY_ADD_OR_REPLACE);
		break;
	case DISABLE_KEY:
		dk.action = cpu_to_le16(KEY_REMOVE);
		break;
	default:
		log(L_INIT, "Unsupported key cmd 0x%x", cmd);
		break;
	}

	ret = acx111_set_key_type(adev, &dk, key, addr);
	if (ret < 0) {
		log(L_INIT, "Set KEY type failed");
		return ret;
	}

	memcpy(dk.MacAddr, addr, ETH_ALEN);

	dk.keySize = key->keylen;
	dk.defaultKeyNum = key->keyidx; /* ignored when setting default key */
	dk.index = 0;

	memcpy(dk.key, key->key, dk.keySize);

	ret = acx_issue_cmd(adev, ACX1xx_CMD_WEP_MGMT, &dk, sizeof(dk));

	return ret;
}

int acx_op_set_key(struct ieee80211_hw *hw, enum set_key_cmd cmd,
                   struct ieee80211_vif *vif, struct ieee80211_sta *sta,
                   struct ieee80211_key_conf *key)
{
	struct acx_device *adev = ieee2adev(hw);
	u8 algorithm;
	int ret=0;

	const u8 *addr;
	static const u8 bcast_addr[ETH_ALEN] =
		{ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

	acx_sem_lock(adev);

	addr = sta ? sta->addr : bcast_addr;

	log(L_DEBUG, "cmd=%d\n", cmd);
	log(L_DEBUG, "addr=" MACSTR, MAC(addr));
	log(L_DEBUG, "key->: cipher=%08x, icv_len=%d, iv_len=%d, hw_key_idx=%d, "
			"flags=%02x, keyidx=%d, keylen=%d\n", key->cipher, key->icv_len,
	        key->iv_len, key->hw_key_idx, key->flags, key->keyidx,
	        key->keylen);
	if (acx_debug & L_DEBUG)
		hexdump("key->: key", key->key, key->keylen);

#if CONFIG_ACX_MAC80211_VERSION < KERNEL_VERSION(2, 6, 37)
        switch (key->alg) {
#else
	switch (key->cipher) {
#endif

#if CONFIG_ACX_MAC80211_VERSION < KERNEL_VERSION(2, 6, 37)
        case ALG_WEP:
                if (key->keylen == 5) {
                    algorithm = ACX_SEC_ALGO_WEP;
                    log(L_INIT, "algorithm=%i: %s\n",
			    algorithm, "ACX_SEC_ALGO_WEP");
                } else {
                    algorithm = ACX_SEC_ALGO_WEP104;
                    log(L_INIT, "algorithm=%i: %s\n", "ACX_SEC_ALGO_WEP104");
                }

		acx_set_hw_encryption_off(adev);
                ret = -EOPNOTSUPP;
                break;
#else
	case WLAN_CIPHER_SUITE_WEP40:
	        algorithm = ACX_SEC_ALGO_WEP;
                log(L_INIT, "algorithm=%i: %s\n", algorithm, "ACX_SEC_ALGO_WEP");

                acx_set_hw_encryption_off(adev);
                ret = -EOPNOTSUPP;
                break;

        case WLAN_CIPHER_SUITE_WEP104:
                algorithm = ACX_SEC_ALGO_WEP104;
                log(L_INIT, "algorithm=%i: %s\n",
			algorithm, "ACX_SEC_ALGO_WEP104");

                acx_set_hw_encryption_off(adev);
                ret = -EOPNOTSUPP;
                break;
#endif

#if CONFIG_ACX_MAC80211_VERSION < KERNEL_VERSION(2, 6, 37)
        case ALG_TKIP:
#else
	case WLAN_CIPHER_SUITE_TKIP:
#endif
	        algorithm = ACX_SEC_ALGO_TKIP;
	        log(L_INIT, "algorithm=%i: %s\n", algorithm, "ACX_SEC_ALGO_TKIP");

	        acx_set_hw_encryption_off(adev);
	        ret = -EOPNOTSUPP;
	        break;

#if CONFIG_ACX_MAC80211_VERSION < KERNEL_VERSION(2, 6, 37)
        case ALG_CCMP:
#else
	case WLAN_CIPHER_SUITE_CCMP:
#endif
		algorithm = ACX_SEC_ALGO_AES;
		log(L_INIT, "algorithm=%i: %s\n", algorithm, "ACX_SEC_ALGO_AES");

		acx_set_hw_encryption_on(adev);
		acx111_set_key(adev, cmd, addr, key);
		ret = 0;
		break;

	default:
		algorithm = ACX_SEC_ALGO_NONE;

		acx_set_hw_encryption_off(adev);
		ret = 0;
		break;
	}

	acx_sem_unlock(adev);
	return ret;
}

void acx_op_configure_filter(struct ieee80211_hw *hw,
			unsigned int changed_flags,
			unsigned int *total_flags, u64 multicast)
{
	acx_device_t *adev = ieee2adev(hw);

	FN_ENTER;

	acx_sem_lock(adev);

	logf1(L_DEBUG, "1: changed_flags=0x%08x, *total_flags=0x%08x\n",
		changed_flags, *total_flags);

	/* OWI TODO: Set also FIF_PROBE_REQ ? */
	*total_flags &= (FIF_PROMISC_IN_BSS | FIF_ALLMULTI | FIF_FCSFAIL
			| FIF_CONTROL | FIF_OTHER_BSS);

	logf1(L_DEBUG, "2: *total_flags=0x%08x\n", *total_flags);

	acx_sem_unlock(adev);
	FN_EXIT0;
}

#if CONFIG_ACX_MAC80211_VERSION >= KERNEL_VERSION(3, 2, 0)
int acx_conf_tx(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
		u16 queue, const struct ieee80211_tx_queue_params *params)
#else
int acx_conf_tx(struct ieee80211_hw *hw, u16 queue,
		const struct ieee80211_tx_queue_params *params)
#endif
{
	acx_device_t *adev = ieee2adev(hw);
	FN_ENTER;
	acx_sem_lock(adev);
    /* TODO */
	acx_sem_unlock(adev);
	FN_EXIT0;
	return 0;
}

int acx_op_get_stats(struct ieee80211_hw *hw,
		struct ieee80211_low_level_stats *stats)
{
	acx_device_t *adev = ieee2adev(hw);

	FN_ENTER;
	acx_sem_lock(adev);

	memcpy(stats, &adev->ieee_stats, sizeof(*stats));

	acx_sem_unlock(adev);
	FN_EXIT0;
	return 0;
}

int acx_op_set_tim(struct ieee80211_hw *hw, struct ieee80211_sta *sta, bool set)
{
	acx_device_t *adev = ieee2adev(hw);

	acx_schedule_task(adev, ACX_AFTER_IRQ_UPDATE_TIM);

	return 0;
}

static int acx_do_job_update_tim(acx_device_t *adev)
{
	int ret;
	struct sk_buff *beacon;
	u16 tim_offset;
	u16 tim_length;

#if CONFIG_ACX_MAC80211_VERSION > KERNEL_VERSION(2, 6, 32)
	beacon = ieee80211_beacon_get_tim(adev->ieee, adev->vif, &tim_offset,
			&tim_length);
#else
	beacon = ieee80211_beacon_get(adev->ieee, adev->vif);
	if (!beacon)
		goto out;

	tim_offset = acx_beacon_find_tim(beacon) - beacon->data;
out:
#endif
	if (!beacon) {
		logf0(L_ANY, "Error: beacon==NULL");
		return NOT_OK;
	}

	if (IS_ACX111(adev)) {
		ret = acx_set_tim_template(adev, beacon->data + tim_offset,
				beacon->len - tim_offset);
	}

	dev_kfree_skb(beacon);

	return (ret);
}

#if CONFIG_ACX_MAC80211_VERSION < KERNEL_VERSION(2, 6, 34)
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
#endif

/*
 * BOM Helpers
 * ==================================================
 */

/*
 * Basically a mdelay/msleep with logging
 */
void acx_mwait(int ms)
{
	FN_ENTER;
	msleep(ms);
	FN_EXIT0;
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
		acx_issue_cmd(adev, ACX1xx_CMD_INTERROGATE, &ie, sizeof(ie));
	}
	FN_EXIT0;
}
#endif


/*
 * BOM Driver, Module
 * ==================================================
 */

static int __init acx_init_module(void)
{
	int r1, r2, r3;

	acx_struct_size_check();

	/* ACX_GIT_VERSION can be an empty string, if something went
	   wrong before on Makefile/shell level. We trap this here
	   ... since trapping empty macro strings in cpp seems not
	   possible (didn't find how ) !? */
	pr_info("acx-mac80211, version: %s (git: %s)\n",
		ACX_RELEASE,
		strlen(ACX_GIT_VERSION) ? ACX_GIT_VERSION : "unknown");

	pr_info("this driver is still EXPERIMENTAL\n"
	       "acx: please read the README file and/or "
	       "go to http://acx100.sourceforge.net/wiki for "
	       "further information\n");

	r1 = r2 = r3 = -EINVAL;

	r1 = acxpci_init_module();
	r2 = acxusb_init_module();
	r3 = acxmem_init_module();

	if (r3 && r2 && r1) {		/* all three failed! */
		pr_info("r1_pci=%i, r2_usb=%i, r3_mem=%i\n", r1, r2, r3);
		return -EINVAL;
	}

	acx_proc_init();
	acx_debugfs_init();

	/* return success if at least one succeeded */
	return 0;
}

static void __exit acx_cleanup_module(void)
{
	/* TODO Check, that interface isn't still up */
	acx_debugfs_exit();
	acx_proc_exit();

	acxpci_cleanup_module();
	acxusb_cleanup_module();
	acxmem_cleanup_module();
}

/*
 * BOM Module
 * ==================================================
 */

module_init(acx_init_module);
module_exit(acx_cleanup_module);

#if ACX_DEBUG

/* will add __read_mostly later */
unsigned int acx_debug = ACX_DEFAULT_MSG;
/* parameter is 'debug', corresponding var is acx_debug */
module_param_named(debug, acx_debug, uint, 0644);
MODULE_PARM_DESC(debug, "Debug level mask (see L_xxx constants)");

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36)
/* implement /sys/module/acx_mac80211/parameters/debugflags */

static const char *flag_names[] = {
	"L_LOCK", "L_INIT", "L_IRQ", "L_ASSOC", "L_FUNC", "L_XFER",
	"L_DATA", "L_DEBUG", "L_IOCTL", "L_CTL", "L_BUFR", "L_XFER_BEACON",
	"L_BUFT", "L_USBRXTX", "L_BUF",
};
/* should check enum debug_flags, but no suitable val is set */
BUILD_BUG_DECL(flag_names, ARRAY_SIZE(flag_names) != 15);

static int acx_debug_flag_get(char *buf, const struct kernel_param *kp)
{
	int i, len;
	char *p = buf; // 1 page preallocated (I think - it didnt crash !!)

	for (i = 0; i < ARRAY_SIZE(flag_names); i++) {
		if (acx_debug & 1 << i)
			len = sprintf(p, "bit %d:%s = 1\n", i, flag_names[i]);
		else
			len = sprintf(p, "bit %d:%s = 0\n", i, flag_names[i]);
		pr_info("%s", p);
		p += len;
	}
	return p - buf;
}

static int acx_debug_flag_set(const char *val, const struct kernel_param *kp)
{
	int i;
	char sign, *e, *p = (char*) val; /* cast away const */

	if ((e = strchr(p, '\n')))
		*e = '\0';	/* trim trailing newline */

	pr_info("parsing flags: %s\n", p);
	for (; p; p = e) {
		sign = *p++;
		switch (sign) {
		case '+':
		case '-':
			break;
		default:
			return -EINVAL;
		}
		if ((e = strchr(p, ',')))
			*e++ = '\0';	/* end 1st term, ready next */

		for (i = 0; i < ARRAY_SIZE(flag_names); i++) {

			if (!strcmp(p, flag_names[i])) {
				if (sign == '+')
					acx_debug |= (1 << i);
				else
					acx_debug &= ~(1 << i);
				break;
			}
		}
		if (i == ARRAY_SIZE(flag_names)) {
			pr_err("no match on val: %s\n", p);
			return -EINVAL;
		}
	}
	return 0;
}

static struct kernel_param_ops acx_debug_flag_ops = {
        .get = acx_debug_flag_get,
        .set = acx_debug_flag_set,
};
module_param_cb(debugflags, &acx_debug_flag_ops, "str", 0644);
MODULE_PARM_DESC(debugflags, "read/set flag names: +L_CTL,-L_BUFT etc");

#endif	/* implement /sys/module/acx_mac80211/parameters/debugflags */
#endif	/* ACX_DEBUG */

#ifdef MODULE_LICENSE
MODULE_LICENSE("Dual MPL/GPL");
#endif
/* USB had this: MODULE_AUTHOR("Martin Wawro <martin.wawro AT uni-dortmund.de>"); */
MODULE_AUTHOR("ACX100 Open Source Driver development team");
MODULE_DESCRIPTION
    ("Driver for TI ACX1xx based wireless cards (CardBus/PCI/USB)");

MODULE_VERSION(ACX_RELEASE);
