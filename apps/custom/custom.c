// SPDX-License-Identifier: BSD-2-Clause
#include <tinsel.h>

int main()
{
  // Get thread id
  int me = tinselId();

  // Get host id
  int host = tinselHostId();

  // Get accelerator id
  int acc = tinselAccId(0, 0, 0, 0);

  // Get pointers to mailbox message slot
  volatile int* msgOut = tinselSendSlot();

  // This message goes to the accelerator
  // The accelerator replies to the address in the first word of the
  // message, which we specify as the host address
  msgOut[0] = host;

  if (me == 0) {
    tinselWaitUntil(TINSEL_CAN_SEND);
    tinselSend(acc, msgOut);
  }

  return 0;
}

