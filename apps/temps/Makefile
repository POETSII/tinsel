# SPDX-License-Identifier: BSD-2-Clause
# Tinsel root
TINSEL_ROOT=../..

include $(TINSEL_ROOT)/globals.mk

.PHONY: all
all: run

$(INC)/config.h: $(TINSEL_ROOT)/config.py
	make -C $(INC)

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
	rm -f *.o *.elf link.ld *.v run sim
