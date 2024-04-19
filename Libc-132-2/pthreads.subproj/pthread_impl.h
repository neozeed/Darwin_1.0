#ifndef _PTHREAD_IMPL_H_
#define _PTHREAD_IMPL_H_
/*
 * Internal implementation details
 */

/* This whole header file will disappear, so don't depend on it... */

#ifndef __POSIX_LIB__

#define __PTHREAD_SIZE__           596 
#define __PTHREAD_ATTR_SIZE__      36
#define __PTHREAD_MUTEXATTR_SIZE__ 8
#define __PTHREAD_MUTEX_SIZE__     40
#define __PTHREAD_CONDATTR_SIZE__  4
#define __PTHREAD_COND_SIZE__      24
#define __PTHREAD_ONCE_SIZE__      4
/*
 * [Internal] data structure signatures
 */
#define _PTHREAD_MUTEX_SIG_init		0x32AAABA7
#define _PTHREAD_COND_SIG_init		0x3CB0B1BB
#define _PTHREAD_ONCE_SIG_init		0x30B1BCBA
/*
 * POSIX scheduling policies
 */
#define SCHED_OTHER                1
#define SCHED_FIFO                 4
#define SCHED_RR                   2

#define __SCHED_PARAM_SIZE__       4

#endif /* __POSIX_LIB__ */

#endif /* _PTHREAD_IMPL_H_ */
