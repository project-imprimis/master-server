#include "io.cpp"
#include "master.cpp"

volatile int reloadcfg = 1;

#ifndef WIN32
void reloadsignal(int signum)
{
    reloadcfg = 1;
}
#endif

int main(int argc, char **argv)
{
    if(enet_initialize()<0)
    {
        io::fatal("Unable to initialise network module");
    }
    atexit(enet_deinitialize);
    const char *dir = "", *ip = NULL;
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

    logfile = fopen(logname, "a");
    if(!logfile)
    {
        logfile = stdout;
    }
    setvbuf(logfile, NULL, _IOLBF, BUFSIZ);
#ifndef WIN32
    signal(SIGUSR1, reloadsignal);
#endif
    setupserver(port, ip);
    for(;;)
    {
        if(reloadcfg)
        {
            conoutf("reloading %s", cfgname);
            //execfile(cfgname);
            bangameservers();
            banclients();
            gengbanlist();
            reloadcfg = 0;
        }
        servtime = enet_time_get();
        checkclients();
        checkgameservers();
    }

    return EXIT_SUCCESS;
}