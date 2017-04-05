#include <tinsel.h>

// Size of region for load/store benchmarks
#define SIZE 1024

// One in every 2^T threads is participating
#define T 0

// Number of messages to send for message-passing benchmarks
#define NUM_MSGS 4096

// One load every 4 instructions
int loadLoop()
{
  int region[SIZE];
  volatile int* p = region;
  int sum = 0;
  for (int i = 0; i < SIZE; i++) sum += p[i];
  return sum;
}

// One store every 4 instructions
void storeLoop()
{
  int region[SIZE];
  volatile int* p = region;
  for (int i = 0; i < SIZE; i++) p[i] = i;
}

// One load&store to same location every 5 instrutions
void modifyLoop()
{
  int region[SIZE]; 
  volatile int* p = region;
  for (int i = 0; i < SIZE; i++) p[i]++;
}

// One load&store to different location every 5 instructions
void copyLoop()
{
  int regionA[SIZE]; 
  int regionB[SIZE]; 
  volatile int* p = regionA;
  volatile int* q = regionB;
  for (int i = 0; i < SIZE; i++) p[i] = q[i];
}

// One load from same line every four instructions
int cacheLoop()
{
  int region[SIZE];
  volatile int* p = region;
  int sum = 0;
  for (int i = 0; i < SIZE; i++) {
    int out;
    asm volatile("lw %0, 0(%1)" : "=r"(out): "r"(p));
    sum += out;
  }
  return sum;
}

// Each pair of threads send messages to each other
void messageLoop()
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
  if (me & 1)
    partner = me & ~1;
  else
    partner = me | 1;

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
}

int main()
{
  int me = tinselId();
  int mask = (1 << T) - 1;
  while ((me & mask) != 0);

  if (me == 0) tinselEmit(0);
  messageLoop();
  if (me == TinselThreadsPerBoard-1) tinselEmit(1);

  return 0;
}
