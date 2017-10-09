#include <tinsel.h>
#include "Polite.h"
#include "Heat.h"

int main()
{
  // Point thread structure at base of thread's heap
  PThread<HeatDevice, HeatMessage>* thread =
    (PThread<HeatDevice, HeatMessage>*) tinselHeapBase();
  
  // Invoke interpreter
  thread->run();

  return 0;
}