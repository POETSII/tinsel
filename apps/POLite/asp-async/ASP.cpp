#include "ASP.h"

#include <tinsel.h>
#include <POLite.h>

typedef DefaultPThread<ASPDevice> ASPThread;

int main()
{
  // Point thread structure at base of thread's heap
  ASPThread* thread = (ASPThread*) tinselHeapBaseSRAM();
  
  // Invoke interpreter
  thread->run();

  return 0;
}
