#include <iostream>
#include <fstream>
#include <cstdarg>

#include "io.h"

namespace io
{
    void lprintf(LogLevel level, const char *format, ...)
    {
        va_list args;
        va_start(args, format);

        char hint;

        // I'm sure there's a better way to store this than in a switch case
        switch(level)
        {
            case LogLevel::Debug:
            {
                hint = 'D';
                break;
            }
            case LogLevel::Info:
            {
                hint = 'I';
                break;
            }
            case LogLevel::Warn:
            {
                hint = 'W';
                break;
            }
            case LogLevel::Error:
            {
                hint = 'E';
                break;
            }
            case LogLevel::Fatal:
            {
                hint = 'F';
                break;
            }
            default:
            {
                hint = '?';
                break;
            }
        }

        // Check loglevel and write to log file
        if(FILEOUT_LOGLEVEL <= level)
        {
            fprintf(logfile, "[%c] ", hint); // Write prefix
            vfprintf(logfile, format, args); // Write log entry
            fputc('\n', logfile); // Write newline character
        }

        // Check loglevel and write to console
        if(CONOUT_LOGLEVEL <= level)
        {
            printf("[%c] ", hint);
            vprintf(format, args);
            putchar('\n');
        }

        va_end(args);
    }

    void fatal(const char *format, ...)
    {
        va_list args;
        va_start(args, format);
        
        lprintf(LogLevel::Fatal, format, args);

        va_end(args);

        exit(EXIT_FAILURE);
    }

    bool init(char *homedir)
    {
        logfile = fopen(logname, "a");
    }
}