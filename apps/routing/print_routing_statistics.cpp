#include "RoutingFactory.hpp"
#include <map>

#include <cfloat>

#include "rapidjson/filereadstream.h"


int main(int argc, char *argv[])
{
    std::string placementPath=argv[1];
    std::string appGraph=argv[2];

    AppPlacement placement;
    { 
        FILE* fp = fopen(placementPath.c_str(), "rb"); // non-Windows use "r"
        char readBuffer[65536];
        rapidjson::FileReadStream is(fp, readBuffer, sizeof(readBuffer));
        
        rapidjson::Document d;
        d.ParseStream(is);

        placement.load(d);
        
        fclose(fp);
    }

    AppGraph app;
    { 
        FILE* fp = fopen(appGraph.c_str(), "rb"); // non-Windows use "r"
        char readBuffer[65536];
        rapidjson::FileReadStream is(fp, readBuffer, sizeof(readBuffer));
        
        rapidjson::Document d;
        d.ParseStream(is);

        app.load(d);
        
        fclose(fp);
    }

    auto system=create_system(&placement.system);

    Routing routes(system.get());


    std::cerr<<"Adding routes\n";

    routes.add_graph(placement, app);

    auto loads = routes.calculate_link_load();

    std::vector<unsigned> route_cost_distribution, route_hops_distribution, route_fpga_hops_distribution, route_mailbox_hops_distribution;
    std::vector<unsigned> all_links_load_distribution, fpga_links_load_distribution, mailbox_links_load_distribution;

    auto add_histogram=[&](std::vector<unsigned> &hist, unsigned x)
    {
        if(x>=hist.size()){
            hist.resize(x+1, 0);
        }
        hist.at(x)++;
    };

    auto dump_distribution=[&](const std::string &domain, const std::string &aspect, std::vector<unsigned> &hist)
    {
        if(hist.size()==0){
            hist.resize(1,0);
        }
        fprintf(stdout, "%s, %s, max, %u\n", domain.c_str(), aspect.c_str(), hist.size()-1);
        double sum=0, total_count=0;
        unsigned off=0;
        for(auto count : hist){
            sum += count * off;
            total_count += count;
            ++off;
        }
        double mean=sum/total_count;

        double oe2=0, oe3=0, oe4=0;
        off=0;
        for(auto count : hist){
            if(count){
                double vv=(off-mean);
                oe2 += count*(vv*vv);
                oe3 += count*(vv*vv*vv);
                oe4 += count*(vv*vv*vv*vv);
            }
            ++off;
        }

        double stddev=sqrt(oe2/mean);
        double skewness=(oe3/mean) / std::pow(stddev, 3);
        double kurtosis=(oe4/mean) / std::pow(stddev, 4) - 3;

        fprintf(stdout, "%s, %s, mean, %g\n", domain.c_str(), aspect.c_str(), sum/total_count);
        fprintf(stdout, "%s, %s, stddev, %g\n", domain.c_str(), aspect.c_str(), stddev);
        fprintf(stdout, "%s, %s, skewness, %g\n", domain.c_str(), aspect.c_str(), skewness);
        fprintf(stdout, "%s, %s, excess_kurtosis, %g\n", domain.c_str(), aspect.c_str(), kurtosis);

        for(unsigned i=0; i<hist.size(); i++){
            if(hist[i]!=0){
                fprintf(stdout, "%s, %s, %u, %u\n", domain.c_str(), aspect.c_str(), i, hist[i]);
            }
        }
    };

    for(const auto &kv : loads){
        add_histogram(all_links_load_distribution, kv.second);
        if(kv.first->type==InterFPGALink){
            add_histogram(fpga_links_load_distribution, kv.second);
        }
        if(kv.first->type==InterMailboxLink){
            add_histogram(mailbox_links_load_distribution, kv.second);
        }
    }
    
    for(const auto &kv : routes.routes){
        add_histogram(route_cost_distribution, kv.second.cost);
        add_histogram(route_hops_distribution, kv.second.hops);
        add_histogram(route_fpga_hops_distribution, kv.second.fpga_hops);
        add_histogram(route_mailbox_hops_distribution, kv.second.mailbox_hops);
    }

    fprintf(stdout, "# Domain, Aspect, Index, Value\n");
    fprintf(stdout, "AppGraph, Name, -, %s\n", app.name.c_str());
    fprintf(stdout, "AppGraph, Generator, -, %s\n", app.generator.c_str());
    fprintf(stdout, "AppGraph, Devices, count, %u\n", app.vertices.size());

    std::vector<unsigned> edist;
    for(unsigned i=0; i<app.vertices.size(); i++){
        add_histogram(edist, app.vertices[i].out.size());
    }
    fprintf(stdout, "AppGraph, Edges, count, %g\n", std::accumulate(edist.begin(), edist.end(), 0.0));
    dump_distribution("AppGraph", "Degree", edist);    

    dump_distribution("Routing", "RouteCost", route_cost_distribution);
    dump_distribution("Routing", "RouteHops", route_hops_distribution);
    dump_distribution("Routing", "RouteFPGAHops", route_fpga_hops_distribution);
    dump_distribution("Routing", "RouteMailboxHops", route_mailbox_hops_distribution);

    dump_distribution("Routing", "LinkLoad", all_links_load_distribution);
    dump_distribution("Routing", "FPGALinkLoad", fpga_links_load_distribution);
    dump_distribution("Routing", "MailboxLinkLoad", mailbox_links_load_distribution);
}