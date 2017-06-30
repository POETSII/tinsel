#include <tinsel.h>

int main()
{
  // Get thread id
  int me = tinselId();

  // Get host id
  int host = tinselHostId();

  // Get pointer to a mailbox message slot
  volatile int* msg = tinselSlot(0);

  while (me == 0) {
    tinselAlloc(msg);
    tinselWaitUntil(TINSEL_CAN_RECV);
    tinselRecv();
    tinselWaitUntil(TINSEL_CAN_SEND);
    msg[0] = 0x41424344;
    tinselSend(host, msg);
  }

  return 0;
}

