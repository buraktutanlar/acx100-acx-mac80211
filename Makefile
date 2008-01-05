#obj-$(CONFIG_ACX_MAC80211) += acx-mac80211.o

#acx-mac80211-obj-$(CONFIG_ACX_MAC80211_PCI) += pci.o
#acx-mac80211-obj-$(CONFIG_ACX_MAC80211_USB) += usb.o

#acx-mac80211-objs := wlan.o conv.o ioctl.o common.o $(acx-mac80211-obj-y)
#acx-mac80211-objs :=  common.o $(acx-mac80211-obj-y)

obj-m = acx-mac80211.o
acx-mac80211-objs := common.o pci.o acx-mac80211.o

# Use this if you have proper Kconfig integration:

#obj-$(CONFIG_ACX) += acx.o
#
#acx-obj-$(CONFIG_ACX_PCI) += pci.o
#acx-obj-$(CONFIG_ACX_USB) += usb.o
#
#acx-objs := wlan.o conv.o ioctl.o common.o $(acx-obj-y)
