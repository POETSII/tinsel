# SPDX-License-Identifier: BSD-2-Clause
all: asp asp-push GenHypercube GenTree

INC=../../../../include

asp: asp.cpp
	g++ -I$(INC) -O3 asp.cpp -o asp

asp-push: asp-push.cpp
	g++ -I$(INC) -O3 asp-push.cpp -o asp-push

GenHypercube: GenHypercube.hs
	ghc -O2 --make GenHypercube.hs

GenTree: GenTree.hs
	ghc -O2 --make GenTree.hs

clean:
	rm -f asp asp-push GenHypercube GenTree *.hi *.o
