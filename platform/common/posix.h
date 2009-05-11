/* define POSIX stuff: dirent, scandir, getcwd, mkdir */
#if defined(__linux__)

#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#else

#error "must provide posix"

#endif


