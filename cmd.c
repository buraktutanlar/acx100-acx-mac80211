#include "acx_debug.h"

#include "acx.h"
#include "usb.h"
#include "merge.h"
#include "cmd.h"

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

int acx_issue_cmd_timeo(acx_device_t *adev, enum acx_cmd cmd, void *param,
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
	return acx_issue_cmd_timeo(adev, cmd, param, len,
	        ACX_CMD_TIMEOUT_DEFAULT);
}

int acx_configure(acx_device_t *adev, void *pdr, enum acx_ie type)
{
	int res;
	char msgbuf[255];

	const u16 typeval = acx_ie_descs[type].val;
	const char *typestr = acx_ie_descs[type].name;
	const u16 len = acx_ie_descs[type].len;

	FN_ENTER;

	if (unlikely(!len))
		log(L_DEBUG, "zero-length type %s?!\n", typestr);

	((acx_ie_generic_t *) pdr)->type = cpu_to_le16(typeval);
	((acx_ie_generic_t *) pdr)->len = cpu_to_le16(len);
	res = acx_issue_cmd(adev, ACX1xx_CMD_CONFIGURE, pdr, len + 4);

	sprintf(msgbuf, "%s: type=0x%04X, typestr=%s, len=%u",
		wiphy_name(adev->ieee->wiphy), typeval, typestr, len);

	if (likely(res == OK))
		log(L_INIT,  "%s: OK\n", msgbuf);
	 else
		log(L_ANY,  "%s: FAILED\n", msgbuf);

	FN_EXIT0;
	return res;
}

int acx_interrogate(acx_device_t *adev, void *pdr, enum acx_ie type)
{
	int res;

	const u16 typeval = acx_ie_descs[type].val;
	const char *typestr = acx_ie_descs[type].name;
	const u16 len = acx_ie_descs[type].len;


	FN_ENTER;

	/* FIXME: no check whether this exceeds the array yet.
	 * We should probably remember the number of entries... */

	log(L_INIT, "(type:%s,len:%u)\n", typestr, len);

	((acx_ie_generic_t *) pdr)->type = cpu_to_le16(typeval);
	((acx_ie_generic_t *) pdr)->len = cpu_to_le16(len);
	res = acx_issue_cmd(adev, ACX1xx_CMD_INTERROGATE, pdr, len + 4);
	if (unlikely(OK != res)) {
#if ACX_DEBUG
		pr_info("%s: (type:%s) FAILED\n",
			wiphy_name(adev->ieee->wiphy), typestr);
#else
		pr_info("%s: (type:0x%X) FAILED\n",
			wiphy_name(adev->ieee->wiphy), typeval);
#endif
		/* dump_stack() is already done in issue_cmd() */
	}
	FN_EXIT1(res);
	return res;
}

