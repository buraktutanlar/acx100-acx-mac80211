/* include/ihw.h
 *
 * --------------------------------------------------------------------
 *
 * Copyright (C) 2003  ACX100 Open Source Project
 *
 *   The contents of this file are subject to the Mozilla Public
 *   License Version 1.1 (the "License"); you may not use this file
 *   except in compliance with the License. You may obtain a copy of
 *   the License at http://www.mozilla.org/MPL/
 *
 *   Software distributed under the License is distributed on an "AS
 *   IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 *   implied. See the License for the specific language governing
 *   rights and limitations under the License.
 *
 *   Alternatively, the contents of this file may be used under the
 *   terms of the GNU Public License version 2 (the "GPL"), in which
 *   case the provisions of the GPL are applicable instead of the
 *   above.  If you wish to allow the use of your version of this file
 *   only under the terms of the GPL and not to allow others to use
 *   your version of this file under the MPL, indicate your decision
 *   by deleting the provisions above and replace them with the notice
 *   and other provisions required by the GPL.  If you do not delete
 *   the provisions above, a recipient may use your version of this
 *   file under either the MPL or the GPL.
 *
 * --------------------------------------------------------------------
 *
 * Inquiries regarding the ACX100 Open Source Project can be
 * made directly to:
 *
 * acx100-users@lists.sf.net
 * http://acx100.sf.net
 *
 * --------------------------------------------------------------------
 */
#ifndef __ACX_IHW_H
#define __ACX_IHW_H

typedef struct cmd {
	u16 vala[0xc];
	u32 valb;
	u16 nul;
	u16 valc;
} cmd_t;

typedef struct mac {
	u32 vala;
	u16 valb;
} mac_t;

#if (WLAN_HOSTIF!=WLAN_USB) /* must be used for non-USB only */
#define INLINE_IO static inline /* undefine for out-of-line */
#include <acx_ioreg.h>
#endif /* (WLAN_HOSTIF!=WLAN_USB) */

#define ACX_CMD_TIMEOUT_DEFAULT	5000

void acx_get_info_state(wlandevice_t *priv);
void acx_get_cmd_state(wlandevice_t *priv);
void acx_write_cmd_type_or_status(wlandevice_t *priv, u32 val, unsigned int is_status);

int acx_issue_cmd(wlandevice_t *priv, unsigned int cmd, /*@null@*/ void *pcmdparam,
		     unsigned int paramlen, u32 timeout);

int acx_configure(wlandevice_t *priv, void *pdr, short type);
int acx_configure_length(wlandevice_t *priv, void *pdr, short type,
			    short length);
int acx_interrogate(wlandevice_t *priv, void *pdr, short type);

void acx_clear_mac_address(const mac_t *mac);
unsigned int acx_is_mac_address_zero(const mac_t *mac);
unsigned int acx_is_mac_address_equal(const u8 *one, const u8 *two);
unsigned int acx_is_mac_address_group(const mac_t *mac);
unsigned int acx_is_mac_address_directed(const mac_t *mac);
void acx_set_mac_address_broadcast(u8 *address);
unsigned int acx_is_mac_address_broadcast(const u8 *address);
unsigned int acx_is_mac_address_multicast(const mac_t *mac);
void acx_log_mac_address(int level, const u8 *mac, const char *tail);

void acx_power_led(wlandevice_t *priv, u8 enable);
#endif /* __ACX_IHW_H */
