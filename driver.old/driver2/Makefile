obj-m := usb-driver.o
SRC := $(shell pwd)
all:
		$(MAKE) -C $(KERNEL_SRC) M=$(SRC)
modules_install:
		$(MAKE) -C $(KERNEL_SRC) M=$(SRC) modules_install
clean:
		rm -f *.o *~ core .depend .*.cmd *.ko *.mod.c
		rm -f Module.markers Module.symvers modules.order
		rm -rf .tmp_versions Modules.symvers

local:
		make -C /usr/src/linux-headers-$(shell uname -r) M=$(shell pwd modules)