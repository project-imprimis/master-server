#include <sys/types.h>

#include "cube.h"
#include <signal.h>
#include <enet/etime.h>

constexpr unsigned int INPUT_LIMIT = 4096;
constexpr unsigned int OUTPUT_LIMIT = (64*1024);
constexpr unsigned int CLIENT_TIME = (3*60*1000);
constexpr unsigned int CLIENT_LIMIT = 4096;
constexpr unsigned int DUP_LIMIT = 16;
constexpr unsigned int PING_TIME = 3000;
constexpr unsigned int PING_RETRY = 5;
constexpr unsigned int KEEPALIVE_TIME = (65*60*1000);
constexpr unsigned int SERVER_LIMIT = 4096;
constexpr unsigned int SERVER_DUP_LIMIT = 10;
constexpr unsigned int MAXTRANS = 5000;                  // max amount of data to swallow in 1 go

FILE *logfile = nullptr;

std::vector<ipmask> bans, servbans, gbans;

void addban(std::vector<ipmask> &bans, const char *name)
{
    ipmask ban;
    ban.parse(name);
    bans.push_back(ban);
}

bool checkban(std::vector<ipmask> &bans, enet_uint32 host)
{
    for(uint i = 0; i < bans.size(); i++)
    {
        if(bans.at(i).check(host))
        {
            return true;
        }
    }
    return false;
}

struct gameserver
{
    ENetAddress address;
    string ip;
    int port, numpings;
    enet_uint32 lastping, lastpong;
};
std::vector<gameserver *> gameservers;

struct messagebuf
{
    std::vector<messagebuf *> &owner;
    std::vector<char> buf;
    int refs;

    messagebuf(std::vector<messagebuf *> &owner) : owner(owner), refs(0) {}

    const char *getbuf()
    {
        return buf.data();
    }

    int length()
    {
        return buf.size();
    }
    void purge();

    bool equals(const messagebuf &m) const
    {
        return buf.size() == m.buf.size() && !memcmp(buf.data(), m.buf.data(), buf.size());
    }

    bool endswith(const messagebuf &m) const
    {
        return buf.size() >= m.buf.size() && !memcmp(&buf[buf.size() - m.buf.size()], m.buf.data(), m.buf.size());
    }

    void concat(const messagebuf &m)
    {
        if(buf.size() && buf.back() == '\0')
        {
            buf.pop_back();
        }
        buf.insert(buf.end(), m.buf.begin(), m.buf.end());
    }
};
std::vector<messagebuf *> gameserverlists, gbanlists;
bool updateserverlist = true;

struct client
{
    ENetAddress address;
    ENetSocket socket;
    char input[INPUT_LIMIT];
    messagebuf *message;
    std::string output;
    int inputpos, outputpos;
    enet_uint32 connecttime, lastinput;
    int servport;
    enet_uint32 lastauth;
    bool shouldpurge;
    bool registeredserver;

    client() : message(nullptr), inputpos(0), outputpos(0), servport(-1), lastauth(0), shouldpurge(false), registeredserver(false) {}
};
std::vector<client *> clients;

ENetSocket serversocket = ENET_SOCKET_NULL;

enet_uint32 servtime = 0;

void fatal(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vfprintf(logfile, fmt, args);
    fputc('\n', logfile);
    va_end(args);
    exit(EXIT_FAILURE);
}

void conoutfv(const char *fmt, va_list args)
{
    vfprintf(logfile, fmt, args);
    fputc('\n', logfile);
}

void conoutf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    conoutfv(fmt, args);
    va_end(args);
}

void purgeclient(int n)
{
    client &c = *clients.at(n);
    if(c.message)
    {
        c.message->purge();
    }
    enet_socket_destroy(c.socket);
    delete clients.at(n);
    clients.erase(clients.begin() + n);
}

void output(client &c, const std::string &msg)
{
    c.output.append(msg);
}

void outputf(client &c, const char *fmt, ...)
{
    DEFV_FORMAT_STRING(msg, fmt, fmt);
    std::string out(msg);
    output(c, out);
}

ENetSocket pingsocket = ENET_SOCKET_NULL;

bool setuppingsocket(ENetAddress *address)
{
    if(pingsocket != ENET_SOCKET_NULL)
    {
        return true;
    }
    pingsocket = enet_socket_create(ENET_SOCKET_TYPE_DATAGRAM);
    if(pingsocket == ENET_SOCKET_NULL)
    {
        return false;
    }
    if(address && enet_socket_bind(pingsocket, address) < 0)
    {
        return false;
    }
    enet_socket_set_option(pingsocket, ENET_SOCKOPT_NONBLOCK, 1);
    return true;
}

void setupserver(int port, const char *ip = nullptr)
{
    time_t starttime;
    ENetAddress address;
    address.host = ENET_HOST_ANY;
    address.port = port;

    if(ip)
    {
        if(enet_address_set_host(&address, ip)<0)
        {
            fatal("failed to resolve server address: %s", ip);
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
    starttime = time(nullptr);
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
    while(gameserverlists.size() && gameserverlists.back()->refs<=0)
    {
        delete gameserverlists.back();
        gameserverlists.pop_back();
    }
    messagebuf *l = new messagebuf(gameserverlists);
    for(uint i = 0; i < gameservers.size(); i++)
    {
        gameserver &s = *gameservers.at(i);
        if(!s.lastpong)
        {
            continue;
        }
        DEF_FORMAT_STRING(cmd, "addserver %s %d\n", s.ip, s.port);
        l->buf.insert(l->buf.end(), std::begin(cmd), std::end(cmd));
    }
    l->buf.push_back('\0');
    gameserverlists.push_back(l);
    updateserverlist = false;
}

void gengbanlist()
{
    messagebuf *l = new messagebuf(gbanlists);
    std::string header = "cleargbans\n";
    l->buf.insert(l->buf.begin(), header.begin(), header.end());
    std::string cmd = "addgban ";
    for(uint i = 0; i < gbans.size(); i++)
    {
        ipmask &b = gbans.at(i);
        char banstr[260];
        b.print(banstr);
        std::string newban = std::string(cmd).append(banstr);
        l->buf.insert(l->buf.end(), newban.begin(), newban.end() );
        l->buf.push_back('\n');
    }
    if(gbanlists.size() && gbanlists.back()->equals(*l))
    {
        delete l;
        return;
    }
    while(gbanlists.size() && gbanlists.back()->refs<=0)
    {
        delete gbanlists.back();
        gbanlists.pop_back();
    }
    for(uint i = 0; i < gbanlists.size(); i++)
    {
        messagebuf *m = gbanlists[i];
        if(m->refs > 0 && !m->endswith(*l))
        {
            m->concat(*l);
        }
    }
    gbanlists.push_back(l);
    for(uint i = 0; i < clients.size(); i++)
    {
        client &c = *clients.at(i);
        if(c.servport >= 0 && !c.message)
        {
            c.message = l;
            c.message->refs++;
        }
    }
}

void addgameserver(client &c)
{
    if(gameservers.size() >= SERVER_LIMIT)
    {
        return;
    }
    int dups = 0;
    for(uint i = 0; i < gameservers.size(); i++)
    {
        gameserver &s = *gameservers.at(i);
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
    gameservers.push_back(new gameserver);
    gameserver &s = *gameservers.back();
    s.address.host = c.address.host;
    s.address.port = c.servport;
    copystring(s.ip, hostname);
    s.port = c.servport;
    s.numpings = 0;
    s.lastping = s.lastpong = 0;
}

client *findclient(gameserver &s)
{
    for(uint i = 0; i < clients.size(); i++)
    {
        client &c = *clients.at(i);
        if(s.address.host == c.address.host && s.port == c.servport)
        {
            return &c;
        }
    }
    return nullptr;
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
        for(uint i = 0; i < gameservers.size(); i++)
        {
            gameserver &s = *gameservers.at(i);
            if(s.address.host == addr.host && s.address.port == addr.port)
            {
                if(s.lastping && (!s.lastpong || ENET_TIME_GREATER(s.lastping, s.lastpong)))
                {
                    client *c = findclient(s);
                    if(c)
                    {
                        c->registeredserver = true;
                        outputf(*c, "succreg\n");
                        if(!c->message && gbanlists.size())
                        {
                            c->message = gbanlists.back();
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
    for(int i = gameservers.size(); --i >=0;) //note reverse iteration
    {
        if(checkban(servbans, gameservers.at(i)->address.host))
        {
            delete gameservers.at(i);
            gameservers.erase(gameservers.begin() + i);
            updateserverlist = true;
        }
    }
}

void checkgameservers()
{
    ENetBuffer buf;
    for(uint i = 0; i < gameservers.size(); i++)
    {
        gameserver &s = *gameservers.at(i);
        if(s.lastping && s.lastpong && ENET_TIME_LESS_EQUAL(s.lastping, s.lastpong))
        {
            if(ENET_TIME_DIFFERENCE(servtime, s.lastpong) > KEEPALIVE_TIME)
            {
                delete gameservers.at(i);
                gameservers.erase(gameservers.begin() + i);
                i--;
                updateserverlist = true;
            }
        }
        else if(!s.lastping || ENET_TIME_DIFFERENCE(servtime, s.lastping) > PING_TIME)
        {
            if(s.numpings >= PING_RETRY)
            {
                servermessage(s, "failreg failed pinging server\n");
                delete gameservers.at(i);
                gameservers.erase(gameservers.begin() + i);
                i--;
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

void messagebuf::purge()
{
    refs = std::max(refs - 1, 0);
    if(refs<=0 && owner.back()!=this)
    {
        owner.erase(std::find(owner.begin(), owner.end(), this));
        delete this;
    }
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
        if(!strncmp(c.input, "list", 4) && (!c.input[4] || c.input[4] == '\n' || c.input[4] == '\r'))
        {
            genserverlist();
            if(gameserverlists.empty() || c.message)
            {
                return false;
            }
            c.message = gameserverlists.back();
            c.message->refs++;
            c.output.clear();
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
        c.inputpos = &c.input[c.inputpos] - end;
        memmove(c.input, end, c.inputpos);

        end = (char *)memchr(c.input, '\n', c.inputpos);
    }
    return c.inputpos < static_cast<int>(sizeof(c.input));
}

void checkclients()
{
    ENetSocketSet readset, writeset;
    ENetSocket maxsock = std::max(serversocket, pingsocket);
    ENET_SOCKETSET_EMPTY(readset);
    ENET_SOCKETSET_EMPTY(writeset);
    ENET_SOCKETSET_ADD(readset, serversocket);
    ENET_SOCKETSET_ADD(readset, pingsocket);
    for(uint i = 0; i < clients.size(); i++)
    {
        client &c = *clients.at(i);
        if(c.message || c.output.size())
        {
            ENET_SOCKETSET_ADD(writeset, c.socket);
        }
        else
        {
            ENET_SOCKETSET_ADD(readset, c.socket);
        }
        maxsock = std::max(maxsock, c.socket);
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
        if(clients.size()>=CLIENT_LIMIT || checkban(bans, address.host))
        {
            enet_socket_destroy(clientsocket);
        }
        else if(clientsocket!=ENET_SOCKET_NULL)
        {
            int dups = 0,
                oldest = -1;
            for(uint i = 0; i < clients.size(); i++)
            {
                if(clients.at(i)->address.host == address.host)
                {
                    dups++;
                    if(oldest<0 || clients.at(i)->connecttime < clients.at(oldest)->connecttime)
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
            clients.push_back(c);
        }
    }

    for(uint i = 0; i < clients.size(); i++)
    {
        client &c = *clients.at(i);
        if((c.message || c.output.size()) && ENET_SOCKETSET_CHECK(writeset, c.socket))
        {
            const char *data = c.output.size() ? c.output.data() : c.message->getbuf();
            int len = c.output.size() ? c.output.size() : c.message->length();
            ENetBuffer buf;
            buf.data = (void *)&data[c.outputpos];
            buf.dataLength = len-c.outputpos;
            int res = enet_socket_send(c.socket, nullptr, &buf, 1);
            if(res>=0)
            {
                c.outputpos += res;
                if(c.outputpos>=len)
                {
                    if(c.output.size())
                    {
                        c.output.clear();
                    }
                    else
                    {
                        c.message->purge();
                        c.message = nullptr;
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
            int res = enet_socket_receive(c.socket, nullptr, &buf, 1);
            if(res>0)
            {
                c.inputpos += res;
                c.input[std::min(c.inputpos, static_cast<int>(sizeof(c.input)-1))] = '\0';
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
        if(c.output.size() > OUTPUT_LIMIT)
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
    for(int i = clients.size(); --i >=0;) //note reverse iteration
    {
        if(checkban(bans, clients.at(i)->address.host))
        {
            purgeclient(i);
        }
    }
}

int main(int argc, char **argv)
{
    int reloadcfg = 1;
    if(enet_initialize()<0)
    {
        fatal("Unable to initialise network module");
    }
    atexit(enet_deinitialize);
    const char *dir = "", *ip = nullptr;
    int port = 42068;
    if(argc>=2)
    {
        dir = argv[1];
    }
    if(argc>=3)
    {
        port = atoi(argv[2]);
    }
    if(argc>=4)
    {
        ip = argv[3];
    }
    DEF_FORMAT_STRING(logname, "%smaster.log", dir);
    DEF_FORMAT_STRING(cfgname, "%smaster.cfg", dir);
    path(logname);
    path(cfgname);
    logfile = fopen(logname, "a");
    if(!logfile)
    {
        logfile = stdout;
    }
    setvbuf(logfile, nullptr, _IOLBF, BUFSIZ);
    setupserver(port, ip);
    for(;;)
    {
        if(reloadcfg)
        {
            conoutf("reloading %s", cfgname);
            //execfile(cfgname);
            bangameservers();
            banclients();
            gengbanlist();
            reloadcfg = 0;
        }
        servtime = enet_time_get();
        checkclients();
        checkgameservers();
    }

    return EXIT_SUCCESS;
}

