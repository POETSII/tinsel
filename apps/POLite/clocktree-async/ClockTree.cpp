// SPDX-License-Identifier: BSD-2-Clause
#include "ClockTree.h"

#include <tinsel.h>
#include <POLite.h>

typedef PThread<
          ClockTreeDevice,
          ClockTreeState,    // State
          None,              // Edge label
          ClockTreeMessage   // Message
        > ClockTreeThread;

int main()
{
  // Point thread structure at base of thread's heap
  ClockTreeThread* thread = (ClockTreeThread*) tinselHeapBaseSRAM();
  
  // Invoke interpreter
  thread->run();

  return 0;
}
