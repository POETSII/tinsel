// SPDX-License-Identifier: BSD-2-Clause
#include <stdint.h>
#include "matrices.h"
/***************************************************
 * Edit values between here ->
 * ************************************************/

// Matrix A as 2D array
const int32_t matrixA[MATAWID][MATALEN] = {
    { 0, 1 },
    { 1, 0 }
};

// Matrix B as 2D array
const int32_t matrixB[MATAWID][MATALEN] = {
    { 1, 0 },
    { 0, 1 }
};

/***************************************************
 * <- And here
 * ************************************************/

uint32_t mult_possible = (MATALEN == MATBWID) ? 1 : 0;

