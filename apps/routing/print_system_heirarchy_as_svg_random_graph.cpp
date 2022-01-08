#include "RoutingFactory.hpp"
#include "DrawableFactory.hpp"


int main(int argc, char *argv[])
{
    std::string sourcePath="/dev/stdin";
    std::string destPath="/dev/stdout";

    std::unique_ptr<SystemParameters> params=initSystemParameters("tinsel-0.6", 4,4, 4,4);

    { 
        FILE* fp = fopen(sourcePath.c_str(), "rb"); // non-Windows use "r"
        char readBuffer[65536];
        rapidjson::FileReadStream is(fp, readBuffer, sizeof(readBuffer));
        
        rapidjson::Document d;
        d.ParseStream(is);

        params->load(d);
        
        fclose(fp);
    }

    auto system=create_system(params.get());


    std::cerr<<"Generating random routes\n";
    std::mt19937 rng;
    
    Routing routes(system.get());
    for(unsigned i=0; i<params->get_total_threads()*10; i++){
        routes.add_route( params->pick_random_thread(rng), params->pick_random_thread(rng), 1 );
    }

    auto loads = routes.calculate_link_load();

    double max_load=0;
    for(const auto &kv : loads){
        max_load=std::max<double>(max_load, kv.second);
    }
    fprintf(stderr, "Max load = %g, load count = %u\n", max_load, loads.size());

    std::cerr<<"Rendering\n";

    std::unique_ptr<Drawable> drawable=create_drawable(system.get());
    Size s=drawable->size();

    SVGWriter writer(s.w, s.h);
    writer.begin_group(SVGWriter::make_scale(0.2));

    LinkMap map;
    drawable->draw(writer, {0,0}, map);

    /*auto to_colour = [](double v) -> SVGWriter::ColourSpec
    {
        assert(0.0<=v && v<=1.0);
        if(v==0.0){
            return SVGWriter::Colour{ 0.9,0.9,0.9  };
        }else if(v<0.5){
            return SVGWriter::Colour{ v, 1-v, v  };
        }else{
            return SVGWriter::Colour{ v, 1-v, 1-v  };
        };
    };*/

    auto to_colour = [](double v) -> SVGWriter::ColourSpec
    {
        assert(0.0<=v && v<=1.0);
        return SVGWriter::Colour{ v, 0, 0  };
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
                load=it2->second / max_load;
            }
            unsigned numRoutes=0;
            auto it3=routes.links.find((const LinkOut*)lo.first);
            if(it3!=routes.links.end()){
                numRoutes = it3->second.size();
            }
            std::string title="routes="+std::to_string(numRoutes)+", load="+std::to_string(load);

            aa.title=title;
            aa.stroke_colour=to_colour(load);
            writer.arrow(lo.second, it->second, aa);
        }
    }

    writer.end_group();

    writer.save(destPath.c_str());
}