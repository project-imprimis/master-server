#ifndef _SHIMS_H_
#define _SHIMS_H_

#include <iostream>

namespace std20
{
    /**
     * Format a string, C-style, without printing it out. Returns an std::string.
     * Shim for C++20's std::format
     */

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-security"
    template<typename... Args>
    std::string format(const std::string &format, Args... args)
    {
        size_t size = snprintf(nullptr, 0, format.c_str(), args...);

        if(size <= 0)
        {
            throw std::runtime_error("Formatting error.");
        }

        std::string buf;

        buf.resize(size + 1); // +1 for null terminator
        size = snprintf(buf.data(), size, format.c_str(), args...);
        buf.resize(size);

        return buf;
    }
#pragma GCC diagnostic pop
}

namespace tools
{

    std::string strtime(const time_t *time);

}

#endif
