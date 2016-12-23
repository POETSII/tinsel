#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <boot.h>
#include <config.h>
#include <getopt.h>
#include <string.h>
#include "HostLink.h"

// =============================================================================
// Host Link Boot
// =============================================================================

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

int bootUsage()
{
  printf("Usage:\n"
         "  hostlink boot [CODE].v [DATA].v\n"
         "    -o          start only one thread\n"
         "    -h          help\n");
  return -1;
}

int bootMain(int argc, char* argv[])
{
  int startOnlyOneThread = 0;

  // Option processing
  optind = 2;
  for (;;) {
    int c = getopt(argc, argv, "ho");
    if (c == -1) break;
    switch (c) {
      case 'h': return bootUsage();
      case 'o': startOnlyOneThread = 1; break;
      default: printf("Here %c\n", c); return bootUsage();
    }
  }
  if (optind+2 != argc) return bootUsage();

  // Open code file
  FILE* code = fopen(argv[optind], "r");
  if (code == NULL) {
    printf("Error: can't open file '%s'\n", argv[1]);
    return -1;
  }

  // Open data file
  FILE* data = fopen(argv[optind+1], "r");
  if (data == NULL) {
    printf("Error: can't open file '%s'\n", argv[2]);
    exit(EXIT_FAILURE);
  }

  // State
  HostLink link;
  uint32_t checksum = 0;

  // Step 1: load code into instruction memory
  // -----------------------------------------

  if (startOnlyOneThread)
    // Write instructions to core 0 only
    link.setDest(0x00000000);
  else
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

    if (startOnlyOneThread) break;
  }

  // Step 3: release the cores
  // -------------------------

  if (startOnlyOneThread)
    // Send start command to core 0 only
    link.setDest(0);
  else
    // Broadcast start commands
    link.setDest(0x80000000);

  // Send start command with initial program counter
  uint32_t numThreads = startOnlyOneThread ? 1 : (1 << LogThreadsPerCore);
  link.put(StartCmd);
  link.put(numThreads);
  checksum += StartCmd + numThreads;
 
  return 0;
}

// =============================================================================
// Host Link Dump
// =============================================================================

int dumpUsage()
{
  printf("Usage:\n"
         "  hostlink dump\n"
         "    -n NUM            exit after NUM messages received\n"
         "    -h                help\n");
  return -1;
}

int dumpMain(int argc, char* argv[])
{
  // Number of messages to dump before exiting
  int numMessages = -1;

  // Option processing
  optind = 2;
  for (;;) {
    int c = getopt(argc, argv, "hn:");
    if (c == -1) break;
    switch (c) {
      case 'h': return dumpUsage();
      case 'n': numMessages = atoi(optarg); break;
      default: return dumpUsage();
    }
  }
  if (optind != argc) return dumpUsage();

  // Dump messages to terminal
  HostLink link;
  int count = 0;
  for (;;) {
    if (numMessages >= 0 && count == numMessages) break;
    uint32_t src, val;
    uint8_t cmd = link.get(&src, &val);
    printf("%d %x %x\n", src, cmd, val);
    count++;
  }
 
  return 0;
}

// =============================================================================
// Main
// =============================================================================

int usage()
{
  printf("Usage:\n"
         "  hostlink boot      load application onto tinsel machine\n"
         "  hostlink dump      dump all messages received from tinsel\n");
  return -1;
}

int main(int argc, char* argv[])
{
  if (argc < 2)
    return usage();
  else if (strcmp(argv[1], "boot") == 0)
    return bootMain(argc, argv);
  else if (strcmp(argv[1], "dump") == 0)
    return dumpMain(argc, argv);
  else
    return usage();
}
