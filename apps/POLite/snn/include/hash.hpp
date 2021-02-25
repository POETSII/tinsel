#ifndef snn_hash_hpp
#define snn_hash_hpp

#include <cstdint>

namespace snn
{
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
        const uint32_t mask4=0x0F0F0F0Ful;

        // Each statment here is ~2 RISCV instructions
        uint32_t hi=ubits&mask4; // Either a load+and  or lui+li+and (?)
        uint32_t lo=ubits&(mask4<<4);
        uint32_t acc=(hi>>4)+lo;
        acc += (acc>>8);
        acc += (acc>>16);
        int acci=int(acc&0xFF)-60;
        return float(acci) * scalef;
    }

    inline uint64_t splitmix64(uint64_t x)
    {
        x ^= x >> 30;
        x *= 0xbf58476d1ce4e5b9ull;
        x ^= x >> 27;
        x *= 0x94d049bb133111ebull;
        x ^= x >> 31;
        return x;
    }

    inline uint32_t hash_2d_to_uint32(uint32_t a, uint32_t b)
    { return splitmix64(splitmix64(a)+b); }

    inline uint32_t hash_3d_to_uint32(uint32_t a, uint32_t b, uint32_t c)
    { return splitmix64(splitmix64(splitmix64(a)+b)+c); }

    inline uint32_t hash_4d_to_uint32(uint32_t a, uint32_t b, uint32_t c, uint32_t d)
    { return splitmix64(splitmix64(splitmix64(splitmix64(a)+b)+c)+d); }

    inline uint64_t hash_2d_to_uint64(uint32_t a, uint32_t b)
    { return splitmix64(splitmix64(a)+b); }

    inline uint64_t hash_3d_to_uint64(uint32_t a, uint32_t b, uint32_t c)
    { return splitmix64(splitmix64(splitmix64(a)+b)+c); }

    inline float hash_3d_to_u01(uint32_t a, uint32_t b, uint32_t c)
    { return hash_3d_to_uint32(a,b,c) * 2.3283064365386962890625e-10f; }

    inline float hash_4d_to_u01(uint32_t a, uint32_t b, uint32_t c, uint32_t d)
    { return hash_4d_to_uint32(a,b,c,d) * 2.3283064365386962890625e-10f; }

    inline bool hash_2d_to_bernoulli(uint32_t a, uint32_t b, uint32_t probTrue)
    { return hash_2d_to_uint32(a,b)>probTrue; }

    inline bool hash_3d_to_bernoulli(uint32_t a, uint32_t b, uint32_t c, uint32_t probTrue)
    { return hash_3d_to_uint32(a,b,c)>probTrue; }

    

    inline uint32_t rng64_next_uint32(uint64_t &s)
    {
        uint32_t c=s>>32, x=s&0xFFFFFFFFul;
        s = x*((uint64_t)4294883355u) + c;
        return x ^ c;
    }

    inline float rng64_next_grng(uint64_t &s)
    {
        return uniform_bits_to_gaussian(rng64_next_uint32(s));
    }

};

#endif
