# Tinsel root
TINSEL_ROOT=../..

include $(TINSEL_ROOT)/globals.mk

# Local compiler flags
CFLAGS = $(RV_CFLAGS) -Os -I $(INC)
LDFLAGS = -melf32lriscv -G 0 

.PHONY: all
all: $(RTL)/InstrMem.hex $(QP)/InstrMem.mif

$(RTL)/InstrMem.hex: InstrMem.ihex
	code-to-img.sh hex InstrMem.ihex > $(RTL)/InstrMem.hex

$(QP)/InstrMem.mif: InstrMem.ihex
	code-to-img.sh mif InstrMem.ihex > $(QP)/InstrMem.mif

InstrMem.ihex: entry.S boot.c link.ld \
               $(INC)/boot.h $(INC)/config.h $(INC)/tinsel.h
	$(RV_CC) $(CFLAGS) -Wall -c -o boot.o boot.c
	$(RV_CC) $(CFLAGS) -Wall -c -o entry.o entry.S
	$(RV_LD) $(LDFLAGS) -T link.ld -o boot.elf entry.o boot.o
	$(RV_OBJCOPY) --only-section=.text -O ihex boot.elf InstrMem.ihex

link.ld: genld.sh
	./genld.sh > link.ld

$(INC)/config.h: $(TINSEL_ROOT)/config.py
	make -C $(INC)

.PHONY: clean
clean:
	rm -f *.o *.elf link.ld *.ihex
