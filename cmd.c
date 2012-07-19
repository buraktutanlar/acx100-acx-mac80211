#include "acx_debug.h"

#include "acx.h"
#include "usb.h"
#include "merge.h"
#include "cmd.h"

int acx_issue_cmd_timeo_debug(acx_device_t *adev, unsigned cmd, void *param,
		unsigned len, unsigned timeout, const char* cmdstr)
{
	if (IS_PCI(adev) || IS_MEM(adev))
		return _acx_issue_cmd_timeo_debug(adev, cmd, param, len,
						timeout, cmdstr);
	if (IS_USB(adev))
		return acxusb_issue_cmd_timeo_debug(adev, cmd, param, len,
						timeout, cmdstr);

	log(L_ANY, "Unsupported dev_type=%i\n", (adev)->dev_type);

	return (NOT_OK);
}

int acx_configure_debug(acx_device_t *adev, void *pdr, int type,
		      const char *typestr)
{
	u16 len;
	int res;
	char msgbuf[255];

	FN_ENTER;

	if (type < 0x1000)
		len = adev->ie_len[type];
	else
		len = adev->ie_len_dot11[type - 0x1000];

	if (unlikely(!len))
		log(L_DEBUG, "zero-length type %s?!\n", typestr);

	((acx_ie_generic_t *) pdr)->type = cpu_to_le16(type);
	((acx_ie_generic_t *) pdr)->len = cpu_to_le16(len);
	res = acx_issue_cmd(adev, ACX1xx_CMD_CONFIGURE, pdr, len + 4);

	sprintf(msgbuf, "%s: type=0x%04X, typestr=%s, len=%u",
		wiphy_name(adev->ieee->wiphy), type, typestr, len);

	if (likely(res == OK))
		log(L_INIT,  "%s: OK\n", msgbuf);
	 else
		log(L_ANY,  "%s: FAILED\n", msgbuf);

	FN_EXIT0;
	return res;
}

int acx_interrogate_debug(acx_device_t *adev, void *pdr, int type,
			const char *typestr)
{
	u16 len;
	int res;

	FN_ENTER;

	/* FIXME: no check whether this exceeds the array yet.
	 * We should probably remember the number of entries... */
	if (type < 0x1000)
		len = adev->ie_len[type];
	else
		len = adev->ie_len_dot11[type - 0x1000];

	log(L_INIT, "(type:%s,len:%u)\n", typestr, len);

	((acx_ie_generic_t *) pdr)->type = cpu_to_le16(type);
	((acx_ie_generic_t *) pdr)->len = cpu_to_le16(len);
	res = acx_issue_cmd(adev, ACX1xx_CMD_INTERROGATE, pdr, len + 4);
	if (unlikely(OK != res)) {
#if ACX_DEBUG
		pr_info("%s: (type:%s) FAILED\n",
			wiphy_name(adev->ieee->wiphy), typestr);
#else
		pr_info("%s: (type:0x%X) FAILED\n",
			wiphy_name(adev->ieee->wiphy), type);
#endif
		/* dump_stack() is already done in issue_cmd() */
	}
	FN_EXIT1(res);
	return res;
}

