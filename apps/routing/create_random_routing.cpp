#include "POLite/Routing.hpp"

int main()
{
    SystemParameters params;
    initSystemParameters(&params, 4, 4, 4, 4);

    auto system=std::make_unique<SystemNode>(&params);
    validate_node_heirarchy(system.get());

    std::mt19937 rng;
    
    Routing routes(system.get());
    for(unsigned i=0; i<params.get_total_threads()*10; i++){
        routes.add_route( params.pick_random_thread(rng), params.pick_random_thread(rng) );
    }

    auto loads = routes.calculate_link_load();

    std::vector<std::pair<double,const LinkOut *>> sorted;
    for(const auto &kv : loads){
        sorted.emplace_back(kv.second, kv.first);
    }

    std::sort(sorted.begin(), sorted.end());

    for(const auto &kv : sorted){
        std::cout<<kv.second->node->full_name<<":"<<kv.second->name<<" : "<<kv.first<<"\n";
    }
}