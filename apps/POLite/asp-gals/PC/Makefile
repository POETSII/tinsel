all: asp

INC=../../../../include

asp: asp.cpp
	g++ -fopenmp -D_DEFAULT_SOURCE -I$(INC) -O3 asp.cpp -o asp

clean:
	rm -f asp
