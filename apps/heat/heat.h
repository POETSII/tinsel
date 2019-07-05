// SPDX-License-Identifier: BSD-2-Clause
// This header file is shared by the tinsel program and the host PC program

#ifndef _HEAT_H_
#define _HEAT_H_

// Number of POETS boxes to use
#define X_BOXES 1
#define Y_BOXES 1

// Format of messages sent to host
typedef struct {
  uint32_t padding;
  uint16_t x, y;
  uint8_t temps[8];
} HostMsg;

#endif
