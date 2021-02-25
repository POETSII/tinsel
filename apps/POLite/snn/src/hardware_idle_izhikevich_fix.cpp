#include "snn_host_main.hpp"

#include "models/snn_model_izhikevich_fix.hpp"
#include "stats/stats_minimal.hpp"

int main(int argc, char *argv[])
{
    using namespace snn;

    using Runner = HardwareIdleRunner<izhikevich_fix_neuron_model, stats_minimal>;

    return snn_host_main_impl<
        Runner
        >
    (argc, argv);
}
