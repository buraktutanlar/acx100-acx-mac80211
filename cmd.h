#ifndef _ACX_CMD_H_
#define _ACX_CMD_H_

#include "acx.h"

int acx_configure_debug(acx_device_t *adev, void *pdr, int type, const char *typestr);
#define acx_configure(adev,pdr,type) \
	acx_configure_debug(adev,pdr,type,#type)
enum acx_cmd {
	ACX1xx_CMD_RESET,
	ACX1xx_CMD_INTERROGATE,
	ACX1xx_CMD_CONFIGURE,
	ACX1xx_CMD_ENABLE_RX,
	ACX1xx_CMD_ENABLE_TX,
	ACX1xx_CMD_DISABLE_RX,
	ACX1xx_CMD_DISABLE_TX,
	ACX1xx_CMD_FLUSH_QUEUE,
	ACX1xx_CMD_SCAN,
	ACX1xx_CMD_STOP_SCAN,
	ACX1xx_CMD_CONFIG_TIM,
	ACX1xx_CMD_JOIN,
	ACX1xx_CMD_WEP_MGMT,
	ACX100_CMD_HALT,
	ACX1xx_CMD_MEM_READ,
	ACX1xx_CMD_MEM_WRITE,
	ACX1xx_CMD_SLEEP,
	ACX1xx_CMD_WAKE,
	ACX1xx_CMD_UNKNOWN_11,
	ACX100_CMD_INIT_MEMORY,
	ACX1FF_CMD_DISABLE_RADIO,
	ACX1xx_CMD_CONFIG_BEACON,
	ACX1xx_CMD_CONFIG_PROBE_RESPONSE,
	ACX1xx_CMD_CONFIG_NULL_DATA,
	ACX1xx_CMD_CONFIG_PROBE_REQUEST,
	ACX1xx_CMD_FCC_TEST,
	ACX1xx_CMD_RADIOINIT,
	ACX111_CMD_RADIOCALIB,
	ACX1FF_CMD_NOISE_HISTOGRAM,
	ACX1FF_CMD_RX_RESET,
	ACX1FF_CMD_LNA_CONTROL,
	ACX1FF_CMD_CONTROL_DBG_TRACE
};

struct acx_cmd_desc {
	unsigned int val;
	const char* name;
};

int acx_interrogate_debug(acx_device_t *adev, void *pdr,
			int type, const char* str);
#define acx_interrogate(adev,pdr,type) \
	acx_interrogate_debug(adev,pdr,type,#type)
extern const struct acx_cmd_desc acx_cmd_descs[];

int acx_issue_cmd_timeo(acx_device_t *adev, enum acx_cmd cmd, void *buffer,
                        unsigned buflen, unsigned cmd_timeout);
int acx_issue_cmd(acx_device_t *adev, enum acx_cmd cmd, void *param, unsigned len);

#endif
