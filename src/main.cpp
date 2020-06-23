#include <csignal>
#include "io.h"
#include "master.h"

volatile int reloadcfg = 1;

void reloadsignal(__attribute__((unused)) int signum)
{
    reloadcfg = 1;
}

int main(int argc, char **argv)
{
    // Initialize Enet, exit on fail
    if(enet_initialize() < 0)
    {
        io::fatal("Unable to initialise network module");
    }
    atexit(enet_deinitialize);

    const char *dir = "";
    const char *ip = nullptr;
    int port = 42068;

    if(argc>=2)
    {
        dir = argv[1];
    }
    if(argc>=3)
    {
        port = atoi(argv[2]);
    }
    if(argc>=4)
    {
        ip = argv[3];
    }

    DEF_FORMAT_STRING(logname, "%smaster.log", dir);
    DEF_FORMAT_STRING(cfgname, "%smaster.cfg", dir);
    path(logname);
    path(cfgname);

    // Open log file and push all cerr output to the log file
    io::logfile.open(logname);
    std::cerr.rdbuf(io::logfile.rdbuf());

    /*logfile = fopen(logname, "a");
    if(!logfile)
    {
        logfile = stdout;
    }
    setvbuf(logfile, NULL, _IOLBF, BUFSIZ);*/
#ifndef WIN32
    signal(SIGUSR1, reloadsignal);
#endif

    /**
     * Initialize the master server component
     */
    master::init(port, ip);

    /**
     * Loop to check if the config file needs to be reloaded.
     * Runs endlessly, does not belong in main.cpp
     */
    while(true)
    {
        if(reloadcfg)
        {
            io::lprintf(LogLevel::Info, "Reloading file: %s", cfgname);
            //execfile(cfgname);
            //bangameservers();
            //banclients();
            //gengbanlist();
            reloadcfg = 0;
        }
        master::servtime = enet_time_get();
        checkclients();
        checkgameservers();
    }

    return EXIT_SUCCESS;
}