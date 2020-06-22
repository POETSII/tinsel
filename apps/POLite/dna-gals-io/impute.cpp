// SPDX-License-Identifier: BSD-2-Clause
#include "impute.h"

#include <tinsel.h>
#include <POLite.h>

typedef PThread<
          ImpDevice,
          ImpState,    // State
          None,         // Edge label
          ImpMessage   // Message
        > ImpThread;

int main()
{
  // Point thread structure at base of thread's heap
  ImpThread* thread = (ImpThread*) tinselHeapBaseSRAM();

  // Invoke interpreter
  thread->run();

  return 0;
}

