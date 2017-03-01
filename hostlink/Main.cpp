#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <boot.h>
#include <config.h>
#include <getopt.h>
#include <string.h>
#include <assert.h>
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

// Display printf output on console
void console(HostLink* link)
{
  // One line buffer per thread
  const int maxLineLen = 1024;
  char lineBuffer[TinselThreadsPerBoard][maxLineLen];
  int lineBufferLen[TinselThreadsPerBoard];

  // Initialise
  for (int i = 0; i < TinselThreadsPerBoard; i++)
    lineBufferLen[i] = 0;

  for (;;) {  
    uint32_t src;
    uint32_t val;

    uint8_t cmd = link->get(&src, &val);
    uint32_t id = val >> 8;
    char ch = val & 0xff;
    assert(id < TinselThreadsPerBoard);
    int len = lineBufferLen[id];
    if (ch == '\n' || len >= maxLineLen-1) {
      lineBuffer[id][len] = '\0';
      printf("%d: %s\n", id, lineBuffer[id]);
      lineBufferLen[id] = 0;
    } else {
      lineBuffer[id][len] = ch;
      lineBufferLen[id]++;
    }
  }
}

int usage()
{
  printf("Usage:\n"
         "  hostlink [CODE] [DATA]\n"
         "    -o            start only one thread\n"
         "    -n [NUMBER]   num messages to dump after boot\n"
         "    -t [SECONDS]  timeout on message dump\n"
         "    -c            load console after boot\n"
         "    -h            help\n");
  return -1;
}

int main(int argc, char* argv[])
{
  int startOnlyOneThread = 0;
  int numMessages = -1;
  int numSeconds = -1;
  int useConsole = 0;

  // Option processing
  optind = 1;
  for (;;) {
    int c = getopt(argc, argv, "hon:t:c");
    if (c == -1) break;
    switch (c) {
      case 'h': return usage();
      case 'o': startOnlyOneThread = 1; break;
      case 'n': numMessages = atoi(optarg); break;
      case 't': numSeconds = atoi(optarg); break;
      case 'c': useConsole = 1; break;
      default: return usage();
    }
  }
  if (optind+2 != argc) return usage();

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
  uint32_t coresPerDRAM =
             1 << (TinselLogCoresPerDCache + TinselLogDCachesPerDRAM);
  for (int i = 0; i < TinselDRAMsPerBoard; i++) {
    // Use one core to initialise each DRAM
    link.setDest(coresPerDRAM * i);

    // Write data file to memory
    uint32_t ignore;
    rewind(data);
    sendFile(StoreCmd, &link, data, i == 0 ? &checksum : &ignore);

    // Send cache flush
    link.put(CacheFlushCmd);
    if (i == 0) checksum += CacheFlushCmd;

    // Obtain response and validate checksum
    uint32_t src;
    uint32_t sum;
    link.get(&src, &sum);

    if (sum != checksum) {
      printf("Error: data checksum failure from core %d ", src);
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
  uint32_t numThreads = startOnlyOneThread ? 1 : (1 << TinselLogThreadsPerCore);
  link.put(StartCmd);
  link.put(numThreads);
  checksum += StartCmd + numThreads;
 
  // Step 4: dump
  // ------------

  if (useConsole) console(&link);
  else {
    // The number of tenths of a second that link has been idle
    int idle = 0;

    // Dump messages to terminal
    int count = 0;
    for (;;) {
      if (numMessages >= 0 && count == numMessages) break;
      if (! link.canGet()) {
        usleep(100000);
        idle++;
        if (idle == 10*numSeconds) break;
        continue;
      }
      else {
        idle = 0;
      }
      uint32_t src, val;
      uint8_t cmd = link.get(&src, &val);
      printf("%d %x %x\n", src, cmd, val);
      count++;
    }
  }

  return 0;
}
