// SPDX-License-Identifier: BSD-2-Clause
// Tinsel boot loader

#include <tinsel.h>
#include <boot.h>

INLINE void sendb(char c) {
  while (tinselUartTryPut(c) == 0);
}

INLINE int puthex(unsigned x)
{
  int count = 0;

  for (count = 0; count < 8; count++) {
    unsigned nibble = x >> 28;
    sendb(nibble > 9 ? ('a'-10)+nibble : '0'+nibble);
    x = x << 4;
  }
  return count;
}


// Main
int main()
{
  // Global id of this thread
  uint32_t me = tinselId();

  // // Core-local thread id
  uint32_t threadId = me & ((1 << TinselLogThreadsPerCore) - 1);

  // Host id
  uint32_t hostId = tinselHostId();
  volatile BootReq* msgIn;

  // Use one flit per message
  tinselSetLen(0);

  if (threadId == 0) {
    // State
    uint32_t addrReg = 0;  // Address register
    uint32_t lastDataStoreAddr = 0;

    // Get mailbox message slot for sending
    volatile uint32_t* msgOut = tinselSendSlot();

    // Command loop
    for (;;) {
      // Receive message
      tinselWaitUntil(TINSEL_CAN_RECV);
      sendb('m');
      msgIn = tinselRecv();
      uint8_t cmd = msgIn->cmd;

      // tinselWaitUntil(TINSEL_CAN_SEND);
      // tinselSetLen(1);
      // msgOut[0] = 0xAC80; // acks
      // msgOut[1] = cmd;
      // tinselSend(hostId, msgOut);
      // tinselSetLen(0);

      // Command dispatch
      // (We avoid using a switch statement here so that the compiler
      // doesn't generate a data section)

      if (cmd == WriteInstrCmd) {
        // Write instructions to instruction memory
        sendb('i');
        int n = msgIn->numArgs;
        for (int i = 0; i < n; i++) {
          tinselWriteInstr(addrReg, msgIn->args[i]);
          addrReg += 4;
        }
      }
      else if (cmd == StoreCmd) {
        sendb('w');
        // Store words to data memory
        int n = msgIn->numArgs;
        for (int i = 0; i < n; i++) {
          uint32_t* ptr = (uint32_t*) addrReg;
          *ptr = msgIn->args[i];
          lastDataStoreAddr = addrReg;
          addrReg += 4;
        }
      }
      else if (cmd == LoadCmd) {
        // Load words from data memory
        sendb('l');
        int n = msgIn->args[0];
        while (n > 0) {
          int m = n > 4 ? 4 : n;
          tinselWaitUntil(TINSEL_CAN_SEND);
          sendb('s');
          for (int i = 0; i < m; i++) {
            uint32_t* ptr = (uint32_t*) addrReg;
            msgOut[i] = *ptr;
            addrReg += 4;
            n--;
          }
          tinselSend(hostId, msgOut);
        }
      }
      else if (cmd == SetAddrCmd) {
        sendb('a');
        // Set address register
        addrReg = msgIn->args[0];
      }
      else if (cmd == FlushCmd) {
        tinselWaitUntil(TINSEL_CAN_SEND);
        msgOut[0] = msgIn->args[0];
        tinselSend(hostId, msgOut);
      }
      else if (cmd == StartCmd) {
        sendb('s');
        // Cache flush
        tinselCacheFlush();
        // Wait until lines written back, by issuing a load
        if (lastDataStoreAddr != 0) {
          volatile uint32_t* ptr = (uint32_t*) lastDataStoreAddr; ptr[0];
        }
        // Send response
        tinselWaitUntil(TINSEL_CAN_SEND);
        msgOut[0] = tinselId();
        tinselSend(hostId, msgOut);

        // Wait for triggerstartOne
        while ((tinselUartTryGet() & 0x100) == 0);
        sendb('g');
        // Start remaining threads
        int numThreads = msgIn->args[0];
        for (int i = 0; i < numThreads; i++) {
          tinselCreateThread(i+1);
        }
        tinselFree(msgIn);
        break;
      }
      tinselFree(msgIn);
    }
  }
  // Call the application's main function
  int (*appMain)() = (int (*)()) (TinselMaxBootImageBytes);
  sendb('c');
  appMain();

  // Restart boot loader
  if (threadId != 0) tinselKillThread();
  asm volatile("jr zero");

  // Unreachable
  return 0;
}


// int main() {
//   while (tinselUartTryPut('h') == 0);
//   while (tinselUartTryPut('i') == 0);
// }
