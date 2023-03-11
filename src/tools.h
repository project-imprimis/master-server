
#ifndef _TOOLS_H
#define _TOOLS_H

typedef unsigned char uchar;
typedef unsigned int uint;

#define PATHDIV '/'

#define PRINTFARGS(fmt, args) __attribute__((format(printf, fmt, args)))

// easy safe strings

#define MAXSTRLEN 260
typedef char string[MAXSTRLEN];

inline void vformatstring(char *d, const char *fmt, va_list v, int len) { vsnprintf(d, len, fmt, v); d[len-1] = 0; }
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
