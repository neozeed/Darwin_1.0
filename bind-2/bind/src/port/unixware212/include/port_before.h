#define __EXTENSIONS__
#define SVR4
#define SIG_FN void
#define _XOPEN_SOURCE
#define _XOPEN_SOURCE_EXTENDED 1
#define _POSIX_TIMERS

#include <limits.h>	/* _POSIX_PATH_MAX */
        typedef unsigned char u_char;
        typedef unsigned short u_short;
        typedef unsigned int u_int;
        typedef unsigned long u_long;



#if defined(HAS_PTHREADS) && defined(_REENTRANT)
#define DO_PTHREADS
#endif

