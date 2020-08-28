#include "imputation.h"

/*****************************************************
 * Hidden Markov Model and Linear Interpolation Node
 * ***************************************************
 * This code performs the stephens and li model for imputation
 * 
 * USE ARRAYS FOR MATCH / ALPHAS TO PUSH THE PROCESSING TO MESSAGE TIME IN FAVOUR OF POETS
 * ****************************************************/

int main()
{
    // Get thread id
    int me = tinselId();
    uint8_t localThreadID = me % (1 << TinselLogThreadsPerMailbox);
    uint8_t hwRow = localThreadID % 8u;

    // Received values
    uint32_t* baseAddress = tinselHeapBase();
    uint32_t observationNo = *baseAddress;
    uint32_t match = *(baseAddress + 1u);
    uint32_t fwdKey = *(baseAddress + 2u);
    uint32_t bwdKey = *(baseAddress + 3u);
    float bwdSame = *(float*)(baseAddress + 4u);
    float bwdDiff = *(float*)(baseAddress + 5u);
    float fwdSame = *(float*)(baseAddress + 6u);
    float fwdDiff = *(float*)(baseAddress + 7u);

    
    // Get host id
    int host = tinselHostId();
    
   
    // Get pointers to mailbox message slot
    volatile HostMessage* msgHost = tinselSendSlot();

    // Prepare message to send to HMM node
    tinselWaitUntil(TINSEL_CAN_SEND);
    msgHost->msgType = match;
    msgHost->observationNo = observationNo;
    msgHost->stateNo = hwRow;
    msgHost->val = fwdDiff;

    tinselSend(host, msgHost);
    
/*    
    // Startup for forward algorithm
    if (observationNo == (NOOFOBS - 1u)) {
        
        // Get pointers to mailbox message slot
        volatile ImpMessage* msgOut = tinselSendSlot();
        
        // Prepare message to send to HMM node
        tinselWaitUntil(TINSEL_CAN_SEND);
        msgOut->msgType = FORWARD;
        msgOut->stateNo = 69u;
        msgOut->val = 0.0f;
        
        // Propagate to next column
        tinselKeySend(bwdKey, msgOut);
        
        volatile HostMessage* msgHost = tinselSendSlot();
        
        // Prepare message to send to HMM node
        tinselWaitUntil(TINSEL_CAN_SEND);
        msgHost->msgType = bwdKey;
        msgHost->observationNo = observationNo;
        msgHost->stateNo = hwRow;
        msgHost->val = 0.0f;

        tinselSend(host, msgHost);
        
    }
    else if (observationNo == 0u) {
        
        tinselWaitUntil(TINSEL_CAN_RECV);
        volatile ImpMessage* msgIn = tinselRecv();
        
        // Get pointers to mailbox message slot
        volatile HostMessage* msgHost = tinselSendSlot();
        
        // Prepare message to send to HMM node
        tinselWaitUntil(TINSEL_CAN_SEND);
        msgHost->msgType = bwdKey;
        msgHost->observationNo = observationNo;
        msgHost->stateNo = hwRow;
        msgHost->val = 0.0f;
        
        tinselSend(host, msgHost);
        
    }
    else {
        
        tinselWaitUntil(TINSEL_CAN_RECV);
        volatile ImpMessage* msgIn = tinselRecv();
        
        // Get pointers to mailbox message slot
        volatile ImpMessage* msgOut = tinselSendSlot();
        
        // Prepare message to send to HMM node
        tinselWaitUntil(TINSEL_CAN_SEND);
        msgOut->msgType = FORWARD;
        msgOut->stateNo = msgIn->stateNo;
        msgOut->val = msgIn->val;
        
        tinselKeySend(bwdKey, msgOut);
        
        volatile HostMessage* msgHost = tinselSendSlot();

        // Prepare message to send to HMM node
        tinselWaitUntil(TINSEL_CAN_SEND);
        msgHost->msgType = bwdKey;
        msgHost->observationNo = observationNo;
        msgHost->stateNo = hwRow;
        msgHost->val = 0.0f;

        tinselSend(host, msgHost);
        
    }
*/
    return 0;
}

