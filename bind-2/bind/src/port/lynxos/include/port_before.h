#undef WANT_IRS_PW
#undef WANT_IRS_GR
#define SIG_FN void
#define SETGRENT_VOID

#define __NO_INCLUDE_WARN__

#if defined(HAS_PTHREADS) && defined(_REENTRANT)
#define DO_PTHREADS
#endif

#include <time.h>
