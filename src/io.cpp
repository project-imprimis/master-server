#include "io.h"

#include <iostream>
#include <fstream>
#include <cstdarg>
//#include "config.h"

std::ofstream io::logfile; /* NOLINT */

io::io(const std::string& logname)
{
    /* Open log file and push all cerr output to the log file */
    logfile.open(logname, std::ios::app);
    std::cerr.rdbuf(io::logfile.rdbuf());
}