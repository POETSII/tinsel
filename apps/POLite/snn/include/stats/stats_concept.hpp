#ifndef snn_stats_concept_hpp
#define snn_stats_concept_hpp

#include <cstdint>
#include <cstdlib>
#include <cassert>
#include <utility>
#include <algorithm>
#include <cstring>

namespace snn
{

//! This is deliberate, to avoid use of memmove in riscv
void copy_words(volatile uint32_t *dst, const volatile uint32_t *src, size_t n)
{
    for(size_t i=0; i<n; i++){
        dst[i]=src[i];
    }
}

template<size_t FragmentWords,class T>
bool fragment_export(uint32_t &progress, const T &body, volatile uint32_t dst[FragmentWords])
{
    static_assert(sizeof(T)%4==0);
    const size_t BodyWords=sizeof(T)/4;
    const uint32_t *body_begin=(const uint32_t *)&body;
    const uint32_t *body_end=body_begin+BodyWords;
    const uint32_t *body_curr=body_begin+progress;
    assert(body_curr < body_end);
    unsigned todo=std::min<unsigned>(FragmentWords,body_end-body_curr);
    copy_words(dst ,body_curr, todo);
    progress += todo;
    return progress==BodyWords;
}

template<size_t FragmentWords,class T>
bool fragment_import(uint32_t &progress, T &body, const volatile uint32_t src[FragmentWords])
{
    static_assert(sizeof(T)%4==0);
    const size_t BodyWords=sizeof(T)/4;
    uint32_t *body_begin=(uint32_t *)&body;
    uint32_t *body_end=body_begin+BodyWords;
    uint32_t *body_curr=body_begin+progress;
    assert(body_curr < body_end);
    unsigned todo=std::min<unsigned>(FragmentWords,body_end-body_curr);
    std::copy(src, src+todo, body_curr);
    progress += todo;
    return progress==BodyWords;
}

template<class T>
bool fragment_complete(const uint32_t &progress, const T &body)
{
    static_assert(sizeof(T)%4==0);
    const size_t BodyWords=sizeof(T)/4;
    return progress==BodyWords;
}


struct frac16p16
{
    uint32_t x;

    frac16p16()
        : x(0)
    {}

    frac16p16(float _x)
    {
        memcpy(&x, &_x, 4);
    }

    void raw(uint32_t _x)
    {
        x=_x;
    }

    operator float() const
    {
        //return x*(1.0f/65536.0f);
        float r;
        memcpy(&r, &x, 4);
        return r;
    }

    uint32_t bits() const
    {
        uint32_t r;
        memcpy(&r, &x, 4);
        return r;
    }
};

struct stats_concept
{
    struct neuron_stats
    {
        void on_sim_start();
        void on_begin_step(int32_t neuron_time_pre_step);
        void on_end_step(int32_t neuron_time_pre_step); // This will match the time given to on_begin_step
        void on_send_spike(int32_t neuron_time);
        void on_recv_spike(int32_t neuron_time, int32_t message_time);
        void on_sim_finish(int32_t neuron_time);

        void reset_fragment();

        //! Return true if the export is complete, false if more fragments
        template<size_t FragmentWords>
        bool do_fragment_export(uint32_t buffer[FragmentWords]);

        //! Return true if the import is complete, false if more fragments
        template<size_t FragmentWords>
        void do_fragment_import(const uint32_t buffer[FragmentWords]);

        // Return true if an export or import has finished (and reset_fragment has not been called)
        bool fragment_complete();
    };
};



};

#endif