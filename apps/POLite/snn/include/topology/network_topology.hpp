#ifndef generate_topology_hpp
#define generate_topology_hpp

#include <tbb/concurrent_vector.h>
#include <tbb/parallel_for.h>
#include <tbb/blocked_range.h>
#include <vector>
#include <cstdint>
#include <algorithm>
#include <utility>
#include <numeric>
#include <atomic>
#include "boost/math/distributions/binomial.hpp"
#include "boost/sort/pdqsort/pdqsort.hpp"

#include "stochastic_source.hpp"

#include "boost/json.hpp"

namespace snn
{

class NetworkTopology
{
public:
    using count_type = uint32_t;

    struct outgoing_range_t
    {
        count_type source;
        count_type count;
        const count_type *destinations;
    };

    typedef std::shared_ptr<outgoing_range_t> outgoing_range_ptr_t;
    typedef std::weak_ptr<outgoing_range_t> outgoing_range_weak_ptr_t;

    virtual ~NetworkTopology()
    {}

    virtual std::string topology_description() const=0;

    virtual count_type neuron_count() const =0;

    virtual count_type estimate_max_neuron_fanin() const =0;

    virtual double avg_neuron_fanin() const =0;

    virtual count_type estimate_max_neuron_fanout() const =0;

    virtual double avg_neuron_fanout() const =0;

    virtual outgoing_range_ptr_t get_outgoing_range(count_type src) const =0;

    virtual void get_outgoing_range(count_type src, std::vector<count_type> &destinations) const
    {
        auto pe=get_outgoing_range(src);
        destinations.assign(pe->destinations, pe->destinations+pe->count);
    }
};

inline std::shared_ptr<NetworkTopology> network_topology_factory(boost::json::object config);

};

#endif