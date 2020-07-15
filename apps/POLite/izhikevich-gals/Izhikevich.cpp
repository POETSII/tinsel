// SPDX-License-Identifier: BSD-2-Clause
#include "Izhikevich.h"

#include <tinsel.h>
#include <POLite.h>

typedef PThread<
          IzhikevichDevice,
          IzhikevichState,    // State
          Weight,             // Edge label
          IzhikevichMsg       // Message
        > IzhikevichThread;

int main()
{
  // Point thread structure at base of thread's heap
  IzhikevichThread* thread = (IzhikevichThread*) tinselHeapBaseSRAM();
  
  // Invoke interpreter
  thread->run();

  return 0;
}
