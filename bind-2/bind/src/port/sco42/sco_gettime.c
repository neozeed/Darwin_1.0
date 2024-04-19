#include <sys/types.h>
#include <sys/timeb.h>
#include <sys/time.h>

#undef gettimeofday

int
__sco_gettimeofday(tdp, tzp)
struct timeval *tdp;
struct timezone *tzp;
{
	struct timeval td;
	struct timezone tz;

	if (gettimeofday(&td, &tz) != 0)
		return -1;
	if (tdp) {
		/* Begin E.V. hack for SCO Unix */
		/* *tdp = td; /**/
		tdp->tv_sec  = td.tv_sec + (td.tv_usec / 1000000);
		tdp->tv_usec = td.tv_usec % 1000000;
		/* End E.V. hack */
	}
	if (tzp) {
		*tzp = tz;
	}
	return 0;
}
