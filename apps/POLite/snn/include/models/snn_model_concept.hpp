



#ifndef snn_model_concept_hpp
#define snn_model_concept_hpp

#include <cstdint>

namespace snn
{
    struct model_concept
    {
        // All types are expected to be PODs

        using model_config_type = char;
        using neuron_state_type = char;
        using weight_type = char;
        using accumulator_type = char;

        static void weight_init(const model_config_type &config, uint32_t src, uint32_t dst, weight_type &weight);

        /* There may be multiple accumulators per neuron for different time-steps, so these need
            to be independent of the actual state.
        */
        static void accumulator_reset(const neuron_state_type &dst_neuron, accumulator_type &acc);
        static void accumulator_add_spike(const neuron_state_type &dst_neuron, const weight_type &weight, accumulator_type &acc);
        
        static void neuron_init(const model_config_type &config, neuron_state_type &neuron, uint32_t neuron_id);
        static bool neuron_step(neuron_state_type &neuron, const accumulator_type &acc);
    };
};

#endif