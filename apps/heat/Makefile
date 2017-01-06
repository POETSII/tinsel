# Tinsel root
TINSEL_ROOT=../..

include $(TINSEL_ROOT)/globals.mk

# Local compiler flags
CFLAGS = -march=$(RV_ARCH) -msoft-float -O2 -I $(INC)
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
	hostlink-sim boot code.v data.v
	hostlink-sim dump -n 57600 > dump.txt
	./vis dump.txt > out.ppm

fpga.ppm: code.v data.v vis
	hostlink boot code.v data.v
	hostlink dump -n 57600 > dump.txt
	./vis dump.txt > out.ppm

vis: vis.cpp
	g++ -O2 vis.cpp -o vis

$(INC)/config.h: $(TINSEL_ROOT)/config.py
	make -C $(INC)

.PHONY: clean
clean:
	rm -f *.o *.elf link.ld *.v vis dump.txt *.ppm
