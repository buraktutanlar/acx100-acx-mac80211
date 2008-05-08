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

#include "acx_config.h"
#include "acx_log.h"
#include "acx_struct.h"
#include "acx_irq.h"

/*
 * TODO: implement!
 */

//static irqreturn_t acx_interrupt_handler(int irq, void *dev_id);

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
//}

/**
 * acx_disable_irqs(): set an interrupt mask on the card, and return the mask
 * that was present on the card.
 * @adev: the device on which to disable the IRQs.
 * @mask: the mask to set.
 */
//u16 acx_disable_irqs(acx_device_t *adev, u16 mask)
//{
//}

