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
#define ACX_IRQ_RX_DATA	0x0001
#define ACX_IRQ_TX_COMPLETE	0x0002
#define ACX_IRQ_TX_XFER	0x0004
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


#endif /* _ACX_IRQ_H_ */
