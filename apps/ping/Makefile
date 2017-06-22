# Tinsel root
TINSEL_ROOT=../..

include $(TINSEL_ROOT)/globals.mk

# Local compiler flags
CFLAGS = $(RV_CFLAGS) -O2 -I $(INC)
LDFLAGS = -melf32lriscv -G 0 

# HostLink object files
HL_OBJ = $(HL)/DebugLink.o $(HL)/HostLink.o \
         $(HL)/MemFileReader.o $(HL)/UART.o

.PHONY: all
all: code.v data.v

code.v: ping.elf
	$(RV_OBJCOPY) -O verilog --only-section=.text ping.elf code.v

data.v: ping.elf
	$(RV_OBJCOPY) -O verilog --remove-section=.text \
                --set-section-flags .bss=alloc,load,contents ping.elf data.v

ping.elf: ping.c link.ld $(INC)/config.h $(INC)/tinsel.h entry.o
	$(RV_CC) $(CFLAGS) -Wall -c -o ping.o ping.c
	$(RV_LD) $(LDFLAGS) -T link.ld -o ping.elf entry.o ping.o

entry.o:
	$(RV_CC) $(CFLAGS) -Wall -c -o entry.o entry.S

link.ld: genld.sh
	./genld.sh > link.ld

$(INC)/config.h: $(TINSEL_ROOT)/config.py
	make -C $(INC)

$(HL)/%.o:
	make -C $(HL)

run: run.cpp $(HL_OBJ)
	g++ -O2 -I $(INC) -I $(HL) -o run run.cpp $(HL_OBJ)

.PHONY: clean
clean:
	rm -f *.o *.elf link.ld *.v
