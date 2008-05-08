#ifndef _ACX_IRQ_H_
#define _ACX_IRQ_H_

/*
 * acx_irq.h: IRQ constants, top half and bottom half declarations.
 *
 * Copyright (c) 2008, the ACX100 project.
 *
 * This file is licensed under GPLv2.
 */

/*
 * List of jobs that have to be done post interrupt.
 *
 * FIXME: the b43 driver uses two queues: a work_queue and a delayed_work. Maybe
 * some of these commands may be delayed? If so, rename TASKLET to WORK/DELAYED.
 */
#define ACX_TASKLET_CMD_STOP_SCAN	0x01
#define ACX_TASKLET_CMD_ASSOCIATE	0x02
#define ACX_TASKLET_CMD_RADIO_RECALIB	0x04
#define ACX_TASKLET_UPDATE_CARD_CFG	0x08
#define ACX_TASKLET_TX_CLEANUP		0x10
#define ACX_TASKLET_COMPLETE_SCAN	0x20
#define ACX_TASKLET_RESTART_SCAN	0x40

/* IRQ Constants
** (outside of "#ifdef PCI" because USB (mis)uses HOST_INT_SCAN_COMPLETE) */
#define ACX_IRQ_RX_DATA		0x0001
#define ACX_IRQ_TX_COMPLETE	0x0002
#define ACX_IRQ_TX_XFER		0x0004
#define ACX_IRQ_RX_COMPLETE	0x0008
#define ACX_IRQ_DTIM		0x0010
#define ACX_IRQ_BEACON		0x0020
#define ACX_IRQ_TIMER		0x0040
#define ACX_IRQ_KEY_NOT_FOUND	0x0080
#define ACX_IRQ_IV_ICV_FAILURE	0x0100
#define ACX_IRQ_CMD_COMPLETE	0x0200
#define ACX_IRQ_INFO		0x0400
#define ACX_IRQ_OVERFLOW	0x0800
#define ACX_IRQ_PROCESS_ERROR	0x1000
#define ACX_IRQ_SCAN_COMPLETE	0x2000
#define ACX_IRQ_FCS_THRESHOLD	0x4000
#define ACX_IRQ_UNKNOWN		0x8000

#define ACX_IRQ_ALL		0xffff

/*
 * The default masks for ACX100 and ACX111 differ!
 *
 * The ACX*_DEFAULT_IRQ_MASK values define what the masks of the card should
 * initially be set to. The ACX*_DISABLE_ALL_IRQS define what mask should be set
 * in order to not receive anymore interrupts.
 *
 * FIXME: the values below are taken from the existing code, they may not be
 * accurate. I don't have any RE skill, so I cannot really tell :/
 */

/*
 * ACX111
 */
#define ACX111_DEFAULT_NEGATED_IRQ_MASK ( \
	ACX_IRQ_TX_COMPLETE | \
	ACX_IRQ_RX_COMPLETE | \
	ACX_IRQ_IV_ICV_FAILURE | \
	ACX_IRQ_CMD_COMPLETE | \
	ACX_IRQ_INFO | \
	ACX_IRQ_SCAN_COMPLETE | \
	ACX_IRQ_FCS_THRESHOLD \
)

#define ACX111_DEFAULT_IRQ_MASK \
	((u16) ~ ACX111_DEFAULT_NEGATED_IRQ_MASK )

#define ACX111_DISABLE_ALL_IRQS \
	((u16) ~ (ACX_IRQ_UNKNOWN))

/*
 * ACX100
 */
#define ACX100_DEFAULT_NEGATED_IRQ_MASK ( \
	ACX_IRQ_TX_COMPLETE | \
	ACX_IRQ_RX_COMPLETE | \
	ACX_IRQ_CMD_COMPLETE | \
	ACX_IRQ_INFO | \
	ACX_IRQ_SCAN_COMPLETE \
)

#define ACX100_DEFAULT_IRQ_MASK \
	((u16) ~ ACX100_DEFAULT_NEGATED_IRQ_MASK )

/*
 * About the value below: the original code said:
 *
 * "Or else acx100 won't signal cmd completion, right?"
 *
 * Also, why isn't ACX_IRQ_UNKNOWN part of that mask? Good question.
 */
#define ACX100_DISABLE_ALL_IRQS \
	((u16) ~ (ACX_IRQ_CMD_COMPLETE))
#endif /* _ACX_IRQ_H_ */
