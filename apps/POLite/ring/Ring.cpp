#include <tinsel.h>
#include <POLite.h>
#include "Ring.h"

typedef PThread<
          RingDevice,
          PEmpty,
          RingState,
          PEmpty,
          RingMessage>
        RingThread;

int main()
{
  // Point thread structure at base of thread's heap
  RingThread* thread = (RingThread*) tinselHeapBaseSRAM();
  
  // Invoke interpreter
  thread->run();

  return 0;
}
