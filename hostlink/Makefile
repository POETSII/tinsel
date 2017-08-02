TINSEL_ROOT = ..
include $(TINSEL_ROOT)/globals.mk

ifndef QUARTUS_ROOTDIR
  $(error Please set QUARTUS_ROOTDIR)
endif

# Local compiler flags
CPPFLAGS = -I $(INC) -O
CPPFLAGS += -g
CPPFLAGS += -O3

# Top-level dependencies
DEPS = Main.cpp Protocol.cpp $(INC)/config.h $(INC)/boot.h \
       HostLink.h JtagAtlantic.h RawLink.h

.PHONY: all
all: $(BIN)/hostlink $(BIN)/hostlink-sim

$(BIN)/hostlink: $(DEPS)
	g++ Main.cpp Protocol.cpp -o $(BIN)/hostlink \
    $(CPPFLAGS) \
    -ljtag_atlantic -ljtag_client \
    -L $(QUARTUS_ROOTDIR)/linux64/

$(BIN)/hostlink-sim: $(DEPS)
	g++ -DSIMULATE Main.cpp Protocol.cpp -o $(BIN)/hostlink-sim\
    $(CPPFLAGS)

$(INC)/config.h: $(TINSEL_ROOT)/config.py
	make -C $(INC)

.PHONY: clean
clean:
	rm -f *.o
