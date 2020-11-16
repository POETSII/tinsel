// SPDX-License-Identifier: BSD-2-Clause
#include "Pipeline.h"

#include <tinsel.h>
#include <POLite.h>

typedef PThread<
          PipelineDevice,
          PipelineState,    // State
          None,             // Edge label
          PipelineMessage   // Message
        > PipelineThread;

int main()
{
  // Point thread structure at base of thread's heap
  PipelineThread* thread = (PipelineThread*) tinselHeapBaseSRAM();
  
  // Invoke interpreter
  thread->run();

  return 0;
}
