# set to 0 if you don't want any debugging code to be compiled in
export ACX_DEBUG=1

.PHONY: all config extract_firmware driver install clean

all:	config driver

config:
	@./Configure

extract_firmware: firmware
	make -C firmware extract_firmware

driver:
	make -C src

install:
	@echo "Sorry, we don't provide a system installation mechanism, since it would need to be distribution specific. Maybe you didn't read the part in the README file mentioning the installation details?"

clean:
	make -C src clean; \
	make -C firmware clean

distclean: 
#	@echo "WARNING this will remove all binaries, including the \
#		file that contains the firmware information. If you \
#		wish to continue press enter, else CTL-C"; \
#	pause; \
#	cd firmware; rm *.o -f; rm CVS -rf; cd ..; \
#	cd src; rm *.o -f; rm CVS -rf; cd ..; \
#	cd include; rm CVS -rf; cd ..; \ 
#	cd scripts; rm CVS -rf; cd ..;
