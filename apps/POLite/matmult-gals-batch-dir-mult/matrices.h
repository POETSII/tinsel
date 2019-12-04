// SPDX-License-Identifier: BSD-2-Clause
#ifndef _MATRICES_H_
#define _MATRICES_H_

#include <stdint.h>

// Parameters

/***************************************************
 * Edit values between here ->
 * ************************************************/

#define MATALEN (8)
#define MATAWID (8)
#define MATBLEN (8)
#define MATBWID (8)

/***************************************************
 * <- And here
 * ************************************************/

#define MESHLEN (MATBLEN/2)
#define MESHWID (MATAWID/2)
#define MESHHEI (MATALEN/2)

#define RETMATSIZE (MESHLEN * MESHWID)

extern int32_t matrixA[MATAWID][MATALEN];
extern int32_t matrixB[MATBWID][MATBLEN];

extern uint32_t mult_possible;

#endif
