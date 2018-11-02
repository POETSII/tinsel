#include "PageRank.h"

#include <tinsel.h>
#include <POLite.h>

typedef PThread<
          PageRankDevice,
          PageRankState,    // State
          None,             // Edge label
          PageRankMessage   // Message
        > PageRankThread;

int main()
{
  // Point thread structure at base of thread's heap
  PageRankThread* thread = (PageRankThread*) tinselHeapBaseSRAM();
  
  // Invoke interpreter
  thread->run();

  return 0;
}
