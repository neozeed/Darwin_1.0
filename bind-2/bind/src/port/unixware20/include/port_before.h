#define __EXTENSIONS__
#define SVR4
#define SIG_FN void
#define _POSIX_TIMERS

#define NEED_DAEMON
int daemon(int nochdir, int noclose);

#include <limits.h>	/* _POSIX_PATH_MAX */

#define IFNAMSIZ 16	/* used in <net/route.h> */


#if defined(HAS_PTHREADS) && defined(_REENTRANT)
#define DO_PTHREADS
#endif

