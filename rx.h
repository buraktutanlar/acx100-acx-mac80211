#ifndef _ACX_RX_H_
#define _ACX_RX_H_

void acx_process_rxbuf(acx_device_t *adev, rxbuffer_t *rxbuf);
u8 acx_signal_determine_quality(u8 signal, u8 noise);

#if !ACX_DEBUG
static inline const char *acx_get_packet_type_string(u16 fc) { return ""; }
#else
const char *acx_get_packet_type_string(u16 fc);
#endif

#endif
