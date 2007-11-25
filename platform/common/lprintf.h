#if defined(__GP2X__)
 #include <stdio.h>
 #define lprintf printf
#elif defined(PSP)
 extern void lprintf(const char *fmt, ...);
#else
 #include "giz.h"
#endif

