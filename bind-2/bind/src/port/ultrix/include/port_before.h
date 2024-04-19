#define WANT_IRS_NIS
#undef WANT_IRS_PW
#undef WANT_IRS_GR

#include <time.h>
struct timespec {
	time_t	tv_sec;		/* seconds */
	long	tv_nsec;	/* nanoseconds */
};

#if defined(HAS_PTHREADS) && defined(_REENTRANT)
#define DO_PTHREADS
#endif

