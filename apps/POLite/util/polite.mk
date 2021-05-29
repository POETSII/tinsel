# SPDX-License-Identifier: BSD-2-Clause

# Tinsel root
TINSEL_ROOT ?= ../../..

include $(TINSEL_ROOT)/globals.mk

# Local compiler flags
CFLAGS = $(RV_CFLAGS) -O2 -I $(INC)
LDFLAGS = -melf32lriscv -G 0 

BUILD=build

.PHONY: all sim hw
all: hw

hw : $(BUILD)/code.v $(BUILD)/data.v $(BUILD)/run

sim : $(BUILD)/sim
sim-release : $(BUILD)/sim-release

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
	$(RV_CPPC) $(CFLAGS) -W -Wall -c -DTINSEL -o $(BUILD)/app.o $(APP_CPP)
	$(RV_LD) $(LDFLAGS) -T $(BUILD)/link.ld -o $(BUILD)/app.elf $(BUILD)/entry.o $(BUILD)/app.o $(LIB)/lib.o

$(BUILD)/entry.o: builddir
	echo "RV_CPPC=$(RV_CPPC)"
	$(RV_CPPC) $(CFLAGS) -W -Wall -c -o $(BUILD)/entry.o $(TINSEL_ROOT)/apps/POLite/util/entry.S

$(LIB)/lib.o:
	make -C $(LIB)

$(BUILD)/link.ld: builddir $(TINSEL_ROOT)/apps/POLite/util/genld.sh
	TINSEL_ROOT=$(TINSEL_ROOT) \
    $(TINSEL_ROOT)/apps/POLite/util/genld.sh > $(BUILD)/link.ld

$(INC)/config.h: $(TINSEL_ROOT)/config.py
	make -C $(INC)

$(HL)/hostlink.a:
	make -C $(HL) hostlink.a

RUN_CPP_OBJ := $(patsubst %.cpp, build/%.run.o, $(RUN_CPP))
SIM_CPP_OBJ := $(patsubst %.cpp, build/%.sim.o, $(RUN_CPP))
SIM_RELEASE_CPP_OBJ := $(patsubst %.cpp, build/%.sim-release.o, $(RUN_CPP))

$(BUILD)/%.run.o : %.cpp
	mkdir -p $(BUILD)
	g++ -c -std=c++11 -I $(INC) -I $(HL) -o $(BUILD)/$*.run.o $*.cpp \
	   -std=c++17 -march=native -g -O3 -DNDEBUG=1 -fno-exceptions -fopenmp \
	   -fno-omit-frame-pointer

$(BUILD)/run: $(RUN_CPP_OBJ) $(RUN_H) $(HL)/hostlink.a
	g++ -std=c++11 -o $(BUILD)/run $(RUN_CPP_OBJ) $(HL)/hostlink.a \
	   -std=c++17 -march=native -g -O3 -DNDEBUG=1 -fno-exceptions -fopenmp \
	   -fno-omit-frame-pointer -ltbb -lmetis


POLITE_SW_SIM_DIR = $(TINSEL_ROOT)/apps/POLite/util/POLiteSWSim

$(BUILD)/%.sim.o : %.cpp
	mkdir -p $(BUILD)
	g++ -std=c++17 -g -c -O0 -I $(POLITE_SW_SIM_DIR)/include -I $(INC) -I $(HL) -o $(BUILD)/$*.sim.o $*.cpp \
		-fopenmp

$(BUILD)/sim: $(SIM_CPP_OBJ) $(RUN_H)
	g++ -std=c++17 -g $(SIM_CPP_OBJ) -o $(BUILD)/sim -g -lmetis -ltbb -fopenmp

$(BUILD)/%.sim-release.o : %.cpp
	mkdir -p $(BUILD)
	g++ -std=c++17 -g -c -O3 -DNDEBUG=1 -I $(POLITE_SW_SIM_DIR)/include -I $(INC) -I $(HL) -o $(BUILD)/$*.sim.o $*.cpp \
		-fopenmp

$(BUILD)/sim-release: $(SIM_RELEASE_CPP_OBJ) $(RUN_H)
	g++ -std=c++17 -g $(SIM_CPP_OBJ) -o $(BUILD)/sim-release -g -lmetis -ltbb -fopenmp

.PHONY: clean
clean:
	rm -rf build
