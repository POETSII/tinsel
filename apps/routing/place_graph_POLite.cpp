#include "Routing.hpp"

#include "rapidjson/filereadstream.h"
#include "rapidjson/filewritestream.h"
#include "rapidjson/writer.h"

#include <POLite.h>

struct HeatMessage {
};

struct HeatState {
};

struct HeatDevice : PDevice<HeatState, None, HeatMessage> {

  void init() {
  }

  inline void send(volatile HeatMessage* msg) {
  }

  inline void recv(HeatMessage* msg, None* edge) {
  }

  inline bool step() { return false; }

  inline bool finish(volatile HeatMessage* msg) {
    return true;
  }
};

int main(int argc, char *argv[])
{
    std::string configPath=argv[1];
    std::string appPath=argv[2];

    std::unique_ptr<SystemParameters> params=initSystemParameters("tinsel-0.6", 4,4, 4,4);
    { 
        FILE* fp = fopen(configPath.c_str(), "rb"); // non-Windows use "r"
        char readBuffer[65536];
        rapidjson::FileReadStream is(fp, readBuffer, sizeof(readBuffer));
        
        rapidjson::Document d;
        d.ParseStream(is);

        params->load(d);
        
        fclose(fp);
    }

    if((params->FPGAMeshXLen%TinselMeshXLenWithinBox)!=0){
        fprintf(stderr, "FPGAMeshXLen=%u, TinselMeshXLenWithinBox=%u\n", params->FPGAMeshXLen, TinselMeshXLenWithinBox);
        throw std::runtime_error("FPGAMeshXLen is not a multiple of TinselMeshXLenWithinBox");
    }

    if((params->FPGAMeshYLen%TinselMeshYLenWithinBox)!=0){
        throw std::runtime_error("FPGAMeshYLen is not a multiple of TinselMeshYLenWithinBox");
    }

    fprintf(stderr, "     Loading app graph.\n");
    AppGraph app;
    { 
        FILE* fp = fopen(appPath.c_str(), "rb"); // non-Windows use "r"
        char readBuffer[65536];
        rapidjson::FileReadStream is(fp, readBuffer, sizeof(readBuffer));
        
        rapidjson::Document d;
        d.ParseStream(is);

        app.load(d);
        
        fclose(fp);
    }

    unsigned numBoxesX=params->FPGAMeshXLen / TinselMeshXLenWithinBox;
    unsigned numBoxesY=params->FPGAMeshYLen / TinselMeshYLenWithinBox;

    fprintf(stderr, "     Building PGraph.\n");
    PGraph<HeatDevice, HeatState, None, HeatMessage> graph(numBoxesX, numBoxesY);

    for(unsigned i=0; i<app.vertices.size(); i++){
        auto pid=graph.newDevice();
        if(pid!=i){
            throw std::runtime_error("POLIte id assumptions wrong.");
        }
    }

    for(const auto &v : app.vertices){
        for(auto [e,w] : v.out){
            graph.addEdge(v.index, 0, e); // TODO : weights on edges
        }
    }

    fprintf(stderr, "     Mapping using POLite.\n");
    graph.map(true);

    std::string placer="POLite-default";
    //if(getenv())

    fprintf(stderr, "     Outputting placement.\n");
    AppPlacement placement;
    placement.graph_id=app.name;
    placement.system=*params;
    placement.placement.resize(app.vertices.size());

    for(unsigned i=0; i<app.vertices.size(); i++){
        placement.placement[i].value=graph.toDeviceAddr[i];
    }

    rapidjson::Document doc;
    auto val=placement.save(doc.GetAllocator());

    {
        char writeBuffer[65536];
        rapidjson::FileWriteStream os(stdout, writeBuffer, sizeof(writeBuffer));
    
        rapidjson::Writer<rapidjson::FileWriteStream> writer(os);
        val.Accept(writer);
    }

}