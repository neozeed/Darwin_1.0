#define WANT_IRS_NIS
#undef WANT_IRS_PW
#undef WANT_IRS_GR
#define SIG_FN void

#if defined(HAS_PTHREADS) && defined(_REENTRANT)
#define DO_PTHREADS
#endif

#include <limits.h>	/* _POSIX_PATH_MAX */
#include <sys/select.h>	/* fdset */
#include <sys/timers.h>	/* struct timespec */
#include <sys/machine.h>/* endianness */
