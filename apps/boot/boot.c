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

  if (threadId == 0) {
    // State
    uint32_t addrReg = 0;  // Address register

    // Get mailbox message slot for send and receive
    volatile BootReq* msgIn = tinselSlot(0);
    volatile uint32_t* msgOut = tinselSlot(1);

    // Command loop
    for (;;) {
      // Receive message
      tinselAlloc(msgIn);
      tinselWaitUntil(TINSEL_CAN_RECV);
      msgIn = tinselRecv();

      // Command dispatch
      // (We avoid using a switch statement here so that the compiler
      // doesn't generate a data section)
      uint8_t cmd = msgIn->cmd;
      if (cmd == WriteInstrCmd) {
        // Write instructions to instruction memory
        int n = msgIn->numArgs;
        for (int i = 0; i < n; i++) {
          tinselWriteInstr(addrReg, msgIn->args[i]);
          addrReg += 4;
        }
      }
      else if (cmd == StoreCmd) {
        // Store words to data memory
        int n = msgIn->numArgs;
        for (int i = 0; i < n; i++) {
          uint32_t* ptr = (uint32_t*) addrReg;
          *ptr = msgIn->args[i];
          addrReg += 4;
        }
      }
      else if (cmd == LoadCmd) {
        // Load words from data memory
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
      }
      else if (cmd == StartCmd) {
        // Cache flush
        tinselCacheFlush();
        // Send response
        tinselWaitUntil(TINSEL_CAN_SEND);
        msgOut[0] = me;
        tinselSend(hostId, msgOut);
        // Wait for trigger
        while ((tinselUartTryGet() & 0x100) == 0);
        // Start remaining threads
        int numThreads = msgIn->args[0];
        for (int i = 0; i < numThreads; i++)
          tinselCreateThread(i+1);
        break;
      }
      else if (cmd == PingCmd) {
        tinselWaitUntil(TINSEL_CAN_SEND);
        msgOut[0] = msgIn->args[0]+1;
        tinselSend(hostId, msgOut);
      }
    }
  }

  // Call the application's main function
  int (*appMain)() = (int (*)()) (TinselMaxBootImageBytes);
  appMain();

  // Restart boot loader
  if (threadId != 0) tinselKillThread();
  asm volatile("jr zero");

  // Unreachable
  return 0;
}
