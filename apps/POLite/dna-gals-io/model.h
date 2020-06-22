// SPDX-License-Identifier: BSD-2-Clause
#ifndef _MODEL_H_
#define _MODEL_H_

#include <stdint.h>

// Parameters

/***************************************************
 * Edit values between here ->
 * ************************************************/

#define NOOFSTATES (10)
#define NOOFSYM (4)
#define NOOFOBS (10)

/***************************************************
 * <- And here
 * ************************************************/

extern const uint32_t observation[NOOFOBS];
extern const float init_prob[NOOFSTATES];
extern const float trans_prob[NOOFSTATES][NOOFSTATES];
extern const float emis_prob[NOOFSTATES][NOOFSYM];

#endif
