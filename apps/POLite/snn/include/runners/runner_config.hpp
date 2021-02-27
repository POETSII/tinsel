#ifndef snn_runner_config_hpp
#define snn_runner_config_hpp

#include <POLite.h>

#ifndef TINSEL

#include <string>
#include <algorithm>

namespace snn
{

    inline PlacerMethod parse_placer_method(::std::string method){
        std::transform(method.begin(), method.end(), method.begin(), [](char c){ return std::tolower(c); });

        if(method=="metis"){
            return PlacerMethod::Metis;
        }else if(method=="bfs"){
            return PlacerMethod::BFS;
        }else if(method=="random"){
            return PlacerMethod::Random;
        }else if(method=="direct"){
            return PlacerMethod::Direct;
        }else if(method=="default"){
            return PlacerMethod::Default;
        }else if(method=="metis"){
            return PlacerMethod::Metis;
        }else{
            fprintf(stderr, "Unknown placement method '%s'\n", method.c_str());
            exit(1);
        }
    }

    struct RunnerConfig
    {
        PlacerMethod placerMethod = PlacerMethod::Default;
        unsigned max_time_steps=1000;
    };

};

#endif

#endif
