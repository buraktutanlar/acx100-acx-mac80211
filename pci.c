/***********************************************************************
** Copyright (C) 2003  ACX100 Open Source Project
**
** The contents of this file are subject to the Mozilla Public
** License Version 1.1 (the "License"); you may not use this file
** except in compliance with the License. You may obtain a copy of
** the License at http://www.mozilla.org/MPL/
**
** Software distributed under the License is distributed on an "AS
** IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
** implied. See the License for the specific language governing
** rights and limitations under the License.
**
** Alternatively, the contents of this file may be used under the
** terms of the GNU Public License version 2 (the "GPL"), in which
** case the provisions of the GPL are applicable instead of the
** above.  If you wish to allow the use of your version of this file
** only under the terms of the GPL and not to allow others to use
** your version of this file under the MPL, indicate your decision
** by deleting the provisions above and replace them with the notice
** and other provisions required by the GPL.  If you do not delete
** the provisions above, a recipient may use your version of this
** file under either the MPL or the GPL.
** ---------------------------------------------------------------------
** Inquiries regarding the ACX100 Open Source Project can be
** made directly to:
**
** acx100-users@lists.sf.net
** http://acx100.sf.net
** ---------------------------------------------------------------------
*/
#define ACX_PCI 1

#include <linux/config.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 10)
#include <linux/moduleparam.h>
#endif
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/skbuff.h>
#include <linux/slab.h>
#include <linux/if_arp.h>
#include <linux/rtnetlink.h>
#include <linux/wireless.h>
#if WIRELESS_EXT >= 13
#include <net/iw_handler.h>
#endif
#include <linux/netdevice.h>
#include <linux/ioport.h>
#include <linux/pci.h>
#include <linux/pm.h>

#include "acx.h"


/*================================================================*/
/* Local Constants */
#define PCI_TYPE		(PCI_USES_MEM | PCI_ADDR0 | PCI_NO_ACPI_WAKE)
#define PCI_ACX100_REGION1		0x01
#define PCI_ACX100_REGION1_SIZE		0x1000	/* Memory size - 4K bytes */
#define PCI_ACX100_REGION2		0x02
#define PCI_ACX100_REGION2_SIZE		0x10000 /* Memory size - 64K bytes */

#define PCI_ACX111_REGION1		0x00
#define PCI_ACX111_REGION1_SIZE		0x2000	/* Memory size - 8K bytes */
#define PCI_ACX111_REGION2		0x01
#define PCI_ACX111_REGION2_SIZE		0x20000 /* Memory size - 128K bytes */

/* Texas Instruments Vendor ID */
#define PCI_VENDOR_ID_TI		0x104c

/* ACX100 22Mb/s WLAN controller */
#define PCI_DEVICE_ID_TI_TNETW1100A	0x8400
#define PCI_DEVICE_ID_TI_TNETW1100B	0x8401

/* ACX111 54Mb/s WLAN controller */
#define PCI_DEVICE_ID_TI_TNETW1130	0x9066

/* PCI Class & Sub-Class code, Network-'Other controller' */
#define PCI_CLASS_NETWORK_OTHERS	0x280

#define CARD_EEPROM_ID_SIZE 6
#define MAX_IRQLOOPS_PER_JIFFY  (20000/HZ) /* a la orinoco.c */


/***********************************************************************
*/
static void acx_l_disable_irq(wlandevice_t *priv);
static void acx_l_enable_irq(wlandevice_t *priv);
static int acx_e_probe_pci(struct pci_dev *pdev,
			    const struct pci_device_id *id);
static void acx_e_remove_pci(struct pci_dev *pdev);

#ifdef CONFIG_PM
static int acx_e_suspend(struct pci_dev *pdev, pm_message_t state);
static int acx_e_resume(struct pci_dev *pdev);
#endif

static void acx_i_tx_timeout(netdevice_t *dev);
static struct net_device_stats *acx_e_get_stats(netdevice_t *dev);
static struct iw_statistics *acx_e_get_wireless_stats(netdevice_t *dev);

static irqreturn_t acx_i_interrupt(int irq, void *dev_id, struct pt_regs *regs);
static void acx_i_set_multicast_list(netdevice_t *dev);

static int acx_e_open(netdevice_t *dev);
static int acx_e_close(netdevice_t *dev);
static void acx_s_up(netdevice_t *dev);
static void acx_s_down(netdevice_t *dev);


/***********************************************************************
** Register access
*/

/* Pick one */
/* #define INLINE_IO static */
#define INLINE_IO static inline

INLINE_IO u32
acx_read_reg32(wlandevice_t *priv, unsigned int offset)
{
#if ACX_IO_WIDTH == 32
	return readl((u8 *)priv->iobase + priv->io[offset]);
#else
	return readw((u8 *)priv->iobase + priv->io[offset])
	    + (readw((u8 *)priv->iobase + priv->io[offset] + 2) << 16);
#endif
}

INLINE_IO u16
acx_read_reg16(wlandevice_t *priv, unsigned int offset)
{
	return readw((u8 *)priv->iobase + priv->io[offset]);
}

INLINE_IO u8
acx_read_reg8(wlandevice_t *priv, unsigned int offset)
{
	return readb((u8 *)priv->iobase + priv->io[offset]);
}

INLINE_IO void
acx_write_reg32(wlandevice_t *priv, unsigned int offset, u32 val)
{
#if ACX_IO_WIDTH == 32
	writel(val, (u8 *)priv->iobase + priv->io[offset]);
#else
	writew(val & 0xffff, (u8 *)priv->iobase + priv->io[offset]);
	writew(val >> 16, (u8 *)priv->iobase + priv->io[offset] + 2);
#endif
}

INLINE_IO void
acx_write_reg16(wlandevice_t *priv, unsigned int offset, u16 val)
{
	writew(val, (u8 *)priv->iobase + priv->io[offset]);
}

INLINE_IO void
acx_write_reg8(wlandevice_t *priv, unsigned int offset, u8 val)
{
	writeb(val, (u8 *)priv->iobase + priv->io[offset]);
}

/* Handle PCI posting properly:
 * Make sure that writes reach the adapter in case they require to be executed
 * *before* the next write, by reading a random (and safely accessible) register.
 * This call has to be made if there is no read following (which would flush the data
 * to the adapter), yet the written data has to reach the adapter immediately. */
INLINE_IO void
acx_write_flush(wlandevice_t *priv)
{
	/* readb(priv->iobase + priv->io[IO_ACX_INFO_MAILBOX_OFFS]); */
	/* faster version (accesses the first register, IO_ACX_SOFT_RESET,
	 * which should also be safe): */
	readb(priv->iobase);
}


/***********************************************************************
*/
static const char name_acx100[] = "ACX100";
static const char name_tnetw1100a[] = "TNETW1100A";
static const char name_tnetw1100b[] = "TNETW1100B";

static const char name_acx111[] = "ACX111";
static const char name_tnetw1130[] = "TNETW1130";

static const struct pci_device_id
acx_pci_id_tbl[] __devinitdata = {
	{
		.vendor = PCI_VENDOR_ID_TI,
		.device = PCI_DEVICE_ID_TI_TNETW1100A,
		.subvendor = PCI_ANY_ID,
		.subdevice = PCI_ANY_ID,
		.driver_data = CHIPTYPE_ACX100,
	},
	{
		.vendor = PCI_VENDOR_ID_TI,
		.device = PCI_DEVICE_ID_TI_TNETW1100B,
		.subvendor = PCI_ANY_ID,
		.subdevice = PCI_ANY_ID,
		.driver_data = CHIPTYPE_ACX100,
	},
	{
		.vendor = PCI_VENDOR_ID_TI,
		.device = PCI_DEVICE_ID_TI_TNETW1130,
		.subvendor = PCI_ANY_ID,
		.subdevice = PCI_ANY_ID,
		.driver_data = CHIPTYPE_ACX111,
	},
	{
		.vendor = 0,
		.device = 0,
		.subvendor = 0,
		.subdevice = 0,
		.driver_data = 0,
	}
};

MODULE_DEVICE_TABLE(pci, acx_pci_id_tbl);

/* FIXME: checks should be removed once driver is included in the kernel */
#ifndef __devexit_p
#warning *** your kernel is EXTREMELY old since it does not even know about
#warning __devexit_p - this driver could easily FAIL to work, so better
#warning upgrade your kernel! ***
#define __devexit_p(x) x
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 11)
/* pci_name() got introduced at start of 2.6.x,
 * got mandatory (slot_name member removed) in 2.6.11-bk1 */
#define pci_name(x) x->slot_name
#endif

static struct pci_driver acx_pci_drv_id = {
	.name        = "acx_pci",
	.id_table    = acx_pci_id_tbl,
	.probe       = acx_e_probe_pci,
	.remove      = __devexit_p(acx_e_remove_pci),
#ifdef CONFIG_PM
	.suspend     = acx_e_suspend,
	.resume      = acx_e_resume
#endif /* CONFIG_PM */
};

typedef struct acx_device {
	netdevice_t *newest;
} acx_device_t;

/* if this driver was only about PCI devices, then we probably wouldn't
 * need this linked list.
 * But if we want to register ALL kinds of devices in one global list,
 * then we need it and need to maintain it properly. */
static struct acx_device root_acx_dev = {
	.newest		= NULL,
};
DECLARE_MUTEX(root_acx_dev_sem);


/***********************************************************************
*/
static inline txdesc_t*
get_txdesc(wlandevice_t* priv, int index)
{
	return (txdesc_t*) (((u8*)priv->txdesc_start) + index * priv->txdesc_size);
}

static inline txdesc_t*
move_txdesc(wlandevice_t* priv, txdesc_t* txdesc, int inc)
{
	return (txdesc_t*) (((u8*)txdesc) + inc * priv->txdesc_size);
}

static txhostdesc_t*
acx_get_txhostdesc(wlandevice_t* priv, txdesc_t* txdesc)
{
	int index = (u8*)txdesc - (u8*)priv->txdesc_start;
	if (ACX_DEBUG && (index % priv->txdesc_size)) {
		printk("bad txdesc ptr %p\n", txdesc);
		return NULL;
	}
	index /= priv->txdesc_size;
	if (ACX_DEBUG && (index >= TX_CNT)) {
		printk("bad txdesc ptr %p\n", txdesc);
		return NULL;
	}
	return &priv->txhostdesc_start[index*2];
}

static client_t*
acx_get_txc(wlandevice_t* priv, txdesc_t* txdesc)
{
	int index = (u8*)txdesc - (u8*)priv->txdesc_start;
	if (ACX_DEBUG && (index % priv->txdesc_size)) {
		printk("bad txdesc ptr %p\n", txdesc);
		return NULL;
	}
	index /= priv->txdesc_size;
	if (ACX_DEBUG && (index >= TX_CNT)) {
		printk("bad txdesc ptr %p\n", txdesc);
		return NULL;
	}
	return priv->txc[index];
}

static void
acx_put_txc(wlandevice_t* priv, txdesc_t* txdesc, client_t* c)
{
	int index = (u8*)txdesc - (u8*)priv->txdesc_start;
	if (ACX_DEBUG && (index % priv->txdesc_size)) {
		printk("bad txdesc ptr %p\n", txdesc);
		return;
	}
	index /= priv->txdesc_size;
	if (ACX_DEBUG && (index >= TX_CNT)) {
		printk("bad txdesc ptr %p\n", txdesc);
		return;
	}
	priv->txc[index] = c;
}

/***********************************************************************
** EEPROM and PHY read/write helpers
*/
/***********************************************************************
** acx_read_eeprom_offset
**
** Function called to read an octet in the EEPROM.
**
** This function is used by acx_probe_pci to check if the
** connected card is a legal one or not.
**
** Arguments:
**	priv		ptr to wlandevice structure
**	addr		address to read in the EEPROM
**	charbuf		ptr to a char. This is where the read octet
**			will be stored
**
** Returns:
**	zero (0)	- failed
**	one (1)		- success
**
** NOT ADAPTED FOR ACX111!!
*/
int
acx_read_eeprom_offset(wlandevice_t *priv, u32 addr, u8 *charbuf)
{
	int result = NOT_OK;
	int count;

	acx_write_reg32(priv, IO_ACX_EEPROM_CFG, 0);
	acx_write_reg32(priv, IO_ACX_EEPROM_ADDR, addr);
	acx_write_flush(priv);
	acx_write_reg32(priv, IO_ACX_EEPROM_CTL, 2);

	count = 0xffff;
	while (acx_read_reg16(priv, IO_ACX_EEPROM_CTL)) {
		/* scheduling away instead of CPU burning loop
		 * doesn't seem to work here at all:
		 * awful delay, sometimes also failure.
		 * Doesn't matter anyway (only small delay). */
		if (unlikely(!--count)) {
			printk("%s: timeout waiting for EEPROM read\n",
							priv->netdev->name);
			goto fail;
		}
	}

	*charbuf = acx_read_reg8(priv, IO_ACX_EEPROM_DATA);
	acxlog(L_DEBUG, "EEPROM at 0x%04X = 0x%02X\n", addr, *charbuf);
	result = OK;

fail:
	return result;
}


/***********************************************************************
** Dummy EEPROM read? why?!
*/
static int
acx_read_eeprom_area(wlandevice_t *priv)
{
	int offs;
	u8 tmp[0x3b];

	for (offs = 0x8c; offs < 0xb9; offs++) {
		acx_read_eeprom_offset(priv, offs, &tmp[offs - 0x8c]);
	}
	return OK;
}


/***********************************************************************
** We don't lock hw accesses here since we never r/w eeprom in IRQ
** Note: this function sleeps only because of GFP_KERNEL alloc
*/
#ifdef UNUSED
int
acx_s_write_eeprom_offset(wlandevice_t *priv, u32 addr, u32 len, const u8 *charbuf)
{
	u8 *data_verify = NULL;
	unsigned long flags;
	int count, i;
	int result = NOT_OK;
	u16 gpio_orig;

	printk("acx: WARNING! I would write to EEPROM now. "
		"Since I really DON'T want to unless you know "
		"what you're doing (THIS CODE WILL PROBABLY "
		"NOT WORK YET!), I will abort that now. And "
		"definitely make sure to make a "
		"/proc/driver/acx_wlan0_eeprom backup copy first!!! "
		"(the EEPROM content includes the PCI config header!! "
		"If you kill important stuff, then you WILL "
		"get in trouble and people DID get in trouble already)\n");
	return OK;

	FN_ENTER;

	data_verify = kmalloc(len, GFP_KERNEL);
	if (!data_verify) {
		goto end;
	}

	/* first we need to enable the OE (EEPROM Output Enable) GPIO line
	 * to be able to write to the EEPROM.
	 * NOTE: an EEPROM writing success has been reported,
	 * but you probably have to modify GPIO_OUT, too,
	 * and you probably need to activate a different GPIO
	 * line instead! */
	gpio_orig = acx_read_reg16(priv, IO_ACX_GPIO_OE);
	acx_write_reg16(priv, IO_ACX_GPIO_OE, gpio_orig & ~1);
	acx_write_flush(priv);

	/* ok, now start writing the data out */
	for (i = 0; i < len; i++) {
		acx_write_reg32(priv, IO_ACX_EEPROM_CFG, 0);
		acx_write_reg32(priv, IO_ACX_EEPROM_ADDR, addr + i);
		acx_write_reg32(priv, IO_ACX_EEPROM_DATA, *(charbuf + i));
		acx_write_flush(priv);
		acx_write_reg32(priv, IO_ACX_EEPROM_CTL, 1);

		while (acx_read_reg16(priv, IO_ACX_EEPROM_CTL)) {
			if (unlikely(++count > 0xffff)) {
				printk("WARNING, DANGER!!! "
					"Timeout waiting for EEPROM write\n");
				goto end;
			}
		}
	}

	/* disable EEPROM writing */
	acx_write_reg16(priv, IO_ACX_GPIO_OE, gpio_orig);
	acx_write_flush(priv);

	/* now start a verification run */
	count = 0xffff;
	for (i = 0; i < len; i++) {
		acx_write_reg32(priv, IO_ACX_EEPROM_CFG, 0);
		acx_write_reg32(priv, IO_ACX_EEPROM_ADDR, addr + i);
		acx_write_flush(priv);
		acx_write_reg32(priv, IO_ACX_EEPROM_CTL, 2);

		while (acx_read_reg16(priv, IO_ACX_EEPROM_CTL)) {
			if (unlikely(!--count)) {
				printk("timeout waiting for EEPROM read\n");
				goto end;
			}
		}

		data_verify[i] = acx_read_reg16(priv, IO_ACX_EEPROM_DATA);
	}

	if (0 == memcmp(charbuf, data_verify, len))
		result = OK; /* read data matches, success */

end:
	kfree(data_verify);
	FN_EXIT1(result);
	return result;
}
#endif /* UNUSED */


/***********************************************************************
** acxpci_s_read_phy_reg
**
** Messing with rx/tx disabling and enabling here
** (acx_write_reg32(priv, IO_ACX_ENABLE, 0b000000xx)) kills traffic
*/
int
acxpci_s_read_phy_reg(wlandevice_t *priv, u32 reg, u8 *charbuf)
{
	int result = NOT_OK;
	int count;

	FN_ENTER;

	acx_write_reg32(priv, IO_ACX_PHY_ADDR, reg);
	acx_write_flush(priv);
	acx_write_reg32(priv, IO_ACX_PHY_CTL, 2);

	count = 0xffff;
	while (acx_read_reg32(priv, IO_ACX_PHY_CTL)) {
		/* scheduling away instead of CPU burning loop
		 * doesn't seem to work here at all:
		 * awful delay, sometimes also failure.
		 * Doesn't matter anyway (only small delay). */
		if (unlikely(!--count)) {
			printk("%s: timeout waiting for phy read\n",
							priv->netdev->name);
			*charbuf = 0;
			goto fail;
		}
	}

	acxlog(L_DEBUG, "count was %u\n", count);
	*charbuf = acx_read_reg8(priv, IO_ACX_PHY_DATA);

	acxlog(L_DEBUG, "radio PHY at 0x%04X = 0x%02X\n", *charbuf, reg);
	result = OK;
	goto fail; /* silence compiler warning */
fail:
	FN_EXIT1(result);
	return result;
}


/***********************************************************************
*/
int
acxpci_s_write_phy_reg(wlandevice_t *priv, u32 reg, u8 value)
{
	FN_ENTER;

	/* FIXME: we didn't use 32bit access here since mprusko said that
	 * it results in distorted sensitivity on his card (huh!?!?
	 * doesn't happen with my setup...)
	 * But with the access reordering and flushing it
	 * shouldn't happen any more...
	 * FIXME: which radio is in the problematic card? My working one
	 * is 0x11 */
	acx_write_reg32(priv, IO_ACX_PHY_DATA, value);
	acx_write_reg32(priv, IO_ACX_PHY_ADDR, reg);
	acx_write_flush(priv);
	acx_write_reg32(priv, IO_ACX_PHY_CTL, 1);
	acx_write_flush(priv);
	acxlog(L_DEBUG, "radio PHY write 0x%02X at 0x%04X\n", value, reg);

	FN_EXIT1(OK);
	return OK;
}


#define NO_AUTO_INCREMENT	1

/***********************************************************************
** acx_s_write_fw
**
** Write the firmware image into the card.
**
** Arguments:
**	priv		wlan device structure
**	apfw_image	firmware image.
**
** Returns:
**	1	firmware image corrupted
**	0	success
*/
static int
acx_s_write_fw(wlandevice_t *priv, const firmware_image_t *apfw_image, u32 offset)
{
	int len, size;
	u32 sum, v32;
	/* we skip the first four bytes which contain the control sum */
	const u8 *image = (u8*)apfw_image + 4;

	/* start the image checksum by adding the image size value */
	sum = image[0]+image[1]+image[2]+image[3];
	image += 4;

	acx_write_reg32(priv, IO_ACX_SLV_END_CTL, 0);

#if NO_AUTO_INCREMENT
	acxlog(L_INIT, "not using auto increment for firmware loading\n");
	acx_write_reg32(priv, IO_ACX_SLV_MEM_CTL, 0); /* use basic mode */
#else
	acx_write_reg32(priv, IO_ACX_SLV_MEM_CTL, 1); /* use autoincrement mode */
	acx_write_reg32(priv, IO_ACX_SLV_MEM_ADDR, offset); /* configure start address */
	acx_write_flush(priv);
#endif

	len = 0;
	size = le32_to_cpu(apfw_image->size) & (~3);

	while (likely(len < size)) {
		v32 = be32_to_cpu(*(u32*)image);
		sum += image[0]+image[1]+image[2]+image[3];
		image += 4;
		len += 4;

#if NO_AUTO_INCREMENT
		acx_write_reg32(priv, IO_ACX_SLV_MEM_ADDR, offset + len - 4);
		acx_write_flush(priv);
#endif
		acx_write_reg32(priv, IO_ACX_SLV_MEM_DATA, v32);
	}

	acxlog(L_DEBUG, "%s: firmware written\n", __func__);

	/* compare our checksum with the stored image checksum */
	return (sum != le32_to_cpu(apfw_image->chksum));
}


/***********************************************************************
** acx_s_validate_fw
**
** Compare the firmware image given with
** the firmware image written into the card.
**
** Arguments:
**	priv		wlan device structure
**   apfw_image  firmware image.
**
** Returns:
**	NOT_OK	firmware image corrupted or not correctly written
**	OK	success
*/
static int
acx_s_validate_fw(wlandevice_t *priv, const firmware_image_t *apfw_image,
				u32 offset)
{
	u32 v32, w32, sum;
	int len, size;
	int result = OK;
	/* we skip the first four bytes which contain the control sum */
	const u8 *image = (u8*)apfw_image + 4;

	/* start the image checksum by adding the image size value */
	sum = image[0]+image[1]+image[2]+image[3];
	image += 4;

	acx_write_reg32(priv, IO_ACX_SLV_END_CTL, 0);

#if NO_AUTO_INCREMENT
	acx_write_reg32(priv, IO_ACX_SLV_MEM_CTL, 0); /* use basic mode */
#else
	acx_write_reg32(priv, IO_ACX_SLV_MEM_CTL, 1); /* use autoincrement mode */
	acx_write_reg32(priv, IO_ACX_SLV_MEM_ADDR, offset); /* configure start address */
#endif

	len = 0;
	size = le32_to_cpu(apfw_image->size) & (~3);

	while (likely(len < size)) {
		v32 = be32_to_cpu(*(u32*)image);
		image += 4;
		len += 4;

#if NO_AUTO_INCREMENT
		acx_write_reg32(priv, IO_ACX_SLV_MEM_ADDR, offset + len - 4);
#endif
		w32 = acx_read_reg32(priv, IO_ACX_SLV_MEM_DATA);

		if (unlikely(w32 != v32)) {
			printk("acx: FATAL: firmware upload: "
			"data parts at offset %d don't match (0x%08X vs. 0x%08X)! "
			"I/O timing issues or defective memory, with DWL-xx0+? "
			"ACX_IO_WIDTH=16 may help. Please report\n",
				len, v32, w32);
			result = NOT_OK;
			break;
		}

		sum += (u8)w32 + (u8)(w32>>8) + (u8)(w32>>16) + (u8)(w32>>24);
	}

	/* sum control verification */
	if (result != NOT_OK) {
		if (sum != le32_to_cpu(apfw_image->chksum)) {
			printk("acx: FATAL: firmware upload: "
				"checksums don't match!\n");
			result = NOT_OK;
		}
	}

	return result;
}


/***********************************************************************
** acx_s_upload_fw
**
** Arguments:
**	wlandevice: private device that contains card device
** Returns:
**	NOT_OK: failed
**	OK: success
** Call context:
**	acx_reset_dev
*/
static int
acx_s_upload_fw(wlandevice_t *priv)
{
	firmware_image_t *apfw_image = NULL;
	int res = NOT_OK;
	int try;
	u32 size;
	char filename[sizeof("tiacx1NNcNN")];

	FN_ENTER;

	/* Try combined, then main image */
	priv->need_radio_fw = 0;
	sprintf(filename, "tiacx1%02dc%02X",
		IS_ACX111(priv)*11, priv->radio_type);

	apfw_image = acx_s_read_fw(&priv->pdev->dev, filename, &size);
	if (!apfw_image) {
		priv->need_radio_fw = 1;
		filename[sizeof("tiacx1NN")-1] = '\0';
		apfw_image = acx_s_read_fw(&priv->pdev->dev, filename, &size);
		if (!apfw_image) {
			FN_EXIT1(NOT_OK);
			return NOT_OK;
		}
	}

	for (try = 1; try <= 5; try++) {
		res = acx_s_write_fw(priv, apfw_image, 0);
		acxlog(L_DEBUG|L_INIT, "acx_write_fw (main/combined):%d\n", res);
		if (OK == res) {
			res = acx_s_validate_fw(priv, apfw_image, 0);
			acxlog(L_DEBUG|L_INIT, "acx_validate_fw "
					"(main/combined):%d\n", res);
		}

		if (OK == res) {
			SET_BIT(priv->dev_state_mask, ACX_STATE_FW_LOADED);
			break;
		}
		printk("acx: firmware upload attempt #%d FAILED, "
			"retrying...\n", try);
		acx_s_msleep(1000); /* better wait for a while... */
	}

	vfree(apfw_image);

	FN_EXIT1(res);
	return res;
}


/***********************************************************************
** acx_s_upload_radio
**
** Uploads the appropriate radio module firmware
** into the card.
*/
int
acx_s_upload_radio(wlandevice_t *priv)
{
	acx_ie_memmap_t mm;
	firmware_image_t *radio_image = NULL;
	acx_cmd_radioinit_t radioinit;
	int res = NOT_OK;
	int try;
	u32 offset;
	u32 size;
	char filename[sizeof("tiacx1NNrNN")];

	if (!priv->need_radio_fw) return OK;

	FN_ENTER;

	acx_s_interrogate(priv, &mm, ACX1xx_IE_MEMORY_MAP);
	offset = le32_to_cpu(mm.CodeEnd);

	sprintf(filename, "tiacx1%02dr%02X",
		IS_ACX111(priv)*11,
		priv->radio_type);
	radio_image = acx_s_read_fw(&priv->pdev->dev, filename, &size);
	if (!radio_image) {
		printk("acx: can't load radio module '%s'\n", filename);
		goto fail;
	}

	acx_s_issue_cmd(priv, ACX1xx_CMD_SLEEP, NULL, 0);

	for (try = 1; try <= 5; try++) {
		res = acx_s_write_fw(priv, radio_image, offset);
		acxlog(L_DEBUG|L_INIT, "acx_write_fw (radio): %d\n", res);
		if (OK == res) {
			res = acx_s_validate_fw(priv, radio_image, offset);
			acxlog(L_DEBUG|L_INIT, "acx_validate_fw (radio): %d\n", res);
		}

		if (OK == res)
			break;
		printk("acx: radio firmware upload attempt #%d FAILED, "
			"retrying...\n", try);
		acx_s_msleep(1000); /* better wait for a while... */
	}

	acx_s_issue_cmd(priv, ACX1xx_CMD_WAKE, NULL, 0);
	radioinit.offset = cpu_to_le32(offset);
	/* no endian conversion needed, remains in card CPU area: */
	radioinit.len = radio_image->size;

	vfree(radio_image);

	if (OK != res)
		goto fail;

	/* will take a moment so let's have a big timeout */
	acx_s_issue_cmd_timeo(priv, ACX1xx_CMD_RADIOINIT,
		&radioinit, sizeof(radioinit), CMD_TIMEOUT_MS(1000));

	res = acx_s_interrogate(priv, &mm, ACX1xx_IE_MEMORY_MAP);
fail:
	FN_EXIT1(res);
	return res;
}


/***********************************************************************
** acx_l_reset_mac
**
** Arguments:
**	wlandevice: private device that contains card device
** Side effects:
**	MAC will be reset
** Call context:
**	acx_reset_dev
** Comment:
**	resets onboard acx100 MAC
**
** Requires lock to be taken
*/
static void
acx_l_reset_mac(wlandevice_t *priv)
{
	u16 temp;

	FN_ENTER;

	/* halt eCPU */
	temp = acx_read_reg16(priv, IO_ACX_ECPU_CTRL) | 0x1;
	acx_write_reg16(priv, IO_ACX_ECPU_CTRL, temp);

	/* now do soft reset of eCPU */
	temp = acx_read_reg16(priv, IO_ACX_SOFT_RESET) | 0x1;
	acxlog(L_DEBUG, "%s: enable soft reset...\n", __func__);
	acx_write_reg16(priv, IO_ACX_SOFT_RESET, temp);
	acx_write_flush(priv);

	/* now reset bit again */
	acxlog(L_DEBUG, "%s: disable soft reset and go to init mode...\n", __func__);
	/* deassert eCPU reset */
	acx_write_reg16(priv, IO_ACX_SOFT_RESET, temp & ~0x1);

	/* now start a burst read from initial flash EEPROM */
	temp = acx_read_reg16(priv, IO_ACX_EE_START) | 0x1;
	acx_write_reg16(priv, IO_ACX_EE_START, temp);
	acx_write_flush(priv);

	FN_EXIT0;
}


/***********************************************************************
** acx_s_verify_init
*/
static int
acx_s_verify_init(wlandevice_t *priv)
{
	int result = NOT_OK;
	int timer;

	FN_ENTER;

	for (timer = 40; timer > 0; timer--) {
		u16 irqstat = acx_read_reg16(priv, IO_ACX_IRQ_STATUS_NON_DES);
		if (irqstat & HOST_INT_FCS_THRESHOLD) {
			result = OK;
			acx_write_reg16(priv, IO_ACX_IRQ_ACK, HOST_INT_FCS_THRESHOLD);
			break;
		}
		/* HZ / 50 resulted in 24 schedules for ACX100 on my machine,
		 * so better schedule away longer for greater efficiency,
		 * decrease loop count */
		acx_s_msleep(50);
	}

	FN_EXIT1(result);
	return result;
}


/***********************************************************************
** A few low-level helpers
**
** Note: these functions are not protected by lock
** and thus are never allowed to be called from IRQ.
** Also they must not race with fw upload which uses same hw regs
*/

/***********************************************************************
** acx_read_info_status
*/
/* Info mailbox format:
2 bytes: type
2 bytes: status
more bytes may follow
    docs say about status:
	0x0000 info available (set by hw)
	0x0001 information received (must be set by host)
	0x1000 info available, mailbox overflowed (messages lost) (set by hw)
    but in practice we've seen:
	0x9000 when we did not set status to 0x0001 on prev message
	0x1001 when we did set it
	0x0000 was never seen
    conclusion: this is really a bitfield:
    0x1000 is 'info available' bit
    'mailbox overflowed' bit is 0x8000, not 0x1000
    value of 0x0000 probably means that there is no message at all
    P.S. I dunno how in hell hw is supposed to notice that messages are lost -
    it does NOT clear bit 0x0001, and this bit will probably stay forever set
    after we set it once. Let's hope this will be fixed in firmware someday
*/
static void
acx_read_info_status(wlandevice_t *priv)
{
	u32 value;

	acx_write_reg32(priv, IO_ACX_SLV_END_CTL, 0x0);
	acx_write_reg32(priv, IO_ACX_SLV_MEM_CTL, 0x1);

	acx_write_reg32(priv, IO_ACX_SLV_MEM_ADDR,
		acx_read_reg32(priv, IO_ACX_INFO_MAILBOX_OFFS));

	/* make sure we only read the data once all cfg registers are written: */
	acx_write_flush(priv);
	value = acx_read_reg32(priv, IO_ACX_SLV_MEM_DATA);

	priv->info_type = (u16)value;
	priv->info_status = (value >> 16);

	/* inform hw that we have read this info message */
	acx_write_reg32(priv, IO_ACX_SLV_MEM_DATA, priv->info_type | 0x00010000);
	acx_write_flush(priv);
	/* now bother hw to notice it: */
	acx_write_reg16(priv, IO_ACX_INT_TRIG, INT_TRIG_INFOACK);
	acx_write_flush(priv);

	acxlog(L_CTL, "info_type 0x%04X, info_status 0x%04X\n",
			priv->info_type, priv->info_status);
}


/***********************************************************************
** acx_write_cmd_type_or_status
*/
static void
acx_write_cmd_type_or_status(wlandevice_t *priv, u32 val)
{
	acx_write_reg32(priv, IO_ACX_SLV_END_CTL, 0x0);
	acx_write_reg32(priv, IO_ACX_SLV_MEM_CTL, 0x1); /* FIXME: why auto increment?? */

	acx_write_reg32(priv, IO_ACX_SLV_MEM_ADDR,
		acx_read_reg32(priv, IO_ACX_CMD_MAILBOX_OFFS));

	/* make sure we only write the data once all config registers are written */
	acx_write_flush(priv);
	acx_write_reg32(priv, IO_ACX_SLV_MEM_DATA, val);
	acx_write_flush(priv);
}
static inline void
acx_write_cmd_type(wlandevice_t *priv, u32 val)
{
	acx_write_cmd_type_or_status(priv, val);
}
static inline void
acx_write_cmd_status(wlandevice_t *priv, u32 val)
{
	acx_write_cmd_type_or_status(priv, val<<16);
}


/***********************************************************************
** acx_read_cmd_status
*/
static void
acx_read_cmd_status(wlandevice_t *priv)
{
	u32 value;

	acx_write_reg32(priv, IO_ACX_SLV_END_CTL, 0x0);
	acx_write_reg32(priv, IO_ACX_SLV_MEM_CTL, 0x1); /* FIXME: why auto increment?? */

	acx_write_reg32(priv, IO_ACX_SLV_MEM_ADDR,
		acx_read_reg32(priv, IO_ACX_CMD_MAILBOX_OFFS));

	/* make sure we only read the data once all config registers are written */
	acx_write_flush(priv);
	value = acx_read_reg32(priv, IO_ACX_SLV_MEM_DATA);

	priv->cmd_type = (u16)value;
	priv->cmd_status = (value >> 16);

	acxlog(L_CTL, "cmd_type 0x%04X, cmd_status 0x%04X [%s]\n",
		priv->cmd_type, priv->cmd_status,
		acx_cmd_status_str(priv->cmd_status));
}


/***********************************************************************
** acx_s_reset_dev
**
** Arguments:
**	netdevice that contains the wlandevice priv variable
** Returns:
**	NOT_OK on fail
**	OK on success
** Side effects:
**	device is hard reset
** Call context:
**	acx_probe_pci
** Comment:
**	This resets the acx100 device using low level hardware calls
**	as well as uploads and verifies the firmware to the card
*/
static int
acx_s_reset_dev(netdevice_t *dev)
{
	wlandevice_t *priv = netdev_priv(dev);
	const char* msg = "";
	unsigned long flags;
	int result = NOT_OK;
	u16 hardware_info;
	u16 ecpu_ctrl;

	FN_ENTER;

	/* we're doing a reset, so hardware is unavailable */

	/* reset the device to make sure the eCPU is stopped
	 * to upload the firmware correctly */

	acx_lock(priv, flags);

	acx_l_reset_mac(priv);

	ecpu_ctrl = acx_read_reg16(priv, IO_ACX_ECPU_CTRL) & 1;
	if (!ecpu_ctrl) {
		msg = "eCPU is already running. ";
		goto fail_unlock;
	}

#ifdef WE_DONT_NEED_THAT_DO_WE
	if (acx_read_reg16(priv, IO_ACX_SOR_CFG) & 2) {
		/* eCPU most likely means "embedded CPU" */
		msg = "eCPU did not start after boot from flash. ";
		goto fail_unlock;
	}

	/* check sense on reset flags */
	if (acx_read_reg16(priv, IO_ACX_SOR_CFG) & 0x10) {
		printk("%s: eCPU did not start after boot (SOR), "
			"is this fatal?\n", dev->name);
	}
#endif
	/* scan, if any, is stopped now, setting corresponding IRQ bit */
	priv->irq_status |= HOST_INT_SCAN_COMPLETE;

	acx_unlock(priv, flags);

	/* without this delay acx100 may fail to report hardware_info
	** (see below). Most probably eCPU runs some init code */
	acx_s_msleep(10);

	/* Need to know radio type before fw load */
	hardware_info = acx_read_reg16(priv, IO_ACX_EEPROM_INFORMATION);
	priv->form_factor = hardware_info & 0xff;
	priv->radio_type = hardware_info >> 8;

	/* load the firmware */
	if (OK != acx_s_upload_fw(priv))
		goto fail;

	acx_s_msleep(10);

	/* now start eCPU by clearing bit */
	acxlog(L_DEBUG, "booted eCPU up and waiting for completion...\n");
	acx_write_reg16(priv, IO_ACX_ECPU_CTRL, ecpu_ctrl & ~0x1);

	/* wait for eCPU bootup */
	if (OK != acx_s_verify_init(priv)) {
		msg = "timeout waiting for eCPU. ";
		goto fail;
	}

	acxlog(L_DEBUG, "eCPU has woken up, card is ready to be configured\n");

	if (IS_ACX111(priv)) {
		acxlog(L_DEBUG, "cleaning up cmd mailbox access area\n");
		acx_write_cmd_status(priv, 0);
		acx_read_cmd_status(priv);
		if (priv->cmd_status) {
			msg = "error cleaning cmd mailbox area. ";
			goto fail;
		}
	}

	/* TODO what is this one doing ?? adapt for acx111 */
	if ((OK != acx_read_eeprom_area(priv)) && IS_ACX100(priv)) {
		/* does "CIS" mean "Card Information Structure"?
		 * If so, then this would be a PCMCIA message...
		 */
		msg = "CIS error. ";
		goto fail;
	}

	result = OK;
	FN_EXIT1(result);
	return result;

/* Finish error message. Indicate which function failed */
fail_unlock:
	acx_unlock(priv, flags);
fail:
	printk("acx: %sreset_dev() FAILED\n", msg);
	FN_EXIT1(result);
	return result;
}


/***********************************************************************
** acx_init_mboxes
*/
void
acx_init_mboxes(wlandevice_t *priv)
{
	u32 cmd_offs, info_offs;

	FN_ENTER;

	cmd_offs = acx_read_reg32(priv, IO_ACX_CMD_MAILBOX_OFFS);
	info_offs = acx_read_reg32(priv, IO_ACX_INFO_MAILBOX_OFFS);
	priv->cmd_area = (u8 *)priv->iobase2 + cmd_offs + 0x4;
	priv->info_area = (u8 *)priv->iobase2 + info_offs + 0x4;
	acxlog(L_DEBUG, "iobase2=%p\n"
		"cmd_mbox_offset=%X cmd_area=%p\n"
		"info_mbox_offset=%X info_area=%p\n",
		priv->iobase2,
		cmd_offs, priv->cmd_area,
		info_offs, priv->info_area);

	FN_EXIT0;
}


/*----------------------------------------------------------------
* acx_s_issue_cmd_timeo
* Excecutes a command in the command mailbox
*
* Arguments:
*   *pcmdparam = an pointer to the data. The data mustn't include
*                the 4 byte command header!
*
* NB: we do _not_ take lock inside, so be sure to not touch anything
* which may interfere with IRQ handler operation
*
* TODO: busy wait is a bit silly, so:
* 1) stop doing many iters - go to sleep after first
* 2) go to waitqueue based approach: wait, not poll!
*----------------------------------------------------------------*/
#undef FUNC
#define FUNC "issue_cmd"

#if !ACX_DEBUG
int
acxpci_s_issue_cmd_timeo(
	wlandevice_t *priv,
	unsigned int cmd,
	void *buffer,
	unsigned buflen,
	unsigned timeout)
{
#else
int
acxpci_s_issue_cmd_timeo_debug(
	wlandevice_t *priv,
	unsigned cmd,
	void *buffer,
	unsigned buflen,
	unsigned timeout,
	const char* cmdstr)
{
	unsigned long start = jiffies;
#endif
	const char *devname;
	unsigned counter;
	u16 irqtype;
	u16 cmd_status;

	FN_ENTER;

	devname = priv->netdev->name;
	if (!devname || !devname[0])
		devname = "acx";

	acxlog(L_CTL, FUNC"(cmd:%s,buflen:%u,timeout:%ums,type:0x%04X)\n",
		cmdstr, buflen, timeout,
		buffer ? le16_to_cpu(((acx_ie_generic_t *)buffer)->type) : -1);

	if (!(priv->dev_state_mask & ACX_STATE_FW_LOADED)) {
		printk("%s: "FUNC"(): firmware is not loaded yet, "
			"cannot execute commands!\n", devname);
		goto bad;
	}

	if ((acx_debug & L_DEBUG) && (cmd != ACX1xx_CMD_INTERROGATE)) {
		printk("input pdr (len=%u):\n", buflen);
		acx_dump_bytes(buffer, buflen);
	}

	/* wait for firmware to become idle for our command submission */
	counter = 199; /* in ms */
	do {
		acx_read_cmd_status(priv);
		/* Test for IDLE state */
		if (!priv->cmd_status)
			break;
		if (counter % 10 == 0) {
			/* we waited 10 iterations, no luck. Sleep 10 ms */
			acx_s_msleep(10);
		}
	} while (--counter);

	if (!counter) {
		/* the card doesn't get idle, we're in trouble */
		printk("%s: "FUNC"(): cmd_status is not IDLE: 0x%04X!=0\n",
			devname, priv->cmd_status);
		goto bad;
	} else if (counter < 190) { /* if waited >10ms... */
		acxlog(L_CTL|L_DEBUG, FUNC"(): waited for IDLE %dms. "
			"Please report\n", 199 - counter);
	}

	/* now write the parameters of the command if needed */
	if (buffer && buflen) {
		/* if it's an INTERROGATE command, just pass the length
		 * of parameters to read, as data */
#if CMD_DISCOVERY
		if (cmd == ACX1xx_CMD_INTERROGATE)
			memset(priv->cmd_area, 0xAA, buflen);
#endif
		memcpy(priv->cmd_area, buffer,
			(cmd == ACX1xx_CMD_INTERROGATE) ? 4 : buflen);
	}
	/* now write the actual command type */
	priv->cmd_type = cmd;
	acx_write_cmd_type(priv, cmd);
	/* execute command */
	acx_write_reg16(priv, IO_ACX_INT_TRIG, INT_TRIG_CMD);
	acx_write_flush(priv);

	/* wait for firmware to process command */

	/* Ensure nonzero and not too large timeout.
	** Also converts e.g. 100->99, 200->199
	** which is nice but not essential */
	timeout = (timeout-1) | 1;
	if (unlikely(timeout > 1199))
		timeout = 1199;
	/* clear CMD_COMPLETE bit. can be set only by IRQ handler: */
	priv->irq_status &= ~HOST_INT_CMD_COMPLETE;

	/* we schedule away sometimes (timeout can be large) */
	counter = timeout;
	do {
		if (!priv->irqs_active) { /* IRQ disabled: poll */
			irqtype = acx_read_reg16(priv, IO_ACX_IRQ_STATUS_NON_DES);
			if (irqtype & HOST_INT_CMD_COMPLETE) {
				acx_write_reg16(priv, IO_ACX_IRQ_ACK,
						HOST_INT_CMD_COMPLETE);
				break;
			}
		} else { /* Wait when IRQ will set the bit */
			irqtype = priv->irq_status;
			if (irqtype & HOST_INT_CMD_COMPLETE)
				break;
		}

		if (counter % 10 == 0) {
			/* we waited 10 iterations, no luck. Sleep 10 ms */
			acx_s_msleep(10);
		}
	} while (--counter);

	/* save state for debugging */
	acx_read_cmd_status(priv);
	cmd_status = priv->cmd_status;

	/* put the card in IDLE state */
	priv->cmd_status = 0;
	acx_write_cmd_status(priv, 0);

	if (!counter) {	/* timed out! */
		printk("%s: "FUNC"(): timed out %s for CMD_COMPLETE. "
			"irq bits:0x%04X irq_status:0x%04X timeout:%dms "
			"cmd_status:%d (%s)\n",
			devname, (priv->irqs_active) ? "waiting" : "polling",
			irqtype, priv->irq_status, timeout,
			cmd_status, acx_cmd_status_str(cmd_status));
		goto bad;
	} else if (timeout - counter > 30) { /* if waited >30ms... */
		acxlog(L_CTL|L_DEBUG, FUNC"(): %s for CMD_COMPLETE %dms. "
			"count:%d. Please report\n",
			(priv->irqs_active) ? "waited" : "polled",
			timeout - counter, counter);
	}

	if (1 != cmd_status) { /* it is not a 'Success' */
		printk("%s: "FUNC"(): cmd_status is not SUCCESS: %d (%s). "
			"Took %dms of %d\n",
			devname, cmd_status, acx_cmd_status_str(cmd_status),
			timeout - counter, timeout);
		/* zero out result buffer */
		if (buffer && buflen)
			memset(buffer, 0, buflen);
		goto bad;
	}

	/* read in result parameters if needed */
	if (buffer && buflen && (cmd == ACX1xx_CMD_INTERROGATE)) {
		memcpy(buffer, priv->cmd_area, buflen);
		if (acx_debug & L_DEBUG) {
			printk("output buffer (len=%u): ", buflen);
			acx_dump_bytes(buffer, buflen);
		}
	}
/* ok: */
	acxlog(L_CTL, FUNC"(%s): took %ld jiffies to complete\n",
			 cmdstr, jiffies - start);
	FN_EXIT1(OK);
	return OK;

bad:
	/* Give enough info so that callers can avoid
	** printing their own diagnostic messages */
#if ACX_DEBUG
	printk("%s: "FUNC"(cmd:%s) FAILED\n", devname, cmdstr);
#else
	printk("%s: "FUNC"(cmd:0x%04X) FAILED\n", devname, cmd);
#endif
	dump_stack();
	FN_EXIT1(NOT_OK);
	return NOT_OK;
}


/*----------------------------------------------------------------
* acx_s_get_firmware_version
*----------------------------------------------------------------*/
static void
acx_s_get_firmware_version(wlandevice_t *priv)
{
	fw_ver_t fw;
	u8 hexarr[4] = { 0, 0, 0, 0 };
	int hexidx = 0, val = 0;
	const char *num;
	char c;

	FN_ENTER;

	acx_s_interrogate(priv, &fw, ACX1xx_IE_FWREV);
	memcpy(priv->firmware_version, fw.fw_id, FW_ID_SIZE);
	priv->firmware_version[FW_ID_SIZE] = '\0';
	acxlog(L_DEBUG, "fw_ver: fw_id='%s' hw_id=%08X\n",
				priv->firmware_version, fw.hw_id);

	if (strncmp(fw.fw_id, "Rev ", 4) != 0) {
		printk("acx: strange firmware version string "
			"'%s', please report\n", priv->firmware_version);
		priv->firmware_numver = 0x01090407; /* assume 1.9.4.7 */
	} else {
		num = &fw.fw_id[4];
		while (1) {
			c = *num++;
			if ((c == '.') || (c == '\0')) {
				hexarr[hexidx++] = val;
				if ((hexidx > 3) || (c == '\0')) /* end? */
					break;
				val = 0;
				continue;
			}
			if ((c >= '0') && (c <= '9'))
				c -= '0';
			else
				c = c - 'a' + (char)10;
			val = val*16 + c;
		}

		priv->firmware_numver = (u32)(
				(hexarr[0] << 24) + (hexarr[1] << 16)
				+ (hexarr[2] << 8) + hexarr[3]);
		acxlog(L_DEBUG, "firmware_numver 0x%08X\n", priv->firmware_numver);
	}
	if (IS_ACX111(priv)) {
		if (priv->firmware_numver == 0x00010011) {
			/* This one does not survive floodpinging */
			printk("acx: firmware '%s' is known to be buggy, "
				"please upgrade\n", priv->firmware_version);
		}
		if (priv->firmware_numver == 0x02030131) {
			/* With this one, all rx packets look mangled
			** Most probably we simply do not know how to use it
			** properly */
			printk("acx: firmware '%s' does not work well "
				"with this driver\n", priv->firmware_version);
		}
	}

	priv->firmware_id = le32_to_cpu(fw.hw_id);

	/* we're able to find out more detailed chip names now */
	switch (priv->firmware_id & 0xffff0000) {
		case 0x01010000:
		case 0x01020000:
			priv->chip_name = name_tnetw1100a;
			break;
		case 0x01030000:
			priv->chip_name = name_tnetw1100b;
			break;
		case 0x03000000:
		case 0x03010000:
			priv->chip_name = name_tnetw1130;
			break;
		default:
			printk("acx: unknown chip ID 0x%08X, "
				"please report\n", priv->firmware_id);
			break;
	}

	FN_EXIT0;
}


/*----------------------------------------------------------------
* acx_display_hardware_details
*
* Arguments:
*	priv: ptr to wlandevice that contains all the details
*	  displayed by this function
* Call context:
*	acx_probe_pci
* Comment:
*	This function will display strings to the system log according
* to device form_factor and radio type. It will needed to be
*----------------------------------------------------------------*/
static void
acx_display_hardware_details(wlandevice_t *priv)
{
	const char *radio_str, *form_str;

	FN_ENTER;

	switch (priv->radio_type) {
	case RADIO_MAXIM_0D:
		/* hmm, the DWL-650+ seems to have two variants,
		 * according to a windows driver changelog comment:
		 * RFMD and Maxim. */
		radio_str = "Maxim";
		break;
	case RADIO_RFMD_11:
		radio_str = "RFMD";
		break;
	case RADIO_RALINK_15:
		radio_str = "Ralink";
		break;
	case RADIO_RADIA_16:
		radio_str = "Radia";
		break;
	case RADIO_UNKNOWN_17:
		/* TI seems to have a radio which is
		 * additionally 802.11a capable, too */
		radio_str = "802.11a/b/g radio?! Please report";
		break;
	case RADIO_UNKNOWN_19:
		radio_str = "A radio used by Safecom cards?! Please report";
		break;
	default:
		radio_str = "UNKNOWN, please report the radio type name!";
		break;
	}

	switch (priv->form_factor) {
	case 0x00:
		form_str = "unspecified";
		break;
	case 0x01:
		form_str = "(mini-)PCI / CardBus";
		break;
	case 0x02:
		form_str = "USB";
		break;
	case 0x03:
		form_str = "Compact Flash";
		break;
	default:
		form_str = "UNKNOWN, Please report";
		break;
	}

	printk("acx: form factor 0x%02X (%s), "
		"radio type 0x%02X (%s), EEPROM version 0x%02X, "
		"uploaded firmware '%s' (0x%08X)\n",
		priv->form_factor, form_str, priv->radio_type, radio_str,
		priv->eeprom_version, priv->firmware_version,
		priv->firmware_id);

	FN_EXIT0;
}

/***********************************************************************
*/
#ifdef NONESSENTIAL_FEATURES
typedef struct device_id {
	unsigned char id[6];
	char *descr;
	char *type;
} device_id_t;

static const device_id_t
device_ids[] =
{
	{
		{'G', 'l', 'o', 'b', 'a', 'l'},
		NULL,
		NULL,
	},
	{
		{0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
		"uninitialized",
		"SpeedStream SS1021 or Gigafast WF721-AEX"
	},
	{
		{0x80, 0x81, 0x82, 0x83, 0x84, 0x85},
		"non-standard",
		"DrayTek Vigor 520"
	},
	{
		{'?', '?', '?', '?', '?', '?'},
		"non-standard",
		"Level One WPC-0200"
	},
	{
		{0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
		"empty",
		"DWL-650+ variant"
	}
};

static void
acx_show_card_eeprom_id(wlandevice_t *priv)
{
	unsigned char buffer[CARD_EEPROM_ID_SIZE];
	int i;

	memset(&buffer, 0, CARD_EEPROM_ID_SIZE);
	/* use direct EEPROM access */
	for (i = 0; i < CARD_EEPROM_ID_SIZE; i++) {
		if (OK != acx_read_eeprom_offset(priv,
					 ACX100_EEPROM_ID_OFFSET + i,
					 &buffer[i]))
		{
			printk("acx: reading EEPROM FAILED\n");
			break;
		}
	}

	for (i = 0; i < VEC_SIZE(device_ids); i++) {
		if (!memcmp(&buffer, device_ids[i].id, CARD_EEPROM_ID_SIZE)) {
			if (device_ids[i].descr) {
				printk("acx: EEPROM card ID string check "
					"found %s card ID: is this %s?\n",
					device_ids[i].descr, device_ids[i].type);
			}
			break;
		}
	}
	if (i == VEC_SIZE(device_ids)) {
		printk("acx: EEPROM card ID string check found "
			"unknown card: expected 'Global', got '%.*s\'. "
			"Please report\n", CARD_EEPROM_ID_SIZE, buffer);
	}
}
#endif /* NONESSENTIAL_FEATURES */


/***********************************************************************
*/
static void
acx_s_device_chain_add(struct net_device *dev)
{
	wlandevice_t *priv = netdev_priv(dev);

	down(&root_acx_dev_sem);
	priv->prev_nd = root_acx_dev.newest;
	root_acx_dev.newest = dev;
	priv->netdev = dev;
	up(&root_acx_dev_sem);
}

static void
acx_s_device_chain_remove(struct net_device *dev)
{
	struct net_device *querydev;
	struct net_device *olderdev;
	struct net_device *newerdev;

	down(&root_acx_dev_sem);
	querydev = root_acx_dev.newest;
	newerdev = NULL;
	while (querydev) {
		olderdev = ((wlandevice_t*)netdev_priv(querydev))->prev_nd;
		if (0 == strcmp(querydev->name, dev->name)) {
			if (!newerdev) {
				/* if we were at the beginning of the
				 * list, then it's the list head that
				 * we need to update to point at the
				 * next older device */
				root_acx_dev.newest = olderdev;
			} else {
				/* it's the device that is newer than us
				 * that we need to update to point at
				 * the device older than us */
				((wlandevice_t*)netdev_priv(newerdev))->
					prev_nd = olderdev;
			}
			break;
		}
		/* "newerdev" is actually the device of the old iteration,
		 * but since the list starts (root_acx_dev.newest)
		 * with the newest devices,
		 * it's newer than the ones following.
		 * Oh the joys of iterating from newest to oldest :-\ */
		newerdev = querydev;

		/* keep checking old devices for matches until we hit the end
		 * of the list */
		querydev = olderdev;
	}
	up(&root_acx_dev_sem);
}


/***********************************************************************
** acx_free_desc_queues
**
** Releases the queues that have been allocated, the
** others have been initialised to NULL so this
** function can be used if only part of the queues were allocated.
*/
static inline void
acx_free_coherent(struct pci_dev *hwdev, size_t size,
			void *vaddr, dma_addr_t dma_handle)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 5, 53)
	dma_free_coherent(hwdev == NULL ? NULL : &hwdev->dev,
			size, vaddr, dma_handle);
#else
	pci_free_consistent(hwdev, size, vaddr, dma_handle);
#endif
}

void
acx_free_desc_queues(wlandevice_t *priv)
{
#define ACX_FREE_QUEUE(size, ptr, phyaddr) \
	if (ptr) { \
		acx_free_coherent(0, size, ptr, phyaddr); \
		ptr = NULL; \
		size = 0; \
	}

	FN_ENTER;

	ACX_FREE_QUEUE(priv->txhostdesc_area_size, priv->txhostdesc_start, priv->txhostdesc_startphy);
	ACX_FREE_QUEUE(priv->txbuf_area_size, priv->txbuf_start, priv->txbuf_startphy);

	priv->txdesc_start = NULL;

	ACX_FREE_QUEUE(priv->rxhostdesc_area_size, priv->rxhostdesc_start, priv->rxhostdesc_startphy);
	ACX_FREE_QUEUE(priv->rxbuf_area_size, priv->rxbuf_start, priv->rxbuf_startphy);

	priv->rxdesc_start = NULL;

	FN_EXIT0;
}


/*----------------------------------------------------------------
* acx_s_delete_dma_regions
*----------------------------------------------------------------*/
static void
acx_s_delete_dma_regions(wlandevice_t *priv)
{
	unsigned long flags;

	FN_ENTER;
	/* disable radio Tx/Rx. Shouldn't we use the firmware commands
	 * here instead? Or are we that much down the road that it's no
	 * longer possible here? */
	acx_write_reg16(priv, IO_ACX_ENABLE, 0);

	acx_s_msleep(100);

	acx_lock(priv, flags);
	acx_free_desc_queues(priv);
	acx_unlock(priv, flags);

	FN_EXIT0;
}


/*----------------------------------------------------------------
* acx_e_probe_pci
*
* Probe routine called when a PCI device w/ matching ID is found.
* Here's the sequence:
*   - Allocate the PCI resources.
*   - Read the PCMCIA attribute memory to make sure we have a WLAN card
*   - Reset the MAC
*   - Initialize the dev and wlan data
*   - Initialize the MAC
*
* Arguments:
*	pdev		ptr to pci device structure containing info about
*			pci configuration.
*	id		ptr to the device id entry that matched this device.
*
* Returns:
*	zero		- success
*	negative	- failed
*
* Call context:
*	process thread
----------------------------------------------------------------*/
static const u16
IO_ACX100[] =
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

static const u16
IO_ACX111[] =
{
	0x0000, /* IO_ACX_SOFT_RESET */

	0x0014, /* IO_ACX_SLV_MEM_ADDR */
	0x0018, /* IO_ACX_SLV_MEM_DATA */
	0x001c, /* IO_ACX_SLV_MEM_CTL */
	0x0020, /* IO_ACX_SLV_END_CTL */

	0x0034, /* IO_ACX_FEMR */

	0x00b4, /* IO_ACX_INT_TRIG */
	0x00d4, /* IO_ACX_IRQ_MASK */
	/* we need NON_DES (0xf0), not NON_DES_MASK which is at 0xe0: */
	0x00f0, /* IO_ACX_IRQ_STATUS_NON_DES */
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

static void
acx_netdev_init(struct net_device *dev) {}

//FIXME: do the same for USB
static int
acx_change_mtu(struct net_device *dev, int mtu)
{
	enum {
		MIN_MTU = 256,
		MAX_MTU = WLAN_DATA_MAXLEN - (ETH_HLEN)
	};

	if (mtu < MIN_MTU || mtu > MAX_MTU)
		return -EINVAL;

	dev->mtu = mtu;
	return 0;
}

static int __devinit
acx_e_probe_pci(struct pci_dev *pdev, const struct pci_device_id *id)
{
	unsigned long mem_region1 = 0;
	unsigned long mem_region2 = 0;
	unsigned long mem_region1_size;
	unsigned long mem_region2_size;
	unsigned long phymem1;
	unsigned long phymem2;
	void *mem1 = NULL;
	void *mem2 = NULL;
	wlandevice_t *priv = NULL;
	struct net_device *dev = NULL;
	const char *chip_name;
	int result = -EIO;
	int err;
	u8 chip_type;

#if SEPARATE_DRIVER_INSTANCES
	struct pci_dev *tdev;
	unsigned int inited;
	static int turn = 0;
#endif /* SEPARATE_DRIVER_INSTANCES */

	FN_ENTER;

#if SEPARATE_DRIVER_INSTANCES
	if (card) {
		turn++;
		inited = 0;
		pci_for_each_dev(tdev) {
			if (tdev->vendor != PCI_VENDOR_ID_TI)
				continue;

			if (tdev == pdev)
				break;
			if (pci_get_drvdata(tdev))
				inited++;
		}
		if (inited + turn != card) {
			result = -ENODEV;
			goto done;
		}
	}
#endif /* SEPARATE_DRIVER_INSTANCES */

	/* Enable the PCI device */
	if (pci_enable_device(pdev)) {
		printk("acx: pci_enable_device() FAILED\n");
		result = -ENODEV;
		goto fail_pci_enable_device;
	}

	/* enable busmastering (required for CardBus) */
	pci_set_master(pdev);

	/* chiptype is u8 but id->driver_data is ulong
	** Works for now (possible values are 1 and 2) */
	chip_type = (u8)id->driver_data;
	/* acx100 and acx111 have different PCI memory regions */
	if (chip_type == CHIPTYPE_ACX100) {
		chip_name = name_acx100;
		mem_region1 = PCI_ACX100_REGION1;
		mem_region1_size  = PCI_ACX100_REGION1_SIZE;

		mem_region2 = PCI_ACX100_REGION2;
		mem_region2_size  = PCI_ACX100_REGION2_SIZE;
	} else if (chip_type == CHIPTYPE_ACX111) {
		chip_name = name_acx111;
		mem_region1 = PCI_ACX111_REGION1;
		mem_region1_size  = PCI_ACX111_REGION1_SIZE;

		mem_region2 = PCI_ACX111_REGION2;
		mem_region2_size  = PCI_ACX111_REGION2_SIZE;
	} else {
		printk("acx: unknown chip type 0x%04X\n", chip_type);
		goto fail_unknown_chiptype;
	}

	/* Figure out our resources */
	phymem1 = pci_resource_start(pdev, mem_region1);
	phymem2 = pci_resource_start(pdev, mem_region2);

	if (!request_mem_region(phymem1, pci_resource_len(pdev, mem_region1), "ACX1xx_1")) {
		printk("acx: cannot reserve PCI memory region 1 (are you sure "
			"you have CardBus support in kernel?)\n");
		goto fail_request_mem_region1;
	}

	if (!request_mem_region(phymem2, pci_resource_len(pdev, mem_region2), "ACX1xx_2")) {
		printk("acx: cannot reserve PCI memory region 2\n");
		goto fail_request_mem_region2;
	}

	mem1 = ioremap(phymem1, mem_region1_size);
	if (NULL == mem1) {
		printk("acx: ioremap() FAILED\n");
		goto fail_ioremap1;
	}

	mem2 = ioremap(phymem2, mem_region2_size);
	if (NULL == mem2) {
		printk("acx: ioremap() #2 FAILED\n");
		goto fail_ioremap2;
	}

	/* Log the device */
	printk("acx: found %s-based wireless network card at %s, irq:%d, "
		"phymem1:0x%lX, phymem2:0x%lX, mem1:0x%p, mem1_size:%ld, "
		"mem2:0x%p, mem2_size:%ld\n",
		chip_name, pci_name(pdev), pdev->irq, phymem1, phymem2,
		mem1, mem_region1_size,
		mem2, mem_region2_size);
	acxlog(L_ANY, "initial debug setting is 0x%04X\n", acx_debug);

	if (0 == pdev->irq) {
		printk("acx: can't use IRQ 0\n");
		goto fail_irq;
	}

	dev = alloc_netdev(sizeof(wlandevice_t), "wlan%d", acx_netdev_init);
	/* (NB: memsets to 0 entire area) */
	if (!dev) {
		printk("acx: no memory for netdevice structure\n");
		goto fail_alloc_netdev;
	}

	ether_setup(dev);
	dev->open = &acx_e_open;
	dev->stop = &acx_e_close;
	dev->hard_start_xmit = &acx_i_start_xmit;
	dev->get_stats = &acx_e_get_stats;
	dev->get_wireless_stats = &acx_e_get_wireless_stats;
#if WIRELESS_EXT >= 13
	dev->wireless_handlers = (struct iw_handler_def *)&acx_ioctl_handler_def;
#else
	dev->do_ioctl = &acx_e_ioctl_old;
#endif
	dev->set_multicast_list = &acx_i_set_multicast_list;
	dev->tx_timeout = &acx_i_tx_timeout;
	dev->change_mtu = &acx_change_mtu;
	dev->watchdog_timeo = 4 * HZ;
	dev->irq = pdev->irq;
	dev->base_addr = pci_resource_start(pdev, 0);

	priv = netdev_priv(dev);
	spin_lock_init(&priv->lock);	/* initial state: unlocked */
	/* We do not start with downed sem: we want PARANOID_LOCKING to work */
	sema_init(&priv->sem, 1);	/* initial state: 1 (upped) */
	/* since nobody can see new netdev yet, we can as well
	** just _presume_ that we're under sem (instead of actually taking it): */
	/* acx_sem_lock(priv); */
	priv->pdev = pdev;
	priv->dev_type = DEVTYPE_PCI;
	priv->chip_type = chip_type;
	priv->chip_name = chip_name;
	priv->io = (CHIPTYPE_ACX100 == chip_type) ? IO_ACX100 : IO_ACX111;
	priv->membase = phymem1;
	priv->iobase = mem1;
	priv->membase2 = phymem2;
	priv->iobase2 = mem2;
	/* to find crashes due to weird driver access
	 * to unconfigured interface (ifup) */
	priv->mgmt_timer.function = (void (*)(unsigned long))0x0000dead;

#ifdef NONESSENTIAL_FEATURES
	acx_show_card_eeprom_id(priv);
#endif /* NONESSENTIAL_FEATURES */

	/* now we have our device, so make sure the kernel doesn't try
	 * to send packets even though we're not associated to a network yet */
	acx_stop_queue(dev, "after setup");

#ifdef SET_MODULE_OWNER
	SET_MODULE_OWNER(dev);
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 5, 70)
	/* this define and its netdev member exist since 2.5.70 */
	SET_NETDEV_DEV(dev, &pdev->dev);
#endif

	/* register new dev in linked list */
	acx_s_device_chain_add(dev);

	acxlog(L_IRQ|L_INIT, "using IRQ %d\n", pdev->irq);

	/* need to be able to restore PCI state after a suspend */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 10)
	/* 2.6.9-rc3-mm2 (2.6.9-bk4, too) introduced this shorter version,
	   then it made its way into 2.6.10 */
	pci_save_state(pdev);
#else
	pci_save_state(pdev, priv->pci_state);
#endif

	/* NB: acx_read_reg() reads may return bogus data before reset_dev().
	** acx100 seems to be more affected than acx111 */
	if (OK != acx_s_reset_dev(dev)) {
		goto fail_reset;
	}

	/* ok, basic setup is finished, now start initialising the card */

	if (OK != acx_read_eeprom_offset(priv, 0x05, &priv->eeprom_version)) {
		goto fail_read_eeprom_version;
	}

	if (OK != acx_s_init_mac(dev)) {
		printk("acx: init_mac() FAILED\n");
		goto fail_init_mac;
	}
	if (OK != acx_s_set_defaults(priv)) {
		printk("acx: set_defaults() FAILED\n");
		goto fail_set_defaults;
	}

	/* needs to be after acx_s_init_mac() due to necessary init stuff */
	acx_s_get_firmware_version(priv);

	acx_display_hardware_details(priv);

	pci_set_drvdata(pdev, dev);

	/* ...and register the card, AFTER everything else has been set up,
	 * since otherwise an ioctl could step on our feet due to
	 * firmware operations happening in parallel or uninitialized data */
	err = register_netdev(dev);
	if (OK != err) {
		printk("acx: register_netdev() FAILED: %d\n", err);
		goto fail_register_netdev;
	}

	acx_carrier_off(dev, "on probe");

	if (OK != acx_proc_register_entries(dev)) {
		goto fail_proc_register_entries;
	}

	/* after register_netdev() userspace may start working with dev
	 * (in particular, on other CPUs), we only need to up the sem */
	/* acx_sem_unlock(priv); */

	printk("acx "WLAN_RELEASE": net device %s, driver compiled "
		"against wireless extensions %d and Linux %s\n",
		dev->name, WIRELESS_EXT, UTS_RELEASE);

#if CMD_DISCOVERY
	great_inquisitor(priv);
#endif

	result = OK;
	goto done;

	/* error paths: undo everything in reverse order... */

#ifdef CONFIG_PROC_FS
fail_proc_register_entries:

	if (priv->dev_state_mask & ACX_STATE_IFACE_UP)
		acx_s_down(dev);

	unregister_netdev(dev);

	/* after unregister_netdev() userspace is guaranteed to finish
	 * working with it. netdev does not exist anymore.
	 * For paranoid reasons I am taking sem anyway */
	acx_sem_lock(priv);
#endif

fail_register_netdev:

	acx_s_delete_dma_regions(priv);
	pci_set_drvdata(pdev, NULL);

fail_set_defaults:
fail_init_mac:
fail_read_eeprom_version:
fail_reset:

	acx_s_device_chain_remove(dev);
	free_netdev(dev);
fail_alloc_netdev:
fail_irq:

	iounmap(mem2);
fail_ioremap2:

	iounmap(mem1);
fail_ioremap1:

	release_mem_region(pci_resource_start(pdev, mem_region2),
			   pci_resource_len(pdev, mem_region2));
fail_request_mem_region2:

	release_mem_region(pci_resource_start(pdev, mem_region1),
			   pci_resource_len(pdev, mem_region1));
fail_request_mem_region1:
fail_unknown_chiptype:

	pci_disable_device(pdev);
fail_pci_enable_device:

	pci_set_power_state(pdev, 3);

done:
	FN_EXIT1(result);
	return result;
}


/*----------------------------------------------------------------
* acx_e_remove_pci
*
* Deallocate PCI resources for the ACX100 chip.
*
* This should NOT execute any other hardware operations on the card,
* since the card might already be ejected. Instead, that should be done
* in cleanup_module, since the card is most likely still available there.
*
* Arguments:
*	pdev		ptr to PCI device structure containing info about
*			PCI configuration.
*
* Call context:
*	process thread
----------------------------------------------------------------*/
static void __devexit
acx_e_remove_pci(struct pci_dev *pdev)
{
	struct net_device *dev;
	wlandevice_t *priv;
	unsigned long mem_region1, mem_region2;

	FN_ENTER;

	dev = (struct net_device *) pci_get_drvdata(pdev);
	if (!dev) {
		acxlog(L_DEBUG, "%s: card is unused. Skipping any release code\n",
			__func__);
		goto end;
	}

	priv = netdev_priv(dev);

	/* unregister the device to not let the kernel
	 * (e.g. ioctls) access a half-deconfigured device
	 * NB: this will cause acx_e_close() to be called,
	 * thus we shouldn't call it under sem! */
	acxlog(L_INIT, "removing device %s\n", dev->name);
	unregister_netdev(dev);

	/* unregister_netdev ensures that no references to us left.
	 * For paranoid reasons we continue to follow the rules */
	acx_sem_lock(priv);

	if (IS_ACX100(priv)) {
		mem_region1 = PCI_ACX100_REGION1;
		mem_region2 = PCI_ACX100_REGION2;
	} else {
		mem_region1 = PCI_ACX111_REGION1;
		mem_region2 = PCI_ACX111_REGION2;
	}

	acx_proc_unregister_entries(dev);

	/* find our PCI device in the global acx list and remove it */
	acx_s_device_chain_remove(dev);

	if (priv->dev_state_mask & ACX_STATE_IFACE_UP)
		acx_s_down(dev);

	CLEAR_BIT(priv->dev_state_mask, ACX_STATE_IFACE_UP);

	acx_s_delete_dma_regions(priv);

	/* finally, clean up PCI bus state */
	if (priv->iobase) iounmap(priv->iobase);
	if (priv->iobase2) iounmap(priv->iobase2);

	release_mem_region(pci_resource_start(pdev, mem_region1),
			   pci_resource_len(pdev, mem_region1));

	release_mem_region(pci_resource_start(pdev, mem_region2),
			   pci_resource_len(pdev, mem_region2));

	pci_disable_device(pdev);

	/* remove dev registration */
	pci_set_drvdata(pdev, NULL);

	/* Free netdev (quite late,
	 * since otherwise we might get caught off-guard
	 * by a netdev timeout handler execution
	 * expecting to see a working dev...)
	 * But don't use free_netdev() here,
	 * it's supported by newer kernels only */
	free_netdev(dev);

	/* put device into ACPI D3 mode (shutdown) */
	pci_set_power_state(pdev, 3);

end:
	FN_EXIT0;
}


/***********************************************************************
*/
#ifdef CONFIG_PM
static int if_was_up = 0; /* FIXME: HACK, do it correctly sometime instead */
static int
acx_e_suspend(struct pci_dev *pdev, pm_message_t state)
{
	struct net_device *dev = pci_get_drvdata(pdev);
	wlandevice_t *priv = netdev_priv(dev);

	FN_ENTER;

	acx_sem_lock(priv);

	printk("acx: experimental suspend handler called for %p\n", priv);
	if (netif_device_present(dev)) {
		if_was_up = 1;
		acx_s_down(dev);
	}
	else
		if_was_up = 0;

	netif_device_detach(dev);	/* This one cannot sleep */
	acx_s_delete_dma_regions(priv);

	acx_sem_unlock(priv);

	FN_EXIT0;
	return OK;
}

static int
acx_e_resume(struct pci_dev *pdev)
{
	struct net_device *dev;
	wlandevice_t *priv;

	printk(KERN_WARNING "rsm: resume\n");
	dev = pci_get_drvdata(pdev);
	printk(KERN_WARNING "rsm: got dev\n");

	if (!netif_running(dev))
		return 0;

	priv = netdev_priv(dev);

	acx_sem_lock(priv);

	printk(KERN_WARNING "rsm: got priv\n");
	FN_ENTER;
	printk("acx: experimental resume handler called for %p!\n", priv);
	pci_set_power_state(pdev, 0);
	acxlog(L_DEBUG, "rsm: power state set\n");
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 10)
	/* 2.6.9-rc3-mm2 (2.6.9-bk4, too) introduced this shorter version,
	   then it made its way into 2.6.10 */
	pci_restore_state(pdev);
#else
	pci_restore_state(pdev, priv->pci_state);
#endif
	acxlog(L_DEBUG, "rsm: PCI state restored\n");
	acx_s_reset_dev(dev);
	acxlog(L_DEBUG, "rsm: device reset done\n");

	if (OK != acx_s_init_mac(dev)) {
		printk("rsm: init_mac FAILED\n");
		goto fail;
	}
	acxlog(L_DEBUG, "rsm: init MAC done\n");

	if (1 == if_was_up)
		acx_s_up(dev);
	acxlog(L_DEBUG, "rsm: acx up\n");

	/* now even reload all card parameters as they were before suspend,
	 * and possibly be back in the network again already :-)
	 * FIXME: should this be done in that scheduled task instead?? */
	if (ACX_STATE_IFACE_UP & priv->dev_state_mask)
		acx_s_update_card_settings(priv, 0, 1);
	acxlog(L_DEBUG, "rsm: settings updated\n");
	netif_device_attach(dev);
	acxlog(L_DEBUG, "rsm: device attached\n");
fail: /* we need to return OK here anyway, right? */
	acx_sem_unlock(priv);
	FN_EXIT0;
	return OK;
}
#endif /* CONFIG_PM */


/*----------------------------------------------------------------
* acx_s_up
*
* Side effects:
*	- Enables on-card interrupt requests
*	- calls acx_start
* Call context:
*	- process thread
* Comment:
*	This function is called by acx_open (when ifconfig sets the
*	device as up).
*----------------------------------------------------------------*/
static void
acx_s_up(netdevice_t *dev)
{
	wlandevice_t *priv = netdev_priv(dev);
	unsigned long flags;

	FN_ENTER;

	acx_lock(priv, flags);
	acx_l_enable_irq(priv);
	acx_unlock(priv, flags);

	/* acx fw < 1.9.3.e has a hardware timer, and older drivers
	** used to use it. But we don't do that anymore, our OS
	** has reliable software timers */
	init_timer(&priv->mgmt_timer);
	priv->mgmt_timer.function = acx_i_timer;
	priv->mgmt_timer.data = (unsigned long)priv;

	/* Need to set ACX_STATE_IFACE_UP first, or else
	** timer won't be started by acx_set_status() */
	SET_BIT(priv->dev_state_mask, ACX_STATE_IFACE_UP);
	switch (priv->mode) {
	case ACX_MODE_0_ADHOC:
	case ACX_MODE_2_STA:
		/* actual scan cmd will happen in start() */
		acx_set_status(priv, ACX_STATUS_1_SCANNING); break;
	case ACX_MODE_3_AP:
	case ACX_MODE_MONITOR:
		acx_set_status(priv, ACX_STATUS_4_ASSOCIATED); break;
	}

	acx_s_start(priv);

	FN_EXIT0;
}


/*----------------------------------------------------------------
* acx_s_down
*
* Side effects:
*	- disables on-card interrupt request
* Call context:
*	process thread
* Comment:
*	this disables the netdevice
*----------------------------------------------------------------*/
static void
acx_s_down(netdevice_t *dev)
{
	wlandevice_t *priv = netdev_priv(dev);
	unsigned long flags;

	FN_ENTER;

	/* Disable IRQs first, so that IRQs cannot race with us */
	acx_lock(priv, flags);
	acx_l_disable_irq(priv);
	acx_unlock(priv, flags);

	/* we really don't want to have an asynchronous tasklet disturb us
	** after something vital for its job has been shut down, so
	** end all remaining work now.
	**
	** NB: carrier_off (done by set_status below) would lead to
	** not yet fully understood deadlock in FLUSH_SCHEDULED_WORK().
	** That's why we do FLUSH first.
	**
	** NB2: we have a bad locking bug here: FLUSH_SCHEDULED_WORK()
	** waits for acx_e_after_interrupt_task to complete if it is running
	** on another CPU, but acx_e_after_interrupt_task
	** will sleep on sem forever, because it is taken by us!
	** Work around that by temporary sem unlock.
	** This will fail miserably if we'll be hit by concurrent
	** iwconfig or something in between. TODO! */
	acx_sem_unlock(priv);
	FLUSH_SCHEDULED_WORK();
	acx_sem_lock(priv);

	/* This is possible:
	** FLUSH_SCHEDULED_WORK -> acx_e_after_interrupt_task ->
	** -> set_status(ASSOCIATED) -> wake_queue()
	** That's why we stop queue _after_ FLUSH_SCHEDULED_WORK
	** lock/unlock is just paranoia, maybe not needed */
	acx_lock(priv, flags);
	acx_stop_queue(dev, "during close");
	acx_set_status(priv, ACX_STATUS_0_STOPPED);
	acx_unlock(priv, flags);

	/* kernel/timer.c says it's illegal to del_timer_sync()
	** a timer which restarts itself. We guarantee this cannot
	** ever happen because acx_i_timer() never does this if
	** status is ACX_STATUS_0_STOPPED */
	del_timer_sync(&priv->mgmt_timer);

	FN_EXIT0;
}


/*----------------------------------------------------------------
* acx_e_open
*
* WLAN device open method.  Called from p80211netdev when kernel
* device open (start) method is called in response to the
* SIOCSIFFLAGS ioctl changing the flags bit IFF_UP
* from clear to set.
*
* Returns:
*	0	success
*	>0	f/w reported error
*	<0	driver reported error
*
* Call context:
*	process thread
----------------------------------------------------------------*/
static int
acx_e_open(netdevice_t *dev)
{
	wlandevice_t *priv = netdev_priv(dev);
	int result = OK;

	FN_ENTER;

	acxlog(L_INIT, "module count++\n");
	WLAN_MOD_INC_USE_COUNT;

	acx_sem_lock(priv);

	acx_init_task_scheduler(priv);

	/* request shared IRQ handler */
	if (request_irq(dev->irq, acx_i_interrupt, SA_SHIRQ, dev->name, dev)) {
		printk("%s: request_irq FAILED\n", dev->name);
		result = -EAGAIN;
		goto done;
	}
	acxlog(L_DEBUG|L_IRQ, "request_irq %d successful\n", dev->irq);

	/* ifup device */
	acx_s_up(dev);

	/* We don't currently have to do anything else.
	 * The setup of the MAC should be subsequently completed via
	 * the mlme commands.
	 * Higher layers know we're ready from dev->start==1 and
	 * dev->tbusy==0.  Our rx path knows to pass up received/
	 * frames because of dev->flags&IFF_UP is true.
	 */
done:
	acx_sem_unlock(priv);

	FN_EXIT1(result);
	return result;
}


/*----------------------------------------------------------------
* acx_e_close
*
* WLAN device close method.  Called from network core when kernel
* device close method is called in response to the
* SIOCSIIFFLAGS ioctl changing the flags bit IFF_UP
* from set to clear.
* (i.e. called for "ifconfig DEV down")
*
* Returns:
*	0	success
*	>0	f/w reported error
*	<0	driver reported error
*
* Call context:
*	process thread
----------------------------------------------------------------*/
static int
acx_e_close(netdevice_t *dev)
{
	wlandevice_t *priv = netdev_priv(dev);

	FN_ENTER;

	acx_sem_lock(priv);

	/* ifdown device */
	CLEAR_BIT(priv->dev_state_mask, ACX_STATE_IFACE_UP);
	if (netif_device_present(dev)) {
		acx_s_down(dev);
	}

	/* disable all IRQs, release shared IRQ handler */
	acx_write_reg16(priv, IO_ACX_IRQ_MASK, 0xffff);
	acx_write_reg16(priv, IO_ACX_FEMR, 0x0);
	free_irq(dev->irq, dev);

	/* We currently don't have to do anything else.
	 * Higher layers know we're not ready from dev->start==0 and
	 * dev->tbusy==1.  Our rx path knows to not pass up received
	 * frames because of dev->flags&IFF_UP is false.
	 */
	acxlog(L_INIT, "module count--\n");
	WLAN_MOD_DEC_USE_COUNT;

	acx_sem_unlock(priv);

	acxlog(L_INIT, "closed device\n");
	FN_EXIT0;
	return OK;
}


/*----------------------------------------------------------------
* acx_i_tx_timeout
*
* Called from network core. Must not sleep!
*----------------------------------------------------------------*/
static void
acx_i_tx_timeout(netdevice_t *dev)
{
	wlandevice_t *priv = netdev_priv(dev);
	unsigned long flags;
	unsigned int tx_num_cleaned;

	FN_ENTER;

	acx_lock(priv, flags);

	/* clean processed tx descs, they may have been completely full */
	tx_num_cleaned = acx_l_clean_tx_desc(priv);

	/* nothing cleaned, yet (almost) no free buffers available?
	 * --> clean all tx descs, no matter which status!!
	 * Note that I strongly suspect that doing emergency cleaning
	 * may confuse the firmware. This is a last ditch effort to get
	 * ANYTHING to work again...
	 *
	 * TODO: it's best to simply reset & reinit hw from scratch...
	 */
	if ((priv->tx_free <= TX_EMERG_CLEAN) && (tx_num_cleaned == 0)) {
		printk("%s: FAILED to free any of the many full tx buffers. "
			"Switching to emergency freeing. "
			"Please report!\n", dev->name);
		acx_l_clean_tx_desc_emergency(priv);
	}

	if (acx_queue_stopped(dev) && (ACX_STATUS_4_ASSOCIATED == priv->status))
		acx_wake_queue(dev, "after tx timeout");

	/* stall may have happened due to radio drift, so recalib radio */
	acx_schedule_after_interrupt_task(priv, ACX_AFTER_IRQ_CMD_RADIO_RECALIB);

	/* do unimportant work last */
	printk("%s: tx timeout!\n", dev->name);
	priv->stats.tx_errors++;

	acx_unlock(priv, flags);

	FN_EXIT0;
}


/*----------------------------------------------------------------
* acx_e_get_stats
*----------------------------------------------------------------*/
static struct net_device_stats*
acx_e_get_stats(netdevice_t *dev)
{
	wlandevice_t *priv = netdev_priv(dev);
	return &priv->stats;
}


/*----------------------------------------------------------------
* acx_e_get_wireless_stats
*----------------------------------------------------------------*/
static struct iw_statistics*
acx_e_get_wireless_stats(netdevice_t *dev)
{
	wlandevice_t *priv = netdev_priv(dev);
	return &priv->wstats;
}


/*----------------------------------------------------------------
* acx_i_set_multicast_list
* FIXME: most likely needs refinement
*----------------------------------------------------------------*/
static void
acx_i_set_multicast_list(netdevice_t *dev)
{
	wlandevice_t *priv = netdev_priv(dev);
	unsigned long flags;

	FN_ENTER;

	acx_lock(priv, flags);

	/* firmwares don't have allmulti capability,
	 * so just use promiscuous mode instead in this case. */
	if (dev->flags & (IFF_PROMISC|IFF_ALLMULTI)) {
		SET_BIT(priv->rx_config_1, RX_CFG1_RCV_PROMISCUOUS);
		CLEAR_BIT(priv->rx_config_1, RX_CFG1_FILTER_ALL_MULTI);
		SET_BIT(priv->set_mask, SET_RXCONFIG);
		/* let kernel know in case *we* needed to set promiscuous */
		dev->flags |= (IFF_PROMISC|IFF_ALLMULTI);
	} else {
		CLEAR_BIT(priv->rx_config_1, RX_CFG1_RCV_PROMISCUOUS);
		SET_BIT(priv->rx_config_1, RX_CFG1_FILTER_ALL_MULTI);
		SET_BIT(priv->set_mask, SET_RXCONFIG);
		dev->flags &= ~(IFF_PROMISC|IFF_ALLMULTI);
	}

	/* cannot update card settings directly here, atomic context */
	acx_schedule_after_interrupt_task(priv, ACX_AFTER_IRQ_UPDATE_CARD_CFG);

	acx_unlock(priv, flags);

	FN_EXIT0;
}

static void
acx_l_update_link_quality_led(wlandevice_t *priv)
{
	int qual;

	qual = acx_signal_determine_quality(priv->wstats.qual.level, priv->wstats.qual.noise);
	if (qual > priv->brange_max_quality)
		qual = priv->brange_max_quality;

	if (time_after(jiffies, priv->brange_time_last_state_change +
				(HZ/2 - HZ/2 * (unsigned long) qual/priv->brange_max_quality ) )) {
		acx_l_power_led(priv, (priv->brange_last_state == 0));
		priv->brange_last_state ^= 1; /* toggle */
		priv->brange_time_last_state_change = jiffies;
	}
}


/*----------------------------------------------------------------
* acx_l_enable_irq
*----------------------------------------------------------------*/
static void
acx_l_enable_irq(wlandevice_t *priv)
{
	FN_ENTER;
	acx_write_reg16(priv, IO_ACX_IRQ_MASK, priv->irq_mask);
	acx_write_reg16(priv, IO_ACX_FEMR, 0x8000);
	priv->irqs_active = 1;
	FN_EXIT0;
}


/*----------------------------------------------------------------
* acx_l_disable_irq
*----------------------------------------------------------------*/
static void
acx_l_disable_irq(wlandevice_t *priv)
{
	FN_ENTER;
	acx_write_reg16(priv, IO_ACX_IRQ_MASK, priv->irq_mask_off);
	acx_write_reg16(priv, IO_ACX_FEMR, 0x0);
	priv->irqs_active = 0;
	FN_EXIT0;
}

/* scan is complete. all frames now on the receive queue are valid */
#define INFO_SCAN_COMPLETE      0x0001
#define INFO_WEP_KEY_NOT_FOUND  0x0002
/* hw has been reset as the result of a watchdog timer timeout */
#define INFO_WATCH_DOG_RESET    0x0003
/* failed to send out NULL frame from PS mode notification to AP */
/* recommended action: try entering 802.11 PS mode again */
#define INFO_PS_FAIL            0x0004
/* encryption/decryption process on a packet failed */
#define INFO_IV_ICV_FAILURE     0x0005

static void
acx_l_handle_info_irq(wlandevice_t *priv)
{
#if ACX_DEBUG
	static const char * const info_type_msg[] = {
		"(unknown)",
		"scan complete",
		"WEP key not found",
		"internal watchdog reset was done",
		"failed to send powersave (NULL frame) notification to AP",
		"encrypt/decrypt on a packet has failed",
		"TKIP tx keys disabled",
		"TKIP rx keys disabled",
		"TKIP rx: key ID not found",
		"???",
		"???",
		"???",
		"???",
		"???",
		"???",
		"???",
		"TKIP IV value exceeds thresh"
	};
#endif
	acx_read_info_status(priv);
	acxlog(L_IRQ, "got Info IRQ: status 0x%04X type 0x%04X: %s\n",
		priv->info_status, priv->info_type,
		info_type_msg[(priv->info_type >= VEC_SIZE(info_type_msg)) ?
				0 : priv->info_type]
	);
}


/*----------------------------------------------------------------
* acx_i_interrupt
*
* IRQ handler (atomic context, must not sleep, blah, blah)
*----------------------------------------------------------------*/
static void
acx_log_unusual_irq(u16 irqtype) {
	/*
	if (!printk_ratelimit())
		return;
	*/

	printk("acx: got");
	if (irqtype & HOST_INT_RX_DATA) {
		printk(" Rx_Data");
	}
		/* HOST_INT_TX_COMPLETE   */
	if (irqtype & HOST_INT_TX_XFER) {
		printk(" Tx_Xfer");
	}
		/* HOST_INT_RX_COMPLETE   */
	if (irqtype & HOST_INT_DTIM) {
		printk(" DTIM");
	}
	if (irqtype & HOST_INT_BEACON) {
		printk(" Beacon");
	}
	if (irqtype & HOST_INT_TIMER) {
		acxlog(L_IRQ, " Timer");
	}
	if (irqtype & HOST_INT_KEY_NOT_FOUND) {
		printk(" Key_Not_Found");
	}
	if (irqtype & HOST_INT_IV_ICV_FAILURE) {
		printk(" IV_ICV_Failure");
	}
		/* HOST_INT_CMD_COMPLETE  */
		/* HOST_INT_INFO          */
	if (irqtype & HOST_INT_OVERFLOW) {
		printk(" Overflow");
	}
	if (irqtype & HOST_INT_PROCESS_ERROR) {
		printk(" Process_Error");
	}
		/* HOST_INT_SCAN_COMPLETE */
	if (irqtype & HOST_INT_FCS_THRESHOLD) {
		printk(" FCS_Threshold");
	}
	if (irqtype & HOST_INT_UNKNOWN) {
		printk(" Unknown");
	}
	printk(" IRQ(s)\n");
}

static irqreturn_t
acx_i_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	wlandevice_t *priv;
	unsigned long flags;
	unsigned int irqcount = MAX_IRQLOOPS_PER_JIFFY;
	u16 irqtype, unmasked;

	priv = (wlandevice_t *) (((netdevice_t *) dev_id)->priv);

	/* LOCKING: can just spin_lock() since IRQs are disabled anyway.
	 * I am paranoid */
	acx_lock(priv, flags);

	unmasked = acx_read_reg16(priv, IO_ACX_IRQ_STATUS_CLEAR);
	if (unlikely(0xffff == unmasked)) {
		/* 0xffff value hints at missing hardware,
		 * so don't do anything.
		 * FIXME: that's not very clean - maybe we are able to
		 * establish a flag which definitely tells us that some
		 * hardware is absent and which we could check here?
		 * Hmm, but other drivers do the very same thing... */
		acxlog(L_IRQ, "IRQ type:FFFF - device removed? IRQ_NONE\n");
		goto none;
	}

	/* We will check only "interesting" IRQ types */
	irqtype = unmasked & ~priv->irq_mask;
	if (!irqtype) {
		/* We are on a shared IRQ line and it wasn't our IRQ */
		acxlog(L_IRQ, "IRQ type:%04X, mask:%04X - all are masked, IRQ_NONE\n",
			unmasked, priv->irq_mask);
		goto none;
	}

	/* Done here because IRQ_NONEs taking three lines of log
	** drive me crazy */
	FN_ENTER;

#define IRQ_ITERATE 1
#if IRQ_ITERATE
if (jiffies != priv->irq_last_jiffies) {
	priv->irq_loops_this_jiffy = 0;
	priv->irq_last_jiffies = jiffies;
}

/* safety condition; we'll normally abort loop below
 * in case no IRQ type occurred */
while (--irqcount) {
#endif
	/* ACK all IRQs asap */
	acx_write_reg16(priv, IO_ACX_IRQ_ACK, 0xffff);

	acxlog(L_IRQ, "IRQ type:%04X, mask:%04X, type & ~mask:%04X\n",
				unmasked, priv->irq_mask, irqtype);

	/* Handle most important IRQ types first */
	if (irqtype & HOST_INT_RX_COMPLETE) {
		acxlog(L_IRQ, "got Rx_Complete IRQ\n");
		acx_l_process_rx_desc(priv);
	}
	if (irqtype & HOST_INT_TX_COMPLETE) {
		acxlog(L_IRQ, "got Tx_Complete IRQ\n");
		/* don't clean up on each Tx complete, wait a bit
		 * unless we're going towards full, in which case
		 * we do it immediately, too (otherwise we might lockup
		 * with a full Tx buffer if we go into
		 * acx_l_clean_tx_desc() at a time when we won't wakeup
		 * the net queue in there for some reason...) */
		if (priv->tx_free <= TX_START_CLEAN) {
#if TX_CLEANUP_IN_SOFTIRQ
			acx_schedule_after_interrupt_task(priv, ACX_AFTER_IRQ_TX_CLEANUP);
#else
			acx_l_clean_tx_desc(priv);
#endif
		}
	}

	/* Less frequent ones */
	if (irqtype & (0
		| HOST_INT_CMD_COMPLETE
		| HOST_INT_INFO
		| HOST_INT_SCAN_COMPLETE
	)) {
		if (irqtype & HOST_INT_CMD_COMPLETE) {
			acxlog(L_IRQ, "got Command_Complete IRQ\n");
			/* save the state for the running issue_cmd() */
			SET_BIT(priv->irq_status, HOST_INT_CMD_COMPLETE);
		}
		if (irqtype & HOST_INT_INFO) {
			acx_l_handle_info_irq(priv);
		}
		if (irqtype & HOST_INT_SCAN_COMPLETE) {
			acxlog(L_IRQ, "got Scan_Complete IRQ\n");
			/* need to do that in process context */
			acx_schedule_after_interrupt_task(priv, ACX_AFTER_IRQ_COMPLETE_SCAN);
			/* remember that fw is not scanning anymore */
			SET_BIT(priv->irq_status, HOST_INT_SCAN_COMPLETE);
		}
	}

	/* These we just log, but either they happen rarely
	 * or we keep them masked out */
	if (irqtype & (0
		| HOST_INT_RX_DATA
		/* | HOST_INT_TX_COMPLETE   */
		| HOST_INT_TX_XFER
		/* | HOST_INT_RX_COMPLETE   */
		| HOST_INT_DTIM
		| HOST_INT_BEACON
		| HOST_INT_TIMER
		| HOST_INT_KEY_NOT_FOUND
		| HOST_INT_IV_ICV_FAILURE
		/* | HOST_INT_CMD_COMPLETE  */
		/* | HOST_INT_INFO          */
		| HOST_INT_OVERFLOW
		| HOST_INT_PROCESS_ERROR
		/* | HOST_INT_SCAN_COMPLETE */
		| HOST_INT_FCS_THRESHOLD
		| HOST_INT_UNKNOWN
	)) {
		acx_log_unusual_irq(irqtype);
	}

#if IRQ_ITERATE
	unmasked = acx_read_reg16(priv, IO_ACX_IRQ_STATUS_CLEAR);
	irqtype = unmasked & ~priv->irq_mask;
	/* Bail out if no new IRQ bits or if all are masked out */
	if (!irqtype)
		break;

	if (unlikely(++priv->irq_loops_this_jiffy > MAX_IRQLOOPS_PER_JIFFY)) {
		printk(KERN_ERR "acx: too many interrupts per jiffy!\n");
		/* Looks like card floods us with IRQs! Try to stop that */
		acx_write_reg16(priv, IO_ACX_IRQ_MASK, 0xffff);
		/* This will short-circuit all future attempts to handle IRQ.
		 * We cant do much more... */
		priv->irq_mask = 0;
		break;
	}
}
#endif
	/* Routine to perform blink with range */
	if (unlikely(priv->led_power == 2))
		acx_l_update_link_quality_led(priv);

/* handled: */
	/* acx_write_flush(priv); - not needed, last op was read anyway */
	acx_unlock(priv, flags);
	FN_EXIT0;
	return IRQ_HANDLED;

none:
	acx_unlock(priv, flags);
	return IRQ_NONE;
}


/*----------------------------------------------------------------
* acx_l_power_led
*----------------------------------------------------------------*/
void
acx_l_power_led(wlandevice_t *priv, int enable)
{
	u16 gpio_pled =	IS_ACX111(priv) ? 0x0040 : 0x0800;

	/* A hack. Not moving message rate limiting to priv->xxx
	 * (it's only a debug message after all) */
	static int rate_limit = 0;

	if (rate_limit++ < 3)
		acxlog(L_IOCTL, "Please report in case toggling the power "
				"LED doesn't work for your card!\n");
	if (enable)
		acx_write_reg16(priv, IO_ACX_GPIO_OUT,
			acx_read_reg16(priv, IO_ACX_GPIO_OUT) & ~gpio_pled);
	else
		acx_write_reg16(priv, IO_ACX_GPIO_OUT,
			acx_read_reg16(priv, IO_ACX_GPIO_OUT) | gpio_pled);
}


/***********************************************************************
** Ioctls
*/

/***********************************************************************
*/
int
acx111pci_ioctl_info(
	struct net_device *dev,
	struct iw_request_info *info,
	struct iw_param *vwrq,
	char *extra)
{
#if ACX_DEBUG
	wlandevice_t *priv = netdev_priv(dev);
	rxdesc_t *rxdesc;
	txdesc_t *txdesc;
	rxhostdesc_t *rxhostdesc;
	txhostdesc_t *txhostdesc;
	struct acx111_ie_memoryconfig memconf;
	struct acx111_ie_queueconfig queueconf;
	unsigned long flags;
	int i;
	char memmap[0x34];
	char rxconfig[0x8];
	char fcserror[0x8];
	char ratefallback[0x5];

	if ( !(acx_debug & (L_IOCTL|L_DEBUG)) )
		return OK;
	/* using printk() since we checked debug flag already */

	acx_sem_lock(priv);

	if (!IS_ACX111(priv)) {
		printk("acx111-specific function called "
			"with non-acx111 chip, aborting\n");
		goto end_ok;
	}

	/* get Acx111 Memory Configuration */
	memset(&memconf, 0, sizeof(memconf));
	/* BTW, fails with 12 (Write only) error code.
	** Retained for easy testing of issue_cmd error handling :) */
	acx_s_interrogate(priv, &memconf, ACX1xx_IE_QUEUE_CONFIG);

	/* get Acx111 Queue Configuration */
	memset(&queueconf, 0, sizeof(queueconf));
	acx_s_interrogate(priv, &queueconf, ACX1xx_IE_MEMORY_CONFIG_OPTIONS);

	/* get Acx111 Memory Map */
	memset(memmap, 0, sizeof(memmap));
	acx_s_interrogate(priv, &memmap, ACX1xx_IE_MEMORY_MAP);

	/* get Acx111 Rx Config */
	memset(rxconfig, 0, sizeof(rxconfig));
	acx_s_interrogate(priv, &rxconfig, ACX1xx_IE_RXCONFIG);

	/* get Acx111 fcs error count */
	memset(fcserror, 0, sizeof(fcserror));
	acx_s_interrogate(priv, &fcserror, ACX1xx_IE_FCS_ERROR_COUNT);

	/* get Acx111 rate fallback */
	memset(ratefallback, 0, sizeof(ratefallback));
	acx_s_interrogate(priv, &ratefallback, ACX1xx_IE_RATE_FALLBACK);

	/* force occurrence of a beacon interrupt */
	/* TODO: comment why is this necessary */
	acx_write_reg16(priv, IO_ACX_HINT_TRIG, HOST_INT_BEACON);

	/* dump Acx111 Mem Configuration */
	printk("dump mem config:\n"
		"data read: %d, struct size: %d\n"
		"Number of stations: %1X\n"
		"Memory block size: %1X\n"
		"tx/rx memory block allocation: %1X\n"
		"count rx: %X / tx: %X queues\n"
		"options %1X\n"
		"fragmentation %1X\n"
		"Rx Queue 1 Count Descriptors: %X\n"
		"Rx Queue 1 Host Memory Start: %X\n"
		"Tx Queue 1 Count Descriptors: %X\n"
		"Tx Queue 1 Attributes: %X\n",
		memconf.len, (int) sizeof(memconf),
		memconf.no_of_stations,
		memconf.memory_block_size,
		memconf.tx_rx_memory_block_allocation,
		memconf.count_rx_queues, memconf.count_tx_queues,
		memconf.options,
		memconf.fragmentation,
		memconf.rx_queue1_count_descs,
	acx2cpu(memconf.rx_queue1_host_rx_start),
		memconf.tx_queue1_count_descs,
		memconf.tx_queue1_attributes);

	/* dump Acx111 Queue Configuration */
	printk("dump queue head:\n"
		"data read: %d, struct size: %d\n"
		"tx_memory_block_address (from card): %X\n"
		"rx_memory_block_address (from card): %X\n"
		"rx1_queue address (from card): %X\n"
		"tx1_queue address (from card): %X\n"
		"tx1_queue attributes (from card): %X\n",
		queueconf.len, (int) sizeof(queueconf),
		queueconf.tx_memory_block_address,
		queueconf.rx_memory_block_address,
		queueconf.rx1_queue_address,
		queueconf.tx1_queue_address,
		queueconf.tx1_attributes);

	/* dump Acx111 Mem Map */
	printk("dump mem map:\n"
		"data read: %d, struct size: %d\n"
		"Code start: %X\n"
		"Code end: %X\n"
		"WEP default key start: %X\n"
		"WEP default key end: %X\n"
		"STA table start: %X\n"
		"STA table end: %X\n"
		"Packet template start: %X\n"
		"Packet template end: %X\n"
		"Queue memory start: %X\n"
		"Queue memory end: %X\n"
		"Packet memory pool start: %X\n"
		"Packet memory pool end: %X\n"
		"iobase: %p\n"
		"iobase2: %p\n",
		*((u16 *)&memmap[0x02]), (int) sizeof(memmap),
		*((u32 *)&memmap[0x04]),
		*((u32 *)&memmap[0x08]),
		*((u32 *)&memmap[0x0C]),
		*((u32 *)&memmap[0x10]),
		*((u32 *)&memmap[0x14]),
		*((u32 *)&memmap[0x18]),
		*((u32 *)&memmap[0x1C]),
		*((u32 *)&memmap[0x20]),
		*((u32 *)&memmap[0x24]),
		*((u32 *)&memmap[0x28]),
		*((u32 *)&memmap[0x2C]),
		*((u32 *)&memmap[0x30]),
		priv->iobase,
		priv->iobase2);

	/* dump Acx111 Rx Config */
	printk("dump rx config:\n"
		"data read: %d, struct size: %d\n"
		"rx config: %X\n"
		"rx filter config: %X\n",
		*((u16 *)&rxconfig[0x02]), (int) sizeof(rxconfig),
		*((u16 *)&rxconfig[0x04]),
		*((u16 *)&rxconfig[0x06]));

	/* dump Acx111 fcs error */
	printk("dump fcserror:\n"
		"data read: %d, struct size: %d\n"
		"fcserrors: %X\n",
		*((u16 *)&fcserror[0x02]), (int) sizeof(fcserror),
		*((u32 *)&fcserror[0x04]));

	/* dump Acx111 rate fallback */
	printk("dump rate fallback:\n"
		"data read: %d, struct size: %d\n"
		"ratefallback: %X\n",
		*((u16 *)&ratefallback[0x02]), (int) sizeof(ratefallback),
		*((u8 *)&ratefallback[0x04]));

	/* protect against IRQ */
	acx_lock(priv, flags);

	/* dump acx111 internal rx descriptor ring buffer */
	rxdesc = priv->rxdesc_start;

	/* loop over complete receive pool */
	if (rxdesc) for (i = 0; i < RX_CNT; i++) {
		printk("\ndump internal rxdesc %d:\n"
			"mem pos %p\n"
			"next 0x%X\n"
			"acx mem pointer (dynamic) 0x%X\n"
			"CTL (dynamic) 0x%X\n"
			"Rate (dynamic) 0x%X\n"
			"RxStatus (dynamic) 0x%X\n"
			"Mod/Pre (dynamic) 0x%X\n",
			i,
			rxdesc,
			acx2cpu(rxdesc->pNextDesc),
			acx2cpu(rxdesc->ACXMemPtr),
			rxdesc->Ctl_8,
			rxdesc->rate,
			rxdesc->error,
			rxdesc->SNR);
		rxdesc++;
	}

	/* dump host rx descriptor ring buffer */

	rxhostdesc = priv->rxhostdesc_start;

	/* loop over complete receive pool */
	if (rxhostdesc) for (i = 0; i < RX_CNT; i++) {
		printk("\ndump host rxdesc %d:\n"
			"mem pos %p\n"
			"buffer mem pos 0x%X\n"
			"buffer mem offset 0x%X\n"
			"CTL 0x%X\n"
			"Length 0x%X\n"
			"next 0x%X\n"
			"Status 0x%X\n",
			i,
			rxhostdesc,
			acx2cpu(rxhostdesc->data_phy),
			rxhostdesc->data_offset,
			le16_to_cpu(rxhostdesc->Ctl_16),
			le16_to_cpu(rxhostdesc->length),
			acx2cpu(rxhostdesc->desc_phy_next),
			rxhostdesc->Status);
		rxhostdesc++;
	}

	/* dump acx111 internal tx descriptor ring buffer */
	txdesc = priv->txdesc_start;

	/* loop over complete transmit pool */
	if (txdesc) for (i = 0; i < TX_CNT; i++) {
		printk("\ndump internal txdesc %d:\n"
			"size 0x%X\n"
			"mem pos %p\n"
			"next 0x%X\n"
			"acx mem pointer (dynamic) 0x%X\n"
			"host mem pointer (dynamic) 0x%X\n"
			"length (dynamic) 0x%X\n"
			"CTL (dynamic) 0x%X\n"
			"CTL2 (dynamic) 0x%X\n"
			"Status (dynamic) 0x%X\n"
			"Rate (dynamic) 0x%X\n",
			i,
			(int) sizeof(struct txdesc),
			txdesc,
			acx2cpu(txdesc->pNextDesc),
			acx2cpu(txdesc->AcxMemPtr),
			acx2cpu(txdesc->HostMemPtr),
			le16_to_cpu(txdesc->total_length),
			txdesc->Ctl_8,
			txdesc->Ctl2_8, txdesc->error,
			txdesc->u.r1.rate);
		txdesc = move_txdesc(priv, txdesc, 1);
	}

	/* dump host tx descriptor ring buffer */

	txhostdesc = priv->txhostdesc_start;

	/* loop over complete host send pool */
	if (txhostdesc) for (i = 0; i < TX_CNT * 2; i++) {
		printk("\ndump host txdesc %d:\n"
			"mem pos %p\n"
			"buffer mem pos 0x%X\n"
			"buffer mem offset 0x%X\n"
			"CTL 0x%X\n"
			"Length 0x%X\n"
			"next 0x%X\n"
			"Status 0x%X\n",
			i,
			txhostdesc,
			acx2cpu(txhostdesc->data_phy),
			txhostdesc->data_offset,
			le16_to_cpu(txhostdesc->Ctl_16),
			le16_to_cpu(txhostdesc->length),
			acx2cpu(txhostdesc->desc_phy_next),
			le32_to_cpu(txhostdesc->Status));
		txhostdesc++;
	}

	/* acx_write_reg16(priv, 0xb4, 0x4); */

	acx_unlock(priv, flags);
end_ok:

	acx_sem_unlock(priv);
#endif /* ACX_DEBUG */
	return OK;
}


/***********************************************************************
*/
int
acx100pci_ioctl_set_phy_amp_bias(
	struct net_device *dev,
	struct iw_request_info *info,
	struct iw_param *vwrq,
	char *extra)
{
	wlandevice_t *priv = netdev_priv(dev);
	unsigned long flags;
	u16 gpio_old;

	if (!IS_ACX100(priv)) {
		/* WARNING!!!
		 * Removing this check *might* damage
		 * hardware, since we're tweaking GPIOs here after all!!!
		 * You've been warned...
		 * WARNING!!! */
		printk("acx: sorry, setting bias level for non-acx100 "
			"is not supported yet\n");
		return OK;
	}

	if (*extra > 7)	{
		printk("acx: invalid bias parameter, range is 0-7\n");
		return -EINVAL;
	}

	acx_sem_lock(priv);

	/* Need to lock accesses to [IO_ACX_GPIO_OUT]:
	 * IRQ handler uses it to update LED */
	acx_lock(priv, flags);
	gpio_old = acx_read_reg16(priv, IO_ACX_GPIO_OUT);
	acx_write_reg16(priv, IO_ACX_GPIO_OUT, (gpio_old & 0xf8ff) | ((u16)*extra << 8));
	acx_unlock(priv, flags);

	acxlog(L_DEBUG, "gpio_old: 0x%04X\n", gpio_old);
	printk("%s: PHY power amplifier bias: old:%d, new:%d\n",
		dev->name,
		(gpio_old & 0x0700) >> 8, (unsigned char)*extra);

	acx_sem_unlock(priv);

	return OK;
}


/***************************************************************
** acxpci_l_alloc_tx
** Actually returns a txdesc_t* ptr
*/
tx_t*
acxpci_l_alloc_tx(wlandevice_t* priv)
{
	struct txdesc *txdesc;
	u8 ctl8;

	FN_ENTER;

	txdesc = get_txdesc(priv, priv->tx_head);
	ctl8 = txdesc->Ctl_8;
	if (unlikely(DESC_CTL_HOSTOWN != (ctl8 & DESC_CTL_DONE))) {
		/* whoops, descr at current index is not free, so probably
		 * ring buffer already full */
		/* FIXME: this causes a deadlock situation (endless
		 * loop) in case the current descriptor remains busy,
		 * so handle it a bit better in the future!! */
		printk("acx: BUG: tx_head->Ctl8=0x%02X, (0x%02X & "
			"0x"DESC_CTL_DONE_STR") != 0x"DESC_CTL_HOSTOWN_STR
			": failed to find free tx descr\n",
			ctl8, ctl8);
		txdesc = NULL;
		goto end;
	}

	priv->tx_free--;
	acxlog(L_BUFT, "tx: got desc %u, %u remain\n",
			priv->tx_head, priv->tx_free);

/*
 * This comment is probably not entirely correct, needs further discussion
 * (restored commented-out code below to fix Tx ring buffer overflow,
 * since it's much better to have a slightly less efficiently used ring
 * buffer rather than one which easily overflows):
 *
 * This doesn't do anything other than limit our maximum number of
 * buffers used at a single time (we might as well just declare
 * TX_STOP_QUEUE less descriptors when we open up.) We should just let it
 * slide here, and back off TX_STOP_QUEUE in acx_l_clean_tx_desc, when given the
 * opportunity to let the queue start back up.
 */
	if (priv->tx_free < TX_STOP_QUEUE) {
		acxlog(L_BUF, "stop queue (%u tx desc left)\n",
				priv->tx_free);
		acx_stop_queue(priv->netdev, NULL);
	}

	/* returning current descriptor, so advance to next free one */
	priv->tx_head = (priv->tx_head + 1) % TX_CNT;
end:
	FN_EXIT0;

	return (tx_t*)txdesc;
}


/***********************************************************************
*/
void*
acxpci_l_get_txbuf(wlandevice_t *priv, tx_t* tx_opaque)
{
	return acx_get_txhostdesc(priv, (txdesc_t*)tx_opaque)->data;
}


/***********************************************************************
** acxpci_l_tx_data
**
** Can be called from IRQ (rx -> (AP bridging or mgmt response) -> tx).
** Can be called from acx_i_start_xmit (data frames from net core).
*/
void
acxpci_l_tx_data(wlandevice_t *priv, tx_t* tx_opaque, int len)
{
	txdesc_t *txdesc = (txdesc_t*)tx_opaque;
	txhostdesc_t *hostdesc1, *hostdesc2;
	client_t *clt;
	u8 Ctl_8, Ctl2_8;

	FN_ENTER;

	/* fw doesn't tx such packets anyhow */
	if (len < WLAN_HDR_A3_LEN)
		goto end;

	hostdesc1 = acx_get_txhostdesc(priv, txdesc);
	hostdesc2 = hostdesc1 + 1;

	/* modify flag status in separate variable to be able to write it back
	 * in one big swoop later (also in order to have less device memory
	 * accesses) */
	Ctl_8 = txdesc->Ctl_8;
	Ctl2_8 = txdesc->Ctl2_8;

	/* DON'T simply set Ctl field to 0 here globally,
	 * it needs to maintain a consistent flag status (those are state flags!!),
	 * otherwise it may lead to severe disruption. Only set or reset particular
	 * flags at the exact moment this is needed...
	 * FIXME: what about Ctl2? Equally problematic? */

	/* let chip do RTS/CTS handshaking before sending
	 * in case packet size exceeds threshold */
	if (len > priv->rts_threshold)
		SET_BIT(Ctl2_8, DESC_CTL2_RTS);
	else
		CLEAR_BIT(Ctl2_8, DESC_CTL2_RTS);

#ifdef DEBUG_WEP
	if (priv->wep_enabled)
		SET_BIT(Ctl2_8, DESC_CTL2_WEP);
	else
		CLEAR_BIT(Ctl2_8, DESC_CTL2_WEP);
#endif

	switch (priv->mode) {
	case ACX_MODE_0_ADHOC:
	case ACX_MODE_3_AP:
		clt = acx_l_sta_list_get(priv, ((wlan_hdr_t*)hostdesc1->data)->a1);
		break;
	case ACX_MODE_2_STA:
		clt = priv->ap_client;
		break;
#if 0
/* testing was done on acx111: */
	case ACX_MODE_MONITOR:
		SET_BIT(Ctl2_8, 0
/* sends CTS to self before packet */
			+ DESC_CTL2_SEQ		/* don't increase sequence field */
/* not working (looks like good fcs is still added) */
			+ DESC_CTL2_FCS		/* don't add the FCS */
/* not tested */
			+ DESC_CTL2_MORE_FRAG	
/* not tested */
			+ DESC_CTL2_RETRY	/* don't increase retry field */
/* not tested */
			+ DESC_CTL2_POWER	/* don't increase power mgmt. field */
/* no effect */
			+ DESC_CTL2_WEP		/* encrypt this frame */
/* not tested */
			+ DESC_CTL2_DUR		/* don't increase duration field */
			);
		/* fallthrough */
#endif
	default: /* ACX_MODE_OFF, ACX_MODE_MONITOR */
		clt = NULL;
		break;
	}

	if (unlikely(clt && !clt->rate_cur)) {
		printk("acx: driver bug! bad ratemask\n");
		goto end;
	}

	/* used in tx cleanup routine for auto rate and accounting: */
	acx_put_txc(priv, txdesc, clt);

	txdesc->total_length = cpu_to_le16(len);
	hostdesc2->length = cpu_to_le16(len - WLAN_HDR_A3_LEN);
	if (IS_ACX111(priv)) {
		u16 rate_cur = clt ? clt->rate_cur : priv->rate_bcast;
		/* note that if !txdesc->do_auto, txrate->cur
		** has only one nonzero bit */
		txdesc->u.r2.rate111 = cpu_to_le16(
			rate_cur
			/* WARNING: I was never able to make it work with prism54 AP.
			** It was falling down to 1Mbit where shortpre is not applicable,
			** and not working at all at "5,11 basic rates only" setting.
			** I even didn't see tx packets in radio packet capture.
			** Disabled for now --vda */
			/*| ((clt->shortpre && clt->cur!=RATE111_1) ? RATE111_SHORTPRE : 0) */
			);
#ifdef TODO_FIGURE_OUT_WHEN_TO_SET_THIS
			/* should add this to rate111 above as necessary */
			| (clt->pbcc511 ? RATE111_PBCC511 : 0)
#endif
		hostdesc1->length = cpu_to_le16(len);
	} else { /* ACX100 */
		u8 rate_100 = clt ? clt->rate_100 : priv->rate_bcast100;
		txdesc->u.r1.rate = rate_100;
#ifdef TODO_FIGURE_OUT_WHEN_TO_SET_THIS
		if (clt->pbcc511) {
			if (n == RATE100_5 || n == RATE100_11)
				n |= RATE100_PBCC511;
		}

		if (clt->shortpre && (clt->cur != RATE111_1))
			SET_BIT(Ctl_8, DESC_CTL_SHORT_PREAMBLE); /* set Short Preamble */
#endif
		/* set autodma and reclaim and 1st mpdu */
		SET_BIT(Ctl_8, DESC_CTL_AUTODMA | DESC_CTL_RECLAIM | DESC_CTL_FIRSTFRAG);
		hostdesc1->length = cpu_to_le16(WLAN_HDR_A3_LEN);
	}
	/* don't need to clean ack/rts statistics here, already
	 * done on descr cleanup */

	/* clears Ctl DESC_CTL_HOSTOWN bit, thus telling that the descriptors
	 * are now owned by the acx100; do this as LAST operation */
	CLEAR_BIT(Ctl_8, DESC_CTL_HOSTOWN);
	/* flush writes before we release hostdesc to the adapter here */
	wmb();
	CLEAR_BIT(hostdesc1->Ctl_16, cpu_to_le16(DESC_CTL_HOSTOWN));
	CLEAR_BIT(hostdesc2->Ctl_16, cpu_to_le16(DESC_CTL_HOSTOWN));

	/* write back modified flags */
	txdesc->Ctl2_8 = Ctl2_8;
	txdesc->Ctl_8 = Ctl_8;

	/* unused: txdesc->tx_time = cpu_to_le32(jiffies); */
//TODO: should it be a mmiowb() instead? we are protecting against race with write[bwl]()
	/* flush writes before we tell the adapter that it's its turn now */
	wmb(); 
	acx_write_reg16(priv, IO_ACX_INT_TRIG, INT_TRIG_TXPRC);
	acx_write_flush(priv);

	/* log the packet content AFTER sending it,
	 * in order to not delay sending any further than absolutely needed
	 * Do separate logs for acx100/111 to have human-readable rates */
	if (unlikely(acx_debug & (L_XFER|L_DATA))) {
		u16 fc = ((wlan_hdr_t*)hostdesc1->data)->fc;
		if (IS_ACX111(priv))
			printk("tx: pkt (%s): len %d "
				"rate %04X%s status %u\n",
				acx_get_packet_type_string(le16_to_cpu(fc)), len,
				le16_to_cpu(txdesc->u.r2.rate111),
				(le16_to_cpu(txdesc->u.r2.rate111) & RATE111_SHORTPRE) ? "(SPr)" : "",
				priv->status);
		else
			printk("tx: pkt (%s): len %d rate %03u%s status %u\n",
				acx_get_packet_type_string(fc), len,
				txdesc->u.r1.rate,
				(Ctl_8 & DESC_CTL_SHORT_PREAMBLE) ? "(SPr)" : "",
				priv->status);

		if (acx_debug & L_DATA) {
			printk("tx: 802.11 [%d]: ", len);
			acx_dump_bytes(hostdesc1->data, len);
		}
	}
end:
	FN_EXIT0;
}


/***********************************************************************
*/
static void
acx_l_handle_tx_error(wlandevice_t *priv, u8 error, unsigned int finger)
{
	const char *err = "unknown error";

	/* hmm, should we handle this as a mask
	 * of *several* bits?
	 * For now I think only caring about
	 * individual bits is ok... */
	switch (error) {
	case 0x01:
		err = "no Tx due to error in other fragment";
		priv->wstats.discard.fragment++;
		break;
	case 0x02:
		err = "Tx aborted";
		priv->stats.tx_aborted_errors++;
		break;
	case 0x04:
		err = "Tx desc wrong parameters";
		priv->wstats.discard.misc++;
		break;
	case 0x08:
		err = "WEP key not found";
		priv->wstats.discard.misc++;
		break;
	case 0x10:
		err = "MSDU lifetime timeout? - try changing "
				"'iwconfig retry lifetime XXX'";
		priv->wstats.discard.misc++;
		break;
	case 0x20:
		err = "excessive Tx retries due to either distance "
			"too high or unable to Tx or Tx frame error - "
			"try changing 'iwconfig txpower XXX' or "
			"'sens'itivity or 'retry'";
		priv->wstats.discard.retries++;
		/* FIXME: set (GETSET_TX|GETSET_RX) here
		 * (this seems to recalib radio on ACX100)
		 * after some more jiffies passed??
		 * But OTOH Tx error 0x20 also seems to occur on
		 * overheating, so I'm not sure whether we
		 * actually want that, since people maybe won't notice
		 * then that their hardware is slowly getting
		 * cooked...
		 * Or is it still a safe long distance from utter
		 * radio non-functionality despite many radio
		 * recalibs
		 * to final destructive overheating of the hardware?
		 * In this case we really should do recalib here...
		 * I guess the only way to find out is to do a
		 * potentially fatal self-experiment :-\
		 * Or maybe only recalib in case we're using Tx
		 * rate auto (on errors switching to lower speed
		 * --> less heat?) or 802.11 power save mode? */

		/* ok, just do it.
		 * ENABLE_TX|ENABLE_RX helps, so even do
		 * DISABLE_TX and DISABLE_RX in order to perhaps
		 * have more impact. */
		if (++priv->retry_errors_msg_ratelimit % 4 == 0) {
			if (priv->retry_errors_msg_ratelimit <= 20)
				printk("%s: several excessive Tx "
					"retry errors occurred, attempting "
					"to recalibrate radio. Radio "
					"drift might be caused by increasing "
					"card temperature, please check the card "
					"before it's too late!\n",
					priv->netdev->name);
			if (priv->retry_errors_msg_ratelimit == 20)
				printk("disabling above "
					"notification message\n");

			acx_schedule_after_interrupt_task(priv, ACX_AFTER_IRQ_CMD_RADIO_RECALIB);
		}
		break;
	case 0x40:
		err = "Tx buffer overflow";
		priv->stats.tx_fifo_errors++;
		break;
	case 0x80:
		err = "DMA error";
		priv->wstats.discard.misc++;
		break;
	}
	priv->stats.tx_errors++;
	if (priv->stats.tx_errors <= 20)
		printk("%s: tx error 0x%02X, buf %02u! (%s)\n",
				priv->netdev->name, error, finger, err);
	else
		printk("%s: tx error 0x%02X, buf %02u!\n",
				priv->netdev->name, error, finger);
}


/***********************************************************************
*/
/* Theory of operation:
** client->rate_cap is a bitmask of rates client is capable of.
** client->rate_cfg is a bitmask of allowed (configured) rates.
** It is set as a result of iwconfig rate N [auto]
** or iwpriv set_rates "N,N,N N,N,N" commands.
** It can be fixed (e.g. 0x0080 == 18Mbit only),
** auto (0x00ff == 18Mbit or any lower value),
** and code handles any bitmask (0x1081 == try 54Mbit,18Mbit,1Mbit _only_).
**
** client->rate_cur is a value for rate111 field in tx descriptor.
** It is always set to txrate_cfg sans zero or more most significant
** bits. This routine handles selection of new rate_cur value depending on
** outcome of last tx event.
**
** client->rate_100 is a precalculated rate value for acx100
** (we can do without it, but will need to calculate it on each tx).
**
** You cannot configure mixed usage of 5.5 and/or 11Mbit rate
** with PBCC and CCK modulation. Either both at CCK or both at PBCC.
** In theory you can implement it, but so far it is considered not worth doing.
**
** 22Mbit, of course, is PBCC always. */

/* maps acx100 tx descr rate field to acx111 one */
static u16
rate100to111(u8 r)
{
	switch (r) {
	case RATE100_1:	return RATE111_1;
	case RATE100_2:	return RATE111_2;
	case RATE100_5:
	case (RATE100_5 | RATE100_PBCC511):	return RATE111_5;
	case RATE100_11:
	case (RATE100_11 | RATE100_PBCC511):	return RATE111_11;
	case RATE100_22:	return RATE111_22;
	default:
		printk("acx: unexpected acx100 txrate: %u! "
			"Please report\n", r);
		return RATE111_2;
	}
}


static void
acx_l_handle_txrate_auto(wlandevice_t *priv, struct client *txc,
			unsigned int idx, u8 rate100, u16 rate111, u8 error)
{
	u16 sent_rate;
	u16 cur = txc->rate_cur;
	int slower_rate_was_used;

	/* FIXME: need to implement some kind of rate success memory
	 * which stores the success percentage per rate, to be taken
	 * into account when considering allowing a new rate, since it
	 * doesn't really help to stupidly count fallback/stepup,
	 * since one invalid rate will spoil the party anyway
	 * (such as 22M in case of 11M-only peers) */

	/* vda: hmm. current code will do this:
	** 1. send packets at 11 Mbit, stepup++
	** 2. will try to send at 22Mbit. hardware will see no ACK,
	**    retries at 11Mbit, success. code notes that used rate
	**    is lower. stepup = 0, fallback++
	** 3. repeat step 2 fallback_count times. Fall back to
	**    11Mbit. go to step 1.
	** If stepup_count is large (say, 16) and fallback_count
	** is small (3), this wouldn't be too bad wrt throughput */

	/* do some preparations, i.e. calculate the one rate that was
	 * used to send this packet */
	if (IS_ACX111(priv)) {
		sent_rate = 1 << highest_bit(rate111 & RATE111_ALL);
	} else {
		sent_rate = rate100to111(rate100);
	}
	/* sent_rate has only one bit set now, corresponding to tx rate
	 * which was used by hardware to tx this particular packet */

	/* now do the actual auto rate management */
	acxlog(L_DEBUG, "tx: %sclient=%p/"MACSTR" used=%04X cur=%04X cfg=%04X "
		"__=%u/%u ^^=%u/%u\n",
		(txc->ignore_count > 0) ? "[IGN] " : "",
		txc, MAC(txc->address), sent_rate, cur, txc->rate_cfg,
		txc->fallback_count, priv->fallback_threshold,
		txc->stepup_count, priv->stepup_threshold
	);

	/* we need to ignore old packets already in the tx queue since
	 * they use older rate bytes configured before our last rate change,
	 * otherwise our mechanism will get confused by interpreting old data.
	 * Do it here only, in order to have the logging above */
	if (txc->ignore_count) {
		txc->ignore_count--;
		return;
	}

	/* true only if the only nonzero bit in sent_rate is
	** less significant than highest nonzero bit in cur */
	slower_rate_was_used = ( cur > ((sent_rate<<1)-1) );

	if (slower_rate_was_used || (error & 0x30)) {
		txc->stepup_count = 0;
		if (++txc->fallback_count <= priv->fallback_threshold)
			return;
		txc->fallback_count = 0;

		/* clear highest 1 bit in cur */
		sent_rate = RATE111_54;
		while (!(cur & sent_rate)) sent_rate >>= 1;
		CLEAR_BIT(cur, sent_rate);

		if (cur) { /* we can't disable all rates! */
			acxlog(L_XFER, "tx: falling back to ratemask %04X\n", cur);
			txc->rate_cur = cur;
			txc->ignore_count = TX_CNT - priv->tx_free;
		}
	} else if (!slower_rate_was_used) {
		txc->fallback_count = 0;
		if (++txc->stepup_count <= priv->stepup_threshold)
			return;
		txc->stepup_count = 0;

		/* sanitize. Sort of not needed, but I dont trust hw that much...
		** what if it can report bogus tx rates sometimes? */
		while (!(cur & sent_rate)) sent_rate >>= 1;
		/* try to find a higher sent_rate that isn't yet in our
		 * current set, but is an allowed cfg */
		while (1) {
			sent_rate <<= 1;
			if (sent_rate > txc->rate_cfg)
				/* no higher rates allowed by config */
				return;
			if (!(cur & sent_rate) && (txc->rate_cfg & sent_rate))
				/* found */
				break;
			/* not found, try higher one */
		}
		SET_BIT(cur, sent_rate);
		acxlog(L_XFER, "tx: stepping up to ratemask %04X\n", cur);
		txc->rate_cur = cur;
		/* FIXME: totally bogus - we could be sending to many peers at once... */
		txc->ignore_count = TX_CNT - priv->tx_free;
	}

	/* calculate acx100 style rate byte if needed */
	if (IS_ACX100(priv)) {
		txc->rate_100 = bitpos2rate100[highest_bit(cur)];
	}
}


/*----------------------------------------------------------------
* acx_l_log_txbuffer
*----------------------------------------------------------------*/
#if !ACX_DEBUG
static inline void acx_l_log_txbuffer(const wlandevice_t *priv) {}
#else
static void
acx_l_log_txbuffer(wlandevice_t *priv)
{
	txdesc_t *txdesc;
	int i;

	/* no FN_ENTER here, we don't want that */
	/* no locks here, since it's entirely non-critical code */
	txdesc = priv->txdesc_start;
	if (!txdesc) return;
	for (i = 0; i < TX_CNT; i++) {
		if ((txdesc->Ctl_8 & DESC_CTL_DONE) == DESC_CTL_DONE)
			printk("tx: buf %d done\n", i);
		txdesc = move_txdesc(priv, txdesc, 1);
	}
}
#endif


/*----------------------------------------------------------------
* acx_l_clean_tx_desc
*
* This function resets the txdescs' status when the ACX100
* signals the TX done IRQ (txdescs have been processed), starting with
* the pool index of the descriptor which we would use next,
* in order to make sure that we can be as fast as possible
* in filling new txdescs.
* Oops, now we have our own index, so everytime we get called we know
* where the next packet to be cleaned is.
* Hmm, still need to loop through the whole ring buffer now,
* since we lost sync for some reason when ping flooding or so...
* (somehow we don't get the IRQ for acx_l_clean_tx_desc any more when
* too many packets are being sent!)
* FIXME: currently we only process one packet, but this gets out of
* sync for some reason when ping flooding, so we need to loop,
* but the previous smart loop implementation causes the ping latency
* to rise dramatically (~3000 ms), at least on CardBus PheeNet WL-0022.
* Dunno what to do :-\
*----------------------------------------------------------------*/
unsigned int
acx_l_clean_tx_desc(wlandevice_t *priv)
{
	txdesc_t *txdesc;
	struct client *txc;
	int finger;
	int num_cleaned;
	int to_process;
	u16 r111;
	u8 error, ack_failures, rts_failures, rts_ok, r100;

	FN_ENTER;

	if (unlikely(acx_debug & L_DEBUG))
		acx_l_log_txbuffer(priv);

	acxlog(L_BUFT, "tx: cleaning up bufs from %u\n", priv->tx_tail);

	finger = priv->tx_tail;
	num_cleaned = 0;
	to_process = TX_CNT;
	do {
		txdesc = get_txdesc(priv, finger);

		/* abort if txdesc is not marked as "Tx finished" and "owned" */
		if ((txdesc->Ctl_8 & DESC_CTL_DONE) != DESC_CTL_DONE) {
			/* we do need to have at least one cleaned,
			 * otherwise we wouldn't get called in the first place
			 */
			if (num_cleaned)
				break;
		}

		/* remember descr values... */
		error = txdesc->error;
		ack_failures = txdesc->ack_failures;
		rts_failures = txdesc->rts_failures;
		rts_ok = txdesc->rts_ok;
		r100 = txdesc->u.r1.rate;
		r111 = txdesc->u.r2.rate111;

#if WIRELESS_EXT > 13 /* wireless_send_event() and IWEVTXDROP are WE13 */
		/* need to check for certain error conditions before we
		 * clean the descriptor: we still need valid descr data here */
		if (unlikely(0x30 & error)) {
			/* only send IWEVTXDROP in case of retry or lifetime exceeded;
			 * all other errors mean we screwed up locally */
			union iwreq_data wrqu;
			wlan_hdr_t *hdr;
			txhostdesc_t *hostdesc;

			hostdesc = acx_get_txhostdesc(priv, txdesc);
			hdr = (wlan_hdr_t *)hostdesc->data;
			MAC_COPY(wrqu.addr.sa_data, hdr->a1);
			wireless_send_event(priv->netdev, IWEVTXDROP, &wrqu, NULL);
		}
#endif
		/* ...and free the descr */
		txdesc->error = 0;
		txdesc->ack_failures = 0;
		txdesc->rts_failures = 0;
		txdesc->rts_ok = 0;
		/* signal host owning it LAST, since ACX already knows that this
		 * descriptor is finished since it set Ctl_8 accordingly:
		 * if _OWN is set at the beginning instead, our own get_tx
		 * might choose a Tx desc that isn't fully cleared
		 * (in case of bad locking). */
		txdesc->Ctl_8 = DESC_CTL_HOSTOWN;
		priv->tx_free++;
		num_cleaned++;

		if ((priv->tx_free >= TX_START_QUEUE)
		&& (priv->status == ACX_STATUS_4_ASSOCIATED)
		&& (acx_queue_stopped(priv->netdev))
		) {
			acxlog(L_BUF, "tx: wake queue (avail. Tx desc %u)\n",
					priv->tx_free);
			acx_wake_queue(priv->netdev, NULL);
		}

		/* do error checking, rate handling and logging
		 * AFTER having done the work, it's faster */

		/* do rate handling */
		txc = acx_get_txc(priv, txdesc);
		if (txc && priv->rate_auto) {
			acx_l_handle_txrate_auto(priv, txc, finger, r100, r111, error);
		}

		if (unlikely(error))
			acx_l_handle_tx_error(priv, error, finger);

		if (IS_ACX111(priv))
			acxlog(L_BUFT, "tx: cleaned %u: !ACK=%u !RTS=%u RTS=%u r111=%04X\n",
				finger, ack_failures, rts_failures, rts_ok, r111);
		else
			acxlog(L_BUFT, "tx: cleaned %u: !ACK=%u !RTS=%u RTS=%u rate=%u\n",
				finger, ack_failures, rts_failures, rts_ok, r100);

		/* update pointer for descr to be cleaned next */
		finger = (finger + 1) % TX_CNT;
	} while (--to_process);

	/* remember last position */
	priv->tx_tail = finger;
/* end: */
	FN_EXIT1(num_cleaned);
	return num_cleaned;
}

/* clean *all* Tx descriptors, and regardless of their previous state.
 * Used for brute-force reset handling. */
void
acx_l_clean_tx_desc_emergency(wlandevice_t *priv)
{
	txdesc_t *txdesc;
	unsigned int i;

	FN_ENTER;

	for (i = 0; i < TX_CNT; i++) {
		txdesc = get_txdesc(priv, i);

		/* free it */
		txdesc->ack_failures = 0;
		txdesc->rts_failures = 0;
		txdesc->rts_ok = 0;
		txdesc->error = 0;
		txdesc->Ctl_8 = DESC_CTL_HOSTOWN;
	}

	priv->tx_free = TX_CNT;

	FN_EXIT0;
}


/*----------------------------------------------------------------
* acx_l_log_rxbuffer
*
* Called from IRQ context only
*----------------------------------------------------------------*/
#if !ACX_DEBUG
static inline void acx_l_log_rxbuffer(const wlandevice_t *priv) {}
#else
static void
acx_l_log_rxbuffer(const wlandevice_t *priv)
{
	const struct rxhostdesc *rxhostdesc;
	int i;

	/* no FN_ENTER here, we don't want that */

	rxhostdesc = priv->rxhostdesc_start;
	if (!rxhostdesc) return;
	for (i = 0; i < RX_CNT; i++) {
		if ((rxhostdesc->Ctl_16 & cpu_to_le16(DESC_CTL_HOSTOWN))
		 && (rxhostdesc->Status & cpu_to_le32(DESC_STATUS_FULL)))
			printk("rx: buf %d full\n", i);
		rxhostdesc++;
	}
}
#endif


/***************************************************************
** acx_l_process_rx_desc
**
** Called directly and only from the IRQ handler
*/
void
acx_l_process_rx_desc(wlandevice_t *priv)
{
	rxhostdesc_t *hostdesc;
	/* unsigned int curr_idx; */
	unsigned int count = 0;

	FN_ENTER;

	if (unlikely(acx_debug & L_BUFR)) {
		acx_l_log_rxbuffer(priv);
	}

	/* First, have a loop to determine the first descriptor that's
	 * full, just in case there's a mismatch between our current
	 * rx_tail and the full descriptor we're supposed to handle. */
	while (1) {
		/* curr_idx = priv->rx_tail; */
		hostdesc = &priv->rxhostdesc_start[priv->rx_tail];
		priv->rx_tail = (priv->rx_tail + 1) % RX_CNT;
		if ((hostdesc->Ctl_16 & cpu_to_le16(DESC_CTL_HOSTOWN))
		 && (hostdesc->Status & cpu_to_le32(DESC_STATUS_FULL))) {
			/* found it! */
			break;
		}
		count++;
		if (unlikely(count > RX_CNT)) {
			/* hmm, no luck: all descriptors empty, bail out */
			goto end;
		}
	}

	/* now process descriptors, starting with the first we figured out */
	while (1) {
		acxlog(L_BUFR, "rx: tail=%u Ctl_16=%04X Status=%08X\n",
			priv->rx_tail, hostdesc->Ctl_16, hostdesc->Status);

		acx_l_process_rxbuf(priv, hostdesc->data);

		hostdesc->Status = 0;
		/* flush all writes before adapter sees CTL_HOSTOWN change */
		wmb();
		/* Host no longer owns this, needs to be LAST */
		CLEAR_BIT(hostdesc->Ctl_16, cpu_to_le16(DESC_CTL_HOSTOWN));

		/* ok, descriptor is handled, now check the next descriptor */
		/* curr_idx = priv->rx_tail; */
		hostdesc = &priv->rxhostdesc_start[priv->rx_tail];

		/* if next descriptor is empty, then bail out */
		/* FIXME: is this check really entirely correct?? */
		/*
//FIXME: inconsistent with check in prev while() loop
		if (!(hostdesc->Ctl & cpu_to_le16(DESC_CTL_HOSTOWN))
		 && !(hostdesc->Status & cpu_to_le32(DESC_STATUS_FULL))) */
		if (!(hostdesc->Status & cpu_to_le32(DESC_STATUS_FULL)))
			break;

		priv->rx_tail = (priv->rx_tail + 1) % RX_CNT;
	}
end:
	FN_EXIT0;
}


/*----------------------------------------------------------------
* acx_s_create_tx_host_desc_queue
*----------------------------------------------------------------*/
static inline void*
acx_alloc_coherent(struct pci_dev *hwdev, size_t size,
			dma_addr_t *dma_handle, int flag)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 5, 53)
	return dma_alloc_coherent(hwdev == NULL ? NULL : &hwdev->dev,
			size, dma_handle, flag);
#else
#warning Using old PCI-specific DMA allocation, may fail with out-of-mem!
#warning Upgrade kernel if it does...
	return pci_alloc_consistent(hwdev, size, dma_handle);
#endif
}

static void*
allocate(wlandevice_t *priv, size_t size, dma_addr_t *phy, const char *msg)
{
	void *ptr = acx_alloc_coherent(priv->pdev, size, phy, GFP_KERNEL);
	if (ptr) {
		acxlog(L_DEBUG, "%s sz=%d adr=0x%p phy=0x%08llx\n",
				msg, (int)size, ptr, (unsigned long long)*phy);
		memset(ptr, 0, size);
		return ptr;
	}
	printk(KERN_ERR "acx: %s allocation FAILED (%d bytes)\n",
					msg, (int)size);
	return NULL;
}

static int
acx_s_create_tx_host_desc_queue(wlandevice_t *priv)
{
	txhostdesc_t *hostdesc;
	u8 *txbuf;
	dma_addr_t hostdesc_phy;
	dma_addr_t txbuf_phy;
	int i;

	FN_ENTER;

	/* allocate TX buffer */
	priv->txbuf_area_size = TX_CNT * WLAN_A4FR_MAXLEN_WEP_FCS;
	priv->txbuf_start = allocate(priv, priv->txbuf_area_size,
			&priv->txbuf_startphy, "txbuf_start");
	if (!priv->txbuf_start)
		goto fail;

	/* allocate the TX host descriptor queue pool */
	priv->txhostdesc_area_size = TX_CNT * 2*sizeof(txhostdesc_t);
	priv->txhostdesc_start = allocate(priv, priv->txhostdesc_area_size,
			&priv->txhostdesc_startphy, "txhostdesc_start");
	if (!priv->txhostdesc_start)
		goto fail;
	/* check for proper alignment of TX host descriptor pool */
	if ((long) priv->txhostdesc_start & 3) {
		printk("acx: driver bug: dma alloc returns unaligned address\n");
		goto fail;
	}

/* Each tx frame buffer is accessed by hardware via
** txdesc -> txhostdesc(s) -> framebuffer(s)
** We use only one txhostdesc per txdesc, but it looks like
** acx111 is buggy: it accesses second txhostdesc
** (via hostdesc.desc_phy_next field) even if
** txdesc->length == hostdesc->length and thus
** entire packet was placed into first txhostdesc.
** Due to this bug acx111 hangs unless second txhostdesc
** has hostdesc.length = 3 (or larger)
** Storing NULL into hostdesc.desc_phy_next
** doesn't seem to help.
*/
/* It is not known whether we need to have 'extra' second
** txhostdescs for acx100. Maybe it is acx111-only bug.
*/
	hostdesc = priv->txhostdesc_start;
	hostdesc_phy = priv->txhostdesc_startphy;
	txbuf = priv->txbuf_start;
	txbuf_phy = priv->txbuf_startphy;

#if 0
/* Works for xterasys xn2522g, does not for WG311v2 !!? */
	for (i = 0; i < TX_CNT*2; i++) {
		hostdesc_phy += sizeof(txhostdesc_t);
		if (!(i & 1)) {
			hostdesc->data_phy = cpu2acx(txbuf_phy);
			/* hostdesc->data_offset = ... */
			/* hostdesc->reserved = ... */
			hostdesc->Ctl_16 = cpu_to_le16(DESC_CTL_HOSTOWN);
			/* hostdesc->length = ... */
			hostdesc->desc_phy_next = cpu2acx(hostdesc_phy);
			hostdesc->pNext = ptr2acx(NULL);
			/* hostdesc->Status = ... */
			/* below: non-hardware fields */
			hostdesc->data = txbuf;

			txbuf += WLAN_A4FR_MAXLEN_WEP_FCS;
			txbuf_phy += WLAN_A4FR_MAXLEN_WEP_FCS;
		} else {
			/* hostdesc->data_phy = ... */
			/* hostdesc->data_offset = ... */
			/* hostdesc->reserved = ... */
			/* hostdesc->Ctl_16 = ... */
			hostdesc->length = 3; /* bug workaround */
			/* hostdesc->desc_phy_next = ... */
			/* hostdesc->pNext = ... */
			/* hostdesc->Status = ... */
			/* below: non-hardware fields */
			/* hostdesc->data = ... */
		}
		hostdesc++;
	}
#endif
	for (i = 0; i < TX_CNT*2; i++) {
		hostdesc_phy += sizeof(txhostdesc_t);
		if (!(i & 1)) {
			hostdesc->data_phy = cpu2acx(txbuf_phy);
			/* done by memset(0): hostdesc->data_offset = 0; */
			/* hostdesc->reserved = ... */
			hostdesc->Ctl_16 = cpu_to_le16(DESC_CTL_HOSTOWN);
			/* hostdesc->length = ... */
			hostdesc->desc_phy_next = cpu2acx(hostdesc_phy);
			/* done by memset(0): hostdesc->pNext = ptr2acx(NULL); */
			/* hostdesc->Status = ... */
			/* below: non-hardware fields */
			hostdesc->data = txbuf;

			txbuf += WLAN_HDR_A3_LEN;
			txbuf_phy += WLAN_HDR_A3_LEN;
		} else {
			hostdesc->data_phy = cpu2acx(txbuf_phy);
			/* done by memset(0): hostdesc->data_offset = 0; */
			/* hostdesc->reserved = ... */
			hostdesc->Ctl_16 = cpu_to_le16(DESC_CTL_HOSTOWN);
			/* hostdesc->length = ...; */
			hostdesc->desc_phy_next = cpu2acx(hostdesc_phy);
			/* done by memset(0): hostdesc->pNext = ptr2acx(NULL); */
			/* hostdesc->Status = ... */
			/* below: non-hardware fields */
			hostdesc->data = txbuf;

			txbuf += WLAN_A4FR_MAXLEN_WEP_FCS - WLAN_HDR_A3_LEN;
			txbuf_phy += WLAN_A4FR_MAXLEN_WEP_FCS - WLAN_HDR_A3_LEN;
		}
		hostdesc++;
	}
	hostdesc--;
	hostdesc->desc_phy_next = cpu2acx(priv->txhostdesc_startphy);

	FN_EXIT1(OK);
	return OK;
fail:
	printk("acx: create_tx_host_desc_queue FAILED\n");
	/* dealloc will be done by free function on error case */
	FN_EXIT1(NOT_OK);
	return NOT_OK;
}


/***************************************************************
** acx_s_create_rx_host_desc_queue
*/
/* the whole size of a data buffer (header plus data body)
 * plus 32 bytes safety offset at the end */
#define RX_BUFFER_SIZE (sizeof(rxbuffer_t) + 32)

static int
acx_s_create_rx_host_desc_queue(wlandevice_t *priv)
{
	rxhostdesc_t *hostdesc;
	rxbuffer_t *rxbuf;
	dma_addr_t hostdesc_phy;
	dma_addr_t rxbuf_phy;
	int i;

	FN_ENTER;

	/* allocate the RX host descriptor queue pool */
	priv->rxhostdesc_area_size = RX_CNT * sizeof(rxhostdesc_t);
	priv->rxhostdesc_start = allocate(priv, priv->rxhostdesc_area_size,
			&priv->rxhostdesc_startphy, "rxhostdesc_start");
	if (!priv->rxhostdesc_start)
		goto fail;
	/* check for proper alignment of RX host descriptor pool */
	if ((long) priv->rxhostdesc_start & 3) {
		printk("acx: driver bug: dma alloc returns unaligned address\n");
		goto fail;
	}

	/* allocate Rx buffer pool which will be used by the acx
	 * to store the whole content of the received frames in it */
	priv->rxbuf_area_size = RX_CNT * RX_BUFFER_SIZE;
	priv->rxbuf_start = allocate(priv, priv->rxbuf_area_size,
			&priv->rxbuf_startphy, "rxbuf_start");
	if (!priv->rxbuf_start)
		goto fail;

	rxbuf = priv->rxbuf_start;
	rxbuf_phy = priv->rxbuf_startphy;
	hostdesc = priv->rxhostdesc_start;
	hostdesc_phy = priv->rxhostdesc_startphy;

	/* don't make any popular C programming pointer arithmetic mistakes
	 * here, otherwise I'll kill you...
	 * (and don't dare asking me why I'm warning you about that...) */
	for (i = 0; i < RX_CNT; i++) {
		hostdesc->data = rxbuf;
		hostdesc->data_phy = cpu2acx(rxbuf_phy);
		hostdesc->length = cpu_to_le16(RX_BUFFER_SIZE);
		CLEAR_BIT(hostdesc->Ctl_16, cpu_to_le16(DESC_CTL_HOSTOWN));
		rxbuf++;
		rxbuf_phy += sizeof(rxbuffer_t);
		hostdesc_phy += sizeof(rxhostdesc_t);
		hostdesc->desc_phy_next = cpu2acx(hostdesc_phy);
		hostdesc++;
	}
	hostdesc--;
	hostdesc->desc_phy_next = cpu2acx(priv->rxhostdesc_startphy);
	FN_EXIT1(OK);
	return OK;
fail:
	printk("acx: create_rx_host_desc_queue FAILED\n");
	/* dealloc will be done by free function on error case */
	FN_EXIT1(NOT_OK);
	return NOT_OK;
}


/***************************************************************
** acx_s_create_hostdesc_queues
*/
int
acx_s_create_hostdesc_queues(wlandevice_t *priv)
{
	int result;
	result = acx_s_create_tx_host_desc_queue(priv);
	if (OK != result) return result;
	result = acx_s_create_rx_host_desc_queue(priv);
	return result;
}


/***************************************************************
** acx_create_tx_desc_queue
*/
static void
acx_create_tx_desc_queue(wlandevice_t *priv, u32 tx_queue_start)
{
	txdesc_t *txdesc;
	txhostdesc_t *hostdesc;
	dma_addr_t hostmemptr;
	u32 mem_offs;
	int i;

	FN_ENTER;

	priv->txdesc_size = sizeof(txdesc_t);

	if (IS_ACX111(priv)) {
		/* the acx111 txdesc is 4 bytes larger */
		priv->txdesc_size = sizeof(txdesc_t) + 4;
	}

	priv->txdesc_start = (txdesc_t *) (priv->iobase2 + tx_queue_start);

	acxlog(L_DEBUG, "priv->iobase2=%p\n"
			"tx_queue_start=%08X\n"
			"priv->txdesc_start=%p\n",
			priv->iobase2,
			tx_queue_start,
			priv->txdesc_start);

	priv->tx_free = TX_CNT;
	/* done by memset: priv->tx_head = 0; */
	/* done by memset: priv->tx_tail = 0; */
	txdesc = priv->txdesc_start;
	mem_offs = tx_queue_start;
	hostmemptr = priv->txhostdesc_startphy;
	hostdesc = priv->txhostdesc_start;

	if (IS_ACX111(priv)) {
		/* ACX111 has a preinitialized Tx buffer! */
		/* loop over whole send pool */
		/* FIXME: do we have to do the hostmemptr stuff here?? */
		for (i = 0; i < TX_CNT; i++) {
			txdesc->HostMemPtr = ptr2acx(hostmemptr);
			txdesc->Ctl_8 = DESC_CTL_HOSTOWN;
			/* reserve two (hdr desc and payload desc) */
			hostdesc += 2;
			hostmemptr += 2 * sizeof(txhostdesc_t);
			txdesc = move_txdesc(priv, txdesc, 1);
		}
	} else {
		/* ACX100 Tx buffer needs to be initialized by us */
		/* clear whole send pool. sizeof is safe here (we are acx100) */
		memset(priv->txdesc_start, 0, TX_CNT * sizeof(txdesc_t));

		/* loop over whole send pool */
		for (i = 0; i < TX_CNT; i++) {
			acxlog(L_DEBUG, "configure card tx descriptor: 0x%p, "
				"size: 0x%X\n", txdesc, priv->txdesc_size);

			/* pointer to hostdesc memory */
			/* FIXME: type-incorrect assignment, might cause trouble
			 * in some cases */
			txdesc->HostMemPtr = ptr2acx(hostmemptr);
			/* initialise ctl */
			txdesc->Ctl_8 = DESC_CTL_INIT;
			txdesc->Ctl2_8 = 0;
			/* point to next txdesc */
			txdesc->pNextDesc = cpu2acx(mem_offs + priv->txdesc_size);
			/* reserve two (hdr desc and payload desc) */
			hostdesc += 2;
			hostmemptr += 2 * sizeof(txhostdesc_t);
			/* go to the next one */
			mem_offs += priv->txdesc_size;
			/* ++ is safe here (we are acx100) */
			txdesc++;
		}
		/* go back to the last one */
		txdesc--;
		/* and point to the first making it a ring buffer */
		txdesc->pNextDesc = cpu2acx(tx_queue_start);
	}
	FN_EXIT0;
}


/***************************************************************
** acx_create_rx_desc_queue
*/
static void
acx_create_rx_desc_queue(wlandevice_t *priv, u32 rx_queue_start)
{
	rxdesc_t *rxdesc;
	u32 mem_offs;
	int i;

	FN_ENTER;

	/* done by memset: priv->rx_tail = 0; */

	/* ACX111 doesn't need any further config: preconfigures itself.
	 * Simply print ring buffer for debugging */
	if (IS_ACX111(priv)) {
		/* rxdesc_start already set here */

		priv->rxdesc_start = (rxdesc_t *) ((u8 *)priv->iobase2 + rx_queue_start);

		rxdesc = priv->rxdesc_start;
		for (i = 0; i < RX_CNT; i++) {
			acxlog(L_DEBUG, "rx descriptor %d @ 0x%p\n", i, rxdesc);
			rxdesc = priv->rxdesc_start = (rxdesc_t *)
				(priv->iobase2 + acx2cpu(rxdesc->pNextDesc));
		}
	} else {
		/* we didn't pre-calculate rxdesc_start in case of ACX100 */
		/* rxdesc_start should be right AFTER Tx pool */
		priv->rxdesc_start = (rxdesc_t *)
			((u8 *) priv->txdesc_start + (TX_CNT * sizeof(txdesc_t)));
		/* NB: sizeof(txdesc_t) above is valid because we know
		** we are in if(acx100) block. Beware of cut-n-pasting elsewhere!
		** acx111's txdesc is larger! */

		memset(priv->rxdesc_start, 0, RX_CNT * sizeof(rxdesc_t));

		/* loop over whole receive pool */
		rxdesc = priv->rxdesc_start;
		mem_offs = rx_queue_start;
		for (i = 0; i < RX_CNT; i++) {
			acxlog(L_DEBUG, "rx descriptor @ 0x%p\n", rxdesc);
			rxdesc->Ctl_8 = DESC_CTL_RECLAIM | DESC_CTL_AUTODMA;
			/* point to next rxdesc */
			rxdesc->pNextDesc = cpu2acx(mem_offs + sizeof(rxdesc_t));
			/* go to the next one */
			mem_offs += sizeof(rxdesc_t);
			rxdesc++;
		}
		/* go to the last one */
		rxdesc--;

		/* and point to the first making it a ring buffer */
		rxdesc->pNextDesc = cpu2acx(rx_queue_start);
	}
	FN_EXIT0;
}


/***************************************************************
** acx_create_desc_queues
*/
void
acx_create_desc_queues(wlandevice_t *priv, u32 tx_queue_start, u32 rx_queue_start)
{
	acx_create_tx_desc_queue(priv, tx_queue_start);
	acx_create_rx_desc_queue(priv, rx_queue_start);
}


/***************************************************************
** acxpci_s_proc_diag_output
*/
char*
acxpci_s_proc_diag_output(char *p, wlandevice_t *priv)
{
	const char *rtl, *thd, *ttl;
	rxhostdesc_t *rxhostdesc;
	txdesc_t *txdesc;
	int i;

	FN_ENTER;

	p += sprintf(p, "** Rx buf **\n");
	rxhostdesc = priv->rxhostdesc_start;
	if (rxhostdesc) for (i = 0; i < RX_CNT; i++) {
		rtl = (i == priv->rx_tail) ? " [tail]" : "";
		if ((rxhostdesc->Ctl_16 & cpu_to_le16(DESC_CTL_HOSTOWN))
		 && (rxhostdesc->Status & cpu_to_le32(DESC_STATUS_FULL)) )
			p += sprintf(p, "%02u FULL%s\n", i, rtl);
		else
			p += sprintf(p, "%02u empty%s\n", i, rtl);
		rxhostdesc++;
	}
	p += sprintf(p, "** Tx buf (free %d, Linux netqueue %s) **\n", priv->tx_free,
				acx_queue_stopped(priv->netdev) ? "STOPPED" : "running");
	txdesc = priv->txdesc_start;
	if (txdesc) for (i = 0; i < TX_CNT; i++) {
		thd = (i == priv->tx_head) ? " [head]" : "";
		ttl = (i == priv->tx_tail) ? " [tail]" : "";
		if (txdesc->Ctl_8 & DESC_CTL_ACXDONE)
			p += sprintf(p, "%02u DONE   (%02X)%s%s\n", i, txdesc->Ctl_8, thd, ttl);
		else
		if (!(txdesc->Ctl_8 & DESC_CTL_HOSTOWN))
			p += sprintf(p, "%02u TxWait (%02X)%s%s\n", i, txdesc->Ctl_8, thd, ttl);
		else
			p += sprintf(p, "%02u empty  (%02X)%s%s\n", i, txdesc->Ctl_8, thd, ttl);
		txdesc = move_txdesc(priv, txdesc, 1);
	}
	p += sprintf(p,
		"\n"
		"** PCI data **\n"
		"txbuf_start %p, txbuf_area_size %u, txbuf_startphy %08llx\n"
		"txdesc_size %u, txdesc_start %p\n"
		"txhostdesc_start %p, txhostdesc_area_size %u, txhostdesc_startphy %08llx\n"
		"rxdesc_start %p\n"
		"rxhostdesc_start %p, rxhostdesc_area_size %u, rxhostdesc_startphy %08llx\n"
		"rxbuf_start %p, rxbuf_area_size %u, rxbuf_startphy %08llx\n",
		priv->txbuf_start, priv->txbuf_area_size, (u64)priv->txbuf_startphy,
		priv->txdesc_size, priv->txdesc_start,
		priv->txhostdesc_start, priv->txhostdesc_area_size, (u64)priv->txhostdesc_startphy,
		priv->rxdesc_start,
		priv->rxhostdesc_start, priv->rxhostdesc_area_size, (u64)priv->rxhostdesc_startphy,
		priv->rxbuf_start, priv->rxbuf_area_size, (u64)priv->rxbuf_startphy);

	FN_EXIT0;
	return p;
}


/***********************************************************************
*/
int
acx_proc_eeprom_output(char *buf, wlandevice_t *priv)
{
	char *p = buf;
	int i;

	FN_ENTER;

	for (i = 0; i < 0x400; i++) {
		acx_read_eeprom_offset(priv, i, p++);
	}

	FN_EXIT1(p - buf);
	return p - buf;
}


/***********************************************************************
*/
void
acx_set_interrupt_mask(wlandevice_t *priv)
{
	if (IS_ACX111(priv)) {
		priv->irq_mask = (u16) ~(0
				/* | HOST_INT_RX_DATA        */
				| HOST_INT_TX_COMPLETE
				/* | HOST_INT_TX_XFER        */
				| HOST_INT_RX_COMPLETE
				/* | HOST_INT_DTIM           */
				/* | HOST_INT_BEACON         */
				/* | HOST_INT_TIMER          */
				/* | HOST_INT_KEY_NOT_FOUND  */
				| HOST_INT_IV_ICV_FAILURE
				| HOST_INT_CMD_COMPLETE
				| HOST_INT_INFO
				/* | HOST_INT_OVERFLOW       */
				/* | HOST_INT_PROCESS_ERROR  */
				| HOST_INT_SCAN_COMPLETE
				| HOST_INT_FCS_THRESHOLD
				/* | HOST_INT_UNKNOWN        */
				);
		priv->irq_mask_off = (u16)~( HOST_INT_CMD_COMPLETE ); /* 0xfdff */
	} else {
		priv->irq_mask = (u16) ~(0
				/* | HOST_INT_RX_DATA        */
				| HOST_INT_TX_COMPLETE
				/* | HOST_INT_TX_XFER        */
				| HOST_INT_RX_COMPLETE
				/* | HOST_INT_DTIM           */
				/* | HOST_INT_BEACON         */
				/* | HOST_INT_TIMER          */
				/* | HOST_INT_KEY_NOT_FOUND  */
				/* | HOST_INT_IV_ICV_FAILURE */
				| HOST_INT_CMD_COMPLETE
				| HOST_INT_INFO
				/* | HOST_INT_OVERFLOW       */
				/* | HOST_INT_PROCESS_ERROR  */
				| HOST_INT_SCAN_COMPLETE
				/* | HOST_INT_FCS_THRESHOLD  */
				/* | HOST_INT_UNKNOWN        */
				);
		priv->irq_mask_off = (u16)~( HOST_INT_UNKNOWN ); /* 0x7fff */
	}
}


/***********************************************************************
*/
int
acx100_s_set_tx_level(wlandevice_t *priv, u8 level_dbm)
{
	/* since it can be assumed that at least the Maxim radio has a
	 * maximum power output of 20dBm and since it also can be
	 * assumed that these values drive the DAC responsible for
	 * setting the linear Tx level, I'd guess that these values
	 * should be the corresponding linear values for a dBm value,
	 * in other words: calculate the values from that formula:
	 * Y [dBm] = 10 * log (X [mW])
	 * then scale the 0..63 value range onto the 1..100mW range (0..20 dBm)
	 * and you're done...
	 * Hopefully that's ok, but you never know if we're actually
	 * right... (especially since Windows XP doesn't seem to show
	 * actual Tx dBm values :-P) */

	/* NOTE: on Maxim, value 30 IS 30mW, and value 10 IS 10mW - so the
	 * values are EXACTLY mW!!! Not sure about RFMD and others,
	 * though... */
	static const u8 dbm2val_maxim[21] = {
		63, 63, 63, 62,
		61, 61, 60, 60,
		59, 58, 57, 55,
		53, 50, 47, 43,
		38, 31, 23, 13,
		0
	};
	static const u8 dbm2val_rfmd[21] = {
		 0,  0,  0,  1,
		 2,  2,  3,  3,
		 4,  5,  6,  8,
		10, 13, 16, 20,
		25, 32, 41, 50,
		63
	};
	const u8 *table;

	switch (priv->radio_type) {
	case RADIO_MAXIM_0D:
		table = &dbm2val_maxim[0];
		break;
	case RADIO_RFMD_11:
	case RADIO_RALINK_15:
		table = &dbm2val_rfmd[0];
		break;
	default:
		printk("%s: unknown/unsupported radio type, "
			"cannot modify tx power level yet!\n",
				priv->netdev->name);
		return NOT_OK;
	}
	printk("%s: changing radio power level to %u dBm (%u)\n",
			priv->netdev->name, level_dbm, table[level_dbm]);
	acxpci_s_write_phy_reg(priv, 0x11, table[level_dbm]);
	return OK;
}


/*----------------------------------------------------------------
* acx_e_init_module
*
* Module initialization routine, called once at module load time.
*
* Returns:
*	0	- success
*	~0	- failure, module is unloaded.
*
* Call context:
*	process thread (insmod or modprobe)
----------------------------------------------------------------*/
int __init
acxpci_e_init_module(void)
{
	int res;

	FN_ENTER;

#if (ACX_IO_WIDTH==32)
	printk("acx: compiled to use 32bit I/O access. "
		"I/O timing issues might occur, such as "
		"non-working firmware upload. Report them\n");
#else
	printk("acx: compiled to use 16bit I/O access only "
		"(compatibility mode)\n");
#endif

#ifdef __LITTLE_ENDIAN
	acxlog(L_INIT, "running on a little-endian CPU\n");
#else
	acxlog(L_INIT, "running on a BIG-ENDIAN CPU\n");
#endif
	acxlog(L_INIT, "PCI module " WLAN_RELEASE " initialized, "
		"waiting for cards to probe...\n");

	res = pci_module_init(&acx_pci_drv_id);
	FN_EXIT1(res);
	return res;
}


/*----------------------------------------------------------------
* acx_e_cleanup_module
*
* Called at module unload time.  This is our last chance to
* clean up after ourselves.
*
* Call context:
*	process thread
----------------------------------------------------------------*/
void __exit
acxpci_e_cleanup_module(void)
{
	struct net_device *dev;
	unsigned long flags;

	FN_ENTER;

	/* Since the whole module is about to be unloaded,
	 * we recursively shutdown all cards we handled instead
	 * of doing it in remove_pci() (which will be activated by us
	 * via pci_unregister_driver at the end).
	 * remove_pci() might just get called after a card eject,
	 * that's why hardware operations have to be done here instead
	 * when the hardware is available. */

	down(&root_acx_dev_sem);

	dev = root_acx_dev.newest;
	while (dev != NULL) {
		/* doh, netdev_priv() doesn't have const! */
		wlandevice_t *priv = netdev_priv(dev);

		acx_sem_lock(priv);

		/* disable both Tx and Rx to shut radio down properly */
		acx_s_issue_cmd(priv, ACX1xx_CMD_DISABLE_TX, NULL, 0);
		acx_s_issue_cmd(priv, ACX1xx_CMD_DISABLE_RX, NULL, 0);

#ifdef REDUNDANT
		/* put the eCPU to sleep to save power
		 * Halting is not possible currently,
		 * since not supported by all firmware versions */
		acx_s_issue_cmd(priv, ACX100_CMD_SLEEP, NULL, 0);
#endif
		acx_lock(priv, flags);

		/* disable power LED to save power :-) */
		acxlog(L_INIT, "switching off power LED to save power :-)\n");
		acx_l_power_led(priv, 0);

		/* stop our eCPU */
		if (IS_ACX111(priv)) {
			/* FIXME: does this actually keep halting the eCPU?
			 * I don't think so...
			 */
			acx_l_reset_mac(priv);
		} else {
			u16 temp;

			/* halt eCPU */
			temp = acx_read_reg16(priv, IO_ACX_ECPU_CTRL) | 0x1;
			acx_write_reg16(priv, IO_ACX_ECPU_CTRL, temp);
			acx_write_flush(priv);
		}

		acx_unlock(priv, flags);

		acx_sem_unlock(priv);

		dev = priv->prev_nd;
	}

	up(&root_acx_dev_sem);

	/* now let the PCI layer recursively remove
	 * all PCI related things (acx_e_remove_pci()) */
	pci_unregister_driver(&acx_pci_drv_id);

	FN_EXIT0;
}
