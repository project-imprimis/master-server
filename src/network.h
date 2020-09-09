#ifndef _NETWORK_H_
#define _NETWORK_H_

#include <iostream>
#include <deque>
#include <enet/unix.h>
#include <enet/types.h>
#include <enet/enet.h>
#include "types.h"

/**
 * @brief Base code for a server structure using enet
 */
class basic_server
{
protected:
    hostaddr ip;
    hostport port;

    ENetSocket  serversocket    = ENET_SOCKET_NULL;
    ENetSocket  pingsocket      = ENET_SOCKET_NULL;
    time_t      starttime       = 0;
    enet_uint32 servertime      = 0;

    bool initpingsocket(ENetAddress *address);
    bool initserversocket(ENetAddress *address);
public:
    basic_server(hostaddr ip, hostport port);

    void runloop();
};

/**
 * @brief Stores network messages as a queue to either send out or receive.
 *
 * @warning DO NOT USE THE SAME OBJECT FOR BOTH SENDING AND RECEIVING!
 * @warning Dire things will happen!
 */
class basic_netmessage
{
protected:
    ENetBuffer buf{};
public:
    std::deque<std::string> messages;
    ssize_t bytesleft = 0;

    basic_netmessage() = default;
    basic_netmessage(basic_netmessage const &clone) :
            messages(clone.messages) {}
    explicit basic_netmessage(std::deque<std::string> messages) :
            messages(std::move(messages)) {}
};

class netmessage_input: public basic_netmessage
{
public:
    bool receive(ENetSocket socket);
};

class netmessage_output: public basic_netmessage
{
public:
    bool send(ENetSocket socket);
};


#endif
