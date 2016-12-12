#include <stdint.h>
#include <tinsel.h>

// Main
int main()
{
  int host = get_host_id();

  int nums[16];
  for (int i = 0; i < 16; i++) nums[i] = i+100;

  // Pointers into scratchpad for incoming and outgoing msgs
  volatile uint8_t* out = mailbox(0);
  volatile uint8_t* in  = mailbox(1);

  // Allocate space for incoming message
  mb_alloc(in);

  for (;;) {
    // Receive message
    //while (! mb_can_recv());
    mb_wait_until(CAN_RECV);
    uint8_t* msg = mb_recv();

    // Send message
    //while (! mb_can_send());
    mb_wait_until(CAN_SEND);
    //out[0] = msg[0]+1;
    out[0] = nums[msg[0]];
    mb_send(host, out);

    // Reallocate space for incoming message
    mb_alloc(msg);
  }

  return 0;
}
