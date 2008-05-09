/*
 * IRQ handling: top half, bottom half (halves?) and utilities.
 *
 * This file is mainly modeled on the b43 way of doing things.
 *
 * Copyright (c) 2008, Francis Galiegue <fgaliegue@gmail.com> for the ACX100
 * driver project.
 *
 * This file is licensed under GPLv2.
 */
#include <linux/irq.h>
#include <linux/spinlock.h>

#include "acx_config.h"
#include "acx_log.h"
#include "acx_struct.h"
#include "acx_irq.h"

/*
 * acx_enable_irqs() and acx_disable_irqs():
 */

/**
 * acx_enable_irqs(): set the interrupt mask on the card. Should be called with
 * a mask saved by the acx_disable_irqs() function.
 * @adev: the device on which to enable the IRQs.
 * @mask: the mask to apply to this device.
 */
//void acx_enable_irqs(acx_device_t *adev, u16 mask)
//{
//	write_reg16(adev, ACX_IO_IRQ_MASK, mask);
//	write_reg16(adev, ACX_IO_FEMR, 0x0);
//	smp_wmb();
//}

/**
 * acx_disable_irqs(): set an interrupt mask on the card, and return the mask
 * that was present on the card.
 * @adev: the device on which to disable the IRQs.
 * @mask: the mask to set.
 *
 * FIXME: I couldn't actually find an example of reading the current interrupt
 * mask, but it would make sense that you read it from the same location that
 * you write to...
 */
//u16 acx_disable_irqs(acx_device_t *adev, u16 mask)
//{
//	u16 saved_mask = read_reg16(adev, ACX_IO_IRQ_MASK);
//	write_reg16(adev, ACX_IO_IRQ_MASK, mask);
//	write_reg16(adev, ACX_IO_FEMR, 0x0);
//	smp_wmb();
//	return saved_mask;
//}


/**
 * acx_interrupt_tophalf(): the IRQ top half routine, called when an interrupt
 * is raised.
 * @irq: the IRQ line number.
 * @dev_id: the current device addressed by this interrupt (or not, since our
 * IRQ line can be shared).
 *
 * Theory of operation:
 * 	- spin_lock the device's irqlock;
 * 	- acknowledge the IRQ on the card;
 * 	- schedule the bottom half;
 * 	- disable ALL interrupts on the card they will be re-enabled by the
 * 	  bottom half;
 * 	- spin_unlock the device's irqlock;
 * 	- tell the kernel that we are done.
 */

//irqreturn_t acx_interrupt_tophalf(int irq, void *dev_id)
//{
//	irqreturn_t ret = IRQ_NONE;
//	acx_device_t *adev = dev_id;
//	u16 saved_mask, reason;
//	
//	if (!adev)
//		return IRQ_NONE;
//
//	spin_lock(&adev->irqlock);
//	
//	/* Check whether the device is started. If not, out! */
//	if (!adev->initialized)
//		goto out;
//
//	reason = read_reg16(adev, ACX_IO_IRQ_REASON);
//	if (reason == ACX_IRQ_ALL) {
//		/*
//		 * Original code says this is because of missing hardware?
//		 *
//		 * Anyway, other drivers hint at this meaning: shared interrupt
//		 * and it's not ours. I like that description better.
//		 */
//		goto out;
//	}
//
//	ret = IRQ_HANDLED;
//	/*
//	 * If it's not an interrupt the card is currently handling, out
//	 */
//	reason &= read_reg16(adev, ACX_IO_IRQ_MASK);
//	if (!reason)
//		goto out;
//	
//	/*
//	 * Tell the card that we got it.
//	 *
//	 * FIXME: the original code writes ACX_IRQ_ALL?? Other drivers write
//	 * only the real reason, I choose to do it that way.
//	 */
//	write_reg16(adev, ACX_IO_IRQ_ACK, reason);
//	/*
//	 * FIXME: the current code leaves some interrupts in the clear, let's
//	 * try not to here
//	 */
//	adev->irq_saved_mask = acx_disable_irqs(adev, ACX_IRQ_ALL);
//	/*
//	 * Save the IRQ reason for our bottom half to read
//	 */
//	adev->irq_reason = reason;
//
//	/*
//	 * TODO!!
//	 *
//	 * tasklet_schedule(&adev->some_field);
//	 */
//
//
//out:
//	//mmiowb(); Necessary?
//	spin_unlock(&adev->irqlock);
//	return ret;
//}

/**
 * acx_interrupt_bottomhalf(): the interrupt bottom half, as its name says.
 * @adev: the device on behalf of which this bottom half is called.
 *
 * Theory of operation:
 * 	- enter with all card interrupts DISABLED!
 * 	- spin_lock_irqsave() the device's irq_lock;
 * 	- handle the interrupt (reason is stored in ->irq_reason);
 * 	- re-enable on-card interrupts (saved in ->irq_saved_mask);
 * 	- spin_unlock_irqrestore() the device's irq_lock.
 */

//void acx_interrupt_bottomhalf(acx_device_t *adev)
//{
//	u16 reason;
//	unsigned long flags;
//
//	spin_lock_irqsave(&adev->irqlock, flags);
//
//	reason = adev->irq_reason;
//	//TODO: fill these!
//	
//
//
//	acx_enable_irqs(adev, adev->irq_saved_mask);
//	spin_unlock_irqrestore(&adev->irqlock, flags);
//}

