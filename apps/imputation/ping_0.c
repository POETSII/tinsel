// SPDX-License-Identifier: BSD-2-Clause
// Respond to ping command by incrementing received value by 1

#include <tinsel.h>

/*****************************************************
 * Hidden Markov Model Node
 * ***************************************************
 * This code performs the stephens and li model for imputation
 * ****************************************************/
 
#define NULL 0


int main()
{
    
    uint32_t me = tinselId();
    
    uint8_t localThreadID = me % (1 << TinselLogThreadsPerMailbox);
    uint8_t row = 0u;
    
    if (((localThreadID >= 8u) && (localThreadID <= 15)) || ((localThreadID >= 24u) && (localThreadID <= 31))) {
        row = 1u;
    }
    
    uint8_t mailboxX = (me >> TinselLogThreadsPerMailbox) % (1 << TinselMailboxMeshXBits);
    uint8_t mailboxY = (me >> (TinselLogThreadsPerMailbox + TinselMailboxMeshXBits)) % (1 << TinselMailboxMeshYBits);
    uint8_t boardX = (me >> (TinselLogThreadsPerMailbox + TinselMailboxMeshXBits + TinselMailboxMeshYBits)) % (1 << TinselMeshXBits);
    uint8_t boardY = (me >> (TinselLogThreadsPerMailbox + TinselMailboxMeshXBits + TinselMailboxMeshYBits + TinselMeshXBits)) % (1 << TinselMeshYBits);
    
    uint32_t* baseAddress = tinselHeapBase();
    uint32_t key = baseAddress[0];
    
    
    // Get host id
    int host = tinselHostId();
    
    // Get pointers to mailbox message slot
    volatile int* msgOut = tinselSendSlot();
    
     //If first row
    if ((row == 0u) && (mailboxX == 0u) && (boardX == 0u)) {

        //tinselWaitUntil(TINSEL_CAN_RECV);
        //volatile int* msgIn = tinselRecv();
        tinselWaitUntil(TINSEL_CAN_SEND);
        msgOut[0] = me;
        msgOut[1] = 0u;
        //tinselFree(msgIn);
        tinselKeySend(key, msgOut);
        //tinselSend(host, msgOut);
    
    }
    // If last row
    else if ((row == 1u) && (mailboxX == 3u) && (boardX == 2u)) {

        tinselWaitUntil(TINSEL_CAN_RECV);
        volatile int* msgIn = tinselRecv();
        tinselWaitUntil(TINSEL_CAN_SEND);
        msgOut[0] = me;
        msgOut[1] = msgIn[1] + 1u;
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
        msgOut[0] = me;
        msgOut[1] = msgIn[1] + 1u;
        tinselFree(msgIn);
        tinselKeySend(key, msgOut);
        
    }

  return 0;
}

