#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <boot.h>
#include <config.h>
#include <getopt.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include "HostLink.h"

// Send file contents over host-link
uint32_t sendFile(BootCmd cmd, HostLink* link, FILE* fp, uint32_t* checksum, int verbosity)
{
  // Iterate over data file
  uint32_t addr = 0;
  uint32_t value = 0;
  uint32_t byteCount = 0;
  uint32_t byte = 0;
  for (;;) {
    // Send write address
    if (fscanf(fp, " @%x", &addr) > 0) {
      if(verbosity>1){
	fprintf(stderr, "    Writing to address 0x%08x\n", addr);
      }
      link->put(SetAddrCmd);
      link->put(addr);
      *checksum += SetAddrCmd + addr;
    }
    else break;
    // Send write values
    uint32_t currAddr=addr;
    while (fscanf(fp, " %x", &byte) > 0) {
      value = (byte << 24) | (value >> 8);
      byteCount++;
      if (byteCount == 4) {
	if(verbosity>2){
	  fprintf(stderr, "      Write: mem[0x%08x] = 0x%08x\n", currAddr, value);
	}
        link->put(cmd);
	if(verbosity>3){
	  fprintf(stderr, "        wrote command\n");
	}
        link->put(value);
	if(verbosity>3){
	  fprintf(stderr, "        wrote value\n");
	}
        *checksum += cmd + value;
        value = 0;
        byteCount = 0;
	currAddr+=4;
      }
    }
    // Pad & send final word, if necessary
    if (byteCount > 0) {
      while (byteCount < 4) {
        value = value >> 8;
        byteCount++;
      }
      if(verbosity>2){
	fprintf(stderr, "      Write: mem[0x%08x] = 0x%08x\n", currAddr, value);
      }
      link->put(cmd);
      link->put(value);
      *checksum += cmd + value;
      byteCount = 0;
    }
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

extern void protocol(HostLink *link, FILE *keyValDst, FILE *measureDst, int verbosity);

double now()
{
  struct timespec tp;
  clock_gettime(CLOCK_REALTIME, &tp);
  return tp.tv_sec+1e-9*tp.tv_nsec;
}

int usage()
{
  printf("Usage:\n"
         "  hostlink [CODE] [DATA]\n"
         "    -o            start only one thread\n"
         "    -n [NUMBER]   num messages to dump after boot\n"
         "    -t [SECONDS]  timeout on message dump\n"
         "    -c            load console after boot\n"
	 "    -p            load protocol after boot\n"
	 "    -k            (protocol) set destination file for key value output\n"
	 "    -m [CSVFILE]  File to write timing and execution measurements to\n"
	 "    -v            increase verbosity\n"
         "    -h            help\n"
         "\n"
	 "     measurement file is a csv file containing key, index, value, unit tuples\n");
  return -1;
}

int main(int argc, char* argv[])
{
  int startOnlyOneThread = 0;
  int numMessages = -1;
  int numSeconds = -1;
  int useConsole = 0;
  int useProtocol = 0;
  int verbosity = 0;

  FILE *keyValDst = 0;
  FILE *measureDst = 0;

  // Option processing
  optind = 1;
  for (;;) {
    int c = getopt(argc, argv, "hon:t:cpk:m:v");
    if (c == -1) break;
    switch (c) {
      case 'h': return usage();
      case 'o': startOnlyOneThread = 1; break;
      case 'n': numMessages = atoi(optarg); break;
      case 't': numSeconds = atoi(optarg); break;
      case 'c': useConsole = 1; break;
      case 'p': useProtocol = 1; break;
      case 'v': verbosity++; break;
      case 'k':{
        keyValDst=fopen(optarg,"wt");
	if(keyValDst==0){
	  fprintf(stderr, "Error : couldn't open key value file '%s'\n", optarg);
	  exit(1);
	}
	break;
      }
    case 'm':{
        measureDst=fopen(optarg,"wt");
	if(keyValDst==0){
	  fprintf(stderr, "Error : couldn't open measurement file '%s'\n", optarg);
	  exit(1);
	}
	break;
      }
      default: return usage();
    }
  }
  if (optind+2 != argc) return usage();

  if(verbosity>0){
    fprintf(stderr, "Reading code from '%s'\n", argv[optind]);
  }
  // Open code file
  FILE* code = fopen(argv[optind], "r");
  if (code == NULL) {
    printf("Error: can't open file '%s'\n", argv[optind]);
    return -1;
  }

  if(verbosity>0){
    fprintf(stderr, "Reading data from '%s'\n", argv[optind+1]);
  }
  // Open data file
  FILE* data = fopen(argv[optind+1], "r");
  if (data == NULL) {
    printf("Error: can't open file '%s'\n", argv[optind+1]);
    exit(EXIT_FAILURE);
  }

  // State
  HostLink link;
  uint32_t checksum = 0;

  // Step 1: load code into instruction memory
  // -----------------------------------------
  double start=now();
  fseek(code, 0, SEEK_END);
  unsigned codeSize=ftell(code);
  rewind(code);
  if(verbosity>0){  fprintf(stderr, "Loading code into instruction memory, size = %u bytes\n", codeSize);  }

  if (startOnlyOneThread)
    // Write instructions to core 0 only
    link.setDest(0x00000000);
  else
    // Broadcast instructions to all cores
    link.setDest(0x80000000);

  // Write instructions to instruction memory
  uint32_t instrBase = sendFile(WriteInstrCmd, &link, code, &checksum, verbosity);

  double finish=now();
  if(measureDst){
    fprintf(measureDst, "hostlinkLoadInstructions, -, %f, sec\n", finish-start);
    fflush(measureDst);
  }
  start=now();

  // Step 2: initialise memory using data file
  // -----------------------------------------
  fseek(data, 0, SEEK_END);
  unsigned dataSize=ftell(data);
  rewind(data);
  if(verbosity>0){  fprintf(stderr, "Initialise memory using data file, size = %u bytes\n", dataSize);  }

  // Iterate over each DRAM
  uint32_t coresPerDRAM =
             1 << (TinselLogCoresPerDCache + TinselLogDCachesPerDRAM);
  for (int i = 0; i < TinselDRAMsPerBoard; i++) {
    if(verbosity>1){  fprintf(stderr, "  Initialising DRAM %u\n", i);  }
    // Use one core to initialise each DRAM
    link.setDest(coresPerDRAM * i);

    // Write data file to memory
    if(verbosity>1){  fprintf(stderr, "    sending file to memory\n");  }
    uint32_t ignore;
    rewind(data);
    sendFile(StoreCmd, &link, data, i == 0 ? &checksum : &ignore, verbosity);

    // Send cache flush
    if(verbosity>1){  fprintf(stderr, "    sending cache flush\n");  }
    link.put(CacheFlushCmd);
    if (i == 0) checksum += CacheFlushCmd;

    // Obtain response and validate checksum
    if(verbosity>1){  fprintf(stderr, "    obtaining response and validating checksum\n");  }
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

  finish=now();
  if(measureDst){
    fprintf(measureDst, "hostlinkLoadData, -, %f, sec\n", finish-start);
    fflush(measureDst);
  }
  start=now();

  // Step 3: release the cores
  // -------------------------
 if(verbosity>0){  fprintf(stderr, "Releasing the cores\n");  }

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

  finish=now();
  if(measureDst){
    fprintf(measureDst, "hostlinkReleaseCores, -, %f, sec\n", finish-start);
    fflush(measureDst);
  }
  start=now();
 
  // Step 4: dump
  // ------------

  if (useConsole) console(&link);
  else if(useProtocol){
    if(verbosity>0){  fprintf(stderr, "Starting protocol\n");  }
    protocol(&link, keyValDst, measureDst, verbosity);
  }
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
        if (numSeconds > 0 && idle == 10*numSeconds) break;
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
