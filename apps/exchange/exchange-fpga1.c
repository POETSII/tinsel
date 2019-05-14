// SPDX-License-Identifier: BSD-2-Clause
// Benchmark the inter-FPGA bandwidth
// Each tile sends messages to neighbouring FPGAs

#include <tinsel.h>

// Each thread sends N messages to a thread on each neighbour tile
#define NumMsgs 100000

#define BOARDS_X 3
#define BOARDS_Y 2

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

  tinselSetLen(0);

  // Determine neighbours
  int neighbour = -1;
  if (myX < BOARDS_X && myY < BOARDS_Y) {
    // North
    if (myTileX == 0 && myTileY == 3 && myY < (BOARDS_Y-1))
      neighbour = toAddr(myX, myY+1, 0, 0, myCore, myThread);
    // South
    if (myTileX == 0 && myTileY == 0 && myY > 0)
      neighbour = toAddr(myX, myY-1, 0, 3, myCore, myThread);
    // East
    if (myTileX == 3 && myTileY == 0 && myX < (BOARDS_X-1))
      neighbour = toAddr(myX+1, myY, 0, 1, myCore, myThread);
    // West
    if (myTileX == 0 && myTileY == 1 && myX > 0)
      neighbour = toAddr(myX-1, myY, 3, 0, myCore, myThread);
  }

  // Block unused threads
  if (neighbour == -1) {
    while (!tinselIdle(0)) {};
    while (1);
  }

  uint32_t startTime = tinselCycleCount();

  // Send loop
  for (uint32_t i = 0; i < NumMsgs; i++) {
    while (! tinselCanSend()) {
      while (tinselCanRecv()) {
        volatile int* msgIn = tinselRecv();
        tinselAlloc(msgIn);
        got++;
      }
    }
    tinselSend(neighbour, msgOut);
  }

  // Number of messages expected at each thread
  uint32_t expected = NumMsgs;

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
  if (myTileX == 0 && myTileY == 0 && myThread == 0) {
    tinselWaitUntil(TINSEL_CAN_SEND);
    msgOut[0] = stopTime - startTime;
    tinselSend(host, msgOut);
  }

  return 0;
}
