#ifndef snn_runner_config_hpp
#define snn_runner_config_hpp

#include <POLite.h>

namespace snn
{
    struct runner_config
    {
        Placer::Method placerMethod = Placer::Method::Default;
        unsigned max_time_steps=1000;

        
    };

};

#endif