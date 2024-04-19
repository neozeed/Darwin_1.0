#ifndef	PORT_AFTER_H
#define	PORT_AFTER_H
#define	NEED_DAEMON
extern int daemon(int,int);
#define NEED_MKTEMP
extern char *mktemp(const char *);
#define NEED_MKSTEMP
extern int mkstemp(const char *);
#define NEED_STRCASECMP
extern int strcasecmp(const char *, const char *);
extern int strncasecmp(const char *, const char *, size_t);
#define NEED_STRSEP
extern char *strsep(char **, const char *);
#define NEED_PSELECT

#undef	RENICE

#define WAIT_T	int

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/param.h>
#if (!defined(BSD)) || (BSD < 199306)
#include <sys/bitypes.h>
#endif

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
#endif /* ! PORT_AFTER_H */
