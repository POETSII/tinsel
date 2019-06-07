// SPDX-License-Identifier: BSD-2-Clause
#include "ping.h"

#include <tinsel.h>
#include <POLite.h>

typedef PThread<
          PingDevice,
          PingState,     // State
          None,         // Edge label
          PingMessage    // Message
        > PingThread;

int main()
{
  // Point thread structure at base of thread's heap
  PingThread* thread = (PingThread*) tinselHeapBaseSRAM();

  // Invoke interpreter
  thread->run();

  return 0;
}
