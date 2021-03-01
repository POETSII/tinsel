#include "snn_tinsel_main.hpp"

#include "models/snn_model_poisson.hpp"
#include "stats/stats_minimal.hpp"
#include "runners/hardware_idle_runner.hpp"

int main()
{
  using namespace snn;

  return snn_tinsel_main_impl<
    HardwareIdleRunner<
      poisson_neuron_model,
      stats_minimal
      >
  >();
}
