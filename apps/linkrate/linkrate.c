// SPDX-License-Identifier: BSD-2-Clause
// Measure the bandwidth of a any given inter-FPGA link

#include <tinsel.h>
#include "linkrate.h"

int main()
{
  // Get thread id
  uint32_t me = tinselId();

  // Get host id
  uint32_t host = tinselHostId();

  // Get pointers to mailbox message slots
  volatile int* msgOut = tinselSendSlot();

  // Intiialise send slot
  msgOut[0] = me;

  // Use single flit messages
  tinselSetLen(0);

  while (1) {
    // Wait for trigger
    // Trigger value contains X and Y coords of destination board (3 bits each)
    uint32_t trigger = 0;
    while ((trigger & 0x100) == 0) trigger = tinselUartTryGet();

    // Determine coordinates of destination board 
    trigger &= 0xff;
    uint32_t active = trigger & 1;
    trigger >>= 2;
    uint32_t destX = trigger & 7;
    uint32_t destY = trigger >> 3;

    // Determine my core id and thread id
    uint32_t myX, myY, myTileX, myTileY, myCore, myThread;
    tinselFromAddr(me, &myX, &myY, &myTileX, &myTileY, &myCore, &myThread);

    // Determine destination thread
    int neighbour =
    tinselToAddr(destX, destY, myTileX, myTileY, myCore, myThread);

    // Block unused threads
    if (active == 0) {
      while (!tinselIdle(0)) {};
    }
    else {
      // Track number of messages received
      uint32_t got = 0;

      // Start timer
      tinselPerfCountReset();

      // Send loop
      for (uint32_t i = 0; i < NumMsgs; i++) {
        while (! tinselCanSend()) {
          while (tinselCanRecv()) {
            volatile int* msgIn = tinselRecv();
            tinselFree(msgIn);
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
          tinselFree(msgIn);
          got++;
        }
      }

      // Wait until all threads done
      while (! tinselIdle(0)) {};

      // How long did we take?
      uint32_t duration = tinselCycleCount();

      // Tell host we're done
      if (myTileX == 0 && myTileY == 0 && myCore == 0 && myThread == 0) {
        tinselWaitUntil(TINSEL_CAN_SEND);
        msgOut[0] = duration;
        tinselSend(host, msgOut);
      }
    }
  }

  return 0;
}
