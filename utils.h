#ifndef _ACX_UTILS_H_
#define _ACX_UTILS_H_

char *acx_print_mac(char *buf, const u8 *mac);
void acx_print_mac2(const char *head, const u8 *mac, const char *tail);
void acxlog_mac(int level, const char *head, const u8 *mac, const char *tail);
void acx_dump_bytes(const void *data, int num);
void hexdump(char *note, unsigned char *buf, unsigned int len);

#endif
