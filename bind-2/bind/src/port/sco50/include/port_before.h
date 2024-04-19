#include <sched.h>	/* struct timespec */
#include <limits.h>	/* _POSIX_PATH_MAX */
#include <sys/select.h>

#define PATH_MAX _POSIX_PATH_MAX
#define MAXPATHLEN _POSIX_PATH_MAX

#if defined(HAS_PTHREADS) && defined(_REENTRANT)
#define DO_PTHREADS
#endif

#define LITTLE_ENDIAN  1234
#define BYTE_ORDER LITTLE_ENDIAN
