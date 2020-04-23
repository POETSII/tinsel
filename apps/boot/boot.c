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

  // One thread per mailbox initialises receive slots
  uint32_t mboxThreadId = me & ((1 << TinselLogThreadsPerMailbox) - 1);
  if (mboxThreadId == 0) {
    // Reserve two send slots per thread at beginning of mailbox scratchpad
    for (int i = 2<<TinselLogThreadsPerMailbox;
           i < (1<<TinselLogMsgsPerMailbox); i++)
      tinselFree(tinselMailboxSlot(i));
  }

  // Boot loader restarts at this point after main() returns
  int restarted = 0;
  restart:

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
      volatile BootReq* msgIn = tinselRecv();

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
          lastDataStoreAddr = addrReg;
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
        // Wait until lines written back, by issuing a load
        if (lastDataStoreAddr != 0) {
          volatile uint32_t* ptr = (uint32_t*) lastDataStoreAddr; ptr[0];
        }
        // Is extra send slot not requested?
        if (mboxThreadId == 0 && !restarted) {
          if (!msgIn->args[1]) {
            // Use the extra send slot for receiving instead
            for (int i = 1<<TinselLogThreadsPerMailbox;
                   i < (2<<TinselLogThreadsPerMailbox); i++)
              tinselFree(tinselMailboxSlot(i));
          }
        }
        // Send response
        tinselWaitUntil(TINSEL_CAN_SEND);
        msgOut[0] = tinselId();
        tinselSend(hostId, msgOut);
        // Wait for trigger
        while ((tinselUartTryGet() & 0x100) == 0);
        // Start remaining threads
        int numThreads = msgIn->args[0];
        for (int i = 0; i < numThreads; i++)
          tinselCreateThread(i+1);
        tinselFree(msgIn);
        break;
      }
      tinselFree(msgIn);
    }
  }

  // Call the application's main function
  int (*appMain)() = (int (*)()) (TinselMaxBootImageBytes);
  appMain();

  // Restart boot loader
  if (threadId != 0) tinselKillThread();
  restarted = 1;
  goto restart;

  // Unreachable
  return 0;
}
