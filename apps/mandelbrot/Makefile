# Tinsel root
TINSEL_ROOT=../..

include $(TINSEL_ROOT)/globals.mk

# Local compiler flags
CFLAGS = $(RV_CFLAGS) -O2 -I $(INC)
LDFLAGS = -melf32lriscv -G 0 

.PHONY: all
all: code-v1.v data-v1.v

code-%.v: man-%.elf
	$(RV_OBJCOPY) -O verilog --only-section=.text $< $@

data-%.v: man-%.elf
	$(RV_OBJCOPY) -O verilog --remove-section=.text \
                --set-section-flags .bss=alloc,load,contents $< $@

man-%.elf: man-%.c link.ld $(INC)/config.h $(INC)/tinsel.h entry.o
	$(RV_CC) $(CFLAGS) -Wall -c -o man.o $<
	$(RV_LD) $(LDFLAGS) -T link.ld -o $@ entry.o man.o

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

vis: vis.cpp
	g++ -O2 vis.cpp -o vis

.PHONY: fpga-download
fpga-download:
	make -C ../../de5 download-sof

.PHONY: run-jtag
run-jtag: code-v1.v data-v1.v vis
	echo P3 512 512 255 0 > fpga-v1.ppm
	eog fpga-v1.ppm &
	LD_LIBRARY_PATH=$(QUARTUS_ROOTDIR)/linux64 hostlink \
    code-v1.v data-v1.v | \
    ./vis 256 8 fpga-v1.ppm

.PHONY: download-run-jtag
download-run-jtag: fpga-download run-jtag

.PHONY: run-sim
run-sim: sim-v1.ppm

$(INC)/config.h: $(TINSEL_ROOT)/config.py
	make -C $(INC)

.PHONY: clean
clean:
	rm -f *.o *.elf link.ld *.v vis *.txt *.ppm
