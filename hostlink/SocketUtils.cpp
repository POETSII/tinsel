#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <netinet/in.h>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <assert.h>
#include <poll.h>
#include <errno.h>
#include "SocketUtils.h"

// Check if connection is alive
bool socketAlive(int conn)
{
  char buf;
  return send(conn, &buf, 0, 0) == 0;
}

// Can receive from connection?
bool socketCanGet(int conn)
{
  struct pollfd fd; fd.fd = conn; fd.events = POLLIN;
  int ret = poll(&fd, 1, 0);
  return (fd.revents & POLLIN);
}

// Can send on connection?
bool socketCanPut(int conn)
{
  struct pollfd fd; fd.fd = conn; fd.events = POLLOUT;
  int ret = poll(&fd, 1, 0);
  return (fd.revents & POLLOUT);
}

// Read exactly numBytes from socket, if any data is available
// Return < 0 on error, 0 if no data available, and 1 on success
int socketGet(int fd, char* buf, int numBytes)
{
  if (socketCanGet(fd)) {
    int got = 0;
    while (numBytes > 0) {
      int ret = recv(fd, &buf[got], numBytes, 0);
      if (ret < 0)
        return ret;
      else {
        got += ret;
        numBytes -= ret;
      }
    }
    return 1;
  }
  return 0;
}

// Either send exactly numBytes to a socket, or don't send anything
// Return < 0 on error, 0 if no data can be sent, and 1 on success
int socketPut(int fd, char* buf, int numBytes)
{
  int sent = 0;
  // Try non-blocking send
  int ret = send(fd, &buf[sent], numBytes, MSG_DONTWAIT);
  if (ret < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
    return ret;
  }
  else if (ret != numBytes) {
    // Partial send, not expected to happen in our use cases
    // But if it does, resort to blocking send
    sent += ret;
    numBytes -= ret;
    while (numBytes > 0) {
      ret = send(fd, &buf[sent], numBytes, 0);
      if (ret < 0)
        return ret;
      else {
        sent += ret;
        numBytes -= ret;
      }
    }
  }
  return 1;
}

// Create TCP connection to given host/port
int socketConnectTCP(const char* hostname, int port)
{
  // Resolve hostname
  hostent* hostInfo = gethostbyname2(hostname, AF_INET);
  if (hostInfo == NULL) {
    fprintf(stderr, "Can't result host name '%s'\n", hostname);
    exit(EXIT_FAILURE);
  }

  // Fill in socket address
  sockaddr_in sockAddr;
  sockAddr.sin_family = AF_INET;
  sockAddr.sin_port = htons(port);
  memcpy(&sockAddr.sin_addr, hostInfo->h_addr_list[0], hostInfo->h_length);

  // Create socket
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock == -1) {
    perror("socket");
    exit(EXIT_FAILURE);
  }

  // Connect to host
  if (connect(sock, (sockaddr*) &sockAddr, sizeof(sockAddr))) {
    fprintf(stderr, "Can't connect to host '%s' on port '%d'\n",
      hostname, port);
    exit(EXIT_FAILURE);
  }

  return sock;
}

// Read exactly numBytes from socket, blocking
void socketBlockingGet(int fd, char* buf, int numBytes)
{
  int got = 0;
  while (numBytes > 0) {
    int ret = recv(fd, &buf[got], numBytes, 0);
    if (ret < 0) {
      fprintf(stderr, "Error reading from socket\n");
      exit(EXIT_FAILURE); 
    }
    else {
      got += ret;
      numBytes -= ret;
    }
  }
  return;
}

// Either send exactly numBytes to a socket, blocking
void socketBlockingPut(int fd, char* buf, int numBytes)
{
  int sent = 0;
  while (numBytes > 0) {
    int ret = send(fd, &buf[sent], numBytes, 0);
    if (ret < 0) {
      fprintf(stderr, "Error writing to socket\n");
      exit(EXIT_FAILURE); 
    }
    else {
      sent += ret;
      numBytes -= ret;
    }
  }
}
