ifeq ($(VERSION).$(PATCHLEVEL), 2.6)

EXTRA_CFLAGS += -I$(src)/../include -DWLAN_HOSTIF=WLAN_PCI 

obj-m += acx100_pci.o

acx100_pci-objs := acx100.o acx100_conv.o acx100_helper.o acx100_helper2.o acx100_ioctl.o acx80211frm.o idma.o ihw.o

else
include $(ACX100_SRC)../config.mk

CC = gcc
#CFLAGS = -D__KERNEL__ -I/usr/src/linux/include  -Wall -Wstrict-prototypes  -Wno-trigraphs  -O -fno-strict-aliasing -fno-common -fomit-frame-pointer -pipe -mpreferred-stack-boundary=4 -march=i586 -DMODULE -DMODVERSIONS -include /usr/src/linux/include/linux/modversions.h -I../include/ -c -DWLAN_HOSTIF=WLAN_PCI
CFLAGS = -D__KERNEL__ -I$(KERNEL_BUILD)/include  -Wall -Wstrict-prototypes  -Wno-trigraphs  -O -fno-strict-aliasing -fno-common -fomit-frame-pointer -pipe -mpreferred-stack-boundary=4 -march=i586 -DMODULE -DMODVERSIONS -I../include/ -c -DWLAN_HOSTIF=WLAN_PCI

CFLAGS+=-include $(KERNEL_BUILD)/include/linux/modversions.h
LDFLAGS = -m elf_i386 -r

objects = acx100.o acx100_ioctl.o acx100_helper.o acx100_helper2.o ihw.o idma.o acx100_conv.o acx80211frm.o


acx100_pci.o: $(objects)
	ld $(LDFLAGS) -o $@ $^

$(objects): ../include/version.h \
	../include/p80211hdr.h \
	../include/p80211mgmt.h \
	../include/p80211msg.h \
	../include/p80211ioctl.h \
	../include/acx100.h \
	../include/acx100_conv.h \
	../include/p80211netdev.h \
	../include/p80211req.h \
	../include/p80211types.h \
	../include/acx100_helper.h \
	../include/acx100_helper2.h \
	../include/idma.h \
	../include/ihw.h \
	../include/acx100mgmt.h

clean:
	rm -f $(objects) acx100_pci.o *~ ../include/*~ ../scripts/*~

endif
