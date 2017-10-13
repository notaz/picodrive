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
#else
#define NOINLINE
#define ALIGNED(n)
#endif

#endif // PICO_PORT_INCLUDED
