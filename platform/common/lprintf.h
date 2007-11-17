#if defined(__GP2X__)
 #include <stdio.h>
 #define lprintf printf
#elif defined(PSP)
 extern void lprintf_f(const char *fmt, ...);
 #define lprintf lprintf_f
#else
 #include "giz.h"
#endif

