/*

 */

#include "acx_struct_dev.h"

u16 interrupt_masks[3][3] = {
	[ DEVTYPE_MEM ] = {
		[ CHIPTYPE_ACX100 ]
		= (u16) ~ (0
			| HOST_INT_RX_DATA
			| HOST_INT_TX_COMPLETE
			/* | HOST_INT_TX_XFER        */
			/* | HOST_INT_RX_COMPLETE    */
			/* | HOST_INT_DTIM           */
			/* | HOST_INT_BEACON         */
			/* | HOST_INT_TIMER          */
			/* | HOST_INT_KEY_NOT_FOUND  */
			/* | HOST_INT_IV_ICV_FAILURE */
			| HOST_INT_CMD_COMPLETE
			| HOST_INT_INFO
			| HOST_INT_OVERFLOW
			| HOST_INT_PROCESS_ERROR
			| HOST_INT_SCAN_COMPLETE
			/* | HOST_INT_FCS_THRESHOLD  */
			/* | HOST_INT_BEACON_MISSED  */
			),
		
		[ CHIPTYPE_ACX111 ]
		= (u16) ~ (0
			| HOST_INT_RX_DATA
			| HOST_INT_TX_COMPLETE
			/* | HOST_INT_TX_XFER        */
			/* | HOST_INT_RX_COMPLETE    */
			/* | HOST_INT_DTIM           */
			/* | HOST_INT_BEACON         */
			/* | HOST_INT_TIMER          */
			/* | HOST_INT_KEY_NOT_FOUND  */
			| HOST_INT_IV_ICV_FAILURE
			| HOST_INT_CMD_COMPLETE
			| HOST_INT_INFO
			| HOST_INT_OVERFLOW
			/* | HOST_INT_PROCESS_ERROR  */
			| HOST_INT_SCAN_COMPLETE
			| HOST_INT_FCS_THRESHOLD
			| HOST_INT_UNKNOWN),
	},

	[ DEVTYPE_PCI ] = {
		[ CHIPTYPE_ACX100 ]
		= (u16) ~ (0
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
			),
		
		[ CHIPTYPE_ACX111 ]
		= (u16) ~ (0
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
			),
	}
};
