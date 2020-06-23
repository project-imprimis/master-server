#include <iostream>
#include <fstream>
#include <cstdarg>

#include "io.h"

/*
Anonymous namespace for private, io-related methods.
*/
namespace
{
    /*
    Returns the given LogLevel value's prefix
    */
    char loglevelprefix(LogLevel level)
    {
        switch(level)
        {
            case LogLevel::Debug:
            {
                return 'D';
            }
            case LogLevel::Info:
            {
                return 'I';
            }
            case LogLevel::Warn:
            {
                return 'W';
            }
            case LogLevel::Error:
            {
                return 'E';
            }
            case LogLevel::Fatal:
            {
                return 'F';
            }
            default:
            {
                return '?';
            }
        }
    }
}

bool io::init(char *homedir)
{
    logfile.open(homedir + std::string(LOGNAME) + ".log", std::ios::app);
    return true;
}