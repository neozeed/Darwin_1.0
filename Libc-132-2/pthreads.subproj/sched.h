#ifndef _SCHED_H_
#define _SCHED_H_

#include <pthread_impl.h>

/*
 * Scheduling paramters
 */
#ifndef __POSIX_LIB__
struct sched_param { int sched_priority;  char opaque[__SCHED_PARAM_SIZE__]; };
#endif

extern int sched_yield(void);

#endif /* _SCHED_H_ */