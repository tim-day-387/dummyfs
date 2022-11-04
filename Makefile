# Build everything
all: kmod mkfs.dummyfs truncate view.dummyfs


# Builds utils
mkfs.dummyfs: ./utils/mkfs.dummyfs.c
	$(CC) -Wall -o ./utils/$@ $<

truncate: ./utils/truncate.c
	$(CC) -Wall -o ./utils/$@ $<

view.dummyfs: ./utils/view.dummyfs.c
	$(CC) -Wall -o ./utils/$@ $<


# Build kernel module
ifneq ($(KERNELRELEASE),)

include Kbuild

else

KDIR ?= /usr/src/linux-headers-`uname -r`

kmod:
	$(MAKE) -C $(KDIR) CC=$(CC) M=$(PWD)

endif


# Clean everything
clean: clean-util
	$(MAKE) -C $(KDIR) CC=$(CC) M=$(PWD) clean


# Clean utils
clean-util:
	rm -f utils/mkfs.dummyfs
	rm -f utils/truncate
	rm -f utils/view.dummyfs


# Check formatting
check-format:
	./scripts/format-checker.sh dummyfs/block.c
	./scripts/format-checker.sh dummyfs/block.h
	./scripts/format-checker.sh dummyfs/inode.c
	./scripts/format-checker.sh dummyfs/inode.h
	./scripts/format-checker.sh dummyfs/mod.c
	./scripts/format-checker.sh dummyfs/mod.h
	./scripts/format-checker.sh utils/mkfs.dummyfs.c
	./scripts/format-checker.sh utils/truncate.c
	./scripts/format-checker.sh utils/view.dummyfs.c
