#include "Ring.h"
#include <tinsel.h>
#include <POLite.h>


typedef DefaultPThread<RingDevice> RingThread;

int main()
{
  // Point thread structure at base of thread's heap
  RingThread* thread = (RingThread*) tinselHeapBaseSRAM();
  
  // Invoke interpreter
  thread->run();

  return 0;
}
