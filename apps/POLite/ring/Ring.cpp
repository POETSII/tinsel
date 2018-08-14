#include <tinsel.h>
#include <POLite.h>
#include "Ring.h"

int main()
{
  // Point thread structure at base of thread's heap
  PThread<RingDevice, RingMessage>* thread =
    (PThread<RingDevice, RingMessage>*) tinselHeapBaseSRAM();
  
  // Invoke interpreter
  thread->run();

  return 0;
}
