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

/*================================================================*/
/* Project Includes */

#include <p80211mgmt.h>
#include <acx100.h>
#include <ioregister.h>

/* static vars */
static UINT16 *pIO_ACX100 = NULL;
static UINT16 *pIO_ACX111 = NULL;

UINT16* acx100_get_io_register_array() 
{
	if(pIO_ACX100 != NULL) return pIO_ACX100;

	pIO_ACX100 = kmalloc(IO_INDICES_SIZE, GFP_KERNEL);
	if(pIO_ACX100 == NULL) {
		/* alloc failure */
		return NULL;
	}

	memset(pIO_ACX100, 0xff, IO_INDICES_SIZE);

	pIO_ACX100[IO_ACX_SOFT_RESET] = (0x0000);

	pIO_ACX100[IO_ACX_SLV_MEM_ADDR] = (0x0014);
	pIO_ACX100[IO_ACX_SLV_MEM_DATA] = (0x0018);
	pIO_ACX100[IO_ACX_SLV_MEM_CTL] = (0x001c);
	pIO_ACX100[IO_ACX_SLV_END_CTL] = (0x0020);

	pIO_ACX100[IO_ACX_FEMR] = (0x0034);

	pIO_ACX100[IO_ACX_INT_TRIG] = (0x007c);
	pIO_ACX100[IO_ACX_IRQ_MASK] = (0x0098);
	pIO_ACX100[IO_ACX_IRQ_STATUS_NON_DES] = (0x00a4);
	pIO_ACX100[IO_ACX_IRQ_STATUS_CLEAR] = (0x00a8);
	pIO_ACX100[IO_ACX_IRQ_ACK] = (0x00ac);
	pIO_ACX100[IO_ACX_HINT_TRIG] = (0x00b0);

	pIO_ACX100[IO_ACX_ENABLE] = (0x0104);

	pIO_ACX100[IO_ACX_EEPROM_CTL] = (0x0250);
	pIO_ACX100[IO_ACX_EEPROM_ADDR] = (0x0254);
	pIO_ACX100[IO_ACX_EEPROM_DATA] = (0x0258);
	pIO_ACX100[IO_ACX_EEPROM_CFG] = (0x025c);

	pIO_ACX100[IO_ACX_PHY_ADDR] = (0x0268);
	pIO_ACX100[IO_ACX_PHY_DATA] = (0x026c);
	pIO_ACX100[IO_ACX_PHY_CTL] = (0x0270);

	pIO_ACX100[IO_ACX_GPIO_OE] = (0x0290);

	pIO_ACX100[IO_ACX_GPIO_OUT] = (0x0298);

	pIO_ACX100[IO_ACX_CMD_MAILBOX_OFFS] = (0x02a4);
	pIO_ACX100[IO_ACX_INFO_MAILBOX_OFFS] = (0x02a8);
	pIO_ACX100[IO_ACX_EEPROM_INFORMATION] = (0x02ac);

	pIO_ACX100[IO_ACX_EE_START] = (0x02d0);
	pIO_ACX100[IO_ACX_SOR_CFG] = (0x02d4);
	pIO_ACX100[IO_ACX_ECPU_CTRL] = (0x02d8);

	return pIO_ACX100;
}

UINT16* acx111_get_io_register_array() 
{
	if(pIO_ACX111 != NULL) return pIO_ACX111;

	pIO_ACX111 = kmalloc(IO_INDICES_SIZE, GFP_KERNEL);
	if(pIO_ACX111 == NULL) {
		/* alloc failure */
		return NULL;
	}

	memset(pIO_ACX111, 0xff, IO_INDICES_SIZE);

	pIO_ACX111[IO_ACX_SOFT_RESET] = (0x0000);

	pIO_ACX111[IO_ACX_SLV_MEM_ADDR] = (0x0014);
	pIO_ACX111[IO_ACX_SLV_MEM_DATA] = (0x0018);
	pIO_ACX111[IO_ACX_SLV_MEM_CTL] = (0x001c);
	pIO_ACX111[IO_ACX_SLV_END_CTL] = (0x0020);

	pIO_ACX111[IO_ACX_FEMR] = (0x0034);

	pIO_ACX111[IO_ACX_INT_TRIG] = (0x00b4);
	pIO_ACX111[IO_ACX_IRQ_MASK] = (0x00d4);
	pIO_ACX111[IO_ACX_IRQ_STATUS_NON_DES] = (0x00e0);
	pIO_ACX111[IO_ACX_IRQ_STATUS_CLEAR] = (0x00e4);
	pIO_ACX111[IO_ACX_IRQ_ACK] = (0x00e8);
	pIO_ACX111[IO_ACX_HINT_TRIG] = (0x00ec);

	pIO_ACX111[IO_ACX_EE_START] = (0x0100);
	pIO_ACX111[IO_ACX_SOR_CFG] = (0x0104);
	pIO_ACX111[IO_ACX_ECPU_CTRL] = (0x0108);

	pIO_ACX111[IO_ACX_ENABLE] = (0x01d0);

	pIO_ACX111[IO_ACX_EEPROM_CTL] = (0x0338);
	pIO_ACX111[IO_ACX_EEPROM_ADDR] = (0x033c);
	pIO_ACX111[IO_ACX_EEPROM_DATA] = (0x0340);
	pIO_ACX111[IO_ACX_EEPROM_CFG] = (0x0344);

	pIO_ACX111[IO_ACX_PHY_ADDR] = (0x0350);
	pIO_ACX111[IO_ACX_PHY_DATA] = (0x0354);
	pIO_ACX111[IO_ACX_PHY_CTL] = (0x0358);
	
	pIO_ACX111[IO_ACX_GPIO_OE] = (0x0374);

	pIO_ACX100[IO_ACX_GPIO_OUT] = (0x037c);

	pIO_ACX111[IO_ACX_CMD_MAILBOX_OFFS] = (0x0388);
	pIO_ACX111[IO_ACX_INFO_MAILBOX_OFFS] = (0x038c);
	pIO_ACX111[IO_ACX_EEPROM_INFORMATION] = (0x0390);

	return pIO_ACX111;
}

void acx_free_io_register_arrays(void)
{
	if (pIO_ACX100) {
		kfree(pIO_ACX100);
		pIO_ACX100 = NULL;
	}
	if (pIO_ACX111) {
		kfree(pIO_ACX111);
		pIO_ACX111 = NULL;
	}
}
