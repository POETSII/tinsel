#include <tinsel.h>
#include <io.h>

// Each thread sends a message to N random neighbours
#define N 10

// Address construction
uint32_t toAddr(uint32_t meshX, uint32_t meshY,
             uint32_t coreId, uint32_t threadId)
{
  uint32_t addr;
  addr = meshY;
  addr = (addr << TinselMeshXBits) | meshX;
  addr = (addr << TinselLogCoresPerBoard) | coreId;
  addr = (addr << TinselLogThreadsPerCore) | threadId;
  return addr;
}

// Address deconstruction
void fromAddr(uint32_t addr, uint32_t* meshX, uint32_t* meshY,
         uint32_t* coreId, uint32_t* threadId)
{
  *threadId = addr & ((1 << TinselLogThreadsPerCore) - 1);
  addr >>= TinselLogThreadsPerCore;

  *coreId = addr & ((1 << TinselLogCoresPerBoard) - 1);
  addr >>= TinselLogCoresPerBoard;

  *meshX = addr & ((1 << TinselMeshXBits) - 1);
  addr >>= TinselMeshXBits;

  *meshY = addr;
}

uint32_t min(uint32_t a, uint32_t b)
  { return a < b ? a : b; }

// Psuedo random address
uint32_t randAddr(uint32_t* state)
{
  *state = *state * 1103515245 + 12345;
  uint32_t mask = (1 << (TinselMeshXBits + TinselMeshYBits +
                           TinselLogThreadsPerBoard)) - 1;
  uint32_t addr = *state & mask;
  
  uint32_t x, y, c, t;
  fromAddr(addr, &x, &y, &c, &t);
  return toAddr(min(x, TinselMeshXLenWithinBox-1),
                min(y, TinselMeshYLenWithinBox-1), c, t);
}

int main()
{
  // Get thread id
  uint32_t me = tinselId();

  // Get host id
  uint32_t host = tinselHostId();

  // Get pointers to mailbox message slots
  volatile int* msgOut = tinselSlot(0);
  volatile int* msgIn = tinselSlot(1);

  // Intiialise send slot
  msgOut[0] = me;

  // Allocate receive slot
  tinselAlloc(msgIn);

  // Set number of flits per message
  tinselSetLen(3);

  // Determine random neighbours
  uint32_t seed = me;
  uint32_t neighbours[N];
  for (uint32_t i = 0; i < N; i++)
    neighbours[i] = randAddr(&seed);

  // Track number of messages sent & received
  uint32_t sent = 0;
  uint32_t received = 0;

  // Send messages
  while (1) {
    if (tinselCanSend()) {
      if (sent == N) {
        // Tell host we're done
        tinselSetLen(0);
        tinselSend(host, msgOut);
        sent++;
      }
      else if (sent < N) {
        tinselSend(neighbours[sent], msgOut);
        sent++;
      }
    }
    if (tinselCanRecv()) {
      tinselAlloc(tinselRecv());
      received++;
    }
  }


  return 0;
}
