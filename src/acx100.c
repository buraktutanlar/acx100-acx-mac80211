/* src/acx100.c - main module functions
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
 * single spinlock, hw->lock.  The lock is used in user, bh and irq
 * context, so when taken outside hardirq context it should always be
 * taken with interrupts disabled.  The lock protects both the
 * hardware and the struct wlandevice.
 *
 * Another flag, hw->hw_unavailable indicates that the hardware is
 * unavailable for an extended period of time (e.g. suspended, or in
 * the middle of a hard reset).  This flag is protected by the
 * spinlock.  All code which touches the hardware should check the
 * flag after taking the lock, and if it is set, give up on whatever
 * they are doing and drop the lock again.  The acx100_lock()
 * function handles this (it unlocks and returns -EBUSY if
 * hw_unavailable is true). */

/*================================================================*/
/* System Includes */
#include <linux/config.h>
#define WLAN_DBVAR	prism2_debug
#include <linux/version.h>

#include <linux/kernel.h>
#include <linux/module.h>

#include <linux/sched.h>
#include <linux/types.h>
#include <linux/skbuff.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/if_arp.h>
#include <linux/rtnetlink.h>
#include <linux/wireless.h>
#include <linux/netdevice.h>

#include <wlan_compat.h>

#include <linux/ioport.h>

#include <linux/pci.h>
#include <linux/init.h>

#include <linux/pm.h>

#include <linux/dcache.h>
#include <linux/highmem.h>
#include <linux/sched.h>
#include <linux/skbuff.h>
#include <linux/etherdevice.h>

/*================================================================*/
/* Project Includes */

#include <version.h>
#include <p80211hdr.h>
#include <p80211mgmt.h>
#include <acx100.h>
#include <acx100_conv.h>
#include <p80211netdev.h>
#include <p80211types.h>
#include <acx100_helper.h>
#include <acx100_helper2.h>
#include <idma.h>
#include <ihw.h>
#include <acx100mgmt.h>

/********************************************************************/
/* Module information                                               */
/********************************************************************/

MODULE_AUTHOR("The ACX100 Open Source Driver development team");
MODULE_DESCRIPTION("Driver for TI ACX100 based wireless cards");
#ifdef MODULE_LICENSE
MODULE_LICENSE("Dual MPL/GPL");
#endif

/*================================================================*/
/* Local Constants */
#define PCI_TYPE		(PCI_USES_MEM | PCI_ADDR0 | PCI_NO_ACPI_WAKE)
#define PCI_SIZE		0x1000	/* Memory size - 4K bytes */
#define PCI_SIZE2   0x10000

/* ACX100 22Mb/s WLAN controller */
#define PCI_VENDOR_ID_TI		0x104c
#define PCI_DEVICE_ID_TI_ACX100		0x8400
#define PCI_DEVICE_ID_TI_ACX100_CB	0x8401

/* PCI Class & Sub-Class code, Network-'Other controller' */
#define PCI_CLASS_NETWORK_OTHERS 0x280

/*================================================================*/
/* Local Macros */

/*================================================================*/
/* Local Types */

/*================================================================*/
/* Local Static Definitions */
#define DRIVER_SUFFIX	"_pci"

#define MAX_WLAN_DEVICES 4
#define CARD_EEPROM_ID_SIZE 6
#define MAX_IRQLOOPS_PER_JIFFY  (20000/HZ) //a la orinoco.c

typedef char *dev_info_t;
static dev_info_t dev_info = "TI acx100" DRIVER_SUFFIX;

static char *version = "TI acx100" DRIVER_SUFFIX ".o: " WLAN_RELEASE;

#ifdef DEBUG
int debug = 0x9b;
#endif

int use_eth_name = 0;

char *firmware_dir;

extern const struct iw_handler_def acx100_ioctl_handler_def;

typedef struct device_id {
	unsigned char id[6];
	char *descr;
	char *type;
} device_id_t;

static device_id_t device_ids[] =
{
	{
		"Global",
		NULL,
		NULL,
	},
	{
		{0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
		"uninitialised",
		"SpeedStream SS1021 or Gigafast WF721-AEX"
	},
	{
		{0x80, 0x81, 0x82, 0x83, 0x84, 0x85},
		"non-standard",
		"DrayTek Vigor 520"
	},
	{
		"??????",
		"non-standard",
		"Level One WPC-0200"
	},
	{
		{0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
		"empty",
		"DWL-650+ variant"
	}
};

static struct pci_device_id acx100_pci_id_tbl[] = {
	{
		.vendor = PCI_VENDOR_ID_TI,
		.device = PCI_DEVICE_ID_TI_ACX100,
		.subvendor = PCI_ANY_ID,
		.subdevice = PCI_ANY_ID,
	},
	{
		.vendor = PCI_VENDOR_ID_TI,
		.device = PCI_DEVICE_ID_TI_ACX100_CB,
		.subvendor = PCI_ANY_ID,
		.subdevice = PCI_ANY_ID,
	},
	{ 0, }
};

MODULE_DEVICE_TABLE(pci, acx100_pci_id_tbl);

static int acx100_probe_pci(struct pci_dev *pdev,
			    const struct pci_device_id *id);
static void acx100_remove_pci(struct pci_dev *pdev);

static int acx100_pm_callback(struct pm_dev *dev, pm_request_t rqst, void *data);


static struct pci_driver acx100_pci_drv_id = {
	.name        = "acx100_pci",
	.id_table    = acx100_pci_id_tbl,
	.probe       = acx100_probe_pci,
	.remove      = __devexit_p(acx100_remove_pci),
};

typedef struct acx100_device {
	netdevice_t *newest;

} acx100_device_t;

/* if this driver was only about PCI devices, then we probably wouldn't
 * need this linked list.
 * But if we want to register ALL kinds of devices in one global list,
 * then we need it and need to maintain it properly. */
static struct acx100_device root_acx100_dev = {
	.newest        = NULL,
};


static int acx100_start_xmit(struct sk_buff *skb, netdevice_t * dev);
static void acx100_tx_timeout(netdevice_t * dev);
static struct net_device_stats *acx100_get_stats(netdevice_t * hw);
static struct iw_statistics *acx100_get_wireless_stats(netdevice_t * hw);

irqreturn_t acx100_interrupt(int irq, void *dev_id, struct pt_regs *regs);
static void acx100_set_rx_mode(netdevice_t * netdev);
void acx100_rx(/*wlan_pb_t * p80211*/
	struct rxhostdescriptor *rxdesc, wlandevice_t *hw);

static int acx100_open(netdevice_t * dev);
static int acx100_close(netdevice_t * dev);
static void acx100_up(netdevice_t * dev);
static void acx100_down(netdevice_t * dev);

static void acx100_get_firmware_version(wlandevice_t *wlandev)
{
	struct {
		UINT16 type;
		UINT16 length;
		char fw_id[20];
		UINT32 val0x14;
	} fw;
	char fw_major = 0, fw_minor = 0, fw_sub = 0, fw_extra = 0;

	FN_ENTER;
	
	acx100_interrogate(wlandev, &fw, ACX100_RID_FWREV);
	memcpy(wlandev->firmware_version, fw.fw_id, 20);
	if (strncmp(fw.fw_id, "Rev ", 4))
	{
		acxlog(L_STD|L_INIT, "Huh, strange firmware version string \"%s\" without leading \"Rev \" string detected, please report!\n", fw.fw_id);
		wlandev->firmware_numver = 0x01090407; /* assume 1.9.4.7 */
		return;
	}
	fw_major = fw.fw_id[4] - '0';
	fw_minor = fw.fw_id[6] - '0';
	fw_sub = fw.fw_id[8] - '0';
	if (strlen(fw.fw_id) >= 11)
	{
		if ((fw.fw_id[10] >= '0') && (fw.fw_id[10] <= '9'))
			fw_extra = fw.fw_id[10] - '0';
		else
			fw_extra = fw.fw_id[10] - 'a' + 10;
	}
	wlandev->firmware_numver =
		(fw_major << 24) + (fw_minor << 16) + (fw_sub << 8) + fw_extra;
	acxlog(L_DEBUG, "firmware_numver %08lx\n", wlandev->firmware_numver);

	wlandev->firmware_id = fw.val0x14;

	FN_EXIT(0, 0);
}

/*----------------------------------------------------------------
* acx100_display_hardware_details
*
*
* Arguments:
*	wlandev: ptr to wlandevice that contains all the details 
*	  displayed by this function
* Returns:
*	void
*
* Side effects:
*	none
* Call context:
*	acx100_probe_pci
* STATUS:
*	stable
* Comment:
*	This function will display strings to the system log according 
* to device form_factor and radio type. It will needed to be 
*----------------------------------------------------------------*/

void acx100_display_hardware_details(wlandevice_t *wlandev)
{
	char *radio_str, *form_str;

	FN_ENTER;

	switch(wlandev->radio_type) {
		case 0x11:
			radio_str = "RFMD";
			break;
		case 0x0d:
			/* hmm, the DWL-650+ seems to have two variants,
			 * according to a windows driver changelog comment:
			 * RFMD and Maxim. */
			radio_str = "Maxim";
			break;
		case 0x15:
			radio_str = "UNKNOWN, used e.g. in DrayTek Vigor 520, please report the radio type name!";
			break;
		default:
			radio_str = "UNKNOWN, please report the radio type name!";
			break;
	}

	switch(wlandev->form_factor) {
		case 0x00:
			form_str = "standard?";
			break;
		case 0x01:
			form_str = "D-Link DWL-520+/650+/Planet WL-8305?";
			break;
		default:
			form_str = "UNKNOWN, please report!";
			break;
	}

	acxlog(L_STD, "acx100: form factor 0x%02x (%s), radio type 0x%02x (%s), EEPROM version 0x%04x. Uploaded firmware '%s' (0x%08lx).\n", wlandev->form_factor, form_str, wlandev->radio_type, radio_str, wlandev->eeprom_version, wlandev->firmware_version, wlandev->firmware_id);

	FN_EXIT(0, 0);
}
/*----------------------------------------------------------------
* acx100_probe_pci
*
* Probe routine called when a PCI device w/ matching ID is found.
* The ISL3874 implementation uses the following map:
*   BAR0: Prism2.x registers memory mapped, size=4k
* Here's the sequence:
*   - Allocate the PCI resources.
*   - Read the PCMCIA attribute memory to make sure we have a WLAN card
*   - Reset the MAC
*   - Initialize the netdev and wlan data
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
acx100_probe_pci(struct pci_dev *pdev, const struct pci_device_id *id)
{
	int result = -EIO;
	int err;

	unsigned long phymem1;
	unsigned long phymem2;
	unsigned long mem1 = 0;
	unsigned long mem2 = 0;

	wlandevice_t *wlandev = NULL;
	struct net_device *netdev = NULL;
	unsigned char buffer[CARD_EEPROM_ID_SIZE];
	unsigned int i;
	UINT32 hardware_info;
	char *devname_mask;

	FN_ENTER;

	/* FIXME: flag device somehow to make sure ioctls don't have access
	 * to uninitialized structures before init is finished */

	/* Enable the PCI device */

	if (pci_enable_device(pdev) != 0) {
		acxlog(L_BINSTD | L_INIT,
		       "%s: %s: pci_enable_device() failed\n",
		       __func__, dev_info);
		result = -EIO;
		goto fail;
	}

	/* enable busmastering (required for CardBus) */
	pci_set_master(pdev);
#if DOES_NOT_WORK
	/* on my Dell Inspiron 8000, if I try to suspend it,
	 * the notebook immediately resumes after shutdown when my
	 * ACX100 mini-PCI card is installed. This is obviously not useful :-(
	 * Thus I'm trying to fix this severe problem by playing with
	 * PCI power management bits. So far it's not very successful
	 * :-\
	 */
	acxlog(L_DEBUG, "wake: %d\n", pci_enable_wake(pdev, 0, 0));
#endif

	/* Figure out our resources */
	phymem1 = pci_resource_start(pdev, 1);
	phymem2 = pci_resource_start(pdev, 2);

	if (!request_mem_region
	    (phymem1, pci_resource_len(pdev, 1), "Acx100_1")) {
		acxlog(L_BINSTD | L_INIT,
		       "%s: acx100: Cannot reserve PCI memory region 1 (or also: are you sure you have CardBus support in kernel?)\n", __func__);
		result = -EIO;
		goto fail;
	}

	if (!request_mem_region
	    (phymem2, pci_resource_len(pdev, 2), "Acx100_2")) {
		acxlog(L_BINSTD | L_INIT,
		       "%s: acx100: Cannot reserve PCI memory region 2\n", __func__);
		result = -EIO;
		goto fail;
	}

	mem1 = (unsigned long) ioremap(phymem1, PCI_SIZE);
	if (mem1 == 0) {
		acxlog(L_BINSTD | L_INIT,
		       "%s: %s: ioremap() failed.\n",
		       __func__, dev_info);
		result = -EIO;
		goto fail;
	}

	mem2 = (unsigned long) ioremap(phymem2, PCI_SIZE2);
	if (mem2 == 0) {
		acxlog(L_BINSTD | L_INIT,
		       "%s: %s: ioremap() failed.\n",
		       __func__, dev_info);
		result = -EIO;
		goto fail;
	}

	/* Log the device */
	acxlog(L_STD | L_INIT,
	       "Found ACX100-based wireless network card, phymem1:0x%x, phymem2:0x%x, irq:%d, mem1:0x%x, mem2:0x%x\n",
	       (unsigned int) phymem1, (unsigned int) phymem2, pdev->irq,
	       (unsigned int) mem1, (unsigned int) mem2);

	acxlog(L_INIT, "Allocating %d, %Xh bytes for wlandevice_t\n",sizeof(wlandevice_t),sizeof(wlandevice_t));
	if ((wlandev = kmalloc(sizeof(wlandevice_t), GFP_KERNEL)) == NULL) {
		acxlog(L_BINSTD | L_INIT,
		       "%s: %s: Memory allocation failure\n",
		       __func__, dev_info);
		result = -EIO;
		goto fail;
	}

	memset(wlandev, 0, sizeof(wlandevice_t));

	spin_lock_init(&wlandev->lock);

	wlandev->open = 0;
	acxlog(L_INIT, "hw_unavailable = 1\n");
	wlandev->hw_unavailable = 1;

	wlandev->membase = phymem1;
	wlandev->iobase = mem1;
	wlandev->pvMemBaseAddr1 = mem1;

	wlandev->membase2 = phymem2;
	wlandev->iobase2 = mem2;
	wlandev->pvMemBaseAddr2 = mem2;

	wlandev->mgmt_timer.function = (void *)0x0000dead; /* to find crashes due to weird driver access to unconfigured interface (ifup) */

	memset(&buffer, 0, CARD_EEPROM_ID_SIZE);
	for (i = 0; i < CARD_EEPROM_ID_SIZE; i++) {
		if (!(acx100_read_eeprom_offset(wlandev,
					 ACX100_EEPROM_ID_OFFSET + i,
					 &buffer[i])))
		{
			acxlog(L_STD, "huh, reading EEPROM failed!?\n");
			break;
		}
	}
	for (i = 0; i < sizeof(device_ids) / sizeof(struct device_id); i++)
	{
		if (!memcmp(&buffer, device_ids[i].id, CARD_EEPROM_ID_SIZE))
		{
			if (device_ids[i].descr != NULL)
			acxlog(L_STD, "%s: EEPROM card ID string check found %s card ID: this is a %s, no??\n", __func__, device_ids[i].descr, device_ids[i].type);
			break;
		}
	}
	if (i == sizeof(device_ids) / sizeof(device_id_t))
	{
		acxlog(L_STD,
	       "%s: EEPROM card ID string check found unknown card: expected \"Global\", got \"%.*s\"! Please report!\n", __func__, CARD_EEPROM_ID_SIZE, buffer);
	}

	if ((netdev = kmalloc(sizeof(netdevice_t), GFP_ATOMIC)) == NULL) {
		acxlog(L_BINSTD | L_INIT,
		       "%s: Failed to alloc netdev\n", __func__);
		result = -EIO;
		goto fail;
	}

	memset(netdev, 0, sizeof(netdevice_t));
	ether_setup(netdev);

#if QUEUE_OPEN_AFTER_ASSOC
	/* now we have our device, so make sure the kernel doesn't try
	 * to send packets even though we're not associated to a network yet */
	acxlog(L_XFER, "stop queue after setup.\n");
	netif_stop_queue(netdev);
#endif

	netdev->priv = wlandev;

	/* register new netdev in linked list */
	wlandev->prev_nd = root_acx100_dev.newest;
	root_acx100_dev.newest = netdev;
	wlandev->netdev = netdev;

	if (pdev->irq == 0) {
		acxlog(L_BINSTD | L_IRQ | L_INIT,
		       "%s: %s: Can't get IRQ %d\n",
		       __func__, dev_info, 0);
		result = -EIO;
		goto fail;
	}

	netdev->irq = pdev->irq;
	netdev->base_addr = pci_resource_start(pdev, 0);

	if (!acx100_reset_dev(netdev)) {
		acxlog(L_BINSTD | L_INIT,
		       "%s: %s: MAC initialize failure!\n",
		       __func__, dev_info);
		result = -EIO;
		goto fail;
	}

	devname_mask = (use_eth_name) ? "eth%d" : "wlan%d";
	if (dev_alloc_name(netdev, devname_mask) < 0)
	{
		result = -EIO;
		goto fail;
	}

	/* now that device init was successful, fill remaining fields... */
	netdev->open = &acx100_open;
	netdev->stop = &acx100_close;
	netdev->hard_start_xmit = &acx100_start_xmit;
	/* FIXME: no handler for netdev->hard_reset! could reset CPU,
	 * reinit packet templates etc. */
	netdev->get_stats = &acx100_get_stats;
	netdev->get_wireless_stats = &acx100_get_wireless_stats;
#if WIRELESS_EXT >= 13
	netdev->wireless_handlers = (struct iw_handler_def *)&acx100_ioctl_handler_def;
#endif
	netdev->do_ioctl = &acx100_ioctl_main;
	netdev->set_multicast_list = &acx100_set_rx_mode;
	netdev->tx_timeout = &acx100_tx_timeout;
	netdev->watchdog_timeo = 4 * HZ;	/* 400 */

	/* ok, basic setup is finished, now start initialising the card */

	hardware_info = acx100_read_reg16(wlandev, 0x2ac);
	wlandev->form_factor = hardware_info & 0xff;
	wlandev->radio_type = hardware_info >> 8 & 0xff;
//	wlandev->eeprom_version = hardware_info >> 16;
	acx100_read_eeprom_offset(wlandev, 0x05, &wlandev->eeprom_version);

	if (acx100_init_mac(netdev) != 0) {
		acxlog(L_DEBUG | L_INIT,
		       "Danger Will Robinson, MAC did not come back\n");
		result = -EIO;
		goto fail_registered;
	}

	/* card initialized, so let's release hardware */
	acxlog(L_INIT, "hw_unavailable--\n");
	wlandev->hw_unavailable--;

	/* needs to be after acx100_init_mac() due to necessary init stuff */
	acx100_get_firmware_version(wlandev);

	acx100_display_hardware_details(wlandev);

	/* ...and register the card, AFTER everything else has been set up,
	 * since otherwise an ioctl could step on our feet due to
	 * firmware operations happening in parallel or uninitialized data */
	if ((err = register_netdev(netdev)) != 0) {
		acxlog(L_BINSTD | L_INIT,
		       "%s: %s: Register net device of %s failed: %d\n\n",
		       __func__, dev_info, netdev->name, err);
		result = -EIO;
		goto fail;
	}

	pci_set_drvdata(pdev, netdev);
	wlandev->pm = pm_register(PM_PCI_DEV,PM_PCI_ID(pdev),
			&acx100_pm_callback);

        create_proc_read_entry("driver/acx100", 0, 0, acx100_read_proc, wlandev);

	result = 0;
	goto done;


fail_registered:
	unregister_netdev(netdev);
fail:
	if (wlandev)
	{
		if (wlandev->pm)
			pm_unregister(wlandev->pm);
		kfree(wlandev);
	}
	
	if (netdev)
		kfree(netdev);

	if (mem1)
		iounmap((void *) mem1);
	if (mem2)
		iounmap((void *) mem2);

	release_mem_region(pci_resource_start(pdev, 1),
			   pci_resource_len(pdev, 1));

	release_mem_region(pci_resource_start(pdev, 2),
			   pci_resource_len(pdev, 2));
	pci_disable_device(pdev);

done:
	FN_EXIT(1, result);
	return result;
} /* acx100_probe_pci() */


/*----------------------------------------------------------------
* acx100_remove_pci
*
* Deallocate PCI resources for the ACX100 chip.
*
* Arguments:
*	pdev		ptr to pci device structure containing info about
*			pci configuration.
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
* Comment: The function was rewritten according to the V3 driver.
----------------------------------------------------------------*/
void __devexit acx100_remove_pci(struct pci_dev *pdev)
{
	struct net_device *netdev;
	wlandevice_t *hw;
	struct net_device *querydev;
	struct net_device *olderdev;
	struct net_device *newerdev;

	FN_ENTER;

	netdev = (struct net_device *) pci_get_drvdata(pdev);
	hw = (struct wlandevice *) netdev->priv;

        remove_proc_entry("driver/acx100", NULL);
	/* unregister the device to not let the kernel
	 * (e.g. ioctls) access a half-deconfigured device */
	netif_device_detach(netdev);
	unregister_netdev(netdev);

	pm_unregister(hw->pm);

	/* disable both Tx and Rx to shut radio down properly */
	acx100_issue_cmd(hw, ACX100_CMD_DISABLE_TX, NULL, 0, 5000);
	acx100_issue_cmd(hw, ACX100_CMD_DISABLE_RX, NULL, 0, 5000);

	/* disable power LED to save power :-) */
	acxlog(L_INIT, "switching off power LED to save power. :-)\n");
	acx100_power_led(hw, 0);

	/* put the eCPU to sleep to save power
	 * Halting is not possible currently,
	 * since not supported by all firmware versions */
	acx100_issue_cmd(hw, ACX100_CMD_SLEEP, 0, 0, 5000);

	/* find our PCI device in the global acx100 list and remove it */
	querydev = (struct net_device *) root_acx100_dev.newest;
	newerdev = NULL;
	while (querydev != NULL) {
		olderdev = ((struct wlandevice *) querydev->priv)->prev_nd;
		if (strcmp(querydev->name, netdev->name) == 0) {
			if (newerdev == NULL) {
				/* if we were at the beginning of the
				 * list, then it's the list head that
				 * we need to update to point at the
				 * next older device */
				root_acx100_dev.newest = olderdev;
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
		 * but since the list starts (root_acx100_dev.newest)
		 * with the newest devices,
		 * it's newer than the ones following.
		 * Oh the joys of iterating from newest to oldest :-\ */
		newerdev = querydev;

		/* keep checking old devices for matches until we hit the end
		 * of the list */
		querydev = olderdev;
	}

	if (!hw->open)
		acx100_down(netdev);

	hw->open = 0;

	acxlog(L_STD, "hw_unavailable++\n");
	hw->hw_unavailable++;

	acx100_delete_dma_region(hw);

	if (netdev)
		kfree(netdev);

	iounmap((void *) hw->iobase);
	iounmap((void *) hw->iobase2);
	if (hw)
		kfree(hw);

	/* finally, clean up PCI bus state */

	release_mem_region(pci_resource_start(pdev, 1),
			   pci_resource_len(pdev, 1));

	release_mem_region(pci_resource_start(pdev, 2),
			   pci_resource_len(pdev, 2));

	/* put device into ACPI D3 mode (shutdown) */
	pci_set_power_state(pdev, 3);

	pci_set_drvdata(pdev, NULL);

	acxlog(L_BINSTD | L_INIT, "Device %s removed!\n", netdev->name);

	FN_EXIT(0, 0);
}


static int acx100_pm_callback(struct pm_dev *dev, pm_request_t rqst, void *data)
{
	int result = 0;
	netdevice_t *ndev = root_acx100_dev.newest;
	wlandevice_t *wlandev = (wlandevice_t*)ndev->priv;
	client_t client;

	FN_ENTER;
	
	switch(rqst)
	{
		case PM_SUSPEND: /* OK, we got a suspend request */
			if (netif_running(ndev) && netif_device_present(ndev))
				netif_device_detach(ndev);
			
			/* Cancel our association */
			if (transmit_disassoc(&client, wlandev) == 1) {
				result = -EINVAL;
			}
			/* better wait for some time to make sure Tx is
			 * finished */
			current->state = TASK_UNINTERRUPTIBLE;
			schedule_timeout(HZ / 4);

			/* Then we set our flag */
			acx100_set_status(wlandev, ISTATUS_0_STARTED);

			/* Close off IRQs */
			acx100_disable_irq(wlandev);

			/* Disable the Rx/Tx queues */
			acx100_issue_cmd(wlandev, ACX100_CMD_DISABLE_TX, NULL, 0, 5000);
			acx100_issue_cmd(wlandev, ACX100_CMD_DISABLE_RX, NULL, 0, 5000);
//			acx100_issue_cmd(wlandev, ACX100_CMD_FLUSH_QUEUE, NULL, 0, 5000);

			/* disable power LED to save power */
			acxlog(L_INIT, "switching off power LED to save power. :-)\n");
			acx100_power_led(wlandev, 0);
	
			printk("Asked to suspend: %X\n", rqst);
			/* Now shut off everything else */
			if (acx100_issue_cmd(wlandev, ACX100_CMD_SLEEP, 0, 0, 5000) == 0)
			{
				result = -EBUSY;
			} else
			{
				result = 0;
				break;
			}
		case PM_RESUME:
			pm_access(wlandev->pm);
#ifndef RESUME_STANDBY_ONLY
			/* not sure whether we actually had our power
			 * removed or not, so let's do a full reset! */
			acx100_reset_dev(ndev);
#else
			acx100_issue_cmd(wlandev, ACX100_CMD_WAKE, 0, 0, 5000);

			acx100_issue_cmd(wlandev, ACX100_CMD_ENABLE_TX, NULL, 0, 5000);
			acx100_issue_cmd(wlandev, ACX100_CMD_ENABLE_RX, NULL, 0, 5000);
#endif

			acx100_enable_irq(wlandev);
//			acx100_join_bssid(wlandev);
			acx100_set_status(wlandev, ISTATUS_0_STARTED);
			printk("Asked to resume: %X\n", rqst);
			if (netif_running(ndev) && !netif_device_present(ndev))
				netif_device_attach(ndev);
			break;
		default:
			printk("Asked for PM: %X\n", rqst);
			result = -EINVAL;
			break;
	}
	FN_EXIT(1, result);
	return result;
}

/*----------------------------------------------------------------
* acx100_up
*
*
* Arguments:
*	dev: netdevice structure that contains the private wlandev
* Returns:
*	void
* Side effects:
*	- Enables on-card interrupt requests
*	- calls acx100_start
* Call context:
*	- process thread
* STATUS:
*	stable
* Comment:
*	This function is called by acx100_open (when ifconfig sets the 
*	device as up).
*----------------------------------------------------------------*/
static void acx100_up(netdevice_t * dev)
{
	wlandevice_t *wlandev = (wlandevice_t *) dev->priv;

	FN_ENTER;

	acx100_enable_irq(wlandev);
	if (wlandev->firmware_numver >= 0x0109030e) /* FIXME: first version? */
	{ /* newer firmware versions don't use a hardware timer any more */
		acxlog(L_INIT, "firmware version >= 1.9.3.e --> using software timer\n");
		init_timer(&wlandev->mgmt_timer);
		wlandev->mgmt_timer.function = acx100_timer;
		wlandev->mgmt_timer.data = (unsigned long)dev;
	}
	else
		acxlog(L_INIT, "firmware version < 1.9.3.e --> using hardware timer\n");
	acx100_start(wlandev);
	acxlog(L_XFER, "start queue on startup.\n");
	netif_start_queue(dev);

	FN_EXIT(0, 0);
}

/*----------------------------------------------------------------
* acx100_down
*
*
* Arguments:
*	dev: netdevice structure that contains the private wlandev
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
static void acx100_down(netdevice_t * dev)
{
	wlandevice_t *wlandev = (wlandevice_t *) dev->priv;

	FN_ENTER;

	acxlog(L_XFER, "stop queue during close.\n");
	netif_stop_queue(dev);

	if (wlandev->firmware_numver >= 0x0109030e) /* FIXME: first version? */
	{ /* newer firmware versions don't use a hardware timer any more */
		del_timer_sync(&wlandev->mgmt_timer);
	}
	acx100_disable_irq(wlandev);
	FN_EXIT(0, 0);
}


/*----------------------------------------------------------------
* acx100_open
*
* WLAN device open method.  Called from p80211netdev when kernel
* device open (start) method is called in response to the
* SIOCSIFFLAGS ioctl changing the flags bit IFF_UP
* from clear to set.
*
* Arguments:
*	hw		wlan device structure
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

static int acx100_open(netdevice_t * ndev)
{
	wlandevice_t *wlandev = (wlandevice_t *) ndev->priv;
	int result;
	unsigned long flags;

	FN_ENTER;

	if (acx100_lock(wlandev, &flags)) {
		acxlog(L_INIT, "card unavailable, bailing\n");
		result = -ENODEV;
		goto done;
	}
	/* request shared IRQ handler */
	if (request_irq(ndev->irq, acx100_interrupt, SA_SHIRQ, ndev->name, ndev)) {
		acxlog(L_BINSTD | L_INIT | L_IRQ, "request_irq failed\n");
		result = -EAGAIN;
		goto done;
	}
	acx100_up(ndev);

	wlandev->open = 1;

	WLAN_MOD_INC_USE_COUNT;
	result = 0;

	/* We don't currently have to do anything else.
	 * The setup of the MAC should be subsequently completed via
	 * the mlme commands.
	 * Higher layers know we're ready from dev->start==1 and
	 * dev->tbusy==0.  Our rx path knows to pass up received/
	 * frames because of dev->flags&IFF_UP is true.
	 */
done:
	acx100_unlock(wlandev, &flags);
	FN_EXIT(1, result);
	return result;
}

/*----------------------------------------------------------------
* acx100_close
*
* WLAN device close method.  Called from p80211netdev when kernel
* device close method is called in response to the
* SIOCSIIFFLAGS ioctl changing the flags bit IFF_UP
* from set to clear.
* (i.e. called for "ifconfig DEV down")
*
* Arguments:
*	hw		wlan device structure
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

static int acx100_close(netdevice_t *dev)
{
	wlandevice_t *wlandev = (wlandevice_t *) dev->priv;

	FN_ENTER;

	/* don't use acx100_lock() here: need to close card even in case
	 * of hw_unavailable set (card ejected) */
	spin_lock_irq(&wlandev->lock);

	wlandev->open = 0;

	if (netif_device_present(dev)) {
		acx100_down(dev);
	}

	free_irq(dev->irq, dev);
	/* hw->val0x240c = 0; */

	/* We currently don't have to do anything else.
	 * Higher layers know we're not ready from dev->start==0 and
	 * dev->tbusy==1.  Our rx path knows to not pass up received
	 * frames because of dev->flags&IFF_UP is false.
	 */

	WLAN_MOD_DEC_USE_COUNT;

	spin_unlock_irq(&wlandev->lock);

	FN_EXIT(0, 0);
	return 0;
}
/*----------------------------------------------------------------
* acx100_start_xmit
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

static int acx100_start_xmit(struct sk_buff *skb, netdevice_t * dev)
{
	int txresult = 0;
	unsigned long flags;
	wlandevice_t *hw = (wlandevice_t *) dev->priv;
//	wlan_pb_t *pb;
//	wlan_pb_t pb1;
	struct txdescriptor *tx_desc;
	int templen;

//	pb = &pb1;

	FN_ENTER;

	if (!skb) {
		return 0;
	}
	if (!hw) {
		return 1;
	}
	if (!(hw->open)) {
		return 1;
	}

	if (acx100_lock(hw, &flags))
		return 1;

	if (netif_queue_stopped(dev)) {
		acxlog(L_BINSTD, "%s: called when queue stopped\n", __func__);
		txresult = 1;
		goto end;
	}

	if (hw->iStatus != ISTATUS_4_ASSOCIATED) {
		acxlog(L_XFER, "Trying to xmit, but not associated yet: aborting...\n");
		/* silently drop the packet, since we're not connected yet */
		dev_kfree_skb(skb);
		hw->stats.tx_errors++;
		txresult = 0;
		goto end;
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
	acxlog(L_XFER, "stop queue during Tx.\n");
	netif_stop_queue(dev);
#endif
//	if (acx100_lock(hw,&flags)) ...

//	memset(pb, 0, sizeof(wlan_pb_t) /*0x14*4 */ );

/*	pb->ethhostbuf = skb;
	pb->ethbuf = skb->data;
*/
	templen = skb->len;
/*	if (templen > ETH_FRAME_LEN) {
		templen = ETH_FRAME_LEN;
	}
	pb->ethbuflen = templen;
	pb->ethfrmlen = templen;

	pb->eth_hdr = (wlan_ethhdr_t *) pb->ethbuf;
 */
	if ((tx_desc = acx100_get_tx_desc(hw)) == NULL){
		acxlog(L_BINSTD,"BUG: txdesc ring full\n");
		txresult = 1;
		goto end;
	}

	acx100_ether_to_txdesc(hw,tx_desc,skb);
	dev_kfree_skb(skb);

	acx100_dma_tx_data(hw, tx_desc);
	dev->trans_start = jiffies;

/*	if (txresult == 1){
		return 1;
	} */

	/* tx_desc = &hw->dc.pTxDescQPool[hw->dc.pool_idx]; */
#if 0
	/* if((tx_desc->Ctl & 0x80) != 0){ */
		acxlog(L_XFER, "wake queue after Tx start.\n");
		netif_wake_queue(dev);
	/* } */
#endif
	hw->stats.tx_packets++;
	hw->stats.tx_bytes += templen;

end:
	acx100_unlock(hw, &flags);

	FN_EXIT(1, txresult);
	return txresult;
}
/*----------------------------------------------------------------
* acx100_tx_timeout
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

/* acx100_tx_timeout()
 * STATUS: ok.
 */
static void acx100_tx_timeout(netdevice_t * dev)
{
//	char tmp[4];
/*	dev->trans_start = jiffies;
	netif_wake_queue(dev);
*/

//	ctlCmdFlushTxQueue((wlandevice_t*)dev->priv,(memmap_t*)&tmp);
	FN_ENTER;
	acxlog(L_STD, "Tx timeout!\n");
	((wlandevice_t *)dev->priv)->stats.tx_errors++;
	FN_EXIT(0, 0);
}

/*----------------------------------------------------------------
* acx100_get_stats
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

/* acx100_get_stats()
 * STATUS: should be ok..
 */
static struct net_device_stats *acx100_get_stats(netdevice_t * dev)
{
	FN_ENTER;
	return &((wlandevice_t *) dev->priv)->stats;
}
/*----------------------------------------------------------------
* acx100_get_wireless_stats
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

static struct iw_statistics *acx100_get_wireless_stats(netdevice_t *dev)
{
	wlandevice_t *wlandev = (wlandevice_t *)dev->priv;

	FN_ENTER;
	return &wlandev->wstats;
}

/*----------------------------------------------------------------
* acx100_set_rx_mode
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

static void acx100_set_rx_mode(netdevice_t * netdev)
{
	FN_ENTER;
}

/*----------------------------------------------------------------
* acx100_interrupt
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

irqreturn_t acx100_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	wlandevice_t *wlandev = (wlandevice_t *) (((netdevice_t *) dev_id)->priv);
	UINT16 irqtype;
	int irqcount = MAX_IRQLOOPS_PER_JIFFY;
	static unsigned long entry_count = 0;
	static int loops_this_jiffy = 0;
	static unsigned long last_irq_jiffies = 0;
	
	FN_ENTER;

	irqtype = acx100_read_reg16(wlandev, ACX100_IRQ_STATUS) & ~(wlandev->irq_mask);
	pm_access(wlandev->pm);
	/* immediately return if we don't get signalled that an interrupt
	 * has occurred that we are interested in (interrupt sharing
	 * with other cards!) */
	if (!irqtype) {
		acxlog(L_IRQ, "IRQTYPE: %X, irq_mask: %X\n", irqtype, wlandev->irq_mask);
		return IRQ_NONE;
	}
	else
	{
		entry_count++;
		acxlog(L_IRQ, "IRQTYPE: %X, irq_mask: %X, entry count: %ld\n", irqtype, wlandev->irq_mask, entry_count);
	}
//	if (acx100_lock(wlandev,&flags)) ...
#define IRQ_ITERATE 1
#if IRQ_ITERATE
  if (jiffies != last_irq_jiffies)
      loops_this_jiffy = 0;
  last_irq_jiffies = jiffies;

  while (irqtype && (irqcount--)) {
    if (++loops_this_jiffy > MAX_IRQLOOPS_PER_JIFFY) {
      acxlog(-1, "HARD ERROR: Too many interrupts this jiffy\n");
      wlandev->irq_mask = 0;
        break;
        }
#endif

	if (irqtype & 0x8) {
		acx100_process_rx_desc(wlandev);
		acx100_write_reg16(wlandev, ACX100_IRQ_ACK, 0x8);
	}
	if (irqtype & 0x2) {
		acx100_clean_tx_desc(wlandev);
		acx100_write_reg16(wlandev, ACX100_IRQ_ACK, 0x2);
#if 0
/* this shouldn't happen here generally, since we'd also enable user packet
 * xmit for management packets, which we really DON'T want */
/* BS: disabling this caused my card to stop working after a few 
 * seconds when floodpinging. This should be reinvestigated ! */
		  if (netif_queue_stopped(dev_id)) {
			  acxlog(L_XFER, "wake queue after Tx complete.\n");
			  netif_wake_queue(dev_id);
		  }
#endif
	}
	if (irqtype & HOST_INT_SCAN_COMPLETE /* 0x2000 */ ) {
		/* V1_3CHANGE: dbg msg only in V1 */
		acxlog(L_IRQ,
		       "<%s> HOST_INT_SCAN_COMPLETE\n", __func__);

		if (wlandev->iStatus == ISTATUS_5_UNKNOWN) {
			acx100_set_status(wlandev, wlandev->unknown0x2350);
			wlandev->unknown0x2350 = 0;
		} else if (wlandev->iStatus == ISTATUS_1_SCANNING) {

			d11CompleteScan(wlandev);
		}
		acx100_write_reg16(wlandev, ACX100_IRQ_ACK, HOST_INT_SCAN_COMPLETE);
	}
	if (irqtype & HOST_INT_TIMER /* 0x40 */ ) {
		acx100_timer((u_long)wlandev->netdev);
		acx100_write_reg16(wlandev, ACX100_IRQ_ACK, HOST_INT_TIMER);
	}
	if (irqtype & 0x10) {
		printk("Got DTIM IRQ\n");
		acx100_write_reg16(wlandev, ACX100_IRQ_ACK, 0x10);
	}
	if (irqtype & 0x20) {
		printk("Got Beacon IRQ\n");
		acx100_write_reg16(wlandev, ACX100_IRQ_ACK, 0x20);
	}
	if (irqtype & 0x80) {
		printk("Got Key Not Found IRQ\n");
		acx100_write_reg16(wlandev, ACX100_IRQ_ACK, 0x80);
	}
	if (irqtype & 0x100) {
		printk("Got IV ICV Failure IRQ\n");
		acx100_write_reg16(wlandev, ACX100_IRQ_ACK, 0x100);
	}
	if (irqtype & 0x200) {
		printk("Got Command Complete IRQ\n");
		acx100_write_reg16(wlandev, ACX100_IRQ_ACK, 0x200);
	}
	if (irqtype & 0x400) {
		printk("Got Info IRQ\n");
		acx100_write_reg16(wlandev, ACX100_IRQ_ACK, 0x400);
	}
	if (irqtype & 0x800) {
		printk("Got Overflow IRQ\n");
		acx100_write_reg16(wlandev, ACX100_IRQ_ACK, 0x800);
	}
	if (irqtype & 0x1000) {
		printk("Got Process Error IRQ\n");
		acx100_write_reg16(wlandev, ACX100_IRQ_ACK, 0x1000);
	}
	if (irqtype & 0x4000) {
		printk("Got FCS Threshold IRQ\n");
		acx100_write_reg16(wlandev, ACX100_IRQ_ACK, 0x4000);
	}
	if (irqtype & 0x8000) {
		printk("Got Unknown IRQ\n");
		acx100_write_reg16(wlandev, ACX100_IRQ_ACK, 0x8000);
	}
//	acx100_write_reg16(wlandev, ACX100_IRQ_ACK, irqtype);
#if IRQ_ITERATE
	irqtype = acx100_read_reg16(wlandev, ACX100_IRQ_STATUS) & ~(wlandev->irq_mask);
	if (irqtype)
		acxlog(L_IRQ, "IRQTYPE: %X\n", irqtype);
   }
#endif

//	acx100_unlock(hw,&flags);
	entry_count--;
	FN_EXIT(0, 0);
	return IRQ_HANDLED;
}

/*----------------------------------------------------------------
* acx100_rx
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
/* acx100_rx()
 * STATUS: *much* better now, maybe finally bug-free, VERIFIED.
 */
void acx100_rx(struct rxhostdescriptor *rxdesc/* wlan_pb_t * pb */, 
		wlandevice_t * hw)
{
	netdevice_t *ndev;
	struct sk_buff *skb;

	FN_ENTER;
	if (hw->open) {
		if ((skb = acx100_rxdesc_to_ether(hw, rxdesc))) {
			ndev = root_acx100_dev.newest;
			skb->dev = ndev;
			ndev->last_rx = jiffies;

			skb->protocol = eth_type_trans(skb, ndev);
			netif_rx(skb);

			hw->stats.rx_packets++;
			hw->stats.rx_bytes += skb->len;
			/* V1_3CHANGE */
		}
	}
	FN_EXIT(0, 0);
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

#ifdef DEBUG
MODULE_PARM(debug, "i");
MODULE_PARM_DESC(debug, "Debug level mask: 0x0000 - 0x1fff");
#endif

MODULE_PARM(use_eth_name, "i");
MODULE_PARM_DESC(use_eth_name, "Allocate device ethX instead of wlanX");
MODULE_PARM(firmware_dir, "s");
MODULE_PARM_DESC(firmware_dir, "Directory where to load acx100 firmware files from");

static int __init acx100_init_module(void)
{
	int res;

	FN_ENTER;		//.data(0x4)=version

//#ifdef MODULE
	acxlog(L_STD,
	       "acx100: It looks like you were coaxed into buying a wireless network card\n");
	acxlog(L_STD,
	       "acx100: that uses the mysterious ACX100 chip from Texas Instruments.\n");
	acxlog(L_STD,
	       "acx100: You should better have bought e.g. a PRISM(R) chipset based card,\n");
	acxlog(L_STD,
	       "acx100: since that would mean REAL vendor Linux support.\n");
	acxlog(L_STD,
	       "acx100: Given this info, it's plain evident that this driver is EXPERIMENTAL,\n");
	acxlog(L_STD,
	       "acx100: thus your mileage may vary. Visit http://acx100.sf.net for support.\n");

	acxlog(L_BINSTD, "%s: %s Loaded\n", __func__, version);
	acxlog(L_BINDEBUG, "%s: dev_info is: %s\n", __func__, dev_info);
	//.data(0xc)=dev_info

	res = pci_module_init(&acx100_pci_drv_id);
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

static void __exit acx100_cleanup_module(void)
{
	FN_ENTER;
	pci_unregister_driver(&acx100_pci_drv_id);
	FN_EXIT(0, 0);
}

/* For kernels 2.5.* where modutils>=4.2.22, we must have a module_init and module_exit like so: */
module_init(acx100_init_module);
module_exit(acx100_cleanup_module);

#endif /* MODULE */
