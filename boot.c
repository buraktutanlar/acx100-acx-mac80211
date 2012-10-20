/*
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2012
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

#include <linux/vmalloc.h>
#include <linux/firmware.h>

#include "acx.h"
#include "usb.h"
#include "merge.h"
#include "cmd.h"
#include "ie.h"
#include "utils.h"
#include "debug.h"

void acx_get_firmware_version(acx_device_t * adev)
{
	fw_ver_t fw;
	u8 hexarr[4] = { 0, 0, 0, 0 };
	int hexidx = 0, val = 0;
	const char *num;
	char c;



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

}

/*
 * acx_display_hardware_details
 *
 * Displays hw/fw version, radio type etc...
 */
void acx_display_hardware_details(acx_device_t *adev)
{
	const char *radio_str, *form_str;



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

