#ifndef _ACX_UTIL_H_
#define _ACX_UTIL_H_

/*
 * acx_util.h: define some utilities used all along the code.
 *
 * Copyright (c) 2008 Francis Galiegue <fgaliegue@gmail.com> for the ACX10
 * driver project.
 *
 * This file is licensed under the GPLv2.
 */

#include <asm/io.h>

#include "acx_config.h" /* Needed for ACX_IO_WIDTH */
#include "acx_struct.h"

/*
 * I/O routines
 *
 * Basically, make LE/BE safe versions of the OS routines.
 *
 * Declare them all static. They are ultra short operations anyway, and the
 * header is reentrant, so no worries there.
 *
 * FIXME: some devices use ssb_*, whatever that turns out to be. Maybe this
 * could be used instead of using self-constructed offsets?
 *
 * FIXME, 2: use smp_[rw]mb() after each read/write?
 */

#define acx_readl(val)	le32_to_cpu(readl((val)))
#define acx_readw(val)	le16_to_cpu(readw((val)))

#define acx_writew(val, port)	writew(le16_to_cpu((val)), port)
#define acx_writel(val, port)	writel(le32_to_cpu((val)), port)

static inline u32 read_reg32(acx_device_t * adev, unsigned int offset)
{
#if ACX_IO_WIDTH == 32
	return acx_readl((u8 *) adev->iobase + adev->io[offset]);
#else
	return acx_readw((u8 *) adev->iobase + adev->io[offset])
	    + (acx_readw((u8 *) adev->iobase + adev->io[offset] + 2) << 16);
#endif
}

static inline u16 read_reg16(acx_device_t * adev, unsigned int offset)
{
	return acx_readw((u8 *) adev->iobase + adev->io[offset]);
}

static inline u8 read_reg8(acx_device_t * adev, unsigned int offset)
{
	return readb((u8 *) adev->iobase + adev->io[offset]);
}

static inline void write_reg32(acx_device_t * adev, unsigned int offset, u32 val)
{
#if ACX_IO_WIDTH == 32
	acx_writel(val, (u8 *) adev->iobase + adev->io[offset]);
#else
	acx_writew(val & 0xffff, (u8 *) adev->iobase + adev->io[offset]);
	acx_writew(val >> 16, (u8 *) adev->iobase + adev->io[offset] + 2);
#endif
}

static inline void write_reg16(acx_device_t * adev, unsigned int offset, u16 val)
{
	acx_writew(val, (u8 *) adev->iobase + adev->io[offset]);
}

static inline void write_reg8(acx_device_t * adev, unsigned int offset, u8 val)
{
	writeb(val, (u8 *) adev->iobase + adev->io[offset]);
}

#endif /* _ACX_UTIL_H_ */
