// SPDX-License-Identifier: BSD-2-Clause
// Respond to ping command by incrementing received value by 1

#include "imputation.h"

/*****************************************************
 * Hidden Markov Model Node
 * ***************************************************
 * This code performs the stephens and li model for imputation
 * 
 * USE ARRAYS FOR MATCH / ALPHAS TO PUSH THE PROCESSING TO MESSAGE TIME IN FAVOUR OF POETS
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
    uint32_t fwdKey = *baseAddress;
    uint32_t bwdKey = *(baseAddress + 1u);
    uint32_t match = *(baseAddress + 2u);
    float fwdSame = *(float*)(baseAddress + 3u);
    float fwdDiff = *(float*)(baseAddress + 4u);
    float bwdSame = *(float*)(baseAddress + 5u);
    float bwdDiff = *(float*)(baseAddress + 6u);
    
/*****************************************************
* Node Functionality
* ***************************************************/
    
    
    // Get host id
    int host = tinselHostId();
    
    float alpha = 0.0f;
    float beta = 0.0f;

    
    // Startup for forward algorithm
    if (observationNo == 0u) {
        
        // Get pointers to mailbox message slot
        volatile ImpMessage* msgOut = tinselSendSlot();
        
        // Calculate initial probability
        alpha = 1.0f / NOOFSTATES;
        
        // Multiply alpha by emission probability
        if (match == 1u) {
            alpha = alpha * (1.0f - (1.0f / ERRORRATE));
        }
        else {
            alpha = alpha * (1.0f / ERRORRATE);
        }
        
        // Prepare message to send
        tinselWaitUntil(TINSEL_CAN_SEND);
        msgOut->msgType = FORWARD;
        msgOut->stateNo = stateNo;
        msgOut->val = alpha;
        
        // Propagate to next column
        tinselKeySend(fwdKey, msgOut);
        
        // Send to host
        volatile HostMessage* msgHost = tinselSendSlot();

        tinselWaitUntil(TINSEL_CAN_SEND);
        msgHost->msgType = FORWARD;
        msgHost->observationNo = observationNo;
        msgHost->stateNo = stateNo;
        msgHost->val = alpha;

        tinselSend(host, msgHost);
    
    }
    
    // Startup for backward algorithm
    if (observationNo == (NOOFTARGMARK - 1u)) {
      
        // Get pointers to mailbox message slot
        volatile ImpMessage* msgOut = tinselSendSlot();
        
        beta = 1.0f;
        
        tinselWaitUntil(TINSEL_CAN_SEND);
        msgOut->msgType = BACKWARD;
        msgOut->match = match;
        msgOut->stateNo = stateNo;
        msgOut->val = beta;
        
        // Propagate to previous column
        tinselKeySend(bwdKey, msgOut);
        
        volatile HostMessage* msgHost = tinselSendSlot();

        tinselWaitUntil(TINSEL_CAN_SEND);
        msgHost->msgType = BACKWARD;
        msgHost->observationNo = observationNo;
        msgHost->stateNo = stateNo;
        msgHost->val = beta;

        tinselSend(host, msgHost);
        
    }
        
    uint8_t fwdRecCnt = 0u;
    uint8_t bwdRecCnt = 0u;
    
    while (1u) {
        tinselWaitUntil(TINSEL_CAN_RECV);
        volatile ImpMessage* msgIn = tinselRecv();
        
        // Handle forward messages
        if (msgIn->msgType == FORWARD) {
            
            fwdRecCnt++;
        
            if (msgIn->stateNo == stateNo) {
                alpha += msgIn->val * fwdSame;
            }
            else {
                alpha += msgIn->val * fwdDiff;
            }
            
            if (fwdRecCnt == (NOOFSTATES - 1u)) {
                
                // Multiply Alpha by Emission Probability
                if (match == 1u) {
                    alpha = alpha * (1.0f - (1.0f / ERRORRATE));
                }
                else {
                    alpha = alpha * (1.0f / ERRORRATE);
                }
                
                // If we are an intermediate node propagate the alpha to the next column
                // Else send the alpha out to the host
                if (observationNo != 23u) {
                    
                    // Get pointers to mailbox message slot
                    volatile ImpMessage* msgOut = tinselSendSlot();
                    
                    tinselWaitUntil(TINSEL_CAN_SEND);
                    msgOut->msgType = FORWARD;
                    msgOut->stateNo = stateNo;
                    msgOut->val = alpha;
                    
                    tinselKeySend(fwdKey, msgOut);
                }
                
                // Send to host
                volatile HostMessage* msgHost = tinselSendSlot();

                tinselWaitUntil(TINSEL_CAN_SEND);
                msgHost->msgType = FORWARD;
                msgHost->observationNo = observationNo;
                msgHost->stateNo = stateNo;
                msgHost->val = alpha;

                tinselSend(host, msgHost);
            
            }
        
        }
        
        // Handle forward messages
        if (msgIn->msgType == BACKWARD) {
            
            bwdRecCnt++;
            
            float emissionProb = 0.0f;
         
            if (msgIn->match == 1u) {
                emissionProb = (1.0f - (1.0f / ERRORRATE));
            }
            else {
                emissionProb = (1.0f / ERRORRATE);
            }
            
            if (msgIn->stateNo == stateNo) {
                
                beta += msgIn->val * bwdSame * emissionProb;
                
            }
            else {
                
                beta += msgIn->val * bwdDiff * emissionProb;
                
            }
            
            if (bwdRecCnt == (NOOFSTATES - 1u)) {
                
                if (observationNo != 0u) {
                    
                    // Get pointers to mailbox message slot
                    volatile ImpMessage* msgOut = tinselSendSlot();

                    tinselWaitUntil(TINSEL_CAN_SEND);
                    msgOut->msgType = BACKWARD;
                    msgOut->match = match;
                    msgOut->stateNo = stateNo;
                    msgOut->val = beta;
                    
                    // Propagate to previous column
                    tinselKeySend(bwdKey, msgOut);
                }
                
                volatile HostMessage* msgHost = tinselSendSlot();;

                tinselWaitUntil(TINSEL_CAN_SEND);
                msgHost->msgType = BACKWARD;
                msgHost->observationNo = observationNo;
                msgHost->stateNo = stateNo;
                msgHost->val = beta;

                tinselSend(host, msgHost);
            
            }
            
        }
        
        // Free message slot
        tinselFree(msgIn);
    
    }
    
  return 0;
}

