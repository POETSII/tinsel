#ifndef snn_host_main_hpp
#define snn_host_main_hpp

#include "models/snn_model_izhikevich_fix.hpp"
#include "stats/stats_minimal.hpp"
#include "runners/hardware_idle_runner.hpp"
#include "topology/edge_prob_network_topology.hpp"

#include "logging.hpp"

#ifdef TINSEL
static_assert(0, "Not expecting this to be compiled for tinsel.");
#endif

template<class RunnerType>
int snn_host_main_impl(int argc, char *argv[])
{
    using namespace snn;

    Logger log;

    StochasticSourceNode noise;

    std::string riscv_code_path, riscv_data_path;

    RunnerType runner;

    runner.graph.on_phase_hook=[&](const char *name)
    { log.tag_leaf(name); };

    {
        auto r=log.with_region("prepare");

        {
            log.enter_leaf("finding_riscv_code");
            std::vector<char> buffer(4096);
            int c=readlink("/proc/self/exe", &buffer[0], buffer.size()-1);
            if(c==-1 || (c>=buffer.size())){
                fprintf(stderr, "Couldnt fine self executable path.");
                exit(1);
            }
            buffer[c]=0;

            riscv_code_path=std::string(&buffer[0])+".riscv.code.v";
            riscv_data_path=std::string(&buffer[0])+".riscv.data.v";

            if(  access( riscv_code_path.c_str(), F_OK ) != 0){
                fprintf(stderr, "Couldnt find code file at %s\n", riscv_code_path.c_str());
                exit(1);
            }
            if(  access( riscv_data_path.c_str(), F_OK ) != 0){
                fprintf(stderr, "Couldnt find data file at %s\n", riscv_data_path.c_str());
                exit(1);
            }
        }

        log.enter_leaf("create_topology");
        unsigned num_neurons=100000;
        double pEdge=1000.0/num_neurons;
        EdgeProbNetworkTopology net(num_neurons, pEdge, noise);

        log.enter_leaf("build_polite_graph");
        using state_type = typename RunnerType::state_type;
        std::vector<state_type> device_states;
        runner.build_graph(net, device_states);

        {
            auto r=log.with_region("par");
            runner.graph.map();
        }

        log.enter_leaf("assign_states");
        for(unsigned i=0; i<device_states.size(); i++){
            runner.graph.devices[i]->state = device_states[i];
            //fprintf(stderr, "%u, %f, %f\n", i, (float)device_states[i].neuron_state.u, (float)device_states[i].neuron_state.v );
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
                hostlink.boot(riscv_code_path.c_str(), riscv_data_path.c_str());
            }

            {
                auto rr=log.with_region("go");
                hostlink.go();
            }

            {
                auto rr=log.with_region("go");
                runner.collect_output(hostlink, log);
            }
        }
    }

    return 0;
}

#endif

