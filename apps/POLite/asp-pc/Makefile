all: asp GenHypercube

INC=../../../../include

asp: asp.cpp
	g++ -fopenmp -D_DEFAULT_SOURCE -I$(INC) -O3 asp.cpp -o asp

GenHypercube: GenHypercube.hs
	ghc -O2 --make GenHypercube.hs

clean:
	rm -f asp GenHypercube *.hi *.o
