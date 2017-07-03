#include <tinsel.h>

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
    msgOut[0] = msgIn[0]+1;
    tinselSend(host, msgOut);
  }

  return 0;
}

