#if defined(__GP2X__)
 #include <stdio.h>
 #define lprintf printf
#elif defined(PSP)
 #if 0
  #include <stdio.h>
  #define lprintf printf
 #else
  extern void lprintf_f(const char *fmt, ...);
  #define lprintf lprintf_f
 #endif
#else
 #include "giz.h"
#endif

