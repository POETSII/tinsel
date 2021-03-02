#ifndef gals_runner_hpp
#define gals_runner_hpp

#ifdef POLITE_NUM_PINS
#if (POLITE_NUM_PINS) < 2
#error "Not enough pins"
#endif
#else
#define POLITE_NUM_PINS 2
#endif

#include "../models/snn_model_concept.hpp"
#include "../stats/stats_concept.hpp"

#include "POLite.h"

#ifndef TINSEL
#include "../topology/network_topology.hpp"
#include "tbb/pipeline.h"
#include "logging.hpp"
#include "boost/json.hpp"
#endif

#include "config.h"

#include "runner_config.hpp"

namespace snn
{

template<class TNeuronModel=::snn::model_concept, class TStats=::snn::stats_concept>
struct GALSRunner
{   
    typedef TNeuronModel neuron_model_t;

    enum struct MessageTypes : uint32_t{
        Spike = 0,
        Sync = 1,
        Export = 2
    };

    struct message_type
    {
        //constexpr static size_t FragmentWords=(1<<TinselLogBytesPerFlit)/4 - 1 - 1;
        constexpr static size_t FragmentWords=(1<<TinselLogBytesPerFlit)/4 - 1 - 1;

        uint32_t src;
        MessageTypes type;
        union{
            struct{
                uint32_t time;
                bool spike;
            };
            uint32_t words[FragmentWords];
        };

        //static_assert(sizeof(PMessage<message_type>) <= 1<<TinselLogBytesPerFlit);

    };

    struct state_type
    {
        typename TNeuronModel::neuron_state_type neuron_state;

        uint32_t time;
        uint32_t max_time;
        uint32_t id;
        uint32_t degree;
        bool spike;

        typename TNeuronModel::accumulator_type accumulators[2];
        uint32_t seen[2];

        //std::vector<uint32_t> seen_ever;

        typename TStats::neuron_stats stats;
    };

    using edge_type = typename TNeuronModel::weight_type;

    static constexpr PPin SpikePin = Pin((int)MessageTypes::Spike);
    static constexpr PPin SyncPin = Pin((int)MessageTypes::Sync);

    struct device_type
        : PDevice<state_type, edge_type, message_type>
    {
    private:
        void advance()
        {
            assert(s->seen[0]==s->degree);
            s->stats.on_begin_step(s->time);
            s->spike=neuron_model_t::neuron_step(s->neuron_state, s->accumulators[0]);
            s->accumulators[0]=s->accumulators[1];
            s->seen[0]=s->seen[1];
            neuron_model_t::accumulator_reset(s->neuron_state, s->accumulators[1]);
            s->seen[1]=0;
            s->stats.on_end_step(s->time);
            s->time++;
        }

        void calc_rts()
        {
            assert(*readyToSend == No);
            assert(s->time <= s->max_time);
            if( s->seen[0] == s->degree ){
                if(s->time == s->max_time){
                    s->stats.on_sim_finish(s->time);
                    *readyToSend = HostPin ;
                }else{
                    *readyToSend = SpikePin ;
                }
            }
        }
    public:
        using PDevice<state_type, edge_type, message_type>::s;
        using PDevice<state_type, edge_type, message_type>::readyToSend;

        void init()
        {
            s->stats.on_sim_start();
            s->seen[0]=s->degree;
           *readyToSend=SpikePin;
        }

        void send(volatile message_type *msg)
        {
            msg->src=s->id;
            if(*readyToSend==HostPin){
                msg->type=MessageTypes::Export;
                if(s->stats.template do_fragment_export<message_type::FragmentWords>(msg->words)){
                    *readyToSend=No;
                }else{
                    assert(*readyToSend == HostPin);
                }
            }else{
                if(*readyToSend==SpikePin){
                    advance();
                    if(s->spike){
                        s->stats.on_send_spike(s->time);
                    }
                    msg->spike=s->spike;
                    msg->type=MessageTypes::Spike;
                    *readyToSend=SyncPin;
                    s->spike=false;
                }else{
                    assert(s->spike==false);
                    msg->spike=false;
                    msg->type=MessageTypes::Sync;
                    *readyToSend=No;
                    calc_rts();
                }
                
                msg->time=s->time;
            }
        }

       void recv(const volatile message_type *msg, const edge_type *edge)
        {
            assert(msg->type==MessageTypes::Spike || msg->type==MessageTypes::Sync);
            assert(msg->time==s->time || msg->time==s->time+1);
            bool ahead = msg->time > s->time;
            if(msg->spike){
                neuron_model_t::accumulator_add_spike(s->neuron_state, *edge, s->accumulators[ahead]);
                s->stats.on_recv_spike(s->time, msg->time);
            }
            assert(s->seen[ahead] < s->degree);
            s->seen[ahead]++;
            if(ahead){
                // do nothing
            }else{
                if(*readyToSend==No){
                    calc_rts();
                }
            }
        }

        bool step()
        {
            //printf("  %u : step, s->seen[0]=%u, s->t=%u, degree=%u\n", s->id, s->seen[0], s->time, s->degree);
            return false;
        }

        bool finish(message_type *msg)
        {
            return false;
        }
    };

    using thread_type = PThread<
          device_type,
          state_type,    // State
          edge_type,             // Edge label
          message_type       // Message
        >;

    #ifndef TINSEL

    GALSRunner(int boxesX, int boxesY)
        : graph(boxesX, boxesY)
    {}

    unsigned num_neurons = -1;
    typename neuron_model_t::model_config_type model_config;

    PGraph<device_type, state_type, edge_type, message_type> graph;

    void parse_neuron_config(const std::string &s, Logger &logger)
    {
        auto c=boost::json::parse(s).as_object();
        model_config=neuron_model_t::parse_config(c);
        neuron_model_t::log_config(logger, model_config);
    }

    void build_graph(const ::snn::RunnerConfig &config, const NetworkTopology &topology, std::vector<state_type> &states, Logger &logger)
    {
        auto region=logger.with_region("topology_to_pgraph");

        num_neurons=topology.neuron_count();

        logger.tag_leaf("nodes");
        PDeviceId base_id=graph.newDevices(num_neurons);
        assert(base_id==0);

        states.resize(num_neurons);
        for(unsigned i=0; i<num_neurons;i++){
            states[i].time=0;
            states[i].max_time=config.max_time_steps;
            states[i].id=i;    
            neuron_model_t::neuron_init(model_config, i, states[i].neuron_state);
        }

        logger.tag_leaf("edges");

        std::atomic<unsigned> source;
        source=0;

        const auto SpikePin = Pin((int)MessageTypes::Spike);
        const auto SyncPin = Pin((int)MessageTypes::Sync);

        std::atomic<uint64_t> devices_done=0;
        double tStart=logger.now();
        double tLastPrint=tStart;
        double tPrintDelta=10;
        std::mutex print_lock;

        std::vector<tbb::concurrent_vector<uint32_t>> inverse(num_neurons);
        parallel_for_with_grain<unsigned>(config.enable_parallel, 0, num_neurons, 1024, [&](unsigned i){
            inverse[i].reserve(topology.estimate_max_neuron_fanin());
        });

        edge_type Ezero;
        neuron_model_t::weight_init_zero(model_config, Ezero);

        /* We first build all the forward (true) edges
        */
        parallel_for_with_grain<unsigned>(config.enable_parallel, 0, num_neurons, 16, [&](unsigned id){
            auto edges = topology.get_outgoing_range(id);
            edge_type E;

            graph.reserveOutgoingEdgeSpace(edges->source, (int)MessageTypes::Spike, edges->count);
            for(unsigned i=0; i<edges->count; i++){
                neuron_model_t::weight_init(model_config, edges->source, edges->destinations[i], E);
                graph.addLabelledEdgeLockedDst(E, edges->source, (int)MessageTypes::Spike, edges->destinations[i]);

                inverse[edges->destinations[i]].push_back(edges->source);

                //fprintf(stderr, "spike : %u -> %u\n", id, edges->destinations[i]);
            }


            states[id].degree = edges->count;

            uint64_t done=devices_done.fetch_add(1, std::memory_order_relaxed)+1;
            if((done&0xFFF)==0){
                std::unique_lock<std::mutex> lk(print_lock);
                double tNow=logger.now();
                if(tNow-tLastPrint > tPrintDelta){
                    logger.export_value( ("time_to_add_spike_"+std::to_string(devices_done)).c_str(), tNow-tStart, 1);
                    logger.flush();

                    tLastPrint=tNow;
                    tPrintDelta=std::min(60.0, 1.5*tPrintDelta);
                }
            }
        });

        // We dont try to de-duplicate, as it could be expensive. In a sparse graph it doesnt matter much, as the intersection will be small

        // Then add all the backwards (ghost) edges
        parallel_for_with_grain<unsigned>(config.enable_parallel, 0, num_neurons, 64, [&](unsigned id){
            const auto &srcs=inverse[id];
            graph.reserveOutgoingEdgeSpace(id, (int)MessageTypes::Sync, srcs.size());
            for(unsigned dst : srcs){
                graph.addLabelledEdgeLockedDst(Ezero, id, (int)MessageTypes::Sync, dst);
                //fprintf(stderr, "sync : %u -> %u\n", id, dst);
            }

            states[id].degree += srcs.size();

            uint64_t done=devices_done.fetch_add(1, std::memory_order_relaxed)+1;
            if((done&0xFFF)==0){
                std::unique_lock<std::mutex> lk(print_lock);
                double tNow=logger.now();
                if(tNow-tLastPrint > tPrintDelta){
                    logger.export_value( ("time_to_add_sync_"+std::to_string(devices_done)).c_str(), tNow-tStart, 1);
                    logger.flush();

                    tLastPrint=tNow;
                    tPrintDelta=std::min(60.0, 1.5*tPrintDelta);
                }
            }
        });
    }

    void collect_output(const RunnerConfig &config, HostLink &hl, Logger &logger)
    {
        typename TStats::global_stats all;
        all.begin(num_neurons);

        std::vector<typename TStats::neuron_stats> stats(num_neurons);
        unsigned complete=0;
        PMessage<message_type> msg;
        while(complete < num_neurons){
            hl.recvMsg(&msg, sizeof(msg));
            if(stats.at(msg.payload.src).template do_fragment_import<message_type::FragmentWords>(msg.payload.words)){
                complete++;
            }
        }

        all.end();

        for(unsigned i=0; i<stats.size(); i++){
            all.combine(i, stats[i]);
        }

        all.export_values([&](const std::string &name, double value, int level){
            logger.export_value(name.c_str(), value, level);
        }, 1);
    }

    #endif
};

};

#endif