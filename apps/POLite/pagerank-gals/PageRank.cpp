#include <tinsel.h>
#include <POLite.h>
#include "PageRank.h"

int main()
{
  // Point thread structure at base of thread's heap
  PThread<PageRankDevice, PageRankMessage>* thread =
    (PThread<PageRankDevice, PageRankMessage>*) tinselHeapBaseSRAM();
  
  // Invoke interpreter
  thread->run();

  return 0;
}
