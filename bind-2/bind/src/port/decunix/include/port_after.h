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
#define PORT_AFTER_H
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <net/route.h>

/* Digital UNIX underestimates itself. */
#if defined(BSD) && (BSD < 199103)
# undef BSD
# define BSD 199103
#endif

/* Digital UNIX deprecates send() which BIND uses. */
#undef send

#define USE_POSIX
#define USE_UTIME
#define POSIX_SIGNALS
#define NEED_STRSEP
#define USE_WAITPID
#define MAYBE_HESIOD
#define HAVE_GETRUSAGE
#define HAVE_FCHMOD
#define NEED_PSELECT
#define HAVE_SA_LEN
#define FIX_ZERO_SA_LEN
#define USE_LOG_CONS
#define RLIMIT_TYPE rlim_t
#define RLIMIT_FILE_INFINITY
#define HAVE_CHROOT
#define CAN_CHANGE_ID
#define HAVE_PORT_R

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
#include <netinet/in.h>	/* in_port_t */

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
extern char *strsep(char **, const char *);

#define CHECK_UDP_SUM
#define FIX_UDP_SUM
#define KSYMS "/vmunix"
#define KMEM "/dev/kmem"
#define UDPSUM "udpcksum"
#endif	/* ! PORT_AFTER_H */
