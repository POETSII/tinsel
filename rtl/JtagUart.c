// This module enables simulation of designs containing a JTAG UART.
// It connects the input and output byte streams to named pipes in the
// file system.  Note: this code only support designs with a single
// JTAG UART at present.

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

// Filenames of named pipes
#define FIFO_IN  "/tmp/tinsel.in"
#define FIFO_OUT "/tmp/tinsel.out"

int fifoIn = -1;
int fifoOut = -1;

void initFifoIn()
{
  if (fifoIn != -1) return;
  // Create FIFO if it doesn't exist
  struct stat st;
  if (stat(FIFO_IN, &st) != 0)
    mkfifo(FIFO_IN, 0666);
  // Open FIFO
  fifoIn = open(FIFO_IN, O_RDONLY | O_NONBLOCK);
  if (fifoIn == -1) {
    fprintf(stderr, "Error opening host input FIFO");
    exit(EXIT_FAILURE);
  }
}

void initFifoOut()
{
  if (fifoOut != -1) return;
  // Open FIFO
  fifoOut = open(FIFO_OUT, O_WRONLY);
}

uint32_t uartGetByte()
{
  uint8_t byte;
  if (fifoIn == -1) initFifoIn();
  if (read(fifoIn, &byte, 1) == 1)
    return (uint32_t) byte;
  return -1;
}

uint8_t uartPutByte(uint8_t byte)
{
  if (fifoOut == -1) {
    initFifoOut();
    if (fifoOut == -1) return 0;
  }
  if (write(fifoOut, &byte, 1) == 1)
    return 1;
  return 0;
}
