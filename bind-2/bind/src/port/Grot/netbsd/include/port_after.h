#define CAN_RECONNECT
#define USE_POSIX
#define POSIX_SIGNALS
#define USE_UTIME
#define HAVE_SETVBUF
#define USE_WAITPID
#define HAVE_GETRUSAGE
#define HAVE_FCHMOD
#define NEED_PSELECT

#define _TIMEZONE timezone

#define PORT_NONBLOCK	O_NONBLOCK
#define PORT_WOULDBLK	EWOULDBLOCK
#define WAIT_T		int
#define KSYMS		"/bsd"
#define KMEM		"/dev/kmem"
#define UDPSUM		"udpcksum"
