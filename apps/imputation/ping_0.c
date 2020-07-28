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
    uint32_t local = me % (1 << TinselLogThreadsPerMailbox);
    
    // Get host id
    int host = tinselHostId();

    // Get pointers to mailbox message slot
    volatile int* msgOut = tinselSendSlot();

    tinselWaitUntil(TINSEL_CAN_RECV);
    volatile int* msgIn = tinselRecv();
    tinselWaitUntil(TINSEL_CAN_SEND);
    msgOut[0] = 0u;
    msgOut[1] = local;
    tinselFree(msgIn);
    tinselSend(host, msgOut);

  return 0;
}

