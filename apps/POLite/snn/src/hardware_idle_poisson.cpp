#include "models/snn_model_poisson.hpp"
#include "stats/stats_minimal.hpp"
#include "runners/hardware_idle_runner.hpp"
#include "topology/edge_prob_network_topology.hpp"

int main()
{
    using namespace snn;

    StochasticSourceNode noise;

    fprintf(stderr, "Creating runner..\n");
    HardwareIdleRunner<poisson_neuron_model,stats_minimal> runner;

    fprintf(stderr, "Creating topoogy..\n");
    unsigned num_neurons=1000000;
    double pEdge=1000.0/num_neurons;
    EdgeProbNetworkTopology net(num_neurons, pEdge, noise);

    fprintf(stderr, "Building graph..\n");
    runner.build_graph(net);

}