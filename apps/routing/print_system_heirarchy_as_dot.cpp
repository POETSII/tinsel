#include "RoutingFactory.hpp"

#include "rapidjson/filereadstream.h"

int main()
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

    validate_node_heirarchy(system.get());
    print_node_heirarchy_as_dot(std::cout, system.get());
}