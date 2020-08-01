// SPDX-License-Identifier: BSD-2-Clause
#ifndef _MODEL_H_
#define _MODEL_H_

//#include <stdint.h>
#include "imputation.h"
// Parameters

/***************************************************
 * Edit values between here ->
 * ************************************************/

// Pre-processor Switches
//#define PRINTDIAG (1)

extern const uint32_t observation[NOOFTARGMARK][2];
extern const float dm[NOOFOBS-1];
extern const uint8_t hmm_labels[NOOFSTATES][NOOFOBS];

/***************************************************
 * <- And here
 * ************************************************/

#endif

