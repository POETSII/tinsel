#ifndef DrawableFactory_hpp
#define DrawableFactory_hpp


#include "Drawable0p6.hpp"
#include "Drawable0p8.hpp"


std::unique_ptr<Drawable> create_drawable(const SystemNode *sys)
{
    auto s0p6=dynamic_cast<const SystemNode0p6*>(sys);
    auto s0p8=dynamic_cast<const SystemNode0p8*>(sys);

    if(s0p6){
        return std::make_unique<System0p6Drawable>(s0p6);
    }
    if(s0p8){
        return std::make_unique<System0p8Drawable>(s0p8);
    }

    throw std::runtime_error("Unknown topology type.");
}



#endif
