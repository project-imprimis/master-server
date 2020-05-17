#ifndef _io_h
#define _io_h

#include <iostream>
#include <fstream>
#include <cstdarg>
#include "tools.h"

enum LogLevel
{
    Debug = 0,
    Info  = 1,
    Warn  = 2,
    Error = 3,
    Fatal = 4
};

namespace io
{
    const LogLevel FILEOUT_LOGLEVEL = LogLevel::Info;
    const LogLevel CONOUT_LOGLEVEL = LogLevel::Warn;
    const LogLevel STDERR_LOGLEVEL = LogLevel::Error;
    const char *LOGNAME = "master";

    std::ofstream logfile;

    /*
    Print message to a file and/or to the console, depending on the
    specified LogLevel.
    */
    template<typename... Args>
    void lprintf(LogLevel level, const char *format, Args... args)
    {
        char prefix = loglevelprefix(level);

        // Check loglevel and write to log file
        if(FILEOUT_LOGLEVEL <= level)
        {
            logfile << "[" + prefix + "] " + std20::format(format, args...) + '\n';
        }

        // Check loglevel and write to console
        if(CONOUT_LOGLEVEL <= level)
        {
            // Check if we need to write to stderr instead
            if(STDERR_LOGLEVEL <= level)
            {
                std::cerr << "[" + prefix + "] " + std20::format(format, args...) + '\n';
            }
            else
            {
                std::cout << "[" + prefix + "] " + std20::format(format, args...) + '\n';
            }
        }

    }

    template<typename... Args>
    void fatal(const char *format, ...)
    {
        lprintf(LogLevel::Fatal, format, args);
        exit(EXIT_FAILURE);
    }

    bool init();
}

#endif