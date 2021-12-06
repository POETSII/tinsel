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

    double minEdges=DBL_MAX, maxEdges=0, sumEdges=0, sumEdgesSquare=0;
    for(unsigned i=0; i<app.vertices.size(); i++){
        minEdges=std::min<double>(minEdges, app.vertices[i].out.size());
        maxEdges=std::max<double>(maxEdges, app.vertices[i].out.size());
        sumEdges+=app.vertices[i].out.size();
        sumEdgesSquare+=app.vertices[i].out.size()*(double)app.vertices[i].out.size();
    }
    double meanEdges=sumEdges/app.vertices.size();
    double stddevEdges=sqrt(sumEdgesSquare/app.vertices.size() - meanEdges*meanEdges);
    fprintf(stdout, "AppGraph, Edges, count, %g\n", sumEdges);
    fprintf(stdout, "AppGraph, Degree, min, %g\n", minEdges);
    fprintf(stdout, "AppGraph, Degree, max, %g\n", maxEdges);
    fprintf(stdout, "AppGraph, Degree, mean, %g\n", sumEdges/app.vertices.size());
    fprintf(stdout, "AppGraph, Degree, stddev, %g\n", stddevEdges);

    auto dump_distribution=[&](const std::string &domain, const std::string &aspect, std::vector<unsigned> &hist)
    {
        if(hist.size()==0){
            hist.resize(1,0);
        }
        fprintf(stdout, "%s, %s, max, %u\n", domain.c_str(), aspect.c_str(), hist.size()-1);
        double sum=0, sumSqr=0, total_count=0;
        unsigned off=0;
        for(auto count : hist){
            sum += count * off;
            sumSqr += off*off*count;
            total_count += count;
            ++off;
        }

        fprintf(stdout, "%s, %s, mean, %g\n", domain.c_str(), aspect.c_str(), sum/total_count);
        double mean=sum/total_count;
        fprintf(stdout, "%s, %s, stddev, %g\n", domain.c_str(), aspect.c_str(), sqrt(sumSqr/total_count - mean*mean));
        for(unsigned i=0; i<hist.size(); i++){
            fprintf(stdout, "%s, %s, %u, %u\n", domain.c_str(), aspect.c_str(), i, hist[i]);
        }
    };

    dump_distribution("Routing", "RouteCost", route_cost_distribution);
    dump_distribution("Routing", "RouteHops", route_hops_distribution);
    dump_distribution("Routing", "RouteFPGAHops", route_fpga_hops_distribution);
    dump_distribution("Routing", "RouteMailboxHops", route_mailbox_hops_distribution);

    dump_distribution("Routing", "LinkLoad", all_links_load_distribution);
    dump_distribution("Routing", "FPGALinkLoad", fpga_links_load_distribution);
    dump_distribution("Routing", "MailboxLinkLoad", mailbox_links_load_distribution);
}