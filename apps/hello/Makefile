# Tinsel root
TINSEL_ROOT=../..

include $(TINSEL_ROOT)/globals.mk

# Local compiler flags
CFLAGS = -mabi=ilp32 -march=$(RV_ARCH) -O2 -I $(INC)
LDFLAGS = -melf32lriscv -G 0 

.PHONY: all
all: code.v data.v

code.v: hello.elf
	$(RV_OBJCOPY) -O verilog --only-section=.text hello.elf code.v

data.v: hello.elf
	$(RV_OBJCOPY) -O verilog --remove-section=.text \
                --set-section-flags .bss=alloc,load,contents hello.elf data.v

hello.elf: hello.c link.ld $(INC)/config.h $(INC)/tinsel.h
	$(RV_CC) $(CFLAGS) -Wall -c -o hello.o hello.c
	$(RV_LD) $(LDFLAGS) -T link.ld -o hello.elf hello.o

link.ld: genld.sh
	./genld.sh > link.ld

$(INC)/config.h: $(TINSEL_ROOT)/config.py
	make -C $(INC)

.PHONY: clean
clean:
	rm -f *.o *.elf link.ld *.v
