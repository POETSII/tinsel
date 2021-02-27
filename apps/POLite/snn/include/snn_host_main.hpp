#ifndef snn_host_main_hpp
#define snn_host_main_hpp

#include "models/snn_model_izhikevich_fix.hpp"
#include "stats/stats_minimal.hpp"
#include "runners/hardware_idle_runner.hpp"
#include "topology/network_topology_factory.hpp"

#include "logging.hpp"

#include "boost/program_options.hpp"

#ifdef TINSEL
static_assert(0, "Not expecting this to be compiled for tinsel.");
#endif

template<class RunnerType>
int snn_host_main_impl(int argc, char *argv[])
{
    using namespace snn;
    namespace po=boost::program_options;

    uint64_t seed;
    std::string placer_method_str;
    std::string topology_config_str;
    std::string neuron_config_str;
    std::string log_file_name;
    unsigned max_steps;
    Placer::Method placer_method;
    {
        po::options_description desc("Allowed options");
        desc.add_options()
            ("help", "produce help message")
            ("seed", po::value<uint64_t>()->default_value(1), "seed for creating network")
            ("placer", po::value<std::string>()->default_value("default"), "Placer method (metis,bfs,random,default)")
            ("topology", po::value<std::string>()->default_value(R"({"type":"EdgeProbTopology","nNeurons":10000,"pConnect":0.01})"), "Topology config")
            ("neuron-model", po::value<std::string>()->default_value(R"({"type":"IzhikevichFix"})"), "Neuron model config")
            ("max-steps", po::value<uint64_t>()->default_value(1000), "Number of network time-steps to take")
            ("log-file", po::value<std::string>()->default_value("snn.log"), "Where to write the log file")
        ;

        po::variables_map vm;
        po::store(po::parse_command_line(argc, argv, desc), vm);
        po::notify(vm);  

        seed=vm["seed"].as<uint64_t>();
        placer_method= parse_placer_method( vm["placer"].as<std::string>() );
        topology_config_str=vm["topology"].as<std::string>();
        neuron_config_str=vm["neuron-model"].as<std::string>();
        log_file_name=vm["log-file"].as<std::string>();
        max_steps=vm["max-steps"].as<uint64_t>();
    }

    Logger log(log_file_name);


    StochasticSourceNode noise(seed);

    RunnerConfig config;
    config.max_time_steps=max_steps;
    
    RunnerType runner;

    runner.graph.on_phase_hook=[&](const char *name)
    { log.tag_leaf(name); };

    runner.graph.on_fatal_error=[&](const char *msg)
    { log.fatal_error(msg); };

    runner.graph.on_export_value=[&](const char *name, double value)
    { log.export_value(name, value, 2); };

    runner.graph.placer_method=placer_method;

    std::string riscv_code_path, riscv_data_path;
    {
        auto r=log.with_region("prep");

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
        boost::json::object topology_config;
        try{
            topology_config=boost::json::parse(topology_config_str).as_object();
        }catch(...){
            fprintf(stderr, "Couldnt parse json : '%s'\n", topology_config_str.c_str());
            exit(1);
        }
        std::shared_ptr<NetworkTopology> topology=network_topology_factory(noise, topology_config);

        log.enter_leaf("build_graph");
        using state_type = typename RunnerType::state_type;
        std::vector<state_type> device_states;
        runner.build_graph(config, *topology, device_states, log);

        log.export_value("graph_total_nodes", (int64_t)runner.graph.numDevices, 1);
        log.export_value("graph_total_edges", (int64_t)runner.graph.graph.getEdgeCount(), 1);
        log.export_value("graph_max_fan_in", (int64_t)runner.graph.graph.getMaxFanIn(), 1);
        log.export_value("graph_max_fan_out", (int64_t)runner.graph.graph.getMaxFanOut(), 1);
        

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

        HostLinkParams hostLinkParams;
        hostLinkParams.numBoxesX=1;
        hostLinkParams.numBoxesY=1;
        hostLinkParams.useExtraSendSlot=false;
        hostLinkParams.on_phase=[&](const char *name){
            log.tag_leaf(name);
        };
        
        

        log.enter_region("open_hostlink");
        HostLink hostlink(hostLinkParams);
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
                runner.collect_output(config, hostlink, log);
            }
        }
    }

    return 0;
}

#endif

