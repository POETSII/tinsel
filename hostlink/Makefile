# SPDX-License-Identifier: BSD-2-Clause
TINSEL_ROOT = ..
include $(TINSEL_ROOT)/globals.mk

# Local compiler flags
CPPFLAGS = -I$(INC) -O2 -Wall

# HostLink directory
HL = $(TINSEL_ROOT)/hostlink

.PHONY: all
all: DebugLink.o HostLink.o MemFileReader.o jtag/UART.o pciestreamd \
     sim/DebugLink.o sim/HostLink.o sim/MemFileReader.o sim/UART.o \
     SocketUtils.o sim/SocketUtils.o udsock boardctrld \
     sim/boardctrld fancheck \
	 hostlink.a sim/hostlink.a

hostlink.a : DebugLink.o HostLink.o MemFileReader.o SocketUtils.o
	[ ! -f $@ ] || rm $@
	ar rc $@ $^
	ranlib $@

sim/hostlink.a : sim/DebugLink.o sim/HostLink.o sim/MemFileReader.o sim/SocketUtils.o
	[ ! -f $@ ] || rm $@
	ar rc $@ $^
	ranlib $@

pciestreamd: pciestreamd.cpp
	g++ -Wall -I $(HL) -O2 pciestreamd.cpp -o pciestreamd

boardctrld: boardctrld.cpp PowerLink.o JtagAtlantic.h \
            $(INC)/config.h jtag/UART.h Queue.h jtag/UARTBuffer.h \
            jtag/UART.o SocketUtils.o DebugLinkFormat.h BoardCtrl.h
		[ "$(QUARTUS_ROOTDIR)" != "" ] || { echo "Please set QUARTUS_ROOTDIR to compile boardctrld" ; exit 1 ; } ; \
	g++ -std=c++98 boardctrld.cpp jtag/UART.o PowerLink.o SocketUtils.o \
	  $(CPPFLAGS) -I $(HL) -o boardctrld \
	  -ljtag_atlantic -ljtag_client -L $(QUARTUS_ROOTDIR)/linux64/ \
	  -Wl,-rpath,$(QUARTUS_ROOTDIR)/linux64

sim/boardctrld: boardctrld.cpp sim/PowerLink.o JtagAtlantic.h \
                $(INC)/config.h jtag/UART.h Queue.h jtag/UARTBuffer.h \
                sim/UART.o sim/SocketUtils.o DebugLinkFormat.h BoardCtrl.h \
								SocketUtils.o
	g++ -DSIMULATE -std=c++98 boardctrld.cpp sim/UART.o \
	  SocketUtils.o $(CPPFLAGS) -I $(HL) -o sim/boardctrld

udsock: udsock.c
	gcc -Wall -O2 udsock.c -o udsock

fancheck: fancheck.cpp SocketUtils.o
	g++ -I . -O fancheck.cpp SocketUtils.o -o fancheck

# HostLink dependencies
DEPS = $(INC)/config.h $(INC)/boot.h \
       DebugLink.h HostLink.h MemFileReader.h \
       DebugLinkFormat.h BoardCtrl.h SocketUtils.h

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

$(INC)/config.h: $(TINSEL_ROOT)/config.py
	make -C $(INC)

.PHONY: clean
clean:
	rm -f *.o pciestreamd udsock boardctrld fancheck jtag/*.o hostlink.a
	rm -rf sim

echo: echo.cpp
	g++ $(CPPFLAGS) -std=c++11 -g -Wall -I $(HL) -O2 echo.cpp -ljtag_atlantic -ljtag_client -L $(QUARTUS_ROOTDIR)/linux64/ -Wl,-rpath,$(QUARTUS_ROOTDIR)/linux64 -o echo
