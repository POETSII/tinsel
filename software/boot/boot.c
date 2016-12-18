// Tinsel boot loader

#include <tinsel.h>
#include <boot.h>

// See "boot.h" for details of the protocol used between the boot
// loader (running on tinsel threads) and the the host PC.

int main()
{
  // Id for this thread
  uint32_t me = get_my_id();

  while (me & 0xf);

  sim_emit(me);

  for (;;) {
    uint32_t x = from_host();
    sim_emit(x);
    to_host(x+1);
  }

  return 0;
}
