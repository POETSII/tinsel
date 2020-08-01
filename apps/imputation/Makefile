# SPDX-License-Identifier: BSD-2-Clause
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
all: code_0.v data_0.v code_1.v data_1.v run

code_%.v: ping_%.elf
	checkelf.sh $<
	$(RV_OBJCOPY) -O verilog --only-section=.text $< $@

data_%.v: ping_%.elf
	$(RV_OBJCOPY) -O verilog --remove-section=.text \
                --set-section-flags .bss=alloc,load,contents $< $@

ping_%.elf: ping_%.c link_%.ld $(INC)/config.h $(INC)/tinsel.h entry.o
	$(RV_CC) $(CFLAGS) -Wall -c -o $<.o $<
	$(RV_LD) $(LDFLAGS) -T $(word 2,$^) -o $@ entry.o $<.o

entry.o:
	$(RV_CC) $(CFLAGS) -Wall -c -o entry.o entry.S

link_0.ld: genld.sh
	./genld.sh 0x2000000 > $@

link_1.ld: genld.sh
	./genld.sh 0x3000000 > $@

$(INC)/config.h: $(TINSEL_ROOT)/config.py
	make -C $(INC)

$(HL)/%.o:
	make -C $(HL)

run: run.cpp $(HL)/*.o
	g++ -O2 -I $(INC) -I $(HL) -o run run.cpp model.cpp $(HL)/*.o

sim: run.cpp $(HL)/sim/*.o
	g++ -O2 -I $(INC) -I $(HL) -o sim run.cpp model.cpp $(HL)/sim/*.o

.PHONY: clean
clean:
	rm -f *.o *.elf *.ld *.v run sim
