#ifndef snn_hash_hpp
#define snn_hash_hpp

#include <cstdint>

namespace snn
{
    uint64_t splitmix64(uint64_t x)
    {
        x ^= x >> 30;
        x *= 0xbf58476d1ce4e5b9ull;
        x ^= x >> 27;
        x *= 0x94d049bb133111ebull;
        x ^= x >> 31;
        return x;
    }

    uint32_t hash_2d_to_uint32(uint32_t a, uint32_t b)
    { return splitmix64(splitmix64(a)+b); }

    uint32_t hash_3d_to_uint32(uint32_t a, uint32_t b, uint32_t c)
    { return splitmix64(splitmix64(splitmix64(a)+b)+c); }

    uint32_t hash_4d_to_uint32(uint32_t a, uint32_t b, uint32_t c, uint32_t d)
    { return splitmix64(splitmix64(splitmix64(splitmix64(a)+b)+c)+d); }

    uint64_t hash_2d_to_uint64(uint32_t a, uint32_t b);
    uint64_t hash_3d_to_uint64(uint32_t a, uint32_t b, uint32_t c);

    float hash_3d_to_u01(uint32_t a, uint32_t b, uint32_t c);
    float hash_4d_to_u01(uint32_t a, uint32_t b, uint32_t c, uint32_t d)
    { return hash_4d_to_uint32(a,b,c,d) * 2.3283064365386962890625e-10f; }

    bool hash_2d_to_bernoulli(uint32_t a, uint32_t b, uint32_t probTrue)
    { return hash_2d_to_uint32(a,b)>probTrue; }

    bool hash_3d_to_bernoulli(uint32_t a, uint32_t b, uint32_t c, uint32_t probTrue)
    { return hash_3d_to_uint32(a,b,c)>probTrue; }

    uint32_t rng64_next_uint32(uint64_t &s);
    float rng64_next_grng(uint64_t &s);

};

#endif
