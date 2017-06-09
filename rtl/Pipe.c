// This module gives the BSV simulator access to bidirectional pipe(s)
// on the filesystem.

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>
#include <signal.h>

// Filename prefix of named pipes
// (to be suffixed with a board id and a pipe id)
#define PIPE_IN  "/tmp/tinsel.in"
#define PIPE_OUT "/tmp/tinsel.out"

// Max number of bidirectional pipes supported
#define MAX_PIPES 8

// A file descriptor for each pipe
int pipeIn[MAX_PIPES] = {-1,-1,-1,-1,-1,-1,-1,-1};
int pipeOut[MAX_PIPES] = {-1,-1,-1,-1,-1,-1,-1,-1};

// Get board identifier from environment
uint32_t getBoardId()
{
  char* s = getenv("BOARD_ID");
  if (s == NULL) {
    fprintf(stderr, "ERROR: Environment variable BOARD_ID not defined\n");
    exit(EXIT_FAILURE);
  }
  return (uint32_t) atoi(s);
}

void initPipeIn(int id)
{
  assert(id < MAX_PIPES);
  if (pipeIn[id] != -1) return;
  // Ignore SIGPIPE
  signal(SIGPIPE, SIG_IGN);
  // Create filename
  char filename[256];
  snprintf(filename, sizeof(filename), "%s.b%i.%i", PIPE_IN, getBoardId(), id);
  // Create pipe if it doesn't exist
  struct stat st;
  if (stat(filename, &st) != 0)
    mkfifo(filename, 0666);
  // Open pipe
  pipeIn[id] = open(filename, O_RDONLY | O_NONBLOCK);
  if (pipeIn[id] == -1) {
    fprintf(stderr, "Error opening input pipe %i", id);
    exit(EXIT_FAILURE);
  }
}

void initPipeOut(int id)
{
  assert(id < MAX_PIPES);
  if (pipeOut[id] != -1) return;
  // Ignore SIGPIPE
  signal(SIGPIPE, SIG_IGN);
  // Create filename
  char filename[256];
  snprintf(filename, sizeof(filename), "%s.b%i.%i", PIPE_OUT, getBoardId(), id);
  // Create pipe if it doesn't exist
  struct stat st;
  if (stat(filename, &st) != 0)
    mkfifo(filename, 0666);
  // Open pipe
  pipeOut[id] = open(filename, O_WRONLY | O_NONBLOCK);
}

// Non-blocking read of 8 bits
uint32_t pipeGet8(int id)
{
  assert(id < MAX_PIPES);
  uint8_t byte;
  if (pipeIn[id] == -1) initPipeIn(id);
  int n = read(pipeIn[id], &byte, 1);
  if (n == 1)
    return (uint32_t) byte;
  else if (n == 0 || (n == -1 && errno != EAGAIN)) {
    close(pipeIn[id]);
    pipeIn[id] = -1;
  }
  return -1;
}

// Non-blocking write of 8 bits
uint8_t pipePut8(int id, uint8_t byte)
{
  assert(id < MAX_PIPES);
  if (pipeOut[id] == -1) {
    initPipeOut(id);
    if (pipeOut[id] == -1) return 0;
  }
  int n = write(pipeOut[id], &byte, 1);
  if (n == 1)
    return 1;
  else if (n == 0 || (n == -1 && errno != EAGAIN)) {
    close(pipeOut[id]);
    pipeOut[id] = -1;
  }
  return 0;
}

// Try to read N bytes from pipe, giving N+1 byte result. Bottom N
// bytes contain data and MSB is 0 if data is valid or non-zero if no
// data is available.  Non-blocking on N-byte boundaries.
void pipeGetN(unsigned int* result, int id, int nbytes)
{
  assert(id < MAX_PIPES);
  if (pipeIn[id] == -1) initPipeIn(id);
  uint8_t* bytes = (uint8_t*) result;
  int count = read(pipeIn[id], bytes, nbytes);
  if (count == nbytes) {
    bytes[nbytes] = 0;
    return;
  }
  else if (count > 0) {
    // Use blocking reads to get remaining data 
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(pipeIn[id], &fds);
    while (count < nbytes) {
      int res = select(1, &fds, NULL, NULL, NULL);
      assert(res >= 0);
      res = read(pipeIn[id], &bytes[count], nbytes-count);
      assert(res >= 0);
      count += res;
    }
    bytes[nbytes] = 0;
    return;
  }
  else {
    bytes[nbytes] = 0xff;
    if (count == 0 || (count == -1 && errno != EAGAIN)) {
      close(pipeIn[id]);
      pipeIn[id] = -1;
    }
    return;
  }
}

// Try to write N bytes to pipe.  Non-blocking on N-bytes boundaries,
// returning 0 when no write performed.
uint8_t pipePutN(int id, int nbytes, unsigned int* data)
{
  assert(id < MAX_PIPES);
  if (pipeOut[id] == -1) {
    initPipeOut(id);
    if (pipeOut[id] == -1) return 0;
  }
  uint8_t* bytes = (uint8_t*) data;
  int count = write(pipeOut[id], bytes, nbytes);
  if (count == nbytes)
    return 1;
  else if (count > 0) {
    // Use blocking writes to put remaining data
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(pipeOut[id], &fds);
    while (count < nbytes) {
      int res = select(1, &fds, NULL, NULL, NULL);
      assert(res >= 0);
      res = write(pipeOut[id], &bytes[count], nbytes-count);
      assert(res >= 0);
      count += res;
    }
    return 1;
  }
  else {
    if (count == 0 && (count == -1 && errno != EAGAIN)) {
      close(pipeOut[id]);
      pipeOut[id] = -1;
    }
    return 0;
  }
}
