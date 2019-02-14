#include <tinsel.h>

// Each thread sends N messages to a thread on every other board
#define N 1

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

int main()
{
  // Get thread id
  uint32_t me = tinselId();

  // Get host id
  uint32_t host = tinselHostId();

  // Get pointers to mailbox message slots
  volatile int* msgIn = tinselSlot(0);
  volatile int* msgOut = tinselSlot(1);

  // Allocate receive slot
  tinselAlloc(msgIn);

  // Intiialise send slot
  msgOut[0] = me;

  // Track number of messages received
  uint32_t got = 0;

  // Determine my core id and thread id
  uint32_t myX, myY, myCore, myThread;
  fromAddr(me, &myX, &myY, &myCore, &myThread);

  // Send loop
  for (uint32_t i = 0; i < N; i++) {
    for (uint32_t x = 0; x < TinselMeshXLenWithinBox; x++) {
      for (uint32_t y = 0; y < TinselMeshYLenWithinBox; y++) {
        if (x != myX || y != myY) {
          uint32_t dest = toAddr(x, y, myCore, myThread);

          while (! tinselCanSend()) {
            if (tinselCanRecv()) {
              tinselRecv();
              tinselAlloc(msgIn);
              got++;
            }
          }
          tinselSend(dest, msgOut);
        }
      }
    }
  }
  
  // Number of messages expected at each thread
  uint32_t expected =
    ((TinselMeshXLenWithinBox * TinselMeshYLenWithinBox) - 1) * N;

  // Keep receiving
  while (got < expected) {
    if (tinselCanRecv()) {
      tinselRecv();
      tinselAlloc(msgIn);
      got++;
    }
  }

  // Tell host we're done
  tinselWaitUntil(TINSEL_CAN_SEND);
  tinselSend(host, msgOut);

  return 0;
}
