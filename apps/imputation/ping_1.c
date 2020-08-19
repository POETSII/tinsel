// SPDX-License-Identifier: BSD-2-Clause
// Respond to ping command by incrementing received value by 2

#include "imputation.h"

/*****************************************************
 * Linear Interpolation Node
 * ***************************************************
 * This code performs the linear interpolation for imputation
 * ****************************************************/
 
#define PREVA (1u << 0u)
#define NEXTA (1u << 1u)
#define PREVB (1u << 2u)
#define NEXTB (1u << 3u)

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
    
    // Populate local genetic distances and calculate total genetic distance
    float dmLocal[LINRATIO] = {0.0f};
    float totalDistance = 0.0f;
    
    for (uint32_t x = 0u; x < LINRATIO; x++) {
        
        dmLocal[x] = *(float*)(baseAddress + x);;
        totalDistance += dmLocal[x];
        
    }
    
    uint8_t recFlags = 0u;

/*****************************************************
* Node Functionality
* ***************************************************/
    
    float alpha[LINRATIO] = {0.0f};
    float beta[LINRATIO] = {0.0f};
    float prevAlpha = 0.0f;
    float nextAlpha = 0.0f;
    float prevBeta = 0.0f;
    float nextBeta = 0.0f;
    
    while(1) {
        tinselWaitUntil(TINSEL_CAN_RECV);
        volatile ImpMessage* msgIn = tinselRecv();
        
        // Handle forward messages
        if (msgIn->msgType == FORWARD) {
            
            if (msgIn->match == 1u) {
                prevAlpha = msgIn->val;
                recFlags |= PREVA;
            }
            else {
                nextAlpha = msgIn->val;
                recFlags |= NEXTA;
            }
            
            // If we have received both values for linear interpolation
            if ((recFlags & PREVA) && (recFlags & NEXTA)) {
                
                float totalDiff = prevAlpha - nextAlpha;
                
                for (uint32_t x = 0u; x < LINRATIO; x++) {
                    
                    alpha[x] = prevAlpha - ((dmLocal[x] * totalDistance) * totalDiff);
                    
                }
                
            }
            
        }
        
        // Handle backward messages
        if (msgIn->msgType == BACKWARD) {
        
            if (msgIn->match == 1u) {
                nextBeta = msgIn->val;
                recFlags |= NEXTB;
            }
            else {
                prevBeta = msgIn->val;
                recFlags |= PREVB;
            }
            
            // If we have received both values for linear interpolation
            if ((recFlags & PREVB) && (recFlags & NEXTB)) {
                
                float totalDiff = nextBeta - prevBeta;
                
                for (uint32_t x = 0u; x < LINRATIO; x++) {
                    
                    beta[(LINRATIO - 1u) - x] = nextBeta - ((dmLocal[(LINRATIO - 1u) - x] * totalDistance) * totalDiff);
                    
                }
                
            }
            
            
        }
        
        // Free message slot
        tinselFree(msgIn);
    }


    return 0;
}

