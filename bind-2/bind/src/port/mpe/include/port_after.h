/* This top portion was based on the HPUX port and is mostly unaltered. */

#ifndef	PORT_AFTER_H
#define	PORT_AFTER_H
#undef HAVE_SA_LEN
#define USE_POSIX
#define POSIX_SIGNALS
#define USE_WAITPID
#undef HAVE_GETRUSAGE
#undef HAVE_FCHMOD
#define NEED_PSELECT
#define SETGRENT_VOID
#define SETPWENT_VOID
#define SIOCGIFCONF_ADDR
#define CANNOT_SET_SNDBUF
#define CANNOT_CONNECT_DGRAM
#define CAN_CHANGE_ID
#define GET_HOST_ID_MISSING

#define PORT_NONBLOCK	O_NONBLOCK
#define PORT_WOULDBLK	EWOULDBLOCK
#define WAIT_T		int

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/param.h>
#if (!defined(BSD)) || (BSD < 199306)
#include <sys/bitypes.h>
#endif

#ifndef MIN
# define MIN(x, y)	((x > y) ?y :x)
#endif
#ifndef MAX
# define MAX(x, y)	((x > y) ?x :y)
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

#ifndef	PF_INET6
#define PF_INET6	AF_INET6
#endif

#ifndef HAS_INET6_STRUCTS
/* Replace with structure from later rev of O/S if known. */
struct in6_addr {
	u_int8_t	s6_addr[16];
};

/* Replace with structure from later rev of O/S if known. */
struct sockaddr_in6 {
#ifdef	HAVE_SA_LEN
	u_int8_t	sin6_len;	/* length of this struct */
	u_int8_t	sin6_family;	/* AF_INET6 */
#else
	u_int16_t	sin6_family;	/* AF_INET6 */
#endif
	u_int16_t	sin6_port;	/* transport layer port # */
	u_int32_t	sin6_flowinfo;	/* IPv6 flow information */
	struct in6_addr	sin6_addr;	/* IPv6 address */
	u_int32_t	sin6_scope_id;	/* set of interfaces for a scope */
};
#endif	/* HAS_INET6_STRUCTS */
extern int gethostname(char *, size_t);

#define NEED_STRSEP
extern char *strsep(char **, const char *);

#define NEED_DAEMON
int daemon(int nochdir, int noclose);

#undef _ptr

/* This bottom portion is all MPE-specific. */

/* Define various constants omitted from MPE /usr/include */

#define AF_UNSPEC	0
#define IFF_POINTOPOINT	0x10
#define IN_LOOPBACKNET	127
#define INADDR_NONE	(u_long)0xffffffff
#define ITIMER_REAL	0
#define PF_INET		AF_INET
#define PF_UNSPEC	AF_UNSPEC
#define SIOCGIFDSTADDR	0
#define S_IFSOCK	0140000 /* socket */
#define S_ISSOCK(_M)	((_M & S_IFMT)==S_IFSOCK) /* test for socket */
#define S_IREAD		S_IRUSR /* read permission, owner */
#define S_IWRITE	S_IWUSR /* write permission, owner */
#define SOCK_RAW	3

/* Some constants are defined in /usr/include, yet are unsupported.  Nuke em. */

#undef F_DUPFD		/* unsupported for sfcntl() */
#undef SO_RCVBUF
#undef SO_SNDBUF

/* Functions omitted from MPE but supplied elsewhere by BIND. */

#define NEED_GETTIMEOFDAY
#define _TIMEZONE timezone
#define NEED_MKSTEMP
#define NEED_READV
#define readv(a,b,c) __readv(a,b,c)
#define NEED_SETITIMER
#define setitimer(a,b,c) __setitimer(a,b,c)

#ifndef _MPE_ITIMERVAL
#define _MPE_ITIMERVAL
struct itimerval {
	struct timeval it_interval;    /* timer interval */
	struct timeval it_value;       /* current value */
};
#endif

#define NEED_STRDUP
extern char *strdup(const char *);
#define NEED_UTIMES
#define utimes __utimes
#define NEED_WRITEV
#define writev(a,b,c) __writev(a,b,c)

/* Functions omitted from MPE but with acceptable substitutes in MPE. */

#define bcmp			memcmp
#define bcopy(src,dst,len)	memcpy(dst,src,len)
#define bzero(dst,len)		memset(dst,0,len)
#define vfork			fork
#define WCOREDUMP(status)	0
#define WCOREDUMP(status)	0
#endif /* ! PORT_AFTER_H */
