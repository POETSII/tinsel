#include "runners/gals_runner.hpp"
#include "models/snn_model_poisson.hpp"
#include "stats/stats_minimal.hpp"


#include "snn_host_main.hpp"



int main(int argc, char *argv[])
{
    using namespace snn;

    using Runner = GALSRunner<poisson_neuron_model, stats_minimal>;

    return snn_host_main_impl<
        Runner
        >
    (argc, argv);
}
