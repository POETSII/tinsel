// Tinsel boot loader

#include <tinsel.h>
#include <boot.h>

// See "boot.h" for further details of the supported boot commands.

// Get a 32-bit word from the host and update the checksum
inline uint32_t get(uint32_t *checksum) {
  uint32_t cmd = hostGet();
  *checksum += cmd;
  return cmd;
}

// Main
int main()
{
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
      writeInstr(addrReg, instr);
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
      hostPut(data);
      addrReg += 4;
    }
    else if (cmd == SetAddrCmd) {
      // Set address register
      addrReg = get(&checksum);
    }
    else if (cmd == CacheFlushCmd) {
      // Cache flush and checksum send
      flush();
      hostPut(checksum);
    }
    else if (cmd == StartCmd) {
      // Start all threads running from given PC
      uint32_t pc = get(&checksum);
      for (int i = 1; i < (1 << LogThreadsPerCore); i++)
        threadCreate(i, pc);
      asm volatile("jr %0" : : "r"(pc));
    }
  }

  // Unreachable
  return 0;
}
