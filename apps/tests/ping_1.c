// SPDX-License-Identifier: BSD-2-Clause
// Respond to ping command by incrementing received value by 2

#include <tinsel.h>

/*****************************************************
 * Linear Interpolation Node
 * ***************************************************
 * This code performs the linear interpolation for imputation
 * ****************************************************/

int main()
{
    // Reset Cycle Counters
    tinselPerfCountReset();
    tinselPerfCountStart();
    
    // Get host id
    int host = tinselHostId();

    // Get pointers to mailbox message slot
    volatile unsigned int* msgOut = tinselSendSlot();
    
    volatile uint64_t val = 0u;
    for (uint64_t x = 0u; x < 10000000u; x++) {
        val = val + x;
    }

    tinselWaitUntil(TINSEL_CAN_RECV);
    volatile int* msgIn = tinselRecv();
    tinselWaitUntil(TINSEL_CAN_SEND);
    tinselPerfCountStop();
    msgOut[0] = tinselCycleCount();
    msgOut[1] = tinselCycleCountU();
    tinselFree(msgIn);
    tinselSend(host, msgOut);

    return 0;
}

