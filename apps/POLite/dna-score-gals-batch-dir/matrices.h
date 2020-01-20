// SPDX-License-Identifier: BSD-2-Clause
#ifndef _MATRICES_H_
#define _MATRICES_H_

#include <stdint.h>

// Parameters

/***************************************************
 * Edit values between here ->
 * ************************************************/

#define SUBLENGTH (2111)
#define QUERYLENGTH (2044)

/***************************************************
 * <- And here
 * ************************************************/

extern char seqSub[SUBLENGTH];
extern char seqQuery[QUERYLENGTH];

const uint32_t RESLEN = (SUBLENGTH > QUERYLENGTH) ? SUBLENGTH : QUERYLENGTH;

#endif
