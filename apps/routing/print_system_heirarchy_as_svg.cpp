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

    std::cerr<<"Rendering\n";

    std::unique_ptr<Drawable> drawable=create_drawable(system.get());
    Size s=drawable->size();

    SVGWriter writer(s.w, s.h);

    LinkMap map;
    drawable->draw(writer, {0,0}, map);

    SVGWriter::Attributes aa;
    aa.width=4.0;
    for(const auto &lo : map.linksOut){
        auto it=map.linksIn.find(lo.first->sink);
        if(it==map.linksIn.end()){
            std::cerr<<"No array for "<<lo.first->name<<"\n";
        }else{
            writer.arrow(lo.second, it->second, aa);
        }
    }

    writer.save(destPath.c_str());
}