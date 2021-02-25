// SPDX-License-Identifier: BSD-2-Clause
#ifndef snn_tinsel_main_hpp
#define snn_tinsel_main_hpp

#include "models/snn_model_izhikevich_fix.hpp"
#include "stats/stats_minimal.hpp"
#include "runners/hardware_idle_runner.hpp"

#include <tinsel.h>
#include <POLite.h>

#ifndef TINSEL
static_assert(0, "Expected this header to be compiled for TINSEL.");
#endif

template<class RunnerType>
int snn_tinsel_main_impl()
{
    using ThreadType = typename RunnerType::thread_type;

  // Point thread structure at base of thread's heap
  ThreadType* thread = (ThreadType*) tinselHeapBaseSRAM();
  
  // Invoke interpreter
  thread->run();

  return 0;
}

#endif
