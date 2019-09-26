// SPDX-License-Identifier: BSD-2-Clause
#include <stdint.h>
#include "matrices.h"

// Matrix A as 2D array
uint32_t matrixA[MATAWID][MATALEN] = {
    { 0, 1 },
    { 2, 3 }
};

// Matrix B as 2D array
uint32_t matrixB[MATBWID][MATBLEN] = {
    { 7, 6 },
    { 5, 4 }
};

uint32_t mult_possible = (MATALEN == MATBWID) ? 1 : 0;
