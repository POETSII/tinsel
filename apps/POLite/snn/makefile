CPPFLAGS += -I include
CPPFLAGS += -I ../../../include
CPPFLAGS += -I ../../../hostlink

CXXFLAGS += -g -std=c++17 
CXXFLAGS += -O3 -DNDEBUG=1 -march=native -fopenmp -lpthread

LDLIBS += -ltbb -lmetis

include src/test_hash_all.mk

bin/% : src/%.o
	mkdir -p bin
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -o $@ $(LDFLAGS) $(LDLIBS)

all : bin/test_pair_hash