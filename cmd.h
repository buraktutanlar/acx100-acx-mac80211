#ifndef _ACX_CMD_H_
#define _ACX_CMD_H_

#include "acx.h"

int acx_configure_debug(acx_device_t *adev, void *pdr, int type, const char *typestr);
#define acx_configure(adev,pdr,type) \
	acx_configure_debug(adev,pdr,type,#type)

int acx_issue_cmd_timeo_debug(acx_device_t * adev, unsigned cmd,
		void *buffer, unsigned buflen, unsigned cmd_timeout,
		const char *cmdstr);
#define acx_issue_cmd(adev,cmd,param,len) \
	acx_issue_cmd_timeo_debug(adev,cmd,param,len,ACX_CMD_TIMEOUT_DEFAULT,#cmd)
#define acx_issue_cmd_timeo(adev,cmd,param,len,timeo) \
	acx_issue_cmd_timeo_debug(adev,cmd,param,len,timeo,#cmd)

int acx_interrogate_debug(acx_device_t *adev, void *pdr,
			int type, const char* str);
#define acx_interrogate(adev,pdr,type) \
	acx_interrogate_debug(adev,pdr,type,#type)

#endif
