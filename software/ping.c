#include "tinsel.h"

// Set output bits
inline void set(int bits)
{
  asm volatile("csrs 0x800, %0" : : "r"(bits));
}

// Clear output bits
inline void clear(int bits)
{
  asm volatile("csrc 0x800, %0" : : "r"(bits));
}

// Application-defined message structure
typedef struct {
  int source;  // Id of sender
  int value;   // Value to send
  //short value; // Value to send
} msg_t;

// Main
int main()
{
  int id = me();
  while (id != 0 && id != 16);

  // Pointers into scratchpad for incoming and outgoing msgs
  volatile msg_t* out = mailbox(0);
  volatile msg_t* in  = mailbox(1);

  // Initialise ping message
  out->source = id;
  out->value  = 0;

  // Send initial ping from thread 0 to thread 16
  if (id == 0) {
    while (! mb_can_send());
    mb_send(16, out);
  }

  // Allocate space for incoming message
  mb_alloc(in);

  for (;;) {
    while (! mb_can_recv());
    msg_t* msg = recv();
    set(1 << msg->value);
    while (! mb_can_send());
    out->value = msg->value + 1;
    mb_send(msg->source, out);
    mb_alloc(msg);
  }

  return 0;
}
