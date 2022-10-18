
all: kernel_mod mkfs.vvsfs truncate view.vvsfs

mkfs.vvsfs: ./utils/mkfs.vvsfs.c
	gcc -Wall -o ./utils/$@ $<

truncate: ./utils/truncate.c
	gcc -Wall -o ./utils/$@ $<

view.vvsfs: ./utils/view.vvsfs.c
	gcc -Wall -o ./utils/$@ $<

ifneq ($(KERNELRELEASE),)
# kbuild part of makefile, for backwards compatibility
include Kbuild

else
# normal makefile
KDIR ?= /usr/src/linux-headers-`uname -r`

kernel_mod:
	$(MAKE) -C $(KDIR) M=$$PWD

endif
