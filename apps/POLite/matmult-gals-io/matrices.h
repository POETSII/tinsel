// SPDX-License-Identifier: BSD-2-Clause
#ifndef _MATRICES_H_
#define _MATRICES_H_

#include <stdint.h>

// Parameters

#define MATALEN (2)
#define MATAWID (2)
#define MATBLEN (2)
#define MATBWID (2)

#define MESHLEN (MATBLEN)
#define MESHWID (MATAWID)
#define MESHHEI (MATALEN)

#define RETMATSIZE (MATALEN * MATBWID)

extern uint32_t matrixA[MATAWID][MATALEN];
extern uint32_t matrixB[MATBWID][MATBLEN];

extern uint32_t mult_possible;

#endif
