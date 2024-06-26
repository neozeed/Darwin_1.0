/*
 * Copyright 1996 1995 by Open Software Foundation, Inc. 1997 1996 1995 1994 1993 1992 1991  
 *              All Rights Reserved 
 *  
 * Permission to use, copy, modify, and distribute this software and 
 * its documentation for any purpose and without fee is hereby granted, 
 * provided that the above copyright notice appears in all copies and 
 * that both the copyright notice and this permission notice appear in 
 * supporting documentation. 
 *  
 * OSF DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE 
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS 
 * FOR A PARTICULAR PURPOSE. 
 *  
 * IN NO EVENT SHALL OSF BE LIABLE FOR ANY SPECIAL, INDIRECT, OR 
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM 
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN ACTION OF CONTRACT, 
 * NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION 
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. 
 * 
 */
/*
 * MkLinux
 */

/*
 * POSIX Threads - IEEE 1003.1c
 */

#ifndef _POSIX_PTHREAD_H
#define _POSIX_PTHREAD_H

#ifndef __POSIX_LIB__
#include <pthread_impl.h>
#endif
#include <errno.h>
#include <sched.h>
#include <time.h>
#include <mach/mach_types.h>

/*
 * These symbols indicate which [optional] features are available
 * They can be tested at compile time via '#ifdef XXX'
 * The way to check for pthreads is like so:
 
 * #include <unistd.h>
 * #ifdef _POSIX_THREADS
 * #include <pthread.h>
 * #endif

 */

/* These will be moved to unistd.h */

#define _POSIX_THREAD_ATTR_STACKADDR
#define _POSIX_THREAD_ATTR_STACKSIZE
#define _POSIX_THREAD_PRIORITY_SCHEDULING
#define _POSIX_THREAD_PRIO_INHERIT
#define _POSIX_THREAD_PRIO_PROTECT

/* These two should be defined also */
#undef  _POSIX_THREAD_PROCESS_SHARED
#undef  _POSIX_THREAD_SAFE_FUNCTIONS

/* This will be moved to limits.h */
#define PTHREAD_STACK_MIN 8192
/*
 * Note: These data structures are meant to be opaque.  Only enough
 * structure is exposed to support initializers.
 * All of the typedefs will be moved to <sys/types.h>
 */

#include <sys/cdefs.h>

__BEGIN_DECLS
/*
 * Threads
 */
struct _pthread_handler_rec
{
	void           *(*routine)(void *);  /* Routine to call */
	void           *arg;                 /* Argument to pass */
	struct _pthread_handler_rec *next;
};

#ifndef __POSIX_LIB__
typedef struct _opaque_pthread_t { long sig; struct _pthread_handler_rec *cleanup_stack; char opaque[__PTHREAD_SIZE__];} *pthread_t;
#endif

/*
 * Cancel cleanup handler management.  Note, since these are implemented as macros,
 * they *MUST* occur in matched pairs!
 */

#define pthread_cleanup_push(routine, arg) \
   { \
	     struct _pthread_handler_rec __handler; \
	     pthread_t __self; \
	     __handler.routine = routine; \
	     __handler.arg = arg; \
	     __handler.next = __self->cleanup_stack; \
	     __self->cleanup_stack = &__handler;

#define pthread_cleanup_pop(execute) \
	     /* Note: 'handler' must be in this same lexical context! */ \
	     __self->cleanup_stack = __handler.next; \
	     if (execute) (handler.routine)(handler.arg); \
   }
	
/*
 * Thread attributes
 */
#ifndef __POSIX_LIB__
typedef struct _opaque_pthread_attr_t { long sig; char opaque[__PTHREAD_ATTR_SIZE__]; } pthread_attr_t;
#endif

#define PTHREAD_CREATE_JOINABLE      1
#define PTHREAD_CREATE_DETACHED      2

#define PTHREAD_INHERIT_SCHED        1
#define PTHREAD_EXPLICIT_SCHED       2

#define PTHREAD_CANCEL_ENABLE        0x01  /* Cancel takes place at next cancellation point */
#define PTHREAD_CANCEL_DISABLE       0x00  /* Cancel postponed */
#define PTHREAD_CANCEL_DEFERRED      0x02  /* Cancel waits until cancellation point */
#define PTHREAD_CANCEL_ASYNCHRONOUS  0x00  /* Cancel occurs immediately */

/* We only support PTHREAD_SCOPE_SYSTEM */
#define PTHREAD_SCOPE_SYSTEM         1
#define PTHREAD_SCOPE_PROCESS        2

/* Who defines this? */

#if !defined(ENOTSUP)
#define ENOTSUP 89
#endif
/*
 * Mutex attributes
 */
#ifndef __POSIX_LIB__
typedef struct _opaque_pthread_mutexattr_t { long sig; char opaque[__PTHREAD_MUTEXATTR_SIZE__]; } pthread_mutexattr_t;
#endif

#define PTHREAD_PRIO_NONE            0
#define PTHREAD_PRIO_INHERIT         1
#define PTHREAD_PRIO_PROTECT         2

/*
 * Mutex variables
 */
#ifndef __POSIX_LIB__
typedef struct _opaque_pthread_mutex_t { long sig; char opaque[__PTHREAD_MUTEX_SIZE__]; } pthread_mutex_t;
#endif

#define PTHREAD_MUTEX_INITIALIZER {_PTHREAD_MUTEX_SIG_init}

/*
 * Condition variable attributes
 */
#ifndef __POSIX_LIB__
typedef struct _opaque_pthread_condattr_t { long sig; char opaque[__PTHREAD_CONDATTR_SIZE__]; } pthread_condattr_t;
#endif

/*
 * Condition variables
 */
#ifndef __POSIX_LIB__
typedef struct _opaque_pthread_cond_t { long sig;  char opaque[__PTHREAD_COND_SIZE__]; } pthread_cond_t;
#endif

#define PTHREAD_COND_INITIALIZER {_PTHREAD_COND_SIG_init}

/*
 * Initialization control (once) variables
 */
#ifndef __POSIX_LIB__

typedef struct { long sig; char opaque[__PTHREAD_ONCE_SIZE__]; } pthread_once_t;
#endif

#define PTHREAD_ONCE_INIT {_PTHREAD_ONCE_SIG_init}

/*
 * Thread Specific Data - keys
 */
typedef unsigned long pthread_key_t;    /* Opaque 'pointer' */

#include <sys/time.h>

/*
 * Prototypes for all PTHREAD interfaces
 */
int       pthread_attr_destroy __P((pthread_attr_t *attr));
int       pthread_attr_getdetachstate __P((const pthread_attr_t *attr,
				      int *detachstate));
int       pthread_attr_getinheritsched __P((const pthread_attr_t *attr, 
				       int *inheritsched));
int       pthread_attr_getschedparam __P((const pthread_attr_t *attr, 
                                     struct sched_param *param));
int       pthread_attr_getschedpolicy __P((const pthread_attr_t *attr, 
				      int *policy));
int       pthread_attr_getstackaddr __P((const pthread_attr_t *attr,
                                      void **stackaddr));
int       pthread_attr_getstacksize __P((const pthread_attr_t *attr,
                                      size_t *stacksize));
int       pthread_attr_init __P((pthread_attr_t *attr));
int       pthread_attr_setdetachstate __P((pthread_attr_t *attr, 
				      int detachstate));
int       pthread_attr_setinheritsched __P((pthread_attr_t *attr, 
				       int inheritsched));
int       pthread_attr_setschedparam __P((pthread_attr_t *attr, 
                                     const struct sched_param *param));
int       pthread_attr_setschedpolicy __P((pthread_attr_t *attr, 
				      int policy));
int       pthread_attr_setstackaddr __P((pthread_attr_t *attr,
                                      void *stackaddr));
int       pthread_attr_setstacksize __P((pthread_attr_t *attr,
                                      size_t stacksize));
int       pthread_cancel __P((pthread_t thread));
int       pthread_setcancelstate __P((int state, int *oldstate));
int       pthread_setcanceltype __P((int type, int *oldtype));
void      pthread_testcancel __P((void));
int       pthread_cond_broadcast __P((pthread_cond_t *cond));
int       pthread_cond_destroy __P((pthread_cond_t *cond));
int       pthread_cond_init __P((pthread_cond_t *cond,
                            const pthread_condattr_t *attr));
int       pthread_cond_signal __P((pthread_cond_t *cond));
int       pthread_cond_wait __P((pthread_cond_t *cond, 
			    pthread_mutex_t *mutex));
int       pthread_cond_timedwait __P((pthread_cond_t *cond, 
				 pthread_mutex_t *mutex,
				 const struct timespec *abstime));
int       pthread_cond_timedwait_relative_np __P((pthread_cond_t *cond, 
				 pthread_mutex_t *mutex,
				 const struct timespec *reltime));
int       pthread_create __P((pthread_t *thread, 
                         const pthread_attr_t *attr,
                         void *(*start_routine)(void *), 
                         void *arg));
int       pthread_detach __P((pthread_t thread));
int       pthread_equal __P((pthread_t t1, 
			pthread_t t2));
void      pthread_exit __P((void *value_ptr));
int       pthread_getschedparam __P((pthread_t thread, 
				int *policy,
                                struct sched_param *param));
int       pthread_join __P((pthread_t thread, 
		       void **value_ptr));
int       pthread_mutex_destroy __P((pthread_mutex_t *mutex));
int       pthread_mutex_getprioceiling __P((const pthread_mutex_t *mutex, 
                                       int *prioceiling));
int       pthread_mutex_init __P((pthread_mutex_t *mutex, 
			     const pthread_mutexattr_t *attr));
int       pthread_mutex_lock __P((pthread_mutex_t *mutex));
int       pthread_mutex_setprioceiling __P((pthread_mutex_t *mutex, 
                                       int prioceiling, 
                                       int *old_prioceiling));
int       pthread_mutex_trylock __P((pthread_mutex_t *mutex));
int       pthread_mutex_unlock __P((pthread_mutex_t *mutex));
int       pthread_mutexattr_destroy __P((pthread_mutexattr_t *attr));
int       pthread_mutexattr_getprioceiling __P((const pthread_mutexattr_t *attr, 
                                           int *prioceiling));
int       pthread_mutexattr_getprotocol __P((const pthread_mutexattr_t *attr, 
                                        int *protocol));
int       pthread_mutexattr_init __P((pthread_mutexattr_t *attr));
int       pthread_mutexattr_setprioceiling __P((pthread_mutexattr_t *attr, 
                                           int prioceiling));
int       pthread_mutexattr_setprotocol __P((pthread_mutexattr_t *attr, 
                                        int protocol));
int       pthread_once __P((pthread_once_t *once_control, 
		       void (*init_routine)(void)));
pthread_t pthread_self __P((void));
int       pthread_setschedparam __P((pthread_t thread, 
				int policy,
                                const struct sched_param *param));
int       pthread_key_create __P((pthread_key_t *key,
			     void (*destructor)(void *)));
int       pthread_key_delete __P((pthread_key_t key));
int       pthread_setspecific __P((pthread_key_t key,
			      const void *value));
void     *pthread_getspecific __P((pthread_key_t key));
int       pthread_attr_getscope __P((pthread_attr_t *, int *));
int       pthread_attr_setscope __P((pthread_attr_t *, int));

/* returns non-zero if pthread_create or cthread_fork have been called */
int		pthread_is_threaded_np __P((void));

/* return the mach thread bound to the pthread */
mach_port_t 	pthread_mach_thread_np __P((pthread_t));
size_t	 	pthread_get_stacksize_np __P((pthread_t));
void *		pthread_get_stackaddr_np __P((pthread_t));

/* Like pthread_cond_signal(), but only wake up the specified pthread */
int		pthread_cond_signal_thread_np __P((pthread_cond_t *, pthread_t));

/* Like pthread_create(), but leaves the thread suspended */
int       pthread_create_suspended_np __P((pthread_t *thread, 
                         const pthread_attr_t *attr,
                         void *(*start_routine)(void *), 
                         void *arg));
__END_DECLS
#endif /* _POSIX_PTHREAD_H */
