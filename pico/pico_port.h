#ifndef PICO_PORT_INCLUDED
#define PICO_PORT_INCLUDED

#if defined(__GNUC__) && defined(__i386__)
#define REGPARM(x) __attribute__((regparm(x)))
#else
#define REGPARM(x)
#endif

#ifdef __GNUC__
#define NOINLINE    __attribute__((noinline))
#define ALIGNED(n)  __attribute__((aligned(n)))
#define unlikely(x) __builtin_expect((x), 0)
#else
#define NOINLINE
#define ALIGNED(n)
#define unlikely(x) (x)
#endif

#ifdef _MSC_VER
#define snprintf _snprintf
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#define strdup _strdup
#endif

#endif // PICO_PORT_INCLUDED
