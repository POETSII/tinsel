# Tinsel root
TINSEL_ROOT=../..

ifndef QUARTUS_ROOTDIR
  $(error Please set QUARTUS_ROOTDIR)
endif

include $(TINSEL_ROOT)/globals.mk

# Local compiler flags
CFLAGS = $(RV_CFLAGS) -O2 -I $(INC)
LDFLAGS = -melf32lriscv -G 0 

.PHONY: all
all: benchmarks run

.PHONY: benchmarks
benchmarks: loadLoop-data.v       loadLoop-code.v           \
            storeLoop-data.v      storeLoop-code.v          \
            copyLoop-data.v       copyLoop-code.v           \
            modifyLoop-data.v     modifyLoop-code.v         \
            cacheLoop-data.v      cacheLoop-code.v          \
            scratchpadLoop-data.v scratchpadLoop-code.v     \
            messageLoop-data.v    messageLoop-code.v

%-code.v: %.elf
	checkelf.sh benchmarks.elf
	$(RV_OBJCOPY) -O verilog --only-section=.text $< $@

%-data.v: %.elf
	$(RV_OBJCOPY) -O verilog --remove-section=.text \
    --set-section-flags .bss=alloc,load,contents $< $@

%.elf: benchmarks.c link.ld $(INC)/config.h \
       $(INC)/tinsel.h entry.o $(LIB)/lib.o
	$(RV_CC) $(CFLAGS) -DBENCHMARK=$(basename $@) -Wall -c \
    -o benchmarks.o benchmarks.c
	$(RV_LD) $(LDFLAGS) -T link.ld -o $@ entry.o \
    benchmarks.o $(LIB)/lib.o

entry.o:
	$(RV_CC) $(CFLAGS) -Wall -c -o entry.o entry.S

$(LIB)/lib.o:
	make -C $(LIB)

link.ld: genld.sh
	./genld.sh > link.ld

$(INC)/config.h: $(TINSEL_ROOT)/config.py
	make -C $(INC)

$(HL)/%.o:
	make -C $(HL)

run: run.cpp $(HL)/*.o
	g++ -O2 -I $(INC) -I $(HL) -o run run.cpp $(HL)/*.o \
	  -ljtag_atlantic -ljtag_client -L $(QUARTUS_ROOTDIR)/linux64/ \
	  -Wl,-rpath,$(QUARTUS_ROOTDIR)/linux64

sim: run.cpp $(HL)/sim/*.o
	g++ -O2 -I $(INC) -I $(HL) -o sim run.cpp $(HL)/sim/*.o

.PHONY: clean
clean:
	rm -f *.o *.elf link.ld *.v run sim
