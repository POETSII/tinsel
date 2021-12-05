# SPDX-License-Identifier: BSD-2-Clause
# Tinsel root
TINSEL_ROOT=../..

include $(TINSEL_ROOT)/globals.mk

CPPFLAGS += -I $(INC) -g -W -Wall -Wno-unused-but-set-variable --max-errors=1
CPPFLAGS += -fsanitize=undefined -fsanitize=address

CPPFLAGS += --std=c++17
CPPFLAGS += $(shell pkg-config --cflags libxml++-2.6)

LDLIBS += $(shell pkg-config --libs-only-l libxml++-2.6)

.PHONY: clean
clean:
	# Nothing
