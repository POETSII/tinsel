#ifndef thread_rng_hpp
#define thread_rng_hpp

#include <cstdint.h>

inline float uniform_bits_to_gaussian(uint32_t ubits)
{
    // Distribution in matlab is:
    // p1_16=ones(1,16)/16; p2_16=conv(p1_16,p1_16); p4_16=conv(p2_16,p2_16); p8_16=conv(p4_16,p4_16);
    // x8_16=0:length(p8_16)-1; x8_16=(x8_16-60)/sqrt(170);
    // First four moments are:
    // mean=0, variance=1, skewness=0, kurtosis=2.8488
    // Weakness is it only produces 121 distinct values, though these are over the range [-4.6,+4.6]

    // sum of four uniforms has mean 8*7.5=60 and variance of 8*85/4=170
    // a four-bit uniform has mean 7.5 and variance ((15-0+1)^2-1)/12 = 85/4
    const float scalef= 0.07669649888473704f; // == 1/sqrt(170)
    const uint32_t mask4=0x0F0F0F0Ful

    // Each statment here is ~2 RISCV instructions
    uint32_t hi=ubits&mask4; // Either a load+and  or lui+li+and (?)
    uint32_t lo=ubits&(mask4<<4);
    uint3_t acc=(hi>>4)+lo;
    acc += (acc>>8);
    acc += (acc>>16);
    int acci=int(acc&0xFF)-60;
    return float(acci) * scalef;
}

/* This is a reversible uniform to uniform hash. It should be very high
    quality, but is a little expensive.

    It is not going to be perfectly random as it is only 32-bits, but
    should be very effective for seeding.

    https://github.com/skeeto/hash-prospector

    Note that it is cheaply invertible using the same structure with different
    constants, if that is every needed.
*/
inline uint32_t hash32_to_uniform_bits(uint32_t x)
{
    // Each xorshift is 2 instr, while each multiply is 3 instr (need 2 to construct 32-bit immediate)
    // So about 15 instructions if inlined
    x ^= x >> 17;
    x *= 0xed5ad4bbu;
    x ^= x >> 11;
    x *= 0xac4c1b51u;
    x ^= x >> 15;
    x *= 0x31848babu;
    x ^= x >> 14;
    return x;
}

/* Produces a unique hash for the pair (a,b). This is asymmetric,
    so hash(a,b) != hash(b,a)

    It is assumed that a and b are effectively IID uniform.
    If a and b are based on linear identifiers they should
    be padded with randomness in the LSBs.

    Is is required that the LSB is 1, in order to avoid over-population
    of zeros in the results.
*/
inline uint32_t pairwise_hash_asymmetric_ubits(uint32_t a, uint32_t b)
{
    assert( (a&b&1) ); // LSBs must be 1
    // We use {20-bit value}+{12-bit zero} as it turns into a single lui
    const uint32_t FIDDLE1=914375u<<12; // arbitrary 20 bit constant
    const uint32_t FIDDLE2=221849u<<12; // arbitrary 20 bit constant
    uint64_t ab=(a+FIDDLE1) * (b+FIDDLE2);
    return (ab>>32) + ab;
}

/* This is a 64-bit generator which is very cheap in RISCV, and
  passes statistical tests for a single thread. It _does_ have
  inter-thread correlations if you are not careful about
  seeding though.
*/
inline uint32_t next_uniform_ubits(uint32_t &x, uint32_t &c)
{
    // about 10 instructions in RISCV
    uint64_t s = x*((uint64_t)4294883355u);  // mulh, mul
    s=s+c; // add, cmp, add
    c=s>>32;
    x=s&0xFFFFFFFFu;
    return x^c; // xor
}


/* This is only a 64-bit generator, so the chance of overlapping
    streams gets large if there are many parallel generators due
    to the birthday paradox.

    The initialisation should be pretty good given distinct thread ids.
*/
struct thread_urng_mwc64x
{
    uint32_t x;
    uint32_t c;

    thread_urng_mwc64x(uint32_t tid)
    {
        x=hash32_to_uniform_bits(id);
        c=hash32_to_uniform_bits(id+0x4000000u);
    }

    uint32_t next_ubits()
    { return next_uniform_bits(x,c); }

    float next_u01()
    { return next_ubits()*2.328306436538696e-10f; }

    float next_gaussian()
    { return uniform_bits_to_gaussian(next_ubits()); }
};

struct thread_urng_dummy
{
    thread_urng_dummy(uint32_t tid)
    {}

    constexpr uint32_t next_ubits()
    { return 0; }

    constexpr float next_u01()
    { return 0.0f; }

    constexpr float next_gaussian()
    { return 0.0f; }
};

using thread_urng_default = thread_urng_dummy;

#endif