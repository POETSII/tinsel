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

  uint32_t startTime = tinselCycleCount();
  for (int i = 0; i < 40000; i++) {
    int r = tinselIdle(0);
    tinselEmit(r);
  }
  uint32_t endTime = tinselCycleCount();

  if (me == 0) {
    tinselWaitUntil(TINSEL_CAN_SEND);
    msgOut[0] = endTime - startTime;
    tinselSend(host, msgOut);
  }

  return 0;
}
