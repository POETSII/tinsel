#include "RoutingFactory.hpp"
#include "DrawableFactory.hpp"


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

    double max_load_fpga=0;
    double max_load_mailbox=0;
    for(const auto &kv : loads){
        if(kv.first->type==InterFPGALink){
            max_load_fpga=std::max<double>(max_load_fpga, kv.second);
        }else{
            max_load_mailbox=std::max<double>(max_load_mailbox, kv.second);
        }
    }
    fprintf(stderr, "Max load FPGA= %g, Max load mailbox = %g, load count = %u\n", max_load_fpga, max_load_mailbox, loads.size());

    std::cerr<<"Rendering\n";

    std::unique_ptr<Drawable> drawable=create_drawable(system.get());
    Size s=drawable->size();

    double scale=0.2;
    SVGWriter writer(s.w*scale, s.h*scale);
    writer.begin_group(SVGWriter::make_scale(0.2));

    LinkMap map;
    drawable->draw(writer, {0,0}, map);


    auto to_colour_fpga = [](double v) -> SVGWriter::ColourSpec
    {
        assert(0.0<=v && v<=1.0);
        return SVGWriter::Colour{ v, 0, 0  };
    };

    auto to_colour_mailbox = [](double v) -> SVGWriter::ColourSpec
    {
        assert(0.0<=v && v<=1.0);
        return SVGWriter::Colour{ 0, 0, v  };
    };

    SVGWriter::Attributes aa;
    aa.width=8.0;
    for(const auto &lo : map.linksOut){
        auto it=map.linksIn.find(lo.first->sink);
        if(it==map.linksIn.end()){
            std::cerr<<"No array for "<<lo.first->name<<"\n";
        }else{
            double load=0;
            auto it2=loads.find(lo.first);
            if(it2!=loads.end()){
                if(lo.first->type==InterFPGALink){
                    load=it2->second / max_load_fpga;
                }else{
                    load=it2->second / max_load_mailbox;
                }
            }
            unsigned numRoutes=0;
            auto it3=routes.links.find((const LinkOut*)lo.first);
            if(it3!=routes.links.end()){
                numRoutes = it3->second.size();
            }
            std::string title="routes="+std::to_string(numRoutes)+", load="+std::to_string(load);

            aa.title=title;
            aa.stroke_colour=lo.first->type==InterFPGALink ? to_colour_fpga(load) : to_colour_mailbox(load) ;
            writer.arrow(lo.second, it->second, aa);
        }
    }

    writer.end_group();

    writer.save("/dev/stdout");
}