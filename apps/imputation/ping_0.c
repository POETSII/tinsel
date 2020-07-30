// SPDX-License-Identifier: BSD-2-Clause
// Respond to ping command by incrementing received value by 1

#include <tinsel.h>

/*****************************************************
 * Hidden Markov Model Node
 * ***************************************************
 * This code performs the stephens and li model for imputation
 * ****************************************************/
 


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
    uint32_t key = *baseAddress;
    
    // Get host id
    int host = tinselHostId();
    
    // If first row
    if ((row == 0u) && (mailboxX == 0u) && (boardX == 0u)) {
    
        // Get pointers to mailbox message slot
        volatile int* msgOut = tinselSendSlot();

        //tinselWaitUntil(TINSEL_CAN_RECV);
        //volatile int* msgIn = tinselRecv();
        tinselWaitUntil(TINSEL_CAN_SEND);
        msgOut[0] = 69u;
        msgOut[1] = key;
        //tinselFree(msgIn);
        tinselKeySend(key, msgOut);
        //tinselSend(host, msgOut);
    
    }
    
    // EXPECTING THIS TO TRIGGER ON THE SECOND ROW
    // Get pointers to mailbox message slot
    volatile int* msgOut = tinselSendSlot();

    tinselWaitUntil(TINSEL_CAN_RECV);
    volatile int* msgIn = tinselRecv();
    tinselWaitUntil(TINSEL_CAN_SEND);
    msgOut[0] = me;
    msgOut[1] = localThreadID;
    msgOut[2] = row;
    msgOut[3] = mailboxX;
    msgOut[4] = mailboxY;
    msgOut[5] = boardX;
    msgOut[6] = boardY;
    msgOut[7] = msgIn[0];
    tinselFree(msgIn);
    tinselSend(host, msgOut);
    

  return 0;
}

