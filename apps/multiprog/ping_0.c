// SPDX-License-Identifier: BSD-2-Clause
// Respond to ping command by incrementing received value by 1

#include <tinsel.h>

uint32_t incVal = 1;

int main()
{
  // Get host id
  int host = tinselHostId();

  // Get pointers to mailbox message slot
  volatile int* msgOut = tinselSendSlot();

  tinselWaitUntil(TINSEL_CAN_RECV);
  volatile int* msgIn = tinselRecv();
  tinselWaitUntil(TINSEL_CAN_SEND);
  msgOut[0] = msgIn[0]+incVal;
  tinselFree(msgIn);
  tinselSend(host, msgOut);

  return 0;
}

