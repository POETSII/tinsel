// SPDX-License-Identifier: BSD-2-Clause
// This header file is shared by the tinsel program and the host PC program

#ifndef _HEAT_H_
#define _HEAT_H_

// Each board uses a X_LOCAL_LEN x Y_LOCAL_LEN grid of threads
#define X_LOCAL_LEN (1 << ((TinselLogThreadsPerBoard+1) >> 1))
#define Y_LOCAL_LEN (1 << (TinselLogThreadsPerBoard >> 1))

// And there is a 2D mesh of boards
// Assumption 1: X_BOARDS <= TinselMeshXLenWithinBox
// Assumption 2: Y_BOARDS <= TinselMeshYLenWithinBox
#define X_BOARDS 1
#define Y_BOARDS 1

// In all, the program uses an X_LEN * Y_LEN grid of threads
#define X_LEN (X_LOCAL_LEN*X_BOARDS)
#define Y_LEN (Y_LOCAL_LEN*Y_BOARDS)

// Format of messages sent to host
typedef struct {
  uint32_t padding;
  uint16_t x, y;
  uint8_t temps[8];
} HostMsg;

#endif
