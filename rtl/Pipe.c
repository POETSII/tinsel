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

// Filename prefix of named pipes
// (to be appended with a pipe id)
#define PIPE_IN  "/tmp/tinsel.in"
#define PIPE_OUT "/tmp/tinsel.out"

// Max number of bidirectional pipes supported
#define MAX_PIPES 8

// A file descriptor for each pipe
int pipeIn[MAX_PIPES] = {-1,-1,-1,-1,-1,-1,-1,-1};
int pipeOut[MAX_PIPES] = {-1,-1,-1,-1,-1,-1,-1,-1};

void initPipeIn(int id)
{
  assert(id < MAX_PIPES);
  if (pipeIn[id] != -1) return;
  // Create filename
  char filename[256];
  snprintf(filename, sizeof(filename), "%s.%i", PIPE_IN, id);
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
  // Create filename
  char filename[256];
  snprintf(filename, sizeof(filename), "%s.%i", PIPE_OUT, id);
  // Open pipe
  pipeOut[id] = open(filename, O_WRONLY);
}

// Non-blocking read of 8 bits
uint32_t pipeGet8(int id)
{
  assert(id < MAX_PIPES);
  uint8_t byte;
  if (pipeIn[id] == -1) initPipeIn(id);
  if (read(pipeIn[id], &byte, 1) == 1)
    return (uint32_t) byte;
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
  if (write(pipeOut[id], &byte, 1) == 1)
    return 1;
  return 0;
}

// Try to read 64 bits from pipe, giving 96-bit result. Bottom 64 bits
// contain data and MSB is 0 if data is valid or 1 if no data is
// available.  Non-blocking on 64-bit boundaries.
void pipeGet64(unsigned int* result96, int id)
{
  assert(id < MAX_PIPES);
  if (pipeIn[id] == -1) initPipeIn(id);
  uint8_t* bytes = (uint8_t*) result96;
  int count = read(pipeIn[id], bytes, 8);
  if (count == 8) {
    bytes[11] = 0;
    return;
  }
  else if (count > 0) {
    // Use blocking reads to get remaining data 
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(pipeIn[id], &fds);
    while (count < 8) {
      int res = select(1, &fds, NULL, NULL, NULL);
      assert(res >= 0);
      res = read(pipeIn[id], &bytes[count], 8-count);
      assert(res >= 0);
      count += res;
    }
    bytes[11] = 0;
    return;
  }
  else {
    bytes[11] = 0x80;
    return;
  }
}

// Try to write 64 bits to pipe.  Non-blocking on 64-bit boundaries,
// returning 0 when no write performed.
uint8_t pipePut64(int id, unsigned int* data64)
{
  assert(id < MAX_PIPES);
  if (pipeOut[id] == -1) {
    initPipeOut(id);
    if (pipeOut[id] == -1) return 0;
  }
  uint8_t* bytes = (uint8_t*) data64;
  int count = write(pipeOut[id], bytes, 8);
  if (count == 8)
    return 1;
  else if (count > 0) {
    // Use blocking writes to put remaining data
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(pipeOut[id], &fds);
    while (count < 8) {
      int res = select(1, &fds, NULL, NULL, NULL);
      assert(res >= 0);
      res = write(pipeOut[id], &bytes[count], 8-count);
      assert(res >= 0);
      count += res;
    }
    return 1;
  }
  else
    return 0;
}
