// generic useful stuff for any C++ program

#ifndef _TOOLS_H
#define _TOOLS_H

typedef unsigned char uchar;
typedef unsigned short ushort;
typedef unsigned int uint;
typedef unsigned long ulong;
typedef signed long long int llong;
typedef unsigned long long int ullong;

#define _vsnprintf vsnprintf
#define PATHDIV '/'

#define PRINTFARGS(fmt, args) __attribute__((format(printf, fmt, args)))

// easy safe strings

#define MAXSTRLEN 260
typedef char string[MAXSTRLEN];

inline void vformatstring(char *d, const char *fmt, va_list v, int len) { _vsnprintf(d, len, fmt, v); d[len-1] = 0; }
template<size_t N> inline void vformatstring(char (&d)[N], const char *fmt, va_list v) { vformatstring(d, fmt, v, N); }

inline char *copystring(char *d, const char *s, size_t len)
{
    size_t slen = std::min(strlen(s), len-1);
    memcpy(d, s, slen);
    d[slen] = 0;
    return d;
}
template<size_t N> inline char *copystring(char (&d)[N], const char *s) { return copystring(d, s, N); }

template<size_t N> inline void formatstring(char (&d)[N], const char *fmt, ...) PRINTFARGS(2, 3);
template<size_t N> inline void formatstring(char (&d)[N], const char *fmt, ...)
{
    va_list v;
    va_start(v, fmt);
    vformatstring(d, fmt, v, int(N));
    va_end(v);
}

#define DEF_FORMAT_STRING(d,...) string d; formatstring(d, __VA_ARGS__)
#define DEFV_FORMAT_STRING(d,last,fmt) string d; { va_list ap; va_start(ap, last); vformatstring(d, fmt, ap); va_end(ap); }

inline char *newstring(size_t l)      { return new char[l+1]; }
inline char *newstring(const char *s) { size_t l = strlen(s); char *d = newstring(l); memcpy(d, s, l+1); return d; }

struct stream
{
    typedef off_t offset;

    virtual ~stream() {}
    virtual void close() = 0;
    virtual bool end() = 0;
    virtual offset tell() { return -1; }
    virtual offset rawtell() { return tell(); }
    virtual bool seek(offset pos, int whence = SEEK_SET) { return false; }
    virtual offset size();
    virtual offset rawsize() { return size(); }
    virtual size_t read(void *buf, size_t len) { return 0; }
    virtual size_t write(const void *buf, size_t len) { return 0; }
    virtual bool flush() { return true; }
    virtual bool getline(char *str, size_t len);
    virtual bool putstring(const char *str) { size_t len = strlen(str); return write(str, len) == len; }
    virtual bool putline(const char *str) { return putstring(str) && putchar('\n'); }
    virtual size_t printf(const char *fmt, ...) PRINTFARGS(2, 3);
    virtual uint getcrc() { return 0; }

    template<class T> size_t put(const T *v, size_t n) { return write(v, n*sizeof(T))/sizeof(T); }
    template<class T> bool put(T n) { return write(&n, sizeof(n)) == sizeof(n); }

    template<class T> size_t get(T *v, size_t n) { return read(v, n*sizeof(T))/sizeof(T); }
    template<class T> T get() { T n; return read(&n, sizeof(n)) == sizeof(n) ? n : 0; }
};

template<class T>
struct streambuf
{
    stream *s;

    streambuf(stream *s) : s(s) {}

    T get()
    {
        return s->get<T>();
    }

    size_t get(T *vals, size_t numvals)
    {
        return s->get(vals, numvals);
    }

    void put(const T &val)
    {
        s->put(&val, 1);
    }

    void put(const T *vals, size_t numvals)
    {
        s->put(vals, numvals);
    }

    size_t length()
    {
        return s->size();
    }
};

extern string homedir;

static inline char *path(char *s)
{
    for(char *curpart = s;;)
    {
        char *endpart = strchr(curpart, '&');
        if(endpart)
        {
            *endpart = '\0';
        }
        if(curpart[0]=='<')
        {
            char *file = strrchr(curpart, '>');
            if(!file)
            {
                return s;
            }
            curpart = file+1;
        }
        for(char *t = curpart; (t = strpbrk(t, "/\\")); *t++ = PATHDIV) //note pointer walk
        {
            //(empty body)
        }
        for(char *prevdir = nullptr, *curdir = curpart;;) //note pointer walk
        {
            prevdir = curdir[0]==PATHDIV ? curdir+1 : curdir;
            curdir = strchr(prevdir, PATHDIV);
            if(!curdir)
            {
                break;
            }
            if(prevdir+1==curdir && prevdir[0]=='.')
            {
                memmove(prevdir, curdir+1, strlen(curdir+1)+1);
                curdir = prevdir;
            }
            else if(curdir[1]=='.' && curdir[2]=='.' && curdir[3]==PATHDIV)
            {
                if(prevdir+2==curdir && prevdir[0]=='.' && prevdir[1]=='.')
                {
                    continue;
                }
                memmove(prevdir, curdir+4, strlen(curdir+4)+1);
                if(prevdir-2 >= curpart && prevdir[-1]==PATHDIV)
                {
                    prevdir -= 2;
                    while(prevdir-1 >= curpart && prevdir[-1] != PATHDIV)
                    {
                        --prevdir;
                    }
                }
                curdir = prevdir;
            }
        }
        if(endpart)
        {
            *endpart = '&';
            curpart = endpart+1;
        }
        else
        {
            break;
        }
    }
    return s;
}

struct ipmask
{
    enet_uint32 ip, mask;

    void parse(const char *name);
    int print(char *buf) const;
    bool check(enet_uint32 host) const { return (host & mask) == ip; }
};

#endif
