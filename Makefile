module = memutil.o
# ERNELRELEASEが定義されているならば
# カーネルビルドシステムから起動されているので
# そちらの書式を使用
ifneq ($(KERNELRELEASE),)
	obj-m := $(module)
	mydriver-objs := $(.c=.o)

# そうでないならコマンドラインから起動されているため
# カーネルビルドシステムを呼び出す
else
	KERNELSRC ?= /lib/modules/$(shell uname -r)/build
	PWD := $(shell pwd)

# include .config

filename := .config
fileexists = $(shell ls -a | grep ${filename} | grep -v sample.config )
ifeq (${fileexists}, ${filename})
	message = "${filename} found"
	include .config
	CROSSMAKE := CROSS_COMPILE=$(CROSS_COMPILE) ARCH=$(ARCH)
else
	message = "${filename} not found"
endif
$(warning $(message) )
$(warning KERNELSRC = $(KERNELSRC))


all: modules

modules:
	$(MAKE) -C $(KERNELSRC) M=$(PWD) $(CROSSMAKE) modules

debug:
	#KCFLAGS = -O -g -DMEMUTIL_DEBUG # "-O"はインライン展開に必要
	$(MAKE) -C $(KERNELSRC) M=$(PWD) $(CROSSMAKE) modules KCFLAGS="-O -g -DMEMUTIL_DEBUG"

clean:
	$(MAKE) -C $(KERNELSRC) M=$(PWD) $(CROSSMAKE) clean

cross:
	$(MAKE) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KERNELDIR_CROSS) -I$(KERNELDIR_CROSS)/include M=$(PWD) modules

install:
	sudo cp memutil.ko /nfs/m3ulcb/home/root/

endif

