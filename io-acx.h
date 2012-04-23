
// ick: shouldnt declare things in headers
// cuz including it 2x is bad

static const u16 IO_ACX100[] = {
	0x0000,			/* IO_ACX_SOFT_RESET */

	0x0014,			/* IO_ACX_SLV_MEM_ADDR */
	0x0018,			/* IO_ACX_SLV_MEM_DATA */
	0x001c,			/* IO_ACX_SLV_MEM_CTL */
	0x0020,			/* IO_ACX_SLV_END_CTL */

	0x0034,			/* IO_ACX_FEMR */

	0x007c,			/* IO_ACX_INT_TRIG */
	0x0098,			/* IO_ACX_IRQ_MASK */
	0x00a4,			/* IO_ACX_IRQ_STATUS_NON_DES */
	0x00a8,			/* IO_ACX_IRQ_REASON */
	0x00ac,			/* IO_ACX_IRQ_ACK */
	0x00b0,			/* IO_ACX_HINT_TRIG */

	0x0104,			/* IO_ACX_ENABLE */

	0x0250,			/* IO_ACX_EEPROM_CTL */
	0x0254,			/* IO_ACX_EEPROM_ADDR */
	0x0258,			/* IO_ACX_EEPROM_DATA */
	0x025c,			/* IO_ACX_EEPROM_CFG */

	0x0268,			/* IO_ACX_PHY_ADDR */
	0x026c,			/* IO_ACX_PHY_DATA */
	0x0270,			/* IO_ACX_PHY_CTL */

	0x0290,			/* IO_ACX_GPIO_OE */

	0x0298,			/* IO_ACX_GPIO_OUT */

	0x02a4,			/* IO_ACX_CMD_MAILBOX_OFFS */
	0x02a8,			/* IO_ACX_INFO_MAILBOX_OFFS */
	0x02ac,			/* IO_ACX_EEPROM_INFORMATION */

	0x02d0,			/* IO_ACX_EE_START */
	0x02d4,			/* IO_ACX_SOR_CFG */
	0x02d8			/* IO_ACX_ECPU_CTRL */
};

static const u16 IO_ACX111[] = {
	0x0000,			/* IO_ACX_SOFT_RESET */

	0x0014,			/* IO_ACX_SLV_MEM_ADDR */
	0x0018,			/* IO_ACX_SLV_MEM_DATA */
	0x001c,			/* IO_ACX_SLV_MEM_CTL */
	0x0020,			/* IO_ACX_SLV_END_CTL */

	0x0034,			/* IO_ACX_FEMR */

	0x00b4,			/* IO_ACX_INT_TRIG */
	0x00d4,			/* IO_ACX_IRQ_MASK */
	/* we do mean NON_DES (0xf0), not NON_DES_MASK which is at 0xe0: */
	0x00f0,			/* IO_ACX_IRQ_STATUS_NON_DES */
	0x00e4,			/* IO_ACX_IRQ_REASON */
	0x00e8,			/* IO_ACX_IRQ_ACK */
	0x00ec,			/* IO_ACX_HINT_TRIG */

	0x01d0,			/* IO_ACX_ENABLE */

	0x0338,			/* IO_ACX_EEPROM_CTL */
	0x033c,			/* IO_ACX_EEPROM_ADDR */
	0x0340,			/* IO_ACX_EEPROM_DATA */
	0x0344,			/* IO_ACX_EEPROM_CFG */

	0x0350,			/* IO_ACX_PHY_ADDR */
	0x0354,			/* IO_ACX_PHY_DATA */
	0x0358,			/* IO_ACX_PHY_CTL */

	0x0374,			/* IO_ACX_GPIO_OE */

	0x037c,			/* IO_ACX_GPIO_OUT */

	0x0388,			/* IO_ACX_CMD_MAILBOX_OFFS */
	0x038c,			/* IO_ACX_INFO_MAILBOX_OFFS */
	0x0390,			/* IO_ACX_EEPROM_INFORMATION */

	0x0100,			/* IO_ACX_EE_START */
	0x0104,			/* IO_ACX_SOR_CFG */
	0x0108,			/* IO_ACX_ECPU_CTRL */
};
