
#define LITTLE_ENDIAN	1234
#define BYTE_ORDER	LITTLE_ENDIAN

#include <sys/select.h>
#include <signal.h>
#include <ioctl.h>
#include <unix.h>
#include <sys/cdefs.h>
#define SIG_FN	__sig_func
#define POSIX_SIGNALS
#define USE_POSIX
#define MIN	min
#define MAX	max
#define PORT_NONBLOCK	O_NONBLOCK
#define HAVE_SA_LEN

#if defined(HAS_PTHREADS) && defined(_REENTRANT)
#define DO_PTHREADS
#endif

