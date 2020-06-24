# SPDX-License-Identifier: BSD-2-Clause
all: heat

INC=../../../include

heat: heat.cpp
	g++ -I$(INC) -O3 heat.cpp -o heat

.PHONY: clean
clean:
	rm heat
