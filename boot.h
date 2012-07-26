#ifndef _ACX_BOOT_H_
#define _ACX_BOOT_H_

void acx_get_firmware_version(acx_device_t * adev);
void acx_display_hardware_details(acx_device_t *adev);
firmware_image_t *acx_read_fw(struct device *dev, const char *file, u32 * size);
void acx_parse_configoption(acx_device_t *adev,
                            const acx111_ie_configoption_t *pcfg);

int acx_read_phy_reg(acx_device_t *adev, u32 reg, u8 *charbuf);
int acx_write_phy_reg(acx_device_t *adev, u32 reg, u8 value);

#endif
