include ../../../globals.mk

CPPFLAGS += -I include
CPPFLAGS += -I ../../../include
CPPFLAGS += -I ../../../hostlink

CPPFLAGS += -I /home/dt10/local/include

CXXFLAGS += -g -std=c++17 -fopenmp -lpthread -fno-omit-frame-pointer

CXXFLAGS += -O2 -DNDEBUG=1
#CXXFLAGS += -O3 -DNDEBUG=1 -march=native 

# This is needed to stop valgrind crashing. We dont need ultra-high precision
# from boost math anyway
CXXFLAGS += -DBOOST_MATH_NO_LONG_DOUBLE_MATH_FUNCTIONS=1

LDFLAGS += -L /home/dt10/local/lib

LDLIBS += -ltbb -lmetis -lboost_program_options -lquadmath

include src/test_hash_all.mk

HOSTLINK_SRCS := HostLink.cpp SocketUtils.cpp MemFileReader.cpp DebugLink.cpp
HOSTLINK_OBJS := $(patsubst %.cpp, obj/%.o,$(HOSTLINK_SRCS))

EXTRA_OBJS := src/per_program_definitions.o

obj/%.o : ../../../hostlink/%.cpp
	mkdir -p obj
	$(CXX) -c $(CPPFLAGS) $(CXXFLAGS) $< -o $@

bin/% : src/%.o $(HOSTLINK_OBJS) $(EXTRA_OBJS)
	mkdir -p bin
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(filter %.c %.o, $^) -o $@ $(LDFLAGS) $(LDLIBS)

bin/%.riscv.elf bin/%.riscv.code.v bin/%.riscv.data.v : src/%_tinsel.cpp
	src/compile_riscv.sh $< bin/$*-build
	cp bin/$*-build/app.elf bin/$*.riscv.elf
	cp bin/$*-build/code.v bin/$*.riscv.code.v
	cp bin/$*-build/code.v bin/$*.riscv.data.v

bin/hardware_idle_izhikevich_fix	: bin/hardware_idle_izhikevich_fix.riscv.elf


all : bin/test_pair_hash
