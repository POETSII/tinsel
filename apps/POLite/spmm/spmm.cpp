#include <tinsel.h>
#include <POLite.h>
#include "spmm.h"

int main()
{
  // Point thread structure at base of thread's heap
  PThread<SPMMDevice, SPMMMessage>* thread =
    (PThread<SPMMDevice, SPMMMessage>*) tinselHeapBaseSRAM();
  
  // Invoke interpreter
  thread->run();

  return 0;
}
