// This header file is shared by the tinsel program and the host PC program

#ifndef _HEAT_H_
#define _HEAT_H_

// Simulate heat diffusion on a LENxLEN grid of threads.
// Each thread handles an 8x8 subgrid of cells.
#define LEN 8

// Format of messages sent to host
typedef struct {
  uint32_t padding;
  uint16_t x, y;
  uint8_t temps[8];
} HostMsg;

#endif
