# SPDX-License-Identifier: BSD-2-Clause
all: sssp

INC=../../../include

sssp: sssp.cpp
	g++ -I$(INC) -O3 sssp.cpp -o sssp

.PHONY: clean
clean:
	rm sssp
