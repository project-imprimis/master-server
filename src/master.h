#ifndef _MASTER_H
#define _MASTER_H

#include <fstream>
#include <vector>
#include <unordered_map>
#include "tools.h"
#include <enet/enet.h>
#include <enet/time.h>

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
    char *name;
    void *pubkey; // Replace, only store private key, public key should be generated on the fly as a 128-bit TOTP string

    user(char *name, char *pubkey);
    user(const user &_user);

    ~user();

    // Add a user and add them to the list of users, replaces adduser()
    static void create(char *name, char *pubkey);

    // Destroys a user based on their username
    static void destroy(char *name);

    // Renames or changes a user's key
    static bool update(char *name, char* newname, void *newpubkey);

    // Clears the entire user map, replaces clearusers()
    static void clear();
};

struct ban
{
    enum type
    {
        CLIENT,
        SERVER,
        GLOBAL
    };

    ipmask ipaddr;
    char *reason;
    time_t expiry;

    ban(ipmask ipaddr, char *reason = "", time_t expiry = NULL);

    ~ban();

    static void create(type bantype, ipmask ipaddr, char *reason = "", time_t expiry = NULL);

    static void destroy(type bantype, ipmask ipaddr);
};

struct authreq
{
    enet_uint32 reqtime;
    uint id;
    void *answer;
};

struct msgbuffer
{
    std::vector<msgbuffer *> &owner;
    std::vector<char> buf;

    int refs;

    msgbuffer(std::vector<msgbuffer *> &owner) : owner(owner), refs(0) {}

    /*
    Get the current contents of the msgbuffer vector
    */
    const char *get();

    /*
    Get the current size of the msgbuffer vector
    */
    int size();

    /*
    Purge the entire msgbuffer vector
    */
    void purge();

    /*
    Verify equality between two msgbuffer objects
    */
    bool equals(const msgbuffer &m) const;

    /*
    Deprecate? Comparison should be in the caller function
    */
    bool endswith(const msgbuffer &m) const;

    /*
    Concatenate another msgbuffer object with this one
    */
    void concat(const msgbuffer &m);


    /* DEPRECATED */

    // Use get() instead
    const char *getbuf();

    // Use size() instead
    int length();
};

struct gameserver
{
    ENetAddress address;
    std::string ipaddr;
    int port,
        numpings;
    enet_uint32 lastping,
        lastpong;
};

struct client
{
    ENetAddress address;
    ENetSocket socket;
    char input[INPUT_LIMIT];
    msgbuffer *message;
    std::vector<char> output;
    int inputpos,
        outputpos;
    enet_uint32 connecttime,
        lastinput,
        lastauth;
    int servport;
    std::vector<authreq> authreqs;
    bool shouldpurge;
    bool registeredserver;

    client();

};

namespace master
{
    std::ofstream logfile;
    ENetSocket serversocket = ENET_SOCKET_NULL,
        pingsocket = ENET_SOCKET_NULL;
    ENetSocketSet readset,
        writeset;
    time_t starttime;
    enet_uint32 servtime = 0;

    std::vector<ipmask> userbans,
        serverbans,
        globalbans;

    std::unordered_map<ipmask, ban> bans[3]; // One for each ban::type
    std::unordered_map<char *, user> users;
    std::vector<client *> clients;
    std::vector<gameserver *> gameservers;
    std::vector<msgbuffer *> gameserverlists,
        gbanlists;

    bool updateserverlist = true;
};

#endif