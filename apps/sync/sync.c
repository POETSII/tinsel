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

  for (int i = 0; i < 3; i++) {
    tinselIdle();
    tinselEmit(i);
  }

  if (me == 0) {
    tinselWaitUntil(TINSEL_CAN_SEND);
    msgOut[0] = 100;
    tinselSend(host, msgOut);
  }

  return 0;
}
