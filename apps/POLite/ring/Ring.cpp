#include "Ring.h"
#include <tinsel.h>
#include <POLite.h>

typedef PThread<
          RingDevice,
          None,         // Accumulator (small state)
          RingState,    // State
          None,         // Edge label
          RingMessage   // Message
        > RingThread;

int main()
{
  // Point thread structure at base of thread's heap
  RingThread* thread = (RingThread*) tinselHeapBaseSRAM();
  
  // Invoke interpreter
  thread->run();

  return 0;
}
