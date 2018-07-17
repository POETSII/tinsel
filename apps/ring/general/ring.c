#include <tinsel.h>
#include "ring.h"
#include "layout.h"

int main()
{
  // Get thread id
  int me = tinselId();

  // Get pointer to mailbox message slot
  volatile int* msgOut = tinselSlot(0);

  // Allocate space for some incoming messages
  for (int i = 0; i < 4; i++) tinselAlloc(tinselSlot(i+1));

  // Next thread in ring
  int next = layout[me];

  // If thread is unused, sleep
  if (next < 0) tinselWaitUntil(TINSEL_CAN_RECV);

  // Number of tokens to send
  uint32_t toSend = 0;
  if (me == 0) toSend = NUM_TOKENS;

  // Number of tokens to receive before finishing
  uint32_t toRecv = NUM_LOOPS*NUM_TOKENS;

  while (1) {
    // Termination condition
    if (me == 0 && toRecv == 0 && tinselCanSend()) {
      int host = tinselHostId();
      tinselSend(host, msgOut);
    }
    // Receive
    if (tinselCanRecv()) {
      volatile int* msgIn = tinselRecv();
      tinselAlloc(msgIn);
      if (toRecv > 0) {
        toRecv--;
        toSend++;
      }
    }
    // Send
    if (toSend > 0 && tinselCanSend()) {
      tinselSend(next, msgOut);
      toSend--;
    }
  }

  return 0;
}

