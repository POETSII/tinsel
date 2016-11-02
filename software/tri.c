// Compute Nth triangular number, verify result, and output success

#include "tinsel.h"

// Set output bits
inline void set(int bits)
{
  asm volatile("csrs 0x800, %0" : : "r"(bits));
}

// Clear output bits
inline void clear(int bits)
{
  asm volatile("csrc 0x800, %0" : : "r"(bits));
}

// Sleep for a second (approximately)
void sleep() {
  // Assuming 400MHz clock and 16 threads and 4 cycles per iteration
  volatile int delay = 6250000;
  while (delay > 0) delay--;
}

#define N 10

// Main
int main()
{
  int id = me();
  int nums[N];
  int i;
  int sum = 0;

  // Mask off core id
  // (assuming 16 threads per core)
  id = id & 0xf;

  for (i = 0; i < N; i++) nums[i] = i;
  for (i = 0; i < N; i++) sum = sum + nums[i];
  if (sum == 45) set(1 << id); // Put my id on output bits
  while (1);

  return 0;
}
