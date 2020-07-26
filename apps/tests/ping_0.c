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
    uint32_t local = me % (1 << TinselLogThreadsPerBoard);
    
    
    if (local == 0u) {
        // First node so send message to node 1
        // Get pointers to mailbox message slot
        
        volatile unsigned int* msgOut = tinselSendSlot();
        tinselWaitUntil(TINSEL_CAN_SEND);
        msgOut[0] = 0u;
        tinselSend(local+1u, msgOut);
        
    }
    else if (local == 15u) {
        // Last node so send message back to host
        
        // Reset Cycle Counters
        //tinselPerfCountReset();
        //tinselPerfCountStart();
        
        // Get host id
        int host = tinselHostId();

        // Get pointers to mailbox message slot
        volatile unsigned int* msgOut = tinselSendSlot();

        tinselWaitUntil(TINSEL_CAN_RECV);
        volatile int* msgIn = tinselRecv();
        tinselWaitUntil(TINSEL_CAN_SEND);
        msgOut[0] = msgIn[0] + local;
        tinselFree(msgIn);
        tinselSend(host, msgOut);
    }
    else {
        // Intermediate node so pass message along to next node

        // Get pointers to mailbox message slot
        volatile unsigned int* msgOut = tinselSendSlot();

        tinselWaitUntil(TINSEL_CAN_RECV);
        volatile int* msgIn = tinselRecv();
        tinselWaitUntil(TINSEL_CAN_SEND);
        msgOut[0] = msgIn[0] + local;
        tinselFree(msgIn);
        tinselSend(local+1u, msgOut);
    }

  return 0;
}

