# SPDX-License-Identifier: BSD-2-Clause
TINSEL_ROOT = ..
include $(TINSEL_ROOT)/globals.mk

# Local compiler flags
CPPFLAGS = -I$(INC) -O2 -Wall -g

# HostLink directory
HL = $(TINSEL_ROOT)/hostlink

.PHONY: all
all: DebugLink.o HostLink.o MemFileReader.o jtag/UART.o pciestreamd \
     sim/DebugLink.o sim/HostLink.o sim/MemFileReader.o sim/UART.o \
     SocketUtils.o sim/SocketUtils.o udsock boardctrld \
     sim/boardctrld fancheck \
	 hostlink.a sim/hostlink.a


udsock: udsock.c
	gcc -Wall -O2 udsock.c -o udsock


sim/UART.o: jtag/UART.cpp $(DEPS)
	mkdir -p sim
	g++ -I $(HL) -DSIMULATE -o $@ $(CPPFLAGS) -c $<

sim/%.o: %.cpp $(DEPS)
	mkdir -p sim
	g++ -I $(HL) -DSIMULATE -o $@ $(CPPFLAGS) -c $<

jtag/UART.o: jtag/UART.cpp $(DEPS)
	g++ -I $(HL) -o $@ $(CPPFLAGS) -c $<

%.o: %.cpp $(DEPS)
	g++ -I $(HL) -o $@ $(CPPFLAGS) -c $<

.PHONY: clean
clean:
	rm -f *.o pciestreamd udsock boardctrld fancheck jtag/*.o hostlink.a
	rm -rf sim
