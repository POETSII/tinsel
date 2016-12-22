#include <tinsel.h>
#include <boot.h>

int main()
{
  // Id for this thread
  uint32_t me = myId();

  // Send id to host over host-link
  hostPut(me);

  return 0;
}
