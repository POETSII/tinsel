# Tinsel root
TINSEL_ROOT=../..

include $(TINSEL_ROOT)/globals.mk

# Local compiler flags
CFLAGS = $(RV_CFLAGS) -O2 -I $(INC)
LDFLAGS = -melf32lriscv -G 0 

.PHONY: all
all: code.v data.v

code.v: heat.elf
	$(RV_OBJCOPY) -O verilog --only-section=.text heat.elf code.v

data.v: heat.elf
	$(RV_OBJCOPY) -O verilog --remove-section=.text \
                --set-section-flags .bss=alloc,load,contents heat.elf data.v

heat.elf: heat.c link.ld $(INC)/config.h $(INC)/tinsel.h
	$(RV_CC) $(CFLAGS) -Wall -c -o heat.o heat.c
	$(RV_LD) $(LDFLAGS) -T link.ld -o heat.elf heat.o

link.ld: genld.sh
	./genld.sh > link.ld

sim.ppm: code.v data.v vis
	hostlink-sim code.v data.v -n 230400 > dump.txt
	./vis dump.txt > sim.ppm

fpga.ppm: code.v data.v vis
	LD_LIBRARY_PATH=$(QUARTUS_ROOTDIR)/linux64 hostlink \
    code.v data.v -n 230400 > dump.txt
	./vis dump.txt > fpga.ppm

vis: vis.cpp
	g++ -O2 vis.cpp -o vis

.PHONY: run-jtag
run-jtag: fpga.ppm

.PHONY: run-sim
run-sim: sim.ppm

$(INC)/config.h: $(TINSEL_ROOT)/config.py
	make -C $(INC)

.PHONY: clean
clean:
	rm -f *.o *.elf link.ld *.v vis dump.txt *.ppm
