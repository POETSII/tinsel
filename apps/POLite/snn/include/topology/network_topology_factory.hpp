#ifndef topology_factory_hpp
#define topology_factory_hpp

#include "network_topology.hpp"
#include "edge_prob_network_topology.hpp"

namespace snn
{

inline std::shared_ptr<NetworkTopology> network_topology_factory(const StochasticSourceNode &source, const boost::json::object &config)
{
    if(!config.contains("type")){
        std::cerr<<"Config = "<<config<<"\n";
        throw std::runtime_error("NetworkTopology config has no 'type' key.");
    }
    std::string type=config.at("type").as_string().c_str();

    if( type == "EdgeProbTopology"){
        return edge_prob_network_topology_factory(source, config);
    }else{
        fprintf(stderr, "Didnt understand network topology type '%s'.\n", type.c_str());
        exit(1);
    }
}

};

#endif