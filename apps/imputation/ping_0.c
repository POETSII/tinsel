// SPDX-License-Identifier: BSD-2-Clause
// Respond to ping command by incrementing received value by 1

#include "imputation.h"

/*****************************************************
 * Hidden Markov Model Node
 * ***************************************************
 * This code performs the stephens and li model for imputation
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
    uint32_t key = *baseAddress;
    uint32_t match = *(baseAddress + 1u);
    float same = *(float*)(baseAddress + 2u);
    float diff = *(float*)(baseAddress + 3u);
    
/*****************************************************
* Node Functionality
* ***************************************************/
    
    
    // Get host id
    int host = tinselHostId();
    
    // Get pointers to mailbox message slot
    volatile ImpMessage* msgOut = tinselSendSlot();
    
     //If first row
    if (observationNo == 0u) {

        //tinselWaitUntil(TINSEL_CAN_RECV);
        //volatile int* msgIn = tinselRecv();
        tinselWaitUntil(TINSEL_CAN_SEND);
        msgOut->threadID = observationNo;
        //tinselFree(msgIn);
        tinselKeySend(key, msgOut);
        //tinselSend(host, msgOut);
    
    }
    // If last row
    else if (observationNo == 23u) {

        tinselWaitUntil(TINSEL_CAN_RECV);
        volatile int* msgIn = tinselRecv();
        tinselWaitUntil(TINSEL_CAN_SEND);
        msgOut->threadID = stateNo;
        msgOut->val = match;
        tinselFree(msgIn);
        tinselSend(host, msgOut);
    
    }
    // If a node in between
    else {
        
        uint8_t recCnt = 0u;
        
        volatile int* msgIn = NULL;
        
        for (recCnt = 0u; recCnt < 128u; recCnt++) {
            tinselWaitUntil(TINSEL_CAN_RECV);
            msgIn = tinselRecv();
        }
        
        tinselWaitUntil(TINSEL_CAN_SEND);
        msgOut->threadID = me;
        tinselFree(msgIn);
        tinselKeySend(key, msgOut);
        
    }

  return 0;
}

