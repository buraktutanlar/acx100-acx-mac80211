/*
 * BOM Data Access
 * ==================================================
 */

/* Endianess: read[lw], write[lw] do little-endian conversion internally */
#define acx_readl(v)		readl((v))
#define acx_readw(v)		readw((v))
#define acx_writel(v, r)	writel((v), (r))
#define acx_writew(v, r)	writew((v), (r))

INLINE_IO u32 read_reg32(acx_device_t *adev, unsigned int offset)
{
#if ACX_IO_WIDTH == 32
	return acx_readl((u8 *) adev->iobase + adev->io[offset]);
#else
	return acx_readw((u8 *) adev->iobase + adev->io[offset])
	    + (acx_readw((u8 *) adev->iobase + adev->io[offset] + 2) << 16);
#endif
}

INLINE_IO u16 read_reg16(acx_device_t *adev, unsigned int offset)
{
	return acx_readw((u8 *) adev->iobase + adev->io[offset]);
}

INLINE_IO u8 read_reg8(acx_device_t *adev, unsigned int offset)
{
	return readb((u8 *) adev->iobase + adev->io[offset]);
}

INLINE_IO void write_reg32(acx_device_t *adev, unsigned int offset, u32 val)
{
#if ACX_IO_WIDTH == 32
	acx_writel(val, (u8 *) adev->iobase + adev->io[offset]);
#else
	acx_writew(val & 0xffff, (u8 *) adev->iobase + adev->io[offset]);
	acx_writew(val >> 16, (u8 *) adev->iobase + adev->io[offset] + 2);
#endif
}

INLINE_IO void write_reg16(acx_device_t *adev, unsigned int offset, u16 val)
{
	acx_writew(val, (u8 *) adev->iobase + adev->io[offset]);
}

INLINE_IO void write_reg8(acx_device_t *adev, unsigned int offset, u8 val)
{
	writeb(val, (u8 *) adev->iobase + adev->io[offset]);
}

/* Handle PCI posting properly:
 * Make sure that writes reach the adapter in case they require to be executed
 * *before* the next write, by reading a random (and safely accessible) register.
 * This call has to be made if there is no read following (which would flush the data
 * to the adapter), yet the written data has to reach the adapter immediately. */
INLINE_IO void write_flush(acx_device_t *adev)
{
	/* readb(adev->iobase + adev->io[IO_ACX_INFO_MAILBOX_OFFS]); */
	/* faster version (accesses the first register, IO_ACX_SOFT_RESET,
	 * which should also be safe): */
	readb(adev->iobase);
}
