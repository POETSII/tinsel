// Copyright (c) Matthew Naylor

// PCIeStream Daemon
// =================
//
// Connects named pipes to FPGA over PCIeStream.

#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <assert.h>
#include <sys/ioctl.h>
#include <poll.h>

// Constants
// ---------

// Default pipe locations
#define PIPE_IN       "/tmp/pciestream-in"
#define PIPE_OUT      "/tmp/pciestream-out"
#define PIPE_CTRL_IN  "/tmp/pciestream-ctrl-in"

// Size of each DMA buffer in bytes
#define DMABufferSize 1048576

// Number of bytes per cache line
#define CacheLineBytes 64

// PCIeStream CSRs
#define CSR_ADDR_RX_A 0
#define CSR_ADDR_RX_B 1
#define CSR_ADDR_TX_A 2
#define CSR_ADDR_TX_B 3
#define CSR_LEN_RX_A  4
#define CSR_LEN_RX_B  5
#define CSR_LEN_TX_A  6
#define CSR_LEN_TX_B  7
#define CSR_EN        8
#define CSR_INFLIGHT  10
#define CSR_RESET     11

// Helper functions
// ----------------

// Memory barrier
inline void mfence()
{
  asm volatile("mfence");
}

// Flush cache line
static inline void clflush(volatile void *p)
{
  asm volatile("clflush %0" : "+m" (*(volatile char *) p));
}

// Return minimum of two integers
inline int min(int x, int y)
{
  return x < y ? x : y;
}

// Swap pointers
void swap(volatile char** p, volatile char** q)
{
  volatile char* tmp = *p; *p = *q; *q = tmp;
}

// Transmitter thread
// ------------------

// Read from pipe and write to FPGA
void transmitter(
       volatile uint64_t* csrs,
         volatile char* txA,
           volatile char* txB)
{
  // Get filename of input pipe
  char* filename = getenv("PIPE_IN");
  if (filename == NULL) filename = PIPE_IN;

  // Which buffer in the double buffer is currently being written to?
  int activeBuffer = 0;

  // Is the DMA buffer ready to be filled
  int bufferReady = 0;

  for (;;) {
    // Open pipe
    int pipe = open(filename, O_RDONLY);
    if (pipe == -1) {
      fprintf(stderr, "Error opening input pipe %s\n", filename);
      exit(EXIT_FAILURE);
    }

    // The number of bytes written to DMA buffer but not yet sent
    int pending = 0;

    int reopen = 0;
    while (! reopen) {
      // This flag indicates that data should now be sent
      int doSend = 0;

      // Send pending data if buffer is full
      if (pending == DMABufferSize) doSend = 1;

      // Send pending data if: (1) pending is a non-zero multiple of
      // 16 and (2) there's no data available on the read pipe.
      if (pending != 0 && (pending&0xf) == 0) {
        struct pollfd fd; fd.fd = pipe; fd.events = POLLIN;
        int ret = poll(&fd, 1, 1);
        if (ret <= 0) doSend = 1;
      }

      // Try to read data
      if (! doSend) {
        // Wait until DMA buffer available
        if (! bufferReady) {
          while (csrs[2*(CSR_LEN_TX_A+activeBuffer)] != 0) usleep(100);
          bufferReady = 1;
        }
        // Read data
        int n = read(pipe, (void*) &txA[pending], DMABufferSize-pending);
        if (n <= 0) {
          // On EOF/failure, send pending data and reopen pipe
          if (pending >= 16) doSend = 1;
          close(pipe);
          reopen = 1;
        }
        else {
          pending += n;
        }
      }

      // Send pending data, if requested
      if (doSend) {
        // Flush cache
        mfence();
        for (int i = 0; i < pending; i += CacheLineBytes) clflush(&txA[i]);
        mfence();
        // Trigger send
        assert(bufferReady && pending >= 16);
        csrs[2*(CSR_LEN_TX_A+activeBuffer)] = pending/16;
        // Switch buffers
        swap(&txA, &txB);
        activeBuffer = (activeBuffer+1)&1;
        pending = 0;
        bufferReady = 0;
      }
    }
  }
}

// Receiver thread
// ---------------

// Read from FPGA and write to pipe
void receiver(
       volatile uint64_t* csrs,
         volatile char* rxA,
           volatile char* rxB)
{
  // Get filename of output pipe
  char* filename = getenv("PIPE_OUT");
  if (filename == NULL) filename = PIPE_OUT;

  // Ignore SIGPIPE
  signal(SIGPIPE, SIG_IGN);

  // Which buffer in the double buffer is currently being read from?
  int activeBuffer = 0;

  // Number of bytes written to the pipe
  int written = 0;

  for (;;) {
    // Open pipe
    int pipe = open(filename, O_WRONLY);
    if (pipe == -1) {
      fprintf(stderr, "Error opening output pipe %s\n", filename);
      exit(EXIT_FAILURE);
    }

    // The number of bytes in the DMA buffer available for reading
    int available = 0;

    for (;;) {
      // Wait until data available
      if (available == 0) {
        for (;;) {
          available = 16 * csrs[2*(CSR_LEN_RX_A+activeBuffer)];
          if (available != 0) break;
        }
        // Cache flush
        for (int i = 0; i < available; i += CacheLineBytes) clflush(&rxA[i]);
        mfence();
      }
 
      // Write data to pipe
      int n = write(pipe, (void*) &rxA[written], available-written);
      if (n <= 0) {
        close(pipe);
        break;
      }
      else {
        written += n;
      }

      // Consume data from FPGA
      if (written == available) {
        // Make sure all the reads are done before the next write
        mfence();
        // Finished with this buffer
        csrs[2*(CSR_LEN_RX_A+activeBuffer)] = 0;
        // Switch buffers
        swap(&rxA, &rxB);
        activeBuffer = (activeBuffer+1)&1;
        available = written = 0;
      }
    }
  }
}

// Control thread
// --------------

void control(pid_t pidTransmitter, pid_t pidReceiver)
{
  // Get filenames of control pipes
  char* inFilename = getenv("PIPE_CTRL_IN");
  if (inFilename == NULL) inFilename = PIPE_CTRL_IN;

  for (;;) {
    // Open pipes
    static int pipeIn = -1;
    if (pipeIn == -1) pipeIn = open(inFilename, O_RDONLY);
    if (pipeIn == -1) {
      fprintf(stderr, "Error opening control pipe %s\n", inFilename);
      exit(EXIT_FAILURE);
    }

    for (;;) {
      char cmd;
      int n = read(pipeIn, &cmd, 1);
      if (n <= 0) {
        close(pipeIn);
        pipeIn = -1;
        break;
      }
      else {
        // Kill threads on reset and exit commands
        if (cmd == 'r' || cmd == 'e') {
          kill(pidTransmitter, SIGTERM);
          kill(pidReceiver, SIGTERM);
          int status;
          waitpid(pidTransmitter, &status, 0);
          waitpid(pidReceiver, &status, 0);
        }
        if (cmd == 'r') return;
        else if (cmd == 'e') exit(EXIT_SUCCESS);
      }
    }
  }
}

// Main function
// -------------

// Display usage and quit
void usage()
{
  fprintf(stderr, "Usage: pciestreamd [BAR0]\n"
    "Where BAR0 is a physical address in hex\n");
  exit(EXIT_FAILURE);
}

// Open a DMA buffer
volatile char* openDMABuffer(char* deviceFile, int prot, uint64_t* addr)
{
  int dev = open(deviceFile, O_RDWR);
  if (dev == -1)
  {
    perror("open DMA buffer");
    exit(EXIT_FAILURE);
  }

  if (ioctl(dev, 0, addr) == -1) {
    perror("ioctl DMA buffer");
    exit(EXIT_FAILURE);
  }

  void* ptr =
    mmap(NULL,
         0x100000,
         prot,
         MAP_SHARED,
         dev,
         0);

  if (ptr == MAP_FAILED) {
    perror("mmap DMA buffer");
    exit(EXIT_FAILURE);
  }

  return (volatile char*) ptr;
}

int main(int argc, char* argv[])
{
  if (argc != 2) usage();

  uint64_t ctrlBAR;
  if (sscanf(argv[1], "%lx", &ctrlBAR) <= 0) usage();

  // Obtain access to control BAR
  // ----------------------------

  int memDev = open("/dev/mem", O_RDWR);
  if (memDev == -1)
  {
    perror("open /dev/mem");
    exit(EXIT_FAILURE);
  }

  void *csrsPtr =
    mmap(NULL,
         0x40000,
         PROT_READ | PROT_WRITE,
         MAP_SHARED,
         memDev,
         ctrlBAR);

  if (csrsPtr == MAP_FAILED) {
    perror("mmap csrs");
    exit(EXIT_FAILURE);
  }

  volatile uint64_t* csrs = (uint64_t*) csrsPtr;

  // Obtain access to DMA buffers
  // ----------------------------

  uint64_t addrRxA, addrRxB, addrTxA, addrTxB;

  volatile char* rxA = openDMABuffer("/dev/dmabuffer0", PROT_READ, &addrRxA);
  volatile char* rxB = openDMABuffer("/dev/dmabuffer1", PROT_READ, &addrRxB);
  volatile char* txA = openDMABuffer("/dev/dmabuffer2", PROT_WRITE, &addrTxA);
  volatile char* txB = openDMABuffer("/dev/dmabuffer3", PROT_WRITE, &addrTxB);

  // Main loop
  // ---------

  // Ignore SIGPIPE
  signal(SIGPIPE, SIG_IGN);

  for (;;) {
    // Reset PCIeStream hardware
    csrs[2*CSR_EN] = 0;
    while (csrs[2*CSR_INFLIGHT] != 0);
    csrs[2*CSR_RESET] = 1;
    usleep(100000);
    csrs[2*CSR_ADDR_RX_A] = addrRxA;
    csrs[2*CSR_ADDR_RX_B] = addrRxB;
    csrs[2*CSR_ADDR_TX_A] = addrTxA;
    csrs[2*CSR_ADDR_TX_B] = addrTxB;
    csrs[2*CSR_EN] = 1;

    // Transmitter thread
    pid_t pidTransmitter = fork();
    if (pidTransmitter == 0) transmitter(csrs, txA, txB);

    // Receiver thread
    pid_t pidReceiver = fork();
    if (pidReceiver == 0) receiver(csrs, rxA, rxB);

    // Control thread
    control(pidTransmitter, pidReceiver);
  }

  return 0;
}
