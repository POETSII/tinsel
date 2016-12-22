# Tinsel root
TINSEL_ROOT=../..

include $(TINSEL_ROOT)/globals.mk

ifndef QUARTUS_ROOTDIR
  $(error Please set QUARTUS_ROOTDIR)
endif

# Local compiler flags
CPPFLAGS = -I $(INC) -O

# Top-level dependencies
DEPS = Boot.cpp $(INC)/config.h $(INC)/boot.h \
       HostLink.h JtagAtlantic.h RawLink.h

.PHONY: all
all: $(BIN)/boot $(BIN)/simboot $(BIN)/term $(BIN)/simterm

$(BIN)/boot: $(DEPS)
	g++ Boot.cpp -o $(BIN)/boot \
    $(CPPFLAGS) \
    -ljtag_atlantic -ljtag_client \
    -L $(QUARTUS_ROOTDIR)/linux64/

$(BIN)/simboot: $(DEPS)
	g++ -DSIMULATE Boot.cpp -o $(BIN)/simboot\
    $(CPPFLAGS)

$(BIN)/term: $(DEPS)
	g++ Term.cpp -o $(BIN)/term \
    $(CPPFLAGS) \
    -ljtag_atlantic -ljtag_client \
    -L $(QUARTUS_ROOTDIR)/linux64/

$(BIN)/simterm: $(DEPS)
	g++ -DSIMULATE Term.cpp -o $(BIN)/simterm\
    $(CPPFLAGS)

$(INC)/config.h: $(TINSEL_ROOT)/config.py
	make -C $(INC)

.PHONY: clean
clean:
	rm -f *.o
