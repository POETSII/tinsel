#ifndef _LINK_H_
#define _LINK_H_

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

// =============================================================================
// Communicate with tinsel in simulation
// =============================================================================

#define FIFO_IN  "/tmp/tinsel.in"
#define FIFO_OUT "/tmp/tinsel.out"

class Link {
  int fifoIn;
  int fifoOut;
 public:
  Link() {
    fifoIn = fifoOut = -1;
  }

  void get(void* buffer, int count) {
    // Open fifo if not already opened
    if (fifoIn < 0) {
      // Create tinsel output fifo if it doesn't exist
      struct stat st;
      if (stat(FIFO_OUT, &st) != 0)
        mkfifo(FIFO_OUT, 0666);
      // Open tinsel output fifo for reading
      fifoIn = open(FIFO_OUT, O_RDONLY);
      if (fifoIn < 0) {
        fprintf(stderr, "Error opening " FIFO_OUT);
        exit(EXIT_FAILURE);
      }
    }
    // Read bytes
    uint8_t* ptr = (uint8_t*) buffer;
    while (count > 0) {
      int n = read(fifoIn, (void*) ptr, count);
      if (n < 0) {
        fprintf(stderr, "Error reading " FIFO_OUT);
        exit(EXIT_FAILURE);
      }
      ptr += n;
      count -= n;
    }
  }

  void put(void* buffer, int count) {
    // Open fifo if not already opened
    if (fifoOut < 0) {
      // Open tinsel input fifo for writing
      fifoOut = open(FIFO_IN, O_WRONLY);
      if (fifoOut < 0) {
        fprintf(stderr, "Error opening " FIFO_IN);
        exit(EXIT_FAILURE);
      }
    }
    // Write bytes
    uint8_t* ptr = (uint8_t*) buffer;
    while (count > 0) {
      int n = write(fifoOut, (void*) ptr, count);
      if (n < 0) {
        fprintf(stderr, "Error writing to " FIFO_IN);
        exit(EXIT_FAILURE);
      }
      ptr += n;
      count -= n;
    }
  }

  void flush() {
    if (fifoOut >= 0) fsync(fifoOut);
  }

  ~Link() {
    if (fifoIn >= 0) close(fifoIn);
    if (fifoOut >= 0) close(fifoOut);
  }
};

#endif
