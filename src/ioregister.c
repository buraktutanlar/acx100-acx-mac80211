/* src/ioregisters.c
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


/*================================================================*/
/* System Includes */
#include <linux/config.h>
#include <linux/version.h>

#include <linux/if_arp.h>
#include <linux/wireless.h>
#if WIRELESS_EXT >= 13
#include <net/iw_handler.h>
#endif

/*================================================================*/
/* Project Includes */

#include <acx.h>

/* we re-decided to go with statically created arrays,
 * since malloc'ing them and then inserting values would probably take away
 * an awful lot more space than simply always predefining for both
 * ACX100 and ACX111... */

const u16 IO_ACX100[] =
{
	0x0000, /* IO_ACX_SOFT_RESET */
	
	0x0014, /* IO_ACX_SLV_MEM_ADDR */
	0x0018, /* IO_ACX_SLV_MEM_DATA */
	0x001c, /* IO_ACX_SLV_MEM_CTL */
	0x0020, /* IO_ACX_SLV_END_CTL */

	0x0034, /* IO_ACX_FEMR */

	0x007c, /* IO_ACX_INT_TRIG */
	0x0098, /* IO_ACX_IRQ_MASK */
	0x00a4, /* IO_ACX_IRQ_STATUS_NON_DES */
	0x00a8, /* IO_ACX_IRQ_STATUS_CLEAR */
	0x00ac, /* IO_ACX_IRQ_ACK */
	0x00b0, /* IO_ACX_HINT_TRIG */

	0x0104, /* IO_ACX_ENABLE */

	0x0250, /* IO_ACX_EEPROM_CTL */
	0x0254, /* IO_ACX_EEPROM_ADDR */
	0x0258, /* IO_ACX_EEPROM_DATA */
	0x025c, /* IO_ACX_EEPROM_CFG */

	0x0268, /* IO_ACX_PHY_ADDR */
	0x026c, /* IO_ACX_PHY_DATA */
	0x0270, /* IO_ACX_PHY_CTL */

	0x0290, /* IO_ACX_GPIO_OE */

	0x0298, /* IO_ACX_GPIO_OUT */

	0x02a4, /* IO_ACX_CMD_MAILBOX_OFFS */
	0x02a8, /* IO_ACX_INFO_MAILBOX_OFFS */
	0x02ac, /* IO_ACX_EEPROM_INFORMATION */

	0x02d0, /* IO_ACX_EE_START */
	0x02d4, /* IO_ACX_SOR_CFG */
	0x02d8 /* IO_ACX_ECPU_CTRL */
};

const u16 IO_ACX111[] =
{
	0x0000, /* IO_ACX_SOFT_RESET */

	0x0014, /* IO_ACX_SLV_MEM_ADDR */
	0x0018, /* IO_ACX_SLV_MEM_DATA */
	0x001c, /* IO_ACX_SLV_MEM_CTL */
	0x0020, /* IO_ACX_SLV_END_CTL */

	0x0034, /* IO_ACX_FEMR */

	0x00b4, /* IO_ACX_INT_TRIG */
	0x00d4, /* IO_ACX_IRQ_MASK */
	0x00f0, /* IO_ACX_IRQ_STATUS_NON_DES */ /* we need NON_DES, not NON_DES_MASK at 0xe0 */
	0x00e4, /* IO_ACX_IRQ_STATUS_CLEAR */
	0x00e8, /* IO_ACX_IRQ_ACK */
	0x00ec, /* IO_ACX_HINT_TRIG */

	0x01d0, /* IO_ACX_ENABLE */

	0x0338, /* IO_ACX_EEPROM_CTL */
	0x033c, /* IO_ACX_EEPROM_ADDR */
	0x0340, /* IO_ACX_EEPROM_DATA */
	0x0344, /* IO_ACX_EEPROM_CFG */

	0x0350, /* IO_ACX_PHY_ADDR */
	0x0354, /* IO_ACX_PHY_DATA */
	0x0358, /* IO_ACX_PHY_CTL */
	
	0x0374, /* IO_ACX_GPIO_OE */

	0x037c, /* IO_ACX_GPIO_OUT */

	0x0388, /* IO_ACX_CMD_MAILBOX_OFFS */
	0x038c, /* IO_ACX_INFO_MAILBOX_OFFS */
	0x0390, /* IO_ACX_EEPROM_INFORMATION */

	0x0100, /* IO_ACX_EE_START */
	0x0104, /* IO_ACX_SOR_CFG */
	0x0108, /* IO_ACX_ECPU_CTRL */
};

void acx_select_io_register_set(wlandevice_t *priv, u16 chip_type)
{
	/* set the correct io resource list for the active chip */
	if (CHIPTYPE_ACX100 == chip_type) {
		priv->io = IO_ACX100;
	} else if (CHIPTYPE_ACX111 == chip_type) {
		priv->io = IO_ACX111;
	}
	acxlog(L_STD, "%s: using %s io resource addresses (size: %d)\n", __func__, priv->chip_name, IO_INDICES_SIZE);
}
