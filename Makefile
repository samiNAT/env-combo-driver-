obj-m := env-combo.o

KDIR ?= /path/to/linux-kernel-build-dir
ARCH ?= arm64
CROSS_COMPILE ?= aarch64-poky-linux-

all:
	make -C $(KDIR) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules

clean:
	make -C $(KDIR) M=$(PWD) clean
