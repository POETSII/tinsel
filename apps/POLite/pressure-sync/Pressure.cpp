// SPDX-License-Identifier: BSD-2-Clause
#include "Pressure.h"

#include <tinsel.h>
#include <POLite.h>

typedef PThread<
          PressureDevice,
          PressureState,    // State
          Dir,              // Edge label
          PressureMessage   // Message
        > PressureThread;

int main()
{
  // Point thread structure at base of thread's heap
  PressureThread* thread = (PressureThread*) tinselHeapBaseSRAM();
  
  // Invoke interpreter
  thread->run();

  return 0;
}
