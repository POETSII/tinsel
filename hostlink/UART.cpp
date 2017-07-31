#ifdef SIMULATE

// =============================================================================
// Simulation version
// =============================================================================

#include "UART.h"
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>
#include <assert.h>

// Open UART socket
static int openSocket(int instId)
{
  // Create socket
  int sock = socket(AF_UNIX, SOCK_STREAM, 0);
  if (sock == -1) {
    perror("socket");
    return -1;
  }

  // Connect to socket
  struct sockaddr_un addr;
  memset(&addr, 0, sizeof(struct sockaddr_un));
  addr.sun_family = AF_UNIX;
  snprintf(&addr.sun_path[1], sizeof(addr.sun_path)-2, "tinsel.b%i.0", instId);
  addr.sun_path[0] = '\0';
  int ret = connect(sock, (struct sockaddr *) &addr,
              sizeof(struct sockaddr_un));
  if (ret < 0) {
    perror("connect");
    return -1;
  }

  return sock;
}

// Constructor
UART::UART()
{
  sock = -1;
}

// Open UART with given instance id
void UART::open(int instId)
{
  instanceId = instId;
}

// Send bytes over UART
void UART::put(void* buffer, int numBytes)
{
  if (sock == -1) sock = openSocket(instanceId);
  uint8_t* ptr = (uint8_t*) buffer;
  while (numBytes > 0) {
    int n = write(sock, (void*) ptr, numBytes);
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
  if (sock == -1) sock = openSocket(instanceId);
  pollfd pfd;
  pfd.fd = sock;
  pfd.events = POLLIN;
  return poll(&pfd, 1, 0);
}

// Receive bytes over UART
void UART::get(void* buffer, int numBytes)
{
  if (sock == -1) sock = openSocket(instanceId);
  uint8_t* ptr = (uint8_t*) buffer;
  while (numBytes > 0) {
    int n = read(sock, (void*) ptr, numBytes);
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
  if (sock != -1) fsync(sock);
}

// Close UART
void UART::close()
{
  if (sock != -1) ::close(sock);
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
    if (n < 0) {
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
    if (n < 0) {
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
