#include <tinsel.h>

int main()
{
  // Get thread id
  int me = tinselId();

  // Get pointer to a mailbox message slot
  volatile int* msg = tinselSlot(0);

  if (me == 0) {
    printf("me=%x\n", me);
    msg[0] = 0x1234;
    tinselWaitUntil(TINSEL_CAN_SEND);
    tinselSend(128, msg);
  }
  else if (me == 128) {
    printf("me=%x\n", me);
    tinselAlloc(msg);
    tinselWaitUntil(TINSEL_CAN_RECV);
    tinselRecv();
    printf("Received: 0x%x\n", msg[0]);
  }

  return 0;
}
