// Tinsel boot loader

#include <tinsel.h>
#include <boot.h>

// See "boot.h" for further details of the supported boot commands.

// Get a 32-bit word from the host and update the checksum
inline uint32_t get(uint32_t *checksum) {
  uint32_t cmd = tinselHostGet();
  *checksum += cmd;
  return cmd;
}

// Main
int main()
{
  // Global id of this thread
  uint32_t me = tinselId();

  // Core-local thread id
  uint32_t threadId = me & ((1 << TinselLogThreadsPerCore) - 1);

  if (threadId == 0) {
    // State
    uint32_t checksum = 0; // Checksum of requests received
    uint32_t addrReg = 0;  // Address register

    // Command loop
    for (;;) {
      // Receive a command
      uint32_t cmd = get(&checksum);

      // Command dispatch
      // (We avoid using a switch statement here so that the compiler
      // doesn't generate a data section)
      if (cmd == WriteInstrCmd) {
        // Write instruction to instruction memory
        uint32_t instr = get(&checksum);
        tinselWriteInstr(addrReg, instr);
        addrReg += 4;
      }
      else if (cmd == StoreCmd) {
        // Store word to memory
        uint32_t data = get(&checksum);
        uint32_t* ptr = (uint32_t*) addrReg;
        *ptr = data;
        addrReg += 4;
      }
      else if (cmd == LoadCmd) {
        // Load word from memory
        uint32_t* ptr = (uint32_t*) addrReg;
        uint32_t data = *ptr;
        tinselHostPut(data);
        addrReg += 4;
      }
      else if (cmd == SetAddrCmd) {
        // Set address register
        addrReg = get(&checksum);
      }
      else if (cmd == CacheFlushCmd) {
        // Cache flush and checksum send
        tinselCacheFlush();
        tinselHostPut(checksum);
      }
      else if (cmd == StartCmd) {
        uint32_t maxThreads = get(&checksum) - 1;
        // Start threads running
        for (int i = 1; i < (1 << TinselLogThreadsPerCore); i++) {
          if (maxThreads == 0) break;
          maxThreads--;
          tinselCreateThread(i);
        }
        break;
      }
    }
  }

  // Call the application's main function
  int (*appMain)() = (int (*)()) (TinselMaxBootImageBytes);
  appMain();
  for (;;);

  // Unreachable
  return 0;
}
