#ifndef snn_runner_config_hpp
#define snn_runner_config_hpp

#include <POLite.h>

#ifndef TINSEL

#include <string>
#include <algorithm>

#include "ParallelFor.h"

namespace snn
{
    struct RunnerConfig
    {
        PlacerMethod placerMethod = PlacerMethod::Default;
        unsigned max_time_steps=1000;
        ParallelFlag enable_parallel;
    };

};

#endif

#endif