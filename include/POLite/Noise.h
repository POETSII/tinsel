#ifndef POLite_Noise_hpp
#define POLite_Noise_hpp

#include <cstdint>

namespace noise
{

    static uint64_t splitmix64(uint64_t x)
    {
        x ^= x >> 30;
        x *= 0xbf58476d1ce4e5b9ull;
        x ^= x >> 27;
        x *= 0x94d049bb133111ebull;
        x ^= x >> 31;
        return x;
    }

    struct MiniRng
    {
        using uint128_t = unsigned __int128;

        uint128_t x;

        MiniRng(uint64_t s)
            : x( (uint128_t(splitmix64(s))<<64)+splitmix64(s) )
        {}

        using result_type = uint64_t;
        static constexpr uint64_t min() { return 0; }
        static constexpr uint64_t max() { return ~uint64_t(0); }

        static const uint128_t M = (uint128_t(0x2360ed051fc65da4ull)<<64) + 0x4385df649fcb5cedull;

        uint64_t operator()()
        {
            x=x*M + 1;
            return x>>64;
        }
    };

}

#endif