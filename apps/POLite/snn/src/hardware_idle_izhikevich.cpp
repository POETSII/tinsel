#include "models/snn_model_izhikevich.hpp"
#include "runners/hardware_idle_runner.hpp"
#include "stats/stats_minimal.hpp"
#include "topology/edge_prob_network_topology.hpp"

#include "logging.hpp"

template<class TNeuronModel, class TStats>
void main_impl()
{
    Logger log;

    using namespace snn;

    StochasticSourceNode noise;

    HardwareIdleRunner<TNeuronModel,TStats> runner;
    runner.graph.on_phase_hook=[&](const char *name)
    { log.tag_leaf(name); };

    {
        auto r=log.with_region("prepare");

        log.enter_leaf("create_topology");
        unsigned num_neurons=100000;
        double pEdge=1000.0/num_neurons;
        EdgeProbNetworkTopology net(num_neurons, pEdge, noise);

        log.enter_leaf("build_polite_graph");
        runner.build_graph(net);

        {
            auto r=log.with_region("par");
            runner.graph.map();
        }
    }

    {
        auto r=log.with_region("execute");

        log.enter_region("open_hostlink");
        HostLink hostlink;
        log.exit_region();

        {
            auto r=log.with_region("write");
            runner.graph.write(&hostlink);
        }

        {
            auto r=log.with_region("run");

            {
                auto rr=log.with_region("boot");
                hostlink.boot("code.v", "data.v");
            }

            {
                auto rr=log.with_region("go");
                hostlink.go();
            }

            {
                auto rr=log.with_region("go");
                runner.collect_output(hostlink);
            }
        }
    }
}

int main()
{
    using namespace snn;

    main_impl<
        izhikevich_neuron_model,
        stats_minimal
        >
    ();
}