ifndef ACX_DEBUG
  ACX_DEBUG=1
  $(warning ACX_DEBUG was not defined, assuming 1)
endif

export CPPFLAGS += -DACX_DEBUG=$(ACX_DEBUG)

# 2.6 in-tree (kbuild)
ifeq ($(VERSION).$(PATCHLEVEL), 2.6)

EXTRA_CFLAGS += -Idrivers/net/wireless/acx100 -DWLAN_HOSTIF=WLAN_PCI

obj-m += acx100_pci.o

acx100_pci-objs := acx100.o acx100_conv.o acx100_helper.o acx100_helper2.o acx100_ioctl.o acx80211frm.o idma.o ihw.o

else

# 2.4 out-of-tree
include $(ACX100_SRC)../config.mk

CC = gcc
USBCFLAGS = -D__KERNEL__ -I$(KERNEL_BUILD)/include  -Wall -Wstrict-prototypes  -Wno-trigraphs  -O -fno-strict-aliasing -fno-common -fomit-frame-pointer -pipe -mpreferred-stack-boundary=4 -march=i586 -DMODULE -DMODVERSIONS -I../include/ -c -DWLAN_HOSTIF=WLAN_USB -DDEBUG -DACX_DEBUG

USBCFLAGS+=-include $(KERNEL_BUILD)/include/linux/modversions.h
CFLAGS += -Wall -Wstrict-prototypes -Wno-trigraphs -O -fno-strict-aliasing -fno-common -fomit-frame-pointer -pipe -mpreferred-stack-boundary=4 -march=i586
#CPPFLAGS += -D__KERNEL__ -DMODULE -DMODVERSIONS -DWLAN_HOSTIF=WLAN_PCI -I$(KERNEL_BUILD)/include -I../include -include $(KERNEL_BUILD)/include/linux/modversions.h
CPPFLAGS += -D__KERNEL__ -DMODULE -DWLAN_HOSTIF=WLAN_PCI -I$(KERNEL_BUILD)/include -I../include
LDFLAGS = -m elf_i386 -r

includes = $(wildcard ../include/*.h)
objects = acx100.o acx100_ioctl.o acx100_helper.o acx100_helper2.o ihw.o idma.o acx100_conv.o acx80211frm.o

usb_objs = acx100_usb.o ihw_usb.o acx100usb_helper.o acx100usb_ioctl.o acx100usb_helper2.o acx80211frm.o idmausb.o acx100_conv.o 

.PHONY: all clean

all: acx100_pci.o acx100usb.o

acx100_pci.o: $(objects)
	$(LD) $(LDFLAGS) -o $@ $^

acx100_usb.o: acx100_usb.c
	$(CC) $(USBCFLAGS) -c -o $@ $<

acx100usb.o: $(usb_objs)
	ld $(LDFLAGS) -o $@ $^

acx100usb_helper.o: acx100_helper.c Makefile
	$(CC) $(USBCFLAGS) -c -o $@ $<

ihw_usb.o: ihw_usb.c Makefile
	$(CC) $(USBCFLAGS) -c -o $@ $<

acx100usb_ioctl.o: acx100_ioctl.c Makefile
	$(CC) $(USBCFLAGS) -c -o $@ $<

acx100usb_helper2.o: acx100_helper2.c Makefile
	$(CC) $(USBCFLAGS) -c -o $@ $<

idmausb.o: idma.c
	$(CC) $(USBCFLAGS) -c -o $@ $<

$(objects): $(includes)

$(usb_objs): $(includes)

clean:
	rm -f $(objects) acx100_pci.o acx100_usb.o *~ ../include/*~ ../scripts/*~

endif
