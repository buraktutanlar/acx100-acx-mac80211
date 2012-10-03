#include "acx_debug.h"

#include "acx.h"
#include "merge.h"
#include "cmd.h"
#include "ie.h"
#include "utils.h"
#include "tx.h"
#include "boot.h"
#include "cardsetting.h"

/* Please keep acx_reg_domain_ids_len in sync... */
const u8 acx_reg_domain_ids[acx_reg_domain_ids_len] =
    { 0x10, 0x20, 0x30, 0x31, 0x32, 0x40, 0x41, 0x51 };
static const u16 reg_domain_channel_masks[acx_reg_domain_ids_len] =
    { 0x07ff, 0x07ff, 0x1fff, 0x0600, 0x1e00, 0x2000, 0x3fff, 0x01fc };

/* in crda ?? */
const char *const
acx_reg_domain_strings[] = {
	[0] = " 1-11 FCC (USA)",
	[1] = " 1-11 DOC/IC (Canada)",
	/* BTW: WLAN use in ETSI is regulated by ETSI standard EN 300 328-2 V1.1.2 */
	[2] = " 1-13 ETSI (Europe)",
	[3] = "10-11 Spain",
	[4] = "10-13 France",
	[5] = "   14 MKK (Japan)",
	[6] = " 1-14 MKK1",
	[7] = "  3-9 Israel (not all firmware versions)",
	[8] = NULL,	/* needs to remain as last entry */
};


int acx111_get_feature_config(acx_device_t *adev,
			    u32 *feature_options, u32 *data_flow_options)
{
	struct acx111_ie_feature_config feat;



	if (!IS_ACX111(adev))
		return NOT_OK;

	memset(&feat, 0, sizeof(feat));

	if (OK != acx_interrogate(adev, &feat, ACX1xx_IE_FEATURE_CONFIG)) {

		return NOT_OK;
	}
	log(L_DEBUG,
	    "acx: got Feature option:0x%X, DataFlow option: 0x%X\n",
	    feat.feature_options, feat.data_flow_options);

	if (feature_options)
		*feature_options = le32_to_cpu(feat.feature_options);
	if (data_flow_options)
		*data_flow_options = le32_to_cpu(feat.data_flow_options);


	return OK;
}

int acx111_set_feature_config(acx_device_t *adev,
			    u32 feature_options, u32 data_flow_options,
			    unsigned int mode
			    /* 0 == remove, 1 == add, 2 == set */ )
{
	struct acx111_ie_feature_config feat;
	int i;



	if (!IS_ACX111(adev)) {

		return NOT_OK;
	}

	if ((mode < 0) || (mode > 2)) {

		return NOT_OK;
	}

	if (mode != 2)	{
		/* need to modify old data */
		i = acx111_get_feature_config(adev, &feat.feature_options,
				&feat.data_flow_options);
		if (i != OK) {
			printk("%s: acx111_s_get_feature_config: NOT_OK\n",
				__FUNCTION__);
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
		SET_BIT(feat.data_flow_options,
			cpu_to_le32(data_flow_options));
	}

	log(L_DEBUG,
	    "acx: old: feature 0x%08X dataflow 0x%08X. mode: %u\n"
	    "acx: new: feature 0x%08X dataflow 0x%08X\n",
	    feature_options, data_flow_options, mode,
	    le32_to_cpu(feat.feature_options),
	    le32_to_cpu(feat.data_flow_options));

	if (OK != acx_configure(adev, &feat, ACX1xx_IE_FEATURE_CONFIG)) {

		return NOT_OK;
	}

	return OK;
}

inline int acx111_feature_off(acx_device_t *adev, u32 f, u32 d)
{
	return acx111_set_feature_config(adev, f, d, 0);
}
inline int acx111_feature_on(acx_device_t *adev, u32 f, u32 d)
{
	return acx111_set_feature_config(adev, f, d, 1);
}
inline int acx111_feature_set(acx_device_t *adev, u32 f, u32 d)
{
	return acx111_set_feature_config(adev, f, d, 2);
}



int acx_selectchannel(acx_device_t *adev, u8 channel, int freq)
{
	int res = 0;



	adev->rx_status.freq = freq;
	adev->rx_status.band = IEEE80211_BAND_2GHZ;

	adev->channel = channel;

	adev->tx_enabled = 1;
	adev->rx_enabled = 1;

	res += acx1xx_update_tx(adev);
	res += acx1xx_update_rx(adev);



	return res ? NOT_OK : OK;
}

static void acx111_sens_radio_16_17(acx_device_t *adev)
{
	u32 feature1, feature2;

	if ((adev->sensitivity < 1) || (adev->sensitivity > 3)) {
		pr_info("%s: invalid sensitivity setting (1..3), "
		       "setting to 1\n", wiphy_name(adev->ieee->wiphy));
		adev->sensitivity = 1;
	}
	acx111_get_feature_config(adev, &feature1, &feature2);
	CLEAR_BIT(feature1, FEATURE1_LOW_RX | FEATURE1_EXTRA_LOW_RX);
	if (adev->sensitivity > 1)
		SET_BIT(feature1, FEATURE1_LOW_RX);
	if (adev->sensitivity > 2)
		SET_BIT(feature1, FEATURE1_EXTRA_LOW_RX);
	acx111_feature_set(adev, feature1, feature2);
}

void acx_get_sensitivity(acx_device_t *adev)
{

	if ( (RADIO_11_RFMD == adev->radio_type) ||
		(RADIO_0D_MAXIM_MAX2820 == adev->radio_type) ||
		(RADIO_15_RALINK == adev->radio_type))
	{
		acx_read_phy_reg(adev, 0x30, &adev->sensitivity);
	} else {
		log(L_INIT, "don't know how to get sensitivity "
				"for radio type 0x%02X\n", adev->radio_type);
		return;
	}
	log(L_INIT, "got sensitivity value %u\n", adev->sensitivity);
}

void acx_set_sensitivity(acx_device_t *adev, u8 sensitivity)
{
	adev->sensitivity = sensitivity;
	acx_update_sensitivity(adev);
}


void acx_update_sensitivity(acx_device_t *adev)
{
	if (IS_USB(adev) && IS_ACX100(adev)) {
		log(L_ANY, "Updating sensitivity on usb acx100 doesn't work yet.\n");
		return;
	}

	log(L_INIT, "updating sensitivity value: %u\n",
		adev->sensitivity);
	switch (adev->radio_type) {
	case RADIO_0D_MAXIM_MAX2820:
	case RADIO_11_RFMD:
	case RADIO_15_RALINK:
		acx_write_phy_reg(adev, 0x30, adev->sensitivity);
		break;
	case RADIO_16_RADIA_RC2422:
	case RADIO_17_UNKNOWN:
		/* TODO: check whether RADIO_1B (ex-Radia!) has same
		 * behaviour */
		acx111_sens_radio_16_17(adev);
		break;
	default:
		log(L_INIT, "don't know how to modify the sensitivity "
			"for radio type 0x%02X\n", adev->radio_type);
	}
}

static void acx_set_sane_reg_domain(acx_device_t *adev, int do_set)
{
	unsigned mask;

	unsigned int i;

	for (i = 0; i < sizeof(acx_reg_domain_ids); i++)
		if (acx_reg_domain_ids[i] == adev->reg_dom_id)
			break;

	if (sizeof(acx_reg_domain_ids) == i) {
		log(L_INIT, "Invalid or unsupported regulatory domain"
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
		memset(&dom, 0, sizeof(dom));

		dom.m.bytes[0] = adev->reg_dom_id;
		acx_configure(adev, &dom, ACX1xx_IE_DOT11_CURRENT_REG_DOMAIN);
	}

	adev->reg_dom_chanmask = reg_domain_channel_masks[i];

	mask = (1 << (adev->channel - 1));
	if (!(adev->reg_dom_chanmask & mask)) {
		/* hmm, need to adjust our channel to reside within domain */
		mask = 1;
		for (i = 1; i <= 14; i++) {
			if (adev->reg_dom_chanmask & mask) {
				pr_info("%s: Adjusting the selected channel from %d "
					"to %d due to the new regulatory domain\n",
					wiphy_name(adev->ieee->wiphy), adev->channel, i);
				adev->channel = i;
				break;
			}
			mask <<= 1;
		}
	}
}

void acx_get_reg_domain(acx_device_t *adev)
{
	acx_ie_generic_t dom;

	acx_interrogate(adev, &dom,
			ACX1xx_IE_DOT11_CURRENT_REG_DOMAIN);
	adev->reg_dom_id = dom.m.bytes[0];
	log(L_INIT, "Got regulatory domain 0x%02X\n", adev->reg_dom_id);
}

void acx_set_reg_domain(acx_device_t *adev, u8 domain_id)
{
	adev->reg_dom_id = domain_id;
	acx_update_reg_domain(adev);
}

void acx_update_reg_domain(acx_device_t *adev)
{
	log(L_INIT, "Updating the regulatory domain: 0x%02X\n",
	    adev->reg_dom_id);
	acx_set_sane_reg_domain(adev, 1);
}

int acx1xx_set_tx_level_dbm(acx_device_t *adev, int level_dbm)
{
	adev->tx_level_dbm = level_dbm;
	return acx1xx_update_tx_level_dbm(adev);
}

int acx1xx_update_tx_level_dbm(acx_device_t *adev)
{
	u8 level_val;
	/* Number of level of device */
	int numl;
	/* Dbm per level */
	int dpl;
	/* New level reverse */
	int nlr;
	/*  Helper for modulo dpl, ... */
	int helper;

	/* The acx is working with power levels, which shift the
	 * tx-power in partitioned steps and also depending on the
	 * configured regulatory domain.
	 *
	 * The acx111 has five tx_power levels, the acx100 we assume two.
	 *
	 * The acx100 also displays them in co_powerlevels_t config
	 * options. We could use this info for a more precise
	 * matching, but for the time being, we assume there two
	 * levels by default.
	 *
	 * The approach here to set the corresponding tx-power level
	 * here, is to translate the requested tx-power in dbm onto a
	 * scale of 0-20dbm, with an assumed maximum of 20dbm. The
	 * maximum would normally vary depending on the regulatory
	 * domain.
	 *
	 * The the value on the 0-20dbm scale is then matched onto the
	 * available levels.
	 */

	if (adev->tx_level_dbm > TX_CFG_MAX_DBM_POWER) {
		logf1(L_ANY, "Err: Setting tx-power > %d dbm not supported\n",
			TX_CFG_MAX_DBM_POWER);
		return (NOT_OK);
	}

	if (IS_ACX111(adev))
		numl = TX_CFG_ACX111_NUM_POWER_LEVELS;
	else if (IS_ACX100(adev))
		numl = TX_CFG_ACX100_NUM_POWER_LEVELS;
	else
		return (NOT_OK);

	dpl = TX_CFG_MAX_DBM_POWER / numl;

	/* Find closest match */
	nlr = adev->tx_level_dbm / dpl;
	helper = adev->tx_level_dbm % dpl;
	if (helper > dpl - helper)
		nlr++;

	/* Adjust to boundaries (level zero doesn't exists, adjust to 1) */
	if (nlr < 1)
		nlr = 1;
	if (nlr > numl)
		nlr = numl;

	/* Translate to final level_val */
	level_val = numl - nlr + 1;

	/* Inform of adjustments */
	if (nlr * dpl != adev->tx_level_dbm) {
		helper = adev->tx_level_dbm;
		adev->tx_level_dbm = nlr * dpl;
		log(L_ANY, "Tx-power adjusted from %d to %d dbm (tx-power-level: %d)\n", helper, adev->tx_level_dbm, level_val);
	}
	return acx1xx_set_tx_level(adev, level_val);
}

int acx1xx_get_tx_level(acx_device_t *adev)
{
	struct acx1xx_ie_tx_level tx_level;



	if (IS_USB(adev)) {
		logf0(L_ANY, "Get tx-level not yet supported on usb\n");
		goto end;
	}

	memset(&tx_level, 0, sizeof(tx_level));

	if (OK != acx_interrogate(adev, &tx_level,
					ACX1xx_IE_DOT11_TX_POWER_LEVEL)) {

		return NOT_OK;
	}
	adev->tx_level_val = tx_level.level;
	log(L_ANY, "Got tx-power-level: %d\n", adev->tx_level_val);
end:

	return OK;
}

int acx1xx_set_tx_level(acx_device_t *adev, u8 level_val)
{
	adev->tx_level_val = level_val;
	return acx1xx_update_tx_level(adev);
}

int acx1xx_update_tx_level(acx_device_t *adev)
{
	struct acx1xx_ie_tx_level tx_level;

	if (IS_USB(adev)) {
		logf0(L_ANY, "Update tx-level not yet supported on usb\n");
		return OK;
	}

	log(L_ANY, "Updating tx-power-level to: %d\n", adev->tx_level_val);
	memset(&tx_level, 0, sizeof(tx_level));
	tx_level.level = adev->tx_level_val;

	return acx_configure(adev, &tx_level, ACX1xx_IE_DOT11_TX_POWER_LEVEL);
}

/* OW: Previously included tx-power related functions, kept for documentation */
#if 0
/*
 * FIXME: this should be solved in a general way for all radio types
 * by decoding the radio firmware module,
 * since it probably has some standard structure describing how to
 * set the power level of the radio module which it controls.
 * Or maybe not, since the radio module probably has a function interface
 * instead which then manages Tx level programming :-\
 */
int acx111_set_tx_level(acx_device_t * adev, u8 level_dbm)
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
		log(L_INIT, "only predefined transmission "
		    "power levels are supported at this time: "
		    "adjusted %d dBm to %d dBm\n", level_dbm,
		    adev->tx_level_dbm);

	return acx_configure(adev, &tx_level, ACX1xx_IE_DOT11_TX_POWER_LEVEL);
}

int acx100pci_set_tx_level(acx_device_t * adev, u8 level_dbm)
{
	/* since it can be assumed that at least the Maxim radio has a
	 * maximum power output of 20dBm and since it also can be
	 * assumed that these values drive the DAC responsible for
	 * setting the linear Tx level, I'd guess that these values
	 * should be the corresponding linear values for a dBm value,
	 * in other words:
	 * calculate the values from that formula:
	 * Y [dBm] = 10 * log (X [mW])
	 * then scale the 0..63 value range onto the 1..100mW range
	 * (0..20 dBm) and you're done...
	 * Hopefully that's ok, but you never know if we're actually
	 * right... (especially since Windows XP doesn't seem to show
	 * actual Tx dBm values :-P) */

	/* NOTE: on Maxim, value 30 IS 30mW, and value 10 IS 10mW - so
	 * the values are EXACTLY mW!!! Not sure about RFMD and
	 * others, though... */
	static const u8 dbm2val_maxim[21] = {
		63, 63, 63, 62,
		61, 61, 60, 60,
		59, 58, 57, 55,
		53, 50, 47, 43,
		38, 31, 23, 13,
		0
	};
	static const u8 dbm2val_rfmd[21] = {
		0, 0, 0, 1,
		2, 2, 3, 3,
		4, 5, 6, 8,
		10, 13, 16, 20,
		25, 32, 41, 50,
		63
	};
	const u8 *table;

	switch (adev->radio_type) {
	case RADIO_0D_MAXIM_MAX2820:
		table = &dbm2val_maxim[0];
		break;
	case RADIO_11_RFMD:
	case RADIO_15_RALINK:
		table = &dbm2val_rfmd[0];
		break;
	default:
		pr_info("%s: unknown/unsupported radio type, "
		       "cannot modify tx power level yet!\n",
			wiphy_name(adev->ieee->wiphy));
		return NOT_OK;
	}
	pr_info("%s: changing radio power level to %u dBm (%u)\n",
	       wiphy_name(adev->ieee->wiphy), level_dbm, table[level_dbm]);
	acx_write_phy_reg(adev, 0x11, table[level_dbm]);
	return OK;
}

/* Comment int acx100mem_set_tx_level(acx_device_t *adev, u8
   level_dbm) Otherwise equal with int
   acx100pci_set_tx_level(acx_device_t * adev, u8 level_dbm) */
/*
 * The hx4700 EEPROM, at least, only supports 1 power setting.  The
 * configure routine matches the PA bias with the gain, so just use
 * its default value.  The values are: 0x2b for the gain and 0x03 for
 * the PA bias.  The firmware writes the gain level to the Tx gain
 * control DAC and the PA bias to the Maxim radio's PA bias register.
 * The firmware limits itself to 0 - 64 when writing to the gain
 * control DAC.
 *
 * Physically between the ACX and the radio, higher Tx gain control
 * DAC values result in less power output; 0 volts to the Maxim radio
 * results in the highest output power level, which I'm assuming
 * matches up with 0 in the Tx Gain DAC register.
 *
 * Although there is only the 1 power setting, one of the radio
 * firmware functions adjusts the transmit power level up and down.
 * That function is called by the ACX FIQ handler under certain
 * conditions.
 */
#endif

int acx1xx_get_antenna(acx_device_t *adev)
{
	int res;
	u8 antenna[4 + acx_ie_descs[ACX1xx_IE_DOT11_CURRENT_ANTENNA].len];



	memset(antenna, 0, sizeof(antenna));
	res = acx_interrogate(adev, antenna,
			  ACX1xx_IE_DOT11_CURRENT_ANTENNA);
	adev->antenna[0] = antenna[4];
	adev->antenna[1] = antenna[5];
	log(L_INIT, "Got antenna[0,1]: 0x%02X 0x%02X\n", adev->antenna[0], adev->antenna[1]);


	return res;
}

int acx1xx_set_antenna(acx_device_t *adev, u8 val0, u8 val1)
{
	int res;



	adev->antenna[0] = val0;
	adev->antenna[1] = val1;
	res = acx1xx_update_antenna(adev);


	return res;
}

int acx1xx_update_antenna(acx_device_t *adev)
{
	int res;
	u8 antenna[4 + acx_ie_descs[ACX1xx_IE_DOT11_CURRENT_ANTENNA].len];



	log(L_INIT, "Updating antenna[0,1]: 0x%02X 0x%02X\n",
		adev->antenna[0], adev->antenna[1]);
	memset(antenna, 0, sizeof(antenna));
	antenna[4] = adev->antenna[0];
	antenna[5] = adev->antenna[1];
	res = acx_configure(adev, &antenna,
			ACX1xx_IE_DOT11_CURRENT_ANTENNA);


	return res;
}

/* OW: Transfered from the acx-20080210 ioctl calls, but didn't test of verify */
#if 0
/*
 * 0 = antenna1; 1 = antenna2; 2 = full diversity; 3 = partial diversity
 */
static int acx100_set_rx_antenna(acx_device_t *adev, u8 val)
{
	int result;



	if (val > 3) {
		result = -EINVAL;
		goto end;
	}

	logf1(L_ANY, "old antenna value: 0x%02X\n", adev->antenna[0]);

	acx_sem_lock(adev);

	adev->antenna[0] &= 0x3f;
	SET_BIT(adev->antenna[0], (val << 6));
	logf1(L_ANY, "new antenna value: 0x%02X\n", adev->antenna[0]);

	result = acx1xx_update_antenna(adev);

	acx_sem_unlock(adev);
end:

	return result;
}

/*
 * Arguments: 0 == antenna1; 1 == antenna2;
 * Could anybody test which antenna is the external one?
 */
static int acx100_set_tx_antenna(acx_device_t *adev, u8 val)
{
	int result;
	u8 val2;



	if (val > 1) {
		result = -EINVAL;
		goto end;
	}

	logf1(L_ANY, "old antenna value: 0x%02X\n", adev->antenna[0]);

	acx_sem_lock(adev);

	/* swap antenna 1/2 values */
	switch (val) {
	case 0:
		val2 = 1;
		break;
	case 1:
		val2 = 0;
		break;
	default:
		val2 = val;
	}
	logf1(L_ANY, "val2=%02d\n", val2);

	adev->antenna[0] &= ~0x30;
	SET_BIT(adev->antenna[0], ((val2 & 0x01) << 5));
	logf1(L_ANY, "new antenna value: 0x%02X\n", adev->antenna[0]);

	result = acx1xx_update_antenna(adev);

	acx_sem_unlock(adev);
end:

	return result;
}
#endif

#ifdef UNUSED
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
		/* other types of stations do not emit beacons */
	}

	if (adev->wep_restricted)
		SET_BIT(cap, WF_MGMT_CAP_PRIVACY);

	if (acfg->dot11ShortPreambleOption)
		SET_BIT(cap, WF_MGMT_CAP_SHORT);

	if (acfg->dot11PBCCOption)
		SET_BIT(cap, WF_MGMT_CAP_PBCC);

	if (acfg->dot11ChannelAgility)
		SET_BIT(cap, WF_MGMT_CAP_AGILITY);

	log(L_DEBUG, "caps updated from 0x%04X to 0x%04X\n",
	    adev->capabilities, cap);
	adev->capabilities = cap;
}
#endif

static int acx1xx_get_station_id(acx_device_t *adev)
{
	u8 stationID[4 + acx_ie_descs[ACX1xx_IE_DOT11_STATION_ID].len];
	const u8 *paddr;
	int i, res;



	res = acx_interrogate(adev, &stationID, ACX1xx_IE_DOT11_STATION_ID);
	paddr = &stationID[4];
	for (i = 0; i < ETH_ALEN; i++) {
		/* we copy the MAC address (reversed in the card) to
		 * the netdevice's MAC address, and on ifup it will be
		 * copied into iwadev->dev_addr */
		adev->dev_addr[ETH_ALEN - 1 - i] = paddr[i];
	}

	log(L_INIT, "Got station_id: " MACSTR "\n", MAC(adev->dev_addr));


	return res;
}

int acx1xx_set_station_id(acx_device_t *adev, u8 *new_addr)
{
	int res;



	MAC_COPY(adev->dev_addr, new_addr);
	res = acx1xx_update_station_id(adev);


	return res;
}

int acx1xx_update_station_id(acx_device_t *adev)
{
	u8 stationID[4 + acx_ie_descs[ACX1xx_IE_DOT11_STATION_ID].len];
	u8 *paddr;
	int i, res;



	log(L_INIT, "Updating station_id to: " MACSTR "\n",
		MAC(adev->dev_addr));

	paddr = &stationID[4];
	for (i = 0; i < ETH_ALEN; i++) {
		/* copy the MAC address we obtained when we noticed
		 * that the ethernet iface's MAC changed to the card
		 * (reversed in the card!) */
		paddr[i] = adev->dev_addr[ETH_ALEN - 1 - i];
	}
	res = acx_configure(adev, &stationID, ACX1xx_IE_DOT11_STATION_ID);


	return res;
}

static int acx100_get_ed_threshold(acx_device_t *adev)
{
	int res;
	u8 ed_threshold[4 + acx_ie_descs[ACX100_IE_DOT11_ED_THRESHOLD].len];


	memset(ed_threshold, 0, sizeof(ed_threshold));
	res = acx_interrogate(adev, ed_threshold,
			  ACX100_IE_DOT11_ED_THRESHOLD);
	adev->ed_threshold = ed_threshold[4];


	return res;
}

static int acx1xx_get_ed_threshold(acx_device_t *adev)
{
	int res = NOT_OK;



	if (IS_ACX100(adev)) {
		res = acx100_get_ed_threshold(adev);
	} else {
		log(L_INIT, "acx111 doesn't support ED\n");
		adev->ed_threshold = 0;
	}

	log(L_INIT, "Got Energy Detect (ED) threshold %u\n",
	    adev->ed_threshold);


	return res;
}

#ifdef UNUSED
static int acx1xx_set_ed_threshold(acx_device_t *adev, u8 ed_threshold)
{
	int res;


	adev->ed_threshold=ed_threshold;
	res = acx1xx_update_ed_threshold(adev);


	return res;
}
#endif

static int acx100_update_ed_threshold(acx_device_t *adev)
{
	int res;
	u8 ed_threshold[4 + acx_ie_descs[ACX100_IE_DOT11_ED_THRESHOLD].len];


	memset(ed_threshold, 0, sizeof(ed_threshold));
	ed_threshold[4] = adev->ed_threshold;
	res = acx_configure(adev, &ed_threshold,
			ACX100_IE_DOT11_ED_THRESHOLD);


	return res;
}

int acx1xx_update_ed_threshold(acx_device_t *adev)
{
	int res = NOT_OK;


	log(L_INIT, "Updating the Energy Detect (ED) threshold: %u\n",
	    adev->ed_threshold);

	if (IS_ACX100(adev))
		res = acx100_update_ed_threshold(adev);
	else
		log(L_INIT, "acx111 doesn't support ED threshold\n");


	return res;
}

static int acx100_get_cca(acx_device_t *adev)
{
	int res;
	u8 cca[4 + acx_ie_descs[ACX1xx_IE_DOT11_CURRENT_CCA_MODE].len];


	memset(cca, 0, sizeof(cca));
	res = acx_interrogate(adev, cca,
			ACX1xx_IE_DOT11_CURRENT_CCA_MODE);
	adev->cca = cca[4];


	return res;
}

static int acx1xx_get_cca(acx_device_t *adev)
{
	int res = NOT_OK;


	if (IS_ACX100(adev))
		acx100_get_cca(adev);
	else {
		log(L_INIT, "acx111 doesn't support CCA\n");
		adev->cca = 0;
	}
	log(L_INIT, "Got Channel Clear Assessment (CCA) value %u\n",
		adev->cca);


	return res;
}

#ifdef UNUSED
static int acx1xx_set_cca(acx_device_t *adev, u8 cca)
{
	int res;



	adev->cca = cca;
	res = acx1xx_update_cca(adev);


	return res;
}
#endif

static int acx100_update_cca(acx_device_t *adev)
{
	int res;
	u8 cca[4 + acx_ie_descs[ACX1xx_IE_DOT11_CURRENT_CCA_MODE].len];



	memset(cca, 0, sizeof(cca));
	cca[4] = adev->cca;
	res = acx_configure(adev, &cca,
			ACX1xx_IE_DOT11_CURRENT_CCA_MODE);


	return res;
}

int acx1xx_update_cca(acx_device_t *adev)
{
	int res = NOT_OK;


	log(L_INIT, "Updating the Channel Clear Assessment (CCA) value: "
			"0x%02X\n", adev->cca);
	if (IS_ACX100(adev))
		res = acx100_update_cca(adev);
	else
		log(L_INIT, "acx111 doesn't support CCA\n");


	return res;
}

#ifdef UNUSED
static int acx1xx_get_rate_fallback(acx_device_t *adev)
{
	int res = NOT_OK;
	u8 rate[4 + acx_ie_descs[ACX1xx_IE_RATE_FALLBACK].len];


	memset(rate, 0, sizeof(rate));
	res = acx_interrogate(adev, &rate,
			ACX1xx_IE_RATE_FALLBACK);
	adev->rate_auto = rate[4];


	return res;
}

static int acx1xx_set_rate_fallback(acx_device_t *adev, u8 rate_auto)
{
	int res;

	adev->rate_auto = rate_auto;
	res = acx1xx_update_rate_fallback(adev);

	return res;
}
#endif

int acx1xx_update_rate_fallback(acx_device_t *adev)
{
	int res;
	u8 rate[4 + acx_ie_descs[ACX1xx_IE_RATE_FALLBACK].len];


	/* configure to not do fallbacks when not in auto rate mode */
	rate[4] = (adev->rate_auto) /* adev->txrate_fallback_retries */
		? 1 : 0;
	log(L_INIT, "Updating Tx fallback to %u retries\n", rate[4]);

	res = acx_configure(adev, &rate, ACX1xx_IE_RATE_FALLBACK);

	return res;
}

#ifdef UNUSED
static int acx1xx_set_channel(acx_device_t *adev, u8 channel)
{
	int res;

	adev->channel = channel;
	res = acx1xx_update_tx(adev);

	return res;
}
#endif

static int acx1xx_set_tx_enable(acx_device_t *adev, u8 tx_enabled)
{
	int res;

	adev->tx_enabled = tx_enabled;
	res = acx1xx_update_tx(adev);

	return res;
}

int acx1xx_update_tx(acx_device_t *adev)
{
	int res;


	log(L_XFER, "Updating TX: %s, channel=%d\n",
		adev->tx_enabled ? "enable" : "disable", adev->channel);

	if (adev->tx_enabled)
		res = acx_issue_cmd(adev, ACX1xx_CMD_ENABLE_TX,
				&adev->channel, 1);
	else
		res = acx_issue_cmd(adev, ACX1xx_CMD_DISABLE_TX, NULL, 0);


	return res;
}

static int acx1xx_set_rx_enable(acx_device_t *adev, u8 rx_enabled)
{
	int res;

	adev->rx_enabled = rx_enabled;
	res = acx1xx_update_rx(adev);

	return res;
}

int acx1xx_update_rx(acx_device_t *adev)
{
	int res;


	log(L_XFER, "Updating RX: %s, channel=%d\n",
		adev->rx_enabled ? "enable" : "disable", adev->channel);

	if (adev->rx_enabled)
		res = acx_issue_cmd(adev, ACX1xx_CMD_ENABLE_RX,
				&adev->channel, 1);
	else
		res = acx_issue_cmd(adev, ACX1xx_CMD_DISABLE_RX, NULL, 0);


	return res;
}

int acx1xx_update_retry(acx_device_t *adev)
{
	int res;
	u8 short_retry[4 + acx_ie_descs[ACX1xx_IE_DOT11_SHORT_RETRY_LIMIT].len];
	u8 long_retry[4 + acx_ie_descs[ACX1xx_IE_DOT11_LONG_RETRY_LIMIT].len];



	log(L_INIT, "Updating the short retry limit: %u, "
		"long retry limit: %u\n",
		adev->short_retry, adev->long_retry);

	short_retry[0x4] = adev->short_retry;
	long_retry[0x4] = adev->long_retry;
	res = acx_configure(adev, &short_retry,
			ACX1xx_IE_DOT11_SHORT_RETRY_LIMIT);
	res += acx_configure(adev, &long_retry,
			ACX1xx_IE_DOT11_LONG_RETRY_LIMIT);


	return res;
}

int acx1xx_update_msdu_lifetime(acx_device_t *adev)
{
	int res = NOT_OK;
	u8 xmt_msdu_lifetime[4 + acx_ie_descs[ACX1xx_IE_DOT11_MAX_XMIT_MSDU_LIFETIME].len];


	log(L_INIT, "Updating the tx MSDU lifetime: %u\n",
		adev->msdu_lifetime);

	*(u32 *) &xmt_msdu_lifetime[4] = cpu_to_le32(
		(u32) adev->msdu_lifetime);
	res = acx_configure(adev, &xmt_msdu_lifetime,
	                ACX1xx_IE_DOT11_MAX_XMIT_MSDU_LIFETIME);

	return res;
}

int acx_set_hw_encryption_on(acx_device_t *adev)
{
	int res;

	if(adev->hw_encrypt_enabled)
		return OK;

	if (IS_ACX100(adev)){
		log(L_INIT, "acx100: hw-encryption not supported\n");
		return -EOPNOTSUPP;
	}

	log(L_INIT, "Enabling hw-encryption\n");

	res = acx111_feature_off(adev, 0,
				FEATURE2_NO_TXCRYPT | FEATURE2_SNIFFER);
	adev->hw_encrypt_enabled=1;

	return res;
}

int acx_set_hw_encryption_off(acx_device_t *adev)
{
	int res;

	if(!adev->hw_encrypt_enabled)
		return OK;

	if (IS_ACX100(adev)){
		log(L_INIT, "acx100: hw-encryption not supported\n");
		return OK;
	}

	log(L_INIT, "Disabling hw-encryption\n");

	res = acx111_feature_on(adev, 0,
				FEATURE2_NO_TXCRYPT | FEATURE2_SNIFFER);
	adev->hw_encrypt_enabled=0;

	return res;
}


static int acx111_update_recalib_auto(acx_device_t *adev)
{
	acx111_cmd_radiocalib_t cal;

	if (!IS_ACX111(adev)) {
		log(L_INIT, "Firmware auto radio-recalibration"
			" not supported on acx100.\n");
		return(-1);
	}

	if (adev->recalib_auto) {
		log(L_INIT, "Enabling firmware auto radio-recalibration.\n");
		/* automatic recalibration, choose all methods: */
		cal.methods = cpu_to_le32(0x8000000f);
		/* automatic recalibration every 60 seconds (value in TUs)
		 * I wonder what the firmware default here is? */
		cal.interval = cpu_to_le32(58594);
	} else {
		log(L_INIT, "Disabling firmware auto radio-recalibration.\n");
		cal.methods = 0;
		cal.interval = 0;
	}

	return acx_issue_cmd_timeout(adev, ACX111_CMD_RADIOCALIB,
			&cal, sizeof(cal),
			CMD_TIMEOUT_MS(100));
}

int acx111_set_recalib_auto(acx_device_t *adev, int enable)
{
	adev->recalib_auto=enable;
	return(acx111_update_recalib_auto(adev));
}

#ifdef UNUSED
static void acx100_set_wepkey(acx_device_t *adev)
{
	ie_dot11WEPDefaultKey_t dk;
	int i;

	for (i = 0; i < DOT11_MAX_DEFAULT_WEP_KEYS; i++) {
		if (adev->wep_keys[i].size != 0) {
			log(L_INIT, "setting WEP key: %d with "
				"total size: %d\n", i,
				(int)adev->wep_keys[i].size);
			dk.action = 1;
			dk.keySize = adev->wep_keys[i].size;
			dk.defaultKeyNum = i;
			memcpy(dk.key, adev->wep_keys[i].key, dk.keySize);
			acx_configure(adev, &dk,
				ACX100_IE_DOT11_WEP_DEFAULT_KEY_WRITE);
		}
	}
}

static void acx_set_wepkey(acx_device_t * adev)
{
	if (IS_ACX111(adev))
		acx111_set_wepkey(adev);
	else
		acx100_set_wepkey(adev);
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

static int acx_update_wep(acx_device_t *adev)
{
	int res = NOT_OK;
	ie_dot11WEPDefaultKeyID_t dkey;



#ifdef DEBUG_WEP
	struct {
		u16 type;
		u16 len;
		u8 val;
	} ACX_PACKED keyindic;
#endif
	log(L_INIT, "updating WEP key settings\n");

	acx_set_wepkey(adev);
	if (adev->wep_enabled) {
		dkey.KeyID = adev->wep_current_index;
		log(L_INIT, "setting WEP key %u as default\n", dkey.KeyID);
		res = acx_configure(adev, &dkey,
				ACX1xx_IE_DOT11_WEP_DEFAULT_KEY_SET);
#ifdef DEBUG_WEP
		keyindic.val = 3;
		acx_configure(adev, &keyindic, ACX111_IE_KEY_CHOOSE);
#endif
	}


	return res;
}

static int acx_update_wep_options(acx_device_t *adev)
{
	int res = NOT_OK;



	if (IS_ACX111(adev))
		log(L_DEBUG, "setting WEP Options for acx111"
			" is not supported\n");
	else
		res = acx100_update_wep_options(adev);


	return res;
}

static int acx100_update_wep_options(acx_device_t *adev)
{
	int res = NOT_OK;
	acx100_ie_wep_options_t options;


	log(L_INIT, "acx100: setting WEP Options\n");

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

	res = acx_configure(adev, &options, ACX100_IE_WEP_OPTIONS);


	return res;
}
#endif

int acx_set_tim_template(acx_device_t *adev, u8 *data, int len)
{
	acx_template_tim_t templ;
	int res;



	if (acx_debug & L_DEBUG) {
		logf1(L_ANY, "data, len=%d:\n", len);
		acx_dump_bytes(data, len);
	}

	/* We need to set always a tim template, even with len=0,
	* since otherwise the acx is sending a not 100% well
	* structured beacon (this may not be blocking though, but it's
	* better like this)
	*/
	memset(&templ, 0, sizeof(templ));
	if (data)
		memcpy((u8*) &templ.tim_eid, data, len);
	templ.size = cpu_to_le16(len);

	res = acx_issue_cmd(adev, ACX1xx_CMD_CONFIG_TIM, &templ,
			sizeof(templ));

	return res;
}

int acx_set_probe_request_template(acx_device_t *adev, unsigned char *data, unsigned int len)
{
        struct acx_template_probereq probereq;
        int res;

        if (len > sizeof(probereq)-2)
        {
        	WARN_ONCE(1, "size of acx_template_probereq too small!");
        	return -1;
        }

        memcpy(&probereq.fc, data, len);

        probereq.size = cpu_to_le16(len);
        res = acx_issue_cmd(adev, ACX1xx_CMD_CONFIG_PROBE_REQUEST, &probereq, len+2);
        return res;
}


#ifdef UNUSED_BUT_USEFULL
static int acx_s_set_tim_template_off(acx_device_t *adev)
{
	acx_template_nullframe_t templ;
	int result;


	memset(&templ, 0, sizeof(templ));
	templ.size = cpu_to_le16(sizeof(templ) - 2);;

	result = acx_issue_cmd(adev, ACX1xx_CMD_CONFIG_TIM,
			&templ, sizeof(templ));


	return result;
}
#endif

#if POWER_SAVE_80211
static int acx_s_set_null_data_template(acx_device_t *adev)
{
	struct acx_template_nullframe b;
	int result;



	/* memset(&b, 0, sizeof(b)); not needed, setting all members */

	b.size = cpu_to_le16(sizeof(b) - 2);
	b.hdr.fc = WF_FTYPE_MGMTi | WF_FSTYPE_NULLi;
	b.hdr.dur = 0;
	MAC_BCAST(b.hdr.a1);
	MAC_COPY(b.hdr.a2, adev->dev_addr);
	MAC_COPY(b.hdr.a3, adev->bssid);
	b.hdr.seq = 0;

	result = acx_issue_cmd(adev, ACX1xx_CMD_CONFIG_NULL_DATA,
			&b, sizeof(b));


	return result;
}
#endif

static int acx_set_beacon_template(acx_device_t *adev, u8 *data, int len)
{
	struct acx_template_beacon templ;
	int res;



	if (acx_debug & L_DEBUG) {
		logf1(L_ANY, "data, len=%d, sizeof(struct"
			"acx_template_beacon)=%d:\n",
			len, (int)sizeof(struct acx_template_beacon));
		acx_dump_bytes(data, len);
	}

	memcpy((u8*) &templ.fc, data, len);
	templ.size = cpu_to_le16(len);

	/* +2: include 'u16 size' field */
	res = acx_issue_cmd(adev, ACX1xx_CMD_CONFIG_BEACON, &templ, len+2);


	return res;
}

static int acx_set_probe_response_template(acx_device_t *adev, u8* data,
					int len)
{
	struct acx_template_proberesp templ;
	int res;



	memcpy((u8*) &templ.fc, data, len);
	templ.fc = cpu_to_le16(IEEE80211_FTYPE_MGMT
			| IEEE80211_STYPE_PROBE_RESP);

	templ.size = cpu_to_le16(len);

	res = acx_issue_cmd(adev, ACX1xx_CMD_CONFIG_PROBE_RESPONSE,
			&templ, len+2);


	return res;
}


/* Find position of TIM IE */
static u8* acx_beacon_find_tim(struct sk_buff *beacon_skb)
{
	u8 *p1, *p2, *tim;
	int len1;

	struct wlan_ie_base {
		u8 eid;
		u8 len;
	} __attribute__ ((packed));
	struct wlan_ie_base *ie;

	p1 = beacon_skb->data;
	len1 = beacon_skb->len;
	p2 = p1;
	p2 += offsetof(struct ieee80211_mgmt, u.beacon.variable);

	tim = p2;
	while (tim < p1 + len1) {
		ie = (struct wlan_ie_base*) tim;
		if (ie->eid == WLAN_EID_TIM)
			break;
		tim += ie->len + 2;
	}
	if (tim >= p1 + len1) {
		logf0(L_ANY, "No TIM IE found\n");
		return NULL;
	}
	return tim;
}

int acx_set_beacon(acx_device_t *adev, struct sk_buff *beacon)
{
	int res;
	u8 *tim_pos;
	int len_wo_tim;
	int len_tim;

	/* The TIM template handling between ACX100 and ACX111 works
	 * differently:
	 *
	 * ACX111: Needs TIM in dedicated template via
	 * ACX1xx_CMD_CONFIG_TIM
	 *
	 * ACX100: Needs TIM included into the beacon, however space
	 * for TIM template needs to be configured during memory-map
	 * setup
	 */

	/* TODO Use pos provided by ieee80211_beacon_get_tim instead */
	tim_pos = acx_beacon_find_tim(beacon);
	if (tim_pos == NULL)
		logf0(L_DEBUG, "No tim contained in beacon skb");

	/* ACX111: If beacon contains tim, only configure
	 * beacon-template until tim
	 */
	if (IS_ACX111(adev) && tim_pos)
		len_wo_tim = tim_pos - beacon->data;
	else
		len_wo_tim = beacon->len;

	res = acx_set_beacon_template(adev, beacon->data, len_wo_tim);
	if (res)
		goto out;

	/* We need to set always a tim template, even if length it
	 * null, since otherwise the acx is not sending fully correct
	 * structured beacons.
	 */
	if (IS_ACX111(adev))
	{
		len_tim = beacon->len - len_wo_tim;
		acx_set_tim_template(adev, tim_pos, len_tim);
	}

	/* BTW acx111 firmware would not send probe responses if probe
	 * request does not have all basic rates flagged by 0x80!
	 * Thus firmware does not conform to 802.11, it should ignore
	 * 0x80 bit in ratevector from STA.  We can 'fix' it by not
	 * using this template and sending probe responses by
	 * hand. TODO --vda */
	res = acx_set_probe_response_template(adev, beacon->data, beacon->len);
	if (res)
		goto out;
	/* acx_s_set_probe_response_template_off(adev); */

	/* Needed if generated frames are to be emitted at different
	 * tx rate now */
	logf0(L_ANY, "Redoing cmd_join_bssid() following template cfg\n");
	res = acx_cmd_join_bssid(adev, adev->bssid);

	out:
	return res;
}

#ifdef UNUSED_BUT_USEFULL
static int acx_s_set_probe_response_template_off(acx_device_t *adev)
{
	acx_template_nullframe_t templ;
	int result;


	memset(&templ, 0, sizeof(templ));
	templ.size = cpu_to_le16(sizeof(templ) - 2);;

	result = acx_issue_cmd(adev, ACX1xx_CMD_CONFIG_PROBE_RESPONSE,
			&templ, sizeof(templ));


	return result;
}
#endif


#ifdef UNUSED_BUT_USEFULL
static int acx_s_set_probe_request_template(acx_device_t *adev)
{
	struct acx_template_probereq probereq;
	char *p;
	int res;
	int frame_len;



	memset(&probereq, 0, sizeof(probereq));

	probereq.fc = WF_FTYPE_MGMTi | WF_FSTYPE_PROBEREQi;
	MAC_BCAST(probereq.da);
	MAC_COPY(probereq.sa, adev->dev_addr);
	MAC_BCAST(probereq.bssid);

	p = probereq.variable;
	p = wlan_fill_ie_ssid(p, adev->essid_len, adev->essid);
	p = wlan_fill_ie_rates(p, adev->rate_supported_len,
			adev->rate_supported);
	p = wlan_fill_ie_rates_ext(p, adev->rate_supported_len,
			adev->rate_supported);
	frame_len = p - (char*)&probereq;
	probereq.size = cpu_to_le16(frame_len - 2);

	res = acx_issue_cmd(adev, ACX1xx_CMD_CONFIG_PROBE_REQUEST,
			&probereq, frame_len);

	return res;
}
#endif


/*
 * acx_l_update_ratevector
 *
 * Updates adev->rate_supported[_len] according to rate_{basic,oper}
 */
static void acx_update_ratevector(acx_device_t *adev)
{
	u16 bcfg = adev->rate_basic;
	u16 ocfg = adev->rate_oper;
	u8 *supp = adev->rate_supported;
	const u8 *dot11 = acx_bitpos2ratebyte;



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
		pr_info("new ratevector: ");
		acx_dump_bytes(adev->rate_supported, adev->rate_supported_len);
	}

}


static int acx_update_rx_config(acx_device_t *adev)
{
	int res;
	struct {
		u16 id;
		u16 len;
		u16 rx_cfg1;
		u16 rx_cfg2;
	} ACX_PACKED cfg;



	switch (adev->mode) {
	case ACX_MODE_MONITOR:
		log(L_INIT, "acx_update_rx_config: ACX_MODE_MONITOR\n");
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
		   | RX_CFG2_RCV_OTHER
		   );
		break;

	case ACX_MODE_3_AP:
		log(L_INIT, "acx_update_rx_config: ACX_MODE_3_AP\n");
		adev->rx_config_1 = (u16) (0
		   /* | RX_CFG1_INCLUDE_RXBUF_HDR */
		   /* | RX_CFG1_FILTER_SSID       */
		   /* | RX_CFG1_FILTER_BCAST      */
		   /* | RX_CFG1_RCV_MC_ADDR1      */
		   /* | RX_CFG1_RCV_MC_ADDR0      */
		   /* | RX_CFG1_FILTER_ALL_MULTI  */
		   /* | RX_CFG1_FILTER_BSSID      */
		   | RX_CFG1_FILTER_MAC
		   /* | RX_CFG1_RCV_PROMISCUOUS   */
		   /* | RX_CFG1_INCLUDE_FCS       */
		   /* | RX_CFG1_INCLUDE_PHY_HDR   */
		   );
		adev->rx_config_2 = (u16) (0
		   | RX_CFG2_RCV_ASSOC_REQ
		   | RX_CFG2_RCV_AUTH_FRAMES
		   /*| RX_CFG2_RCV_BEACON_FRAMES  */
		   | RX_CFG2_RCV_CONTENTION_FREE
		   | RX_CFG2_RCV_CTRL_FRAMES
		   | RX_CFG2_RCV_DATA_FRAMES
		   /*| RX_CFG2_RCV_BROKEN_FRAMES  */
		   | RX_CFG2_RCV_MGMT_FRAMES
		   /* | RX_CFG2_RCV_PROBE_REQ     */
		   | RX_CFG2_RCV_PROBE_RESP
		   /*| RX_CFG2_RCV_ACK_FRAMES     */
		   | RX_CFG2_RCV_OTHER
		   );
		break;

	default:
		log(L_INIT, "acx_update_rx_config: default\n");
		adev->rx_config_1 = (u16) (0
		   /* | RX_CFG1_INCLUDE_RXBUF_HDR  */
		   /* | RX_CFG1_FILTER_SSID        */
		   /* | RX_CFG1_FILTER_BCAST       */
		   /* | RX_CFG1_RCV_MC_ADDR1       */
		   /* | RX_CFG1_RCV_MC_ADDR0       */
		   /* | RX_CFG1_FILTER_ALL_MULTI   */
		   /* | RX_CFG1_FILTER_BSSID       */
		   | RX_CFG1_FILTER_MAC
		   /* | RX_CFG1_RCV_PROMISCUOUS    */
		   /* | RX_CFG1_INCLUDE_FCS        */
		   /* | RX_CFG1_INCLUDE_PHY_HDR    */
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
		   /* | RX_CFG2_RCV_PROBE_REQ      */
		   | RX_CFG2_RCV_PROBE_RESP
		   /*| RX_CFG2_RCV_ACK_FRAMES      */
		   | RX_CFG2_RCV_OTHER
		   );
		break;
	}

	adev->rx_config_1 |= RX_CFG1_INCLUDE_RXBUF_HDR;

	if ((adev->rx_config_1 & RX_CFG1_INCLUDE_PHY_HDR)
		|| (adev->firmware_numver >= 0x02000000))
		adev->phy_header_len = IS_ACX111(adev) ? 8 : 4;
	else
		adev->phy_header_len = 0;

	log(L_INIT, "Updating RXconfig to mode=0x%04X,"
		"rx_config_1:2=%04X:%04X\n",
		adev->mode, adev->rx_config_1, adev->rx_config_2);

	cfg.rx_cfg1 = cpu_to_le16(adev->rx_config_1);
	cfg.rx_cfg2 = cpu_to_le16(adev->rx_config_2);
	res = acx_configure(adev, &cfg, ACX1xx_IE_RXCONFIG);


	return res;
}

int acx_set_mode(acx_device_t *adev, u16 mode)
{
	adev->mode = mode;
	return acx_update_mode(adev);
}

int acx_update_mode(acx_device_t *adev)
{
	int res = 0;


	log(L_INIT, "Updating to mode=0x%04x\n", adev->mode);

	switch (adev->mode) {
	case ACX_MODE_2_STA:
	case ACX_MODE_0_ADHOC:
	case ACX_MODE_3_AP:
		res += acx_update_rx_config(adev);

		acx1xx_set_rx_enable(adev, 1);
		acx1xx_set_tx_enable(adev, 1);

		break;
	case ACX_MODE_MONITOR:
		break;
	case ACX_MODE_OFF:
		res += acx1xx_set_tx_enable(adev, 0);
		res += acx1xx_set_rx_enable(adev, 0);
		break;

	default:
		logf1(L_INIT, "Error: Undefined mode=0x%04x\n",
				adev->mode);
		return NOT_OK;
	}


	return res ? NOT_OK : OK;
}

void acx_set_defaults(acx_device_t *adev)
{
	struct eeprom_cfg *acfg = &adev->cfgopt;


	/* do it before getting settings, prevent bogus channel 0 warning */
	adev->channel = 1;

	/* query some settings from the card.
	 * NOTE: for some settings, e.g. CCA and ED (ACX100!), an initial
	 * query is REQUIRED, otherwise the card won't work correctly! */

	acx1xx_get_station_id(adev);
	SET_IEEE80211_PERM_ADDR(adev->ieee, adev->dev_addr);

	MAC_COPY(adev->bssid, adev->dev_addr);

	acx1xx_get_antenna(adev);

	acx_get_reg_domain(adev);

	/* Only ACX100 supports ED and CCA */
	if (IS_ACX100(adev)) {
		acx1xx_get_cca(adev);
		acx1xx_get_ed_threshold(adev);
	}

	acx_get_sensitivity(adev);

	/* set our global interrupt mask */
	if (IS_PCI(adev) || IS_MEM(adev))
		acx_set_interrupt_mask(adev);

	adev->led_power = 1;	/* LED is active on startup */
	adev->brange_max_quality = 60;	/* LED blink max quality is 60 */
	adev->brange_time_last_state_change = jiffies;

	adev->essid_len =
		snprintf(adev->essid, sizeof(adev->essid), "ACXSTA%02X%02X%02X",
			adev->dev_addr[3], adev->dev_addr[4], adev->dev_addr[5]);
	adev->essid_active = 1;

	/* we have a nick field to waste, so why not abuse it
	 * to announce the driver version? ;-) */
	strncpy(adev->nick, "acx " ACX_RELEASE, IW_ESSID_MAX_SIZE);

	if (IS_PCI(adev)) {	/* FIXME: this should be made to apply to USB, too! */
		/* first regulatory domain entry in EEPROM == default
		 * reg. domain */
		adev->reg_dom_id = acfg->domains.list[0];
	} else if(IS_MEM(adev)){
		/* first regulatory domain entry in EEPROM == default
		 * reg. domain */
		adev->reg_dom_id = acfg->domains.list[0];
	}

	/* 0xffff would be better, but then we won't get a "scan
	 * complete" interrupt, so our current infrastructure will
	 * fail: */
	adev->scan_count = 1;
	adev->scan_mode = ACX_SCAN_OPT_ACTIVE;
	adev->scan_duration = 100;
	adev->scan_probe_delay = 200;
	/* reported to break scanning: adev->scan_probe_delay =
	 * acfg->probe_delay; */
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
	if (IS_ACX111(adev))
		adev->rate_oper = RATE111_ALL;
	else
		adev->rate_oper = RATE111_ACX100_COMPAT;

	/* Supported Rates element - the rates here are given in units
	 * of 500 kbit/s, plus 0x80 added. See 802.11-1999.pdf item
	 * 7.3.2.2 */
	acx_update_ratevector(adev);

	/* Get current tx-power setting */
	acx1xx_get_tx_level(adev);

	/* Sensitivity settings */
	if (IS_ACX111(adev))
		/* start with sensitivity level 2 out of 3: */
		adev->sensitivity = 2;

	/* Enable hw-encryption (normally by default enabled) */
	acx_set_hw_encryption_on(adev);

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

}

#if 0
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
