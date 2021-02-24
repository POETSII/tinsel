# SPDX-License-Identifier: BSD-2-Clause

# Tinsel root
TINSEL_ROOT ?= ../../..

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
	$(RV_CPPC) $(CFLAGS) -Wall -c -o $(BUILD)/entry.o $(TINSEL_ROOT)/apps/POLite/util/entry.S

$(LIB)/lib.o:
	make -C $(LIB)

$(BUILD)/link.ld: builddir $(TINSEL_ROOT)/apps/POLite/util/genld.sh
	TINSEL_ROOT=$(TINSEL_ROOT) \
    $(TINSEL_ROOT)/apps/POLite/util/genld.sh > $(BUILD)/link.ld

$(INC)/config.h: $(TINSEL_ROOT)/config.py
	make -C $(INC)

$(HL)/%.o:
	make -C $(HL)

$(BUILD)/run: $(RUN_CPP) $(RUN_H) $(HL)/*.o
	g++ -std=c++11 -O0 -I $(INC) -I $(HL) -o $(BUILD)/run $(RUN_CPP) $(HL)/*.o \
	 -march=native -g -O3 -DNDEBUG=1 -lmetis -fno-exceptions -fopenmp \
	 -fno-omit-frame-pointer -ltbb

$(BUILD)/run.debug: $(RUN_CPP) $(RUN_H) $(HL)/*.o
	g++ -std=c++11 -O0 -I $(INC) -I $(HL) -o $(BUILD)/run $(RUN_CPP) $(HL)/*.o \
	 -g -O0 -lmetis -fno-exceptions -fopenmp -pthread \
	 -fno-omit-frame-pointer  -fsanitize=undefined -ltbb \
	 -fsanitize=thread -std=c++17
# -fsanitize=address

$(BUILD)/sim: $(RUN_CPP) $(RUN_H) $(HL)/sim/*.o
	g++ -O2 -I $(INC) -I $(HL) -o $(BUILD)/sim $(RUN_CPP) $(HL)/sim/*.o \
    -g -lmetis

.PHONY: clean
clean:
	rm -rf build
