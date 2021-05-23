# SPDX-License-Identifier: BSD-2-Clause
# Tinsel root
TINSEL_ROOT=../..

include $(TINSEL_ROOT)/globals.mk

# Local compiler flags
CFLAGS = $(RV_CFLAGS) -O2 -I $(INC)
LDFLAGS = -melf32lriscv -G 0 

.PHONY: all
all: code.v data.v run

code.v: hello.elf
	checkelf.sh hello.elf
	$(RV_OBJCOPY) -O verilog --only-section=.text hello.elf code.v

data.v: hello.elf
	$(RV_OBJCOPY) -O verilog --remove-section=.text \
                --set-section-flags .bss=alloc,load,contents hello.elf data.v

hello.elf: hello.c link.ld $(INC)/config.h $(INC)/tinsel.h entry.o $(LIB)/lib.o
	$(RV_CC) $(CFLAGS) -Wall -c -o hello.o hello.c
	$(RV_LD) $(LDFLAGS) -T link.ld -o hello.elf entry.o hello.o $(LIB)/lib.o

entry.o:
	$(RV_CC) $(CFLAGS) -Wall -c -o entry.o entry.S

$(LIB)/lib.o:
	make -C $(LIB)

link.ld: genld.sh
	./genld.sh > link.ld

$(INC)/config.h: $(TINSEL_ROOT)/config.py
	make -C $(INC)

$(HL)/%.o:
	make -C $(HL)

run: run.cpp $(HL)/*.o
	g++ -O2 -I $(INC) -I $(HL) -o run run.cpp $(HL)/*.o

sim: run.cpp $(HL)/sim/*.o
	g++ -O2 -I $(INC) -I $(HL) -o sim run.cpp $(HL)/sim/*.o

.PHONY: clean
clean:
	rm -f *.o *.elf link.ld *.v run sim
