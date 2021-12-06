# SPDX-License-Identifier: BSD-2-Clause
TINSEL_ROOT = ..

include $(TINSEL_ROOT)/globals.mk

# Local compiler flags
CFLAGS = $(RV_CFLAGS) -O2 -I $(INC)
LDFLAGS = -melf32lriscv -G 0 

.PHONY: all
all: $(patsubst %.S,%.code.v,$(wildcard *.S)) \
     $(patsubst %.S,%.data.v,$(wildcard *.S)) run

%.code.v: %.elf
	$(RV_OBJCOPY) -O verilog --only-section=.text $< $@

%.data.v: %.elf
	$(RV_OBJCOPY) -O verilog --remove-section=.text \
                --set-section-flags .bss=alloc,load,contents $< $@

%.elf: %.o link.ld
	$(RV_LD) $(LDFLAGS) -T link.ld -o $@ $<

%.o: %.S test_macros.h riscv_test.h encoding.h
	$(RV_CC) $(CFLAGS) -Wall -c -o $@ $<

link.ld: genld.sh
	./genld.sh > link.ld

$(HL)/hostlink.a :
	make -C $(HL) hostlink.a

run: run.cpp $(HL)/hostlink.a
	g++ -O2 -I $(INC) -I $(HL) -o run run.cpp $(HL)/hostlink.a

$(HL)/sim/hostlink.a :
	make -C $(HL) sim/hostlink.a

sim: run.cpp $(HL)/sim/hostlink.a
	g++ -O2 -I $(INC) -I $(HL) -o sim run.cpp $(HL)/sim/hostlink.a

.PHONY: clean
clean:
	rm -f *.o *.elf link.ld *.v *.out run sim
