#include "master.h"
#include "io.h"

master_server::master_server(hostaddr ip, hostport port) : basic_server(ip, port)
{
    io::lprintf(LogLevel::Info, "*** Starting master server on %s %d on %s ***", ip ? ip : "localhost", port, tools::strtime(&starttime).c_str());
}
>>>>>>> 1435a63... Base rewrite
