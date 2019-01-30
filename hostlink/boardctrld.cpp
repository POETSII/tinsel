// Copyright (c) Matthew Naylor

// Board Control Daemon
// ====================
//
// Control FPGAs, and communicate with each FPGA JTAG via a TCP socket.

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
#include "UART.h"
#include "PowerLink.h"
#include "boardctrld.h"

// Constants
// ---------

// Default TCP port to listen on
#define TCP_PORT 10101

// Functions
// ---------

// Create listening socket
int createListener()
{
  // Create socket
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock == -1) {
    perror("boardctrld: socket");
    exit(EXIT_FAILURE);
  }

  // Bind socket
  struct sockaddr_in sockAddr;
  memset(&sockAddr, 0, sizeof(struct sockaddr_un));
	sockAddr.sin_family = AF_INET;
	sockAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	sockAddr.sin_port = htons(TCP_PORT);
  int ret = bind(sock, (const struct sockaddr *) &sockAddr,
                   sizeof(struct sockaddr_in));
  if (ret == -1) {
    perror("boardctrld: bind");
    exit(EXIT_FAILURE);
  }

  // Listen for connections
  ret = listen(sock, 0);
  if (ret == -1) {
    perror("boardctrld: listen");
    exit(EXIT_FAILURE);
  }

  return sock;
}

// Buffer for sending and receiving packets
struct UARTBuffer {
  UART* uart;
  int size;
  int nbytesIn, nbytesOut;
  char* bytesIn;
  char* bytesOut;

  PktBuffer(UART* u, int n) {
    size = n;
    uart = u;
    bytesIn = new char [n];
    bytesOut = new char [n];
    nbytesIn = nbytesOut = 0;
  }

  void progress() {
    if (nbytesOut > 0) {
      int ret = uart->write(&bytesOut[nbytesOut], nbytesOut);
      if (ret > 0) {
        bytesOut += ret;
        if bytesOut
      }
    }
  };

  bool canPut() {
    if (nbytesOut < size) return true;

    uart->write();
  };
  void put(char byte) {
    bytesOut[nbytesOut] = byte;
    nbytesOut++
  }

  bool canGet() { return bytesIn == sizeof(BoardCtrlPkt); }
  void get() {
  }

  ~PktBuffer() {
    delete [] bytesIn;
    delete [] bytesOut;
  }
};

// Receive a packet over the connection
int getPacket(int fd, BoardCtrlPkt* pkt)
{
  char* buf = &pkt;
  int numBytes = sizeof(BoardCtrlPkt);
  int got = 0;
  while (numBytes > 0) {
    int ret = read(fd, &buf[got], numBytes);
    if (ret <= 0)
      return 0;
    else {
      got += ret;
      numBytes -= ret;
    }
  }
  return 1;
}

// Send a packet over the connection
int putPacket(int fd, BoardCtrlPkt* pkt)
{
  char* buf = &pkt;
  int numBytes = sizeof(BoardCtrlPkt);
  int sent = 0;
  while (numBytes > 0) {
    int ret = read(fd, &buf[sent], numBytes);
    if (ret <= 0)
      return 0;
    else {
      sent += ret;
      numBytes -= ret;
    }
  }
  return 1;
}

int server(int conn)
{
  // Determine number of boards
  int numBoards = TinselMeshXLenWithinBox * TinselMeshYLenWithinBox + 1;

  // Create a UART link to each board
  UART* uartLinks = new UART [numBoards];

  // Open each UART
  #ifdef SIMULATE
    // Worker boards
    int count = 0;
    for (int y = 0; y < TinselMeshYLenWithinBox; y++)
      for (int x = 0; x < TinselMeshXLenWithinBox; x++) {
        int boardId = (y<<TinselMeshXBitsWithinBox) + x;
        uartLinks[count++].open(boardId);
      }
    // Host board
    uartLinks[count++].open(-1);
  #else
    for (int i = 0; i < numBoards; i++) uartLinks[i].open(i+1);
  #endif

  // Packet buffer for sending and receiving
  BoardCtrlPkt pkt;

  // Send initial packet to indicate that all boards are up
  pkt.linkId = 0;
  pkt.channel = CtrlChannel;
  pkt.payload = 0;
  if (! putPacket(conn, &pkt)) return 0;

  // Can we send or receive over the socket?
  struct pollfd fd; fd.fd = s->client; fd.events = POLLIN | POLLOUT;
  int ret = poll(&fd, 1, -1);
  if (ret < 0)
    return 0;
  else {
    if (fd.revents & POLLIN) {
      if (getPacket(conn, &pkt)) {
        assert(pkt.channel == UartChannel);
        uartLinks[pkt.linkId].put(pkt.payload);
      }
      else
        return 0;
    }
    else if (fd.revents & POLLOUT) {
      bool sent = false;
      for (int i = 0; i < numBoards; i++) {
        if (uartLinks[i].canGet()) {
          pkt.linkId = i;
          pkt.channel = UartChannel;
          uartLinks[i].get(&pkt.payload, 1);
          if (!putPacket(conn, &pkt)) return 0;
          sent = true;
        }
      }
      if (!sent) usleep(1000);
    }
  }
}

int main(int argc, char* argv[])
{
  // Ignore SIGPIPE
  signal(SIGPIPE, SIG_IGN);

  int sock = createListener();

  while (1) {
    // Accept connection
    int conn = accept(sock, NULL, NULL);
    if (conn == -1) {
      perror("boardctrld: accept");
      exit(EXIT_FAILURE);
    }

    // Open lock file
    int lockFile = open("/tmp/HostLink.lock", O_CREAT, 0444);
    if (lockFile == -1) return -1;

    // Acquire lock
    if (flock(lockFile, LOCK_EX | LOCK_NB) != 0) return -1;

    // Power up worker boards
    #ifndef SIMULATE
    powerEnable(1);
    sleep(1);
    waitForFPGAs(numBoards);
    sleep(1);
    #endif

    // Invoke server to handle connection
    server(conn);

    // Finished
    close(conn);

    // Power down worker boards
    powerEnable(0);
  }

  return 0;
}
