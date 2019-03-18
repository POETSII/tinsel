// Benchmark the inter-tile bandwidth
// Each tile sends messages to neighbouring tiles

#include <tinsel.h>

// Each thread sends N messages to a thread on each neighbour tile
#define NumMsgs 100000

// Address construction
uint32_t toAddr(uint32_t meshX, uint32_t meshY,
                  uint32_t tileX, uint32_t tileY,
                    uint32_t coreId, uint32_t threadId)
{
  uint32_t addr;
  addr = meshY;
  addr = (addr << TinselMeshXBits) | meshX;
  addr = (addr << TinselMailboxMeshYBits) | tileY;
  addr = (addr << TinselMailboxMeshXBits) | tileX;
  addr = (addr << TinselLogCoresPerMailbox) | coreId;
  addr = (addr << TinselLogThreadsPerCore) | threadId;
  return addr;
}

// Address deconstruction
void fromAddr(uint32_t addr, uint32_t* meshX, uint32_t* meshY,
                uint32_t* tileX, uint32_t* tileY,
                  uint32_t* coreId, uint32_t* threadId)
{
  *threadId = addr & ((1 << TinselLogThreadsPerCore) - 1);
  addr >>= TinselLogThreadsPerCore;

  *coreId = addr & ((1 << TinselLogCoresPerMailbox) - 1);
  addr >>= TinselLogCoresPerMailbox;

  *tileX = addr & ((1 << TinselMailboxMeshXBits) - 1);
  addr >>= TinselMailboxMeshXBits;

  *tileY = addr & ((1 << TinselMailboxMeshYBits) - 1);
  addr >>= TinselMailboxMeshYBits;

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
  volatile int* msgOut = tinselSlot(0);

  // Allocate receive slots
  for (int i = 1; i < 16; i++)
    tinselAlloc(tinselSlot(i));

  // Intiialise send slot
  msgOut[0] = me;

  // Track number of messages received
  uint32_t got = 0;

  // Determine my core id and thread id
  uint32_t myX, myY, myTileX, myTileY, myCore, myThread;
  fromAddr(me, &myX, &myY, &myTileX, &myTileY, &myCore, &myThread);

  // Block unused threads
  if (myX != 0 || myY != 0) {
    while (!tinselIdle(0)) {};
    while (1);
  }

  //tinselSetLen(3);

  // Determine neighbours
  int neighbours[4];
  int numNeighbours = 0;
  if (myTileX > 0) {
    neighbours[numNeighbours++] = 
      toAddr(myX, myY, myTileX-1, myTileY, myCore, myThread);
  }
  if (myTileX < (TinselMailboxMeshXLen-1)) {
    neighbours[numNeighbours++] = 
      toAddr(myX, myY, myTileX+1, myTileY, myCore, myThread);
  }
  if (myTileY > 0) {
    neighbours[numNeighbours++] = 
      toAddr(myX, myY, myTileX, myTileY-1, myCore, myThread);
  }
  if (myTileY < (TinselMailboxMeshYLen-1)) {
    neighbours[numNeighbours++] = 
      toAddr(myX, myY, myTileX, myTileY+1, myCore, myThread);
  }

  uint32_t startTime = tinselCycleCount();

  // Send loop
  for (uint32_t i = 0; i < NumMsgs; i++) {
    for (uint32_t i = 0; i < numNeighbours; i++) {
      while (! tinselCanSend()) {
        while (tinselCanRecv()) {
          volatile int* msgIn = tinselRecv();
          tinselAlloc(msgIn);
          got++;
        }
      }
      tinselSend(neighbours[i], msgOut);
    }
  }

  // Number of messages expected at each thread
  uint32_t expected = numNeighbours * NumMsgs;

  // Keep receiving
  while (got < expected) {
    if (tinselCanRecv()) {
      volatile int* msgIn = tinselRecv();
      tinselAlloc(msgIn);
      got++;
    }
  }

  // Wait until all threads done
  while (! tinselIdle(0)) {};

  uint32_t stopTime = tinselCycleCount();

  // Tell host we're done
  if (me == 0) {
    tinselWaitUntil(TINSEL_CAN_SEND);
    msgOut[0] = stopTime - startTime;
    tinselSend(host, msgOut);
  }

  return 0;
}
