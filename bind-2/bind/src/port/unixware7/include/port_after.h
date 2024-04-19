#ifndef	PORT_AFTER_H
#define	PORT_AFTER_H
#define HAVE_SA_LEN
#define USE_POSIX
#define POSIX_SIGNALS
#define USE_WAITPID
#define HAVE_GETRUSAGE
#define HAVE_FCHMOD
#define SETGRENT_VOID
#define SETPWENT_VOID
#define IP_OPT_BUF_SIZE 40
#define NEED_PSELECT
#define SIOCGIFCONF_ADDR
#define HAVE_CHROOT
#define CAN_CHANGE_ID

#define PORT_NONBLOCK	O_NONBLOCK
#define PORT_WOULDBLK	EWOULDBLOCK
#define WAIT_T		int

#ifndef MIN
# define MIN(x, y)	((x > y) ?y :x)
#endif
#ifndef MAX
# define MAX(x, y)	((x > y) ?x :y)
#endif

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in6_f.h>
#include <netinet/in6.h>
#include <sys/param.h>
#include <sys/bitypes.h>

extern int gethostname(char *, size_t);

#define NEED_STRSEP
extern char *strsep(char **, const char *);

#define NEED_DAEMON
int daemon(int nochdir, int noclose);

#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN 255
#endif

#endif /* ! PORT_AFTER_H */
