// SPDX-License-Identifier: BSD-2-Clause
#include <stdint.h>
#include "model.h"

/***************************************************
 * Edit values between here ->
 * ************************************************/

// Model
// M -> Observable Symbols in HMM
const uint32_t observation[NOOFOBS] = { 0, 1, 2, 3 };

// pi -> Initial Probabilities 
const float init_prob[NOOFSTATES] = { 0.6, 0.3, 0.1 };

// alpha -> Transition Probabilities 
const float trans_prob[NOOFSTATES][NOOFSTATES] = {
    { 0.7, 0.1, 0.2 },
    { 0.3, 0.6, 0.1 },
    { 0.1, 0.3, 0.6 }
};

// b -> Emission Probabilities 
const float emis_prob[NOOFSTATES][NOOFSYM] = {
    { 0.5, 0.3, 0.1, 0.1 },
    { 0.1, 0.1, 0.6, 0.2 },
    { 0.5, 0.1, 0.2, 0.2 }
};

/***************************************************
 * <- And here
 * ************************************************/
