obj-$(CONFIG_ACX_D80211) += acx-d80211.o

acx-d80211-obj-$(CONFIG_ACX_D80211_PCI) += pci.o
acx-d80211-obj-$(CONFIG_ACX_D80211_USB) += usb.o

#acx-d80211-objs := wlan.o conv.o ioctl.o common.o $(acx-d80211-obj-y)
acx-d80211-objs :=  common.o $(acx-d80211-obj-y)

# Use this if you have proper Kconfig integration:

#obj-$(CONFIG_ACX) += acx.o
#
#acx-obj-$(CONFIG_ACX_PCI) += pci.o
#acx-obj-$(CONFIG_ACX_USB) += usb.o
#
#acx-objs := wlan.o conv.o ioctl.o common.o $(acx-obj-y)
