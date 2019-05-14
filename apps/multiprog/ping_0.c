// SPDX-License-Identifier: BSD-2-Clause
// Respond to ping command by incrementing received value by 1

#include <tinsel.h>

uint32_t incVal = 1;

int main()
{
  // Get host id
  int host = tinselHostId();

  // Get pointers to mailbox message slots
  volatile int* msgIn = tinselSlot(0);
  volatile int* msgOut = tinselSlot(1);

  tinselAlloc(msgIn);
  tinselWaitUntil(TINSEL_CAN_RECV);
  tinselRecv();
  tinselWaitUntil(TINSEL_CAN_SEND);
  msgOut[0] = msgIn[0]+incVal;
  tinselSend(host, msgOut);

  return 0;
}

