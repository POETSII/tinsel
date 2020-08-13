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
    /*
    
    float beta = 0.0f;
    
    if (observationNo == 23u) {
      
        // Get pointers to mailbox message slot
        volatile ImpMessage* msgOut = tinselSendSlot();
        
        beta = 1.0f;
        
        tinselWaitUntil(TINSEL_CAN_SEND);
        msgOut->match = match;
        msgOut->stateNo = stateNo;
        msgOut->val = beta;
        
        // Propagate to previous column
        tinselKeySend(bwdKey, msgOut);
        
    }
    else {
      
        // Get pointers to mailbox message slot
        volatile ImpMessage* msgOut = tinselSendSlot();
        
        uint8_t recCnt = 0u;
        uint8_t mtcCnt = 0u;
        
        float emissionProb = 0.0f; 
        
        for (recCnt = 0u; recCnt < 128u; recCnt++) {
            
            tinselWaitUntil(TINSEL_CAN_RECV);
            volatile ImpMessage* msgIn = tinselRecv();
            
            if (msgIn->match == 1u) {
                mtcCnt++;
                emissionProb = (1.0f - (1.0f / ERRORRATE));
            }
            else {
                emissionProb = (1.0f / ERRORRATE);
            }
            
            if (msgIn->stateNo == stateNo) {
                beta += msgIn->val * bwdSame * emissionProb;
                //beta += msgIn->val * bwdSame;
            }
            else {
                beta += msgIn->val * bwdDiff * emissionProb;
                //beta += msgIn->val * bwdDiff;
            }
            
            tinselFree(msgIn);
            
        }
        
        tinselWaitUntil(TINSEL_CAN_SEND);
        msgOut->match = match;
        msgOut->stateNo = stateNo;
        msgOut->val = beta;
        
        if (observationNo != 0u) {
            // Propagate to previous column
            tinselKeySend(bwdKey, msgOut);
        }

    }
    
    volatile HostMessage* msgHost = tinselSendSlot();;
        
    tinselWaitUntil(TINSEL_CAN_SEND);
    msgHost->observationNo = observationNo;
    msgHost->stateNo = stateNo;
    msgHost->val = beta;
    
    tinselSend(host, msgHost);
    */
    
    float alpha = 0.0f;
    
     //If first row
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
        msgHost->observationNo = observationNo;
        msgHost->stateNo = stateNo;
        msgHost->val = alpha;

        tinselSend(host, msgHost);
    
    }
    // If a node in between
    else {
        
        uint8_t recCnt = 0u;
        
        while (1u) {
            tinselWaitUntil(TINSEL_CAN_RECV);
            volatile ImpMessage* msgIn = tinselRecv();
            
            if (msgIn->msgType == FORWARD) {
            
                if (msgIn->stateNo == stateNo) {
                    alpha += msgIn->val * fwdSame;
                }
                else {
                    alpha += msgIn->val * fwdDiff;
                }
                
                recCnt++;
                
                tinselFree(msgIn);
                
            
                if (recCnt == (NOOFSTATES - 1u)) {
                    
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
                        msgOut->stateNo = stateNo;
                        msgOut->val = alpha;
                        
                        tinselKeySend(fwdKey, msgOut);
                    }
                    
                    // Send to host
                    volatile HostMessage* msgHost = tinselSendSlot();

                    tinselWaitUntil(TINSEL_CAN_SEND);
                    msgHost->observationNo = observationNo;
                    msgHost->stateNo = stateNo;
                    msgHost->val = alpha;

                    tinselSend(host, msgHost);
                
                }
            
            }
        
        }
        
    }
    
  return 0;
}

