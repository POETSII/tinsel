#ifndef snn_host_main_hpp
#define snn_host_main_hpp

#include "models/snn_model_izhikevich_fix.hpp"
#include "stats/stats_minimal.hpp"
#include "topology/network_topology_factory.hpp"
#include "runners/runner_config.hpp"

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
    std::string neuron_config_str = RunnerType::neuron_model_t::default_model_config();
    std::string log_file_name;
    unsigned max_steps;
    PlacerMethod placer_method;
    std::vector<std::string> user_key_values;
    int boxesX=1;
    int boxesY=1;
    {
        po::options_description desc("Allowed options");
        desc.add_options()
            ("help", "produce help message")
            ("seed", po::value<uint64_t>()->default_value(1), "seed for creating network")
            ("placer", po::value<std::string>()->default_value("default"), "Placer method (metis,bfs,random,default)")
            ("topology", po::value<std::string>()->default_value(R"({"type":"EdgeProbTopology","nNeurons":10000,"pConnect":0.01})"), "Topology config")
            ("neuron-model", po::value<std::string>()->default_value(neuron_config_str), "Neuron model config")
            ("max-steps", po::value<uint64_t>()->default_value(1000), "Number of network time-steps to take")
            ("log-file", po::value<std::string>()->default_value("snn.log"), "Where to write the log file")
            ("user-key-value", po::value<std::vector<std::string>>(), "User-defined key:value pairs to add to the log. e,g. '--user-key-value=[key]:[value]'")
            ("boxes", po::value<std::string>()->default_value("1x1"), "Number of boxes in XxY form, or max for maximum availalbe.")
        ;

        po::variables_map vm;
        po::store(po::parse_command_line(argc, argv, desc), vm);
        po::notify(vm);  

        if(vm.count("help")>0){
            std::cerr << desc << "\n";
            return 1;
        }

        seed=vm["seed"].as<uint64_t>();
        placer_method= parse_placer_method( vm["placer"].as<std::string>() );
        topology_config_str=vm["topology"].as<std::string>();
        neuron_config_str=vm["neuron-model"].as<std::string>();
        log_file_name=vm["log-file"].as<std::string>();
        max_steps=vm["max-steps"].as<uint64_t>();
        if (vm.count("user-key-value")>0){
            user_key_values=vm["user-key-value"].as<std::vector<std::string>>();
        }

        std::string boxes_config=vm["boxes"].as<std::string>();
        if(boxes_config=="max"){
            boxesX=-1;
            boxesY=-1;
        }
    }

    Logger log(log_file_name);
    if(!user_key_values.empty()){
        auto r=log.with_region("user-params");
        for(const auto &kv : user_key_values){
            auto colon=kv.find(':');
            if(colon==std::string::npos){
                log.export_value(kv.c_str(), "", 1);
            }else{
                std::string key=kv.substr(0, colon);
                std::string value=kv.substr(colon+1);
                log.export_value(key.c_str(), value.c_str(), 1);
            }
        }
    }

    if(boxesX==-1){
        boxesX=TinselBoxMeshXLen;
        boxesY=TinselBoxMeshYLen;
    }
    log.export_value("numBoxesX", (int64_t)boxesX, 1);
    log.export_value("numBoxesY", (int64_t)boxesY, 1);

    StochasticSourceNode noise(seed);

    RunnerConfig config;
    config.max_time_steps=max_steps;
    
    RunnerType runner(boxesX, boxesY);
    runner.parse_neuron_config(neuron_config_str, log);

    {
        auto r=log.with_region("POLite_config");
        log.export_value("app_message_size", (int64_t)sizeof(typename RunnerType::message_type), 2);
        log.export_value("POLite_message_size", (int64_t)sizeof(PMessage<typename RunnerType::message_type>), 2);
        log.export_value("POLITE_NUM_PINS", (int64_t)POLITE_NUM_PINS, 2);
    }



    runner.graph.on_phase_hook=[&](const char *name) { log.tag_leaf(name); };
    runner.graph.on_fatal_error=[&](const char *msg) { log.fatal_error(msg); };
    runner.graph.on_export_value=[&](const char *name, double value) { log.export_value(name, value, 2); };
    runner.graph.on_export_string=[&](const char *name, const char * value) { log.export_value(name, value, 2); };

    runner.graph.placer_method=placer_method;

    std::string riscv_code_path, riscv_data_path;
    {
        auto r=log.with_region("prep");

        {
            log.enter_leaf("finding_riscv_code");
            if(runner.graph.is_simulation){
                log.log(3, "Skipping code search as we are simulating.");
            }else{
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
        log.export_value("graph_total_edges", (int64_t)runner.graph.getEdgeCount(), 1);
        log.export_value("graph_max_fan_in", (int64_t)runner.graph.getMaxFanIn(), 1);
        log.export_value("graph_max_fan_out", (int64_t)runner.graph.getMaxFanOut(), 1);
        

        {
            auto r=log.with_region("par");
            runner.graph.map();
        }

        log.enter_leaf("assign_states");
        for(unsigned i=0; i<device_states.size(); i++){
            runner.graph.devices[i]->state = device_states[i];
        }
    }

    {
        auto r=log.with_region("execute");

        HostLinkParams hostLinkParams;
        hostLinkParams.numBoxesX=boxesX;
        hostLinkParams.numBoxesY=boxesY;
        hostLinkParams.useExtraSendSlot=false;
        hostLinkParams.on_phase=[&](const char *name){
            log.tag_leaf(name);
        };
        hostLinkParams.max_connection_attempts=8;
        
        

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

