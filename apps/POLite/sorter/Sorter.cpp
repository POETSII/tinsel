#include <tinsel.h>
#include <POLite.h>
#include "Sorter.h"

int main()
{
  // Point thread structure at base of thread's heap
  PThread<TwoSorterDevice, TwoSorterMsg>* thread =
    (PThread<TwoSorterDevice, TwoSorterMsg>*) tinselHeapBaseSRAM();
  
  // Invoke interpreter
  thread->run();

  return 0;
}
