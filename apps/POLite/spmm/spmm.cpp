// #ifndef TINSEL
// #define TINSEL
// #endif

#include <tinsel.h>
#include <POLite.h>
#include "spmm.h"

int main()
{
  // Point thread structure at base of thread's heap
  InterruptiblePThread<SPMMDevice>* thread = (InterruptiblePThread<SPMMDevice>*) tinselHeapBaseSRAM();
  
  // Invoke interpreter
  thread->run();

  return 0;
}
