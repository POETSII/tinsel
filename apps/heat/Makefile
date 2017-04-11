# Tinsel root
TINSEL_ROOT=../..

include $(TINSEL_ROOT)/globals.mk

# Local compiler flags
CFLAGS = $(RV_CFLAGS) -O2 -I $(INC)
LDFLAGS = -melf32lriscv -G 0 

.PHONY: all
all: code-v1.v data-v1.v code-v2.v data-v2.v

code-%.v: heat-%.elf
	$(RV_OBJCOPY) -O verilog --only-section=.text $< $@

data-%.v: heat-%.elf
	$(RV_OBJCOPY) -O verilog --remove-section=.text \
                --set-section-flags .bss=alloc,load,contents $< $@

heat-%.elf: heat-%.c link.ld $(INC)/config.h $(INC)/tinsel.h entry.o
	$(RV_CC) $(CFLAGS) -Wall -c -o heat.o $<
	$(RV_LD) $(LDFLAGS) -T link.ld -o $@ entry.o heat.o

entry.o:
	$(RV_CC) $(CFLAGS) -Wall -c -o entry.o entry.S

link.ld: genld.sh
	./genld.sh > link.ld

sim-v1.ppm: code-v1.v data-v1.v vis
	LD_LIBRARY_PATH=$(QUARTUS_ROOTDIR)/linux64 hostlink \
    code-v1.v data-v1.v -n 65536 | \
    ./vis 256 8 sim-v1.ppm

fpga-v1.ppm: code-v1.v data-v1.v vis
	LD_LIBRARY_PATH=$(QUARTUS_ROOTDIR)/linux64 hostlink \
    code-v1.v data-v1.v -n 65536 | \
    ./vis 256 8 fpga-v1.ppm

sim-v2.ppm: code-v2.v data-v2.v vis
	LD_LIBRARY_PATH=$(QUARTUS_ROOTDIR)/linux64 hostlink \
    code-v2.v data-v2.v -n 262144 | \
    ./vis 512 16 sim-v2.ppm

fpga-v2.ppm: code-v2.v data-v2.v vis
	LD_LIBRARY_PATH=$(QUARTUS_ROOTDIR)/linux64 hostlink \
    code-v2.v data-v2.v -n 262144 | \
    ./vis 512 16 fpga-v2.ppm

vis: vis.cpp
	g++ -O2 vis.cpp -o vis

.PHONY: run-jtag
run-jtag: code-v1.v data-v1.v vis
	echo P3 512 512 255 0 > fpga-v1.ppm
	eog fpga-v1.ppm &
	LD_LIBRARY_PATH=$(QUARTUS_ROOTDIR)/linux64 hostlink \
    code-v1.v data-v1.v | \
    ./vis 256 8 fpga-v1.ppm

.PHONY: run-sim
run-sim: sim.ppm

$(INC)/config.h: $(TINSEL_ROOT)/config.py
	make -C $(INC)

.PHONY: clean
clean:
	rm -f *.o *.elf link.ld *.v vis *.txt *.ppm
