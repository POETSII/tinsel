#include <config.h>

entry:

# Get hardware thread id
csrr a0, 0xf14

# Take bottom log(ThreadsPerDRAM) bits
li a1, ThreadsPerDRAM-1
and a0, a0, a1

# Set stack pointer to DRAM_TOP - (id * 1M) - 4
sll a0, a0, 20
la sp, DRAM_TOP
sub sp, sp, a0
addi sp, sp, -4

# Jump to main (which we assume will never return)
j main
