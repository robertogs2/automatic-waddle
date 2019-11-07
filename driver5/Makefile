FILE = arduano
obj-m += $(FILE).o
ccflags-y := -std=gnu99 -Wno-declaration-after-statement
all:
		make -C /lib/modules/$(shell uname -r)/build M=$(CURDIR) modules
clean:
		make -C /lib/modules/$(shell uname -r)/build M=$(CURDIR) clean
