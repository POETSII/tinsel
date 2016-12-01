#include <stdint.h>
#include "tinsel.h"

// Main
int main()
{
  int id = me();
  while (id != 0);

  int host = get_host_id();

  // Pointers into scratchpad for incoming and outgoing msgs
  volatile uint8_t* out = mailbox(0);
  volatile uint8_t* in  = mailbox(1);

  // Allocate space for incoming message
  mb_alloc(in);

  for (;;) {
    // Receive message
    while (! mb_can_recv());
    uint8_t* msg = mb_recv();

    // Send message
    while (! mb_can_send());
    out[0] = msg[0]+1;
    mb_send(host, out);

    // Reallocate space for incoming message
    mb_alloc(msg);
  }

  return 0;
}
