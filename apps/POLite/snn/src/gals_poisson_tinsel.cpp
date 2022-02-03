#include "runners/gals_runner.hpp"
#include "snn_tinsel_main.hpp"


#include "models/snn_model_poisson.hpp"
#include "stats/stats_minimal.hpp"

int main()
{
  using namespace snn;

  return snn_tinsel_main_impl<
    GALSRunner<
      poisson_neuron_model,
      stats_minimal
      >
  >();
}
