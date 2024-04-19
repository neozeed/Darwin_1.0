#ifndef PORT_AFTER_H
#define PORT_AFTER_H
#include <sys/param.h>
#if _BSDI_VERSION < 199802
# define NEED_PSELECT
#else
# define HAS_INET6_STRUCTS
#endif

#define CAN_RECONNECT
#define USE_POSIX
#define POSIX_SIGNALS
#define USE_UTIME
#define USE_WAITPID
#define HAVE_GETRUSAGE
#define HAVE_FCHMOD
#define HAVE_MINIMUM_IFREQ
#define USE_LOG_CONS
#define SIOCGIFCONF_ADDR

#define RLIMIT_LONGLONG
#define RLIMIT_FILE_INFINITY
#define HAVE_CHROOT
#define CAN_CHANGE_ID
#undef	HAVE_PW_CLASS
#undef	HAVE_PW_CHANGE
#undef	HAVE_PW_EXPIRE

#define _TIMEZONE timezone

#define PORT_NONBLOCK	O_NONBLOCK
#define PORT_WOULDBLK	EWOULDBLOCK
#define WAIT_T		int
#define KSYMS		"/bsd"
#define KMEM		"/dev/kmem"
#define UDPSUM		"udpcksum"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/param.h>
#if (!defined(BSD)) || (BSD < 199306)
#include <sys/bitypes.h>
#endif
#include <netinet/in.h>	/* in_port_t */
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

#define	isascii(c)	((unsigned)(c)<=0177)

extern char *optarg;
extern int optind;

#define S_ISSOCK(mode)	(((mode) & (_S_IFMT)) == (_S_IFSOCK))

#define WCOREDUMP(status)	((status) & 0200)

#ifndef ULONG_MAX
#define ULONG_MAX 4294967295U
#endif	/* ! ULONG_MAX */

#endif	/* ! PORT_AFTER_H */
