#ifndef _MASTER_H
#define _MASTER_H

#include <fstream>
#include <utility>
#include <vector>
#include <unordered_map>
#include "tools.h"
#include <enet/enet.h>
#include <enet/time.h> // Replace with ctime?
#include <ctime>

#define INPUT_LIMIT 4096            // Document
#define OUTPUT_LIMIT (64*1024)      // Document

#define AUTH_TIME (30*1000)         // Document
#define AUTH_LIMIT 100              // Document
#define AUTH_THROTTLE 1000          // Document

#define CLIENT_TIME (3*60*1000)     // Document
#define CLIENT_LIMIT 4096           // Document
#define DUP_LIMIT 16                // Document

#define SERVER_LIMIT 4096           // Maximum amount of servers connected
#define SERVER_DUP_LIMIT 10         // Maximum amount of duplicate server entries
#define MAXTRANS 5000               // Maximum amount of data to allow at once

#define PING_TIME 3000              // Maximum ping time for the connection
#define PING_RETRY 5                // Maximum amount of ping tries allowed
#define KEEPALIVE_TIME (65*60*1000) // Maximum time to keep a connection open

struct user
{
    std::string name;
    std::string privkey;

    user(std::string name, std::string privkey) : name(std::move(name)), privkey(std::move(privkey)) {}
};

struct ban
{
    enum type
    {
        CLIENT,
        SERVER,
        GLOBAL
    };

    type bantype;
    ipmask ipaddr;
    std::string reason;
    time_t expiry;

    ban(ipmask ipaddr, std::string reason = "", time_t expiry = NULL);
};

struct authreq
{
    enet_uint32 reqtime;
    uint id;
    void *answer;
};

typedef class buffer_node: public std::string
{
    /*
    Reference to the parents of the msgbuffer object
    */
    std::vector<buffer_node *> &owner;

public:

    template<class... Ts>
    buffer_node(std::vector<buffer_node *> &owner, Ts&&... args) : std::string(std::forward<Ts>(args)...), owner(owner) {}

    template<class T>
    buffer_node(std::vector<buffer_node *> &owner, std::initializer_list<T> args) : std::string(args), owner(owner) {}
} msgbuffer;

struct gameserver
{
    ENetAddress address;
    std::string ipaddr;
    int port;
    int numpings;
    enet_uint32 lastping;
    enet_uint32 lastpong;
};

struct client
{
    ENetAddress address{};
    ENetSocket socket{};

    std::string _input;
    char input[INPUT_LIMIT]{};
    msgbuffer *message;
    std::string output;
    size_t inputpos;
    size_t outputpos;
    enet_uint32 connecttime{},
        lastinput{},
        lastauth;
    int servport;
    std::vector<authreq> authreqs;

    /* Flag client for destruction */
    bool shoulddestroy;

    /* Is the client a registered server? */
    bool isregisteredserver;

    client();

    ~client();

    /**
     * Read client input, return success value.
     *
     * @return true, false on error
     * @replaces checkclientinput
     */
    bool readinput();

    /**
     * Sends a network message to the client object.
     * Supports C-style formatting
     * @param format    C string to format
     * @param args      List of replacements to perform
     * @replaces output
     * @replaces outputf
     */
    template<typename... Args>
    void sendnetmsg(const char *format, Args... args)
    {
        output += std20::format(format, args...);
    }

    /**
     * Delete a client index and remove it from the list.
     * @return void
     * @replaces purgeclient
     */
    static void destroy(int n);

};

namespace master
{
    std::ofstream logfile;
    ENetSocket serversocket = ENET_SOCKET_NULL;
    ENetSocket pingsocket = ENET_SOCKET_NULL;
    ENetSocketSet readset,
        writeset;
    time_t starttime;
    enet_uint32 servtime = 0;

    std::vector<ipmask> userbans,
        serverbans,
        globalbans;

    ENetSocketSet readset, writeset;

    std::unordered_map<std::string, std::unordered_map<ipmask, ban>> bans = {
            {"user", {}},
            {"server", {}},
            {"global", {}}
    };
    std::unordered_map<std::string, user> users;
    std::vector<client *> clients;
    std::vector<gameserver *> gameservers;
    std::vector<msgbuffer *> gameserverlists,
        gbanlists;

    bool updateserverlist = true;

    /*
    Sets up the ping socket
    Replaces ``configpingsocket``
    */
    bool configpingsocket(ENetAddress *address);

    /**
     * Adds a client as a game server
     */
    void addgameserver(client &c);

    /*
    Initialize the master server
    Replaces ``setupserver``
    */
    void init(int port, const char *ip);
};

#endif