#ifndef snn_model_poisson_hpp
#define snn_model_poisson_hpp

#include <cstdint>

#include "hash.hpp"

#ifndef TINSEL
#include "logging.hpp"
#include <boost/json.hpp>
#endif

namespace snn{
    struct poisson_neuron_model
    {
        struct model_config_type {
            uint32_t seed;
            uint32_t firing_rate_per_step;  // Expressed as fixed point with 32 fractional bits.
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

        static void weight_init_zero(const model_config_type &config, weight_type &weight)
        {
            weight.inc = 0;
        }

        static void accumulator_reset(const neuron_state_type &dst_neuron, accumulator_type &acc)
        {
            acc.acc = 0;
        }

        static void accumulator_add_spike(const neuron_state_type &dst_neuron, const weight_type &weight, accumulator_type &acc)
        {
            acc.acc += weight.inc;
        }
        
        static void neuron_init(const model_config_type &config, uint32_t neuron_id, neuron_state_type &neuron)
        {
            neuron.rng_state=hash_2d_to_uint64(config.seed, neuron_id);
            neuron.firing_thresh=config.firing_rate_per_step;
        }

        static bool neuron_step(neuron_state_type &neuron, const accumulator_type &acc)
        {
            uint32_t u=rng64_next_uint32(neuron.rng_state);
            u += acc.acc;
            return u <= neuron.firing_thresh;
        }

        #ifndef TINSEL
        static std::string default_model_config()
        {
            return R"({"type":"Poisson", "firing_rate_per_step":0.01, "seed":1 })";
        }

        static void log_config(Logger &log, const model_config_type &config)
        {
            auto r=log.with_region("model");
            log.export_value("type", "Poisson", 2);
            log.export_value("firing_rate_per_step_thresh", (int64_t)config.firing_rate_per_step, 2);
            log.export_value("firing_rate_per_step", ldexp(config.firing_rate_per_step,-32), 2);
            log.export_value("seed", (int64_t)config.seed, 2);
        }

        static model_config_type parse_config(const boost::json::object &config)
        {
            if(!config.contains("type")){
                throw std::runtime_error("No type attribute.");
            }
            if(config.at("type") != "Poisson"){
                throw std::runtime_error("Wrong neuron model.");
            }

            model_config_type res;
            double p=config.contains("firing_rate_per_step") ? config.at("firing_rate_per_step").to_number<double>() : 0.01;
            if(p<0 || 1<p){
                throw std::runtime_error("excitatory_fraction is out of range.");
            }

            res.firing_rate_per_step=ldexp(p, 32);
            if(p>=1-ldexp(1,-31)){
                res.firing_rate_per_step=0xFFFFFFFFul;
            }
            res.seed=1;

            return res;
        }
        #endif
    };
};

#endif