#include <tinsel.h>
#include "PageRank.h"

int main()
{
  PageRankDevice::ThreadType* thread = (PageRankDevice::ThreadType*) tinselHeapBaseSRAM();
  thread->run();
  return 0;
}
