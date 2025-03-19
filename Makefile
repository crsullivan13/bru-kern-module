obj-m = bru_driver.o

KERNEL_SRC := #This should point to your linux source in Firemarshal e.g. firesim/sw/firesim-software/boards/firechip/linux
CROSS_COMPILE := riscv64-unknown-linux-gnu-
ARCH := riscv
PWD := $(CURDIR)
EXTRA_CFLAGS += -DFIRESIM
all:
	make -C $(KERNEL_SRC) M=$(PWD) CROSS_COMPILE=$(CROSS_COMPILE)  ARCH=$(ARCH) EXTRA_CFLAGS="$(EXTRA_CFLAGS)" modules

clean:
	rm *.o *.ko *.mod.* *.mod
