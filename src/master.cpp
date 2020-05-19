#ifdef WIN32
#define FD_SETSIZE 4096
#else
#include <sys/types.h>
#undef __FD_SETSIZE
#define __FD_SETSIZE 4096
#endif

#include "master.h"

#include <fstream>
#include <vector>
#include <unordered_map>
#include "cube.h"
#include "io.h"
#include <signal.h>
#include <enet/time.h>

client::client() : message(nullptr), inputpos(0), outputpos(0), servport(-1), lastauth(0), shoulddestroy(false), isregisteredserver(false) {}

client::~client()
{
    enet_socket_destroy(socket);
}

bool client::readinput()
{
    // No new input
    if(inputpos < 0) return true;

    // Count number of bytes until \n is found
    // Returns a memory pointer to the first occurence of \n
    char *end = (char *)memchr(input, '\n', inputpos);

    // Until the end pointer is nullptr
    // This allows us to read multiple commands in one go
    while(end)
    {
        // Replace \n with \0 and then increment pointer by 1
        *end++ = '\0';
        // Set last input to current time
        lastinput = master::servtime;
        // Port of the server currently registering
        int port;
        // Auth ID, not sure what its purpose is
        uint id;
        // Document me
        std::string user;
        // Document me
        std::string val;

        // Check that first 4 characters match "list" and that there's nothing afterwards
        //if(!strncmp(input, "list", 4) && (!input[4] || input[4] == '\n' || input[4] == '\r'))
        if(!_input.rfind("list", 0) && (!_input[4] || input[4] == '\n' || input[4] == '\r'))
        {
            // Generate a server list
            genserverlist();

            // Do nothing if no gameservers are present or if there is a... new message?
            if(master::gameserverlists.empty() || message)
            {
                return false;
            }
            // Get the last gameserver on the list and attribute it as the message.
            message = master::gameserverlists.back();

            // Clear (delete, wipe) the output string
            output.clear();
            // Reset the output position to 0
            outputpos = 0;
            // Mark client for deletion and disconnection
            shoulddestroy = true;

            // Return successfully
            return true;
        }
        else if(sscanf(input, "regserv %d", &port) == 1)
        {
            if(checkban(servbans, address.host))
            {
                return false;
            }
            if(port < 0 || port > 0xFFFF || (servport >= 0 && port != servport))
            {
                outputf(c, "failreg invalid port\n");
            }
            else
            {
                servport = port;
                master::addgameserver(this);
            }
        }
        inputpos = &input[inputpos] - end;
        memmove(input, end, inputpos);

        end = (char *)memchr(input, '\n', inputpos);
    }

    return inputpos < static_cast<int>(sizeof(input));
}

static void client::destroy(int n)
{
    delete master::clients[n];
    master::clients.erase(n);
}

bool master::configpingsocket(ENetAddress *address)
{
    // Ping socket is already set up
    if(pingsocket != ENET_SOCKET_NULL)
    {
        return true;
    }

    pingsocket = enet_socket_create(ENET_SOCKET_TYPE_DATAGRAM);

    // Socket is STILL not set up
    if(pingsocket == ENET_SOCKET_NULL)
    {
        return false;
    }

    // Couldn't bind anything to the socket
    if(address && enet_socket_bind(pingsocket, address) < 0)
    {
        return false;
    }

    enet_socket_set_option(pingsocket, ENET_SOCKOPT_NONBLOCK, 1);

    return true;
}

void master::init(int port, const char *ip = nullptr)
{
    ENetAddress address;
    address.host = ENET_HOST_ANY;
    address.port = port;

    if(ip && enet_address_set_host(&address, ip) < 0)
    {
        io::fatal("Failed to resolve server address: %s", ip);
    }

    serversocket = enet_socket_create(ENET_SOCKET_TYPE_STREAM);
    
    if(serversocket==ENET_SOCKET_NULL)
    {
        io::fatal("Failed to bind socket: null socket error");
    }

    if(enet_socket_set_option(serversocket, ENET_SOCKOPT_REUSEADDR, 1) < 0)
    {
        io::fatal("Failed to bind socket: reuseaddr error");
    }

    if(enet_socket_bind(serversocket, &address) < 0
        || enet_socket_listen(serversocket, -1) < 0)
    {
        io::fatal("Failed to bind socket");
    }

    if(enet_socket_set_option(serversocket, ENET_SOCKOPT_NONBLOCK, 1)<0)
    {
        io::fatal("Failed to make server socket non-blocking");
    }

    if(!configpingsocket(&address))
    {
        io::fatal("Failed to create ping socket");
    }

    enet_time_set(0);
    starttime = time(NULL);
    char *ct = ctime(&starttime);

    // Add terminating null character if given a newline instead
    if(strchr(ct, '\n'))
    {
        *strchr(ct, '\n') = '\0';
    }

    io::lprintf(LogLevel::Info, "*** Starting master server on %s %d at %s ***", ip ? ip : "localhost", port, ct);
}

void genserverlist()
{
    if(!updateserverlist)
    {
        return;
    }
    while(master::gameserverlists.length() && master::gameserverlists.last()->refs<=0)
    {
        delete master::gameserverlists.pop();
    }
    msgbuffer *l = new msgbuffer(master::gameserverlists);
    for(int i = 0; i < gameservers.length(); i++)
    {
        gameserver &s = *gameservers[i];
        if(!s.lastpong)
        {
            continue;
        }
        DEF_FORMAT_STRING(cmd, "addserver %s %d\n", s.ip, s.port);
        l->buf.put(cmd, strlen(cmd));
    }
    l->buf.add('\0');
    master::gameserverlists.add(l);
    updateserverlist = false;
}

void gengbanlist()
{
    msgbuffer *l = new msgbuffer(gbanlists);
    const char *header = "cleargbans\n";
    l->buf.put(header, strlen(header));
    string cmd = "addgban ";
    int cmdlen = strlen(cmd);
    for(int i = 0; i < gbans.length(); i++)
    {
        ipmask &b = gbans[i];
        l->buf.put(cmd, cmdlen + b.print(&cmd[cmdlen]));        
        l->buf.add('\n');
    }
    if(gbanlists.length() && gbanlists.last()->equals(*l))
    {
        delete l;
        return;
    }
    while(gbanlists.length() && gbanlists.last()->refs<=0)
    {
        delete gbanlists.pop();
    }
    for(int i = 0; i < gbanlists.length(); i++)
    {
        msgbuffer *m = gbanlists[i];
        if(m->refs > 0 && !m->endswith(*l))
        {
            m->concat(*l);
        }
    }
    gbanlists.add(l);
    for(int i = 0; i < clients.length(); i++)
    {
        client &c = *clients[i];
        if(c.servport >= 0 && !c.message)
        {
            c.message = l;
            c.message->refs++;
        }
    }
}

void master::addgameserver(client &c)
{
    if(gameservers.length() >= SERVER_LIMIT)
    {
        return;
    }
    int dups = 0;
    for(int i = 0; i < gameservers.length(); i++)
    {
        gameserver &s = *gameservers[i];
        if(s.address.host != c.address.host)
        {
            continue;
        }
        ++dups; 
        if(s.port == c.servport)
        {
            s.lastping = 0;
            s.numpings = 0;
            return;
        }
    }
    if(dups >= SERVER_DUP_LIMIT)
    {
        outputf(c, "failreg too many servers on ip\n");
        return;
    }
    string hostname;
    if(enet_address_get_host_ip(&c.address, hostname, sizeof(hostname)) < 0)
    {
        outputf(c, "failreg failed resolving ip\n");
        return;
    }
    gameserver &s = *gameservers.add(new gameserver);
    s.address.host = c.address.host;
    s.address.port = c.servport;
    copystring(s.ip, hostname);
    s.port = c.servport;
    s.numpings = 0;
    s.lastping = s.lastpong = 0;
}

client *findclient(gameserver &s)
{
    for(int i = 0; i < clients.length(); i++)
    {
        client &c = *clients[i];
        if(s.address.host == c.address.host && s.port == c.servport)
        {
            return &c;
        }
    }
    return NULL;
}

void servermessage(gameserver &s, const char *msg)
{
    client *c = findclient(s);
    if(c)
    {
        outputf(*c, msg);
    }
}

void checkserverpongs()
{
    ENetBuffer buf;
    ENetAddress addr;
    static uchar pong[MAXTRANS];
    for(;;)
    {
        buf.data = pong;
        buf.dataLength = sizeof(pong);
        int len = enet_socket_receive(pingsocket, &addr, &buf, 1);
        if(len <= 0)
        {
            break;
        }
        for(int i = 0; i < gameservers.length(); i++)
        {
            gameserver &s = *gameservers[i];
            if(s.address.host == addr.host && s.address.port == addr.port)
            {
                if(s.lastping && (!s.lastpong || ENET_TIME_GREATER(s.lastping, s.lastpong)))
                {
                    client *c = findclient(s);
                    if(c)
                    {
                        c->registeredserver = true;
                        outputf(*c, "succreg\n");
                        if(!c->message && gbanlists.length())
                        {
                            c->message = gbanlists.last();
                            c->message->refs++;
                        }
                    }
                }
                if(!s.lastpong)
                {
                    updateserverlist = true;
                }
                s.lastpong = servtime ? servtime : 1;
                break;
            }
        }
    }
}

void bangameservers()
{
    for(int i = gameservers.length(); --i >=0;) //note reverse iteration
    {
        if(checkban(servbans, gameservers[i]->address.host))
        {
            delete gameservers.remove(i);
            updateserverlist = true;
        }
    }
}

void checkgameservers()
{
    ENetBuffer buf;
    for(int i = 0; i < gameservers.length(); i++)
    {
        gameserver &s = *gameservers[i];
        if(s.lastping && s.lastpong && ENET_TIME_LESS_EQUAL(s.lastping, s.lastpong))
        {
            if(ENET_TIME_DIFFERENCE(servtime, s.lastpong) > KEEPALIVE_TIME)
            {
                delete gameservers.remove(i--);
                updateserverlist = true;
            }
        }
        else if(!s.lastping || ENET_TIME_DIFFERENCE(servtime, s.lastping) > PING_TIME)
        {
            if(s.numpings >= PING_RETRY)
            {
                servermessage(s, "failreg failed pinging server\n");
                delete gameservers.remove(i--);
                updateserverlist = true;
            }
            else
            {
                static const uchar ping[] = { 0xFF, 0xFF, 1 };
                buf.data = (void *)ping;
                buf.dataLength = sizeof(ping);
                s.numpings++;
                s.lastping = servtime ? servtime : 1;
                enet_socket_send(pingsocket, &s.address, &buf, 1);
            }
        }
    }
}

void msgbuffer::purge()
{
    refs = max(refs - 1, 0);
    if(refs<=0 && owner.last()!=this)
    {
        owner.removeobj(this);
        delete this;
    }
}

void purgeauths(client &c)
{
    int expired = 0;
    for(int i = 0; i < c.authreqs.length(); i++)
    {
        if(ENET_TIME_DIFFERENCE(servtime, c.authreqs[i].reqtime) >= AUTH_TIME)
        {
            outputf(c, "failauth %u\n", c.authreqs[i].id);
            freechallenge(c.authreqs[i].answer);
            expired = i + 1;
        }
        else
        {
            break;
        }
    }
    if(expired > 0)
    {
        c.authreqs.remove(0, expired);
    }
}

void reqauth(client &c, uint id, char *name)
{
    if(ENET_TIME_DIFFERENCE(servtime, c.lastauth) < AUTH_THROTTLE)
    {
        return;
    }
    c.lastauth = servtime;
    purgeauths(c);

    time_t t = time(NULL);
    char *ct = ctime(&t);
    if(ct)
    {
        char *newline = strchr(ct, '\n');
        if(newline)
        {
            *newline = '\0';
        }
    }
    string ip;
    if(enet_address_get_host_ip(&c.address, ip, sizeof(ip)) < 0)
    {
        copystring(ip, "-");
    }
    conoutf("%s: attempting \"%s\" as %u from %s", ct ? ct : "-", name, id, ip);

    userinfo *u = users.access(name);
    if(!u)
    {
        outputf(c, "failauth %u\n", id);
        return;
    }

    if(c.authreqs.length() >= AUTH_LIMIT)
    {
        outputf(c, "failauth %u\n", c.authreqs[0].id);
        freechallenge(c.authreqs[0].answer);
        c.authreqs.remove(0);
    }

    authreq &a = c.authreqs.add();
    a.reqtime = servtime;
    a.id = id;
    uint seed[3] = { uint(starttime), servtime, uint(rand()) };
    static vector<char> buf;
    buf.setsize(0);
    a.answer = genchallenge(u->pubkey, seed, sizeof(seed), buf);

    outputf(c, "chalauth %u %s\n", id, buf.getbuf());
}

void confauth(client &c, uint id, const char *val)
{
    purgeauths(c);

    for(int i = 0; i < c.authreqs.length(); i++)
    {
        if(c.authreqs[i].id == id)
        {
            string ip;
            if(enet_address_get_host_ip(&c.address, ip, sizeof(ip)) < 0)
            {
                copystring(ip, "-");
            }
            if(checkchallenge(val, c.authreqs[i].answer))
            {
                outputf(c, "succauth %u\n", id);
                conoutf("succeeded %u from %s", id, ip);
            }
            else
            {
                outputf(c, "failauth %u\n", id);
                conoutf("failed %u from %s", id, ip);
            }
            freechallenge(c.authreqs[i].answer);
            c.authreqs.remove(i--);
            return;
        }
    }
    outputf(c, "failauth %u\n", id);
}

ENetSocketSet readset, writeset;

void checkclients()
{
    ENetSocketSet readset, writeset;
    ENetSocket maxsock = max(serversocket, pingsocket);
    ENET_SOCKETSET_EMPTY(readset);
    ENET_SOCKETSET_EMPTY(writeset);
    ENET_SOCKETSET_ADD(readset, serversocket);
    ENET_SOCKETSET_ADD(readset, pingsocket);
    for(int i = 0; i < clients.length(); i++)
    {
        client &c = *clients[i];
        if(c.authreqs.length())
        {
            purgeauths(c);
        }
        if(c.message || c.output.length())
        {
            ENET_SOCKETSET_ADD(writeset, c.socket);
        }
        else
        {
            ENET_SOCKETSET_ADD(readset, c.socket);
        }
        maxsock = max(maxsock, c.socket);
    }
    if(enet_socketset_select(maxsock, &readset, &writeset, 1000)<=0)
    {
        return;
    }
    if(ENET_SOCKETSET_CHECK(readset, pingsocket))
    {
        checkserverpongs();
    }
    if(ENET_SOCKETSET_CHECK(readset, serversocket))
    {
        ENetAddress address;
        ENetSocket clientsocket = enet_socket_accept(serversocket, &address);
        if(clients.length()>=CLIENT_LIMIT || checkban(bans, address.host))
        {
            enet_socket_destroy(clientsocket);
        }
        else if(clientsocket!=ENET_SOCKET_NULL)
        {
            int dups = 0,
                oldest = -1;
            for(int i = 0; i < clients.length(); i++)
            {
                if(clients[i]->address.host == address.host)
                {
                    dups++;
                    if(oldest<0 || clients[i]->connecttime < clients[oldest]->connecttime)
                    {
                        oldest = i;
                    }
                }
            }
            if(dups >= DUP_LIMIT)
            {
                purgeclient(oldest);
            }
            client *c = new client;
            c->address = address;
            c->socket = clientsocket;
            c->connecttime = servtime;
            c->lastinput = servtime;
            clients.add(c);
        }
    }

    for(int i = 0; i < clients.length(); i++)
    {
        client &c = *clients[i];
        if((c.message || c.output.length()) && ENET_SOCKETSET_CHECK(writeset, c.socket))
        {
            const char *data = c.output.length() ? c.output.getbuf() : c.message->getbuf();
            int len = c.output.length() ? c.output.length() : c.message->length();
            ENetBuffer buf;
            buf.data = (void *)&data[c.outputpos];
            buf.dataLength = len-c.outputpos;
            int res = enet_socket_send(c.socket, NULL, &buf, 1);
            if(res>=0)
            {
                c.outputpos += res;
                if(c.outputpos>=len)
                {
                    if(c.output.length())
                    {
                        c.output.setsize(0);
                    }
                    else
                    {
                        c.message->purge();
                        c.message = NULL;
                    }
                    c.outputpos = 0;
                    if(!c.message && c.output.empty() && c.shouldpurge)
                    {
                        purgeclient(i--);
                        continue;
                    }
                }
            }
            else
            {
                purgeclient(i--);
                continue;
            }
        }
        if(ENET_SOCKETSET_CHECK(readset, c.socket))
        {
            ENetBuffer buf;
            buf.data = &c.input[c.inputpos];
            buf.dataLength = sizeof(c.input) - c.inputpos;
            int res = enet_socket_receive(c.socket, NULL, &buf, 1);
            if(res>0)
            {
                c.inputpos += res;
                c.input[min(c.inputpos, static_cast<int>(sizeof(c.input)-1))] = '\0';
                if(!checkclientinput(c))
                {
                    purgeclient(i--);
                    continue;
                }
            }
            else
            {
                purgeclient(i--);
                continue;
            }
        }
        if(c.output.length() > OUTPUT_LIMIT)
        {
            purgeclient(i--);
            continue;
        }
        if(ENET_TIME_DIFFERENCE(servtime, c.lastinput) >= (c.registeredserver ? KEEPALIVE_TIME : CLIENT_TIME))
        {
            purgeclient(i--);
            continue;
        }
    }
}

void banclients()
{
    for(int i = clients.length(); --i >=0;) //note reverse iteration
    {
        if(checkban(bans, clients[i]->address.host))
        {
            purgeclient(i);
        }
    }
}