// SPDX-License-Identifier: BSD-2-Clause
#ifndef _IMPUTATION_H_
#define _IMPUTATION_H_

#include <stdint.h>
#include <tinsel.h>

// Parameters

/***************************************************
 * Edit values between here ->
 * ************************************************/

// Hardware Parameters
#define NOOFHWROWS (8)
#define NOOFHWCOLSPERCORE (2)
#define NOOFBOXES (8)
#define NOOFHWCOLS (6144)

#define NOOFLEGS (2)

// Model Parameters
#define NOOFSTATES (80)
#define NOOFSTATEPANELS (NOOFSTATES / NOOFHWROWS)
//NOTARGMARK = 768 * NOOFBOXES
#define NOOFTARGMARK (NOOFHWCOLS * NOOFLEGS)
// NOOFOBS = (NOOFTARGMARK * 10) - (LINRATIO - 1)
#define NOOFOBS (122871)
#define TEMPNOOFOBS (61431)
#define LINRATIO (10)
#define NE (1000000)
#define ERRORRATE (10000)


// MsgTypes
#define FORWARD (0u)
#define BACKWARD (1u)
#define FWDLIN (2u)
#define BWDLIN (3u)
#define COUNTS (4u)

#define NULL (0u)

#define NEXTLINODEOFFSET (32u)
#define PREVLINODEOFFSET (24u)

// Message Structures

typedef struct {
    
    // Spacer for multicast key
    uint16_t blank;
    // Message Type
    uint16_t msgType;
    // Float value
    float val;
    // If Matched
    uint16_t match;
    // Leg / HW Level
    uint16_t leg;
    // State number
    uint32_t stateNo;
    
    
} ImpMessage;

typedef struct {
    
    // Message Type
    uint32_t msgType;
    // Float value
    float val;
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
    
    if (((localThreadID >= 8u) && (localThreadID <= 15)) || ((localThreadID >= 24u) && (localThreadID <= 31)) || ((localThreadID >= 40u) && (localThreadID <= 47u)) || ((localThreadID >= 56u) && (localThreadID <= 63))) {
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
    
    if ((localThreadID < 16u) || (localThreadID >= 32u && localThreadID < 48u)) {
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

