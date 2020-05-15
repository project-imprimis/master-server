#ifdef WIN32
#define FD_SETSIZE 4096
#else
#include <sys/types.h>
#undef __FD_SETSIZE
#define __FD_SETSIZE 4096
#endif

#include <fstream>
#include <vector>
#include <unordered_map>
#include "master.h"
#include "cube.h"
#include <signal.h>
#include <enet/time.h>

struct user
{
    char *name;
    void *pubkey;

    user(char *name, char *pubkey) : name(name), pubkey(pubkey) {}
    user(const user &_user)
    {
        name = _user.name;
        pubkey = _user.pubkey;
    }

    ~user() {}

    static void create(char *name, char *pubkey)
    {
        master::users.try_emplace(name, name, pubkey);
    }

    static void destroy(char *name)
    {
        master::users.erase(name);
    }

    static bool update(char *name, char* newname, void *newpubkey)
    {
        auto result = master::users.find(name);

        if(result == master::users.end())
        { // Not found
            return false;
        }
        else
        {
            if(newname != NULL)
            {
                auto newentry = master::users.extract(name);
                newentry.key() = newname;
                master::users.insert(move(newentry));
                master::users[name].name = newname;
            }
            if(newpubkey != NULL)
            {
                master::users[name].pubkey = newpubkey;
            }
            return true;
        }
        
    }

    static void clear()
    {
        master::users.clear();
    }
};

struct ban
{
    enum type
    {
        CLIENT = 0,
        SERVER = 1,
        GLOBAL = 2
    };

    type bantype;
    ipmask ipaddr;
    char *reason;
    time_t expiry;

    ban(ipmask ipaddr, char *reason = "", time_t expiry = NULL) : ipaddr(ipaddr), reason(reason), expiry(expiry) {}

    ~ban() {}

    static void create(type bantype, ipmask ipaddr, char *reason = "", time_t expiry = NULL)
    {
        master::bans[bantype].try_emplace(ipaddr, ipaddr, reason, expiry);
    }

    static void destroy(type bantype, ipmask ipaddr)
    {
        master::bans[bantype].erase(ipaddr);
    }

    static void clear(type bantype)
    {
        master::bans[bantype].clear();
    }
};

struct authreq
{
    enet_uint32 reqtime;
    uint id;
    void *answer;
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

struct msgbuffer
{
    std::vector<msgbuffer *> &owner; // Probably the client or server who submitted the message?
    std::vector<char> buf;

    int refs; // Document me

    msgbuffer(std::vector<msgbuffer *> &owner) : owner(owner), refs(0) {}

    const char *get()
    {
        return buf.data();
    }

    int size()
    {
        return buf.size();
    }

    const char *getbuf() // Deprecated, use get()
    {
        return get();
    }

    int length() // Deprecated, use size()
    {
        return size();
    }

    void purge();

    bool equals(const msgbuffer &m) const
    {
        return buf.size() == m.buf.size()
            && !memcmp(buf.data(), m.buf.data(), buf.size());
    }

    bool endswith(const msgbuffer &m) const // Deprecate? Comparison should be in the caller function
    {
        return buf.size() >= m.buf.size()
            && !memcmp(&buf[buf.size() - m.buf.size()], m.buf.data(), m.buf.size());
    }

    void concat(const msgbuffer &m)
    {
        // Verify that buf is not already null-terminated.
        // Otherwise, remove nul character
        if(buf.size() && buf.back() == '\0')
        {
            buf.pop_back();
        }
  
      buf.insert(buf.end(), m.buf.begin(), m.buf.end()); // Performance concern?
    }
};

std::vector<msgbuffer *> gameserverlists, gbanlists;
bool updateserverlist = true;

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

    client() : message(NULL), inputpos(0), outputpos(0), servport(-1), lastauth(0), shouldpurge(false), registeredserver(false) {}
};

void purgeclient(int n)
{
    client &c = *clients[n];
    if(c.message)
    {
        c.message->purge();
    }
    enet_socket_destroy(c.socket);
    delete clients[n];
    clients.remove(n);
}

void output(client &c, const char *msg, int len = 0)
{
    if(!len)
    {
        len = strlen(msg);
    }
    c.output.put(msg, len);
}

void outputf(client &c, const char *fmt, ...)
{
    DEFV_FORMAT_STRING(msg, fmt, fmt);

    output(c, msg);
}

bool setuppingsocket(ENetAddress *address)
{
    if(master::pingsocket != ENET_SOCKET_NULL)
    {
        return true;
    }
    master::pingsocket = enet_socket_create(ENET_SOCKET_TYPE_DATAGRAM);
    if(master::pingsocket == ENET_SOCKET_NULL)
    {
        return false;
    }
    if(address && enet_socket_bind(master::pingsocket, address) < 0)
    {
        return false;
    }
    enet_socket_set_option(master::pingsocket, ENET_SOCKOPT_NONBLOCK, 1);
    return true;
}

void setupserver(int port, const char *ip = NULL)
{
    ENetAddress address;
    address.host = ENET_HOST_ANY;
    address.port = port;

    if(ip)
    {
        if(enet_address_set_host(&address, ip) < 0)
        {
            io::fatal("failed to resolve server address: %s", ip);
        }
    }
    serversocket = enet_socket_create(ENET_SOCKET_TYPE_STREAM);
    if(serversocket==ENET_SOCKET_NULL)
    {
        fatal("failed to bind socket: null socket error");
    }
    if(enet_socket_set_option(serversocket, ENET_SOCKOPT_REUSEADDR, 1) < 0)
    {
        fatal("failed to bind socket: reuseaddr error");
    }
    if(enet_socket_bind(serversocket, &address) < 0 ||
       enet_socket_listen(serversocket, -1) < 0)
    {
        fatal("failed to bind socket");
    }
    if(enet_socket_set_option(serversocket, ENET_SOCKOPT_NONBLOCK, 1)<0)
    {
        fatal("failed to make server socket non-blocking");
    }
    if(!setuppingsocket(&address))
    {
        fatal("failed to create ping socket");
    }
    enet_time_set(0);
    starttime = time(NULL);
    char *ct = ctime(&starttime);
    if(strchr(ct, '\n'))
    {
        *strchr(ct, '\n') = '\0';
    }
    conoutf("*** Starting master server on %s %d at %s ***", ip ? ip : "localhost", port, ct);
}

void genserverlist()
{
    if(!updateserverlist)
    {
        return;
    }
    while(gameserverlists.length() && gameserverlists.last()->refs<=0)
    {
        delete gameserverlists.pop();
    }
    msgbuffer *l = new msgbuffer(gameserverlists);
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
    gameserverlists.add(l);
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

void addgameserver(client &c)
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

bool checkclientinput(client &c)
{
    if(c.inputpos<0)
    {
        return true;
    }
    char *end = (char *)memchr(c.input, '\n', c.inputpos);
    while(end)
    {
        *end++ = '\0';
        c.lastinput = servtime;
        int port;
        uint id;
        string user, val;
        if(!strncmp(c.input, "list", 4) && (!c.input[4] || c.input[4] == '\n' || c.input[4] == '\r'))
        {
            genserverlist();
            if(gameserverlists.empty() || c.message)
            {
                return false;
            }
            c.message = gameserverlists.last();
            c.message->refs++;
            c.output.setsize(0);
            c.outputpos = 0;
            c.shouldpurge = true;
            return true;
        }
        else if(sscanf(c.input, "regserv %d", &port) == 1)
        {
            if(checkban(servbans, c.address.host))
            {
                return false;
            }
            if(port < 0 || port > 0xFFFF || (c.servport >= 0 && port != c.servport))
            {
                outputf(c, "failreg invalid port\n");
            }
            else
            {
                c.servport = port;
                addgameserver(c);
            }
        }
        else if(sscanf(c.input, "reqauth %u %100s", &id, user) == 2)
        {
            reqauth(c, id, user);
        }
        else if(sscanf(c.input, "confauth %u %100s", &id, val) == 2)
        {
            confauth(c, id, val);
        }
        c.inputpos = &c.input[c.inputpos] - end;
        memmove(c.input, end, c.inputpos);

        end = (char *)memchr(c.input, '\n', c.inputpos);
    }
    return c.inputpos < static_cast<int>(sizeof(c.input));
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