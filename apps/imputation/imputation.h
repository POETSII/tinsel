// SPDX-License-Identifier: BSD-2-Clause
#ifndef _IMPUTATION_H_
#define _IMPUTATION_H_

#include <stdint.h>

// Parameters

/***************************************************
 * Edit values between here ->
 * ************************************************/
 
#define NOOFSTATES (128)
#define NOOFOBS (24)
#define NOOFTARGMARK (4)
#define NE (1000000)
#define ERRORRATE (10000)

typedef struct {
    
    // threadID
    uint32_t threadID;
    // float value
    float val;
    
} ImpMessage;


/***************************************************
 * <- And here
 * ************************************************/

#endif

