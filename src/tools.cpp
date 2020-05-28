// implementation of network tools

#include <sstream>
#include "cube.h"

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