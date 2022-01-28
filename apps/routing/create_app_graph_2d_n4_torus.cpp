#include "Routing.hpp"

#include <rapidjson/writer.h>
#include <rapidjson/filewritestream.h>


int main(int argc, char *argv[])
{
    AppGraph g;

    unsigned w=atoi(argv[1]);
    unsigned h=atoi(argv[2]);

    g.generator="2d_n4_torus";
    g.name="ag_2d_n4_torus_"+std::to_string(w)+"_"+std::to_string(h);

    auto linear=[=](unsigned x, unsigned y)
    {
        return w*((y+h)%h)+((x+w)%w);
    };

    g.vertices.resize(w*h);
    for(unsigned x=0; x<w; x++){
        for(unsigned y=0; y<w; y++){
            unsigned index=linear(x,y);
            auto &v=g.vertices[index];
            v.index=index;
            v.id=std::to_string(x)+"_"+std::to_string(y);
            v.out[linear(x,y-1)]=1;
            v.out[linear(x,y+1)]=1;
            v.out[linear(x-1,y)]=1;
            v.out[linear(x+1,y)]=1;
        }
    }

    rapidjson::Document doc;
    rapidjson::Value val=g.save(doc.GetAllocator());

    char writeBuffer[65536];
    rapidjson::FileWriteStream os(stdout, writeBuffer, sizeof(writeBuffer));
 
    rapidjson::Writer<rapidjson::FileWriteStream> writer(os);
    val.Accept(writer);
}