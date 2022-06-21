// SPDX-License-Identifier: BSD-2-Clause
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

#define max(a, b) (a>b? a : b)
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
#define CSR_MAGIC     12
#define CSR_FPGAID    13

// Helper functions
// ----------------

// Memory barrier
inline void mfence()
{
  asm volatile("mfence");
}

void show_csrs(volatile uint64_t* csrs) {
  uint64_t enable = csrs[2*CSR_EN];
  uint32_t outBufferLen = enable & (1<<7-1);
  uint32_t inBufferLen = (enable >> 7) & (1<<4-1);
  uint32_t txTotal = (enable >> 11) & (1<<32-1);
  bool enabled = (enable >> 43) & 1;

  printf("CSR values, as read:"
          "\nCSR_ADDR_RX_A 0x%08lX"
          "\nCSR_ADDR_RX_B 0x%08lX"
          "\nCSR_ADDR_TX_A 0x%08lX"
          "\nCSR_ADDR_TX_B 0x%08lX"
          "\nCSR_LEN_RX_A  0x%08lX"
          "\nCSR_LEN_RX_B  0x%08lX"
          "\nCSR_LEN_TX_A  0x%08lX"
          "\nCSR_LEN_TX_B  0x%08lX"
          "\nCSR_EN outBufferLen %i inBufferLen %i txtotal %i enabled %i (0x%08lX)"
          "\nCSR_INFLIGHT  0x%08lX"
          "\nCSR_RESET     0x%08lX"
          "\nCSR_MAGIC     0x%08lX\n"
          , csrs[2*CSR_ADDR_RX_A]
          , csrs[2*CSR_ADDR_RX_B]
          , csrs[2*CSR_ADDR_TX_A]
          , csrs[2*CSR_ADDR_TX_B]
          , csrs[2*CSR_LEN_RX_A ]
          , csrs[2*CSR_LEN_RX_B ]
          , csrs[2*CSR_LEN_TX_A ]
          , csrs[2*CSR_LEN_TX_B ]
          , outBufferLen, inBufferLen, enabled, txTotal, enable
          , csrs[2*CSR_INFLIGHT ]
          , csrs[2*CSR_RESET    ]
          , csrs[2*CSR_MAGIC    ]);
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
  char buf = 0;
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
  long bytessent;
  long bytesread;
} TxState;

void show_txstate(TxState tx) {
  struct pollfd fd; fd.fd = tx.client; fd.events = POLLIN;
  int ret = poll(&fd, 1, 0);

  printf("Tx state:"
         "\nactiveBuffer %i"
         "\nbufferReady %i"
         "\npending %i\n"
         "\nsocket ready to read? %i\n"
         "\nsent %li out of %li recieved bytes\n",
         tx.activeBuffer, tx.bufferReady, tx.pending,
         ret,
         tx.bytessent, tx.bytesread);
}
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
  s->bytessent=0; s->bytesread=0;
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
    if (ret > 0) printf("inhibit send to read from socket.\n");
  }

  if (s->pending != 0 && (s->pending&0xf) != 0) {
    // printf("have pending data, but inhibiting to wait for mul of 16 bytes (have %i).\n", s->pending);
  }

  // Try to read data from client
  if (! doSend) {
    // Determine if DMA buffer is available
    if (! s->bufferReady) {
      if (s->csrs[2*(CSR_LEN_TX_A + s->activeBuffer)] == 0) {
        s->bufferReady = 1;
      } else {
        return FULL;
      }
    }
    // Is there any data to transmit?
    struct pollfd fd; fd.fd = s->client; fd.events = POLLIN;
    int ret = poll(&fd, 1, 0);
    if (ret == 0) {
      return NO_SEND;
    } else if (ret < 0) {
      return CLOSED;
    } else {
      // Read data from client
      int n = read(s->client, (void*) &s->txA[s->pending],
                DMABufferSize - s->pending);
      if (n <= 0) return CLOSED;
      s->pending += n;
      s->bytesread += n;
    }
  }

  // Send pending data, if requested
  if (doSend) {
    // printf("attempting to send\n");
    // Flush cache
    mfence();
    for (int i = 0; i < s->pending; i += CacheLineBytes) clflush(&s->txA[i]);
    mfence();
    // Trigger send
    assert(s->bufferReady && s->pending >= 16);
    s->csrs[2*(CSR_LEN_TX_A + s->activeBuffer)] = s->pending/16;
    // printf("set pending (addr 0x%08X) to %u\n", 2*(CSR_LEN_TX_A + s->activeBuffer), s->pending/16);
    // printf("Contents of active buffer\n");
    // for (int i=0; i< (s->pending>64? 64 : s->pending); i++) {
    //   printf("%c0x%04X", (i % 16 == 0) ? '\n' : ' ',
    //                      s->activeBuffer == 0 ? s->txA[i] : s->txB[i]);
    // }
    // Switch buffers
    swap(&s->txA, &s->txB);
    s->activeBuffer = (s->activeBuffer+1)&1;
    s->bytessent += s->pending;
    s->pending = 0;
    s->bufferReady = 0;
    printf("sent\n");
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
    uint64_t inflight = s->csrs[2*CSR_INFLIGHT];
    // printf("got pending (addr 0x%08X) = %u (inflight status 0x%02lX)\n", 2*(CSR_LEN_RX_A + s->activeBuffer), s->available, inflight);
    if (s->available == 0)
      return EMPTY;
    // Cache flush
    // printf("available != 0; time to rx line from device\n");
    for (int i = 0; i < s->available; i += CacheLineBytes) clflush(&s->rxA[i]);
    mfence();
  }

  // printf("data is available to receive.\n");
  // show_csrs(s->csrs);
  // printf("Contents of A\n");
  // for (int i=0; i< (s->available>64? 64 : s->available); i++) { printf("%c0x%04X", (i % 16 == 0) ? '\n' : ' ', s->rxA[i]); }
  // printf("\n");
  // printf("Contents of B\n");
  // for (int i=0; i< (s->available>64? 64 : s->available); i++) { printf("%c0x%04X", (i % 16 == 0) ? '\n' : ' ', s->rxB[i]); }
  // printf("\n");

  // Can we send data to the client?
  struct pollfd fd; fd.fd = s->client; fd.events = POLLOUT;
  int ret = poll(&fd, 1, 0);
  if (ret == 0) {
    printf("rx failed, client busy\n");
    return CLIENT_BUSY;
  } else if (ret < 0) {
    printf("rx failed, client closed\n");
    return CLOSED;
  } else {
    // Write data to socket
    // printf("writing to socket\n");
    int n = write(s->client, (void*) &s->rxA[s->written],
              s->available - s->written);
    if (n <= 0) {
      printf("rx failed, client closed\n");
      return CLOSED;
    }
    // printf("rx written %i bytes\n", n);
    s->written += n;

    // Consume data from FPGA
    if (s->written == s->available) {
      // Make sure all the reads are done before the next write
      mfence();
      // Finished with this buffer
      s->csrs[2*(CSR_LEN_RX_A + s->activeBuffer)] = 0;
      // printf("finished with buffer (addr 0x%08X) = %lu\n", 2*(CSR_LEN_RX_A + s->activeBuffer), s->csrs[2*(CSR_LEN_RX_A + s->activeBuffer)]);
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
  printf("[pciestreamd::main] pciestream opened buffers (RxA 0x%lx RxB 0x%lx TxA 0x%lx TxB 0x%lx)\n", addrRxA, addrRxB, addrTxA, addrTxB);

  // Main loop
  // ---------

  // Create listener socket
  int sock = createListener();
  printf("[pciestreamd::main] pciestream created listener socket.\n");
  uint64_t magic = csrs[2*CSR_MAGIC];
  printf("[pciestreamd::main] pciestream magic = 0x%08lX\n", magic);
  if (magic != 0xCAFECAFE) exit(1);

  uint64_t fpga_id = 0;

  // Transmitter and receiver state
  TxState txState;
  RxState rxState;

  for (;;) {
    uint64_t inflight = csrs[2*CSR_INFLIGHT];
    int count = 0;
    // Reset and disable PCIeStream hardware
    printf("[pciestreamd::main] sending hardware reset\n");
    csrs[2*CSR_EN] = 0;
    printf("[pciestreamd::main] waiting for ack\n");

    while (inflight != 0) {
      printf("[pciestreamd::main] inflight = %lu\n", inflight);
      inflight = csrs[2*CSR_INFLIGHT];
      usleep(500000);
      count++;
      if (count > 5) exit(2);
    } // returns all ff...

    if (inflight != 0) {
      printf("[pciestreamd::main] inflight = %lu and reset disabled; exiting.\n", inflight);
      exit(3);
    }

    csrs[2*CSR_ADDR_RX_A] = 0xF00DF00D;
    if (csrs[2*CSR_ADDR_RX_A] != 0xF00DF00D) {
      printf("[pciestreamd::main] PCIeStream hardware did not respond to writes; exiting.\n");
      show_csrs(csrs);
      exit(1);
    }

    uint64_t magic = csrs[2*CSR_MAGIC];
    printf("[pciestreamd::main] pciestream magic = 0x%08lX\n", magic);
    if (magic != 0xCAFECAFE) exit(1);

    while (fpga_id == 0) {
      fpga_id = csrs[2*CSR_FPGAID];
      usleep(10000);
    }
    printf("[pciestreamd::main] pciestream board 0 fpgaid = 0x%08lX\n", fpga_id);

    csrs[2*CSR_RESET] = 1;
    printf("[pciestreamd::main] reset PCIeStream hardware\n");
    inflight = csrs[2*CSR_INFLIGHT];
    if (inflight != 0) {
      printf("[pciestreamd::main] inflight = %lu after attempted reset; exiting.\n", inflight);
      exit(3);
    }

    magic = csrs[2*CSR_MAGIC];
    printf("[pciestreamd::main] pciestream magic = 0x%08lX\n", magic);
    if (magic != 0xCAFECAFE) exit(1);
    usleep(500000);

    // Accept connection
    int conn = accept(sock, NULL, NULL);
    if (conn == -1) {
      perror("[pciestreamd::main] pciestreamd: accept");
      exit(EXIT_FAILURE);
    }
    printf("[pciestreamd::main] pciestreamd accepted connection\n");

    // Reset and enable PCIeStream hardware
    printf("[pciestreamd::main] sending hardware reset\n");
    csrs[2*CSR_EN] = 0;
    inflight = csrs[2*CSR_INFLIGHT];

    while (inflight != 0) { printf("[pciestreamd::main] inflight = %lu\n", inflight); inflight = csrs[2*CSR_INFLIGHT]; usleep(500000); count++; if (count > 5) exit(2); } // returns all ff...

    inflight = csrs[2*CSR_INFLIGHT];
    if (inflight != 0) {
      printf("[pciestreamd::main] inflight = %lu and reset disabled; exiting.\n", inflight);
      exit(3);
    }

    // csrs[2*CSR_RESET] = 1;
    printf("[pciestreamd::main] enabled PCIeStream hardware\n");
    usleep(50000);

    magic = csrs[2*CSR_MAGIC];
    printf("[pciestreamd::main] pciestream magic = 0x%08lX\n", magic);
    if (magic != 0xCAFECAFE) exit(1);

    // printf("WARNING: inexplicalbly swapped RX/TX A and B buffer addresses.\n");
    printf("[pciestreamd::main] writing buffer addresses.\n");
    csrs[2*CSR_ADDR_RX_A] = addrRxA;
    csrs[2*CSR_ADDR_RX_B] = addrRxB;
    csrs[2*CSR_ADDR_TX_A] = addrTxA;
    csrs[2*CSR_ADDR_TX_B] = addrTxB;
    csrs[2*CSR_EN] = 1;
    usleep(50000);
    magic = csrs[2*CSR_MAGIC];
    printf("[pciestreamd::main] pciestream magic = 0x%08lX\n", magic);
    if (magic != 0xCAFECAFE) exit(1);

    while ( !((csrs[2*CSR_EN] >> 43) & 1) ) {
      csrs[2*CSR_EN] = 0xFFFFFFFF;
      usleep(50000);
      printf("waiting to enable pciestream\n");
      show_csrs(csrs);
    }
    printf("enabled.\n");
    show_csrs(csrs);
    magic = csrs[2*CSR_MAGIC];
    printf("[pciestreamd::main] pciestream magic = 0x%08lX\n", magic);
    if (magic != 0xCAFECAFE) exit(1);

    // Reset state
    txInit(&txState, conn, csrs, txA, txB);
    rxInit(&rxState, conn, csrs, rxA, rxB);
    uint64_t enable = csrs[2*CSR_EN];
    bool enabled = (enable >> 43) & 1;
    if (enabled == 0) {
      printf("[pciestreamd::main] PCIeStream hardware did not respond to reset; exiting.");
      show_csrs(csrs);
      exit(1);
    }

    // Event loop
    Status txStatus, txStatus_last;
    Status rxStatus, rxStatus_last;
    txStatus_last = CLOSED;
    rxStatus_last = CLOSED;
    for (;;) {
      txStatus = tx(&txState);
      if (txStatus == PROGRESS) {
        // printf("[pciestreamd::main] tx made progress\n");
        // show_csrs(csrs);
        // show_txstate(txState);
      }
      if (txStatus != txStatus_last) {
         // printf("[pciestreamd::main] tx status: %i changed from %i\n", txStatus, txStatus_last);
         // show_csrs(csrs);
         // show_txstate(txState);
         txStatus_last=txStatus;
      }
      if (txStatus == CLOSED) break;
      rxStatus = rx(&rxState);

      if (rxStatus != rxStatus_last) {
        // printf("[pciestreamd::main] rx status: %i changed from %i\n", rxStatus, rxStatus_last); rxStatus_last=rxStatus;
      }
      if (rxStatus == CLOSED) break;
      if (txStatus != PROGRESS && rxStatus != PROGRESS) {
        if (! alive(conn)) break;
        usleep(100);
      }
    }

    close(conn);
  }

  return 0;
}
