
#define pr_fmt(fmt) "acx.%s: " fmt, __func__
#include "acx_debug.h"

#include <linux/version.h>

#include <linux/compiler.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/skbuff.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/wireless.h>
#include <linux/netdevice.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/vmalloc.h>
#include <linux/workqueue.h>
#include <linux/nl80211.h>

#include <net/iw_handler.h>
#include <net/mac80211.h>
#include <asm/io.h>

#define pr_acx	pr_info

/* this will be problematic when combined with the *_PCI macro.
   acx_struct_dev.h defines iobase field 2x, with different types, for
   MEM and PCI includes.  Punt for now..
*/
#define ACX_MAC80211_MEM
// #define ACX_MAC80211_PCI

#include "acx.h"
#include "mem-inlines.h"

// merge adaptation help
#include "pci.h"
#include "mem.h"

// from mem.c:98
#define FW_NO_AUTO_INCREMENT 1

// identical from pci.c, mem.c
irqreturn_t acx_interrupt(int irq, void *dev_id)
{
	acx_device_t *adev = dev_id;
	unsigned long flags;
	u16 irqreason;
	u16 irqmasked;

	if (!adev)
		return IRQ_NONE;

	// On a shared irq line, irqs should only be for us, when enabled them
	if (!adev->irqs_active)
		return IRQ_NONE;

	FN_ENTER;

	spin_lock_irqsave(&adev->spinlock, flags);

	/* We only get an irq-signal for IO_ACX_IRQ_MASK unmasked irq reasons.
	 * However masked irq reasons we still read with IO_ACX_IRQ_REASON or
	 * IO_ACX_IRQ_STATUS_NON_DES
	 */
	irqreason = read_reg16(adev, IO_ACX_IRQ_STATUS_NON_DES);
	irqmasked = irqreason & ~adev->irq_mask;
	log(L_IRQ, "irqstatus=%04X, irqmasked=%04X,\n", irqreason, irqmasked);

	if (unlikely(irqreason == 0xffff)) {
		/* 0xffff value hints at missing hardware,
		 * so don't do anything.
		 * Not very clean, but other drivers do the same... */
		log(L_IRQ, "irqstatus=FFFF: Device removed?: IRQ_NONE\n");
		goto none;
	}

	/* Our irq-signals are the ones, that were triggered by the IO_ACX_IRQ_MASK
	 * unmasked irqs.
	 */
	if (!irqmasked) {
		/* We are on a shared IRQ line and it wasn't our IRQ */
		log(L_IRQ, "irqmasked zero: IRQ_NONE\n");
		goto none;
	}

	// Mask all irqs, until we handle them. We will unmask them later in the tasklet.
	write_reg16(adev, IO_ACX_IRQ_MASK, HOST_INT_MASK_ALL);
	write_flush(adev);
	acx_schedule_task(adev, 0);

	spin_unlock_irqrestore(&adev->spinlock, flags);
	FN_EXIT0;
	return IRQ_HANDLED;
none:
	spin_unlock_irqrestore(&adev->spinlock, flags);
	FN_EXIT0;
	return IRQ_NONE;
}

/*
 * modified from acxmem_s_upload_radio, and wrapped below
 */
static int acx_upload_radio(acx_device_t *adev, char *filename)
{
	acx_ie_memmap_t mm;
	firmware_image_t *radio_image;
	acx_cmd_radioinit_t radioinit;
	int res = NOT_OK;
	int try;
	u32 offset;
	u32 size;

	acxmem_lock_flags;

	if (!adev->need_radio_fw)
		return OK;

	FN_ENTER;

	pr_notice("firmware: %s\n", filename);

	acx_interrogate(adev, &mm, ACX1xx_IE_MEMORY_MAP);
	offset = le32_to_cpu(mm.CodeEnd);

	radio_image = acx_read_fw(adev->bus_dev, filename, &size);
	if (!radio_image) {
		pr_acx("can't load radio module '%s'\n", filename);
		goto fail;
	}

	acx_issue_cmd(adev, ACX1xx_CMD_SLEEP, NULL, 0);

	for (try = 1; try <= 5; try++) {
		// JC: merge mem vs pci here.
		acxmem_lock();
		if (IS_MEM(adev))
			res = acxmem_write_fw(adev, radio_image, offset);
		else if (IS_PCI(adev))
			res = acxpci_write_fw(adev, radio_image, offset);
		else BUG();

		log(L_DEBUG|L_INIT, "acx_write_fw (radio): %d\n", res);
		if (OK == res) {
			// JC: merge mem vs pci here.
			if (IS_MEM(adev))
				res = acxmem_validate_fw(adev, radio_image, offset);
			else if (IS_PCI(adev))
				res = acxpci_validate_fw(adev, radio_image, offset);
			else BUG();
			log(L_DEBUG|L_INIT, "acx_validate_fw (radio): %d\n", res);
		}
		acxmem_unlock();

		if (OK == res)
			break;
		pr_acx("radio firmware upload attempt #%d FAILED, "
			"retrying...\n", try);
		acx_mwait(1000); /* better wait for a while... */
	}

	acx_issue_cmd(adev, ACX1xx_CMD_WAKE, NULL, 0);
	radioinit.offset = cpu_to_le32(offset);

	/* no endian conversion needed, remains in card CPU area: */
	radioinit.len = radio_image->size;

	vfree(radio_image);

	if (OK != res)
		goto fail;

	/* will take a moment so let's have a big timeout */
	acx_issue_cmd_timeo(adev, ACX1xx_CMD_RADIOINIT, &radioinit,
			sizeof(radioinit), CMD_TIMEOUT_MS(1000));

	res = acx_interrogate(adev, &mm, ACX1xx_IE_MEMORY_MAP);

fail:
	FN_EXIT1(res);
	return res;
}

int acxmem_upload_radio(acx_device_t *adev)
{
	char filename[sizeof("RADIONN.BIN")];

	snprintf(filename, sizeof(filename), "RADIO%02x.BIN", adev->radio_type);
	return acx_upload_radio(adev, filename);
}

int acxpci_upload_radio(acx_device_t *adev)
{
        char filename[sizeof("tiacx1NNrNN")];

        snprintf(filename, sizeof(filename), "tiacx1%02dr%02X",
		IS_ACX111(adev) * 11, adev->radio_type);
	return acx_upload_radio(adev, filename);
}

int acx_create_hostdesc_queues(acx_device_t *adev)
{
        int result;

	pr_notice("notice IS_PCI(%p): %d\n", adev, IS_PCI(adev));

	result = (IS_MEM(adev))
		? acxmem_create_tx_host_desc_queue(adev)
		: acxpci_create_tx_host_desc_queue(adev);
        if (OK != result)
                return result;
        result = (IS_MEM(adev))
		? acxmem_create_rx_host_desc_queue(adev)
		: acxpci_create_rx_host_desc_queue(adev);
        return result;
}

