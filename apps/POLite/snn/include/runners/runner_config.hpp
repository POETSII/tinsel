#ifndef snn_runner_config_hpp
#define snn_runner_config_hpp

#include <POLite.h>

#ifndef TINSEL

#include <string>
#include <algorithm>

namespace snn
{

    inline Placer::Method parse_placer_method(::std::string method){
        std::transform(method.begin(), method.end(), method.begin(), [](char c){ return std::tolower(c); });

        if(method=="metis"){
            return Placer::Method::Metis;
        }else if(method=="bfs"){
            return Placer::Method::BFS;
        }else if(method=="random"){
            return Placer::Method::Random;
        }else if(method=="direct"){
            return Placer::Method::Direct;
        }else if(method=="default"){
            return Placer::Method::Default;
        }else if(method=="metis"){
            return Placer::Method::Metis;
        }else{
            fprintf(stderr, "Unknown placement method '%s'\n", method.c_str());
            exit(1);
        }
    }

    struct RunnerConfig
    {
        Placer::Method placerMethod = Placer::Method::Default;
        unsigned max_time_steps=1000;
    };

};

#endif

#endif
