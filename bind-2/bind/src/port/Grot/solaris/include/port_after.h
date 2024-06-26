/* (ISC = INTERACTIVE Systems Corporation in the next #ifdef, btw.) */
#ifdef ISC
# ifndef _POSIX_SOURCE
#  define _POSIX_SOURCE
# endif
# define SYSV
# define SVR3
# define _SYSV3
# define NEED_STRTOUL
# define NEED_FTRUNCATE
# define USE_POSIX
# include <sys/bsdtypes.h>
# include <sys/sioctl.h>
# include <sys/stream.h>
# include <net/errno.h>
#endif

#if defined(__convex__)
# if !defined(_POSIX_SOURCE)
#  define _POSIX_SOURCE
# endif
# define USE_UTIME
# define NEED_PUTENV
#endif

#if defined(_CRAY)
# if !defined(_POSIX_SOURCE)
#  define _POSIX_SOURCE
# endif
# define writev(a,b,c) __writev(a,b,c)
# define setitimer(a,b,c) __setitimer(a,b,c)
#endif

/* This is for AIX 4.1.x */
#ifdef _AIX41
# include <sys/select.h>
# include <sys/time.h>
# include <time.h>
# define vfork fork
#endif

/* This is defined in the Makefile for INTERACTIVE compiles. */
#if defined(ISC)
# define ftruncate(a,b) __ftruncate(a,b)
# define USE_MEMCPY
# define USE_UTIME
# define HAVE_FCHMOD
#endif

/* SCO UNIX defines only this unique symbol, apparently. */
#if defined(M_UNIX)
# define POSIX_SIGNALS
# if !defined(_SCO_DS)
/* This section is for 3.2v4.2/ODT3.0 and maybe also for 3.2v4.1/3.2v4.0 */
/* XXX - why is this POSIX_SOURCE instead of _POSIX_SOURCE? */
#  undef POSIX_SOURCE
#  define HAVE_FCHMOD
#  define NEED_WRITEV
#  define writev(a,b,c) __writev(a,b,c)
#  define ftruncate(a,b) __ftruncate(a,b)
# endif
#endif

#ifdef NeXT
# define NEED_PUTENV
# define NEED_SETENV
# define inet_addr(a) __inet_addr(a)
#endif

#if defined(__sgi)
# define BSD 43
# define vfork fork
#endif

#if defined(SUNOS4)
# define BSD 43
# define NEED_STRTOUL
#endif

#if defined(__osf__) && defined(__alpha) && defined(BSD) && (BSD < 199103)
# undef BSD
# define BSD 199103
#endif

#if defined(_AUX_SOURCE)
# define vfork fork
# define NEED_STRERROR
# define NEED_STRTOUL
# define SIG_FN void
# define USE_MEMCPY
#endif

#if defined(apollo)
# define HAVE_STDLIB_H
#endif

#if defined(SVR4) && !defined(SYSV)
# define SYSV
#endif

#if defined(_POSIX_SOURCE) || defined(__sgi) || defined(__ultrix) || \
	defined(__hpux) || (defined(BSD) && (BSD >= 199103)) || defined(sun)
# define USE_POSIX
#endif

#if defined(__ultrix) && !defined(BSD)
# define BSD 42
#endif

#if defined(host_mips) && defined(SYSTYPE_BSD43)
# define RISCOS_BSD
#endif

#if defined(SYSV) || defined(__ultrix) || defined(__osf__) \
	|| (defined(BSD) && BSD >= 199306) || defined(linux)
# define USE_UTIME
# define HAVE_SETVBUF
#endif

#if defined(SYSV) && !defined(SVR4)
# define vfork fork
#endif

#if defined(sun) || defined(SVR4)
# define NETREAD_BROKEN
#endif

#if defined(sun) && !defined(SVR4)
# define NEED_TIMESPEC
#endif

#if !defined(BSD) || (BSD <= 199006)
# if !defined(NeXT)
#  define NEED_INETADDR
# endif
# define NEED_INETATON
#endif

#if defined(__hpux)
# if defined(__STDC__)
#  define select(a,b,c,d,e) select(a, (int *)b, (int *)c, (int *)d, e)
#  define ctime(x) ctime((const time_t *)x)
# endif /*__STDC__*/
# if !defined(SYSV)
#  define USE_UTIME
#  define setlinebuf(x) setvbuf(x, NULL, _IOLBF, BUFSIZ)
#  if !defined(SIGWINCH)  /*pre 9.0*/
#   define SIGWINCH SIGWINDOW
#  endif
# endif /*SYSV*/
/* XXX: better autodetection of the need for "struct linger" would be nice */
# if 0
struct	linger {
	int	l_onoff;		/* option on/off */
	int	l_linger;		/* linger time */
};
# endif
#endif /*__hpux*/

#if defined(_SEQUENT_)
# include <netinet/in_systm.h>
# define USE_UTIME
# define USE_POSIX
# define NEED_GETTIMEOFDAY
# define _TIMEZONE timezoneBSD
struct timezoneBSD {
	int tz_minuteswest;
	int tz_dsttime;
};
#endif

#ifndef __P
# if defined(__STDC__) || defined(__GNUC__)
#  define __P(x) x
# else
#  define __P(x) ()
# endif
#endif

#ifndef _TIMEZONE
# define _TIMEZONE timezone
#endif

#if defined(USE_POSIX) || defined(HAVE_STDLIB_H)
# include <stdlib.h>
# if defined(__ultrix)
#  define NEED_STRDUP
# endif

#else

# if !defined(_SCO_DS)
#  define NEED_STRDUP
#  define NEED_STRTOUL
# endif

# ifndef NeXT
extern char *getenv __P((char *));
# else
extern char *getenv __P((const char *));
# endif

# if !defined(DMALLOC) && !defined(NeXT)
extern char *malloc(), *realloc(), *calloc();
extern void free();
# endif

#endif /*HAVE_STDLIB_H*/

#if defined(USE_POSIX)
# include <unistd.h>
# include <limits.h>

#else

# define STDIN_FILENO	0
# define STDOUT_FILENO	1
# define STDERR_FILENO	2
extern int errno;

extern int getdtablesize __P((void));
# ifdef SHORT_FNAMES
extern long pathconf __P((const char *path, int name));
# endif

extern char *optarg;
extern int optind;
int getopt(int argc, char * const *argv, const char *optstring);

#endif /*USE_POSIX*/

#ifndef UINT_MAX
# ifdef __STDC__
#  define UINT_MAX	4294967295u	/* max value of an "u_int" */
# else
#  define UINT_MAX	((unsigned)4294967295)  /* max value of an "u_int" */
# endif
#  define ULONG_MAX	UINT_MAX	/* max decimal value of a "u_long" */
#endif

#ifndef INT_MAX
# define INT_MAX	2147483647	/* max decimal value of an "int" */
#endif

#ifndef RAND_MAX
# define RAND_MAX	0x7fffffff
#endif

#ifndef	IN_LOOPBACKNET
# define IN_LOOPBACKNET	127
#endif

#ifndef	INADDR_NONE
# define INADDR_NONE	0xffffffff
#endif

#if defined(apollo)
		/* Defined in /usr/include/netinet/in.h but doesn't work */
#undef IP_OPTIONS
#endif

#if !defined(__STDC__) && !defined(const)
# define const /*constant*/
#endif

#if !defined(__convex__) && (!defined(BSD) || (BSD < 199103))
int	strcasecmp __P((const char *, const char *));
#endif

/* is USE_POSIX the right thing to use here? */
#if (!defined(BSD) || (BSD <= 43)) && \
	!defined(NeXT) && \
	!defined(__convex__) && \
	!defined(USE_POSIX)
# if !defined(NCR)
void	syslog();
# endif
char	*ctime __P((const time_t *clock));
# if !defined(M_UNIX)
int	close(), setitimer(), recv(), sendto(), sigsetmask(),
	atoi(), getpid(), fork(), read(), ioctl(),
	setsockopt(), socket(), bind();
# endif
#endif

#if !defined(bcopy)	/* some machines have their own macros for this */
# if (defined(USE_POSIX) && !defined(SUNOS4)) || \
	 (defined(__STDC__) && !defined(sun) && !defined(sequent) \
	  && !defined(M_UNIX))
/* use ANSI C3.159-1989 (``ANSI C'') functions if possible;
 * ideally we would change the code to use them and then
 * define them in terms of bcopy et al if !defined(__STDC__)
 * but that's more work.
 */
#if defined(USE_MEMCPY)
#  define bcopy(a,b,c) memcpy(b,a,c)
#else
#  define bcopy(a,b,c) memmove(b,a,c)
#endif
#  define bzero(a,b) memset(a,0,b)
#  define bcmp(a,b,c) memcmp(a,b,c)
# else
extern void bcopy();
extern void bzero();
extern int bcmp();
# endif /* BSD */
#endif /* bcopy */

#if (!defined(BSD) || (BSD < 43) || defined(RISCOS_BSD)) \
	&& !defined(USE_POSIX) && !defined(apollo) && !defined(sequent) \
	&& !defined(M_UNIX)
# define NEED_STRERROR
#if !defined(ultrix) && !defined(NCR)
# define NEED_PUTENV
#endif
#endif

#if defined(SUNOS4)
# define NEED_STRERROR
# if defined(sun386)
#  define pid_t int
#  define NEED_STRCASECMP
# endif
#endif

#if (!defined(BSD) || (BSD < 43)) && !defined(__hpux)
# define NEED_MKSTEMP
# define NEED_STRSEP
# if !defined(__ultrix) && !defined(apollo)
#  if !defined(_SCO_DS)
#   define NEED_STRCASECMP
#   define NEED_MKTEMP
#   if !defined(SVR4)
#    define NEED_STRPBRK
#   endif
#  endif
# endif
#endif

#if defined(USE_POSIX)
# define POSIX_SIGNALS
#endif

/*
 * Attempt to configure for type of function returned by signal-catching
 * functions (which signal and sigvec.sv_handler take a pointer to).
 * This can guess for BSD; otherwise, define SIG_FN externally.
 */
#ifndef	SIG_FN
# ifdef BSD
#  if (BSD >= 199006) || defined(NeXT) || defined(__osf__) || defined(sun) \
	|| defined(__ultrix) || defined(apollo) || defined(POSIX_SIGNALS)
#   define SIG_FN void		/* signal-catching functions return void */
#  else
#   define SIG_FN int		/* signal-catching functions return int */
#  endif
# else /*BSD*/
#  define SIG_FN void		/* signal-catching functions return void */
# endif /*BSD*/
#endif

#if !defined(SIGUSR1) && !defined(SIGUSR2)
# define SIGUSR1 SIGEMT
# define SIGUSR2 SIGFPE
#endif
#if !defined(SIGCHLD)
# define SIGCHLD SIGCLD
#endif

#if !defined(ntohl) && !defined(htonl) && defined(BSD) && (BSD <= 43)
/* if these aren't null macros in netinet/in.h, extern them here. */
extern u_short htons __P((u_short)), ntohs __P((u_short));
extern u_long htonl __P((u_long)), ntohl __P((u_long));
#endif

#if defined(USE_POSIX) && !defined(sun) && !defined(__sgi) \
	&& !defined(__convex__) && !defined(__ultrix) && !defined(_AUX_SOURCE)
# define PORT_NONBLOCK	O_NONBLOCK
# define PORT_WOULDBLK	EAGAIN
#else
# define PORT_NONBLOCK	O_NDELAY
# define PORT_WOULDBLK	EWOULDBLOCK
#endif

#if defined(USE_POSIX) || !defined(SYSV)
#define USE_WAITPID
#endif

#if !defined(USE_POSIX)
#define waitpid(x,y,z) (wait3(y,z,(struct rusage *)NULL))
#endif

#if defined(NeXT) || defined(_AIX) || defined(sun386)
# undef WIFEXITED
# undef WEXITSTATUS
# undef WIFSIGNALED
# undef WTERMSIG
#endif /* NeXT */

#if defined(sequent)
#define WEXITSTATUS(x)	((x).w_retcode)
#define WTERMSIG(x)	((x).w_termsig)
#define WAIT_T		union wait
#endif /* sequent */

#if !defined(WIFEXITED)
# define WIFEXITED(x) (!(x & 0177))
#endif
#if !defined(WEXITSTATUS)
# define WEXITSTATUS(x) (x >> 8)
#endif
#if !defined(WIFSIGNALED)
# define WIFSIGNALED(x) ((x & 0177) && ((x & 0377) != 0177))
#endif
#if !defined(WTERMSIG)
# define WTERMSIG(x) (x & 0177)
#endif
#if !defined(WAIT_T)
# define WAIT_T int
#endif

#ifndef S_ISDIR
# ifndef S_IFMT
#  define S_IFMT 0170000
# endif
# ifndef S_IFDIR
#  define S_IFDIR 0040000
# endif
# define S_ISDIR(m)	((m & S_IFMT) == S_IFDIR)
#endif

#ifndef S_ISREG
# ifndef S_IFMT
#  define S_IFMT 0170000
# endif
# ifndef S_IFREG
#  define S_IFREG 0100000
# endif
# define S_ISREG(m)	((m & S_IFMT) == S_IFREG)
#endif

#ifndef S_ISFIFO
# ifndef S_IFMT
#  define S_IFMT 0170000
# endif
# ifndef S_IFIFO
#  define S_IFIFO 0010000
# endif
# define S_ISFIFO(m)	((m & S_IFMT) == S_IFIFO)
#endif

#if defined(NEED_STRTOUL) && \
	(defined(__ultrix) || defined(__osf__) || defined(NeXT))
# undef NEED_STRTOUL
#endif

#if defined(__ultrix) || defined(__osf__)
# define MAYBE_HESIOD
#endif

#ifndef FD_SET
#define	NFDBITS		32
#define	FD_SETSIZE	32
#define	FD_SET(n, p)	((p)->fds_bits[(n)/NFDBITS] |= (1 << ((n) % NFDBITS)))
#define	FD_CLR(n, p)	((p)->fds_bits[(n)/NFDBITS] &= ~(1 << ((n) % NFDBITS)))
#define	FD_ISSET(n, p)	((p)->fds_bits[(n)/NFDBITS] & (1 << ((n) % NFDBITS)))
#define FD_ZERO(p)	bzero((char *)(p), sizeof(*(p)))
#endif

#ifndef MIN
# define MIN(x, y)	((x > y) ?y :x)
#endif
#ifndef MAX
# define MAX(x, y)	((x > y) ?x :y)
#endif

#if !defined(PATH_MAX)
# if defined(_POSIX_PATH_MAX)
#  define PATH_MAX _POSIX_PATH_MAX
# else
#  if defined(MAXPATHLEN)
#   define PATH_MAX MAXPATHLEN
#  endif
# endif
#endif

#if defined(BSD) || defined(__osf__) || defined(__convex__)
# define HAVE_GETRUSAGE
#endif

/* May be set in the Makefile. */
#if defined(HAVE_GETRUSAGE)
# include <sys/resource.h>
#endif

/*
 *  Because Convex has true library function feof() which is
 *  patently wrong (it test bit _IOREAD) we need feof() as
 *  a macro.
 */
#if defined(__convex__) && !defined(feof)
#  define   feof(p)	((p)->_flag&_IOEOF)
#endif

#if defined(M_UNIX) || defined(linux)
# define SPURIOUS_ECONNREFUSED
#endif

/*
 * Assume that a system has fchmod() unless something above says otherwise.
 */
#if !defined(HAVE_FCHMOD)
# define HAVE_FCHMOD XXX
#endif

/*
 * Some systems need _res to be linked into text rather than bss.
 */
#if defined(__m88k__)
# define __BIND_RES_TEXT
#endif

/*
 * We need to know the IPv6 address family number even on IPv4-only systems.
 * Note that this is NOT a protocol constant, and that if the system has its
 * own AF_INET6, different from ours below, all of BIND's libraries and
 * executables will need to be recompiled after the system <sys/socket.h>
 * has had this type added.  The type number below is correct on most BSD-
 * derived systems for which AF_INET6 is defined.
 */
#ifndef AF_INET6
#define AF_INET6	24
#endif

/*
 * Prototype the functions we'll be supplying.
 */
#ifdef NEED_PUTENV
extern int putenv __P((char *));
#endif

#ifdef NEED_GETTIMEOFDAY
extern int gettimeofday __P((struct timeval *, struct _TIMEZONE *));
#endif

#if defined(SVR4) && defined(sun)
extern int gethostname __P((char *, size_t));
#endif

#ifdef NEED_STRDUP
extern char *strdup __P((const char *));
#endif

#ifdef NEED_STRSEP
extern char *strsep __P((char **, const char *));
#endif

#ifdef NEED_TIMESPEC
struct timespec {
	time_t	tv_sec;		/* seconds */
	long	tv_nsec;	/* nanoseconds */
};
#endif
