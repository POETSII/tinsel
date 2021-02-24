#ifndef snn_stats_minimal_hpp
#define snn_stats_minimal_hpp

#include "stats_concept.hpp"

namespace snn
{

struct stats_minimal
{
    struct neuron_stats
    {
        struct body_t{
            uint16_t time_steps;
            uint16_t total_sends;
            uint32_t total_recvs;
        }body;

        uint32_t fragment_progress;

        void on_sim_start()
        {}

        void on_begin_step(int32_t neuron_time_pre_step)
        {}

        void on_end_step(int32_t neuron_time_pre_step) // This will match the time given to on_begin_step
        {}

        void on_send_spike(int32_t neuron_time)
        { body.total_sends++; }

        void on_recv_spike(int32_t neuron_time, int32_t message_time)
        { body.total_recvs++; }

        void on_sim_finish(int32_t neuron_time)
        { body.time_steps=neuron_time; }


        void reset_fragment()
        { fragment_progress=0; }

        //! Return true if the export is complete, false if more fragments
        template<size_t FragmentWords>
        bool do_fragment_export(uint32_t buffer[FragmentWords]) const
        { return fragment_export<FragmentWords>(fragment_progress, body, buffer); }

        //! Return true if the import is complete, false if more fragments
        template<size_t FragmentWords>
        bool do_fragment_import(const uint32_t buffer[FragmentWords])
        { return fragment_import<FragmentWords>(fragment_progress, body, buffer); }

        // Return true if an export or import has finished (and reset_fragment has not been called)
        bool is_fragment_complete() const
        { return fragment_complete(fragment_progress, body); }
    };
};

};

#endif