all: asp GenHypercube GenTree GenGeoGraph

INC=../../../../include

asp: asp.cpp
	g++ -fopenmp -D_DEFAULT_SOURCE -I$(INC) -O3 asp.cpp -o asp

GenHypercube: GenHypercube.hs
	ghc -O2 --make GenHypercube.hs

GenTree: GenTree.hs
	ghc -O2 --make GenTree.hs

GenGeoGraph: GenGeoGraph.cpp
	g++ -O2 -lstdc++ GenGeoGraph.cpp -o GenGeoGraph

clean:
	rm -f asp GenHypercube GenTree GenGeoGraph *.hi *.o
