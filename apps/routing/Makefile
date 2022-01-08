# SPDX-License-Identifier: BSD-2-Clause
# Tinsel root
TINSEL_ROOT=../..

include $(TINSEL_ROOT)/globals.mk

CPPFLAGS += -I $(TINSEL_ROOT)/hostlink
CPPFLAGS += -fopenmp

CPPFLAGS += -I $(INC) -g -W -Wall -Wno-unused-but-set-variable --max-errors=1
#CPPFLAGS += -fsanitize=undefined -fsanitize=address
#CPPFLAGS += -Og
CPPFLAGS += -O3 -DNDEBUG=1 -march=native

CPPFLAGS += --std=c++17
CPPFLAGS += $(shell pkg-config --cflags libxml++-2.6)

LDLIBS += $(shell pkg-config --libs-only-l libxml++-2.6)
LDLIBS += -lmetis -lscotch

obj/%.o : %.cpp
	mkdir -p obj
	$(CXX) -c $(CPPFLAGS) $(LDFLAGS) $< -o $@

bin/% : obj/%.o
	mkdir -p bin
	$(CXX) $(CPPFLAGS) $(LDFLAGS) $< -o $@ $(LDLIBS)



.PHONY: clean
clean:
	# Nothing

tools : $(patsubst %.cpp,bin/%,$(wildcard *.cpp))
