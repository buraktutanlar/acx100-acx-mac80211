/* src/acx100.c - main module functions
 *
 * Device probing / identification, main initialization and cleanup steps,
 * nothing more
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
 * Locking and synchronization (taken from orinoco.c):
 *
 * The basic principle is that everything is serialized through a
 * single spinlock, priv->lock.  The lock is used in user, bh and irq
 * context, so when taken outside hardirq context it should always be
 * taken with interrupts disabled.  The lock protects both the
 * hardware and the struct wlandevice.
 *
 * Another flag, priv->hw_unavailable indicates that the hardware is
 * unavailable for an extended period of time (e.g. suspended, or in
 * the middle of a hard reset).  This flag is protected by the
 * spinlock.  All code which touches the hardware should check the
 * flag after taking the lock, and if it is set, give up on whatever
 * they are doing and drop the lock again.  The acx_lock()
 * function handles this (it unlocks and returns -EBUSY if
 * hw_unavailable is true). */

/*================================================================*/
/* System Includes */
#ifdef S_SPLINT_S /* some crap that splint needs to not crap out */
#define __signed__ signed
#define __u64 unsigned long long
#define u64 unsigned long long
#define loff_t unsigned long
#define sigval_t unsigned long
#define siginfo_t unsigned long
#define stack_t unsigned long
#define __s64 signed long long
#endif

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

#include <wlan_compat.h>

#include <linux/ioport.h>

#include <linux/pci.h>
#include <linux/init.h>

#include <linux/pm.h>

#include <linux/dcache.h>
#include <linux/highmem.h>

/*================================================================*/
/* Project Includes */

#include <version.h>
#include <p80211hdr.h>
#include <p80211mgmt.h>
#include <acx100.h>
#include <acx100_conv.h>
#include <p80211types.h>
#include <acx100_helper.h>
#include <acx100_helper2.h>
#include <idma.h>
#include <ihw.h>
#include <ioregister.h>

/********************************************************************/
/* Module information                                               */
/********************************************************************/

MODULE_AUTHOR("The ACX100 Open Source Driver development team");
MODULE_DESCRIPTION("Driver for TI ACX1xx based wireless cards (CardBus/PCI/USB)");
#ifdef MODULE_LICENSE
MODULE_LICENSE("Dual MPL/GPL");
#endif

/*================================================================*/
/* Local Constants */
#define PCI_TYPE		(PCI_USES_MEM | PCI_ADDR0 | PCI_NO_ACPI_WAKE)
#define PCI_ACX100_REGION1		0x01
#define PCI_ACX100_REGION1_SIZE		0x1000	/* Memory size - 4K bytes */
#define PCI_ACX100_REGION2		0x02
#define PCI_ACX100_REGION2_SIZE   	0x10000 /* Memory size - 64K bytes */

#define PCI_ACX111_REGION1		0x00
#define PCI_ACX111_REGION1_SIZE		0x2000	/* Memory size - 8K bytes */
#define PCI_ACX111_REGION2		0x01
#define PCI_ACX111_REGION2_SIZE   	0x20000 /* Memory size - 128K bytes */

/* Texas Instruments Vendor ID */
#define PCI_VENDOR_ID_TI		0x104c

/* ACX100 22Mb/s WLAN controller */
#define PCI_DEVICE_ID_TI_TNETW1100A	0x8400
#define PCI_DEVICE_ID_TI_TNETW1100B	0x8401

/* ACX111 54Mb/s WLAN controller */
#define PCI_DEVICE_ID_TI_TNETW1130	0x9066

/* PCI Class & Sub-Class code, Network-'Other controller' */
#define PCI_CLASS_NETWORK_OTHERS 	0x280

/*================================================================*/
/* Local Macros */

/*================================================================*/
/* Local Types */

/*================================================================*/
/* Local Static Definitions */
#define DRIVER_SUFFIX	"_pci"

#define CARD_EEPROM_ID_SIZE 6
#define MAX_IRQLOOPS_PER_JIFFY  (20000/HZ) /* a la orinoco.c */

typedef char *dev_info_t;
static dev_info_t dev_info = "TI acx" DRIVER_SUFFIX;

/* this one should NOT be __devinitdata, otherwise memory section conflict in some kernel versions! */
static char version[] = "TI acx" DRIVER_SUFFIX ".o: " WLAN_RELEASE;

#ifdef ACX_DEBUG
unsigned int debug = L_BIN|L_ASSOC|L_INIT|L_STD;
#endif

unsigned int use_eth_name = 0;

char *firmware_dir;

extern const struct iw_handler_def acx_ioctl_handler_def;

const char name_acx100[] = "ACX100";
const char name_tnetw1100a[] = "TNETW1100A";
const char name_tnetw1100b[] = "TNETW1100B";

const char name_acx111[] = "ACX111";
const char name_tnetw1130[] = "TNETW1130";

 
/*@-fullinitblock@*/
static const struct pci_device_id acx_pci_id_tbl[] __devinitdata = {
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
/*@=fullinitblock@*/

MODULE_DEVICE_TABLE(pci, acx_pci_id_tbl);

#ifdef NONESSENTIAL_FEATURES
typedef struct device_id {
	unsigned char id[6];
	char *descr;
	char *type;
} device_id_t;

static const device_id_t device_ids[] =
{
	/*@-null@*/
	{
		{'G', 'l', 'o', 'b', 'a', 'l'},
		NULL, 
		NULL,
	},
	{
		{(u8)0xff, (u8)0xff, (u8)0xff, (u8)0xff, (u8)0xff, (u8)0xff},
		"uninitialised",
		"SpeedStream SS1021 or Gigafast WF721-AEX"
	},
	{
		{(u8)0x80, (u8)0x81, (u8)0x82, (u8)0x83, (u8)0x84, (u8)0x85},
		"non-standard",
		"DrayTek Vigor 520"
	},
	{
		{'?', '?', '?', '?', '?', '?'},
		"non-standard",
		"Level One WPC-0200"
	},
	{
		{(u8)0x00, (u8)0x00, (u8)0x00, (u8)0x00, (u8)0x00, (u8)0x00},
		"empty",
		"DWL-650+ variant"
	}
	/*@=null@*/
};
#endif /* NONESSENTIAL_FEATURES */

static void acx_disable_irq(wlandevice_t *priv);
static void acx_enable_irq(wlandevice_t *priv);
static int acx_probe_pci(struct pci_dev *pdev,
			    const struct pci_device_id *id);
static void acx_cleanup_card_and_resources(struct pci_dev *pdev, netdevice_t *dev, wlandevice_t *priv,
	unsigned long mem_region1, void *mem1, unsigned long mem_region2, void *mem2);
static void acx_remove_pci(struct pci_dev *pdev);

#ifdef CONFIG_PM
static int acx_suspend(struct pci_dev *pdev, u32 state);
static int acx_resume(struct pci_dev *pdev);
#endif

#ifndef __devexit_p
/* FIXME: check should be removed once driver is included in the kernel */
#warning *** your kernel is EXTREMELY old since it does not even know about __devexit_p - this driver could easily FAIL to work, so better upgrade your kernel! ***
#define __devexit_p(x) x
#endif

/*@-fullinitblock@*/
static struct pci_driver acx_pci_drv_id = {
	.name        = "acx_pci",
	.id_table    = acx_pci_id_tbl,
	.probe       = acx_probe_pci,
	.remove      = __devexit_p(acx_remove_pci),
#ifdef CONFIG_PM
	.suspend     = acx_suspend,
	.resume      = acx_resume
#endif /* CONFIG_PM */
};
/*@=fullinitblock@*/

typedef struct acx_device {
	netdevice_t *newest;
} acx_device_t;

/* if this driver was only about PCI devices, then we probably wouldn't
 * need this linked list.
 * But if we want to register ALL kinds of devices in one global list,
 * then we need it and need to maintain it properly. */
static struct acx_device root_acx_dev = {
	.newest        = NULL,
};

static int acx_start_xmit(struct sk_buff *skb, netdevice_t *dev);
static void acx_tx_timeout(netdevice_t *dev);
static struct net_device_stats *acx_get_stats(netdevice_t *dev);
static struct iw_statistics *acx_get_wireless_stats(netdevice_t *dev);

static irqreturn_t acx_interrupt(int irq, void *dev_id, struct pt_regs *regs);
static void acx_set_multicast_list(netdevice_t *dev);
void acx_rx(struct rxhostdescriptor *rxdesc, wlandevice_t *priv);

static int acx_open(netdevice_t *dev);
static int acx_close(netdevice_t *dev);
static void acx_up(netdevice_t *dev);
static void acx_down(netdevice_t *dev);

/*----------------------------------------------------------------
    External Functions (PMD: didn't know where to put this!
*----------------------------------------------------------------*/

extern u8 acx_signal_determine_quality(u8 signal, u8 noise);

static void acx_get_firmware_version(wlandevice_t *priv)
{
	fw_ver_t fw;
	unsigned int fw_major = 0, fw_minor = 0, fw_sub = 0, fw_extra = 0;
	unsigned int hexarr[4] = { 0, 0, 0, 0 };
	unsigned int hexidx = 0, val = 0;
	char *num, c;

	FN_ENTER;
	
	(void)acx_interrogate(priv, &fw, ACX1xx_IE_FWREV);
	memcpy(priv->firmware_version, &fw.fw_id, 20);
	if (strncmp((char *)fw.fw_id, "Rev ", 4) != 0)
	{
		acxlog(L_STD|L_INIT, "Huh, strange firmware version string \"%s\" without leading \"Rev \" string detected, please report!\n", fw.fw_id);
		priv->firmware_numver = 0x01090407; /* assume 1.9.4.7 */
		FN_EXIT(0, OK);
		return;
	}

/* FIXME: remove old parsing code once the new parsing turned out to be solid */
#define NEW_PARSING 1
#if NEW_PARSING
	num = &fw.fw_id[3];
	do {
		num++;
		c = *num;
		acxlog(L_DEBUG, "num: %c\n", c);
		if ((c != '.') && (c))
			val <<= 4;
		else
		{
			hexarr[hexidx] = val;
			hexidx++;
			if (hexidx > 3)
			{ /* reached end */
				break;
			}
			val = 0;
			continue;
		}
		if ((c >= '0') && (c <= '9'))
			c -= '0';
		else
			c = c - 'a' + (char)10;
		val |= c;
	} while (*num);
	fw_major = hexarr[0];
	fw_minor = hexarr[1];
	fw_sub = hexarr[2];
	fw_extra = hexarr[3];
#else
	fw_major = (u8)(fw.fw_id[4] - '0');
	fw_minor = (u8)(fw.fw_id[6] - '0');
	fw_sub = (u8)(fw.fw_id[8] - '0');
	if (strlen((char *)fw.fw_id) >= 11)
	{
		if ((fw.fw_id[10] >= '0') && (fw.fw_id[10] <= '9'))
			fw_extra = (u8)(fw.fw_id[10] - '0');
		else
			fw_extra = (u8)(fw.fw_id[10] - 'a' + (char)10);
	}
#endif
	priv->firmware_numver =
		(u32)((fw_major << 24) + (fw_minor << 16) + (fw_sub << 8) + fw_extra);
	acxlog(L_DEBUG, "firmware_numver 0x%08x\n", priv->firmware_numver);

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
			acxlog(0xffff, "unknown chip ID 0x%08x, please report!!\n", priv->firmware_id);
			break;
	}

	FN_EXIT(0, OK);
}

/*----------------------------------------------------------------
* acx_display_hardware_details
*
*
* Arguments:
*	priv: ptr to wlandevice that contains all the details 
*	  displayed by this function
* Returns:
*	void
*
* Side effects:
*	none
* Call context:
*	acx_probe_pci
* STATUS:
*	stable
* Comment:
*	This function will display strings to the system log according 
* to device form_factor and radio type. It will needed to be 
*----------------------------------------------------------------*/

static void acx_display_hardware_details(wlandevice_t *priv)
{
	const char *radio_str, *form_str;

	FN_ENTER;

	switch(priv->radio_type) {
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
			radio_str = "802.11a/b/g radio in ACX111 cards?? Please report!!!";
			break;
		default:
			radio_str = "UNKNOWN, please report the radio type name!";
			break;
	}

	switch(priv->form_factor) {
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
			form_str = "UNKNOWN, please report!";
			break;
	}

	acxlog(L_STD, "acx100: form factor 0x%02x (%s), radio type 0x%02x (%s), EEPROM version 0x%02x. Uploaded firmware '%s' (0x%08x).\n", priv->form_factor, form_str, priv->radio_type, radio_str, priv->eeprom_version, priv->firmware_version, priv->firmware_id);

	FN_EXIT(0, OK);
}

#ifdef NONESSENTIAL_FEATURES
static void acx_show_card_eeprom_id(wlandevice_t *priv)
{
	unsigned char buffer[CARD_EEPROM_ID_SIZE];
	u16 i;

	memset(&buffer, 0, CARD_EEPROM_ID_SIZE);
	/* use direct EEPROM access */
	for (i = 0; i < CARD_EEPROM_ID_SIZE; i++) {
		if (OK != acx_read_eeprom_offset(priv,
					 (u16)(ACX100_EEPROM_ID_OFFSET + i),
					 &buffer[i]))
		{
			acxlog(L_STD, "huh, reading EEPROM FAILED!?\n");
			break;
		}
	}
	
	for (i = 0; i < (u16)(sizeof(device_ids) / sizeof(struct device_id)); i++)
	{
		if (0 == memcmp(&buffer, device_ids[i].id, CARD_EEPROM_ID_SIZE))
		{
			if (NULL != device_ids[i].descr) {
				acxlog(L_STD, "%s: EEPROM card ID string check found %s card ID: this is a %s, no??\n", __func__, device_ids[i].descr, device_ids[i].type);
			}
			break;
		}
	}
	if (i == (u16)(sizeof(device_ids) / sizeof(device_id_t)))
	{
		acxlog(L_STD,
	       "%s: EEPROM card ID string check found unknown card: expected \"Global\", got \"%.*s\"! Please report!\n", __func__, CARD_EEPROM_ID_SIZE, buffer);
	}
}
#endif /* NONESSENTIAL_FEATURES */

static inline void acx_device_chain_add(struct net_device *dev)
{
	wlandevice_t *priv = (struct wlandevice *) dev->priv;

	priv->prev_nd = root_acx_dev.newest;
	/*@-temptrans@*/
	root_acx_dev.newest = dev;
	/*@=temptrans@*/
	priv->netdev = dev;
}

static void acx_device_chain_remove(struct net_device *dev)
{
	struct net_device *querydev;
	struct net_device *olderdev;
	struct net_device *newerdev;

	querydev = root_acx_dev.newest;
	newerdev = NULL;
	while (NULL != querydev) {
		olderdev = ((struct wlandevice *) querydev->priv)->prev_nd;
		if (0 == strcmp(querydev->name, dev->name)) {
			if (NULL == newerdev) {
				/* if we were at the beginning of the
				 * list, then it's the list head that
				 * we need to update to point at the
				 * next older device */
				root_acx_dev.newest = olderdev;
			} else {
				/* it's the device that is newer than us
				 * that we need to update to point at
				 * the device older than us */
				((struct wlandevice *) newerdev->priv)->
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
}

/*----------------------------------------------------------------
* acx_probe_pci
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
* Side effects:
*
*
* Call context:
*	process thread
*
* STATUS: should be pretty much ok. UNVERIFIED.
*
* Comment: The function was rewritten according to the V3 driver.
*	The debugging information from V1 is left even if
*	absent from V3.
----------------------------------------------------------------*/
static int __devinit
acx_probe_pci(struct pci_dev *pdev, const struct pci_device_id *id)
{
	int result = -EIO;
	int err;

	u16 chip_type;

	const char *chip_name;
	unsigned long mem_region1 = 0;
	unsigned long mem_region1_size;
	unsigned long mem_region2 = 0;
	unsigned long mem_region2_size;

	unsigned long phymem1;
	unsigned long phymem2;
	void *mem1 = NULL;
	void *mem2 = NULL;

	wlandevice_t *priv = NULL;
	struct net_device *dev = NULL;

	const char *devname_template;
	u32 hardware_info;

	FN_ENTER;

	/* FIXME: flag device somehow to make sure ioctls don't have access
	 * to uninitialized structures before init is finished */

	/* Enable the PCI device */
	if (0 != pci_enable_device(pdev)) {
		acxlog(L_BINSTD | L_INIT,
		       "%s: %s: pci_enable_device() FAILED\n",
		       __func__, dev_info);
		result = -ENODEV;
		goto fail_pci_enable_device;
	}

	/* enable busmastering (required for CardBus) */
	pci_set_master(pdev);

	/* acx100 and acx111 have different PCI memory regions */
	chip_type = (u16)id->driver_data;
	if (chip_type == CHIPTYPE_ACX100) {
		chip_name = name_acx100;
		mem_region1 = PCI_ACX100_REGION1;
		mem_region1_size  = PCI_ACX100_REGION1_SIZE;

		mem_region2 = PCI_ACX100_REGION2;
		mem_region2_size  = PCI_ACX100_REGION2_SIZE;
	} else if (chip_type == CHIPTYPE_ACX111) {
		acxlog(L_BINSTD, "%s: WARNING: ACX111 support is quite experimental!\n", __func__);

		chip_name = name_acx111;
		mem_region1 = PCI_ACX111_REGION1;
		mem_region1_size  = PCI_ACX111_REGION1_SIZE;

		mem_region2 = PCI_ACX111_REGION2;
		mem_region2_size  = PCI_ACX111_REGION2_SIZE;
	} else {
		acxlog(L_BINSTD, "%s: unknown or bad chip??\n", __func__);
		result = -EIO;
		goto fail_unknown_chiptype;
	}

	/* Figure out our resources */
	phymem1 = pci_resource_start(pdev, mem_region1);
	phymem2 = pci_resource_start(pdev, mem_region2);

	if (!request_mem_region
	    (phymem1, pci_resource_len(pdev, mem_region1), "ACX1xx_1")) {
		acxlog(L_BINSTD | L_INIT,
		       "%s: acx100: Cannot reserve PCI memory region 1 (or also: are you sure you have CardBus support in kernel?)\n", __func__);
		result = -EIO;
		goto fail_request_mem_region1;
	}

	if (!request_mem_region
	    (phymem2, pci_resource_len(pdev, mem_region2), "ACX1xx_2")) {
		acxlog(L_BINSTD | L_INIT,
		       "%s: acx100: Cannot reserve PCI memory region 2\n", __func__);
		result = -EIO;
		goto fail_request_mem_region2;
	}

	mem1 = ioremap(phymem1, mem_region1_size);
	if (NULL == mem1) {
		acxlog(L_BINSTD | L_INIT,
		       "%s: %s: ioremap() FAILED\n",
		       __func__, dev_info);
		result = -EIO;
		goto fail_ioremap1;
	}

	mem2 = ioremap(phymem2, mem_region2_size);
	if (NULL == mem2) {
		acxlog(L_BINSTD | L_INIT,
		       "%s: %s: ioremap() FAILED\n",
		       __func__, dev_info);
		result = -EIO;
		goto fail_ioremap2;
	}

	/* Log the device */
	acxlog(L_STD | L_INIT,
	       "Found %s-based wireless network card at %s, irq:%d, phymem1:0x%lx, phymem2:0x%lx, mem1:0x%p, mem1_size:%ld, mem2:0x%p, mem2_size:%ld\n",
	       chip_name, (char *)pdev->slot_name /* was: pci_name(pdev) */, pdev->irq, phymem1, phymem2,
	       mem1, mem_region1_size,
	       mem2, mem_region2_size);
	acxlog(0xffff, "initial debug setting is 0x%04x\n", debug);

	if (0 == pdev->irq) {
		acxlog(L_BINSTD | L_IRQ | L_INIT, "%s: %s: Can't get IRQ %d\n",
		       __func__, dev_info, 0);
		result = -EIO;
		goto fail_irq;
	}

	acxlog(L_DEBUG, "Allocating %d, %Xh bytes for wlandevice_t\n",
			sizeof(wlandevice_t), sizeof(wlandevice_t));
	priv = kmalloc(sizeof(wlandevice_t), GFP_KERNEL);
	if (NULL == priv) {
		acxlog(L_BINSTD | L_INIT,
		       "%s: %s: Memory allocation failure\n",
		       __func__, dev_info);
		result = -EIO;
		goto fail_alloc_priv;
	}

	memset(priv, 0, sizeof(wlandevice_t));

	priv->pdev = pdev;
	priv->chip_type = chip_type;
	priv->chip_name = chip_name;
	if (PCI_HEADER_TYPE_CARDBUS == (int)pdev->hdr_type) {
		priv->bus_type = (u8)ACX_CARDBUS;
	} else
	if (PCI_HEADER_TYPE_NORMAL == (int)pdev->hdr_type) {
		priv->bus_type = (u8)ACX_PCI;
	} else {
		acxlog(L_STD, "ERROR: card has unknown bus type!!\n");
	}

	acx_select_io_register_set(priv, chip_type);

	spin_lock_init(&priv->lock);

	acxlog(L_INIT, "hw_unavailable = 1\n");
	priv->hw_unavailable = 1;
	CLEAR_BIT(priv->dev_state_mask, ACX_STATE_IFACE_UP);

	priv->membase = phymem1;
	priv->iobase = mem1;

	priv->membase2 = phymem2;
	priv->iobase2 = mem2;

	priv->mgmt_timer.function = (void *)0x0000dead; /* to find crashes due to weird driver access to unconfigured interface (ifup) */

#ifdef NONESSENTIAL_FEATURES
	acx_show_card_eeprom_id(priv);
#endif /* NONESSENTIAL_FEATURES */

	dev = kmalloc(sizeof(netdevice_t), GFP_KERNEL);
	if (unlikely(NULL == dev)) {
		acxlog(L_BINSTD | L_INIT,
		       "%s: FAILED to alloc netdev\n", __func__);
		result = -EIO;
		goto fail_alloc_netdev;
	}

	memset(dev, 0, sizeof(netdevice_t));
	ether_setup(dev);

	/* now we have our device, so make sure the kernel doesn't try
	 * to send packets even though we're not associated to a network yet */
	acx_stop_queue(dev, "after setup");

	dev->priv = priv;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 5, 70)
	/* this define and its netdev member exist since 2.5.70 */
	SET_NETDEV_DEV(dev, &pdev->dev);
#endif

	/* register new dev in linked list */
	acx_device_chain_add(dev);

	acxlog(L_BINSTD | L_IRQ | L_INIT,
		       "%s: %s: Using IRQ %d\n",
		       __func__, dev_info, pdev->irq);

	dev->irq = pdev->irq;
	dev->base_addr = pci_resource_start(pdev, 0); /* TODO this is maybe incompatible to ACX111 */

	/* need to be able to restore PCI state after a suspend */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 10)
	/* 2.6.9-rc3-mm2 (2.6.9-bk4, too) introduced this shorter version,
	   then it made its way into 2.6.10 */
	pci_save_state(pdev);
#else
	pci_save_state(pdev, priv->pci_state);
#endif

	if (OK != acx_reset_dev(dev)) {
		acxlog(L_BINSTD | L_INIT,
		       "%s: %s: MAC initialize failure!\n",
		       __func__, dev_info);
		result = -EIO;
		goto fail_reset;
	}

	devname_template = (1 == use_eth_name) ? "eth%d" : "wlan%d";
	if (dev_alloc_name(dev, devname_template) < 0)
	{
		result = -EIO;
		goto fail_alloc_name;
	}
	acxlog(L_STD, "acx100: allocated net device %s, driver compiled against wireless extensions v%d and Linux %s\n", dev->name, WIRELESS_EXT, UTS_RELEASE);

	/* now that device init was successful, fill remaining fields... */
	dev->open = &acx_open;
	dev->stop = &acx_close;
	dev->hard_start_xmit = &acx_start_xmit;
	dev->get_stats = &acx_get_stats;
	dev->get_wireless_stats = &acx_get_wireless_stats;
#if WIRELESS_EXT >= 13
	dev->wireless_handlers = (struct iw_handler_def *)&acx_ioctl_handler_def;
#else
	dev->do_ioctl = &acx_ioctl_old;
#endif
	dev->set_multicast_list = &acx_set_multicast_list;
	dev->tx_timeout = &acx_tx_timeout;
	dev->watchdog_timeo = 4 * HZ;	/* 400 */

	/* ok, basic setup is finished, now start initialising the card */

	hardware_info = acx_read_reg16(priv, priv->io[IO_ACX_EEPROM_INFORMATION]);
	priv->form_factor = (u8)(hardware_info & 0xff);
	priv->radio_type = (u8)(hardware_info >> 8 & 0xff);
/*	priv->eeprom_version = hardware_info >> 16; */
	if (OK != acx_read_eeprom_offset(priv, 0x05, &priv->eeprom_version)) {
		result = -EIO;
		goto fail_read_eeprom_version;
	}

	if (OK != acx_init_mac(dev, 1)) {
		acxlog(L_DEBUG | L_INIT,
		       "Danger Will Robinson, MAC did not come back\n");
		result = -EIO;
		goto fail_init_mac;
	}

	/* card initialized, so let's release hardware */
	priv->hw_unavailable--;
	acxlog(L_INIT, "hw_unavailable--\n");

	/* needs to be after acx_init_mac() due to necessary init stuff */
	acx_get_firmware_version(priv);

	acx_display_hardware_details(priv);

	pci_set_drvdata(pdev, dev);

	/* ...and register the card, AFTER everything else has been set up,
	 * since otherwise an ioctl could step on our feet due to
	 * firmware operations happening in parallel or uninitialized data */
	err = register_netdev(dev);
	if (OK != err) {
		acxlog(L_BINSTD | L_INIT,
		       "%s: %s: Register net device of %s FAILED: %d\n",
		       __func__, dev_info, dev->name, err);
		result = -EIO;
		goto fail_register_netdev;
	}
	acx_carrier_off(dev, "on probe");

#ifdef CONFIG_PROC_FS
	if (OK != acx_proc_register_entries(dev)) {
		result = -EIO;
		goto fail_proc_register_entries;
	}
#endif

	acxlog(L_STD|L_INIT, "%s: %s Loaded Successfully\n", __func__, version);
	result = OK;
	goto done;

	/* error paths: undo everything in reverse order... */

#ifdef CONFIG_PROC_FS
fail_proc_register_entries:
#endif

	if (0 != (priv->dev_state_mask & ACX_STATE_IFACE_UP))
		acx_down(dev);
	unregister_netdev(dev);
	
	/* FIXME: try to find a way to re-unify error code path with
	 * acx_cleanup_card_and_resources(), which it used to simply call,
	 * but error handling was too sophisticated for such a simple function call,
	 * so we had to give it up again... */
fail_register_netdev:

	acx_delete_dma_regions(priv);
	pci_set_drvdata(pdev, NULL);
	priv->hw_unavailable++;
	acxlog(L_STD, "hw_unavailable++\n");
fail_init_mac:
fail_read_eeprom_version:
fail_alloc_name:
fail_reset:

	acx_device_chain_remove(dev);
	kfree(dev);
fail_alloc_netdev:

	kfree(priv);
fail_alloc_priv:
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
	acxlog(L_STD|L_INIT, "%s: %s Loading FAILED\n", __func__, version);

done:
	FN_EXIT(1, result);
	return result;
} /* acx_probe_pci() */

static void acx_cleanup_card_and_resources(
	struct pci_dev *pdev,
	netdevice_t *dev,
	wlandevice_t *priv,
	unsigned long mem_region1, void *mem1,
	unsigned long mem_region2, void *mem2)
{
	/* unregister the device to not let the kernel
	 * (e.g. ioctls) access a half-deconfigured device */

	if (dev != NULL)
	{
		acxlog(L_INIT, "Removing device %s!\n", dev->name);
		unregister_netdev(dev);

#ifdef CONFIG_PROC_FS
		acx_proc_unregister_entries(dev);
#endif

		/* find our PCI device in the global acx list and remove it */
		acx_device_chain_remove(dev);
	}

	if (priv != NULL)
	{
		if (0 != (priv->dev_state_mask & ACX_STATE_IFACE_UP))
			acx_down(dev);

		CLEAR_BIT(priv->dev_state_mask, ACX_STATE_IFACE_UP);

		priv->hw_unavailable++;
		acxlog(L_STD, "hw_unavailable++\n");
	}

	if (priv != NULL)
	{
		acx_delete_dma_regions(priv);

		kfree(priv);
	}

	/* finally, clean up PCI bus state */

	if (NULL != mem1)
		iounmap(mem1);
	if (NULL != mem2)
		iounmap(mem2);

	release_mem_region(pci_resource_start(pdev, mem_region1),
			   pci_resource_len(pdev, mem_region1));

	release_mem_region(pci_resource_start(pdev, mem_region2),
			   pci_resource_len(pdev, mem_region2));

	pci_disable_device(pdev);

	if (dev != NULL)
	{
		/* remove dev registration */
		pci_set_drvdata(pdev, NULL);

		/* Free netdev (quite late,
		 * since otherwise we might get caught off-guard
		 * by a netdev timeout handler execution
		 * expecting to see a working dev...)
		 * But don't use free_netdev() here,
		 * it's supported by newer kernels only */
		kfree(dev);
	}

	/* put device into ACPI D3 mode (shutdown) */
	pci_set_power_state(pdev, 3);
}


/*----------------------------------------------------------------
* acx_remove_pci
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
* Returns:
*	nothing
*
* Side effects:
*
* Call context:
*	process thread
*
* STATUS: should be pretty much ok. UNVERIFIED.
*
----------------------------------------------------------------*/
static void __devexit acx_remove_pci(struct pci_dev *pdev)
{
	struct net_device *dev;
	wlandevice_t *priv;
	unsigned long mem_region1 = 0, mem_region2 = 0;

	FN_ENTER;

	dev = (struct net_device *) pci_get_drvdata(pdev);
	priv = (struct wlandevice *) dev->priv;
	
	if (dev == NULL || priv == NULL) {
		acxlog(L_STD, "%s: card not used. Skipping any release code\n", __func__);
		goto end;
	}

	if (priv->chip_type == CHIPTYPE_ACX100) {
		mem_region1 = PCI_ACX100_REGION1;
		mem_region2 = PCI_ACX100_REGION2;
	}
	else
	if (priv->chip_type == CHIPTYPE_ACX111) {
		mem_region1 = PCI_ACX111_REGION1;
		mem_region2 = PCI_ACX111_REGION2;
	}
	else
		acxlog(L_INIT, "unknown chip type!\n");

	acx_cleanup_card_and_resources(pdev, dev, priv, mem_region1, priv->iobase, mem_region2, priv->iobase2);

end:
	FN_EXIT(0, 0);
}

#ifdef CONFIG_PM
static int if_was_up = 0; /* FIXME: HACK, do it correctly sometime instead */
static int acx_suspend(struct pci_dev *pdev, /*@unused@*/ u32 state)
{
	/*@unused@*/ struct net_device *dev = pci_get_drvdata(pdev);
	wlandevice_t *priv = dev->priv;

	FN_ENTER;
	acxlog(L_STD, "acx100: experimental suspend handler called for %p!\n", priv);
	if (0 != netif_device_present(dev)) {
		if_was_up = 1;
		acx_down(dev);
	}
	else
		if_was_up = 0;
	netif_device_detach(dev);
	acx_delete_dma_regions(priv);

	FN_EXIT(0, OK);
	return OK;
}

static int acx_resume(struct pci_dev *pdev)
{
	struct net_device *dev;
	/*@unused@*/ wlandevice_t *priv;

	printk(KERN_WARNING "rsm: resume\n");
	dev = pci_get_drvdata(pdev);
	printk(KERN_WARNING "rsm: got dev\n");

	if (!netif_running(dev))
		return 0;

	priv = dev->priv;
	printk(KERN_WARNING "rsm: got priv\n");
	FN_ENTER;
	acxlog(L_STD, "acx100: experimental resume handler called for %p!\n", priv);
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
	acx_reset_dev(dev);
	acxlog(L_DEBUG, "rsm: device reset done\n");

	if (OK != acx_init_mac(dev, 0))
	{
		acxlog(L_DEBUG, "rsm: init_mac FAILED\n");
		goto fail;
	}
	acxlog(L_DEBUG, "rsm: init MAC done\n");
	
	if (1 == if_was_up)
		acx_up(dev);
	acxlog(L_DEBUG, "rsm: acx up\n");
	
	/* now even reload all card parameters as they were before suspend,
	 * and possibly be back in the network again already :-)
	 * FIXME: should this be done in that scheduled task instead?? */
	acx_update_card_settings(priv, 0, 0, 1);
	acxlog(L_DEBUG, "rsm: settings updated\n");
	netif_device_attach(dev);
	acxlog(L_DEBUG, "rsm: device attached\n");
fail: /* we need to return OK here anyway, right? */
	FN_EXIT(0, OK);
	return OK;
}
#endif /* CONFIG_PM */

/*----------------------------------------------------------------
* acx_up
*
*
* Arguments:
*	dev: netdevice structure that contains the private priv
* Returns:
*	void
* Side effects:
*	- Enables on-card interrupt requests
*	- calls acx_start
* Call context:
*	- process thread
* STATUS:
*	stable
* Comment:
*	This function is called by acx_open (when ifconfig sets the
*	device as up).
*----------------------------------------------------------------*/
static void acx_up(netdevice_t *dev)
{
	wlandevice_t *priv = (wlandevice_t *) dev->priv;

	FN_ENTER;

	acx_enable_irq(priv);
	if ((priv->firmware_numver >= 0x0109030e) || (priv->chip_type == CHIPTYPE_ACX111) ) /* FIXME: first version? */ {
		/* newer firmware versions don't use a hardware timer any more */
		acxlog(L_INIT, "firmware version >= 1.9.3.e --> using S/W timer\n");
		init_timer(&priv->mgmt_timer);
		priv->mgmt_timer.function = acx_timer;
		priv->mgmt_timer.data = (unsigned long)priv;
	}
	else {
		acxlog(L_INIT, "firmware version < 1.9.3.e --> using H/W timer\n");
	}
	/* FIXME: explicitly calling this here doesn't seem too clean... */
	if ( CHIPTYPE_ACX111 == priv->chip_type ) {
	    acx_init_wep(priv);
	}
	acx_start(priv);
	
	/* acx_start_queue(dev, "on startup"); */

	FN_EXIT(0, OK);
}

/*----------------------------------------------------------------
* acx_down
*
*
* Arguments:
*	dev: netdevice structure that contains the private priv
* Returns:
*	void
* Side effects:
*	- disables on-card interrupt request
* Call context:
*	process thread
* STATUS:
*	stable
* Comment:
*	this disables the netdevice
*----------------------------------------------------------------*/
static void acx_down(netdevice_t *dev)
{
	wlandevice_t *priv = (wlandevice_t *) dev->priv;

	FN_ENTER;

	acx_stop_queue(dev, "during close");

	/* we really don't want to have an asynchronous tasklet disturb us
	 * after something vital for its job has been shut down, so
	 * end all remaining work now... */
	acx_flush_task_scheduler();

	acx_set_status(priv, ISTATUS_0_STARTED);

	if ((priv->firmware_numver >= 0x0109030e) || (priv->chip_type == CHIPTYPE_ACX111)) /* FIXME: first version? */
	{ /* newer firmware versions don't use a hardware timer any more */
		del_timer_sync(&priv->mgmt_timer);
	}
	acx_disable_irq(priv);
	FN_EXIT(0, OK);
}


/*----------------------------------------------------------------
* acx_open
*
* WLAN device open method.  Called from p80211netdev when kernel
* device open (start) method is called in response to the
* SIOCSIFFLAGS ioctl changing the flags bit IFF_UP
* from clear to set.
*
* Arguments:
*	dev		network device structure
*
* Returns:
*	0	success
*	>0	f/w reported error
*	<0	driver reported error
*
* Side effects:
*
* Call context:
*	process thread
* STATUS: should be ok.
----------------------------------------------------------------*/

static int acx_open(netdevice_t *dev)
{
	wlandevice_t *priv = (wlandevice_t *) dev->priv;
	int result = OK;
	unsigned long flags;

	FN_ENTER;

	acxlog(L_INIT, "module count ++\n");
	WLAN_MOD_INC_USE_COUNT;

	acxlog(L_STD, "OPENING DEVICE\n");
	if (0 != acx_lock(priv, &flags)) {
		acxlog(L_INIT, "card unavailable, bailing\n");
		result = -ENODEV;
		goto done;
	}

	acx_init_task_scheduler(priv);

	/* request shared IRQ handler */
	if (0 != request_irq(dev->irq, acx_interrupt, SA_SHIRQ, dev->name, dev)) {
		acxlog(L_BINSTD | L_INIT | L_IRQ, "request_irq FAILED\n");
		result = -EAGAIN;
		goto done;
	}
	acxlog(L_DEBUG | L_IRQ, "%s: request_irq %d successful\n", __func__, dev->irq);

	/* ifup device */
	acx_up(dev);
	SET_BIT(priv->dev_state_mask, ACX_STATE_IFACE_UP);

	/* We don't currently have to do anything else.
	 * The setup of the MAC should be subsequently completed via
	 * the mlme commands.
	 * Higher layers know we're ready from dev->start==1 and
	 * dev->tbusy==0.  Our rx path knows to pass up received/
	 * frames because of dev->flags&IFF_UP is true.
	 */
done:
	acx_unlock(priv, &flags);
	FN_EXIT(1, result);
	return result;
}

/*----------------------------------------------------------------
* acx_close
*
* WLAN device close method.  Called from p80211netdev when kernel
* device close method is called in response to the
* SIOCSIIFFLAGS ioctl changing the flags bit IFF_UP
* from set to clear.
* (i.e. called for "ifconfig DEV down")
*
* Arguments:
*	dev		network device structure
*
* Returns:
*	0	success
*	>0	f/w reported error
*	<0	driver reported error
*
* Side effects:
*
* Call context:
*	process thread
*
* STATUS: Should be pretty much perfect now.
----------------------------------------------------------------*/

static int acx_close(netdevice_t *dev)
{
	wlandevice_t *priv = (wlandevice_t *) dev->priv;

	FN_ENTER;

	/* don't use acx_lock() here: need to close card even in case
	 * of hw_unavailable set (card ejected) */
#ifdef BROKEN_LOCKING
	spin_lock_irq(&priv->lock);
#endif

	/* ifdown device */
	CLEAR_BIT(priv->dev_state_mask, ACX_STATE_IFACE_UP);
	if (0 != netif_device_present(dev)) {
		acx_down(dev);
	}

	/* release shared IRQ handler */
	free_irq(dev->irq, dev);

	/* We currently don't have to do anything else.
	 * Higher layers know we're not ready from dev->start==0 and
	 * dev->tbusy==1.  Our rx path knows to not pass up received
	 * frames because of dev->flags&IFF_UP is false.
	 */

	acxlog(L_INIT, "module count --\n");
	WLAN_MOD_DEC_USE_COUNT;
#ifdef BROKEN_LOCKING
	spin_unlock_irq(&priv->lock);
#endif

	FN_EXIT(0, OK);
	return OK;
}
/*----------------------------------------------------------------
* acx_start_xmit
*
*
* Arguments:
*
* Returns:
*
* Side effects:
*
* Call context:
*
* STATUS:
*
* Comment:
*
*----------------------------------------------------------------*/

static int acx_start_xmit(struct sk_buff *skb, netdevice_t *dev)
{
	int txresult = NOT_OK;
	unsigned long flags;
	wlandevice_t *priv = (wlandevice_t *) dev->priv;
	struct txdescriptor *tx_desc;
	unsigned int templen;

	FN_ENTER;

	if (unlikely(!skb)) {
		/* indicate success */
		txresult = OK;
		goto fail_no_unlock;
	}
	if (unlikely(!priv)) {
		txresult = NOT_OK;
		goto fail_no_unlock;
	}
	if (unlikely(0 == (priv->dev_state_mask & ACX_STATE_IFACE_UP))) {
		txresult = NOT_OK;
		goto fail_no_unlock;
	}

	if (unlikely(0 != acx_lock(priv, &flags)))
	{
		txresult = NOT_OK;
		goto fail_no_unlock;
	}

	if (unlikely(0 != acx_queue_stopped(dev))) {
		acxlog(L_BINSTD, "%s: called when queue stopped\n", __func__);
		txresult = NOT_OK;
		goto fail;
	}

	if (unlikely(ISTATUS_4_ASSOCIATED != priv->status)) {
		acxlog(L_XFER, "Trying to xmit, but not associated yet: aborting...\n");
		/* silently drop the packet, since we're not connected yet */
		txresult = OK;
		/* ...but indicate an error nevertheless */
		priv->stats.tx_errors++;
		goto fail;
	}

#if 0
	/* we're going to transmit now, so stop another packet from entering.
	 * FIXME: most likely we shouldn't do it like that, but instead:
	 * stop the queue during card init, then wake the queue once
	 * we're associated to the network, then stop the queue whenever
	 * we don't have any free Tx buffers left, and wake it again once a
	 * Tx buffer becomes free again. And of course also stop the
	 * queue once we lose association to the network (since it
	 * doesn't make sense to allow more user packets if we can't
	 * forward them to a network).
	 * FIXME: Hmm, seems this is all wrong. We SHOULD leave the
	 * queue open from the beginning (as long as we're not full,
	 * and also even before we're even associated),
	 * otherwise we'll get NETDEV WATCHDOG transmit timeouts... */
	acx_stop_queue(dev, "during Tx");
#endif
	if (unlikely((tx_desc = acx_get_tx_desc(priv)) == NULL)) {
		acxlog(L_BINSTD,"BUG: txdesc ring full\n");
		txresult = NOT_OK;
		goto fail;
	}

	templen = skb->len;
	acx_ether_to_txdesc(priv, tx_desc, skb);
	acx_dma_tx_data(priv, tx_desc);
	dev->trans_start = jiffies;

	txresult = OK;
	priv->stats.tx_packets++;
	priv->stats.tx_bytes += templen;

fail:
	acx_unlock(priv, &flags);
fail_no_unlock:
	if ((txresult == OK) && (NULL != skb))
		dev_kfree_skb(skb);

	FN_EXIT(1, txresult);
	return txresult;
}
/*----------------------------------------------------------------
* acx_tx_timeout
*
*
* Arguments:
*
* Returns:
*
* Side effects:
*
* Call context:
*
* STATUS:
*
* Comment:
*
*----------------------------------------------------------------*/

/* acx_tx_timeout()
 * STATUS: ok.
 */
static void acx_tx_timeout(netdevice_t *dev)
{
	wlandevice_t *priv;

	FN_ENTER;
	
	priv = (wlandevice_t *)dev->priv;

/* hmm, maybe it is still better to clean the ring buffer, despite firmware
 * issues?? */
#if DOH_SEEMS_TO_CONFUSE_FIRMWARE_UNFORTUNATELY
	/* clean all tx descs, they may have been completely full */
	acx_clean_tx_desc_emergency(priv);
	
	if ((acx_queue_stopped(dev)) && (ISTATUS_4_ASSOCIATED == priv->status))
		acx_wake_queue(dev, "after Tx timeout");
#endif

	/* stall may have happened due to radio drift, so recalib radio */
	acx_schedule_after_interrupt_task(priv, ACX_AFTER_IRQ_CMD_RADIO_RECALIB);
			
	printk("acx100: Tx timeout!\n");

	priv->stats.tx_errors++;

	FN_EXIT(0, OK);
}

/*----------------------------------------------------------------
* acx_get_stats
*
*
* Arguments:
*
* Returns:
*
* Side effects:
*
* Call context:
*
* STATUS:
*
* Comment:
*
*----------------------------------------------------------------*/

/* acx_get_stats()
 * STATUS: should be ok..
 */
static struct net_device_stats *acx_get_stats(netdevice_t *dev)
{
	wlandevice_t *priv = (wlandevice_t *)dev->priv;
#if ANNOYING_GETS_CALLED_TOO_OFTEN
	FN_ENTER;
	FN_EXIT(1, (int)&priv->stats);
#endif
	return &priv->stats;
}

/*----------------------------------------------------------------
* acx_get_wireless_stats
*
*
* Arguments:
*
* Returns:
*
* Side effects:
*
* Call context:
*
* STATUS:
*
* Comment:
*
*----------------------------------------------------------------*/

static struct iw_statistics *acx_get_wireless_stats(netdevice_t *dev)
{
	wlandevice_t *priv = (wlandevice_t *)dev->priv;
	FN_ENTER;
	FN_EXIT(1, (int)&priv->wstats);
	return &priv->wstats;
}

/*----------------------------------------------------------------
* acx_set_multicast_list
*
*
* Arguments:
*
* Returns:
*
* Side effects:
*
* Call context:
*
* STATUS:
*
* Comment:
*
*----------------------------------------------------------------*/

static void acx_set_multicast_list(netdevice_t *dev)
{
	wlandevice_t *priv = (wlandevice_t *)dev->priv;

	FN_ENTER;

	acxlog(L_STD, "FIXME: most likely needs refinement, first implementation version only...\n");

	/* ACX firmwares don't have allmulti capability,
	 * so just use promiscuous mode instead in this case. */
	if (dev->flags & (IFF_PROMISC|IFF_ALLMULTI)) {
		priv->promiscuous = 1;
		SET_BIT(priv->rx_config_1, RX_CFG1_RCV_PROMISCUOUS);
		CLEAR_BIT(priv->rx_config_1, RX_CFG1_FILTER_ALL_MULTI);
		SET_BIT(priv->set_mask, SET_RXCONFIG);
		/* let kernel know in case *we* needed to set promiscuous */
		dev->flags |= (IFF_PROMISC|IFF_ALLMULTI);
	}
	else {
		priv->promiscuous = 0;
		CLEAR_BIT(priv->rx_config_1, RX_CFG1_RCV_PROMISCUOUS);
		SET_BIT(priv->rx_config_1, RX_CFG1_FILTER_ALL_MULTI);
		SET_BIT(priv->set_mask, SET_RXCONFIG);
		dev->flags &= ~(IFF_PROMISC|IFF_ALLMULTI);
	}

	/* cannot update card settings directly here, atomic context!
	 * FIXME: hmm, most likely it would be much better instead if
	 * acx_update_card_settings() always worked in atomic context! */
	acx_schedule_after_interrupt_task(priv, ACX_AFTER_IRQ_CMD_UPDATE_CARD_CFG);

	FN_EXIT(0, OK);
}

static inline void acx_update_link_quality_led(wlandevice_t *priv)
{
	int qual;

	qual = acx_signal_determine_quality(priv->wstats.qual.level, priv->wstats.qual.noise);
	if (qual > priv->brange_max_quality)
		qual = priv->brange_max_quality;

	if (time_after(jiffies, priv->brange_time_last_state_change + (HZ/2 - HZ/2 * (long) qual/priv->brange_max_quality ) )) {
		acx_power_led(priv, (u8)(priv->brange_last_state == 0));
		priv->brange_last_state ^= 1; /* toggle */
		priv->brange_time_last_state_change = jiffies;
	}
}

/*----------------------------------------------------------------
* acx_enable_irq
*
*
* Arguments:
*
* Returns:
*
* Side effects:
*
* Call context:
*
* STATUS:
*
* Comment:
*
*----------------------------------------------------------------*/

static void acx_enable_irq(wlandevice_t *priv)
{
	FN_ENTER;
#if (WLAN_HOSTIF!=WLAN_USB)
	acx_write_reg16(priv, priv->io[IO_ACX_IRQ_MASK], priv->irq_mask);
	acx_write_reg16(priv, priv->io[IO_ACX_FEMR], 0x8000);
	priv->irqs_active = 1;
#endif
	FN_EXIT(0, OK);
}

/*----------------------------------------------------------------
* acx_disable_irq
*
*
* Arguments:
*
* Returns:
*
* Side effects:
*
* Call context:
*
* STATUS:
*
* Comment:
*
*----------------------------------------------------------------*/

static void acx_disable_irq(wlandevice_t *priv)
{
	FN_ENTER;
#if (WLAN_HOSTIF!=WLAN_USB)
	acx_write_reg16(priv, priv->io[IO_ACX_IRQ_MASK], priv->irq_mask_off);
	acx_write_reg16(priv, priv->io[IO_ACX_FEMR], 0x0);
	priv->irqs_active = 0;
#endif
	FN_EXIT(0, OK);
}

#define INFO_SCAN_COMPLETE      0x0001  /* scan is complete. all frames now on the receive queue are valid */
#define INFO_WEP_KEY_NOT_FOUND  0x0002
#define INFO_WATCH_DOG_RESET    0x0003  /* hw has been reset as the result of a watchdog timer timeout */
#define INFO_PS_FAIL            0x0004  /* failed to send out NULL frame from PS mode notification to AP */
                                        /* recommended action: try entering 802.11 PS mode again */
#define INFO_IV_ICV_FAILURE     0x0005  /* encryption/decryption process on a packet failed */

static void acx_handle_info_irq(wlandevice_t *priv)
{
	static const char *info_type_msg[] = {
		"(unknown)",
		"scan complete",
		"WEP key not found",
		"internal watchdog reset was done",
		"failed to send powersave (NULL frame) notification to AP",
		"encrypt/decrypt on a packet has failed",
		"(unknown)",
		"MIC failure: fake WEP encrypt??"
	};

	acx_get_info_state(priv);
	acxlog(L_STD | L_IRQ, "Got Info IRQ: status 0x%04x, type 0x%04x: %s\n",
		priv->info_status, priv->info_type,
		info_type_msg[(priv->info_type>5) ? 0 : priv->info_type]
	);
}

/*----------------------------------------------------------------
* acx_interrupt
* 
* Never call a schedule or sleep in this method. Result will be a Kernel Panic.
*
* Arguments:
*
* Returns:
*
* Side effects:
*
* Call context:
*
* STATUS:
*
* Comment:
*
*----------------------------------------------------------------*/

static irqreturn_t acx_interrupt(/*@unused@*/ int irq, void *dev_id, /*@unused@*/ struct pt_regs *regs)
{
	register wlandevice_t *priv;
	register u16 irqtype;
	unsigned int irqcount = MAX_IRQLOOPS_PER_JIFFY;
	static unsigned int loops_this_jiffy = 0;
	static unsigned long last_irq_jiffies = 0;

	FN_ENTER;

	priv = (wlandevice_t *) (((netdevice_t *) dev_id)->priv);
	irqtype = acx_read_reg16(priv, priv->io[IO_ACX_IRQ_STATUS_CLEAR]);
	if (unlikely(0xffff == irqtype))
	{
		/* 0xffff value hints at missing hardware,
		 * so don't do anything.
		 * FIXME: that's not very clean - maybe we are able to
		 * establish a flag which definitely tells us that some
		 * hardware is absent and which we could check here? */
		FN_EXIT(0, NOT_OK);
		return IRQ_NONE;
	}
		
	CLEAR_BIT(irqtype, priv->irq_mask); /* check only "interesting" IRQ types */
	/* pm_access(priv->pm); OUTDATED, thus disabled at the moment */
	acxlog(L_IRQ, "IRQTYPE: 0x%X, irq_mask: 0x%X\n", irqtype, priv->irq_mask);
	/* immediately return if we don't get signalled that an interrupt
	 * has occurred that we are interested in (interrupt sharing
	 * with other cards!) */
	if (0 == irqtype) {
		FN_EXIT(0, NOT_OK);
		return IRQ_NONE;
	}

#define IRQ_ITERATE 1
#if IRQ_ITERATE
  if (jiffies != last_irq_jiffies) {
      loops_this_jiffy = 0;
      last_irq_jiffies = jiffies;
  }

  /* safety condition; we'll normally abort loop below
   * in case no IRQ type occurred */
  while (irqcount-- > 0) {
#endif

        /* do most important IRQ types first */
	if (0 != (irqtype & HOST_INT_RX_COMPLETE)) {
		acx_process_rx_desc(priv);
		acx_write_reg16(priv, priv->io[IO_ACX_IRQ_ACK], HOST_INT_RX_COMPLETE);
		acxlog(L_IRQ, "Got Rx Complete IRQ\n");
	}
	if (0 != (irqtype & HOST_INT_TX_COMPLETE)) {
		/* don't clean up on each Tx complete, wait a bit */
		if (++priv->tx_cnt_done % (priv->TxQueueCnt >> 2) == 0)
		{
#if TX_CLEANUP_IN_SOFTIRQ
			acx_schedule_after_interrupt_task(priv, ACX_AFTER_IRQ_CMD_TX_CLEANUP);
#else
			acx_clean_tx_desc(priv);
#endif

			/* no need to set tx_cnt_done back to 0 here, since
			 * an overflow cannot cause counter misalignment on
			 * check above */
		}
		acx_write_reg16(priv, priv->io[IO_ACX_IRQ_ACK], HOST_INT_TX_COMPLETE);
		acxlog(L_IRQ, "Got Tx Complete IRQ\n");
#if 0
/* this shouldn't happen here generally, since we'd also enable user packet
 * xmit for management packets, which we really DON'T want */
/* BS: disabling this caused my card to stop working after a few 
 * seconds when floodpinging. This should be reinvestigated! */
		  if (acx_queue_stopped(dev_id)) {
			  acx_wake_queue(dev_id, "after Tx complete");
		  }
#endif
	}
	/* group all further IRQ types to improve performance */
	if (0 != (irqtype & (  /* 00f5 */
		HOST_INT_RX_DATA |
		HOST_INT_TX_XFER |
		HOST_INT_DTIM |
		HOST_INT_BEACON |
		HOST_INT_TIMER |
		HOST_INT_KEY_NOT_FOUND
		)
	)) {
		if (0 != (irqtype & HOST_INT_RX_DATA)) {
			acx_write_reg16(priv, priv->io[IO_ACX_IRQ_ACK], HOST_INT_RX_DATA);
			acxlog(L_STD|L_IRQ, "Got Rx Data IRQ\n");
		}
		if (0 != (irqtype & HOST_INT_TX_XFER)) {
			acx_write_reg16(priv, priv->io[IO_ACX_IRQ_ACK], HOST_INT_TX_XFER);
			acxlog(L_STD|L_IRQ, "Got Tx Xfer IRQ\n");
		}
		if (0 != (irqtype & HOST_INT_DTIM)) {
			acx_write_reg16(priv, priv->io[IO_ACX_IRQ_ACK], HOST_INT_DTIM);
			acxlog(L_STD|L_IRQ, "Got DTIM IRQ\n");
		}
		if (0 != (irqtype & HOST_INT_BEACON)) {
			acx_write_reg16(priv, priv->io[IO_ACX_IRQ_ACK], HOST_INT_BEACON);
			acxlog(L_STD|L_IRQ, "Got Beacon IRQ\n");
		}
		if (0 != (irqtype & HOST_INT_TIMER)) {
			acx_timer((unsigned long)priv);
			acx_write_reg16(priv, priv->io[IO_ACX_IRQ_ACK], HOST_INT_TIMER);
			acxlog(L_IRQ, "Got Timer IRQ\n");
		}
		if (unlikely(0 != (irqtype & HOST_INT_KEY_NOT_FOUND))) {
			acx_write_reg16(priv, priv->io[IO_ACX_IRQ_ACK], HOST_INT_KEY_NOT_FOUND);
			acxlog(L_STD|L_IRQ, "Got Key Not Found IRQ\n");
		}
	}
	if (0 != (irqtype & (  /* 0f00 */
		HOST_INT_IV_ICV_FAILURE |
		HOST_INT_CMD_COMPLETE |
		HOST_INT_INFO |
		HOST_INT_OVERFLOW
		)
	)) {
		if (unlikely(0 != (irqtype & HOST_INT_IV_ICV_FAILURE))) {
			acx_write_reg16(priv, priv->io[IO_ACX_IRQ_ACK], HOST_INT_IV_ICV_FAILURE);
			acxlog(L_STD|L_IRQ, "Got IV ICV Failure IRQ\n");
		}
		if (0 != (irqtype & HOST_INT_CMD_COMPLETE)) {
			acx_write_reg16(priv, priv->io[IO_ACX_IRQ_ACK], HOST_INT_CMD_COMPLETE);
			/* save the state for the running issue cmd */
			SET_BIT(priv->irq_status, HOST_INT_CMD_COMPLETE);
			acxlog(L_IRQ, "Got Command Complete IRQ\n");
		}
		if (0 != (irqtype & HOST_INT_INFO)) {
			acx_handle_info_irq(priv);
			acx_write_reg16(priv, priv->io[IO_ACX_IRQ_ACK], HOST_INT_INFO);
		}
		if (unlikely(0 != (irqtype & HOST_INT_OVERFLOW))) {
			acx_write_reg16(priv, priv->io[IO_ACX_IRQ_ACK], HOST_INT_OVERFLOW);
			acxlog(L_STD|L_IRQ, "Got Overflow IRQ\n");
		}
	}
	if (0 != (irqtype & ( /* f000 */
		HOST_INT_PROCESS_ERROR |
		HOST_INT_SCAN_COMPLETE |
		HOST_INT_FCS_THRESHOLD |
		HOST_INT_UNKNOWN
		)
	)) {
		if (unlikely(0 != (irqtype & HOST_INT_PROCESS_ERROR))) {
			acx_write_reg16(priv, priv->io[IO_ACX_IRQ_ACK], HOST_INT_PROCESS_ERROR);
			acxlog(L_STD|L_IRQ, "Got Process Error IRQ\n");
		}
		if (0 != (irqtype & HOST_INT_SCAN_COMPLETE)) {

			/* place after_interrupt_task into schedule to get
			   out of interrupt context */
			acx_schedule_after_interrupt_task(priv, 0);

			acx_write_reg16(priv, priv->io[IO_ACX_IRQ_ACK], HOST_INT_SCAN_COMPLETE);
			SET_BIT(priv->irq_status, HOST_INT_SCAN_COMPLETE);

			acxlog(L_IRQ, "<%s> HOST_INT_SCAN_COMPLETE\n", __func__);
		}
		if (0 != (irqtype & HOST_INT_FCS_THRESHOLD)) {
			acx_write_reg16(priv, priv->io[IO_ACX_IRQ_ACK], HOST_INT_FCS_THRESHOLD);
			acxlog(L_STD|L_IRQ, "Got FCS Threshold IRQ\n");
		}
		if (unlikely(0 != (irqtype & HOST_INT_UNKNOWN))) {
			acx_write_reg16(priv, priv->io[IO_ACX_IRQ_ACK], HOST_INT_UNKNOWN);
			acxlog(L_STD|L_IRQ, "Got Unknown IRQ\n");
		}
	}
/*	acx_write_reg16(priv, priv->io[IO_ACX_IRQ_ACK], irqtype); */
#if IRQ_ITERATE
	irqtype = acx_read_reg16(priv, priv->io[IO_ACX_IRQ_STATUS_CLEAR]) & ~(priv->irq_mask);
	if (0 == irqtype)
		break;
    if (unlikely(++loops_this_jiffy > MAX_IRQLOOPS_PER_JIFFY)) {
      acxlog(-1, "HARD ERROR: Too many interrupts this jiffy\n");
      priv->irq_mask = 0;
        break;
        }
    	/* log irqtype going to be used during new iteration */
	acxlog(L_IRQ, "IRQTYPE: %X\n", irqtype);
   }
#endif
	/* Routine to perform blink with range */
	if (unlikely(priv->led_power == 2))
		acx_update_link_quality_led(priv);

	FN_EXIT(0, OK);
	return IRQ_HANDLED;
}

/*------------------------------------------------------------------------------
 * acx_rx
 *
 * The end of the Rx path. Pulls data from a rxhostdescriptor into a socket
 * buffer and feeds it to the network stack via netif_rx().
 *
 * Arguments:
 * 	rxdesc:	the rxhostdescriptor to pull the data from
 *	priv:	the acx100 private struct of the interface
 *
 * Returns:
 *
 * Side effects:
 *
 * Call context:
 *
 * STATUS:
 * 	*much* better now, maybe finally bug-free, VERIFIED.
 *
 * Comment:
 *
 *----------------------------------------------------------------------------*/
void acx_rx(struct rxhostdescriptor *rxdesc, wlandevice_t *priv)
{
	FN_ENTER;
	if (likely(0 != (priv->dev_state_mask & ACX_STATE_IFACE_UP))) {
		struct sk_buff *skb;
		skb = acx_rxdesc_to_ether(priv, rxdesc);
		if (likely(NULL != skb)) {
			(void)netif_rx(skb);
			priv->netdev->last_rx = jiffies;
			priv->stats.rx_packets++;
			priv->stats.rx_bytes += skb->len;
		}
	}
	FN_EXIT(0, OK);
}

/*----------------------------------------------------------------
* init_module
*
* Module initialization routine, called once at module load time.
* This one simulates some of the pcmcia calls.
*
* Arguments:
*	none
*
* Returns:
*	0	- success
*	~0	- failure, module is unloaded.
*
* Side effects:
* 	alot
*
* Call context:
*	process thread (insmod or modprobe)
* STATUS: should be ok.. NONV3.
----------------------------------------------------------------*/

#ifdef MODULE

/* introduced earlier than 2.6.10, but takes more memory, so don't use it
 * if there's no compile warning by kernel */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 10)
#ifdef ACX_DEBUG
module_param(debug, uint, 0);
#endif
module_param(use_eth_name, uint, 0);
module_param(firmware_dir, charp, 0);
#else
#ifdef ACX_DEBUG
MODULE_PARM(debug, "i"); /* doh, 2.6.x screwed up big time: here the define has its own ";" ("double ; detected"), yet in 2.4.x it DOESN'T (the sane thing to do), grrrrr! */
#endif
MODULE_PARM(use_eth_name, "i");
MODULE_PARM(firmware_dir, "s");
#endif

/*@-fullinitblock@*/
MODULE_PARM_DESC(debug, "Debug level mask: 0x0000 - 0x3fff (see L_xxx in include/acx100.h)");
MODULE_PARM_DESC(use_eth_name, "Allocate device ethX instead of wlanX");
MODULE_PARM_DESC(firmware_dir, "Directory to load acx100 firmware files from");
/*@=fullinitblock@*/

static int __init acx_init_module(void)
{
	int res;

	FN_ENTER;

	acxlog(L_STD,
	       "acx100: It looks like you've been coaxed into buying a wireless network card\n"
	       "acx100: that uses the mysterious ACX100/ACX111 chip from Texas Instruments.\n"
	       "acx100: You should better have bought e.g. a PRISM(R) chipset based card,\n"
	       "acx100: since that would mean REAL vendor Linux support.\n"
	       "acx100: Given this info, it's evident that this driver is still EXPERIMENTAL,\n"
	       "acx100: thus your mileage may vary. Reading README file and/or Craig's HOWTO is\n"
	       "recommended, visit http://acx100.sf.net in case of further questions/discussion.\n");

#if (WLAN_HOSTIF==WLAN_USB)
	acxlog(L_STD, "acx100: ENABLED USB SUPPORT!\n");
#endif

#if (ACX_IO_WIDTH==32)
	acxlog(L_STD, "acx100: Compiled to use 32bit I/O access (faster, however I/O timing issues might occur, such as firmware upload failure!) instead of 16bit access\n");
#else
	acxlog(L_STD, "acx100: Warning: compiled to use 16bit I/O access only (compatibility mode). Set Makefile ACX_IO_WIDTH=32 to use slightly problematic 32bit mode.\n");
#endif

	acxlog(L_BINDEBUG, "%s: dev_info is: %s\n", __func__, dev_info);
	acxlog(L_STD|L_INIT, "%s: %s Driver initialized, waiting for cards to probe...\n", __func__, version);

	res = pci_module_init(&acx_pci_drv_id);
	FN_EXIT(1, res);
	return res;
}

/*----------------------------------------------------------------
* cleanup_module
*
* Called at module unload time.  This is our last chance to
* clean up after ourselves.
*
* Arguments:
*	none
*
* Returns:
*	nothing
*
* Side effects:
* 	alot
*
* Call context:
*	process thread
*
* STATUS: should be ok.
----------------------------------------------------------------*/

static void __exit acx_cleanup_module(void)
{
	const struct net_device *dev;

	FN_ENTER;
	
	/* Since the whole module is about to be unloaded,
	 * we recursively shutdown all cards we handled instead
	 * of doing it in remove_pci() (which will be activated by us
	 * via pci_unregister_driver at the end).
	 * remove_pci() might just get called after a card eject,
	 * that's why hardware operations have to be done here instead
	 * when the hardware is available. */

	dev = root_acx_dev.newest;
	while (dev != NULL) {
		wlandevice_t *priv = (struct wlandevice *) dev->priv;

		/* disable both Tx and Rx to shut radio down properly */
		acx_issue_cmd(priv, ACX1xx_CMD_DISABLE_TX, NULL, 0, ACX_CMD_TIMEOUT_DEFAULT);
		acx_issue_cmd(priv, ACX1xx_CMD_DISABLE_RX, NULL, 0, ACX_CMD_TIMEOUT_DEFAULT);
	
		/* disable power LED to save power :-) */
		acxlog(L_INIT, "switching off power LED to save power :-)\n");
		acx_power_led(priv, (u8)0);

#if REDUNDANT
		/* put the eCPU to sleep to save power
		 * Halting is not possible currently,
		 * since not supported by all firmware versions */
		acx_issue_cmd(priv, ACX100_CMD_SLEEP, NULL, 0, ACX_CMD_TIMEOUT_DEFAULT);
#endif

		/* stop our eCPU */
		if(priv->chip_type == CHIPTYPE_ACX111) {
			/* FIXME: does this actually keep halting the eCPU?
			 * I don't think so...
			 */
			acx_reset_mac(priv);
		}
		else if (CHIPTYPE_ACX100 == priv->chip_type) {
			u16 temp;

			/* halt eCPU */
			temp = acx_read_reg16(priv, priv->io[IO_ACX_ECPU_CTRL]) | 0x1;
			acx_write_reg16(priv, priv->io[IO_ACX_ECPU_CTRL], temp);

		}

		dev = priv->prev_nd;
	}

	/* now let the PCI layer recursively remove
	 * all PCI related things (acx_remove_pci()) */
	pci_unregister_driver(&acx_pci_drv_id);
	
	FN_EXIT(0, OK);
}

module_init(acx_init_module)
module_exit(acx_cleanup_module)

#else
static int __init acx_get_firmware_dir(const char *str)
{
	/* I've seen other drivers just pass the string pointer,
	 * so hopefully that's safe */
	firmware_dir = str;
	return OK;
}

__setup("acx_firmware_dir=", acx_get_firmware_dir);
#endif /* MODULE */
