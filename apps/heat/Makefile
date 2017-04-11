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
	hostlink-sim code-v1.v data-v1.v -n 65536 > dump-v1.txt
	./vis 256 dump-v1.txt > sim-v1.ppm

fpga-v1.ppm: code-v1.v data-v1.v vis
	LD_LIBRARY_PATH=$(QUARTUS_ROOTDIR)/linux64 hostlink \
    code-v1.v data-v1.v -n 65536 > dump-v1.txt
	./vis 256 dump-v1.txt > fpga-v1.ppm

sim-v2.ppm: code-v2.v data-v2.v vis
	hostlink-sim code-v2.v data-v2.v -n 262144 > dump-v2.txt
	./vis 512 dump-v2.txt > sim-v2.ppm

fpga-v2.ppm: code-v2.v data-v2.v vis
	LD_LIBRARY_PATH=$(QUARTUS_ROOTDIR)/linux64 hostlink \
    code-v2.v data-v2.v -n 262144 > dump-v2.txt
	./vis 512 dump-v2.txt > fpga-v2.ppm

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
	rm -f *.o *.elf link.ld *.v vis *.txt *.ppm
