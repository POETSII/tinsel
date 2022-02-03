#include "snn_tinsel_main.hpp"

#include "runners/hardware_idle_runner.hpp"
#include "models/snn_model_izhikevich_fix.hpp"
#include "stats/stats_minimal.hpp"
#include "runners/hardware_idle_runner.hpp"

int main()
{
  using namespace snn;

  return snn_tinsel_main_impl<
    HardwareIdleRunner<
      izhikevich_fix_neuron_model,
      stats_minimal
      >
  >();
}
