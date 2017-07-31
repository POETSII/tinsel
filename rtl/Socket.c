// This module gives the BSV simulator access to UNIX domain sockets

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>
#include <signal.h>

// Name prefix of the sockets
// (to be suffixed with a board id and a socket id)
#define SOCKET "@tinsel"

// Max number of sockets supported
#define MAX_SOCKETS 8

// A file descriptor for each socket
int sock[MAX_SOCKETS] = {-1,-1,-1,-1,-1,-1,-1,-1};

// A file descriptor for each connection
int conn[MAX_SOCKETS] = {-1,-1,-1,-1,-1,-1,-1,-1};

// Get board identifier from environment
int getBoardId()
{
  char* s = getenv("BOARD_ID");
  if (s == NULL) {
    fprintf(stderr, "ERROR: Environment variable BOARD_ID not defined\n");
    exit(EXIT_FAILURE);
  }
  return atoi(s);
}

// Make a socket non-blocking
void socketSetNonBlocking(int sock)
{
  int flags = fcntl(sock, F_GETFL, 0);
  if (flags == -1) {
    perror("fcntl");
    exit(EXIT_FAILURE);
  }
  int ret = fcntl(sock, F_SETFL, flags|O_NONBLOCK);
  if (ret == -1) {
    perror("fcntl");
    exit(EXIT_FAILURE);
  }
}

// Open, bind and listen
inline void socketInit(int id)
{
  assert(id < MAX_SOCKETS);
  if (sock[id] != -1) return;

  // Ignore SIGPIPE
  signal(SIGPIPE, SIG_IGN);

  // Create socket
  sock[id] = socket(AF_UNIX, SOCK_STREAM, 0);
  if (sock[id] == -1) {
    perror("socket");
    exit(EXIT_FAILURE);
  }

  // Bind socket
  struct sockaddr_un sockAddr;
  memset(&sockAddr, 0, sizeof(struct sockaddr_un));
  sockAddr.sun_family = AF_UNIX;
  snprintf(sockAddr.sun_path, sizeof(sockAddr.sun_path)-1,
             "%s.b%i.%i", SOCKET, getBoardId(), id);
  if (sockAddr.sun_path[0] == '@') sockAddr.sun_path[0] = '\0';
  int ret = bind(sock[id], (const struct sockaddr *) &sockAddr,
                   sizeof(struct sockaddr_un));
  if (ret == -1) {
    perror("bind");
    exit(EXIT_FAILURE);
  }

  // Listen for connections
  ret = listen(sock[id], 0);
  if (ret == -1) {
    perror("listen");
    exit(EXIT_FAILURE);
  }

  // Make it non-blocking
  socketSetNonBlocking(sock[id]);
}

// Accept connection
inline void socketAccept(int id)
{
  assert(id < MAX_SOCKETS);

  if (conn[id] != -1) return;
  socketInit(id);

  // Accept connection
  conn[id] = accept(sock[id], NULL, NULL);

  // Make connection non-blocking
  if (conn[id] != -1)
    socketSetNonBlocking(conn[id]);
}

// Non-blocking read of 8 bits
uint32_t socketGet8(int id)
{
  uint8_t byte;
  socketAccept(id);
  if (conn[id] == -1) return -1;
  int n = read(conn[id], &byte, 1);
  if (n == 1)
    return (uint32_t) byte;
  else if (n == -1 && errno != EAGAIN) {
    close(conn[id]);
    conn[id] = -1;
  }
  return -1;
}

// Non-blocking write of 8 bits
uint8_t socketPut8(int id, uint8_t byte)
{
  socketAccept(id);
  if (conn[id] == -1) return 0;
  int n = write(conn[id], &byte, 1);
  if (n == 1)
    return 1;
  else if (n == -1 && errno != EAGAIN) {
    close(conn[id]);
    conn[id] = -1;
  }
  return 0;
}

// Try to read N bytes from socket, giving N+1 byte result. Bottom N
// bytes contain data and MSB is 0 if data is valid or non-zero if no
// data is available.  Non-blocking on N-byte boundaries.
void socketGetN(unsigned int* result, int id, int nbytes)
{
  uint8_t* bytes = (uint8_t*) result;
  socketAccept(id);
  if (conn[id] == -1) {
    bytes[nbytes] = 0xff;
    return;
  }
  int count = read(conn[id], bytes, nbytes);
  if (count == nbytes) {
    bytes[nbytes] = 0;
    return;
  }
  else if (count > 0) {
    // Use blocking reads to get remaining data 
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(conn[id], &fds);
    while (count < nbytes) {
      int res = select(1, &fds, NULL, NULL, NULL);
      assert(res >= 0);
      res = read(conn[id], &bytes[count], nbytes-count);
      assert(res >= 0);
      count += res;
    }
    bytes[nbytes] = 0;
    return;
  }
  else {
    bytes[nbytes] = 0xff;
    if (count == -1 && errno != EAGAIN) {
      close(conn[id]);
      conn[id] = -1;
    }
    return;
  }
}

// Try to write N bytes to socket.  Non-blocking on N-bytes boundaries,
// returning 0 when no write performed.
uint8_t socketPutN(int id, int nbytes, unsigned int* data)
{
  socketAccept(id);
  if (conn[id] == -1) return 0;
  uint8_t* bytes = (uint8_t*) data;
  int count = write(conn[id], bytes, nbytes);
  if (count == nbytes)
    return 1;
  else if (count > 0) {
    // Use blocking writes to put remaining data
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(conn[id], &fds);
    while (count < nbytes) {
      int res = select(1, &fds, NULL, NULL, NULL);
      assert(res >= 0);
      res = write(conn[id], &bytes[count], nbytes-count);
      assert(res >= 0);
      count += res;
    }
    return 1;
  }
  else {
    if (count == -1 && errno != EAGAIN) {
      close(conn[id]);
      conn[id] = -1;
    }
    return 0;
  }
}
