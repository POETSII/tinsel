// SPDX-License-Identifier: BSD-2-Clause
// Respond to ping command by incrementing received value by 1

#include "imputation.h"

/*****************************************************
 * Hidden Markov Model Node
 * ***************************************************
 * This code performs the stephens and li model for imputation
 * 
 * USE ARRAYS FOR match / ALPHAS TO PUSH THE PROCESSING TO MESSAGE TIME IN FAVOUR OF POETS
 * ****************************************************/
 
#define NULL 0

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
    uint32_t fwdKey = *baseAddress;
    uint32_t bwdKey = *(baseAddress + 1u);
    uint32_t match = *(baseAddress + 2u);
    float fwdSame = *(float*)(baseAddress + 3u);
    float fwdDiff = *(float*)(baseAddress + 4u);
    
/*****************************************************
* Node Functionality
* ***************************************************/
    
    
    // Get host id
    int host = tinselHostId();
    
    if (observationNo == 22u) {
        
        // Get pointers to mailbox message slot
        volatile ImpMessage* msgOut = tinselSendSlot();
        
        msgOut->stateNo = stateNo;
        msgOut->alpha = 69.0f;
        
        tinselWaitUntil(TINSEL_CAN_SEND);
        
        // Propagate to previous column
        tinselKeySend(bwdKey, msgOut);
        
    }
    else if (observationNo == 0u) {
        
        // Get pointers to mailbox message slot
        volatile HostMessage* msgOut = tinselSendSlot();
        volatile ImpMessage* msgIn = NULL;
        
        tinselWaitUntil(TINSEL_CAN_RECV);
        msgIn = tinselRecv();
        
        msgOut->observationNo = observationNo;
        msgOut->stateNo = stateNo;
        msgOut->alpha = msgIn->alpha;
        
        tinselWaitUntil(TINSEL_CAN_SEND);
        
        // Propagate to previous column
        tinselSend(host, msgOut);
        
    }
    else {
        
        // Get pointers to mailbox message slot
        volatile ImpMessage* msgOut = tinselSendSlot();
        volatile ImpMessage* msgIn = NULL;
        
        tinselWaitUntil(TINSEL_CAN_RECV);
        msgIn = tinselRecv();
        
        msgOut->stateNo = stateNo;
        msgOut->alpha = msgIn->alpha;
        
        tinselWaitUntil(TINSEL_CAN_SEND);
        
        // Propagate to previous column
        tinselKeySend(bwdKey, msgOut);
        
    }
    
    
    /*
     //If first row
    if (observationNo == 0u) {
        
        // Calculate initial probability
        float alpha = 1.0f / NOOFSTATES;
        
        // Multiply alpha by emission probability
        if (match == 1u) {
            alpha = alpha * (1.0f - (1.0f / ERRORRATE));
        }
        else {
            alpha = alpha * (1.0f / ERRORRATE);
        }
        
        // Prepare message to send
        tinselWaitUntil(TINSEL_CAN_SEND);
        msgOut->observationNo = observationNo;
        msgOut->stateNo = stateNo;
        msgOut->alpha = alpha;
        
        // Propagate to next column
        tinselKeySend(fwdKey, msgOut);
    
    }
    // If a node in between
    else {
        
        uint8_t recCnt = 0u;
        float alpha = 0.0f;
        
        
        // Accumulate Alpha
        volatile ImpMessage* msgIn = NULL;
        
        for (recCnt = 0u; recCnt < 128u; recCnt++) {
            tinselWaitUntil(TINSEL_CAN_RECV);
            msgIn = tinselRecv();
            
            if (msgIn->stateNo == stateNo) {
                alpha += msgIn->alpha * fwdSame;
            }
            else {
                alpha += msgIn->alpha * fwdDiff;
            }
            
        }
        
        // Multiply Alpha by Emission Probability
        if (match == 1u) {
            alpha = alpha * (1.0f - (1.0f / ERRORRATE));
        }
        else {
            alpha = alpha * (1.0f / ERRORRATE);
        }
        
        // Prepare the alphas for sending
        msgOut->observationNo = observationNo;
        msgOut->stateNo = stateNo;
        msgOut->alpha = alpha;
        
        // If we are an intermediate node propagate the alpha to the next column
        // Else send the alpha out to the host
        if (observationNo != 23u) {
            tinselWaitUntil(TINSEL_CAN_SEND);
            tinselKeySend(fwdKey, msgOut);
        }
        
        tinselFree(msgIn);
    }
    
    tinselWaitUntil(TINSEL_CAN_SEND);
    tinselSend(host, msgOut);
    */
  return 0;
}

