#ifndef snn_stats_minimal_hpp
#define snn_stats_minimal_hpp

#include "stats_concept.hpp"

#ifndef TINSEL
#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/count.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/accumulators/statistics/count.hpp>
#include <boost/accumulators/statistics/mean.hpp>
#include <boost/accumulators/statistics/max.hpp>
#include <boost/accumulators/statistics/min.hpp>
#include <boost/accumulators/statistics/variance.hpp>
#include <boost/accumulators/statistics/skewness.hpp>
#include <boost/accumulators/statistics/kurtosis.hpp>
#endif

#include "models/snn_model_izhikevich_fix.hpp"


namespace snn
{

#ifndef TINSEL
    namespace bs = boost::accumulators;

    using scalar_statistics = bs::accumulator_set<
        double,
        bs::stats<bs::tag::count, bs::tag::mean, bs::tag::min, bs::tag::max, bs::tag::variance, bs::tag::skewness, bs::tag::kurtosis >
    >;

    void scalar_statistics_export_values(const scalar_statistics &stats, const std::string &name, std::function<void(const std::string &name, double value, int level)> &cb, int base_level)
    {
        cb(name+"_count",  bs::count(stats), base_level+1);
        cb(name+"_mean",  bs::mean(stats), base_level);
        cb(name+"_stddev",  sqrt(bs::variance(stats)), base_level+1);
        cb(name+"_skewness",  bs::skewness(stats), base_level+1);
        cb(name+"_kurtosis",  bs::kurtosis(stats), base_level+1);
        cb(name+"_min",  bs::min(stats), base_level+1);
        cb(name+"_max",  bs::max(stats), base_level+1);
    }
#endif

struct stats_minimal
{
    struct neuron_stats
    {
        struct body_t{
            fix_t bibble;
            uint16_t time_steps=0;
            uint16_t total_sends=0;
            uint32_t total_recvs=0;
            uint64_t init_to_finish=0;
            fix_t bobble;
        }body;

        uint32_t fragment_progress;

        void on_sim_start()
        {
#ifdef TINSEL
            body.init_to_finish=(uint64_t(tinselCycleCountU())<<32) +tinselCycleCount();
#endif
        }

        void on_begin_step(int32_t neuron_time_pre_step)
        {
        }

        void on_end_step(int32_t neuron_time_pre_step) // This will match the time given to on_begin_step
        {
        }

        void on_send_spike(int32_t neuron_time)
        { body.total_sends++; }

        void on_recv_spike(int32_t neuron_time, int32_t message_time)
        { body.total_recvs++; }

        void on_sim_finish(int32_t neuron_time)
        {
            body.time_steps=neuron_time;
#ifdef TINSEL
            body.init_to_finish = (uint64_t(tinselCycleCountU())<<32) +tinselCycleCount() - body.init_to_finish;
#endif
        }


        void reset_fragment()
        { fragment_progress=0; }

        //! Return true if the export is complete, false if more fragments
        template<size_t FragmentWords>
         __attribute__((noinline)) bool do_fragment_export(volatile uint32_t buffer[FragmentWords])
        { return fragment_export<FragmentWords>(fragment_progress, body, buffer); }

        //! Return true if the import is complete, false if more fragments
        template<size_t FragmentWords>
        bool do_fragment_import(volatile const uint32_t buffer[FragmentWords])
        { return fragment_import<FragmentWords>(fragment_progress, body, buffer); }

        // Return true if an export or import has finished (and reset_fragment has not been called)
        bool is_fragment_complete() const
        { return fragment_complete(fragment_progress, body); }
    };

#ifndef TINSEL
    struct global_stats
    {
        uint32_t neuron_count = 0;
        
        scalar_statistics time_steps;
        scalar_statistics sent_per_step;
        scalar_statistics recv_per_step;
        scalar_statistics init_to_finish;

        void init(unsigned num_neurons)
        {
            neuron_count=num_neurons;
        }

        void combine(uint32_t neuron_id, const neuron_stats &stats)
        {
            //fprintf(stderr, "%u : time_steps=%u, bibble=%g, bobble=%g, sent=%u, recv=%u\n", neuron_id, stats.body.time_steps, (float)stats.body.bibble, (float)stats.body.bobble, stats.body.total_sends, stats.body.total_recvs);
            time_steps(stats.body.time_steps);
            sent_per_step(stats.body.total_sends / (double)stats.body.time_steps);
            recv_per_step(stats.body.total_recvs / (double)stats.body.time_steps);
            init_to_finish(stats.body.init_to_finish);
        }

        void export_values(std::function<void(const std::string &name, double value, int level)> cb, int base_level)
        {
            scalar_statistics_export_values(time_steps, "time_steps", cb, base_level);
            scalar_statistics_export_values(sent_per_step, "sent_per_step", cb, base_level);
            scalar_statistics_export_values(recv_per_step, "recv_per_step", cb, base_level);
            scalar_statistics_export_values(init_to_finish, "thread_init_to_finish", cb, base_level);
        }
    };
#endif
};

};

#endif
