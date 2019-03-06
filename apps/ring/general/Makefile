# Tinsel root
TINSEL_ROOT=../../..

ifndef QUARTUS_ROOTDIR
  $(error Please set QUARTUS_ROOTDIR)
endif

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

ring.elf: ring.c link.ld $(INC)/config.h $(INC)/tinsel.h entry.o ring.h layout.h
	$(RV_CC) $(CFLAGS) -Wall -c -o ring.o ring.c
	$(RV_LD) $(LDFLAGS) -T link.ld -o ring.elf entry.o ring.o

GenLayout: GenLayout.cpp $(INC)/config.h ring.h
	gcc -I $(INC) GenLayout.cpp -o GenLayout

layout.h: GenLayout
	./GenLayout > layout.h

entry.o:
	$(RV_CC) $(CFLAGS) -Wall -c -o entry.o entry.S

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
	rm -f *.o *.elf link.ld *.v run sim GenLayout layout.h
