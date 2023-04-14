#
# Makefile for Xtensa Proxy driver
#
CONFIG_MODULE_SIG=n

KERNEL = $(shell uname -r)

all:
	make -C /lib/modules/$(KERNEL)/build M=$(PWD) modules ccflags-y="-I$(PWD)/include -I$(PWD)/include/sys/xt-shmem"

clean:
	make -C /lib/modules/$(KERNEL)/build M=$(PWD) clean
