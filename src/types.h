#ifndef _TYPES_H_
#define _TYPES_H_

#include <bitset>
#include <climits>

/** Basic Types **/

typedef const char * hostaddr;
typedef uint16_t hostport;


/** ipmask **/

struct ipmask
{
    uint32_t ipv4;
    uint8_t mask; // Also the IPv6 prefix

    bool operator==(const ipmask &comparative) const
    {
        return (ipv4 == comparative.ipv4 &&
                mask == comparative.mask);
    }

    explicit ipmask(const std::string &input);
    explicit ipmask(uint32_t ipv4, uint8_t mask = 32) : ipv4(ipv4), mask(mask) {};
    ipmask() : ipv4(0), mask(0) {};

    std::string getstring();
    bool match(uint32_t host) const
    {
        return (host & mask) == ipv4;
    }
};

template<>
struct std::hash<ipmask>
{
    std::size_t operator()(const ipmask &ip)
    {
        constexpr std::size_t N = (sizeof(ip.mask) + sizeof(ip.ipv4)) * CHAR_BIT;
        std::bitset<N> data{ip.mask};
        data <<= sizeof(ip.ipv4) * CHAR_BIT;
        data |= ip.ipv4;
        return std::hash<std::bitset<N>>{}(data);
    }
};

#endif
