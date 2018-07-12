#include <tinsel.h>

#define RING_LENGTH 1024
#define NUM_TOKENS  1000
#define NUM_LOOPS   10000

int main()
{
  // Get thread id
  int me = tinselId();

  // Get pointer to mailbox message slot
  volatile int* msgOut = tinselSlot(0);

  // Allocate space for some incoming messages
  for (int i = 0; i < 4; i++) tinselAlloc(tinselSlot(i+1));

  // Mapping from thread id to ring id
  // (Gains a few % performace!)
  uint32_t id = (me  & 0xffffff00)
              | ((me & 0xf) << 4)
              | ((me >> 4) & 0xf);

  // Next thread in ring
  uint32_t next = id == (RING_LENGTH-1) ? 0 : id+1;

  // Number of tokens to send
  uint32_t toSend = 0;
  if (id == 0) toSend = NUM_TOKENS;

  // Number of tokens to receive before finishing
  uint32_t toRecv = NUM_LOOPS*NUM_TOKENS;

  while (1) {
    // Termination condition
    if (id == 0 && toRecv == 0 && tinselCanSend()) {
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

