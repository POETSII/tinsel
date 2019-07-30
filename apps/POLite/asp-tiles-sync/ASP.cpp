// SPDX-License-Identifier: BSD-2-Clause
#include "ASP.h"

#include <tinsel.h>
#include <POLite.h>

typedef PThread<
          ASPDevice,
          ASPState,     // State
          None,         // Edge label
          ASPMessage    // Message
        > ASPThread;

int main()
{
  // Point thread structure at base of thread's heap
  ASPThread* thread = (ASPThread*) tinselHeapBaseSRAM();
  
  // Invoke interpreter
  thread->run();

  return 0;
}
