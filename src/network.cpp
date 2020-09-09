#include "network.h"
#include "io.h"

#include <cstring>
#include <iostream>
#include <enet/enet.h>
#include <enet/unix.h>
#include <ctime>

basic_server::basic_server(hostaddr ip, hostport port) : ip(ip), port(port)
{
    ENetAddress address;
    address.host = ENET_HOST_ANY;
    address.port = port;

    if(ip && enet_address_set_host(&address, ip) < 0)
    {
        io::fatal("Failed to resolve server address: %s", ip);
    }

    if(!initpingsocket(&address))
    {
        io::fatal("Failed to create ping socket");
    }

    if(!initserversocket(&address))
    {
        io::fatal("Failed to create server socket");
    }

    // Set start time to now
    enet_time_set(0);
    starttime = std::time(nullptr);
}

/**
 * @brief Initialize a socket for ping purposes
 * @param address ENetAddress-formatted value containing the server's address
 * @return Success status
 */
bool basic_server::initpingsocket(ENetAddress *address)
{
    if(pingsocket != ENET_SOCKET_NULL)
    {
        io::lprintf(LogLevel::Debug, "initpingsocket: Already set up");
        return true;
    }

    pingsocket = enet_socket_create(ENET_SOCKET_TYPE_DATAGRAM);

    if(pingsocket == ENET_SOCKET_NULL)
    {
        io::lprintf(LogLevel::Debug, "initpingsocket: Ping socket is NULL");
        return false;
    }

    if(address && enet_socket_bind(pingsocket, address) < 0)
    {
        io::lprintf(LogLevel::Debug, "initpingsocket: Failed to bind");
        return false;
    }

    enet_socket_set_option(pingsocket, ENET_SOCKOPT_NONBLOCK, 1);

    return true;
}

/**
 * @brief Initialize a socket for communication purposes
 * @param address ENetAddress-formatted value containing the server's address
 * @return Success status
 */
bool basic_server::initserversocket(ENetAddress *address)
{

    if(serversocket != ENET_SOCKET_NULL)
    {
        io::lprintf(LogLevel::Debug, "initserversocket: Already set up");
        return true;
    }

    serversocket = enet_socket_create(ENET_SOCKET_TYPE_STREAM);

    if(serversocket == ENET_SOCKET_NULL)
    {
        io::lprintf(LogLevel::Debug, "initserversocket: Server socket is NULL");
        return false;
    }

    if(enet_socket_set_option(serversocket, ENET_SOCKOPT_REUSEADDR, 1) < 0)
    {
        io::lprintf(LogLevel::Debug, "initserversocket: Couldn't set REUSEADDR option");
        return false;
    }

    if(address && enet_socket_bind(serversocket, address) < 0
       || enet_socket_listen(serversocket, -1) < 0)
    {
        io::lprintf(LogLevel::Debug, "initserversocket: Failed to bind");
        return false;
    }

    if(enet_socket_set_option(serversocket, ENET_SOCKOPT_NONBLOCK, 1) < 0)
    {
        io::lprintf(LogLevel::Debug, "initserversocket: Couldn't set NONBLOCK option");
        return false;
    }

    return true;
}

void basic_server::runloop() {
    servertime = enet_time_get();
}

/** Types **/

bool netmessage_output::send(ENetSocket socket)
{

    // Copy all items from the queue into buf.data
    // Update bytesleft based on the length of each message
    for(std::string const &message : messages)
    {
        strncat((char *)buf.data, message.c_str(), message.length());
        bytesleft += message.length();
        messages.pop_front(); // Remove first item, problematic?
    }

    // Synchronize
    buf.dataLength = bytesleft;

    if(buf.dataLength > 0)
    {
        int result = enet_socket_send(socket, nullptr, &buf, 1);
        bytesleft -= result;

        // Nothing was sent even though data was available
        // Client is dead
        if(result <= 0) return false;
    }

    return true;
}

bool netmessage_input::receive(ENetSocket socket)
{

    buf.dataLength = 4096; // INPUT_LIMIT
    int result = enet_socket_receive(socket, nullptr, &buf, 1);

    if(result > 0)
    {
        messages.emplace_back((char *)buf.data);
    }
    else
    {
        return false;
    }

    return true;
}