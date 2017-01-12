TINSEL_ROOT = ..

include $(TINSEL_ROOT)/globals.mk

# Local compiler flags
CFLAGS = -march=$(RV_ARCH) -msoft-float -O2 -I $(INC)
LDFLAGS = -melf32lriscv -G 0 

.PHONY: all
all: $(patsubst %.S,%.code.v,$(wildcard *.S)) \
     $(patsubst %.S,%.data.v,$(wildcard *.S))

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

.PHONY: run-sim
run-sim: all
	make -C $(TINSEL_ROOT)/hostlink
	./run.sh sim

.PHONY: run-jtag
run-jtag: all
	make -C $(TINSEL_ROOT)/hostlink
	./run.sh jtag

.PHONY: clean
clean:
	rm -f *.o *.elf link.ld *.v *.out
