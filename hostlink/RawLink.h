#ifndef _RAWLINK_H_
#define _RAWLINK_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#ifdef SIMULATE

// =============================================================================
// Communicate with tinsel in simulation
// =============================================================================

#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>

#define FIFO_IN  "/tmp/tinsel.in"
#define FIFO_OUT "/tmp/tinsel.out"

class RawLink {
  int fifoIn;
  int fifoOut;
  int boardId;
 public:
  RawLink() {
    fifoIn = fifoOut = -1;
    boardId = 0;
  }

  void setId(int id) {
    boardId = id;
  }

  void get(void* buffer, int count) {
    // Open fifo if not already opened
    if (fifoIn < 0) {
      char filename[256];
      snprintf(filename, sizeof(filename), "%s.b%i.0", FIFO_OUT, boardId);
      // Create tinsel output fifo if it doesn't exist
      struct stat st;
      if (stat(filename, &st) != 0)
        mkfifo(filename, 0666);
      // Open tinsel output fifo for reading
      fifoIn = open(filename, O_RDONLY);
      if (fifoIn < 0) {
        fprintf(stderr, "Error opening %s\n", filename);
        exit(EXIT_FAILURE);
      }
    }
    // Read bytes
    uint8_t* ptr = (uint8_t*) buffer;
    while (count > 0) {
      int n = read(fifoIn, (void*) ptr, count);
      if (n < 0) {
        fprintf(stderr, "Error reading %s.b%i.0\n", FIFO_OUT, boardId);
        exit(EXIT_FAILURE);
      }
      ptr += n;
      count -= n;
    }
  }

  bool canGet() {
    pollfd pfd;
    pfd.fd = fifoIn;
    pfd.events = POLLIN;
    return poll(&pfd, 1, 0);
  }

  void put(void* buffer, int count) {
    // Open fifo if not already opened
    if (fifoOut < 0) {
      char filename[256];
      snprintf(filename, sizeof(filename), "%s.b%i.0", FIFO_IN, boardId);
      // Open tinsel input fifo for writing
      fifoOut = open(filename, O_WRONLY);
      if (fifoOut < 0) {
        fprintf(stderr, "Error opening %s\n", filename);
        exit(EXIT_FAILURE);
      }
    }
    // Write bytes
    uint8_t* ptr = (uint8_t*) buffer;
    while (count > 0) {
      int n = write(fifoOut, (void*) ptr, count);
      if (n < 0) {
        fprintf(stderr, "Error writing to %s.b%i.0\n", FIFO_IN, boardId);
        exit(EXIT_FAILURE);
      }
      ptr += n;
      count -= n;
    }
  }

  void flush() {
    if (fifoOut >= 0) fsync(fifoOut);
  }

  ~RawLink() {
    if (fifoIn >= 0) close(fifoIn);
    if (fifoOut >= 0) close(fifoOut);
  }
};

#else

// =============================================================================
// Communicate with tinsel on FPGA
// =============================================================================

#include "JtagAtlantic.h"

class RawLink {
  JTAGATLANTIC* jtag;
  int deviceId;

  void open() {
    if (jtag == NULL) {
      char chain[256];
      snprintf(chain, sizeof(chain), "%i", deviceId+1);
      jtag = jtagatlantic_open(chain, 0, 0, "hostlink");
      //jtag = jtagatlantic_open(getenv("CABLE"), deviceId, 0, "hostlink");
      if (jtag == NULL) {
        fprintf(stderr, "Error opening JTAG UART\n");
        exit(EXIT_FAILURE);
      }
    }
  }

 public:
  RawLink() {
    jtag = NULL;
    deviceId = 0;
  }

  void setId(int id) {
    deviceId = id;
  }

  void get(void* buffer, int count) {
    // Open JTAG UART if not already opened
    if (jtag == NULL) open();
    // Read bytes
    uint8_t* ptr = (uint8_t*) buffer;
    while (count > 0) {
      int n = jtagatlantic_read(jtag, (char*) ptr, count);
      if (n < 0) {
        fprintf(stderr, "Error reading from JTAG UART\n");
        exit(EXIT_FAILURE);
      }
      ptr += n;
      count -= n;
    }
  }

  bool canGet() {
    return jtagatlantic_bytes_available(jtag) > 0;
  }

  void put(void* buffer, int count) {
    // Open JTAG UART if not already opened
    if (jtag == NULL) open();
    // Write bytes
    uint8_t* ptr = (uint8_t*) buffer;
    while (count > 0) {
      int n = jtagatlantic_write(jtag, (char*) ptr, count);
      if (n < 0) {
        fprintf(stderr, "Error writing to JTAG UART\n");
        exit(EXIT_FAILURE);
      }
      ptr += n;
      count -= n;
    }
  }

  void flush() {
    if (jtag != NULL) jtagatlantic_flush(jtag);
  }

  ~RawLink() {
    if (jtag != NULL) jtagatlantic_close(jtag);
  }
};

#endif

#endif
