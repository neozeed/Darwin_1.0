/*
   Partial emulation of getrusage() (elapsed times only)
   for BIND 8.1 on SCO OSE5.       01-Apr-97 by E.V. :-)
*/

#include <port_before.h>

#include <sys/types.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/resource.h>
#include <sys/select.h>		/* struct timeval */
#include <unistd.h>

#include <port_after.h>

#if 0
main() {
	struct rusage rus;
	unsigned long i;
	for (i=0; i<150000;  printf("%d ", i), i++);
	getrusage(RUSAGE_SELF,&rus);
	printf("\n\n%d.%d, %d.%d\n",
	    rus.ru_utime.tv_sec, rus.ru_utime.tv_usec,
	    rus.ru_stime.tv_sec, rus.ru_stime.tv_usec);
}
#endif

int
getrusage(int who, struct rusage *usage)
{
	static long tps = -1, scale;
	clock_t elapsed;
	struct tms tmsbuf;
	if ( tps == -1 )	/* not initialized */
		if ((tps = sysconf(_SC_CLK_TCK)) == -1)	/* error */
			return(-1);
		else	/* got ticks per second */
			scale = 1000000 / tps;
	if ( (elapsed = times(&tmsbuf)) == -1 )
		return(-1);
	if ( who == RUSAGE_SELF ) {
		usage->ru_utime.tv_sec  = tmsbuf.tms_utime  / tps;
		usage->ru_utime.tv_usec = tmsbuf.tms_utime  % tps * scale;
		usage->ru_stime.tv_sec  = tmsbuf.tms_stime  / tps;
		usage->ru_stime.tv_usec = tmsbuf.tms_stime  % tps * scale;
	} else {	/* Hope it is RUSAGE_CHILDREN. It should be... */
		usage->ru_utime.tv_sec  = tmsbuf.tms_cutime / tps;
		usage->ru_utime.tv_usec = tmsbuf.tms_cutime % tps * scale;
		usage->ru_stime.tv_sec  = tmsbuf.tms_cstime / tps;
		usage->ru_stime.tv_usec = tmsbuf.tms_cstime % tps * scale;
	}
	/* These are hopefully not needed in BIND. */
	usage->ru_ixrss       = usage->ru_idrss    = usage->ru_isrss  =
	    usage->ru_minflt  = usage->ru_majflt   = usage->ru_nswap  =
	    usage->ru_inblock = usage->ru_oublock  = usage->ru_msgsnd =
	    usage->ru_msgrcv  = usage->ru_nsignals = usage->ru_nvcsw  =
	    usage->ru_nivcsw  = -1;
	return(0);
}
