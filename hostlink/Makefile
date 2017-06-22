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
all: DebugLink.o HostLink.o MemFileReader.o UART.o

%.o: %.cpp $(DEPS)
	g++ -DSIMULATE -o $@ $(CPPFLAGS) -c $< \

$(INC)/config.h: $(TINSEL_ROOT)/config.py
	make -C $(INC)

.PHONY: clean
clean:
	rm -f *.o
