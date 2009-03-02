/* define POSIX stuff: dirent, scandir, getcwd */
#if defined(__linux__)

#include <dirent.h>
#include <unistd.h>

#else

#error "must define dirent"

#endif


