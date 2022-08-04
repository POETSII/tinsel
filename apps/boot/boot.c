
// SPDX-License-Identifier: BSD-2-Clause
// Tinsel boot loader

#include <tinsel.h>
#include <boot.h>

INLINE void sendb(char c) {
  while (tinselUartTryPut(c) == 0);
}

INLINE int puthex_me(unsigned x)
{
  int count = 0;

  for (count = 0; count < 8; count++) {
    unsigned nibble = x >> 28;
    sendb(nibble > 9 ? ('a'-10)+nibble : '0'+nibble);
    x = x << 4;
  }

  return 8;
}


// INLINE void sendb(char c) {
//   tinselEmit(c);
// }

// Main
int main()
{
  // Global id of this thread
  uint32_t me = tinselId();
  // this is only correct locally to start.
  // we need to wait until debuglink has communicated the FPGA ID to host to get
  // the boardid correct.

  // // Core-local thread id
  uint32_t threadId = me & ((1 << TinselLogThreadsPerCore) - 1);

  // Host id
  uint32_t hostId = tinselHostId();
  volatile BootReq* msgIn;

  // Use one flit per message
  tinselSetLen(0);
  char postflag = 0;

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
      msgIn = tinselRecv();
      uint8_t cmd = msgIn->cmd;
      // sendb('0'+cmd);

      // all of this depends on the tinselId.
      me = tinselId();
      threadId = me & ((1 << TinselLogThreadsPerCore) - 1);
      // if (threadId == 0) puthex_me(me);
      hostId = tinselHostId();
      msgOut = tinselSendSlot();

      // tinselEmit(0x1);
      // puthex_me(me);

      // Command dispatch
      // (We avoid using a switch statement here so that the compiler
      // doesn't generate a data section)

      if (cmd == WriteInstrCmd) {
        // Write instructions to instruction memory
        // tinselEmit(0x1);
        // tinselEmit(addrReg);
        // puthex_me(addrReg);

        int n = msgIn->numArgs;
        for (int i = 0; i < n; i++) {
          tinselWriteInstr(addrReg, msgIn->args[i]);
          addrReg += 4;
        }
        // sendb('c');
      }
      else if (cmd == StoreCmd) {
        // Store words to data memory
        // tinselEmit(0x2);
        // tinselEmit(addrReg);
        // puthex_me(addrReg);

        int n = msgIn->numArgs;
        // tinselEmit(addrReg);
        // *(uint32_t*)addrReg = 5;
        for (int i = 0; i < n; i++) {
          uint32_t* ptr = (uint32_t*) addrReg;
          *ptr = msgIn->args[i];
          lastDataStoreAddr = addrReg;
          addrReg += 4;
        }
        // sendb('d');
      }
      else if (cmd == LoadCmd) {
        // Load words from data memory
        // if (addrReg == 0) {
        tinselCacheFlush();
        // Wait until lines written back, by issuing a load
        volatile uint32_t* ptr = (uint32_t*) lastDataStoreAddr; ptr[0];

        // add a global memory barrier
        uint32_t stackptr;
        asm volatile("addi %0, sp, 0" : "=r"(stackptr));

        // }
        // tinselEmit(0x3);
        // tinselEmit(addrReg);

        // if (!postflag) {
        //   sendb('0');
        //   sendb('x');
        //   puthex_me(me);
        //   postflag = 1;
        // }

        int n = msgIn->args[0];
        while (n > 0) {
          int m = n > 4 ? 4 : n;
          tinselWaitUntil(TINSEL_CAN_SEND);
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
        // Set address register
        addrReg = msgIn->args[0];
        // sendb('a');

      }
      else if (cmd == StackCmd) {
        tinselWaitUntil(TINSEL_CAN_SEND);
        uint32_t stackptr;
        asm volatile("addi %0, sp, 0" : "=r"(stackptr));
        tinselSetLen(1);
        msgOut[0] = stackptr;
        msgOut[1] = me;
        tinselSend(hostId, msgOut);
        tinselSetLen(0);
      }
      else if (cmd == RemoteStackCmd) {
        tinselWaitUntil(TINSEL_CAN_SEND);
        ((BootReq *)msgOut)->cmd = StackCmd;
        tinselSend(msgIn->args[0], msgOut);
      }
      else if (cmd == FlushCmd) {
        tinselWaitUntil(TINSEL_CAN_SEND);
        msgOut[0] = msgIn->args[0];
        tinselSend(hostId, msgOut);
        tinselWaitUntil(TINSEL_CAN_SEND);
      }
      else if (cmd == SendBCmd) {
        sendb(msgIn->args[0]);
      }
      else if (cmd == StartCmd) {
        // Cache flush
        tinselCacheFlush();
        // Wait until lines written back, by issuing a load
        if (lastDataStoreAddr != 0) {
          volatile uint32_t* ptr = (uint32_t*) lastDataStoreAddr; ptr[0];
        } else {
          volatile uint32_t* ptr = (uint32_t*)4; ptr[0];
        }
        // Send response
        tinselWaitUntil(TINSEL_CAN_SEND);
        msgOut[0] = me; //tinselId();
        tinselSend(hostId, msgOut);

        // Wait for triggerstartOne
        while ((tinselUartTryGet() & 0x100) == 0);
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
  // tinselCacheFlush();
  // volatile uint32_t* ptr = (uint32_t*) lastDataStoreAddr; ptr[0];
  int (*appMain)() = (int (*)()) (TinselMaxBootImageBytes);
  appMain();

  // Restart boot loader
  if (threadId != 0) tinselKillThread();
  asm volatile("jr zero");

  // Unreachable
  return 0;
}

// INLINE int puthex_me(unsigned x)
// {
//   int count = 0;
//
//   for (count = 0; count < 8; count++) {
//     unsigned nibble = x >> 28;
//     sendb(nibble > 9 ? ('a'-10)+nibble : '0'+nibble);
//     x = x << 4;
//   }
//
//   return 8;
// }
// //
// // int main() {
// //   // // Core-local thread id
// //   uint32_t me = tinselId();
// //   uint32_t threadId = me & ((1 << TinselLogThreadsPerCore) - 1);
// //
// //   if (threadId == 0) {
// //     for (volatile uint32_t i=0; i<tinselId()*10000; i++) {}
// //     sendb('c'); sendb('z'); puthex_me(tinselId()); sendb('z'); sendb('\n');
// //   }
// //   while (1);
// // }
//
// int main() {
//   // // Core-local thread id
//   uint32_t me = tinselId();
//   uint32_t threadId = me & ((1 << TinselLogThreadsPerCore) - 1);
//   char c, oldc;
//   oldc = '\0';
//
//   if (me == 0) {
//     sendb('c'); sendb('z'); puthex_me(tinselId()); sendb('z'); sendb('\n');
//   }
//
//   while (1) {
//     uint32_t c = 0;
//     while ((c & 0x100) == 0) c = tinselUartTryGet();
//     sendb((char)c);
//   }
//   return 0;
// }
