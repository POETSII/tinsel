#include <tinsel.h>
#include <boot.h>

int main()
{
  // Id for this thread
  uint32_t me = myId();

  // Emit id to console in simulation
  simEmit(me);

  // Send id to host over host-link
  hostPut(me);

  // Don't return
  for (;;);

  return 0;
}
