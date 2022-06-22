// SPDX-License-Identifier: BSD-2-Clause
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

  ret = fcntl(sock, F_GETFL, 0);
  if (ret < 0) {
    perror("fcntl");
    return -1;
  }

  ret = fcntl(sock, F_SETFL, ret | O_NONBLOCK);
  if (ret < 0) {
    perror("fcntl");
    return -1;
  }
  printf("[DbgLink::UART::openSocket (sim)] connected to tinsel.b%i.0\n", instId);
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
int UART::write(char* ptr, int numBytes)
{
  if (sock == -1) sock = openSocket(instanceId);
  int ret = send(sock, (void*) ptr, numBytes, 0);
  if (ret < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
  }
  return ret;
}

// Read bytes over UART
int UART::read(char* ptr, int numBytes)
{
  if (sock == -1) sock = openSocket(instanceId);
  int ret = recv(sock, (void*) ptr, numBytes, 0);
  if (ret < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
  }
  return ret;
}

// Flush writes
void UART::flush()
{
  if (sock != -1) fsync(sock);
}

// Close UART
void UART::close()
{
  if (sock != -1) {
    ::close(sock);
    sock = -1;
  }
}

// Destructor
UART::~UART()
{
  close();
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
  printf("[UART::open] Opened JTAG UART %i\n", instId);
}

// Send bytes over UART
int UART::write(char* data, int numBytes)
{
  int ret = jtagatlantic_write(jtag, (char*) data, numBytes);
  printf("[UART::write] written %i bytes; retval %i\n", numBytes, ret);
  return ret;
}

// Receive bytes over UART
int UART::read(char* data, int numBytes)
{
  int ret = jtagatlantic_read(jtag, (char*) data, numBytes);
  if (ret > 0) printf("[UART::read] tried to read %i bytes; retval %i\n", numBytes, ret);
  return ret;
}

// Flush writes
void UART::flush()
{
  if (jtag != NULL) jtagatlantic_flush(jtag);
}

// Close UART
void UART::close()
{
  if (jtag != NULL) {
    jtagatlantic_close(jtag);
    jtag = NULL;
    instanceId = -1;
  }
}

// Destructor
UART::~UART()
{
  close();
}

#endif
