FILE = arduino
obj-m := $(FILE).o
ccflags-y := -std=gnu99 -Wno-declaration-after-statement
SRC := $(shell pwd)

local:
		make -C /usr/src/linux-headers-$(shell uname -r)  M=$(shell pwd modules)

clean:
		make -C /lib/modules/$(shell uname -r)/build M=$(CURDIR) clean
		sudo rm *.o*