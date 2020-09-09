#include <iostream>
#include <enet/enet.h>
#include <memory>
#include "types.h"
#include "master.h"

// Temporary, settings macros
#define configdir "~/.config/imprimis-master/"

int main(int argc, char **argv)
{
    // IP Address and port the master server resides on
    hostaddr ip = nullptr;
    hostport port = 42068;

    // Initialize enet or exit
    if(enet_initialize() < 0)
    {
        std::cerr << "Unable to initialize network module" << std::endl;
        exit(1);
    }
    atexit(enet_deinitialize);

    auto master = master_server(ip, port);

    while(true)
    {
        master.runloop();
    }

    return 0;
}
