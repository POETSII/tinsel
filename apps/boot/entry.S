#include <config.h>

entry:

# Get hardware thread id
csrrw a0, 0xf14, zero

#if TinselUseCaches == 1

  # Take bottom log(ThreadsPerDRAM) bits
  li a1, TinselThreadsPerDRAM-1
  and a0, a0, a1

  # Set stack pointer to DRAM_TOP - (id * DRAM partition size) - 32
  sll a0, a0, TinselLogBytesPerDRAMPartition
  la sp, DRAM_TOP
  sub sp, sp, a0
  addi sp, sp, -32

  # Use the partition-interleaved translation
  li a0, 0x80000000
  or sp, sp, a0

#else

  # Set stack pointer to MAILBOX_TOP-4
  la sp, MAILBOX_TOP
  addi sp, sp, -4

#endif

# Jump to main (which we assume will never return)
j main
