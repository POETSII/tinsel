#include "generate_topology.hpp"

int main()
{
    StochasticSourceNode node;

    unsigned n=1<<16;
    unsigned m=1<<10;
    double p=m/double(n);
    std::cerr<<"exp fanin/out="<<n*p<<"\n";

    EdgeProbNetworkTopology top(n, p, node);
}