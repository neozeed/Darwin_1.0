#define WANT_IRS_NIS
#undef WANT_IRS_PW
#undef WANT_IRS_GR
#define SIG_FN void

#include <limits.h>	/* _POSIX_PATH_MAX */
#include <sys/select.h>	/* fdset */
#include <sys/timers.h>	/* struct timespec */
