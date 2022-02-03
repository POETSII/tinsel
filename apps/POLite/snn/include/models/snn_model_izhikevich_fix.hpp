#ifndef snn_model_izhikevich_fix_hpp
#define snn_model_izhikevich_fix_hpp

#include <cstdint>
#include <cstdio>
#include <cmath>

#include "hash.hpp"

#ifndef TINSEL
#include "logging.hpp"
#include <boost/json.hpp>

#endif

namespace snn{
    struct fix_t
    {
        static constexpr int FB=16;
        static constexpr float SCALE=float(1<<FB);
        static constexpr int32_t HALF=1<<(FB-1);

        constexpr fix_t()
            : x(0)
        {}

        constexpr explicit fix_t(float _x)
            : x(_x*SCALE)
        {}

        constexpr fix_t(int32_t _x, bool)
            : x(_x)
        {
        }

        int32_t x;

        const fix_t operator+(fix_t o) const
        { return fix_t(x+o.x, true); }

        const fix_t operator-(fix_t o) const
        { return fix_t(x-o.x, true); }

        const bool operator<(fix_t o) const
        { return x<o.x; }

        const fix_t operator*(fix_t o) const
        {
            int64_t tmp=x * int64_t(o.x) + HALF;
            return fix_t{ (int32_t)(tmp>>FB), true};
        }

        explicit operator float() const
        { return x / SCALE; }
    };

    inline fix_t half(fix_t x)
    {
        // TODO : convergent rounding... ?
        // ???0.0 -> ??0    == x>>1
        // ???0.1 -> ??0    == x>>1
        // ???1.0 -> ??1    == x>>1
        // ???1.1 -> ??1+1  == (x>>1)+1
        //                  == (x>>1) + ((x&3)==3)
        // Costs about 4 extra instructions?

        return {x.x>>1, true};
    }

    struct izhikevich_fix_neuron_model
    {


        struct model_config_type {
            uint32_t seed = 1;
            uint32_t excitatory_fraction = 3435973836 ; // == 0.8 Probability of excitatory is ldexp(excitatory,-32)
        };

        struct neuron_state_type {
            // Constants
            fix_t a, b, c, d, Ir;

            // Mutable
            fix_t u, v;
            uint64_t rng_state;
            
        };

        struct weight_type {
            int16_t w;
        };

        struct accumulator_type {
            fix_t acc;
        };

    private:
        static bool is_neuron_excitatory(const model_config_type &config, uint32_t src)
        { return hash_3d_to_bernoulli(config.seed, src, 2, config.excitatory_fraction); }
    public:

        static void neuron_init(const model_config_type &config, uint32_t src, neuron_state_type &state)
        {
            bool is_excitatory = is_neuron_excitatory(config, src);
            fix_t r{ (int32_t)(hash_3d_to_uint32(config.seed, src, 3) >> (32-fix_t::FB)), true };
            state.rng_state = hash_3d_to_uint64(config.seed, src, 1);
            if(is_excitatory){
                fix_t r2=r*r;
                state.a=fix_t{0.02f};
                state.b=fix_t{0.2f};
                state.c=fix_t{-65.0f}+fix_t{15.f}*r2;
                state.d=fix_t{8.f}+fix_t{-6}*r2;
            }else{
                state.a=fix_t{0.02f}+fix_t{0.08f}*r;
                state.b=fix_t{0.25f}+fix_t{0.05f}*r;
                state.c=fix_t{-65.f};
                state.d=fix_t{2};
            }
            state.v=fix_t{-65.0f};
            state.u=state.b*state.v;
            //fprintf(stderr, " excite=%d, r=%f,  %f * %f = %f\n", is_excitatory, (float)r, (float)state.b, (float)state.v, (float)state.u);
        }

        static void weight_init(const model_config_type &config, uint32_t src, uint32_t dst, weight_type &weight)
        {
            bool is_excitatory = is_neuron_excitatory(config, src);
            fix_t r={ (int32_t)(hash_4d_to_uint32(config.seed, src, dst, 4)>>(32-fix_t::FB)), true};
            if(is_excitatory){
                r=r*fix_t{0.5f};
            }else{
                r=fix_t{0.0f}-r;
            }
            weight.w = (r.x)>>1; // Store as 16 bit with 15 fractional bits
        }

        static void weight_init_zero(const model_config_type &config, weight_type &weight)
        {
            weight.w = 0;
        }

        static void accumulator_reset(const neuron_state_type &dst_neuron, accumulator_type &acc)
        {
            acc.acc = fix_t{0,true};
        }

        static void accumulator_add_spike(const neuron_state_type &dst_neuron, const weight_type &weight, accumulator_type &acc)
        {
            acc.acc = acc.acc + fix_t{weight.w<<1, true}; // recover weight from 15 fractional bits
        }
        
        static __attribute__((noinline)) bool neuron_step( neuron_state_type &neuron, const accumulator_type &acc)
        {
            fix_t v = neuron.v;
            fix_t u = neuron.u;
             bool spike=false;

            fix_t I = fix_t{0.0}; // fix_t{rng64_next_grng(neuron.rng_state)}; // TODO!
            I = I + acc.acc;
            I = fix_t{0};
            v = v+half( fix_t{0.04}*(v*v)+fix_t{5}*v+fix_t{140}-u+I); // Step 0.5 ms
            v = v+half( fix_t{0.04}*(v*v)+fix_t{5}*v+fix_t{140}-u+I); // for numerical
            u = u + neuron.a*(neuron.b*v-u);          // stability
           
            if (fix_t{30.0f} < v ) {
                v = neuron.c;
                u = u + neuron.d;
                spike=true;
            }
            neuron.v=v;
            neuron.u=u;
            return spike;
        }


#ifndef TINSEL
        static void log_config(Logger &log, const model_config_type &config)
        {
            auto r=log.with_region("model");
            log.export_value("type", "IzhikevichFix", 2);
            log.export_value("excitatory_fraction_thresh", (int64_t)config.excitatory_fraction, 2);
            log.export_value("excitatory_fraction", ldexp(config.excitatory_fraction,-32), 2);
            log.export_value("seed", (int64_t)config.seed, 2);
        }

        static std::string default_model_config()
        {
            return R"({"type":"IzhikevichFix", "excitatory_fraction":0.8, "seed":1 })";
        }

        static model_config_type parse_config(const boost::json::object &config)
        {
            if(!config.contains("type")){
                throw std::runtime_error("No type attribute.");
            }
            if(config.at("type") != "IzhikevichFix"){
                throw std::runtime_error("Wrong neuron model.");
            }

            model_config_type res;
            double p=config.contains("excitatory_fraction") ? config.at("excitatory_fraction").to_number<double>() : 0.8;
            if(p<0 || 1<p){
                throw std::runtime_error("excitatory_fraction is out of range.");
            }

            res.excitatory_fraction=ldexp(p, 32);
            res.seed=1;

            return res;
        }
#endif

    };
};

#endif
