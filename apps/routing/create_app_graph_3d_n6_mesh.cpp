#include "Routing.hpp"

#include <rapidjson/writer.h>
#include <rapidjson/filewritestream.h>


int main(int argc, char *argv[])
{
    AppGraph g;

    unsigned w=atoi(argv[1]);
    unsigned h=atoi(argv[2]);
    unsigned d=atoi(argv[3]);


    g.generator="3d_n6_mesh";
    g.name="ag_3d_n6_mesh_"+std::to_string(w)+"_"+std::to_string(h)+"_"+std::to_string(d);

    auto linear=[=](unsigned x, unsigned y, unsigned z)
    {
        return w*h*z + w*y + x;
    };

    g.vertices.resize(w*h*d);
    for(unsigned x=0; x<w; x++){
        for(unsigned y=0; y<w; y++){
            for(unsigned z=0; z<d; z++){
                unsigned index=linear(x,y,z);
                auto &v=g.vertices[index];
                v.index=index;
                v.id=std::to_string(x)+"_"+std::to_string(y)+"_"+std::to_string(z);
                if(x>0){
                    v.out[index-1]=1;
                }
                if(x<w-1){
                    v.out[index+1]=1;
                }
                if(y<h-1){
                    v.out[index+w]=1;
                }
                if(y>0){
                    v.out[index-w]=1;
                }
                if(z<d-1){
                    v.out[index+w*h]=1;
                }
                if(z>0){
                    v.out[index-w*h]=1;
                }
            }
        }
    }

    rapidjson::Document doc;
    rapidjson::Value val=g.save(doc.GetAllocator());

    char writeBuffer[65536];
    rapidjson::FileWriteStream os(stdout, writeBuffer, sizeof(writeBuffer));
 
    rapidjson::Writer<rapidjson::FileWriteStream> writer(os);
    val.Accept(writer);
}