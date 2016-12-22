#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <boot.h>
#include <config.h>
#include "HostLink.h"

// Send file contents over host-link
uint32_t sendFile(BootCmd cmd, HostLink* link, FILE* fp, uint32_t* checksum)
{
  // Iterate over data file
  uint32_t addr = 0;
  uint32_t value = 0;
  uint32_t byteCount = 0;
  uint32_t byte = 0;
  for (;;) {
    // Send write address
    if (fscanf(fp, "@%x", &addr) > 0) {
      link->put(SetAddrCmd);
      link->put(addr);
      *checksum += SetAddrCmd + addr;
      continue;
    }
    // Send write value
    if (fscanf(fp, "%x", &byte) <= 0) break;
    value = (byte << 24) | (value >> 8);
    byteCount++;
    if (byteCount == 4) {
      link->put(cmd);
      link->put(value);
      *checksum += cmd + value;
      value = 0;
      byteCount = 0;
    }
  }
  // Pad & send final word, if necessary
  if (byteCount > 0) {
    while (byteCount < 4) {
      value = value >> 8;
      byteCount++;
    }
    link->put(cmd);
    link->put(value);
    *checksum += cmd + value;
  }

  return addr;
}

int main(int argc, char* argv[])
{
  if (argc != 3) {
    printf("Usage: %s CODE.v DATA.v\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  FILE* code = fopen(argv[1], "r");
  if (code == NULL) {
    printf("Error: can't open file '%s'\n", argv[1]);
    exit(EXIT_FAILURE);
  }

  FILE* data = fopen(argv[2], "r");
  if (code == NULL) {
    printf("Error: can't open file '%s'\n", argv[2]);
    exit(EXIT_FAILURE);
  }

  // State
  HostLink link;
  uint32_t checksum = 0;

  // Step 1: load code into instruction memory
  // -----------------------------------------

  // Broadcast instructions to all cores
  link.setDest(0x80000000);

  // Write instructions to instruction memory
  uint32_t instrBase = sendFile(WriteInstrCmd, &link, code, &checksum);

  // Step 2: initialise memory using data file
  // -----------------------------------------

  // Iterate over each DRAM
  uint32_t coresPerDRAM = 1 << (LogCoresPerDCache + LogDCachesPerDRAM);
  for (int i = 0; i < DRAMsPerBoard; i++) {
    // Use one core to initialise each DRAM
    link.setDest(coresPerDRAM * i);

    // Write data file to memory
    uint32_t ignore;
    sendFile(StoreCmd, &link, data, i == 0 ? &checksum : &ignore);

    // Send cache flush
    link.put(CacheFlushCmd);
    if (i == 0) checksum += CacheFlushCmd;

    // Obtain response and validate checksum
    uint32_t src;
    uint32_t sum;
    link.get(&src, &sum);
    if (sum != checksum) {
      printf("Error: checksum failure from core %d ", src);
      printf("(0x%x v. 0x%x)\n", checksum, sum);
      exit(EXIT_FAILURE);
    }
  }

  // Step 3: release the cores
  // -------------------------

  // Broadcast start commands
  link.setDest(0x80000000);

  // Send start command with initial program counter
  link.put(StartCmd);
  link.put(instrBase);
  checksum += StartCmd + instrBase;
 
  return 0;
}
