/*
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008
 * The ACX100 Open Source Project <acx100-devel@lists.sourceforge.net>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */
#ifndef _ACX_COMMANDS_H_
#define _ACX_COMMANDS_H_

/* 
 * ----------------------------------------------------------------------------
 *  Can be found in the cmdTable table on the "Rev. 1.5.0" (FW150) firmware
 * ----------------------------------------------------------------------------
 */
#define ACX1xx_CMD_RESET		    0x00
#define ACX1xx_CMD_INTERROGATE		0x01
#define ACX1xx_CMD_CONFIGURE		0x02
#define ACX1xx_CMD_ENABLE_RX		0x03
#define ACX1xx_CMD_ENABLE_TX		0x04
#define ACX1xx_CMD_DISABLE_RX		0x05
#define ACX1xx_CMD_DISABLE_TX		0x06
#define ACX1xx_CMD_FLUSH_QUEUE		0x07
#define ACX1xx_CMD_SCAN			    0x08
#define ACX1xx_CMD_STOP_SCAN		0x09
#define ACX1xx_CMD_CONFIG_TIM		0x0A
#define ACX1xx_CMD_JOIN			    0x0B
#define ACX1xx_CMD_WEP_MGMT		    0x0C
#ifdef OLD_FIRMWARE_VERSIONS
#define ACX100_CMD_HALT             0x0E    /* mapped to unknownCMD in FW150 */
#else
#define ACX1xx_CMD_MEM_READ         0x0D
#define ACX1xx_CMD_MEM_WRITE        0x0E
#endif /* OLD_FIRMWARE_VERSIONS */
#define ACX1xx_CMD_SLEEP		    0x0F
#define ACX1xx_CMD_WAKE			    0x10
#define ACX1xx_CMD_UNKNOWN_11		0x11	/* mapped to unknownCMD in FW150 */
#define ACX100_CMD_INIT_MEMORY		0x12
#define ACX1FF_CMD_DISABLE_RADIO	0x12	/* new firmware? TNETW1450? + NOT in BSD driver */
#define ACX1xx_CMD_CONFIG_BEACON	0x13
#define ACX1xx_CMD_CONFIG_PROBE_RESPONSE 0x14
#define ACX1xx_CMD_CONFIG_NULL_DATA	0x15
#define ACX1xx_CMD_CONFIG_PROBE_REQUEST 0x16
#define ACX1xx_CMD_FCC_TEST		    0x17
#define ACX1xx_CMD_RADIOINIT		0x18
#define ACX111_CMD_RADIOCALIB		0x19
#define ACX1FF_CMD_NOISE_HISTOGRAM	0x1c	/* new firmware? TNETW1450? */
#define ACX1FF_CMD_RX_RESET		    0x1d	/* new firmware? TNETW1450? */
#define ACX1FF_CMD_LNA_CONTROL		0x20	/* new firmware? TNETW1450? */
#define ACX1FF_CMD_CONTROL_DBG_TRACE	0x21	/* new firmware? TNETW1450? */

/* 'After Interrupt' Commands */
#define ACX_AFTER_IRQ_CMD_STOP_SCAN	0x01
#define ACX_AFTER_IRQ_CMD_ASSOCIATE	0x02
#define ACX_AFTER_IRQ_CMD_RADIO_RECALIB	0x04
#define ACX_AFTER_IRQ_UPDATE_CARD_CFG	0x08
#define ACX_AFTER_IRQ_TX_CLEANUP	0x10
#define ACX_AFTER_IRQ_COMPLETE_SCAN	0x20
#define ACX_AFTER_IRQ_RESTART_SCAN	0x40

#endif /* _ACX_COMMANDS_H_ */
