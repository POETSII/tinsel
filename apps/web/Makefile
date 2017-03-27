# Tinsel root
TINSEL_ROOT=../..

include $(TINSEL_ROOT)/globals.mk

# Local compiler flags
CFLAGS = $(RV_CFLAGS) -O2 -I $(INC) -I .
LDFLAGS = -melf32lriscv -G 0 

%-code.v: %.elf
	$(RV_OBJCOPY) -O verilog --only-section=.text $< $@

%-data.v: %.elf
	$(RV_OBJCOPY) -O verilog --remove-section=.text \
                --set-section-flags .bss=alloc,load,contents $< $@

%.o: %.c $(INC)/config.h $(INC)/tinsel.h grid.h
	$(RV_CC) $(CFLAGS) -Wall -c -o $@ $< 2> compiler.err

%.elf: %.o grid.o link.ld entry.o
	$(RV_LD) $(LDFLAGS) -T link.ld -o $@ entry.o $< grid.o

entry.o: entry.S
	$(RV_CC) $(CFLAGS) -Wall -c -o entry.o entry.S

link.ld: genld.sh
	./genld.sh > link.ld

%-sim.ppm: %-code.v %-data.v vis
	hostlink-sim $*-code.v $*-data.v -n 1024 -t 5 > dump.txt
	./vis dump.txt > $@

%-fpga.ppm: %-code.v %-data.v vis
	LD_LIBRARY_PATH=$(QUARTUS_ROOTDIR)/linux64 hostlink \
    $*-code.v $*-data.v -n 1024 -t 5 > dump.txt
	./vis dump.txt > $@

%-fpga.png: %-fpga.ppm
	convert $< $@

%-fpga.png.64: %-fpga.png
	base64 $< > $@

vis: vis.cpp
	g++ -O2 vis.cpp -o vis

$(INC)/config.h: $(TINSEL_ROOT)/config.py
	make -C $(INC)

.PHONY: clean
clean:
	rm -f *.o *.elf link.ld *.v vis dump.txt *.ppm *.64 *.err
	rm -f 0.c 1.c 2.c 3.c 4.c 5.c 6.c 7.c 8.c 9.c
