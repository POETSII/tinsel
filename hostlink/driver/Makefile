KVER ?= $(shell uname -r)
KERNEL_SRC_DIR  ?= /lib/modules/$(KVER)/build

obj-m := dmabuffer.o

all:
	make -C $(KERNEL_SRC_DIR) M=$(PWD) modules

clean:
	make -C $(KERNEL_SRC_DIR) M=$(PWD) clean
	rm -f *.ur-safe

