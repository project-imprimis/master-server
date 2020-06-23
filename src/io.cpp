#include <iostream>
#include <fstream>
#include <cstdarg>

#include "io.h"

bool io::init(char *homedir)
{
    logfile.open(homedir + std::string(LOGNAME) + ".log", std::ios::app);
    return true;
}