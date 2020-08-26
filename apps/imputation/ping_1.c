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
    float dmLocal[LINRATIO + 1u] = {0.0f};
    float totalDistance = 0.0f;
    
    for (uint32_t x = 0u; x < (LINRATIO + 1u); x++) {
        
        dmLocal[x] = *(float*)(baseAddress + x);;
        totalDistance += dmLocal[x];
        
    }
    
    uint8_t recFlags = 0u;
    
    // Get host id
    int host = tinselHostId();

/*****************************************************
* Node Functionality
* ***************************************************/
    
    float alpha[LINRATIO] = {0.0f};
    float beta[LINRATIO] = {0.0f};
    //float gamma[LINRATIO] = {0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f};
    float prevAlpha = 0.0f;
    float nextAlpha = 0.0f;
    float prevBeta = 0.0f;
    float nextBeta = 0.0f;
    
    uint8_t recCnt = 0u;
    uint32_t matchVal[2];
    
    while(1) {
        tinselWaitUntil(TINSEL_CAN_RECV);
        volatile ImpMessage* msgIn = tinselRecv();
        /*
        //if (msgIn->msgType == BACKWARD) {
            
            matchVal[recCnt] = recCnt; //msgIn->match;
            
            recCnt++;
            if (recCnt == 4u) {
                //multiple times through here
                
                if (observationNo != 23u) {
                    
                    
                    for (uint32_t x = 0u; x < LINRATIO; x++) {
                        
                        // Send to host
                        volatile HostMessage* msgHost = tinselSendSlot();

                        tinselWaitUntil(TINSEL_CAN_SEND);
                        msgHost->msgType = FWDLIN;
                        msgHost->observationNo = observationNo * (LINRATIO + 1u) + 1u + x;
                        msgHost->stateNo = stateNo;
                        msgHost->val = 111111111111.1111f;

                        tinselSend(host, msgHost);
                        
                    }
                    
                    for (uint32_t x = 0u; x < LINRATIO; x++) {
                        
                        // Send to host
                        volatile HostMessage* msgHost = tinselSendSlot();

                        tinselWaitUntil(TINSEL_CAN_SEND);
                        msgHost->msgType = BWDLIN;
                        msgHost->observationNo = observationNo * (LINRATIO + 1u) + 1u + x;
                        msgHost->stateNo = stateNo;
                        msgHost->val = 111111111111.1111f;

                        tinselSend(host, msgHost);
                        
                    }
                
                }
                
            }
        //}*/
        
        
        
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
                    
                    alpha[x] = prevAlpha - ((dmLocal[x] / totalDistance) * totalDiff);
                    prevAlpha = alpha[x];

                }
                
                for (uint32_t x = 0u; x < LINRATIO; x++) {
                    
                    // Send to host
                    volatile HostMessage* msgHost = tinselSendSlot();

                    tinselWaitUntil(TINSEL_CAN_SEND);
                    msgHost->msgType = FWDLIN;
                    msgHost->observationNo = observationNo * (LINRATIO + 1u) + 1u + x;
                    msgHost->stateNo = stateNo;
                    msgHost->val = alpha[x];

                    tinselSend(host, msgHost);
                    
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
                    
                    beta[x] = nextBeta - ((dmLocal[(LINRATIO - 1u) - x] / totalDistance) * totalDiff);
                    nextBeta = beta[x];
                    
                }
                
                for (uint32_t x = 0u; x < LINRATIO; x++) {
                    
                    // Send to host
                    volatile HostMessage* msgHost = tinselSendSlot();

                    tinselWaitUntil(TINSEL_CAN_SEND);
                    msgHost->msgType = BWDLIN;
                    msgHost->observationNo = observationNo * (LINRATIO + 1u) + 1u + x;
                    msgHost->stateNo = stateNo;
                    msgHost->val = beta[x];
                    //msgHost->val = 00000.00000f;

                    tinselSend(host, msgHost);
                    
                }
                
            }
            
            
        }
        
        // Free message slot
        tinselFree(msgIn);
    }


    return 0;
}

