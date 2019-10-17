// SPDX-License-Identifier: BSD-2-Clause
#include "matrixmult.h"

#include <tinsel.h>
#include <POLite.h>

typedef PThread<
          MatDevice,
          MatState,    // State
          None,         // Edge label
          MatMessage   // Message
        > MatThread;

int main()
{
  // Point thread structure at base of thread's heap
  MatThread* thread = (MatThread*) tinselHeapBaseSRAM();

  // Invoke interpreter
  thread->run();

  return 0;
}

