
all: kmod mkfs.vvsfs truncate view.vvsfs

mkfs.vvsfs: ./utils/mkfs.vvsfs.c
	gcc -Wall -o ./utils/$@ $<

truncate: ./utils/truncate.c
	gcc -Wall -o ./utils/$@ $<

view.vvsfs: ./utils/view.vvsfs.c
	gcc -Wall -o ./utils/$@ $<

ifneq ($(KERNELRELEASE),)

include Kbuild

else

KDIR ?= /usr/src/linux-headers-`uname -r`

kmod:
	$(MAKE) -C $(KDIR) M=$$PWD

endif

clean: clean-util clean-kmod

clean-util:
	rm -f utils/mkfs.vvsfs
	rm -f utils/truncate
	rm -f utils/view.vvsfs

clean-kmod:
	rm -f vvsfs/*.ko
	rm -f vvsfs/*.o
	rm -f vvsfs/*.mod*
	rm -f vvsfs/.*.cmd
	rm -f modules.order
	rm -f Module.symvers
	rm -f .*.cmd
