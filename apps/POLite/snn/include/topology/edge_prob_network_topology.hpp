#ifndef edge_prob_network_topology_hpp
#define edge_prob_network_generate_topology_hpp

#include "network_topology.hpp"

#include "stochastic_source.hpp"

namespace snn
{

class EdgeProbNetworkTopology
    : public NetworkTopology
{
private:
    count_type m_num_neurons;
    double m_connection_prob;
    boost::math::binomial_distribution<> m_edges_per_neuron_dist;
    count_type m_expected_max_connections_per_neuron;
    double m_average_connections_per_neuron;
    double m_unif_bits_to_index;

    StochasticSourceNode m_noise;

    mutable std::vector<outgoing_range_weak_ptr_t> m_cache;

    void enum_outgoing_neurons(count_type src, std::vector<count_type> &dest) const
    {
        auto node_noise=m_noise.get_sub_source(src);
        auto rng=node_noise.get_generator();

        dest.resize(0);

        if(m_connection_prob>=1.0-16*DBL_EPSILON){
            // Construct directly in place
            dest.resize(m_num_neurons);
            std::iota(dest.begin(), dest.end(), 0ull);

        }else if(m_connection_prob >= 0.125){
            uint64_t thresh_bits=ldexp(m_connection_prob,64);
            for(count_type dst=0; dst<m_num_neurons; dst++){
                if(rng() >= thresh_bits){
                    dest.push_back(dst);
                }
            }
        }else{
            // We get this by inversion, as it should be implementation independent...
            double u=ldexp(rng(), -64);
            count_type n=std::floor(boost::math::quantile(m_edges_per_neuron_dist,u));
            n=std::min<count_type>(n, m_num_neurons);

            while(dest.size() < n){
                for(count_type i=dest.size(); i<n; i++){
                    count_type dst=rng()*m_unif_bits_to_index;
                    dst=std::min<count_type>(dst, m_num_neurons-1);

                    dest.push_back(dst);
                }

                boost::sort::pdqsort_branchless(dest.begin(), dest.end());
                dest.erase(std::unique(dest.begin(), dest.end()), dest.end());
            }

            if(dest.size() > n){
                while(dest.size() > n){
                    double scale=ldexp(dest.size(), -64);
                    count_type index=std::floor(rng()*scale);
                    if(index<dest.size()){
                        dest[index]=dest.back();
                        dest.resize(dest.size()-1);
                    }
                }
                boost::sort::pdqsort_branchless(dest.begin(), dest.end());
            }
            
            assert(dest.size()==n);
        }
    }

    struct outgoing_range_vector_t
    {
        std::vector<count_type> storage;
        outgoing_range_t range;
    };
public:
    EdgeProbNetworkTopology(count_type num_neurons, double connection_prob, const StochasticSourceNode &_noise)
        : m_num_neurons(num_neurons)
        , m_connection_prob(connection_prob)
        , m_edges_per_neuron_dist(num_neurons, connection_prob)
        , m_noise(_noise)
        , m_unif_bits_to_index(ldexp(num_neurons,-64))
        , m_cache(num_neurons)
    {
        m_average_connections_per_neuron=boost::math::mean(m_edges_per_neuron_dist); 
        m_expected_max_connections_per_neuron=std::ceil(boost::math::quantile(m_edges_per_neuron_dist, 0.5/num_neurons));
    }

    count_type neuron_count() const override
    { return m_num_neurons; }

    count_type estimate_max_neuron_fanin() const override
    { return m_expected_max_connections_per_neuron; }

    double avg_neuron_fanin() const override
    { return m_average_connections_per_neuron; }

    count_type estimate_max_neuron_fanout() const override
    { return m_expected_max_connections_per_neuron; }

    double avg_neuron_fanout() const override
    { return m_average_connections_per_neuron; }

    outgoing_range_ptr_t get_outgoing_range(count_type src) const override
    {
        outgoing_range_ptr_t res = m_cache.at(src).lock();
        if(!res){
            auto pVec=std::make_shared<outgoing_range_vector_t>();
            pVec->storage.reserve(m_expected_max_connections_per_neuron);
            enum_outgoing_neurons(src, pVec->storage);
            pVec->range.source=src;
            pVec->range.count=pVec->storage.size();
            pVec->range.destinations= pVec->storage.empty() ? nullptr : &pVec->storage[0];

            res=std::shared_ptr<outgoing_range_t>( pVec, &pVec->range );

            outgoing_range_weak_ptr_t wp=res;
            m_cache[src] = wp;
        }
        return res;
    }    

};

inline std::shared_ptr<NetworkTopology> edge_prob_network_topology_factory(const boost::json::object &config)
{
    if(config.at("type") != "EdgeProbTopology"){
        throw std::runtime_error("wrong config type.");
    }

    if(!config.contains("nNeurons")){
        throw std::runtime_error("topology config missing key 'nNeurons'");
    }
    uint64_t nNeurons=config.at("nNeurons").as_uint64();

    if(!config.contains("pConnect")){
        throw std::runtime_error("topology config missing key 'pConnect'");
    }
    double pConnect=config.at("pConnect").as_double();
    if(pConnect <0 || 1 < pConnect){
        throw std::runtime_error("pConnect is outside [0,1]");
    }

    uint64_t seed=1;
    if(config.contains("seed")){
        seed=config.at("seed").as_uint64();
    }

    StochasticSourceNode source(seed);

    return std::make_shared<EdgeProbNetworkTopology>(nNeurons, pConnect, source);
}

};

#endif