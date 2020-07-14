// SPDX-License-Identifier: BSD-2-Clause
#ifndef _MODEL_H_
#define _MODEL_H_

#include <stdint.h>

// Parameters

/***************************************************
 * Edit values between here ->
 * ************************************************/

#define NOOFSTATES (8)
#define NOOFOBS (80)
#define NOOFTARGMARK (9)
#define NE (1000000)
#define ERRORRATE (10000)

// Pre-processor Switches
//#define PRINTDIAG (1)

/***************************************************
 * <- And here
 * ************************************************/

extern const uint32_t observation[NOOFTARGMARK][2];
extern const float dm[NOOFOBS-1];
extern const uint8_t hmm_labels[NOOFSTATES][NOOFOBS];

#endif

