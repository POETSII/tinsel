#ifndef snn_model_poisson_hpp
#define snn_model_poisson_hpp

#include <cstdint>

#include "hash.hpp"

namespace snn{
    struct poisson_neuron_model
    {
        struct model_config_type {
            uint32_t seed;
            uint32_t firing_rate_per_step;  
        };

        struct neuron_state_type {
            uint32_t firing_thresh;
            uint64_t rng_state;
        };

        struct weight_type {
            uint16_t inc;
        };

        struct accumulator_type {
            uint16_t acc;
        };

        static void weight_init(const model_config_type &config, uint32_t src, uint32_t dst, weight_type &weight)
        {
            weight.inc = hash_2d_to_uint32(src, dst) & 0xFF;
        }

        static void accumulator_reset(const neuron_state_type &dst_neuron, accumulator_type &acc)
        {
            acc.acc = 0;
        }

        static void accumulator_add_spike(const neuron_state_type &dst_neuron, const weight_type &weight, accumulator_type &acc)
        {
            acc.acc += weight.inc;
        }
        
        static void neuron_init(const model_config_type &config, neuron_state_type &neuron, uint32_t neuron_id)
        {
            neuron.rng_state=hash_2d_to_uint64(config.seed, neuron_id);
            neuron.firing_thresh=config.firing_rate_per_step;
        }

        static bool neuron_step(neuron_state_type &neuron, const accumulator_type &acc)
        {
            uint32_t u=rng64_next_uint32(neuron.rng_state);
            u += acc.acc;
            return u > neuron.firing_thresh;
        }
    };
};

#endif