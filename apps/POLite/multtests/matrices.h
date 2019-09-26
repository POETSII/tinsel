// SPDX-License-Identifier: BSD-2-Clause
#ifndef _MATRICES_H_
#define _MATRICES_H_

#include <stdint.h>

// Parameters

#define MATALEN (2)
#define MATAWID (2)
#define MATBLEN (2)
#define MATBWID (2)

#define MESHLEN (MATAWID)
#define MESHWID (MATBLEN)
#define MESHHEI (MATAWID)

#define RETMATSIZE (MESHLEN * MESHWID)

extern uint32_t matrixA[MATAWID][MATALEN];
extern uint32_t matrixB[MATBWID][MATBLEN];

extern uint32_t mult_possible;

#endif
