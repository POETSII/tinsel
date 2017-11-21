# Tinsel root
TINSEL_ROOT=../../..

ifndef QUARTUS_ROOTDIR
  $(error Please set QUARTUS_ROOTDIR)
endif

include $(TINSEL_ROOT)/globals.mk

# Local compiler flags
CFLAGS = $(RV_CFLAGS) -O2 -I $(INC)
LDFLAGS = -melf32lriscv -G 0 

.PHONY: all
all: code.v data.v run

code.v: Sorter.elf
	checkelf.sh Sorter.elf
	$(RV_OBJCOPY) -O verilog --only-section=.text Sorter.elf code.v

data.v: Sorter.elf
	$(RV_OBJCOPY) -O verilog --remove-section=.text \
                --set-section-flags .bss=alloc,load,contents Sorter.elf data.v

Sorter.elf: Sorter.cpp Sorter.h link.ld $(INC)/config.h \
          $(INC)/tinsel.h entry.o $(LIB)/lib.o
	$(RV_CPPC) $(CFLAGS) -Wall -c -DTINSEL -o Sorter.o Sorter.cpp
	$(RV_LD) $(LDFLAGS) -T link.ld -o Sorter.elf entry.o Sorter.o $(LIB)/lib.o

entry.o:
	$(RV_CPPC) $(CFLAGS) -Wall -c -o entry.o entry.S

$(LIB)/lib.o:
	make -C $(LIB)

link.ld: genld.sh
	./genld.sh > link.ld

$(INC)/config.h: $(TINSEL_ROOT)/config.py
	make -C $(INC)

$(HL)/%.o:
	make -C $(HL)

run: Run.cpp $(HL)/*.o
	g++ -O2 -I $(INC) -I $(HL) -o run Run.cpp $(HL)/*.o \
	  -ljtag_atlantic -ljtag_client -L $(QUARTUS_ROOTDIR)/linux64/ \
	  -Wl,-rpath,$(QUARTUS_ROOTDIR)/linux64 -lmetis

sim: Run.cpp $(HL)/sim/*.o
	g++ -O2 -I $(INC) -I $(HL) -o sim Run.cpp $(HL)/sim/*.o \
    -lmetis

.PHONY: clean
clean:
	rm -f *.o *.elf link.ld *.v run sim
