#include <tinsel.h>
#include "benchmarks.h"

// Size of region for load/store benchmarks
#define SIZE 1024

// Number of messages to send for message-passing benchmarks
#define NUM_MSGS 1024

// Benchmark function
#define BenchmarkFunction int __attribute__ ((noinline))

// Benchmark to compile
#ifndef BENCHMARK
#define BENCHMARK loadLoop
#endif

// One load every 4 instructions
BenchmarkFunction loadLoop()
{
  int* region = (int*) tinselHeapBase();
  volatile int* p = region;
  int sum = 0;
  for (int i = 0; i < SIZE; i++)
    sum += p[i];
  return sum;
}

// One store every 4 instructions
BenchmarkFunction storeLoop()
{
  int* region = (int*) tinselHeapBase();
  volatile int* p = region;
  for (int i = 0; i < SIZE; i++) p[i] = i;
  return 0;
}

// One load&store to same location every 5 instrutions
BenchmarkFunction modifyLoop()
{
  int* region = (int*) tinselHeapBase();
  volatile int* p = region;
  for (int i = 0; i < SIZE; i++) p[i]++;
  return 0;
}

// One load&store to different location every 5 instructions
BenchmarkFunction copyLoop()
{
  int* regionA = (int*) tinselHeapBase();
  int* regionB = ((int*) tinselHeapBase()) + SIZE;
  volatile int* p = regionA;
  volatile int* q = regionB;
  for (int i = 0; i < SIZE; i++) p[i] = q[i];
  return 0;
}

// One load from same line every four instructions
BenchmarkFunction cacheLoop()
{
  int* region = (int*) tinselHeapBase();
  volatile int* p = region;
  int sum = 0;
  for (int i = 0; i < SIZE; i++) {
    int out;
    asm volatile("lw %0, 0(%1)" : "=r"(out): "r"(p));
    sum += out;
  }
  return sum;
}

// One load every 4 instructions from scratchpad
BenchmarkFunction scratchpadLoop()
{
  volatile int* p = tinselSlot(0);
  for (int i = 0; i < SIZE; i++)
    asm volatile("lw a0, 0(%0)\nnop\n" :  : "r"(p) : "a0");
  return 0;
}

// Each pair of threads send messages to each other
BenchmarkFunction messageLoop()
{
  // Get thread id
  int me = tinselId();

  // Get pointer to a mailbox message slot
  volatile int* out = tinselSlot(0);

  // Allocate receive buffer
  for (int i = 0; i < 4; i++) tinselAlloc(tinselSlot(i+1));

  // Track number of messages received & sent
  int received = 0;
  int sent = 0;

  // Determine partner
  int partner;
  partner = (me & 1) ? me - 1 : me + 1;

  // Send & receive loop
  while (sent < NUM_MSGS || received < NUM_MSGS) {
    TinselWakeupCond cond = TINSEL_CAN_RECV |
        (sent < NUM_MSGS ? TINSEL_CAN_SEND : 0);
    tinselWaitUntil(cond);

    if (sent < NUM_MSGS && tinselCanSend())  {
      tinselSend(partner, out);
      sent++;
    }

    if (tinselCanRecv()) {
      tinselAlloc(tinselRecv());
      received++;
    }
  }
  return 0;
}

int main()
{
  // Get id
  int me = tinselId();

  // Threads not being used, return immediately
  if (me >> LogThreadsUsed) return 0;

  // Get host id
  int host = tinselHostId();

  // Response message
  volatile uint32_t* resp = tinselSlot(0);

storeLoop();

  // Benchmark
  uint32_t start = tinselCycleCount();
  BENCHMARK();
  uint32_t stop = tinselCycleCount();
  uint32_t diff = stop > start ? stop - start : start - stop;
  resp[0] = diff;

  // Send time host
  tinselWaitUntil(TINSEL_CAN_SEND);
  tinselSend(host, resp);
  
  return 0;
}
