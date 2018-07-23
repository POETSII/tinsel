// Respond to ping command by incrementing received value by 2

#include <tinsel.h>

uint32_t incVal = 2;

int main()
{
  // Get thread id
  int me = tinselId();

  // Get host id
  int host = tinselHostId();

  // Get pointers to mailbox message slots
  volatile int* msgIn = tinselSlot(0);
  volatile int* msgOut = tinselSlot(1);

  while (me == 0) {
    tinselAlloc(msgIn);
    tinselWaitUntil(TINSEL_CAN_RECV);
    tinselRecv();
    tinselWaitUntil(TINSEL_CAN_SEND);
    msgOut[0] = msgIn[0]+incVal;
    tinselSend(host, msgOut);
  }

  return 0;
}

