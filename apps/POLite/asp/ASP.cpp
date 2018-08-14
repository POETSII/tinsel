#include <tinsel.h>
#include <POLite.h>
#include "ASP.h"

int main()
{
  // Point thread structure at base of thread's heap
  PThread<ASPDevice, ASPMessage>* thread =
    (PThread<ASPDevice, ASPMessage>*) tinselHeapBaseSRAM();
  
  // Invoke interpreter
  thread->run();

  return 0;
}
