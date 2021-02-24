#ifndef hardware_idle_runner_hpp
#define hardware_idle_runner_hpp

#include "../models/snn_model_concept.hpp"
#include "../stats/stats_concept.hpp"

#include "../topology/network_topology.hpp"

#include "POLite.h"

#include "tbb/pipeline.h"

#include "config.h"

namespace snn
{

template<class TNeuronModel=snn::model_concept, class TStats=snn::stats_concept>
struct HardwareIdleRunner
{   
    typedef TNeuronModel neuron_model_t;

    enum MessageTypes{
        Spike = 0,
        Export = 1
    };

    struct message_type
    {
        constexpr static size_t FragmentWords=(1<<TinselLogBytesPerFlit)/4 - 1 - 1;

        uint32_t src : 30;
        MessageTypes type : 2;
        union{
            uint32_t time;
            uint32_t words[FragmentWords];
        };
    };

    struct state_type
    {
        typename TNeuronModel::neuron_state_type neuron_state;
        typename TNeuronModel::accumulator_type accumulator;
        uint32_t time;
        uint32_t max_time;
        uint32_t id;

        typename TStats::neuron_stats stats;
    };

    using edge_type = typename TNeuronModel::weight_type;

    struct device_type
        : PDevice<state_type, edge_type, message_type>
    {
    public:
        using PDevice<state_type, edge_type, message_type>::s;
        using PDevice<state_type, edge_type, message_type>::readyToSend;

        void init()
        {
            s->stats.on_sim_start();
            // nobody fires here. Go straight to step
        }

        void send(message_type *msg)
        {
            switch(readyToSend->index){
            default: assert(0);
            case PPin(0).index:
                s->stats.on_send_spike(s->time);
                msg->src=s->id;
                msg->type=Spike;
                msg->time=s->time;
                *readyToSend=No;
                break;
            case HostPin.index:
                msg->src=s->id;
                msg->type=Export;
                if(s->stats.export_fragment(msg->words)){
                    *readyToSend=No;
                }else{
                    assert(*readyToSend == HostPin);
                }
                break;
            }
        }

        void recv(const message_type *msg, const edge_type *edge)
        {
            assert(msg->type==MessageTypes::Spike);
            neuron_model_t::accumulator_add_spike(s->neuron, *edge, s->accumulator);
            s->stats.on_recv_spike(s->time, msg->time);
        }

        bool step()
        {
            if(s->time < s->max_time){
                s->stats->on_begin_step(s->time);
                bool spike=neuron_model_t::neuron_step(s->neuron_state, s->accumulator);
                neuron_model_t::accumulator_reset(s->neuron_state, s->accumulator);
                s->stats.on_end_step(s->time);
                s->time++;
                *readyToSend = spike ? PPin(0) : No;
                return true;
            }else if(!s->stats.fragment_complete()){
                s->stats.on_end_sim(s->time);
                *readyToSend = HostPin;
                return true;
            }else{
                *readyToSend = No;
                return false;
            }
        }

        bool finish()
        {
            return false;
        }
    };


    unsigned num_neurons;
    typename neuron_model_t::model_config_type model_config;

    PGraph<device_type, state_type, edge_type, message_type> graph;

    void build_graph(const NetworkTopology &topology)
    {
        num_neurons=topology.neuron_count();

        PDeviceId base_id=graph.newDevices(num_neurons);
        assert(base_id==0);

        std::atomic<unsigned> source;
        source=0;

        const auto SpikePin = PPin(MessageTypes::Spike);

        /* Use a pipeline to generate the edges at the same time as putting them
            into the graph.
            TODO : for small graphs this is over-kill - really it should go
            serial if there are few edges.
        */
        using edge_list_t = NetworkTopology::outgoing_range_ptr_t;
        tbb::parallel_pipeline(
            32,
            tbb::make_filter<void,edge_list_t>(
                tbb::filter::parallel, 
                [&](tbb::flow_control& fc) -> edge_list_t {
                    unsigned id=source.fetch_add(1);
                    if(id>=num_neurons){
                        fc.stop();
                        return {};
                    }else{
                        return topology.get_outgoing_range(id);
                    }
                }
            )
            &
            tbb::make_filter<edge_list_t,void>(
                tbb::filter::serial_out_of_order ,
                [&](edge_list_t edges){
                    assert(edges);
                    edge_type E;
                    for(unsigned i=0; i<edges->count; i++){
                        neuron_model_t::weight_init(model_config, edges->source, edges->destinations[i], E);
                        graph.addLabelledEdge(E, edges->source, MessageTypes::Spike, edges->destinations[i]);
                    }
                }
            )
        );
    }

    void collect_output(HostLink &hl)
    {
        std::vector<typename TStats::neuron_stats> stats(num_neurons);
        unsigned complete=0;
        PMessage<message_type> msg;
        while(complete < num_neurons){
            hl.recvMsg(&msg, sizeof(msg));

            if(stats.at(msg.payload.src).template do_fragment_import<message_type::FragmentWords>(msg.payload.words)){
                complete++;
            }
        }
    }
};

};

#endif