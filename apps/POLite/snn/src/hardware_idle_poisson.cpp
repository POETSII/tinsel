#include "snn_host_main.hpp"

#include "runners/hardware_idle_runner.hpp"
#include "models/snn_model_poisson.hpp"
#include "stats/stats_minimal.hpp"

int main(int argc, char *argv[])
{
    using namespace snn;

    using Runner = HardwareIdleRunner<poisson_neuron_model, stats_minimal>;

    return snn_host_main_impl<
        Runner
        >
    (argc, argv);
}
