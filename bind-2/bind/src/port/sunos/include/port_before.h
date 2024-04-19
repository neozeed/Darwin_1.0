#ifndef PORT_BEFORE_H
#define PORT_BEFORE_H

#define WANT_IRS_NIS
#undef WANT_IRS_PW
#undef WANT_IRS_GR
#define SIG_FN void

#include <time.h>
struct timespec {
	time_t	tv_sec;		/* seconds */
	long	tv_nsec;	/* nanoseconds */
};

#include <limits.h>

#if defined(HAS_PTHREADS) && defined(_REENTRANT)
#define DO_PTHREADS
#endif

#define _BIG_ENDIAN

#endif /*PORT_BEFORE_H*/
