/*
 * @OSF_FREE_COPYRIGHT@
 */
/*
 * Mach Operating System
 * Copyright (c) 1991,1990,1989,1988,1987 Carnegie Mellon University
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */
/*
 */
/*
 *	File:	thread.h
 *	Author:	Avadis Tevanian, Jr.
 *
 *	This file contains the structure definitions for threads.
 *
 */
/*
 * Copyright (c) 1993 The University of Utah and
 * the Computer Systems Laboratory (CSL).  All rights reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * THE UNIVERSITY OF UTAH AND CSL ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS
 * IS" CONDITION.  THE UNIVERSITY OF UTAH AND CSL DISCLAIM ANY LIABILITY OF
 * ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * CSL requests users of this software to return to csl-dist@cs.utah.edu any
 * improvements that they make and grant CSL redistribution rights.
 *
 */

#ifndef	_KERN_THREAD_H_
#define _KERN_THREAD_H_

#include <mach/port.h>
#include <mach/mach_types.h>
#include <mach/message.h>
#include <mach/boolean.h>
#include <mach/vm_types.h>
#include <mach/vm_prot.h>
#include <mach/thread_info.h>
// Start APPLE
//#include <machine/thread.h>
// End APPLE
#include <mach/thread_status.h>

/*
 * Logically, a thread of control consists of two parts:
 *	a thread_shuttle, which may migrate during an RPC, and
 *	a thread_activation, which remains attached to a task.
 * The thread_shuttle is the larger portion of the two-part thread,
 * and contains scheduling info, messaging support, accounting info,
 * and links to the thread_activation within which the shuttle is
 * currently operating.
 *
 * It might make sense to have the thread_shuttle be a proper sub-structure
 * of the thread, with the thread containing links to both the shuttle and
 * activation.  In order to reduce the scope and complexity of source
 * changes and the overhead of maintaining these linkages, we have subsumed
 * the shuttle into the thread, calling it a thread_shuttle.
 *
 * User accesses to threads always come in via the user's thread port,
 * which gets translated to a pointer to the target thread_activation.
 * Kernel accesses intended to effect the entire thread, typically use
 * a pointer to the thread_shuttle (current_thread()) as the target of
 * their operations.  This makes sense given that we have subsumed the
 * shuttle into the thread_shuttle, eliminating one set of linkages.
 * Operations effecting only the shuttle may use a thread_shuttle_t
 * to indicate this.
 *
 * The current_act() macro returns a pointer to the current thread_act, while
 * the current_thread() macro returns a pointer to the currently active
 * thread_shuttle (representing the thread in its entirety).
 */

#ifdef MACH_KERNEL_PRIVATE
#include <cpus.h>
#include <hw_footprint.h>
#include <mach_host.h>
#include <mach_prof.h>
#include <dipc.h>
#include <xkmachkernel.h>
#include <mach_lock_mon.h>
#include <mach_ldebug.h>

#include <kern/sched_prim.h>
#include <kern/ast.h>
#include <kern/kern_types.h>
#include <kern/cpu_number.h>
#include <kern/queue.h>
#include <kern/cpu_data.h>
#include <kern/time_out.h>
#include <kern/timer.h>
#include <kern/lock.h>
#include <kern/sched.h>
#include <vm/vm_map.h>		/* for vm_map_version_t */
#include <kern/thread_pool.h>
#include <kern/thread_act.h>
#include <kern/thread_call.h>
#include <kern/thread_call_private.h>
#include <kern/task.h>
#include <ipc/ipc_kmsg.h>

#define	NO_EVENT	((event_t)0)
#define	WAKING_EVENT	((event_t)~0)

typedef struct thread_shuttle {
        /*
         * Beginning of thread_shuttle proper
         */
        queue_chain_t	links;		/* current run queue links */
        run_queue_t	runq;		/* run queue p is on SEE BELOW */
        int		whichq;		/* which queue level p is on */
/*
 *	NOTE:	The runq field in the thread structure has an unusual
 *	locking protocol.  If its value is RUN_QUEUE_NULL, then it is
 *	locked by the thread_lock, but if its value is something else
 *	(i.e. a run_queue) then it is locked by that run_queue's lock.
 */

        /* Thread bookkeeping */
        queue_chain_t	pset_threads;	/* list of all shuttles in proc set */

        /* Self-preservation */
        decl_simple_lock_data(,lock)	/* scheduling lock (thread_lock()) */
        decl_simple_lock_data(,wake_lock) /* covers wake_active (wake_lock())*/
        decl_mutex_data(,rpc_lock)	/* RPC lock (rpc_lock()) */
        int		ref_count;	/* number of references to me */

        vm_offset_t     kernel_stack;   /* accurate only if the thread is
                                           not swapped and not executing */

        vm_offset_t	stack_privilege;/* reserved kernel stack */

        /* Blocking information */
        int		reason;		/* why we blocked */
        event_t		wait_event;	/* event we are waiting on */
        kern_return_t	wait_result;	/* outcome of wait -
                                           may be examined by this thread
                                           WITHOUT locking */
        queue_chain_t	wait_link;	/* event's wait queue link */
        boolean_t	wake_active;	/* Someone is waiting for this
                                           thread to become suspended */
        int		state;		/* Thread state: */
        boolean_t	preempt;	/* Thread is undergoing preemption */

#if	ETAP_EVENT_MONITOR
        int		etap_reason;	/* real reason why we blocked */
        boolean_t	etap_trace;	/* ETAP trace status */
#endif	/* ETAP_EVENT_MONITOR */

/*
 *	Thread states [bits or'ed]
 */
#define TH_WAIT			0x01	/* thread is queued for waiting */
#define TH_SUSP			0x02	/* thread has been asked to stop */
#define TH_RUN			0x04	/* thread is running or on runq */
#define TH_UNINT		0x08	/* thread is waiting uninteruptibly */
#define	TH_HALTED		0x10	/* thread is halted at clean point ? */

#define TH_ABORT		0x20	/* abort interruptible waits */
#define TH_SWAPPED_OUT		0x40	/* thread is swapped out */

#define TH_IDLE			0x80	/* thread is an idle thread */

#define	TH_SCHED_STATE	(TH_WAIT|TH_SUSP|TH_RUN|TH_UNINT)


/* These two flags will never be seen and might well be removed */
#define	TH_STACK_HANDOFF	0x0100	/* thread has no kernel stack */
#define	TH_STACK_COMING_IN	0x0200	/* thread is waiting for kernel stack */
#define	TH_STACK_STATE	(TH_STACK_HANDOFF | TH_STACK_COMING_IN)

#if 0 /* Grenoble version */
        int		preempt;	/* Thread preemption status */
#define	TH_PREEMPTABLE		0	/* Thread is preemptable */
#define	TH_NOT_PREEMPTABLE	1	/* Thread is not preemptable */
#define	TH_PREEMPTED		2	/* Thread has been preempted */

#if	ETAP_EVENT_MONITOR
        int		etap_reason;	/* real reason why we blocked */
        boolean_t	etap_trace;	/* ETAP trace status */
#endif	/* ETAP_EVENT_MONITOR */

#endif

        /* Stack handoff information */
        void		(*continuation)(/* start here next time runnable */
                                void);
    int         cont_arg;    /* continuation argument */

        /* Scheduling information */
        int		policy;		/* scheduling policy */
        sp_info_t	sp_info;	/* policy-specific information */
        int		pending_policy;	/* pending scheduling policy */
        sp_attributes_t	pending_sched_attr;
                                        /* policy-specific information */
        kern_return_t	change_sfr;	/* return value for pending change */
        int		sched_pri;	/* scheduled (computed) priority */
        unsigned int	sleep_stamp;	/* last time in TH_WAIT state */
#if 0 /* Grenoble */
        unsigned int	sched_change_stamp;
                                        /* last time priority or policy was
                                           explicitly changed (not the same
                                           units as sched_stamp!) */
        int		unconsumed_quantum;	/* leftover quantum (RR/FIFO) */
#endif

        /* VM global variables */
        boolean_t	vm_privilege;	/* can use reserved memory? */
        vm_offset_t	recover;	/* page fault recovery (copyin/out) */

        /* IPC data structures */
        struct thread_shuttle *ith_next, *ith_prev;
        mach_msg_return_t ith_state;
        mach_msg_size_t ith_msize;		/* max size for recvd msg */
        struct ipc_kmsg *ith_kmsg;		/* received message */
        mach_port_seqno_t ith_seqno;		/* seqno of recvd message */

        struct ipc_kmsg_queue ith_messages;

        decl_mutex_data(,ith_lock_data)
        mach_port_t ith_mig_reply;	/* reply port for mig */
        struct ipc_port	*ith_rpc_reply;	/* reply port for kernel RPCs */

        /* Various bits of stashed state */
        union {
                struct {
                        mach_msg_option_t option;
                        mach_msg_body_t *scatter_list;
                        mach_msg_size_t scatter_list_size;
                } receive;
                char *other;		/* catch-all for other state */
        } saved;

        /* Timing data structures */
        timer_data_t	user_timer;	/* user mode timer */
        timer_data_t	system_timer;	/* system mode timer */
        timer_data_t	depressed_timer;/* depressed priority timer */
        timer_save_data_t user_timer_save;  /* saved user timer value */
        timer_save_data_t system_timer_save;  /* saved sys timer val. */
        /*** ??? should the next two fields be moved to SP-specific struct?***/
        unsigned int	cpu_delta;	/* cpu usage since last update */
        unsigned int	sched_delta;	/* weighted cpu usage since update */

        /* Timers for time-outs */
        thread_call_data_t		wait_timer;
        boolean_t				wait_timer_is_set;
        thread_call_data_t		depress_timer;

        /* Ast/Halt data structures */
        boolean_t	active;		/* how alive is the thread */

        /* Processor data structures */
        processor_set_t	processor_set;	/* assigned processor set */
#if	NCPUS > 1
        processor_t	bound_processor;	/* bound to processor ?*/
#endif	/* NCPUS > 1 */
#if	MACH_HOST
        boolean_t	may_assign;	/* may assignment change? */
        boolean_t	assign_active;	/* someone waiting for may_assign */
#endif	/* MACH_HOST */

#if	XKMACHKERNEL
        int		xk_type;
#endif	/* XKMACHKERNEL */

#if	NCPUS > 1
        processor_t	last_processor; /* processor this last ran on */
#if	MACH_LOCK_MON
        unsigned	lock_stack;	/* number of locks held */
#endif  /* MACH_LOCK_MON */
#endif	/* NCPUS > 1 */

        int		at_safe_point;	/* thread_abort_safely allowed */
        mutex_t		*funnel_lock;

#if	MACH_LDEBUG
        /*
         *	Debugging:  track acquired mutexes and locks.
         *	Because a thread can block while holding such
         *	synchronizers, we think of the thread as
         *	"owning" them.
         */
#define	MUTEX_STACK_DEPTH	20
#define	LOCK_STACK_DEPTH	20
        mutex_t		*mutex_stack[MUTEX_STACK_DEPTH];
        lock_t		*lock_stack[LOCK_STACK_DEPTH];
        unsigned int	mutex_stack_index;
        unsigned int	lock_stack_index;
        unsigned	mutex_count;	/* XXX to be deleted XXX */
        boolean_t	kthread;	/* thread is a kernel thread */
#endif	/* MACH_LDEBUG */

        /*
         * End of thread_shuttle proper
         */

        /*
         * Migration and thread_activation linkage information
         */
        struct thread_activation *top_act; /* "current" thr_act */

} Thread_Shuttle;

#define THREAD_SHUTTLE_NULL	((thread_shuttle_t)0)

/* typedef of thread_t is in kern/kern_types.h */

#define	ith_wait_result		wait_result

#define	ith_option		saved.receive.option
#define ith_scatter_list	saved.receive.scatter_list
#define ith_scatter_list_size	saved.receive.scatter_list_size
#define ith_other		saved.other

/*
 * thread_t->at_safe_point values
 */
#define NOT_AT_SAFE_POINT		 0
#define SAFE_EXCEPTION_RETURN		-1
#define SAFE_BOOTSTRAP_RETURN		-2
#define SAFE_EXTERNAL_RECEIVE		-3
#define SAFE_THR_DEPRESS		-4
#define SAFE_SUSPENDED			-5
#define SAFE_MISCELLANEOUS		-6
#define SAFE_INTERNAL_RECEIVE		-7

extern thread_act_t active_kloaded[NCPUS];	/* "" kernel-loaded acts */
extern vm_offset_t active_stacks[NCPUS];	/* active kernel stacks */
extern vm_offset_t kernel_stack[NCPUS];

decl_mutex_data(extern,funnel_lock);

#ifndef MACHINE_STACK_STASH
/*
 * MD Macro to fill up global stack state,
 * keeping the MD structure sizes + games private
 */
#define MACHINE_STACK_STASH(stack)								\
MACRO_BEGIN														\
        mp_disable_preemption();									\
        active_stacks[cpu_number()] = (stack);						\
        kernel_stack[cpu_number()] = (stack) + KERNEL_STACK_SIZE;	\
        mp_enable_preemption();										\
MACRO_END
#endif	/* MACHINE_STACK_STASH */

/*
 *	Kernel-only routines
 */

/* Initialize thread module */
extern void		thread_init(void);

/* Take reference on thread (make sure it doesn't go away) */
extern void		thread_reference(
                                        thread_t		thread);

/* Release reference on thread */
extern void		thread_deallocate(
                                        thread_t		thread);

/* Set priority of calling thread */
extern void		thread_set_own_priority(
                                        int				priority);

/* Reset thread's priority */
extern kern_return_t	thread_priority(
                                                        thread_act_t	thr_act,
                                                        int				priority,
                                                        boolean_t		set_max);

/* Reset thread's max priority */
extern kern_return_t	thread_max_priority(
                                                        thread_act_t	thr_act,
                                                        processor_set_t	pset,
                                                        int				max_priority);

/* Reset thread's max priority while holding RPC locks */
extern kern_return_t	thread_max_priority_locked(
                                                        thread_t		thread,
                                                        processor_set_t	pset,
                                                        int				max_priority);

/* Set a thread's priority while holding RPC locks */
extern kern_return_t	thread_priority_locked(
                                                        thread_t		thread,
                                                        int				priority,
                                                        boolean_t		set_max);

/* Start a thread at specified routine */
#define thread_start(thread, start)						\
                                        (thread)->continuation = (start)

/* Create a kernel mode thread */
extern thread_t		kernel_thread(
                                                        task_t			task,
                                                        void			(*start_at)(void));

extern thread_t		kernel_thread_with_attributes(
                                                        task_t			task,
                                                        sp_attributes_t	attributes,
                                                        void			(*start_at)(void),
                                                        boolean_t		start_running);

/* Reaps threads waiting to be destroyed */
extern void		thread_reaper(void);

extern boolean_t	thread_not_preemptable(
                                                        thread_t		thread);

#if	MACH_HOST
/* Preclude thread processor set assignement */
extern void		thread_freeze(
                                        thread_t		thread);

/* Assign thread to a processor set */
extern void		thread_doassign(
                                        thread_t		thread,
                                        processor_set_t	new_pset,
                                        boolean_t		release_freeze);

/* Allow thread processor set assignement */
extern void		thread_unfreeze(
                                        thread_t		thread);

#endif	/* MACH_HOST */

/* Insure thread always has a kernel stack */
extern void		stack_privilege(
                                        thread_t		thread);

extern void		consider_thread_collect(void);

/*
 *	Arguments to specify aggressiveness to thread halt.
 *	Can't have MUST_HALT and SAFELY at the same time.
 */
#define	THREAD_HALT_NORMAL	0
#define	THREAD_HALT_MUST_HALT	1	/* no deadlock checks */
#define	THREAD_HALT_SAFELY	2	/* result must be restartable */

/*
 *	Macro-defined routines
 */

#define thread_pcb(th)		((th)->pcb)

#define	thread_lock_init(th)											\
                                simple_lock_init(&(th)->lock, ETAP_THREAD_LOCK)
#define thread_lock(th)		simple_lock(&(th)->lock)
#define thread_unlock(th)	simple_unlock(&(th)->lock)

#define thread_should_halt(thread)										\
                        (!(thread)->top_act || !(thread)->top_act->active ||		\
                                        (thread)->top_act->ast & (AST_HALT|AST_TERMINATE))

#if 0 /* Gernoble */
/*
 * We consider a thread not preemptable if it is marked as either
 * suspended, waiting or halted.
 * XXX - when scheduling framework and such is done, the
 * thread state check can be eliminated
*/
#define thread_not_preemptable(thread)								\
                                (((thread)->state & (TH_WAIT|TH_SUSP)) != 0)

#endif

#define	thread_lock_pair(ta,tb)					\
MACRO_BEGIN										\
        if ((ta) < (tb)) {							\
                thread_lock(ta);						\
                thread_lock(tb);						\
        }											\
        else {										\
                thread_lock(tb);						\
                thread_lock(ta);						\
        }											\
MACRO_END

#define	thread_unlock_pair(ta,tb)				\
MACRO_BEGIN										\
        if ((ta) < (tb)) {							\
                thread_unlock(tb);						\
                thread_unlock(ta);						\
        }											\
        else {										\
                thread_unlock(ta);						\
                thread_unlock (tb);						\
        }											\
MACRO_END

#define rpc_lock_init(th)	mutex_init(&(th)->rpc_lock, ETAP_THREAD_RPC)
#define rpc_lock(th)		mutex_lock(&(th)->rpc_lock)
#define rpc_lock_try(th)	mutex_try(&(th)->rpc_lock)
#define rpc_unlock(th)		mutex_unlock(&(th)->rpc_lock)

/*
 * Lock to cover wake_active only; like thread_lock(), is taken
 * at splsched().  Used to avoid calling into scheduler with a
 * thread_lock() held.  Precedes thread_lock() (and other scheduling-
 * related locks) in the system lock ordering.
 */
#define wake_lock_init(th)					\
                        simple_lock_init(&(th)->wake_lock, ETAP_THREAD_WAKE)
#define wake_lock(th)		simple_lock(&(th)->wake_lock)
#define wake_unlock(th)		simple_unlock(&(th)->wake_lock)

#define current_act()						\
        (current_thread() ? current_thread()->top_act : THR_ACT_NULL)
#define current_act_fast()	(current_thread()->top_act)

static __inline__ vm_offset_t current_stack(void);
static __inline__ vm_offset_t
current_stack(void)
{
        vm_offset_t	ret;

        mp_disable_preemption();
        ret = active_stacks[cpu_number()];
        mp_enable_preemption();
        return ret;
}

/* These should be safe if only called in running task context */
#define	current_task()		(current_act_fast()->task)
#define	current_map()		(current_act_fast()->map)
#define	current_space()		(current_task()->itk_space)

extern void		pcb_module_init(void);

extern void		pcb_init(
                                        thread_act_t	thr_act);

extern void		pcb_terminate(
                                        thread_act_t	thr_act);

extern void		pcb_collect(
                                        thread_act_t	thr_act);

extern void		pcb_user_to_kernel(
                                        thread_act_t	thr_act);

extern kern_return_t	thread_setstatus(
                                                        thread_act_t			thr_act,
                                                        int						flavor,
                                                        thread_state_t			tstate,
                                                        mach_msg_type_number_t	count);

extern kern_return_t	thread_getstatus(
                                                        thread_act_t			thr_act,
                                                        int						flavor,
                                                        thread_state_t			tstate,
                                                        mach_msg_type_number_t	*count);

extern boolean_t		stack_alloc_try(
                                                        thread_t			    thread,
                                                        void					(*continuation)(void),
                                                        mutex_t					*funnel);

/* This routine now used only internally */
extern kern_return_t	thread_info_shuttle(
                                                        thread_act_t			thr_act,
                                                        thread_flavor_t			flavor,
                                                        thread_info_t			thread_info_out,
                                                        mach_msg_type_number_t	*thread_info_count);

extern void		thread_terminate_self(void);

extern void		thread_user_to_kernel(
                                        thread_t		thread);

/* Machine-dependent routines */
extern void		thread_machine_init(void);

extern void		thread_machine_set_current(
                                        thread_t		thread );

extern kern_return_t	thread_machine_create(
                                                        thread_t			thread,
                                                        thread_act_t		thr_act,
                                                        void				(*start_pos)(void));

extern void		thread_set_syscall_return(
                                        thread_t		thread,
                                        kern_return_t	retval);

extern void		thread_machine_destroy(
                                        thread_t		thread );

extern void		thread_machine_flush(
                                        thread_act_t	thr_act);

#endif /* MACH_KERNEL_PRIVATE */

extern boolean_t	thread_get_funneled(void);

extern boolean_t	thread_set_funneled(
                                                boolean_t		funneled);

#endif	/* _KERN_THREAD_H_ */






/*
 *	File:	thread.h
 *	Author:	A.Ramesh.
 *
 *	This file contains the structure definitions for BSD only.
 *
 */

#ifndef	_KERN_THREAD_H_
#define _KERN_THREAD_H_

#include <mach/mach_types.h>
// Start APPLE
//#include <kern/task.h>
#include "task.h"
// End APPLE
/*
 * JMM - These now come out of mach_types.h
 *       (more to come)
 *
 * struct thread ;
 * typedef struct thread *thread_t;
 * struct thread_activation ;
 * typedef struct thread_activation *thread_act_t;
 */
extern boolean_t	is_thread_running(thread_t); /* True is TH_RUN */
extern boolean_t	is_thread_idle(thread_t); /* True is TH_IDLE */

#define THREAD_NULL	((thread_t) 0)
#define THREAD_MAX	512		/* Max number of threads */
#define THREAD_CHUNK	64		/* Allocation chunk */


extern thread_should_halt(thread_t);

/*
 *	Machine specific implementations of the current thread macro
 *	designate this by defining CURRENT_THREAD.
 */


#define current_thread() get_current_thread()
extern thread_t get_current_thread(void);
#define current_task() get_current_task()
extern task_t get_current_task();
extern void *get_bsdthread_info(thread_t);
extern set_bsdthread_info(thread_t, void *);
extern task_t get_threadtask(thread_t);

/*   Added frolm sched_prim
 *	Possible results of assert_wait - returned in
 *	current_thread()->wait_result.
 */
#define THREAD_AWAKENED		0		/* normal wakeup */
#define THREAD_TIMED_OUT	1		/* timeout expired */
#define THREAD_INTERRUPTED	2		/* interrupted by clear_wait */
#define THREAD_RESTART		3		/* restart operation entirely*/
#if 0
#define THREAD_SHOULD_TERMINATE	4		/* thread should terminate */
#endif
/****** FIXME ********/
#define THREAD_SHOULD_TERMINATE	THREAD_INTERRUPTED /* thread should terminate*/

#endif	/* _KERN_THREAD_H_ */
