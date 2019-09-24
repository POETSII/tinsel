// SPDX-License-Identifier: BSD-2-Clause
#ifndef _MATRICES_H_
#define _MATRICES_H_

#include <stdint.h>

// Parameters
#define LENGTH (2)
#define WIDTH  (2)
#define HEIGHT (2)
#define RETMATSIZE (LENGTH * WIDTH)

extern uint32_t matrixA[WIDTH][LENGTH];
extern uint32_t matrixB[WIDTH][LENGTH];

#endif
