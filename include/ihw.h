/* include/ihw.h
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
 */

typedef struct cmd {
	UINT16 vala[0xc];
	UINT valb;
	UINT16 nul;
	UINT16 valc;
} cmd_t;
typedef struct mac {
	UINT vala;
	UINT16 valb;
} mac_t;

UINT32 hwReadRegister32(wlandevice_t * act, UINT valb);
void hwWriteRegister32(wlandevice_t * act, UINT vala, UINT valb);
UINT16 hwReadRegister16(wlandevice_t * act, UINT valb);
void hwWriteRegister16(wlandevice_t * act, UINT vala, UINT16 valb);
UINT8 hwReadRegister8(wlandevice_t * act, UINT valb);
void hwWriteRegister8(wlandevice_t * act, UINT vala, UINT valb);
void get_cmd_state(wlandevice_t * act);
void write_cmd_type(wlandevice_t * act, UINT16 vala);
void write_cmd_status(wlandevice_t * act, UINT vala);
int write_cmd_Parameters(wlandevice_t * hw, memmap_t * cmd, int valb);
int read_cmd_Parameters(wlandevice_t * hw, memmap_t * pdr, int valb);
int ctlIssueCommand(wlandevice_t * wlandev, UINT command, void *hw,
		    int valb, UINT32 valc);

int ctlConfigure(wlandevice_t * act, void * pdr, short type);
int ctlConfigureLength(wlandevice_t * act, void * pdr, short type, short length);
int ctlInterrogate(wlandevice_t * hw, void * pdr, short type);

int read_mem(wlandevice_t * wlandev, int vald);
int IsMacAddressZero(mac_t * mac);
void ClearMacAddress(mac_t * mac);
int IsMacAddressEqual(UINT8 * one, UINT8 * two);
void CopyMacAddress(UINT8 * from, UINT8 * to);
UINT8 IsMacAddressGroup(mac_t * mac);
UINT8 IsMacAddressDirected(mac_t * mac);
void SetMacAddressBroadcast(char *addr);
int IsMacAddressBroadcast(mac_t * mac);
int IsMacAddressMulticast(mac_t * mac);
void IsOurMulticastAddress(wlandevice_t * wlandev);

void LogMacAddress(int level, UINT8 * mac);
void acx100_power_led(wlandevice_t *wlandev, int enable);
