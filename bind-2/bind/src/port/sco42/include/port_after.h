#ifndef	PORT_AFTER_H
#define	PORT_AFTER_H
#undef HAVE_SA_LEN
#define POSIX_SIGNALS
#define USE_WAITPID
#define WAIT_T int
#define SETGRENT_VOID
#define SETPWENT_VOID
#define USE_UTIME
#define HAVE_CHROOT

#define PORT_NONBLOCK	O_NONBLOCK
#define PORT_WOULDBLK	EWOULDBLOCK

#define _S_IFREG S_IFREG

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/param.h>
#if (!defined(BSD)) || (BSD < 199306)
#include <sys/bitypes.h>
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
#define NEED_STRSEP
extern char *strsep(char **, const char *);

#define NEED_DAEMON
int daemon(int nochdir, int noclose);

#define	NEED_MKSTEMP
int mkstemp(char *);

#include <sys/uio.h>
#define NEED_WRITEV
#define writev __writev
int __writev(int, const struct iovec*, int);

#define NEED_READV
#define readv __readv
int __readv(int, const struct iovec*, int);

#define NEED_UTIMES
#include <sys/time.h>
#define utimes __utimes
int __utimes(char *, struct timeval *);

#define ftruncate __ftruncate
int __ftruncate(int, long);

#define vfork fork

extern char *optarg;
extern int optind, opterr;

/* SCO3.2v4.2 provides realloc(), but it doesn't have ANSI C semantics. */
#include "ansi_realloc.h"


/* SCO3.2v4.2 provides gettimeofday(), but it has some problems. */
#include "sco_gettime.h"
#endif /* ! PORT_AFTER_H */
