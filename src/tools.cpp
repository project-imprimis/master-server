#include <unordered_map>
#include <ctime>
#include "cube.h"

#include "tools.h"

/* Tools namespace */

void * tools::strmemchr(std::string s, int c, size_t n) {
    if(n == NULL) n = s.length();
    return (char *)memchr(s.c_str(), c, n);
}

std::string tools::strtime(const std::time_t *time) {
    std::string _time = std::asctime(std::localtime(time));

    if(_time.back() == '\n') _time.pop_back();
    return _time;
}

std::vector<std::string> tools::parsecmd(std::string command) {
    std::vector<std::string> mainbuffer;
    std::string segmentbuffer;
    bool quoted = false;

    auto pushbuffer = [&]()
    {
        if(!segmentbuffer.empty())
        {
            mainbuffer.push_back(segmentbuffer);
            segmentbuffer.clear();
        }
    };

    for(size_t i = 0; i < command.size(); i++)
    {
        if(command[i] == '\"'
           && !(i-1 >= 0 && command[i-1] == '\\'))
        {
            quoted = !quoted;
            if(quoted) pushbuffer();
        }
        else if((command[i] == ' ' || command[i] == '\r' || command[i] == '\n') && !quoted)
        {
            pushbuffer();
        }
        else
        {
            segmentbuffer.push_back(command[i]);
        }

    }

    pushbuffer();
    return mainbuffer;
}

/*
std::string tools::resolvepath(std::string relpath)
{
    enum flag
    {
        ESCAPE = 1,
        FORMAT = 2,
    };

    for(auto c : relpath)
    {

    }

}
*/

///////////////////////// network ///////////////////////

// all network traffic is in 32bit ints, which are then compressed using the following simple scheme (assumes that most values are small).

ipmask::ipmask(const std::string &input)
{
    std::vector<uint8_t> values;
    std::string segment;
    bool masked = false;

    values.reserve(5);

    auto pushbuffer = [&]()
    {
        if(!segment.empty())
        {
            if(std::stoi(segment) > 255) throw std::invalid_argument("Malformed IPv4 Address");
            values.push_back(std::stoi(segment));
            segment.clear();
        }
    };

    for(auto & item : input)
    {
        if(item != '.' && item != '/')
        {
            segment.push_back(item);
        }
        else
        {
            pushbuffer();
            if(item == '/')
            {
                if(masked) throw std::invalid_argument("Malformed IPv4 Mask"); // Double mask - invalid
                masked = true;
            }
        }
    }

    pushbuffer();

    if(masked && values.size() < 5) throw std::invalid_argument("Malformed IPv4 Mask"); // Mask is misplaced - invalid
    if(values.size() < 4) throw std::invalid_argument("Malformed IPv4 Address"); // Malformed IP address - invalid

    ipv4 =  values[0] << 24u;
    ipv4 += values[1] << 16u;
    ipv4 += values[2] << 8u;
    ipv4 += values[3] << 0u;
    if(masked) mask = values[4];
}

std::string ipmask::getstring()
{
    std::string buffer = std20::format("%d.%d.%d.%d",
        (ipv4 >> 24u) | 0xFFu,
        (ipv4 >> 16u) | 0xFFu,
        (ipv4 >> 8u ) | 0xFFu,
        (ipv4 >> 0u ) | 0xFFu);

    if(mask < 32) buffer += '/' + std::to_string(mask);

    return buffer;
}