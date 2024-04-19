#define SVR4
#define WANT_IRS_NIS
#undef WANT_IRS_PW
#undef WANT_IRS_GR
#define SIG_FN void

#if defined(HAS_PTHREADS) && defined(_REENTRANT)
#define DO_PTHREADS
#endif

#include <limits.h>	/* _POSIX_PATH_MAX */
#include <sys/endian.h> /* endianness */

#define IRIX_EMUL_IOCTL_SIOCGIFCONF

#ifdef IRIX_EMUL_IOCTL_SIOCGIFCONF
#include <sys/types.h>
#include <net/if.h>	
		/* struct ifconf, struct if_msghdr, struct ifa_msghdr,
		 * struct ifreq
		 */

int emul_ioctl(	struct ifconf * ifc );

#endif	/* #ifdef IRIX_EMUL_IOCTL_SIOCGIFCONF */
