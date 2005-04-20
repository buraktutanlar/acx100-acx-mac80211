/* --------------------------------------------------------------------
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

/* This file expects INLINE_IO to be:
 * a) #defined to 'static inline': will emit inlined functions (for .h file); or
 * b) #defined to '': will emit non-inlined functions (for .c file); or
 * c) not #defined at all: emit prototypes only
 */

#ifndef INLINE_IO

u32 acx_read_reg32(wlandevice_t *priv, unsigned int offset);
u16 acx_read_reg16(wlandevice_t *priv, unsigned int offset);
u8 acx_read_reg8(wlandevice_t *priv, unsigned int offset);
void acx_write_reg32(wlandevice_t *priv, unsigned int offset, u32 val);
void acx_write_reg16(wlandevice_t *priv, unsigned int offset, u16 val);
void acx_write_reg8(wlandevice_t *priv, unsigned int offset, u8 val);

#else

#include <linux/pci.h> /* readl() etc. */

INLINE_IO u32 acx_read_reg32(wlandevice_t *priv, unsigned int offset)
{
#if ACX_IO_WIDTH == 32
	return readl(priv->iobase + priv->io[offset]);
#else 
	return readw(priv->iobase + priv->io[offset])
	    + (readw(priv->iobase + priv->io[offset] + 2) << 16);
#endif
}

INLINE_IO u16 acx_read_reg16(wlandevice_t *priv, unsigned int offset)
{
	return readw(priv->iobase + priv->io[offset]);
}

INLINE_IO u8 acx_read_reg8(wlandevice_t *priv, unsigned int offset)
{
	return readb(priv->iobase + priv->io[offset]);
}

INLINE_IO void acx_write_reg32(wlandevice_t *priv, unsigned int offset, u32 val)
{
#if ACX_IO_WIDTH == 32
	writel(val, priv->iobase + priv->io[offset]);
#else 
	writew(val & 0xffff, priv->iobase + priv->io[offset]);
	writew(val >> 16, priv->iobase + priv->io[offset] + 2);
#endif
}

INLINE_IO void acx_write_reg16(wlandevice_t *priv, unsigned int offset, u16 val)
{
	writew(val, priv->iobase + priv->io[offset]);
}

INLINE_IO void acx_write_reg8(wlandevice_t *priv, unsigned int offset, u8 val)
{
	writeb(val, priv->iobase + priv->io[offset]);
}
#endif
