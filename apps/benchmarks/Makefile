# Tinsel root
TINSEL_ROOT=../..

include $(TINSEL_ROOT)/globals.mk

# Local compiler flags
CFLAGS = $(RV_CFLAGS) -O2 -I $(INC)
LDFLAGS = -melf32lriscv -G 0 

.PHONY: all
all: code.v data.v

code.v: main.elf
	$(RV_OBJCOPY) -O verilog --only-section=.text main.elf code.v

data.v: main.elf
	$(RV_OBJCOPY) -O verilog --remove-section=.text \
                --set-section-flags .bss=alloc,load,contents main.elf data.v

main.elf: main.c link.ld $(INC)/config.h $(INC)/tinsel.h entry.o $(LIB)/lib.o
	$(RV_CC) $(CFLAGS) -Wall -c -o main.o main.c
	$(RV_LD) $(LDFLAGS) -T link.ld -o main.elf entry.o main.o $(LIB)/lib.o

entry.o:
	$(RV_CC) $(CFLAGS) -Wall -c -o entry.o entry.S

$(LIB)/lib.o:
	make -C $(LIB)

link.ld: genld.sh
	./genld.sh > link.ld

$(INC)/config.h: $(TINSEL_ROOT)/config.py
	make -C $(INC)

.PHONY: run-jtag
run-jtag: code.v data.v
	LD_LIBRARY_PATH=$(QUARTUS_ROOTDIR)/linux64 hostlink code.v data.v -c

.PHONY: run-sim
run-sim: code.v data.v
	hostlink-sim code.v data.v -c

.PHONY: clean
clean:
	rm -f *.o *.elf link.ld *.v
