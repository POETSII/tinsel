TINSEL_ROOT = ..
include $(TINSEL_ROOT)/globals.mk

ifndef QUARTUS_ROOTDIR
  $(error Please set QUARTUS_ROOTDIR)
endif

# Local compiler flags
CPPFLAGS = -I$(INC) -O2

# Top-level dependencies
DEPS = $(INC)/config.h $(INC)/boot.h \
       DebugLink.h HostLink.h JtagAtlantic.h MemFileReader.h UART.h

.PHONY: all
all: DebugLink.o HostLink.o MemFileReader.o UART.o pciestreamd \
     sim/DebugLink.o sim/HostLink.o sim/MemFileReader.o sim/UART.o \
     udsock tinsel-power

pciestreamd: pciestreamd.cpp PowerLink.o
	g++ -O2 pciestreamd.cpp PowerLink.o -o pciestreamd

udsock: udsock.c
	gcc -O2 udsock.c -o udsock

tinsel-power: tinsel-power.cpp PowerLink.o
	g++ $(CPPFLAGS) tinsel-power.cpp PowerLink.o -o tinsel-power

sim/%.o: %.cpp $(DEPS)
	mkdir -p sim
	g++ -DSIMULATE -o $@ $(CPPFLAGS) -c $<

%.o: %.cpp $(DEPS)
	g++ -o $@ $(CPPFLAGS) -c $<

$(INC)/config.h: $(TINSEL_ROOT)/config.py
	make -C $(INC)

.PHONY: clean
clean:
	rm -f *.o pciestreamd udsock tinsel-power
	rm -rf sim
