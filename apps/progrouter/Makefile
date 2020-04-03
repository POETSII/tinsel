# Tinsel root
TINSEL_ROOT=../..

ifndef QUARTUS_ROOTDIR
  $(error Please set QUARTUS_ROOTDIR)
endif

include $(TINSEL_ROOT)/globals.mk

# RISC-V compiler flags
CFLAGS = $(RV_CFLAGS) -O2 -I $(INC)
LDFLAGS = -melf32lriscv -G 0 

.PHONY: all
all: code.v data.v run

code.v: progrouter.elf
	checkelf.sh progrouter.elf
	$(RV_OBJCOPY) -O verilog --only-section=.text progrouter.elf code.v

data.v: progrouter.elf
	$(RV_OBJCOPY) -O verilog --remove-section=.text \
                --set-section-flags .bss=alloc,load,contents \
                progrouter.elf data.v

progrouter.elf: progrouter.cpp link.ld $(INC)/config.h $(INC)/tinsel.h entry.o $(LIB)/lib.o
	$(RV_CPPC) $(CFLAGS) -Wall -c -o progrouter.o progrouter.cpp
	$(RV_LD) $(LDFLAGS) -T link.ld -o progrouter.elf entry.o progrouter.o $(LIB)/lib.o

entry.o:
	$(RV_CPPC) $(CFLAGS) -Wall -c -o entry.o entry.S

link.ld: genld.sh
	./genld.sh > link.ld

$(LIB)/lib.o:
	make -C $(LIB)

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
