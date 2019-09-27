// SPDX-License-Identifier: BSD-2-Clause
#ifndef _MATRICES_H_
#define _MATRICES_H_

#include <stdint.h>

// Parameters

/***************************************************
 * Edit values between here ->
 * ************************************************/

#define MATALEN (2)
#define MATAWID (3)
#define MATBLEN (1)
#define MATBWID (2)

/***************************************************
 * <- And here
 * ************************************************/

#define MESHLEN (MATBLEN)
#define MESHWID (MATAWID)
#define MESHHEI (MATALEN)

#define RETMATSIZE (MATAWID * MATBLEN)

extern uint32_t matrixA[MATAWID][MATALEN];
extern uint32_t matrixB[MATBWID][MATBLEN];

extern uint32_t mult_possible;

#endif
