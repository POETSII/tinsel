// SPDX-License-Identifier: BSD-2-Clause
// Benchmark the inter-FPGA bandwidth

#include <tinsel.h>

// Each thread sends N messages to a thread on every other board
#define NumMsgs 100000

// The size of the FPGA mesh
#define MESH_X 2
#define MESH_Y 1

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
  volatile int* msgOut = tinselSlot(0);

  // Allocate receive slots
  for (int i = 1; i < 8; i++)
    tinselAlloc(tinselSlot(i));

  // Intiialise send slot
  msgOut[0] = me;

  // Track number of messages received
  uint32_t got = 0;

  // Determine my core id and thread id
  uint32_t myX, myY, myCore, myThread;
  fromAddr(me, &myX, &myY, &myCore, &myThread);

  // Block unused threads
  if (myX >= MESH_X || myY >= MESH_Y) {
    while (!tinselIdle(0)) {};
    while (1);
  }


  uint32_t startTime = tinselCycleCount();

  // Send loop
  for (uint32_t i = 0; i < NumMsgs; i++) {
    for (uint32_t x = 0; x < MESH_X; x++) {
      for (uint32_t y = 0; y < MESH_Y; y++) {
        if (x != myX || y != myY) {
          uint32_t dest = toAddr(x, y, myCore, myThread);

          while (! tinselCanSend()) {
            if (tinselCanRecv()) {
              volatile int* msgIn = tinselRecv();
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
  uint32_t expected = ((MESH_X * MESH_Y) - 1) * NumMsgs;

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
