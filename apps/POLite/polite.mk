# Tinsel root
TINSEL_ROOT=../../..

ifndef QUARTUS_ROOTDIR
  $(error Please set QUARTUS_ROOTDIR)
endif

include $(TINSEL_ROOT)/globals.mk

# Local compiler flags
CFLAGS = $(RV_CFLAGS) -O2 -I $(INC)
LDFLAGS = -melf32lriscv -G 0 

BUILD=build

.PHONY: all
all: $(BUILD)/code.v $(BUILD)/data.v $(BUILD)/run

$(BUILD)/code.v: $(BUILD)/app.elf
	checkelf.sh $(BUILD)/app.elf
	$(RV_OBJCOPY) -O verilog --only-section=.text $(BUILD)/app.elf $(BUILD)/code.v

$(BUILD)/data.v: $(BUILD)/app.elf
	$(RV_OBJCOPY) -O verilog --remove-section=.text \
                --set-section-flags .bss=alloc,load,contents $(BUILD)/app.elf $(BUILD)/data.v

builddir:
	mkdir -p $(BUILD)

$(BUILD)/app.elf: $(APP_CPP) $(APP_HDR) $(BUILD)/link.ld $(INC)/config.h \
          $(INC)/tinsel.h $(BUILD)/entry.o $(LIB)/lib.o
	$(RV_CPPC) $(CFLAGS) -Wall -c -DTINSEL -o $(BUILD)/app.o $(APP_CPP)
	$(RV_LD) $(LDFLAGS) -T $(BUILD)/link.ld -o $(BUILD)/app.elf $(BUILD)/entry.o $(BUILD)/app.o $(LIB)/lib.o

$(BUILD)/entry.o: builddir
	$(RV_CPPC) $(CFLAGS) -Wall -c -o $(BUILD)/entry.o $(TINSEL_ROOT)/apps/util/entry.S

$(LIB)/lib.o:
	make -C $(LIB)

$(BUILD)/link.ld: builddir $(TINSEL_ROOT)/apps/util/genld.sh
	$(TINSEL_ROOT)/apps/util/genld.sh > $(BUILD)/link.ld

$(INC)/config.h: $(TINSEL_ROOT)/config.py
	make -C $(INC)

$(HL)/%.o:
	make -C $(HL)

run: $(RUN_CPP) $(HL)/*.o
	g++ -std=c++11 -O2 -I $(INC) -I $(HL) -o $(BUILD)/run $(RUN_CPP) $(HL)/*.o \
	  -ljtag_atlantic -ljtag_client -L $(QUARTUS_ROOTDIR)/linux64/ \
	  -Wl,-rpath,$(QUARTUS_ROOTDIR)/linux64 -lmetis -fno-exceptions

sim: $(RUN_CPP) $(HL)/sim/*.o
	g++ -O2 -I $(INC) -I $(HL) -o sim $(RUN_CPP) $(HL)/sim/*.o \
    -lmetis

.PHONY: clean
clean:
	rm -r build
