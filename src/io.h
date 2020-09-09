#ifndef _IO_H_
#define _IO_H_

#include <iostream>
#include <fstream>
#include <cstdarg>
#include "shims.h"
#include "config.h"

enum LogLevel
{
    Debug = 0,
    Info  = 1,
    Warn  = 2,
    Error = 3,
    Fatal = 4
};

/**
 * @brief Anonymous namespace for private, io-related methods.
 */
namespace
{
    // Returns the given LogLevel value's prefix
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

struct io
{
    static std::ofstream logfile;

    explicit io(const std::string& logname);

    template<typename... Args>
    static void lprintf(LogLevel level, const std::string &format, Args... args)
    {
        char prefix = loglevelprefix(level);

        // Check loglevel and write to log file
        if(IMP_FILEOUT_LOGLEVEL <= level)
        {
            logfile << "[" + std::string(1, prefix)  + "] " + std20::format(format, args...) + '\n';
        }

        // Check loglevel and write to console
        if(IMP_CONOUT_LOGLEVEL <= level)
        {
            // Check if we need to write to stderr instead
            if(IMP_STDERR_LOGLEVEL <= level)
            {
                std::cerr << "[" + std::string(1, prefix) + "] " + std20::format(format, args...) + '\n';
            }
            else
            {
                std::cout << "[" + std::string(1, prefix)  + "] " + std20::format(format, args...) + '\n';
            }
        }

    }

    template<typename... Args>
    static void fatal(const std::string &format, Args... args)
    {
        lprintf(LogLevel::Fatal, format, args...);
        exit(EXIT_FAILURE);
    }
};

#endif