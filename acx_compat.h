
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 0, 0) || LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 40)
#include <linux/ratelimit.h>
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 33)

#define printk_ratelimited(args...) \
do { \
	if (printk_ratelimit()) \
		printk(args); \
} while (0)

#endif


