# SPDX-License-Identifier: BSD-2-Clause
# Tinsel root
TINSEL_ROOT=../..

include $(TINSEL_ROOT)/globals.mk

APP ?= simple

# Local compiler flags
CFLAGS = $(RV_CFLAGS) -O2 -I $(INC)
LDFLAGS = -melf32lriscv -G 0 

.PHONY: all
all: code.v data.v run out.ppm $(APP).png.64

$(APP).png.64: $(APP).png
	base64 $(APP).png > $(APP).png.64

$(APP).png: out.ppm
	convert out.ppm $(APP).png

out.ppm: run
	./run

code.v: $(APP).elf
	checkelf.sh $(APP).elf
	$(RV_OBJCOPY) -O verilog --only-section=.text $< $@

data.v: $(APP).elf
	$(RV_OBJCOPY) -O verilog --remove-section=.text \
                --set-section-flags .bss=alloc,load,contents $< $@

$(APP).elf: $(APP).c link.ld $(INC)/config.h $(INC)/tinsel.h entry.o
	$(RV_CC) $(CFLAGS) -I. -Wall -c -o $(APP).o $< 2> compiler.err
	$(RV_LD) $(LDFLAGS) -T link.ld -o $@ entry.o $(APP).o

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
	rm -f *.o *.elf link.ld *.v run sim *.ppm *.png *.err *.64
	rm -f 0.c 1.c 2.c 3.c 4.c 5.c 6.c 7.c 8.c 9.c
