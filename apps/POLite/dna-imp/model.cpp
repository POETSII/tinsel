// SPDX-License-Identifier: BSD-2-Clause
#include <stdint.h>
#include "model.h"

/***************************************************
 * Edit values between here ->
 * ************************************************/

// Model

// HMM States Labels
const uint8_t hmm_labels[NOOFSTATES][NOOFOBS] = {
    { 0, 0, 1, 1, 1, 0, 1, 0, 1, 1 },
    { 0, 0, 1, 0, 0, 0, 0, 0, 1, 1 },
    { 0, 0, 1, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 1, 0, 0, 1, 0, 1, 0, 0, 1, 0 },
    { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0 },
    { 0, 0, 0, 1, 0, 1, 0, 0, 0, 0 },
    { 0, 1, 0, 1, 0, 1, 0, 0, 0, 0 },
    { 1, 1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 1, 0, 0, 0, 0, 0, 1, 0, 0 }
};

// M -> Observable Symbols in HMM
const uint32_t observation[NOOFTARGMARK][2] = {
    { 0, 0 },
    { 9, 0 }
};

// dm -> Genetic Distances
const float dm[NOOFOBS] = { 
    0.0000006352228, 
    0.0000008341337, 
    0.0000002999896, 
    0.0000045218641, 
    0.0000004620745, 
    0.0000000135741, 
    0.0000006157206, 
    0.0000004930816, 
    0.0000008833401 
};

/*
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
*/

/***************************************************
 * <- And here
 * ************************************************/
