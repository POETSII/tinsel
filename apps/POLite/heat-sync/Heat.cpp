#include <tinsel.h>
#include <POLite.h>
#include "Heat.h"

int main()
{
  // Point thread structure at base of thread's heap
  PThread<HeatDevice, HeatMessage>* thread =
    (PThread<HeatDevice, HeatMessage>*) tinselHeapBaseSRAM();
  
  // Invoke interpreter
  thread->run();

  return 0;
}
