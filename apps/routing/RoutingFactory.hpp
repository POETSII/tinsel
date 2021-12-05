#ifndef RoutingFactory_hpp
#define RoutingFactory_hpp


#include "Routing0p6.hpp"
#include "Routing0p8.hpp"


std::unique_ptr<SystemNode> create_system(const SystemParameters *params)
{
    if(params->topology=="tinsel-0.6"){
        return std::make_unique<SystemNode0p6>(params);
    }else if(params->topology=="tinsel-0.8"){
        return std::make_unique<SystemNode0p8>(params);
    }else{
        throw std::runtime_error("Unknown params : "+params->topology);
    }
}



#endif
