# SPDX-License-Identifier: BSD-2-Clause
all: asp GenHypercube GenTree

INC=../../../../include

asp: asp.cpp
	g++ -I$(INC) -O3 asp.cpp -o asp

GenHypercube: GenHypercube.hs
	ghc -O2 --make GenHypercube.hs

GenTree: GenTree.hs
	ghc -O2 --make GenTree.hs

clean:
	rm -f asp GenHypercube GenTree *.hi *.o
