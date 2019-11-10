#include "POLite/Routing.hpp"

int main()
{
    SystemParameters params;
    initSystemParameters(&params, 2, 2, 4, 4);

    auto system=std::make_unique<SystemNode>(&params);
    validate_node_heirarchy(system.get());
    print_node_heirarchy(std::cout, "", system.get());
}