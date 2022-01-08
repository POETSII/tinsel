#include "Routing0p6.hpp"

int main()
{
    auto params=initSystemParameters("tinsel-0.6", 2, 2, 4, 4);

    auto system=std::make_unique<SystemNode0p6>(params.get());
    validate_node_heirarchy(system.get());
    print_node_heirarchy(std::cout, "", system.get());
}