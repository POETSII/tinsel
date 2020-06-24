// SPDX-License-Identifier: BSD-2-Clause
#include <tinsel.h>

int main()
{
  // Get thread id
  int me = tinselId();

  // Sample outgoing message
  volatile uint32_t* msgOut = (uint32_t*) tinselSendSlot();
  msgOut[0] = 0x10;
  msgOut[1] = 0x20;
  msgOut[2] = 0x30;
  msgOut[3] = 0x40;
  msgOut[4] = 0x50;
  msgOut[5] = 0x60;
  msgOut[6] = 0x70;
  msgOut[7] = 0x80;

  // On thread 0, send to key supplied by host
  if (me == 0) {
    tinselSetLen(1);
    tinselWaitUntil(TINSEL_CAN_RECV);
    volatile uint32_t* msgIn = (uint32_t*) tinselRecv();
    uint32_t key = msgIn[0];
    tinselFree(msgIn);
    
    tinselWaitUntil(TINSEL_CAN_SEND);
    tinselKeySend(key, msgOut);
  }

  // Print anything received
  while (1) {
    tinselWaitUntil(TINSEL_CAN_RECV);
    volatile uint32_t* msgIn = (uint32_t*) tinselRecv();
    printf("%x %x %x %x %x %x %x %x\n",
        msgIn[0], msgIn[1], msgIn[2], msgIn[3]
      , msgIn[4], msgIn[5], msgIn[6], msgIn[7]);
    tinselFree(msgIn);
  }

  return 0;
}
