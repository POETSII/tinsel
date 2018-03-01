// Copyright (c) Matthew Naylor

// PCIeStream Daemon
// =================
//
// Connect UNIX domain socket to FPGA FIFO via PCIeStream.

#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <assert.h>
#include <poll.h>
#include <errno.h>
#include "PowerLink.h"

// Constants
// ---------

// Default socket location
#define SOCKET_NAME "pciestream"

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

// Check if connection is alive
int alive(int sock)
{
  //char buf;
  //int ret = recv(sock, &buf, 1, MSG_PEEK | MSG_DONTWAIT);
  //int closed = ret == 0 || (ret == -1 && errno != EAGAIN);
  char buf;
  return send(sock, &buf, 0, 0) == 0;
}

// Types
// -----

// Return value of transmitter or receiver step function
typedef enum {
  CLOSED,      // Client has closed the connection
  FULL,        // Transmit buffer is full
  EMPTY,       // Receive buffer is empty
  NO_SEND,     // Nothing to send from client
  CLIENT_BUSY, // Client can't currently receive more data
  PROGRESS     // Progress was made
} Status;

// Transmitter state
typedef struct {
  // Access to control/status registers on FPGA side
  volatile uint64_t* csrs;
  // Transmit buffer A
  volatile char* txA;
  // Transmit buffer B
  volatile char* txB;
  // Client connection (socket)
  int client;
  // Which buffer in the double buffer is currently being written to?
  int activeBuffer;
  // Is the active buffer ready to be filled?
  int bufferReady;
  // The number of bytes written to the active buffer but not yet sent
  int pending;
} TxState;

// Receiver state
typedef struct {
  // Access to control/status registers on FPGA side
  volatile uint64_t* csrs;
  // Receive buffer A
  volatile char* rxA;
  // Receive buffer B
  volatile char* rxB;
  // Client connection (socket)
  int client;
  // Which buffer in the double buffer is currently being read from?
  int activeBuffer;
  // Number of bytes written to the client
  int written;
  // The number of bytes in the DMA buffer available for reading
  int available;
} RxState;

// Transmitter
// -----------

// Intialise the transmitter state
void txInit(TxState* s, int client, volatile uint64_t* csrs,
       volatile char* txA, volatile char* txB)
{
  s->client = client;
  s->csrs = csrs;
  s->txA = txA;
  s->txB = txB;
  s->activeBuffer = 0;
  s->bufferReady = 0;
  s->pending = 0;
}

// Read from socket and write to FPGA
Status tx(TxState* s)
{
  // This flag indicates that data should now be sent
  int doSend = 0;

  // Send pending data if buffer is full
  if (s->pending == DMABufferSize) doSend = 1;

  // Send pending data if: (1) pending is a non-zero multiple of
  // 16 and (2) there's no data available on the socket.
  if (s->pending != 0 && (s->pending&0xf) == 0) {
    struct pollfd fd; fd.fd = s->client; fd.events = POLLIN;
    int ret = poll(&fd, 1, 0);
    if (ret <= 0) doSend = 1;
  }

  // Try to read data from client
  if (! doSend) {
    // Determine if DMA buffer is available
    if (! s->bufferReady) {
      if (s->csrs[2*(CSR_LEN_TX_A + s->activeBuffer)] == 0)
        s->bufferReady = 1;
      else
        return FULL;
    }
    // Is there any data to transmit?
    struct pollfd fd; fd.fd = s->client; fd.events = POLLIN;
    int ret = poll(&fd, 1, 0);
    if (ret == 0)
      return NO_SEND;
    else if (ret < 0)
      return CLOSED;
    else {
      // Read data from client
      int n = read(s->client, (void*) &s->txA[s->pending],
                DMABufferSize - s->pending);
      if (n <= 0) return CLOSED;
      s->pending += n;
    }
  }

  // Send pending data, if requested
  if (doSend) {
    // Flush cache
    mfence();
    for (int i = 0; i < s->pending; i += CacheLineBytes) clflush(&s->txA[i]);
    mfence();
    // Trigger send
    assert(s->bufferReady && s->pending >= 16);
    s->csrs[2*(CSR_LEN_TX_A + s->activeBuffer)] = s->pending/16;
    // Switch buffers
    swap(&s->txA, &s->txB);
    s->activeBuffer = (s->activeBuffer+1)&1;
    s->pending = 0;
    s->bufferReady = 0;
  }

  return PROGRESS;
}

// Receiver
// --------

// Initialise the receiver state
void rxInit(RxState* s, int client, volatile uint64_t* csrs,
       volatile char* rxA, volatile char* rxB)
{
  s->client = client;
  s->csrs = csrs;
  s->rxA = rxA;
  s->rxB = rxB;
  s->activeBuffer = 0;
  s->written = 0;
  s->available = 0;
}

// Read from FPGA and write to socket
Status rx(RxState* s) 
{
  // Determine if data is available to receive
  if (s->available == 0) {
    s->available = 16 * s->csrs[2*(CSR_LEN_RX_A + s->activeBuffer)];
    if (s->available == 0)
      return EMPTY;
    // Cache flush
    for (int i = 0; i < s->available; i += CacheLineBytes) clflush(&s->rxA[i]);
    mfence();
  }

  // Can we send data to the client?
  struct pollfd fd; fd.fd = s->client; fd.events = POLLOUT;
  int ret = poll(&fd, 1, 0);
  if (ret == 0)
    return CLIENT_BUSY;
  else if (ret < 0)
    return CLOSED;
  else {
    // Write data to socket
    int n = write(s->client, (void*) &s->rxA[s->written],
              s->available - s->written);
    if (n <= 0) return CLOSED;
    s->written += n;

    // Consume data from FPGA
    if (s->written == s->available) {
      // Make sure all the reads are done before the next write
      mfence();
      // Finished with this buffer
      s->csrs[2*(CSR_LEN_RX_A + s->activeBuffer)] = 0;
      // Switch buffers
      swap(&s->rxA, &s->rxB);
      s->activeBuffer = (s->activeBuffer+1)&1;
      s->available = s->written = 0;
    }
  }

  return PROGRESS;
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
volatile char* openDMABuffer(const char* deviceFile, int prot, uint64_t* addr)
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

// Create listening socket
int createListener()
{
  // Create socket
  int sock = socket(AF_UNIX, SOCK_STREAM, 0);
  if (sock == -1) {
    perror("pciestreamd: socket");
    exit(EXIT_FAILURE);
  }

  // Bind socket
  struct sockaddr_un sockAddr;
  memset(&sockAddr, 0, sizeof(struct sockaddr_un));
  sockAddr.sun_family = AF_UNIX;
  sockAddr.sun_path[0] = '\0';
  strncpy(&sockAddr.sun_path[1], SOCKET_NAME,
    sizeof(sockAddr.sun_path)-2);
  int ret = bind(sock, (const struct sockaddr *) &sockAddr,
                   sizeof(struct sockaddr_un));
  if (ret == -1) {
    perror("pciestreamd: bind");
    exit(EXIT_FAILURE);
  }

  // Listen for connections
  ret = listen(sock, 0);
  if (ret == -1) {
    perror("Control: listen");
    exit(EXIT_FAILURE);
  }

  return sock;
}

int main(int argc, char* argv[])
{
  if (argc != 2) usage();

  uint64_t ctrlBAR;
  if (sscanf(argv[1], "%lx", &ctrlBAR) <= 0) usage();

  // Ignore SIGPIPE
  signal(SIGPIPE, SIG_IGN);

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

  // Create listener socket
  int sock = createListener();

  // Transmitter and receiver state
  TxState txState;
  RxState rxState;

  for (;;) {
    // Accept connection
    int conn = accept(sock, NULL, NULL);
    if (conn == -1) {
      perror("pciestreamd: accept");
      exit(EXIT_FAILURE);
    }

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

    // Reset state
    txInit(&txState, conn, csrs, txA, txB);
    rxInit(&rxState, conn, csrs, rxA, rxB);

    // Event loop
    for (;;) {
      Status txStatus = tx(&txState);
      if (txStatus == CLOSED) break;
      Status rxStatus = rx(&rxState);
      if (rxStatus == CLOSED) break;
      if (txStatus != PROGRESS && rxStatus != PROGRESS) {
        if (! alive(conn)) break;
        usleep(100);
      }
    }

    close(conn);

    // Power down FPGAs
    powerEnable(0);
  }

  return 0;
}
