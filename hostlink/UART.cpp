#ifdef SIMULATE

// =============================================================================
// Simulation version
// =============================================================================

#include "UART.h"
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>
#include <assert.h>

// Open input fifo
static int openFifoIn(int instId)
{
  char filename[256];
  // Open tinsel output fifo for reading
  snprintf(filename, sizeof(filename), "/tmp/tinsel.out.b%i.0", instId);
  int fifoIn = ::open(filename, O_RDONLY);
  if (fifoIn < 0) {
    fprintf(stderr, "Error opening %s\n", filename);
    exit(EXIT_FAILURE);
  }
  return fifoIn;
}

// Open output fifo
static int openFifoOut(int instId)
{
  char filename[256];
  // Open tinsel input fifo for writing
  snprintf(filename, sizeof(filename), "/tmp/tinsel.in.b%i.0", instId);
  int fifoOut = ::open(filename, O_WRONLY);
  if (fifoOut < 0) {
    fprintf(stderr, "Error opening %s\n", filename);
    exit(EXIT_FAILURE);
  }
  return fifoOut;
}

// Constructor
UART::UART()
{
  fifoIn = fifoOut = -1;
}

// Open UART with given instance id
void UART::open(int instId)
{
  instanceId = instId;
}

// Send bytes over UART
void UART::put(void* buffer, int numBytes)
{
  if (fifoOut < 0) fifoOut = openFifoOut(instanceId);
  uint8_t* ptr = (uint8_t*) buffer;
  while (numBytes > 0) {
    int n = write(fifoOut, (void*) ptr, numBytes);
    if (n <= 0) {
      fprintf(stderr, "Error writing to UART %i\n", instanceId);
      exit(EXIT_FAILURE);
    }
    ptr += n;
    numBytes -= n;
  }
}

// Is a byte available for reading?
bool UART::canGet()
{
  if (fifoIn < 0) fifoIn = openFifoIn(instanceId);
  pollfd pfd;
  pfd.fd = fifoIn;
  pfd.events = POLLIN;
  return poll(&pfd, 1, 0);
}

// Receive bytes over UART
void UART::get(void* buffer, int numBytes)
{
  if (fifoIn < 0) fifoIn = openFifoIn(instanceId);
  uint8_t* ptr = (uint8_t*) buffer;
  while (numBytes > 0) {
    int n = read(fifoIn, (void*) ptr, numBytes);
    if (n <= 0) {
      fprintf(stderr, "Error reading UART %i\n", instanceId);
      exit(EXIT_FAILURE);
    }
    ptr += n;
    numBytes -= n;
  }
}

// Flush writes
void UART::flush()
{
  if (fifoOut >= 0) fsync(fifoOut);
}

// Close UART
void UART::close()
{
  if (fifoIn >= 0) ::close(fifoIn);
  if (fifoOut >= 0) ::close(fifoOut);
}

#else

// =============================================================================
// FPGA version
// =============================================================================

#include "UART.h"
#include "JtagAtlantic.h"

// Constructor
UART::UART()
{
  jtag = NULL;
  instanceId = -1;
}

// Open UART with given instance id
void UART::open(int instId)
{
  char chain[256];
  snprintf(chain, sizeof(chain), "%i", instId);
  jtag = jtagatlantic_open(chain, 0, 0, "hostlink");
  if (jtag == NULL) {
    fprintf(stderr, "Error opening JTAG UART %i\n", instId);
    exit(EXIT_FAILURE);
  }
  instanceId = instId;
}

// Send bytes over UART
void UART::put(void* buffer, int numBytes)
{
  uint8_t* ptr = (uint8_t*) buffer;
  while (numBytes > 0) {
    int n = jtagatlantic_write(jtag, (char*) ptr, numBytes);
    if (n <= 0) {
      fprintf(stderr, "Error writing to JTAG UART\n");
      exit(EXIT_FAILURE);
    }
    ptr += n;
    numBytes -= n;
  }
}

// Is a byte available for reading?
bool UART::canGet()
{
  return jtagatlantic_bytes_available(jtag) > 0;
}

// Receive bytes over UART
void UART::get(void* buffer, int numBytes)
{
  uint8_t* ptr = (uint8_t*) buffer;
  while (numBytes > 0) {
    int n = jtagatlantic_read(jtag, (char*) ptr, numBytes);
    if (n <= 0) {
      fprintf(stderr, "Error reading from JTAG UART\n");
      exit(EXIT_FAILURE);
    }
    ptr += n;
    numBytes -= n;
  }
}

// Flush writes
void UART::flush()
{
  if (jtag != NULL) jtagatlantic_flush(jtag);
}

// Close UART
void UART::close()
{
  if (jtag != NULL) jtagatlantic_close(jtag);
}

#endif
