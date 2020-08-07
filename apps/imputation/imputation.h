// SPDX-License-Identifier: BSD-2-Clause
#ifndef _IMPUTATION_H_
#define _IMPUTATION_H_

#include <stdint.h>
#include <tinsel.h>

// Parameters

/***************************************************
 * Edit values between here ->
 * ************************************************/

// Model Parameters

#define NOOFSTATES (128)
#define NOOFOBS (24)
#define NOOFTARGMARK (24)
#define NE (1000000)
#define ERRORRATE (10000)

// Message Structures

typedef struct {
    
    // threadID
    uint32_t observationNo;
    uint32_t stateNo;
    // float value
    float alpha;
    
} ImpMessage;

typedef struct {
    
    // Observation Number
    uint32_t observationNo;
    // State Number
    uint32_t stateNo;
    
} HostMessage;

// Useful Inline Functions

// Get globally unique thread id of caller
INLINE uint32_t getObservationNumber(uint8_t boardX, uint8_t mailboxX, uint8_t localThreadID)
{
    uint32_t observationNo;
    uint8_t row = 0u;
    
    if (((localThreadID >= 8u) && (localThreadID <= 15)) || ((localThreadID >= 24u) && (localThreadID <= 31))) {
        row = 1u;
    }
    
    observationNo = (boardX * (TinselMailboxMeshXLen) * ((TinselThreadsPerMailbox/2u)/16u)) + (mailboxX * ((TinselThreadsPerMailbox/2u)/16u)) + row;
    
    return observationNo;
}

// Get globally unique thread id of caller
INLINE uint32_t getStateNumber(uint8_t boardY, uint8_t mailboxY, uint8_t localThreadID)
{

    uint8_t threadY;
    uint32_t stateNo;
    
    if (localThreadID < 16u) {
        threadY = localThreadID % 8u;
    }
    else {
        threadY = 8u + (localThreadID % 8u);
    }
                            
    stateNo = (((TinselMeshYLenWithinBox - 1u) - boardY) * (TinselMailboxMeshYLen) * ((TinselThreadsPerMailbox/2u)/2u)) 
                     + (((TinselMailboxMeshYLen - 1u) - mailboxY) * ((TinselThreadsPerMailbox/2u)/2u))
                     + threadY;
    
    return stateNo;
}


/***************************************************
 * <- And here
 * ************************************************/

#endif

