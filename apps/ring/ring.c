#include <tinsel.h>

#define RING_LENGTH 1024
#define NUM_TOKENS  16
#define NUM_LOOPS   16

int main()
{
  // Get thread id
  int me = tinselId();

  // Get pointer to mailbox message slot
  volatile int* msgOut = tinselSlot(0);

  // Allocate space for some incoming messages
  for (int i = 0; i < 4; i++) tinselAlloc(tinselSlot(i+1));

  // Number of tokens to send
  uint32_t toSend = 0;
  if (me == 0) toSend = NUM_TOKENS;

  // Number of tokens to receive before finishing
  uint32_t toRecv = NUM_LOOPS*NUM_TOKENS;

  // Next thread in ring
  uint32_t next = me == (RING_LENGTH-1) ? 0 : me+1;

  while (1) {
    // Termination condition
    if (me == 0 && toRecv == 0 && tinselCanSend()) {
      int host = tinselHostId();
      tinselSend(host, msgOut);
    }
    // Send
    else if (toSend > 0 && tinselCanSend()) {
      tinselSend(next, msgOut);
      toSend--;
    }
    // Receive
    else if (tinselCanRecv()) {
      volatile int* msgIn = tinselRecv();
      tinselAlloc(msgIn);
      if (toRecv > 0) {
        toRecv--;
        toSend++;
      }
    }
  }

  return 0;
}

