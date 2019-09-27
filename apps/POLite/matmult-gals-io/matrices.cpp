// SPDX-License-Identifier: BSD-2-Clause
#include <stdint.h>
#include "matrices.h"

/***************************************************
 * Edit values between here ->
 * ************************************************/

// Matrix A as 2D array
uint32_t matrixA[MATAWID][MATALEN] = {
    { 2, 3 },
    { 4, 5 },
    { 6, 7 }
};

// Matrix B as 2D array
uint32_t matrixB[MATBWID][MATBLEN] = {
    { 1 },
    { 8 }
};

/***************************************************
 * <- And here
 * ************************************************/

uint32_t mult_possible = (MATALEN == MATBWID) ? 1 : 0;
