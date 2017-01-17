#include <tinsel.h>

int main()
{
  // Id for this thread
  uint32_t me = tinselId();

  // Send id to host over host-link
  tinselHostPut(me);

  return 0;
}
