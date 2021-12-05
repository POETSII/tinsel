#include "Routing.hpp"

int main()
{
    SystemParameters params;
    initSystemParameters(&params, 6,6, 3, 3);

    auto system=std::make_unique<SystemNode>(&params);
    validate_node_heirarchy(system.get());

    std::mt19937 rng;
    
    Routing routes(system.get());
    for(unsigned i=0; i<params.get_total_threads()*10; i++){
        routes.add_route( params.pick_random_thread(rng), params.pick_random_thread(rng) );
    }

    auto loads = routes.calculate_link_load();

    double max_load=0;
    for(const auto &kv : loads){
        max_load=std::max(max_load, kv.second);
    }


    std::unordered_map<const LinkOut *,std::vector<std::string>> properties;
    char buff[64]={0};
    for(const auto &kv : loads){
        int p=floor(kv.second / max_load * 255);
        snprintf(buff, sizeof(buff)-1,"color=\"#%02x%02x%02x\"",p,255-p,0);
        properties[kv.first].push_back(buff);
        properties[kv.first].push_back("penwidth=5");
        
    }

    print_node_heirarchy_as_dot_no_links(std::cout, system.get(), properties);
}