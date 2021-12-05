#include "Routing.hpp"

#include <rapidjson/writer.h>
#include <rapidjson/filewritestream.h>


int main(int argc, char *argv[])
{
    AppGraph g;

    unsigned nv=atoi(argv[1]);
    unsigned d=atoi(argv[2]);

    std::mt19937 urng;

    g.vertices.resize(nv);
    for(unsigned i=0; i<nv; i++){
        g.vertices[i].index=i;
        g.vertices[i].id=std::to_string(i);
        for(unsigned j=0; j<d; j++){
            unsigned dst=urng()%nv;
            g.vertices[i].out[dst]+=1;
        }
    }

    rapidjson::Document doc;
    rapidjson::Value val=g.save(doc.GetAllocator());

    char writeBuffer[65536];
    rapidjson::FileWriteStream os(stdout, writeBuffer, sizeof(writeBuffer));
 
    rapidjson::Writer<rapidjson::FileWriteStream> writer(os);
    val.Accept(writer);
}