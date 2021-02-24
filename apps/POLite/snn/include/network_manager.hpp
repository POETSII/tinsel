

#include 

class NetworkManager
{
    // Do all preparatory work that doesnt require a hostlink
    void prepare_off_hardware();

    // Connect to hardware, load, and run
    void run_on_hardware();
};

