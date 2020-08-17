// SPDX-License-Identifier: BSD-2-Clause
// Respond to ping command by incrementing received value by 2

#include "imputation.h"

/*****************************************************
 * Linear Interpolation Node
 * ***************************************************
 * This code performs the linear interpolation for imputation
 * ****************************************************/

int main()
{
    
/*****************************************************
* Node Initialisation
* ***************************************************/
    uint32_t me = tinselId();
    uint8_t localThreadID = me % (1 << TinselLogThreadsPerMailbox);
    
    // Derived Values
    uint8_t mailboxX = (me >> TinselLogThreadsPerMailbox) % (1 << TinselMailboxMeshXBits);
    uint8_t mailboxY = (me >> (TinselLogThreadsPerMailbox + TinselMailboxMeshXBits)) % (1 << TinselMailboxMeshYBits);
    uint8_t boardX = (me >> (TinselLogThreadsPerMailbox + TinselMailboxMeshXBits + TinselMailboxMeshYBits)) % (1 << TinselMeshXBits);
    uint8_t boardY = (me >> (TinselLogThreadsPerMailbox + TinselMailboxMeshXBits + TinselMailboxMeshYBits + TinselMeshXBits)) % (1 << TinselMeshYBits);
    
    // Model location values
    uint32_t stateNo = getStateNumber(boardY, mailboxY, localThreadID);
    uint32_t observationNo = getObservationNumber(boardX, mailboxX, localThreadID);
    
    // Received values
    uint32_t* baseAddress = tinselHeapBase();

/*****************************************************
* Node Functionality
* ***************************************************/

    float dm[LINRATIO] = {0.0f};
    float alpha[LINRATIO] = {0.0f};
    float beta[LINRATIO] = {0.0f};
    
    // Calculate total genetic distance
    
    float totalDistance = 0.0f;
    
    for (uint32_t x = 0u; x < LINRATIO; x++) {
        
        totalDistance += dm[x];
        
    }
    
    
    

    return 0;
}

