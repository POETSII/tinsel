# SPDX-License-Identifier: BSD-2-Clause
# Tinsel root
TINSEL_ROOT=../..

include $(TINSEL_ROOT)/globals.mk

# RISC-V compiler flags
CFLAGS = $(RV_CFLAGS) -O2 -I $(INC)
LDFLAGS = -melf32lriscv -G 0 

.PHONY: all
all: code.v data.v run

code.v: ring.elf
	checkelf.sh ring.elf
	$(RV_OBJCOPY) -O verilog --only-section=.text ring.elf code.v

data.v: ring.elf
	$(RV_OBJCOPY) -O verilog --remove-section=.text \
                --set-section-flags .bss=alloc,load,contents ring.elf data.v

ring.elf: ring.c link.ld $(INC)/config.h $(INC)/tinsel.h entry.o
	$(RV_CC) $(CFLAGS) -Wall -c -o ring.o ring.c
	$(RV_LD) $(LDFLAGS) -T link.ld -o ring.elf entry.o ring.o

entry.o:
	$(RV_CC) $(CFLAGS) -Wall -c -o entry.o entry.S

link.ld: genld.sh
	./genld.sh > link.ld

$(INC)/config.h: $(TINSEL_ROOT)/config.py
	make -C $(INC)

$(HL)/hostlink.a :
	make -C $(HL) hostlink.a

run: run.cpp $(HL)/hostlink.a
	g++ -O2 -I $(INC) -I $(HL) -o run run.cpp $(HL)/hostlink.a

$(HL)/sim/hostlink.a :
	make -C $(HL) sim/hostlink.a

sim: run.cpp $(HL)/sim/hostlink.a
	g++ -O2 -I $(INC) -I $(HL) -o sim run.cpp $(HL)/sim/hostlink.a

.PHONY: clean
clean:
	rm -f *.o *.elf link.ld *.v run sim
