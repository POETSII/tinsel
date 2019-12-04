// SPDX-License-Identifier: BSD-2-Clause
#ifndef _MATRICES_H_
#define _MATRICES_H_

#include <stdint.h>

// Parameters

/***************************************************
 * Edit values between here ->
 * ************************************************/

#define MATALEN (3)
#define MATAWID (3)
#define MATBLEN (3)
#define MATBWID (3)

/***************************************************
 * <- And here
 * ************************************************/

#define MESHLEN (MATBLEN/3)
#define MESHWID (MATAWID/3)
#define MESHHEI (MATALEN/3)

#define RETMATSIZE (MESHLEN * MESHWID)

extern int32_t matrixA[MATAWID][MATALEN];
extern int32_t matrixB[MATBWID][MATBLEN];

extern uint32_t mult_possible;

#endif
