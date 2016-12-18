// Tinsel boot loader

#include <tinsel.h>
#include <boot.h>

int main()
{
  // Id for this thread
  uint32_t me = get_my_id();
  sim_emit(me);

  for (;;) {
    uint32_t x = from_host();
    sim_emit(x);
    to_host(x+1);
  }

  return 0;
}
