#include <tinsel.h>

int main()
{
  // Get thread id
  int me = tinselId();

  // Get host id
  int host = tinselHostId();

  // Get pointer to mailbox message send slot
  volatile int* msgOut = tinselSendSlot();

  while (me == 0) {
    // Receive
    tinselWaitUntil(TINSEL_CAN_RECV);
    volatile int* msgIn = tinselRecv();
    msgOut[0] = msgIn[0]+1;
    tinselFree(msgIn);
    // Respond
    tinselWaitUntil(TINSEL_CAN_SEND);
    tinselSend(host, msgOut);
  }

  return 0;
}

