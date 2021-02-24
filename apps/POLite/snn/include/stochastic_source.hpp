#ifndef stochastic_source_hpp
#define stochastic_source_hpp

#include <cstdint>
#include <cstdlib>

/* This is something of an abomination. It defines a recursive heirarchy of 
    stochastic source, such that each node in the heirarchy provides:

    - A indexed set of child stochastic nodes, accessible in O(1) time
    - A 1d set of random values, acessible in O(1) time
    - A 2d set of random values, accessible in O(1) time
    - A single stream of random numbers, accessible in O(1) time
*/

class StochasticSourceGen
{
private:
    using uint128_t = unsigned __int128;

    uint128_t x;

public:
    StochasticSourceGen(uint64_t a, uint64_t b)
        : x((uint128_t(a+b)<<64)|(a-b+1))
    {}

    using result_type = uint64_t;

    static constexpr uint64_t min()
    { return 0; }

    static constexpr uint64_t max()
    { return ~uint64_t(0); }

    uint64_t operator()()
    {
        static const uint128_t M = (uint128_t(0x2360ed051fc65da4ull)<<64) + 0x4385df649fcb5cedull;
        x=x*M + 1;
        return x>>64;
    }
};

class StochasticSourceNode
{
private:
    uint64_t x, y;

    uint64_t splitmix64(uint64_t x) const
    {
        x ^= x >> 30;
        x *= 0xbf58476d1ce4e5b9ull;
        x ^= x >> 27;
        x *= 0x94d049bb133111ebull;
        x ^= x >> 31;
        return x;
    }
public:
    StochasticSourceNode(uint64_t _x=0x123456789abcdeull)
        : x(_x)
        , y(splitmix64(_x))
    {}


    StochasticSourceNode(uint64_t _x, uint64_t _y)
        : x(_x)
        , y(_y)
    {}

    StochasticSourceNode get_sub_source(size_t index) const
    {
        return {splitmix64(x+index*4591136446195781615ull), splitmix64(y+index*10001159622029045764ull)};
    }

    uint64_t get_sub_value(size_t i1) const
    {
        return splitmix64(x+i1*13996646434549430221ull) + splitmix64(y+i1*15042195478859648586ull);
    }

    uint64_t get_sub_value(size_t i1, size_t i2) const
    {
        return splitmix64(x+i1*11360731194352354631ull) + splitmix64(y+i2*9626042508353951179ull);
    }

    StochasticSourceGen get_generator() const
    {
        return {x, y};
    }
};


#endif
