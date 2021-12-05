#include "Routing0p8.hpp"

#include <rapidjson/writer.h>
#include <rapidjson/filewritestream.h>

#include <string>

int main(int argc, char *argv[])
{
    std::unique_ptr<SystemParameters> params=initSystemParameters("tinsel-0.8", 4, 4, 4, 4);

    if(argc>1){
        params->FPGAMeshXLen=std::stoi(argv[1]);
        while( (1u<<params->FPGAMeshXBits) < params->FPGAMeshXLen){
            params->FPGAMeshXBits++;
        }
    }
    if(argc>2){
        params->FPGAMeshYLen=std::stoi(argv[2]);
        while( (1u<<params->FPGAMeshYBits) < params->FPGAMeshYLen){
            params->FPGAMeshYBits++;
        }
    }
    if(argc>3){
        params->MailboxMeshXLen=std::stoi(argv[3]);
        while( (1u<<params->MailboxMeshXBits) < params->MailboxMeshXLen){
            params->MailboxMeshXBits++;
        }
    }
    if(argc>4){
        params->MailboxMeshYLen=std::stoi(argv[4]);
        while( (1u<<params->MailboxMeshYBits) < params->MailboxMeshYLen){
            params->MailboxMeshYBits++;
        }
    }

    std::cerr<<"Sanity checking system parameters.\n";
    auto system=std::make_unique<SystemNode0p8>(params.get());
    validate_node_heirarchy(system.get());
    
    rapidjson::Document doc;
    auto val=params->save(doc.GetAllocator());


    char writeBuffer[65536];
    rapidjson::FileWriteStream os(stdout, writeBuffer, sizeof(writeBuffer));
 
    rapidjson::Writer<rapidjson::FileWriteStream> writer(os);
    val.Accept(writer);
}