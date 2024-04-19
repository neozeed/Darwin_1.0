#undef WANT_IRS_NIS
#undef WANT_IRS_PW
#undef WANT_IRS_GR
#define SIG_FN void

#if defined(HAS_PTHREADS) && defined(_REENTRANT)
#define DO_PTHREADS
#endif

/* We grab paths.h now to avoid troubles with redefining _PATH_HEQUIV later. */
#include <paths.h>
