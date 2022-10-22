
all: kmod mkfs.dummyfs truncate view.dummyfs

mkfs.dummyfs: ./utils/mkfs.dummyfs.c
	gcc -Wall -o ./utils/$@ $<

truncate: ./utils/truncate.c
	gcc -Wall -o ./utils/$@ $<

view.dummyfs: ./utils/view.dummyfs.c
	gcc -Wall -o ./utils/$@ $<

ifneq ($(KERNELRELEASE),)

include Kbuild

else

KDIR ?= /usr/src/linux-headers-`uname -r`

kmod:
	$(MAKE) -C $(KDIR) M=$(PWD)

endif

clean: clean-util
	$(MAKE) -C $(KDIR) M=$(PWD) clean

clean-util:
	rm -f utils/mkfs.dummyfs
	rm -f utils/truncate
	rm -f utils/view.dummyfs
