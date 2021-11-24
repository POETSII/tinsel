// SPDX-License-Identifier: BSD-2-Clause
// Tinsel boot loader

#include <tinsel.h>
#include <boot.h>

// See "boot.h" for further details of the supported boot commands.

// Main
int main()
{
  // Global id of this thread
  uint32_t me = tinselId();

  // Core-local thread id
  uint32_t threadId = me & ((1 << TinselLogThreadsPerCore) - 1);

  // Host id
  uint32_t hostId = tinselHostId();

  // Use one flit per message
  tinselSetLen(0);

  if (threadId == 0) {
    // Get mailbox message slot for sending
    volatile uint32_t* msgOut = tinselSendSlot();

    // Command loop
    for (;;) {
      int c = tinselUartTryGet();
      if ((c & 0x100) != 0) {
        tinselWaitUntil(TINSEL_CAN_SEND);
        msgOut[0] = c & 0xff;
        tinselSend(64, msgOut);
      }

      if (tinselCanRecv()) {
        volatile uint32_t* msgIn = tinselRecv();
        while (tinselUartTryPut(msgIn[0]) == 0);
        tinselFree(msgIn);
      }
    }
  }

  while (1);

  // Unreachable
  return 0;
}
