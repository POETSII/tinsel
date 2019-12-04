// SPDX-License-Identifier: BSD-2-Clause
#include <stdint.h>
#include "matrices.h"

/***************************************************
 * Edit values between here ->
 * ************************************************/

// Matrix A as 2D array
int32_t matrixA[MATAWID][MATALEN] = {
    { 0, 1, 2, 3, 4, 5 },
    { 0, 1, 2, 3, 4, 5 },
    { 0, 1, 2, 3, 4, 5 },
    { 0, 1, 2, 3, 4, 5 },
    { 0, 1, 2, 3, 4, 5 },
    { 0, 1, 2, 3, 4, 5 }
};

// Matrix B as 2D array
int32_t matrixB[MATBWID][MATBLEN] = {
    { 0, 1, 2, 3, 4, 5 },
    { 0, 1, 2, 3, 4, 5 },
    { 0, 1, 2, 3, 4, 5 },
    { 0, 1, 2, 3, 4, 5 },
    { 0, 1, 2, 3, 4, 5 },
    { 0, 1, 2, 3, 4, 5 }
};


/***************************************************
 * <- And here
 * ************************************************/

uint32_t mult_possible = (MATALEN == MATBWID) ? 1 : 0;
