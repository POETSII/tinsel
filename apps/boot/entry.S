#include <config.h>

entry:

# Take bottom log(ThreadsPerDRAM) bits of hardware thread id
li a1, TinselThreadsPerDRAM-1
csrrw a0, 0xf14, zero
and a0, a0, a1

# Set stack pointer to DRAM_TOP - (id * DRAM partition size) - 32
sll a0, a0, TinselLogBytesPerDRAMPartition
la sp, DRAM_TOP
sub sp, sp, a0
addi sp, sp, -32

# Use the partition-interleaved translation
li a0, 0x80000000
or sp, sp, a0

# Jump to main (which we assume will never return)
j main
