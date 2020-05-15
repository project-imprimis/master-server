#ifndef _io_h
#define _io_h

#include <iostream>
#include <fstream>
#include <cstdarg>

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
    const LogLevel CONOUT_LOGLEVEL = LogLevel::Error;

    FILE *logfile;

    /*
    Print message to a file and/or to the console, depending on the
    specified LogLevel.
    */
    void lprintf(LogLevel level, const char *format, ...);

    void fatal(const char *format, ...);

    bool init();
}

#endif