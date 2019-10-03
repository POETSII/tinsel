// SPDX-License-Identifier: BSD-2-Clause
#include "NHood.h"

#include <tinsel.h>
#include <POLite.h>

typedef PThread<
          NHoodDevice,
          NHoodState,    // State
          None,          // Edge label
          NHoodMessage   // Message
        > NHoodThread;

int main()
{
  // Point thread structure at base of thread's heap
  NHoodThread* thread = (NHoodThread*) tinselHeapBaseSRAM();
  
  // Invoke interpreter
  thread->run();

  return 0;
}
