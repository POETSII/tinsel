# Tinsel root
TINSEL_ROOT=../

include $(TINSEL_ROOT)/globals.mk

# Local compiler flags
CFLAGS = $(RV_CFLAGS) -O2 -I $(INC)

.PHONY: all
all: lib.o

lib.o: io.c $(INC)/tinsel.h $(INC)/config.h
	$(RV_CC) $(CFLAGS) -Wall -c -o lib.o io.c

$(INC)/config.h: $(TINSEL_ROOT)/config.py
	make -C $(INC)

.PHONY: clean
clean:
	rm -f *.o
