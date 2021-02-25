#ifndef snn_model_poisson_hpp
#define snn_model_poisson_hpp

#include <cstdint>
#include <cstdio>
#include <cmath>

#include "hash.hpp"

namespace snn{
    struct izhikevich_neuron_model
    {
        struct model_config_type {
            uint32_t seed;
            uint32_t excitatory_probability; // Probability of excitatory is ldexp(excitatory,-32)
        };

        struct neuron_state_type {
            // Constants
            float a, b, c, d, Ir;

            // Mutable
            float u, v;
            uint64_t rng_state;
            
        };

        struct weight_type {
            int32_t inc;  // stimulus with 16 fractional bits
        };

        struct accumulator_type {
            int32_t acc;  // Current with 16 fractional bits
        };

    private:
        static bool is_neuron_excitatory(const model_config_type &config, uint32_t src)
        { return src < 5; };
        //{ return hash_3d_to_bernoulli(config.seed, src, 2, config.excitatory_probability); }
    public:

        static void neuron_init(const model_config_type &config, uint32_t src, neuron_state_type &state)
        {
            bool is_excitatory = is_neuron_excitatory(config, src);
            float r=hash_3d_to_u01(config.seed, src, 3);
            state.rng_state = hash_3d_to_uint64(config.seed, src, 1);
            if(is_excitatory){
                float r2=r*r;
                state.a=0.02f;
                state.b=0.2f;
                state.c=-65.0f;
                state.c+=15.f*r2;
                state.d=8.f;
                state.d-=6*r2;
            }else{
                state.a=0.02f;
                state.a+=0.08f*r;
                state.b=0.25f;
                state.b-=0.05f*r;
                state.c=-65.f;
                state.d=2;
            }
            state.v=-65.0f;
            state.u=state.b*state.v;
        }

        static void weight_init(const model_config_type &config, uint32_t src, uint32_t dst, weight_type &weight)
        {
            bool is_excitatory = is_neuron_excitatory(config, src);
            float r=hash_4d_to_u01(config.seed, src, dst, 4);
            if(is_excitatory){
                r=r*0.5f;
            }else{
                r=-r;
            }
            weight.inc = int32_t((0.5f * r)*65536.0f);
        }

        static void accumulator_reset(const neuron_state_type &dst_neuron, accumulator_type &acc)
        {
            acc.acc = 0;
        }

        static void accumulator_add_spike(const neuron_state_type &dst_neuron, const weight_type &weight, accumulator_type &acc)
        {
            acc.acc += weight.inc;
        }
        
        static __attribute__((noinline)) bool neuron_step( neuron_state_type &neuron, const accumulator_type &acc)
        {
            float v = neuron.v;
            float u = neuron.u;
             bool spike=false;

            /*
            float I = rng64_next_grng(neuron.rng_state);
            //I += acc.acc * float(1.f / 65536.0f);
            I = 0;
            v = v+0.5*(0.04*v*v+5*v+140-u+I); // Step 0.5 ms
            v = v+0.5*(0.04*v*v+5*v+140-u+I); // for numerical
            u = u + neuron.a*(neuron.b*v-u);          // stability
           
            if (v >= 30.0) {
                v = neuron.c;
                u += neuron.d;
                spike=true;
            }
            */
            neuron.v=0.1f;
            return spike;
        }
    };
};

#endif
