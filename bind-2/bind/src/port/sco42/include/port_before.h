#if !defined(__GNUC__)		
# define const
#endif

#define SIOCGIFCONF_ADDR

#define SIG_FN void

/* #define EVENTLIB_DEBUG 1 /**/

#define USE_POSIX
#define POSIX_SIGNALS

#include <sys/types.h>
#include <limits.h>	/* _POSIX_PATH_MAX */

#define PATH_MAX _POSIX_PATH_MAX
#define MAXPATHLEN _POSIX_PATH_MAX

#include <stdio.h>	/* To avoid ptr/_ptr/__ptr name clash. */

#include <stdarg.h>

#define NEED_PSELECT
struct timespec {
        time_t  tv_sec;         /* seconds */
        long    tv_nsec;        /* nanoseconds */
};
 

#if defined(HAS_PTHREADS) && defined(_REENTRANT)
#define DO_PTHREADS
#endif

