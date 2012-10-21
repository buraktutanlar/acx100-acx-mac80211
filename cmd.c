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

#include "acx.h"
#include "usb.h"
#include "merge.h"
#include "cmd.h"
#include "utils.h"

/* Controller Commands
 * Can be found in the cmdTable table on the "Rev. 1.5.0" (FW150) firmware
 */

#define DEF_CMD(name, val) [name]={val, #name}
const struct acx_cmd_desc acx_cmd_descs[] = {
	DEF_CMD(ACX1xx_CMD_RESET, 		0x00),
        DEF_CMD(ACX1xx_CMD_INTERROGATE, 	0x01),
        DEF_CMD(ACX1xx_CMD_CONFIGURE, 		0x02),
        DEF_CMD(ACX1xx_CMD_ENABLE_RX, 		0x03),
        DEF_CMD(ACX1xx_CMD_ENABLE_TX, 		0x04),
        DEF_CMD(ACX1xx_CMD_DISABLE_RX, 		0x05),
        DEF_CMD(ACX1xx_CMD_DISABLE_TX, 		0x06),
        DEF_CMD(ACX1xx_CMD_FLUSH_QUEUE, 	0x07),
        DEF_CMD(ACX1xx_CMD_SCAN, 		0x08),
        DEF_CMD(ACX1xx_CMD_STOP_SCAN, 		0x09),
        DEF_CMD(ACX1xx_CMD_CONFIG_TIM, 		0x0A),
        DEF_CMD(ACX1xx_CMD_JOIN, 		0x0B),
        DEF_CMD(ACX1xx_CMD_WEP_MGMT, 		0x0C),
        DEF_CMD(ACX100_CMD_HALT, 		0x0E), /* mapped to unknownCMD in FW150 */
        DEF_CMD(ACX1xx_CMD_MEM_READ, 		0x0D),
        DEF_CMD(ACX1xx_CMD_MEM_WRITE, 		0x0E),
        DEF_CMD(ACX1xx_CMD_SLEEP, 		0x0F),
        DEF_CMD(ACX1xx_CMD_WAKE, 		0x10),
        DEF_CMD(ACX1xx_CMD_UNKNOWN_11, 		0x11), /* mapped to unknownCMD in FW150 */
        DEF_CMD(ACX100_CMD_INIT_MEMORY, 	0x12),
        DEF_CMD(ACX1FF_CMD_DISABLE_RADIO, 	0x12), /* new firmware? TNETW1450? + NOT in BSD driver */
        DEF_CMD(ACX1xx_CMD_CONFIG_BEACON, 	0x13),
        DEF_CMD(ACX1xx_CMD_CONFIG_PROBE_RESPONSE,
        					0x14),
        DEF_CMD(ACX1xx_CMD_CONFIG_NULL_DATA, 	0x15),
        DEF_CMD(ACX1xx_CMD_CONFIG_PROBE_REQUEST,
        					0x16),
        DEF_CMD(ACX1xx_CMD_FCC_TEST, 		0x17),
        DEF_CMD(ACX1xx_CMD_RADIOINIT, 		0x18),
        DEF_CMD(ACX111_CMD_RADIOCALIB, 		0x19),
        DEF_CMD(ACX1FF_CMD_NOISE_HISTOGRAM,	0x1c), /* new firmware? TNETW1450? */
        DEF_CMD(ACX1FF_CMD_RX_RESET, 		0x1d), /* new firmware? TNETW1450? */
        DEF_CMD(ACX1FF_CMD_LNA_CONTROL,		0x20), /* new firmware? TNETW1450? */
        DEF_CMD(ACX1FF_CMD_CONTROL_DBG_TRACE, 	0x21), /* new firmware? TNETW1450? */
};

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

int acx_issue_cmd_timeout(acx_device_t *adev, enum acx_cmd cmd, void *param,
		unsigned len, unsigned timeout)
{
	const unsigned int cmdval = acx_cmd_descs[cmd].val;
	const char *cmdstr = acx_cmd_descs[cmd].name;

	if (IS_PCI(adev) || IS_MEM(adev))
		return _acx_issue_cmd_timeo_debug(adev, cmdval, param, len,
						timeout, cmdstr);
	if (IS_USB(adev))
		return acxusb_issue_cmd_timeo_debug(adev, cmdval, param, len,
						timeout, cmdstr);

	log(L_ANY, "Unsupported dev_type=%i\n", (adev)->dev_type);

	return (NOT_OK);
}

inline int acx_issue_cmd(acx_device_t *adev, enum acx_cmd cmd, void *param, unsigned len)
{
	return acx_issue_cmd_timeout(adev, cmd, param, len,
	        ACX_CMD_TIMEOUT_DEFAULT);
}

inline int acx_configure(acx_device_t *adev, void *pdr, enum acx_ie type)
{
	return acx_configure_len(adev, pdr, type, acx_ie_descs[type].len);
}

int acx_configure_len(acx_device_t *adev, void *pdr, enum acx_ie type, u16 len)
{
	int res;
	char msgbuf[255];

	const u16 typeval = acx_ie_descs[type].val;
	const char *typestr = acx_ie_descs[type].name;

	if (unlikely(!len))
		log(L_DEBUG, "zero-length type %s?!\n", typestr);

	((acx_ie_generic_t *) pdr)->type = cpu_to_le16(typeval);
	((acx_ie_generic_t *) pdr)->len = cpu_to_le16(len);
	res = acx_issue_cmd(adev, ACX1xx_CMD_CONFIGURE, pdr, len + 4);

	sprintf(msgbuf, "%s: type=0x%04X, typestr=%s, len=%u",
		wiphy_name(adev->hw->wiphy), typeval, typestr, len);

	if (likely(res == OK))
		log(L_INIT,  "%s: OK\n", msgbuf);
	 else
		log(L_ANY,  "%s: FAILED\n", msgbuf);

	return res;
}

int acx_interrogate(acx_device_t *adev, void *pdr, enum acx_ie type)
{
	int res;

	const u16 typeval = acx_ie_descs[type].val;
	const char *typestr = acx_ie_descs[type].name;
	const u16 len = acx_ie_descs[type].len;




	/* FIXME: no check whether this exceeds the array yet.
	 * We should probably remember the number of entries... */

	log(L_INIT, "(type:%s,len:%u)\n", typestr, len);

	((acx_ie_generic_t *) pdr)->type = cpu_to_le16(typeval);
	((acx_ie_generic_t *) pdr)->len = cpu_to_le16(len);
	res = acx_issue_cmd(adev, ACX1xx_CMD_INTERROGATE, pdr, len + 4);
	if (unlikely(OK != res)) {
#if ACX_DEBUG
		pr_info("%s: (type:%s) FAILED\n",
			wiphy_name(adev->hw->wiphy), typestr);
#else
		pr_info("%s: (type:0x%X) FAILED\n",
			wiphy_name(adev->hw->wiphy), typeval);
#endif
		/* dump_stack() is already done in issue_cmd() */
	}

	return res;
}

/* Looks scary, eh?
** Actually, each one compiled into one AND and one SHIFT,
** 31 bytes in x86 asm (more if uints are replaced by u16/u8) */
static inline unsigned int acx_rate111to5bits(unsigned int rate)
{
	return (rate & 0x7)
	    | ((rate & RATE111_11) / (RATE111_11 / JOINBSS_RATES_11))
	    | ((rate & RATE111_22) / (RATE111_22 / JOINBSS_RATES_22));
}

extern const u8 bitpos2genframe_txrate[];

/*
 * acx_cmd_join_bssid
 *
 * Common code for both acx100 and acx111.
 */
/* NB: does NOT match RATE100_nn but matches ACX[111]_SCAN_RATE_n */
int acx_cmd_join_bssid(acx_device_t *adev, const u8 *bssid)
{
	int res;
        acx_joinbss_t tmp;
        int dtim_interval;
        int i;

        if (mac_is_zero(bssid))
                return OK;



        dtim_interval = (ACX_MODE_0_ADHOC == adev->mode) ?
                        1 : adev->dtim_interval;

        memset(&tmp, 0, sizeof(tmp));

        for (i = 0; i < ETH_ALEN; i++) {
                tmp.bssid[i] = bssid[ETH_ALEN-1 - i];
        }

        tmp.beacon_interval = cpu_to_le16(adev->beacon_interval);

        /* Basic rate set. Control frame responses (such as ACK or CTS
	 * frames) are sent with one of these rates */
        if (IS_ACX111(adev)) {
                /* It was experimentally determined that rates_basic
		 * can take 11g rates as well, not only rates defined
		 * with JOINBSS_RATES_BASIC111_nnn.  Just use
		 * RATE111_nnn constants... */
                tmp.u.acx111.dtim_interval = dtim_interval;
                tmp.u.acx111.rates_basic = cpu_to_le16(adev->rate_basic);
                log(L_ASSOC, "rates_basic:%04X, rates_supported:%04X\n",
                        adev->rate_basic, adev->rate_oper);
        } else {
                tmp.u.acx100.dtim_interval = dtim_interval;
                tmp.u.acx100.rates_basic =
			acx_rate111to5bits(adev->rate_basic);
                tmp.u.acx100.rates_supported =
			acx_rate111to5bits(adev->rate_oper);
                log(L_ASSOC, "rates_basic:%04X->%02X, "
                        "rates_supported:%04X->%02X\n",
                        adev->rate_basic, tmp.u.acx100.rates_basic,
                        adev->rate_oper, tmp.u.acx100.rates_supported);
        }

        /* Setting up how Beacon, Probe Response, RTS, and PS-Poll
	 * frames will be sent (rate/modulation/preamble) */
        tmp.genfrm_txrate = bitpos2genframe_txrate[lowest_bit(adev->rate_basic)];
        tmp.genfrm_mod_pre = 0;
        /* FIXME: was = adev->capab_short (which was always 0); */

        /* we can use short pre *if* all peers can understand it */
        /* FIXME #2: we need to correctly set PBCC/OFDM bits here too */

        /* we switch fw to STA mode in MONITOR mode, it seems to be
	 * the only mode where fw does not emit beacons by itself but
	 * allows us to send anything (we really want to retain
	 * ability to tx arbitrary frames in MONITOR mode)
	 */
        tmp.macmode = (adev->mode != ACX_MODE_MONITOR
		? adev->mode : ACX_MODE_2_STA);
        tmp.channel = adev->channel;
        tmp.essid_len = adev->essid_len;

        memcpy(tmp.essid, adev->essid, tmp.essid_len);
        res = acx_issue_cmd(adev, ACX1xx_CMD_JOIN, &tmp, tmp.essid_len + 0x11);

        log(L_ASSOC|L_DEBUG, "BSS_Type = %u\n", tmp.macmode);
        acxlog_mac(L_ASSOC|L_DEBUG, "JoinBSSID MAC:", adev->bssid, "\n");

	/* acx_update_capabilities(adev); */

        return res;
}

int acx_cmd_scan(acx_device_t *adev)
{
	int res;

        union {
                acx111_scan_t acx111;
                acx100_scan_t acx100;
        } s;

	res = acx_issue_cmd(adev, ACX1xx_CMD_STOP_SCAN, NULL, 0);

        memset(&s, 0, sizeof(s));
        /* first common positions... */

        s.acx111.count = cpu_to_le16(adev->scan_count);
        s.acx111.rate = adev->scan_rate;
        s.acx111.options = adev->scan_mode;
        s.acx111.chan_duration = cpu_to_le16(adev->scan_duration);
        s.acx111.max_probe_delay = cpu_to_le16(adev->scan_probe_delay);

        /* ...then differences */

        if (IS_ACX111(adev)) {
                s.acx111.channel_list_select = 0; /* scan every allowed channel */
                /*s.acx111.channel_list_select = 1;*/ /* scan given channels */
                /*s.acx111.modulation = 0x40;*/ /* long preamble? OFDM? -> only for active scan */
                s.acx111.modulation = 0;
                /*s.acx111.channel_list[0] = 6;
                s.acx111.channel_list[1] = 4;*/
        } else {
                s.acx100.start_chan = cpu_to_le16(1);
                s.acx100.flags = cpu_to_le16(0x8000);
        }

        res = acx_issue_cmd(adev, ACX1xx_CMD_SCAN, &s, sizeof(s));

        return res;
}
