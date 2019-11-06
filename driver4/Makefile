FILE = arduano
obj-m := $(FILE).o
ccflags-y := -std=gnu99 -Wno-declaration-after-statement
SRC := $(shell pwd)

local:
		make -C /usr/src/linux-headers-$(shell uname -r)  M=$(shell pwd modules)

clean:
		sudo rm modules.order
		sudo rm Module*
		
		sudo rm -f .cache.mk
		sudo rm -rf .tmp/*
		sudo rm -r .tmp_versions
		sudo rm -f .$(FILE)*
		sudo rm -f $(FILE).mod*
		sudo rm -f $(FILE) memory.mod*
		sudo rm -f $(FILE).ko