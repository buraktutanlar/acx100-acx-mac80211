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
	UINT16 vala[0xc];
	UINT valb;
	UINT16 nul;
	UINT16 valc;
} cmd_t;

typedef struct mac {
	UINT32 vala;
	UINT16 valb;
} mac_t;

#define IO_AS_MACROS
#ifdef IO_AS_MACROS
#if ACX_IO_WIDTH == 32
#define acx100_read_reg32(priv, offset) \
	readl(priv->iobase + offset)
#define acx100_write_reg32(priv, offset, val) \
	writel(val, priv->iobase + offset)
#else
#define acx100_read_reg32(priv, offset) \
	readw(priv->iobase + offset) \
	+ (readw(priv->iobase + offset + 2) << 16)
#define acx100_write_reg32(priv, offset, val) \
	do { \
		writew((val) & 0xffff, priv->iobase + offset); \
		writew((val) >> 16, priv->iobase + offset + 2); \
	} while (0)
#endif
#define acx100_read_reg16(priv, offset) \
	readw(priv->iobase + offset)
#define acx100_write_reg16(priv, offset, val) \
	writew(val, priv->iobase + offset)
#define acx100_read_reg8(priv, offset) \
	readb(priv->iobase + offset)
#define acx100_write_reg8(priv, offset, val) \
	writeb(val, priv->iobase + offset)
#else
UINT32 acx100_read_reg32(wlandevice_t *priv, UINT valb);
void acx100_write_reg32(wlandevice_t *priv, UINT vala, UINT valb);
UINT16 acx100_read_reg16(wlandevice_t *priv, UINT valb);
void acx100_write_reg16(wlandevice_t *priv, UINT vala, UINT16 valb);
UINT8 acx100_read_reg8(wlandevice_t *priv, UINT valb);
void acx100_write_reg8(wlandevice_t *priv, UINT vala, UINT valb);
#endif

void acx100_get_info_state(wlandevice_t *priv);
void acx100_get_cmd_state(wlandevice_t *priv);
void acx100_write_cmd_type_or_status(wlandevice_t *priv, UINT val, INT is_status);

int acx100_issue_cmd(wlandevice_t *priv, UINT cmd, /*@null@*/ void *pcmdparam,
		     int paramlen, UINT32 timeout);

int acx100_configure(wlandevice_t *priv, void *pdr, short type);
int acx100_configure_length(wlandevice_t *priv, void *pdr, short type,
			    short length);
int acx100_interrogate(wlandevice_t *priv, void *pdr, short type);

int acx100_is_mac_address_zero(mac_t *mac);
void acx100_clear_mac_address(mac_t *mac);
int acx100_is_mac_address_equal(UINT8 *one, UINT8 *two);
UINT8 acx100_is_mac_address_group(mac_t *mac);
UINT8 acx100_is_mac_address_directed(mac_t *mac);
void acx100_set_mac_address_broadcast(UINT8 *address);
int acx100_is_mac_address_broadcast(const UINT8 * const address);
int acx100_is_mac_address_multicast(mac_t *mac);
void acx100_log_mac_address(int level, UINT8 *mac);

void acx100_power_led(wlandevice_t *priv, UINT8 enable);
#endif /* __ACX_IHW_H */
