#include "acx_debug.h"

#include "acx.h"

char* acx_print_mac(char *buf, const u8 *mac)
{
	sprintf(buf, MACSTR, MAC(mac));
	return(buf);
}

void acx_print_mac2(const char *head, const u8 *mac, const char *tail)
{
	pr_info("%s" MACSTR "%s", head, MAC(mac), tail);
}

void acxlog_mac(int level, const char *head, const u8 *mac, const char *tail)
{
	if (acx_debug & level)
		acx_print_mac2(head, mac, tail);
}

void acx_dump_bytes(const void *data, int num)
{
	const u8 *ptr = (const u8 *)data;

	if (num <= 0) {
		printk("\n");
		return;
	}

	while (num >= 16) {
		printk("%02X %02X %02X %02X %02X %02X %02X %02X "
		       "%02X %02X %02X %02X %02X %02X %02X %02X\n",
		       ptr[0], ptr[1], ptr[2], ptr[3],
		       ptr[4], ptr[5], ptr[6], ptr[7],
		       ptr[8], ptr[9], ptr[10], ptr[11],
		       ptr[12], ptr[13], ptr[14], ptr[15]);
		num -= 16;
		ptr += 16;
	}
	if (num > 0) {
		while (--num > 0)
			printk("%02X ", *ptr++);
		printk("%02X\n", *ptr);
	}

}

// OWI: Maybe could replace acx_dump_bytes() ? ... avail in all kernel versions ?
void hexdump(char *note, unsigned char *buf, unsigned int len)
{
               printk(KERN_INFO "%s:\n", note);
               print_hex_dump(KERN_CONT, "", DUMP_PREFIX_OFFSET, 16, 1, buf, len,
                       false);
}


