// Copyright (c) Matthew Naylor

// Board Control Daemon
// ====================
//
// Control FPGAs, and communicate with each FPGA JTAG UART, via a TCP socket.

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
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <assert.h>
#include <poll.h>
#include <errno.h>
#include <config.h>
#include "jtag/UARTBuffer.h"
#include "jtag/UART.h"
#include "PowerLink.h"
#include "BoardCtrl.h"
#include "SocketUtils.h"
#include "DebugLinkFormat.h"

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
  sockaddr_in sockAddr;
  memset(&sockAddr, 0, sizeof(sockaddr_in));
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

void server(int conn, int numBoards, UARTBuffer* uartLinks)
{
  // Open each UART
  #ifdef SIMULATE
    // Worker boards
    int count = 0;
    for (int y = 0; y < TinselMeshYLenWithinBox; y++)
      for (int x = 0; x < TinselMeshXLenWithinBox; x++) {
        int boardId = (y<<TinselMeshXBitsWithinBox) + x;
        uartLinks[count++].uart->open(boardId);
      }
    // Host board
    uartLinks[count++].uart->open(-1);
  #else
    for (int i = 0; i < numBoards; i++) uartLinks[i].uart->open(i+1);
  #endif

  // Packet buffer for sending and receiving
  BoardCtrlPkt pkt;

  // Send initial packet to indicate that all boards are up
  pkt.linkId = 0;
  pkt.payload[0] = DEBUGLINK_READY;
  while (1) {
    int n = socketPut(conn, (char*) &pkt, sizeof(BoardCtrlPkt));
    if (n < 0) return;
    if (n > 0) break;
  }

  // Serve UARTs every 'serveCount' iterations of event loop
  int serveMax = 32;
  int serveCount = 0;

  // Event loop
  while (1) {
    bool didPut = false;
    bool didGet = false;

    // Can we write a DebugLink packet to all UART buffers?
    bool allCanPut = true;
    for (int i = 0; i < numBoards; i++)
      allCanPut = allCanPut && uartLinks[i].canPut(DEBUGLINK_MAX_PKT_BYTES);

    // If so, try to receive a network packet and forward to UART
    if (allCanPut) {
      int ok = socketGet(conn, (char*) &pkt, sizeof(BoardCtrlPkt));
      if (ok < 0) return;
      if (ok > 0) {
        int numBytes = toDebugLinkSize(pkt.payload[0]);
        for (int i = 0; i < numBytes; i++)
          uartLinks[pkt.linkId].put(pkt.payload[i]);
        didPut = true;
      }
    }

    // Try to read from each UART, and forward to network
    for (int i = 0; i < numBoards; i++) {
      if (uartLinks[i].canGet(1)) {
        pkt.linkId = i;
        uint8_t cmd = uartLinks[i].peek();
        uint8_t numBytes = fromDebugLinkSize(cmd);
        if (uartLinks[i].canGet(numBytes)) {
          for (int j = 0; j < numBytes; j++)
            pkt.payload[j] = uartLinks[i].peekAt(j);
          int ok = socketPut(conn, (char*) &pkt, sizeof(BoardCtrlPkt));
          if (ok < 0) return;
          if (ok > 0) {
            for (int j = 0; j < numBytes; j++) uartLinks[i].get();
            didGet = true;
          }
        }
      }
    }

    // Sleep for a while if no progress made
    bool didSleep = false;
    if (!didPut && !didGet) {
      usleep(100);
      didSleep = true;
    }

    // Make progress on each UART buffer
    serveCount++;
    if (didSleep || serveCount >= serveMax) {
      if (!socketAlive(conn)) return;
      for (int i = 0; i < numBoards; i++) uartLinks[i].serve();
      serveCount = 0;
    }
  }
}

int main(int argc, char* argv[])
{
  // Ignore SIGPIPE
  signal(SIGPIPE, SIG_IGN);

  // JTAG UARTs
  UARTBuffer* uartLinks;

  // Determine number of boards
  int numBoards = TinselMeshXLenWithinBox * TinselMeshYLenWithinBox + 1;

  // Listen on TCP port
  int sock = createListener();

  while (1) {
    // Accept connection
    int conn = accept(sock, NULL, NULL);
    if (conn == -1) {
      perror("boardctrld: accept");
      exit(EXIT_FAILURE);
    }

    // Open lock file
    int lockFile = open("/tmp/boardctrld.lock", O_CREAT, 0444);
    if (lockFile == -1) return -1;

    // Acquire lock
    if (flock(lockFile, LOCK_EX | LOCK_NB) != 0) {
      close(lockFile);
      close(conn);
      continue;
    }

    // Power up worker boards
    #ifndef SIMULATE
    powerEnable(1);
    sleep(1);
    waitForFPGAs(numBoards);
    sleep(1);
    #endif

    // Create a UART link to each board
    uartLinks = new UARTBuffer [numBoards];

    // Invoke server to handle connection
    server(conn, numBoards, uartLinks);

    // Finished
    close(conn);

    // Free UART links
    delete [] uartLinks;

    // Power down worker boards
    #ifndef SIMULATE
    powerEnable(0);
    usleep(1500000);
    #endif
  }

  return 0;
}
