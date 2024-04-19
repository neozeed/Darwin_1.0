/*
 * Copyright (c) 1996,1999 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

#ifndef	PORT_AFTER_H
#define	PORT_AFTER_H
#define USE_POSIX
#define USE_UTIME
#define POSIX_SIGNALS
#define USE_WAITPID
#define MAYBE_HESIOD
#define HAVE_GETRUSAGE
#define HAVE_FCHMOD
#define NEED_DAEMON
#define NEED_MKSTEMP
#define SIOCGIFCONF_ADDR
#define HAVE_CHROOT
#define CAN_CHANGE_ID

#define _TIMEZONE	timezone
#define SIG_FN		void
#define PORT_NONBLOCK	O_NONBLOCK
#define PORT_WOULDBLK	EWOULDBLOCK
#define WAIT_T		int

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/param.h>
#if (!defined(BSD)) || (BSD < 199306)
#include <sys/bitypes.h>
#endif

#ifndef AF_INET6
# define AF_INET6	24
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

#define NEED_STRDUP
extern char *strdup(const char *);

#define NEED_PSELECT

#define CHECK_UDP_SUM
#define FIX_UDP_SUM
#define KSYMS "/vmunix"
#define KMEM "/dev/kmem"
#define UDPSUM "udpcksum"

/* ULTRIX provides this but there is no include file that prototypes it. */
int getopt(int, char **, char *);
extern char *optarg;
extern int optind, opterr;

/* ULTRIX has the old two-argument openlog(). */
#define SYSLOG_42BSD
#define LOG_KERN	0
#define LOG_USER	0
#define LOG_MAIL	0
#define LOG_DAEMON	0
#define LOG_AUTH	0
#define LOG_SYSLOG	0
#define LOG_LPR		0
#define LOG_LOCAL0	0
#define LOG_LOCAL1	0
#define LOG_LOCAL2	0
#define LOG_LOCAL3	0
#define LOG_LOCAL4	0
#define LOG_LOCAL5	0
#define LOG_LOCAL6	0
#define LOG_LOCAL7	0
#endif /* ! PORT_AFTER_H */
