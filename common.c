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

/* Helpers */
//-void acx_mwait(int ms);
/* void great_inquisitor(acx_device_t * adev); */

/* Driver, Module */
static int __init acx_init_module(void);
static void __exit acx_cleanup_module(void);



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
