#include <tinsel.h>
#include "Polite.h"
#include "Heat.h"

int main()
{
  // Point thread structure at base of thread's heap
  PThread<HeatDevice, HeatMessage>* thread =
    (PThread<HeatDevice, HeatMessage>*) tinselHeapBase();
  
//printf("%x\n", thread->devices[0].t);
//printf("%x\n", thread->devices[0].val >> 16);

  // Invoke interpreter
  thread->run();

  return 0;
}
