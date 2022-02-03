#ifndef snn_model_ihikevich_hpp
#define snn_model_ihikevich_hpp





namespace snn
{
   

    struct izhikevich_model
    {
        struct state_type
        {
            // Random-number-generator state
            rng_type rng;
            // Neuron state
            float u, v;
            uint32_t spikeCount;
            // Neuron properties
            float a, b, c, d, Ir;
        };

        using synapse_type = float;

        using accumulator_type = float;

        static void accumulator_reset(accumulator_type &acc)
        {
            acc=0;
        }

        static void accumulator_add_spike(accumulator_type &acc, const synapse_type &s)
        {
            acc += s;
        }

        static void state_init(uint32_t id, state_type *s)
        {

        }

        static bool state_advance(state_type *s)
        {
            float &v = s->v;
            float &u = s->u;
            float &I = s->Inow;
            I += s->Ir * grng(s->rng);
            v = v+0.5*(0.04*v*v+5*v+140-u+I); // Step 0.5 ms
            v = v+0.5*(0.04*v*v+5*v+140-u+I); // for numerical
            u = u + s->a*(s->b*v-u);          // stability
            bool spike=false;
            if (v >= 30.0) {
                v = s->c;
                u += s->d;
                spike=true;
            }
        }
    };
};

#endif