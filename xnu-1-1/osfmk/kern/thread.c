/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
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
 *	File:	kern/thread.c
 *	Author:	Avadis Tevanian, Jr., Michael Wayne Young, David Golub
 *	Date:	1986
 *
 *	Thread/thread_shuttle management primitives implementation.
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

#include <cpus.h>
#include <mach_host.h>
#include <simple_clock.h>
#include <mach_debug.h>
#include <mach_prof.h>
#include <dipc.h>
#include <stack_usage.h>

#include <mach/boolean.h>
#include <mach/policy.h>
#include <mach/thread_info.h>
#include <mach/thread_special_ports.h>
#include <mach/thread_status.h>
#include <mach/time_value.h>
#include <mach/vm_param.h>
#include <kern/ast.h>
#include <kern/cpu_data.h>
#include <kern/counters.h>
#include <kern/etap_macros.h>
#include <kern/ipc_mig.h>
#include <kern/ipc_tt.h>
#include <kern/mach_param.h>
#include <kern/machine.h>
#include <kern/misc_protos.h>
#include <kern/processor.h>
#include <kern/queue.h>
#include <kern/sched.h>
#include <kern/sched_prim.h>
#include <kern/sf.h>
#include <kern/mk_sp.h>	/*** ??? fix so this can be removed ***/
#include <kern/task.h>
#include <kern/thread.h>
#include <kern/thread_act.h>
#include <kern/thread_swap.h>
#include <kern/host.h>
#include <kern/zalloc.h>
#include <vm/vm_kern.h>
#include <ipc/ipc_kmsg.h>
#include <ipc/ipc_port.h>
#include <machine/thread.h>		/* for MACHINE_STACK */
#include <kern/profile.h>
#include <kern/assert.h>
#include <sys/kdebug.h>

/*
 * Exported interfaces
 */

#include <mach/thread_act_server.h>
#include <mach/mach_host_server.h>

/*
 * Per-Cpu stashed global state
 */
vm_offset_t			active_stacks[NCPUS];	/* per-cpu active stacks	*/
vm_offset_t			kernel_stack[NCPUS];	/* top of active stacks		*/
thread_act_t		active_kloaded[NCPUS];	/*  + act if kernel loaded	*/

decl_mutex_data(,	funnel_lock)

struct zone			*thread_shuttle_zone;

queue_head_t		reaper_queue;
decl_simple_lock_data(,reaper_lock)
thread_call_t		thread_reaper_call;

extern int		tick;

extern void		pcb_module_init(void);

/* private */
static struct thread_shuttle	thr_sh_template;

#if	MACH_DEBUG
#if	STACK_USAGE
static void	stack_init(vm_offset_t stack, unsigned int bytes);
void		stack_finalize(vm_offset_t stack);
vm_size_t	stack_usage(vm_offset_t stack);
#else	/*STACK_USAGE*/
#define stack_init(stack, size)
#define stack_finalize(stack)
#define stack_usage(stack) (vm_size_t)0
#endif	/*STACK_USAGE*/

#ifdef	MACHINE_STACK
extern
#endif
    void	stack_statistics(
			unsigned int	*totalp,
			vm_size_t	*maxusagep);

#define	STACK_MARKER	0xdeadbeef
#if	STACK_USAGE
boolean_t		stack_check_usage = TRUE;
#else	/* STACK_USAGE */
boolean_t		stack_check_usage = FALSE;
#endif	/* STACK_USAGE */
decl_simple_lock_data(,stack_usage_lock)
vm_size_t		stack_max_usage = 0;
vm_size_t		stack_max_use = KERNEL_STACK_SIZE - 64;
#endif	/* MACH_DEBUG */

/* Forwards */
void		thread_collect_scan(void);

kern_return_t thread_create_shuttle(
	thread_act_t			thr_act,
	sp_attributes_t			policy_attributes,
	void					(*start_at)(void),
	thread_t				*new_thread);

extern void		Load_context(
	thread_t                thread);


/*
 *	Machine-dependent code must define:
 *		thread_machine_init
 *		thread_machine_terminate
 *		thread_machine_collect
 *
 *	The thread->pcb field is reserved for machine-dependent code.
 */

#ifdef	MACHINE_STACK
/*
 *	Machine-dependent code must define:
 *		stack_alloc_try
 *		stack_alloc
 *		stack_free
 *		stack_collect
 *	and if MACH_DEBUG:
 *		stack_statistics
 */
#else	/* MACHINE_STACK */
/*
 *	We allocate stacks from generic kernel VM.
 *	Machine-dependent code must define:
 *		machine_kernel_stack_init
 *
 *	The stack_free_list can only be accessed at splsched,
 *	because stack_alloc_try/thread_invoke operate at splsched.
 */

decl_simple_lock_data(,stack_lock_data)         /* splsched only */
#define stack_lock()	simple_lock(&stack_lock_data)
#define stack_unlock()	simple_unlock(&stack_lock_data)

vm_offset_t stack_free_list;		/* splsched only */
unsigned int stack_free_count = 0;	/* splsched only */
unsigned int stack_free_limit = 1;	/* patchable */

unsigned int stack_alloc_hits = 0;	/* debugging */
unsigned int stack_alloc_misses = 0;	/* debugging */
unsigned int stack_alloc_max = 0;	/* debugging */

unsigned int stack_alloc_total = 0;
unsigned int stack_alloc_hiwater = 0;

/*
 *	The next field is at the base of the stack,
 *	so the low end is left unsullied.
 */

#define stack_next(stack) (*((vm_offset_t *)((stack) + KERNEL_STACK_SIZE) - 1))

/*
 *	stack_alloc:
 *
 *	Allocate a kernel stack for an activation.
 *	May block.
 */
vm_offset_t
stack_alloc(
	thread_t thread,
	void (*continuation)(void))
{
	vm_offset_t stack;
	spl_t	s;

	/*
	 *	We first try the free list.  It is probably empty,
	 *	or stack_alloc_try would have succeeded, but possibly
	 *	a stack was freed before the swapin thread got to us.
	 */

	s = splsched();
	stack_lock();
	stack = stack_free_list;
	if (stack != 0) {
		stack_free_list = stack_next(stack);
		stack_free_count--;
	}
	stack_unlock();
	splx(s);

	if (stack == 0) {
		/*
		 *	Kernel stacks should be naturally aligned,
		 *	so that it is easy to find the starting/ending
		 *	addresses of a stack given an address in the middle.
		 */

		if (kmem_alloc_aligned(kernel_map, &stack,
				round_page(KERNEL_STACK_SIZE)) != KERN_SUCCESS)
			panic("stack_alloc");

		stack_alloc_total++;
		if (stack_alloc_total > stack_alloc_hiwater)
		  stack_alloc_hiwater = stack_alloc_total;

#if	MACH_DEBUG
		stack_init(stack, round_page(KERNEL_STACK_SIZE));
#endif	/* MACH_DEBUG */

		/*
		 * If using fractional pages, free the remainder(s)
		 */
		if (KERNEL_STACK_SIZE < round_page(KERNEL_STACK_SIZE)) {
		    vm_offset_t ptr  = stack + KERNEL_STACK_SIZE;
		    vm_offset_t endp = stack + round_page(KERNEL_STACK_SIZE);
		    while (ptr < endp) {
#if	MACH_DEBUG
			    /*
			     * We need to initialize just the end of the 
			     * region.
			     */
			    stack_init(ptr, (unsigned int) (endp - ptr));
#endif
				stack_lock();
				stack_next(stack) = stack_free_list;
				stack_free_list = stack;
				if (++stack_free_count > stack_alloc_max)
				  stack_alloc_max = stack_free_count;
				stack_unlock();
			    ptr += KERNEL_STACK_SIZE;
		    }
		}
	}
	stack_attach(thread, stack, continuation);
	return (stack);
}

/*
 *	stack_free:
 *
 *	Free a kernel stack.
 *	Called at splsched.
 */

void
stack_free(
	thread_t thread)
{
    vm_offset_t stack = stack_detach(thread);
	assert(stack);
	if (stack != thread->stack_privilege) {
	  stack_lock();
	  stack_next(stack) = stack_free_list;
	  stack_free_list = stack;
	  if (++stack_free_count > stack_alloc_max)
		stack_alloc_max = stack_free_count;
	  stack_unlock();
	}
}

/*
 *	stack_collect:
 *
 *	Free excess kernel stacks.
 *	May block.
 */

void
stack_collect(void)
{
	register vm_offset_t stack;
	spl_t	s;

	/* If using fractional pages, Cannot just call kmem_free(),
	 * and we're too lazy to coalesce small chunks.
	 */
	if (KERNEL_STACK_SIZE < round_page(KERNEL_STACK_SIZE))
		return;

	s = splsched();
	stack_lock();
	while (stack_free_count > stack_free_limit) {
		stack = stack_free_list;
		stack_free_list = stack_next(stack);
		stack_free_count--;
		stack_unlock();
		splx(s);

#if	MACH_DEBUG
		stack_finalize(stack);
#endif	/* MACH_DEBUG */
		kmem_free(kernel_map, stack, KERNEL_STACK_SIZE);

		s = splsched();
		stack_alloc_total--;
		stack_lock();
	}
	stack_unlock();
	splx(s);
}


#if	MACH_DEBUG
/*
 *	stack_statistics:
 *
 *	Return statistics on cached kernel stacks.
 *	*maxusagep must be initialized by the caller.
 */

void
stack_statistics(
	unsigned int	*totalp,
	vm_size_t	*maxusagep)
{
	spl_t	s;

	s = splsched();
	stack_lock();

#if	STACK_USAGE
	if (stack_check_usage) {
		vm_offset_t stack;

		/*
		 *	This is pretty expensive to do at splsched,
		 *	but it only happens when someone makes
		 *	a debugging call, so it should be OK.
		 */

		for (stack = stack_free_list; stack != 0;
		     stack = stack_next(stack)) {
			vm_size_t usage = stack_usage(stack);

			if (usage > *maxusagep)
				*maxusagep = usage;
		}
	}
#endif	/* STACK_USAGE */

	*totalp = stack_free_count;
	stack_unlock();
	splx(s);
}
#endif	/* MACH_DEBUG */

#endif	/* MACHINE_STACK */


/*
 *	stack_privilege:
 *
 *	stack_alloc_try on this thread must always succeed.
 */

void
stack_privilege(
	register thread_t thread)
{
	/*
	 *	This implementation only works for the current thread.
	 */

	if (thread != current_thread())
		panic("stack_privilege");

	if (thread->stack_privilege == 0)
		thread->stack_privilege = current_stack();
}

/*
 *	stack_alloc_try:
 *
 *	Non-blocking attempt to allocate a kernel stack.
 *	Called at splsched with the thread locked.
 */

boolean_t stack_alloc_try(
	thread_t	thread,
	void		(*resume)(void))
{
	register vm_offset_t stack;

	if ((stack = thread->stack_privilege) == (vm_offset_t)0) {
	  stack_lock();
	  stack = stack_free_list;
	  if (stack != (vm_offset_t)0) {
	    stack_free_list = stack_next(stack);
	    stack_free_count--;
	  }
	  stack_unlock();
	}

	if (stack != 0) {
		stack_attach(thread, stack, resume);
		stack_alloc_hits++;
		return TRUE;
	} else {
		stack_alloc_misses++;
		return FALSE;
	}
}

void
thread_init(void)
{
	thread_shuttle_zone = zinit(
			sizeof(struct thread_shuttle),
			THREAD_MAX * sizeof(struct thread_shuttle),
			THREAD_CHUNK * sizeof(struct thread_shuttle),
			"threads");

	/*
	 *	Fill in a template thread_shuttle for fast initialization.
	 *	[Fields that must be (or are typically) reset at
	 *	time of creation are so noted.]
	 */

	/* thr_sh_template.links (none) */
	thr_sh_template.runq = RUN_QUEUE_NULL;


	/* thr_sh_template.task (later) */
	/* thr_sh_template.thread_list (later) */
	/* thr_sh_template.pset_threads (later) */

	/* one ref for being alive, one to return to the creator */
	thr_sh_template.ref_count = 2;

	thr_sh_template.wait_event = NO_EVENT;
	thr_sh_template.wait_result = KERN_SUCCESS;
	thr_sh_template.wait_queue = WAIT_QUEUE_NULL;
	thr_sh_template.wake_active = FALSE;
	/*thr_sh_template.state = TH_SUSP;*/
	thr_sh_template.state = 0;
	thr_sh_template.continuation = (void (*)(void))thread_bootstrap_return;
	thr_sh_template.top_act = THR_ACT_NULL;

/*	thr_sh_template.priority (later) */
/***???	thr_sh_template.max_priority = BASEPRI_USER; ***/
/*	thr_sh_template.sched_pri (later - compute_priority) */
/***???	thr_sh_template.sched_data = 0; ***/
	thr_sh_template.policy = POLICY_TIMESHARE;
/***???	thr_sh_template.depress_priority = -1; ***/
/***???	thr_sh_template.cpu_usage = 0; ***/
/***???	thr_sh_template.sched_usage = 0; ***/
	/* thr_sh_template.sched_stamp (later) */
/***???	thr_sh_template.sched_change_stamp = 1; ***/

	thr_sh_template.vm_privilege = FALSE;

	/* thr_sh_template.<IPC structures> (later) */

	timer_init(&(thr_sh_template.user_timer));
	timer_init(&(thr_sh_template.system_timer));
	thr_sh_template.user_timer_save.low = 0;
	thr_sh_template.user_timer_save.high = 0;
	thr_sh_template.system_timer_save.low = 0;
	thr_sh_template.system_timer_save.high = 0;
	thr_sh_template.cpu_delta = 0;
	thr_sh_template.sched_delta = 0;

	thr_sh_template.active = FALSE; /* reset */

	/* thr_sh_template.processor_set (later) */
#if	NCPUS > 1
	thr_sh_template.bound_processor = PROCESSOR_NULL;
#endif	/*NCPUS > 1*/
#if	MACH_HOST
	thr_sh_template.may_assign = TRUE;
	thr_sh_template.assign_active = FALSE;
#endif	/* MACH_HOST */
	thr_sh_template.funnel_state = 0;

#if	NCPUS > 1
	/* thr_sh_template.last_processor  (later) */
#endif	/* NCPUS > 1 */

	/*
	 *	Initialize other data structures used in
	 *	this module.
	 */

	queue_init(&reaper_queue);
	simple_lock_init(&reaper_lock, ETAP_THREAD_REAPER);
	mutex_init(&funnel_lock,0);

#ifndef MACHINE_STACK
	simple_lock_init(&stack_lock_data, ETAP_THREAD_STACK);
#endif  /* MACHINE_STACK */

#if	MACH_DEBUG
	simple_lock_init(&stack_usage_lock, ETAP_THREAD_STACK_USAGE);
#endif	/* MACH_DEBUG */

#if	MACH_LDEBUG
	thr_sh_template.kthread = FALSE;
	thr_sh_template.mutex_count = 0;
#endif	/* MACH_LDEBUG */

	/*
	 *	Initialize any machine-dependent
	 *	per-thread structures necessary.
	 */
	thread_machine_init();
}

void
thread_reaper_enqueue(
	thread_t		thread)
{
	/*
	 * thread lock is already held, splsched()
	 * not necessary here.
	 */
	simple_lock(&reaper_lock);

	enqueue_tail(&reaper_queue, (queue_entry_t)thread);
#if 0 /* CHECKME! */
	/*
	 * Since thread has been put in the reaper_queue, it must no longer
	 * be preempted (otherwise, it could be put back in a run queue).
	 */
	thread->preempt = TH_NOT_PREEMPTABLE;
#endif

	simple_unlock(&reaper_lock);

	thread_call_enter(thread_reaper_call);
}

void
thread_terminate_self(void)
{
	register thread_t	thread = current_thread();
	thread_act_t		thr_act, prev_act;
	task_t				task;
	sched_policy_t		*policy;
	spl_t				s;

	/*	
	 *	Check for rpc chain. If so, switch to the previous 
	 *	activation, set error code, switch stacks and jump
 	 *	to mach_rpc_return_error.
	 */
	thr_act = thread->top_act;
	thread = thr_act->thread;
	task = thr_act->task;

	if (task) {
		time_value_t	user_time, system_time;
		void			*proc = NULL;

		/*
		 * Accumulate times for dead threads into task.
		 */
		thread_read_times(thread, &user_time, &system_time);

		task_lock(task);
		time_value_add(&task->total_user_time, &user_time);
		time_value_add(&task->total_system_time, &system_time);
		if (task->thr_act_count == 1)
			proc = task->bsd_info;
		task_unlock(task);

		if (proc)
			proc_exit(proc);
	}

	thread = act_lock_thread(thr_act);

	/* Unlink the thr_act from the task's thr_act list,
	 * so it doesn't appear in calls to task_threads and such.
	 * The thr_act still keeps its ref on the task, however.
	 */
	task_lock(task);
	mutex_lock(&task->act_list_lock);
	queue_remove(&task->thr_acts, thr_act, thread_act_t, thr_acts);

	/*
	 * Decrement the act count for this task.
	 */
	task->thr_act_count--;
		
#if	THREAD_SWAPPER
	/*
	 * Thread is supposed to be unswappable by now...
	 */
	assert(thr_act->swap_state == TH_SW_UNSWAPPABLE ||
		       !(thread_swap_unwire_stack ||
			 thread_swap_unwire_user_stack));
#endif	/* THREAD_SWAPPER */
	task->res_act_count--;
	thr_act->thr_acts.next = NULL;
	mutex_unlock(&task->act_list_lock);
	task_unlock(task);

#ifdef CALLOUT_RPC_MODEL
	if (thr_act->lower) {
		/*
		 * JMM - RPC will not be using a callout/stack manipulation
		 * mechanism.  instead we will let it return normally as if
		 * from a continuation.  Accordingly, these need to be cleaned
		 * up a bit.
		 */
		act_unlock(thr_act);
		act_switch_swapcheck(thread, (ipc_port_t)0);
		act_lock(thr_act);	/* hierarchy violation XXX */
		(void) switch_act(THR_ACT_NULL);
		assert(thr_act->ref_count == 1);	/* XXX */
		/* act_deallocate(thr_act);		   XXX */
		prev_act = thread->top_act;
		/* disable preemption to protect kernel stack changes */
		disable_preemption();
		MACH_RPC_RET(prev_act) = KERN_RPC_SERVER_TERMINATED;
		 * machine_kernel_stack_init(thread, 
		 *	(void (*)(void)) mach_rpc_return_error);
		 * Load_context(thread);
		 */
		/* NOTREACHED */
	}

#else /* !CALLOUT_RPC_MODEL */

	assert(!thr_act->lower);

#endif /* CALLOUT_RPC_MODEL */

	act_unlock_thread(thr_act);

	s = splsched();
	thread_lock(thread);
	policy = &sched_policy[thread->policy];
	thr_act = thread->top_act;
	thread->active = FALSE;
	thread_unlock(thread);
	splx(s);

	policy->sp_ops.sp_thread_depress_abort(policy, thread);
	thread_cancel_timer();

	/* flush any lazy HW state while in own context */
	thread_machine_flush(thr_act);

	/* Reap times from dying threads */
	ipc_thr_act_disable(thr_act);

	/*
  	 * the test for task_active seems unnecessary because
   	 * the thread holds a reference to the task (so it
	 * can't be deleted out from under it).
	 */
	if( task && task->active) {
#if	THREAD_SWAPPER
		thread_swap_disable(thr_act);
#endif	/* THREAD_SWAPPER */

		task_lock(task);

		/* Make act inactive iff it was born as a base activation */
		act_lock_thread(thr_act);
		if( thr_act->active && (thr_act->pool_port == IP_NULL))
			act_disable_task_locked( thr_act );
		act_unlock_thread(thr_act);
		task_unlock( task );
	}

	thread_deallocate(thread); /* take caller's ref; 1 left for reaper */

	ipc_thread_terminate(thread);

	s = splsched();
	thread_lock(thread);
	thread->state |= (TH_HALTED|TH_TERMINATE);
	assert((thread->state & TH_UNINT) == 0);
#if 0 /* CHECKME! */
	/*
	 * Since thread has been put in the reaper_queue, it must no longer
	 * be preempted (otherwise, it could be put back in a run queue).
	 */
	thread->preempt = TH_NOT_PREEMPTABLE;
#endif
	thread_mark_wait_locked(thread);
	thread_unlock(thread);
	/* splx(s); */

	ETAP_SET_REASON(thread, BLOCKED_ON_TERMINATION);
	thread_block((void (*)(void)) 0);
	panic("the zombie walks!");
	/*NOTREACHED*/
}


/*
 * Create a new thread in the specified activation (i.e. "populate" the
 * activation).  The activation can be either user or kernel, but it must
 * be brand-new: no thread, no pool_port, nobody else knows about it.
 * Doesn't start the thread running; use thread_setrun to start it.
 */
kern_return_t
thread_create_shuttle(
	thread_act_t			thr_act,
	sp_attributes_t			attributes,
	void					(*start_at)(void),
	thread_t				*new_thread)
{
	thread_t				new_shuttle;
	task_t					parent_task = thr_act->task;
	processor_set_t			pset;
	kern_return_t			result;
	sched_policy_t			*policy;
	sf_return_t				sfr;
	int						suspcnt;

	/*
	 *	Allocate a thread and initialize static fields
	 */
	new_shuttle = (thread_t)zalloc(thread_shuttle_zone);
	if (new_shuttle == THREAD_NULL)
		return (KERN_RESOURCE_SHORTAGE);

	*new_shuttle = thr_sh_template;

	/* Allocate space for scheduling information and attributes */
	/*** Think about integrating with shuttle structure someday ***/
	new_shuttle->sp_info = (sp_info_t)kalloc(max_sched_info_size);
	if (new_shuttle->sp_info == SP_INFO_NULL) {
		zfree(thread_shuttle_zone, (vm_offset_t)new_shuttle);
		return (KERN_RESOURCE_SHORTAGE);
	}
	new_shuttle->pending_sched_attr =
					(sp_attributes_t)kalloc(max_sched_attributes_size);
	if (new_shuttle->pending_sched_attr == SP_ATTRIBUTES_NULL) {
		kfree((vm_offset_t)new_shuttle->sp_info,
			  				(vm_size_t)max_sched_info_size);
		zfree(thread_shuttle_zone, (vm_offset_t)new_shuttle);
		return (KERN_RESOURCE_SHORTAGE);
	}

	thread_lock_init(new_shuttle);
	rpc_lock_init(new_shuttle);
	wake_lock_init(new_shuttle);
	new_shuttle->sleep_stamp = sched_tick;

	/*
	 * No need to lock thr_act, since it can't be known to anyone --
	 * we set its suspend_count to one more than the task suspend_count
	 * by calling thread_hold.
	 */
	thr_act->user_stop_count = 1;
	for (suspcnt = thr_act->task->suspend_count + 1; suspcnt; --suspcnt)
		thread_hold(thr_act);

	simple_lock_init(&new_shuttle->lock, ETAP_THREAD_NEW);

	/*
	 * Initialize system-dependent part.
	 */
	result = thread_machine_create(new_shuttle, thr_act, start_at);
	if (result != KERN_SUCCESS) {
		kfree((vm_offset_t)new_shuttle->pending_sched_attr,
							(vm_size_t)max_sched_attributes_size);
		kfree((vm_offset_t)new_shuttle->sp_info,
							(vm_size_t)max_sched_info_size);
		zfree(thread_shuttle_zone, (vm_offset_t)new_shuttle);
		return (result);
	}

	/* Attach the thread to the activation.  */
	assert(!thr_act->thread);
	assert(!thr_act->pool_port);
	/* Synchronize with act_lock_thread() et al. */
	act_lock(thr_act);
	/* Thread holds a ref to the thr_act */
	act_locked_act_reference(thr_act);
	act_attach(thr_act, new_shuttle, 0);
	act_unlock(thr_act);

	/*
	 *	Initialize runtime-dependent fields
	 */
	thread_timer_setup(new_shuttle);
	machine_kernel_stack_init(new_shuttle, (void (*)(void))thread_continue);
	ipc_thread_init(new_shuttle);
	thread_start(new_shuttle, start_at);

	pset = parent_task->processor_set;
	if (!pset->active) {
		pset = &default_pset;
	}
	pset_lock(pset);

	task_lock(parent_task);

	if (attributes == SP_ATTRIBUTES_NULL)
		attributes = parent_task->sp_attributes;

	/* Associate the thread with that scheduling policy */
	new_shuttle->policy = attributes->policy_id;
	policy = &sched_policy[new_shuttle->policy];
	sfr = policy->sp_ops.sp_thread_attach(policy, new_shuttle);
	if (sfr != SF_SUCCESS)
		panic("thread_create_shuttle: sp_thread_attach");

	/* Indicate that no change in scheduling policy is pending */
	new_shuttle->pending_policy = POLICY_NULL;

	/* Associate the thread with the processor set */
	sfr = policy->sp_ops.sp_thread_processor_set(policy, new_shuttle, pset);
	if (sfr != SF_SUCCESS)
		panic("thread_create_shuttle: sp_thread_proceessor_set");

	/* Set the thread's scheduling parameters */
	sfr = policy->sp_ops.sp_thread_set(policy, new_shuttle, attributes);
	if (sfr != SF_SUCCESS)
		panic("thread_create_shuttle: sp_thread_set");

	/***
	 *** ??? Have to do something here.  I want the call above to
	 *** send the parameters to the policy and have the policy do
	 *** its thing.  Unfortunately, I can't see the "artful" way
	 *** to use the current MK SP routines to do this without
	 *** alteration.
	 ***
	 *** I must return to this.
	 ***
	 *** Perhaps a state bit should be associated with each thread
	 *** under the MK SP indicating whether that thread is runnable.
	 *** If it is not, `compute_priority()' or one of its siblings
	 *** is used to adjust scheduling parameter values; if it is
	 *** runnable, then the scheduling parameter adjustments can be
	 *** followed by code that tries to place the thread on the
	 *** appropriate run queue, or tries to run it immediately.
	 ***/

#if	ETAP_EVENT_MONITOR
	new_thread->etap_reason = 0;
	new_thread->etap_trace  = FALSE;
#endif	/* ETAP_EVENT_MONITOR */

	new_shuttle->active = TRUE;

	/*
	 *	Don't need to initialize because the context switch
	 *	code will set it before it can be used.
	 */
	if (!parent_task->active) {
		task_unlock(parent_task);
		pset_unlock(pset);
		thread_deallocate(new_shuttle);
		/* Drop ref we'd have given caller */
		thread_deallocate(new_shuttle);

		return (KERN_FAILURE);
	}

	task_unlock(parent_task);
	pset_unlock(pset);

	*new_thread = new_shuttle;

	{
	  long dbg_arg1, dbg_arg2, dbg_arg3, dbg_arg4;

	  KERNEL_DEBUG_CONSTANT((TRACEDBG_CODE(DBG_TRACE_DATA, 1)) | DBG_FUNC_NONE,
				new_shuttle, 0,0,0,0);

	  kdbg_trace_string(parent_task->bsd_info, &dbg_arg1, &dbg_arg2, &dbg_arg3, 
			    &dbg_arg4);
          KERNEL_DEBUG_CONSTANT((TRACEDBG_CODE(DBG_TRACE_STRING, 1)) | DBG_FUNC_NONE,
				dbg_arg1, dbg_arg2, dbg_arg3, dbg_arg4, 0);
	}

	return (KERN_SUCCESS);
}

kern_return_t
thread_create(
	task_t				task,
	thread_act_t		*new_act)
{
	thread_act_t		thr_act;
	thread_t			thread;
	kern_return_t		result;
	sched_policy_t		*policy;
	sf_return_t			sfr;
	spl_t				s;
	extern void			thread_bootstrap_return(void);

	result = act_create(task, NULL_PARAMS, &thr_act);
	if (result != KERN_SUCCESS)
		return (result);

	result = thread_create_shuttle(thr_act, SP_ATTRIBUTES_NULL,
										thread_bootstrap_return, &thread);
	if (result != KERN_SUCCESS) {
		thread_terminate(thr_act);
		act_deallocate(thr_act);
		return (result);
	}

	if (task->kernel_loaded)
		thread_user_to_kernel(thread);

	/* Start the thread running (it will immediately suspend itself).  */
	s = splsched();
	thread_ast_set(thr_act, AST_APC);
	thread_lock(thread);
	thread->state |= TH_RUN;	/*** ??? I think this is okay ***/

	/* Allow the thread to execute */
	policy = &sched_policy[thread->policy];
	sfr = policy->sp_ops.sp_thread_dispatch(policy, thread);
	if (sfr != SF_SUCCESS)
		panic("thread_create: sp_thread_dispatch");

	thread_unlock(thread);
	splx(s);
	
	/*****
	act_lock_thread(thr_act);
	thread_dowait( thr_act, FALSE);
	act_unlock_thread(thr_act);
	*****/

	*new_act = thr_act;

	return (KERN_SUCCESS);
}

/*
 * Update thread that belongs to a task created via kernel_task_create().
 */
void
thread_user_to_kernel(
	thread_t		thread)
{
	/*
	 * Used to set special swap_func here...
	 */
}

kern_return_t
thread_create_running(
	register task_t         parent_task,
	int                     flavor,
	thread_state_t          new_state,
	mach_msg_type_number_t  new_state_count,
	thread_act_t			*child_act)		/* OUT */
{
	register kern_return_t  result;

	result = thread_create(parent_task, child_act);
	if (result != KERN_SUCCESS)
		return (result);

	result = act_set_state(*child_act, flavor, new_state, new_state_count);
	if (result != KERN_SUCCESS) {
		(void) thread_terminate(*child_act);
		return (result);
	}

	result = thread_resume(*child_act);
	if (result != KERN_SUCCESS) {
		(void) thread_terminate(*child_act);
		return (result);
	}

	return (result);
}

/*
 *	kernel_thread:
 *
 *	Create and kernel thread in the specified task, and
 *	optionally start it running.
 */
thread_t
kernel_thread_with_attributes(
	task_t				task,
	sp_attributes_t		attributes,
	void				(*start_at)(void),
	boolean_t			start_running)
{
	kern_return_t		result;
	thread_t			thread;
	thread_act_t		thr_act;
	sched_policy_t		*policy;
	sf_return_t			sfr;
	spl_t				s;

	result = act_create(task, NULL_PARAMS, &thr_act);
	if (result != KERN_SUCCESS) {
		printf("kernel_thread act_create %x\n", result);
		panic("act_create failure");
	}

	result = thread_create_shuttle(thr_act, attributes, start_at, &thread);
	if (result != KERN_SUCCESS) {
		printf("kernel_thread create_shuttle %x\n", result);
		panic("create_shuttle failure");
	}

	thread_swappable(thr_act, FALSE);

	s = splsched();
	thread_lock(thread);

	thr_act = thread->top_act;
#if	MACH_LDEBUG
	thread->kthread = TRUE;
#endif	/* MACH_LDEBUG */

	if (start_running) {
		policy = &sched_policy[thread->policy];
		thread->state |= TH_RUN;
		sfr = policy->sp_ops.sp_thread_unblock(policy, thread);
		if (sfr != SF_SUCCESS)
			panic("kernel_thread: sp_thread_unblock");
	}

	thread_unlock(thread);
	splx(s);

	act_deallocate(thr_act);

	if (start_running)
		thread_resume(thr_act);

	return (thread);
}

thread_t
kernel_thread(
	task_t			task,
	void			(*start_at)(void))
{
	return kernel_thread_with_attributes(
						task, SP_ATTRIBUTES_NULL, start_at, TRUE);
}

unsigned int c_weird_pset_ref_exit = 0;	/* pset code raced us */

void
thread_deallocate(
	thread_t			thread)
{
	task_t				task;
	processor_set_t		pset;
	sched_policy_t		*policy;
	sf_return_t			sfr;
	spl_t				s;

	if (thread == THREAD_NULL)
		return;

	/*
	 *	First, check for new count > 0 (the common case).
	 *	Only the thread needs to be locked.
	 */
	s = splsched();
	thread_lock(thread);
	if (--thread->ref_count > 0) {
		thread_unlock(thread);
		splx(s);
		return;
	}

	/*
	 *	Count is zero.  However, the processor set's
	 *	thread list has an implicit reference to
	 *	the thread, and may make new ones.  Its lock also
	 *	dominate the thread lock.  To check for this, we
	 *	temporarily restore the one thread reference, unlock
	 *	the thread, and then lock the pset in the proper order.
	 */
	assert(thread->ref_count == 0); /* Else this is an extra dealloc! */
	thread->ref_count++;
	thread_unlock(thread);
	splx(s);

#if	MACH_HOST
	thread_freeze(thread);
#endif	/* MACH_HOST */

	pset = thread->processor_set;
	pset_lock(pset);

	s = splsched();
	thread_lock(thread);

	if (--thread->ref_count > 0) {
#if	MACH_HOST
		boolean_t need_wakeup = FALSE;
		/*
		 *	processor_set made extra reference.
		 */
		/* Inline the unfreeze */
		thread->may_assign = TRUE;
		if (thread->assign_active) {
			need_wakeup = TRUE;
			thread->assign_active = FALSE;
		}
#endif	/* MACH_HOST */
		thread_unlock(thread);
		splx(s);
		pset_unlock(pset);
#if	MACH_HOST
		if (need_wakeup)
			thread_wakeup((event_t)&thread->assign_active);
#endif	/* MACH_HOST */
		c_weird_pset_ref_exit++;
		return;
	}
#if	MACH_HOST
	assert(thread->assign_active == FALSE);
#endif	/* MACH_HOST */

	/*
	 *	Thread has no references - we can remove it.
	 */

	/*
	 *	A quick sanity check
	 */
	if (thread == current_thread())
	    panic("thread deallocating itself");

	/* Detach thread (shuttle) from its sched policy */
	policy = &sched_policy[thread->policy];
	sfr = policy->sp_ops.sp_thread_detach(policy, thread);
	if (sfr != SF_SUCCESS)
		panic("thread_deallocate: sp_thread_detach");

	/* Release storage used for scheduling info and attributes */
	assert(thread->pending_sched_attr != SP_ATTRIBUTES_NULL);
	kfree((vm_offset_t) thread->pending_sched_attr,
	      (vm_size_t) max_sched_attributes_size);
	assert(thread->sp_info != SP_INFO_NULL);
	kfree((vm_offset_t) thread->sp_info,
	      (vm_size_t) max_sched_info_size);

	pset_remove_thread(pset, thread);

	thread_unlock(thread);		/* no more references - safe */
	splx(s);
	pset_unlock(pset);

	pset_deallocate(thread->processor_set);

	/* frees kernel stack & other MD resources */
	thread->stack_privilege = 0;
	thread_machine_destroy(thread);

	zfree(thread_shuttle_zone, (vm_offset_t) thread);
}

void
thread_reference(
	thread_t	thread)
{
	spl_t		s;

	if (thread == THREAD_NULL)
		return;

	s = splsched();
	thread_lock(thread);
	thread->ref_count++;
	thread_unlock(thread);
	splx(s);
}

/*
 * Called with "appropriate" thread-related locks held on
 * thread and its top_act for synchrony with RPC (see
 * act_lock_thread()).
 */
kern_return_t
thread_info_shuttle(
	register thread_act_t	thr_act,
	thread_flavor_t			flavor,
	thread_info_t			thread_info_out,	/* ptr to OUT array */
	mach_msg_type_number_t	*thread_info_count)	/*IN/OUT*/
{
	register thread_t		thread = thr_act->thread;
	int						state, flags;
	spl_t					s;

	if (thread == THREAD_NULL)
		return (KERN_INVALID_ARGUMENT);

	if (flavor == THREAD_BASIC_INFO) {
	    register thread_basic_info_t	basic_info;

	    if (*thread_info_count < THREAD_BASIC_INFO_COUNT)
			return (KERN_INVALID_ARGUMENT);

	    basic_info = (thread_basic_info_t) thread_info_out;

	    s = splsched();
	    thread_lock(thread);

	    /* fill in info */

	    thread_read_times(thread, &basic_info->user_time,
									&basic_info->system_time);

	    /*** ??? fix me ***/
	    if (thread->policy & (POLICY_TIMESHARE|POLICY_RR|POLICY_FIFO)) {
			mk_sp_info_t	sp_info = (mk_sp_info_t)thread->sp_info;

			/*
			 *	Update lazy-evaluated scheduler info because someone wants it.
			 */
		    assert(sp_info != SP_INFO_NULL);
			if (sp_info->sched_stamp != sched_tick)
				update_priority(thread);

			basic_info->sleep_time = 0;

			/*
			 *	To calculate cpu_usage, first correct for timer rate,
			 *	then for 5/8 ageing.  The correction factor [3/5] is
			 *	(1/(5/8) - 1).
			 */
			basic_info->cpu_usage = sp_info->cpu_usage /
											(TIMER_RATE / TH_USAGE_SCALE);
			basic_info->cpu_usage = (basic_info->cpu_usage * 3) / 5;
#if	SIMPLE_CLOCK
			/*
			 *	Clock drift compensation.
			 */
			basic_info->cpu_usage =
					(basic_info->cpu_usage * 1000000) / sched_usec;
#endif	/* SIMPLE_CLOCK */
	    }
		else
			basic_info->sleep_time = basic_info->cpu_usage = 0;

	    basic_info->policy	= thread->policy;

	    flags = 0;
	    if (thread->state & TH_SWAPPED_OUT)
			flags = TH_FLAGS_SWAPPED;
	    else
		if (thread->state & TH_IDLE)
			flags = TH_FLAGS_IDLE;

	    state = 0;
	    if (thread->state & TH_HALTED)
			state = TH_STATE_HALTED;
	    else
		if (thread->state & TH_RUN)
			state = TH_STATE_RUNNING;
	    else
		if (thread->state & TH_UNINT)
			state = TH_STATE_UNINTERRUPTIBLE;
	    else
		if (thread->state & TH_SUSP)
			state = TH_STATE_STOPPED;
	    else
		if (thread->state & TH_WAIT)
			state = TH_STATE_WAITING;

	    basic_info->run_state = state;
	    basic_info->flags = flags;

	    basic_info->suspend_count = thr_act->user_stop_count;

	    thread_unlock(thread);
	    splx(s);

	    *thread_info_count = THREAD_BASIC_INFO_COUNT;

	    return (KERN_SUCCESS);
	}
	else
	if (flavor == THREAD_SCHED_TIMESHARE_INFO) {
		policy_timeshare_info_t		ts_info;
		mk_sp_info_t				sp_info = (mk_sp_info_t)thread->sp_info;

		if (*thread_info_count < POLICY_TIMESHARE_INFO_COUNT)
			return (KERN_INVALID_ARGUMENT);

		ts_info = (policy_timeshare_info_t)thread_info_out;

	    s = splsched();
		thread_lock(thread);

	    if (thread->policy != POLICY_TIMESHARE) {
	    	thread_unlock(thread);
			splx(s);

			return (KERN_INVALID_POLICY);
	    }

	    /*** ??? fix me ***/
	    assert(sp_info != SP_INFO_NULL);
		ts_info->base_priority = sp_info->priority;
		ts_info->max_priority =	sp_info->max_priority;
		ts_info->cur_priority = thread->sched_pri;

		ts_info->depressed = (sp_info->depress_priority >= 0);
		ts_info->depress_priority = sp_info->depress_priority;

		thread_unlock(thread);
	    splx(s);

		*thread_info_count = POLICY_TIMESHARE_INFO_COUNT;

		return (KERN_SUCCESS);	
	}
	else
	if (flavor == THREAD_SCHED_FIFO_INFO) {
		policy_fifo_info_t			fifo_info;
		mk_sp_info_t				sp_info = (mk_sp_info_t)thread->sp_info;

		if (*thread_info_count < POLICY_FIFO_INFO_COUNT)
			return (KERN_INVALID_ARGUMENT);

		fifo_info = (policy_fifo_info_t)thread_info_out;

	    s = splsched();
		thread_lock(thread);

	    if (thread->policy != POLICY_FIFO) {
	    	thread_unlock(thread);
			splx(s);

			return (KERN_INVALID_POLICY);
	    }

	    /*** ??? fix me ***/
	    assert(sp_info != SP_INFO_NULL);
		fifo_info->base_priority = sp_info->priority;
		fifo_info->max_priority = sp_info->max_priority;

		fifo_info->depressed = (sp_info->depress_priority >= 0);
		fifo_info->depress_priority = sp_info->depress_priority;

		thread_unlock(thread);
	    splx(s);

		*thread_info_count = POLICY_FIFO_INFO_COUNT;

		return (KERN_SUCCESS);	
	}
	else
	if (flavor == THREAD_SCHED_RR_INFO) {
		policy_rr_info_t			rr_info;
		mk_sp_info_t				sp_info = (mk_sp_info_t)thread->sp_info;

		if (*thread_info_count < POLICY_RR_INFO_COUNT)
			return (KERN_INVALID_ARGUMENT);

		rr_info = (policy_rr_info_t) thread_info_out;

	    s = splsched();
		thread_lock(thread);

	    if (thread->policy != POLICY_RR) {
	    	thread_unlock(thread);
			splx(s);

			return (KERN_INVALID_POLICY);
	    }

	    /*** ??? fix me ***/
	    assert(sp_info != SP_INFO_NULL);
		rr_info->base_priority = sp_info->priority;
		rr_info->max_priority = sp_info->max_priority;

	    rr_info->quantum = (sp_info->sched_data * tick) / 1000;

		rr_info->depressed = (sp_info->depress_priority >= 0);
		rr_info->depress_priority = sp_info->depress_priority;

		thread_unlock(thread);
	    splx(s);

		*thread_info_count = POLICY_RR_INFO_COUNT;

		return (KERN_SUCCESS);	
	}

	return (KERN_INVALID_ARGUMENT);
}

void
thread_doreap(
	register thread_t	thread)
{
	thread_act_t		thr_act;
	struct ipc_port		*pool_port;


	thr_act = thread_lock_act(thread);
	assert(thr_act && thr_act->thread == thread);

	act_locked_act_reference(thr_act);
	pool_port = thr_act->pool_port;

	/*
	 * Replace `act_unlock_thread()' with individual
	 * calls.  (`act_detach()' can change fields used
	 * to determine which locks are held, confusing
	 * `act_unlock_thread()'.)
	 */
	rpc_unlock(thread);
	if (pool_port != IP_NULL)
		ip_unlock(pool_port);
	act_unlock(thr_act);

	/* Remove the reference held by a rooted thread */
	if (pool_port == IP_NULL)
		act_deallocate(thr_act);

	/* Remove the reference held by the thread: */
	act_deallocate(thr_act);
}

static thread_call_data_t	thread_reaper_call_data;

/*
 *	reaper_thread:
 *
 *	This kernel thread runs forever looking for threads to destroy
 *	(when they request that they be destroyed, of course).
 *
 *	The reaper thread will disappear in the next revision of thread
 *	control when it's function will be moved into thread_dispatch.
 */
void
thread_reaper(void)
{
	register thread_t	thread;
	spl_t				s;

	s = splsched();
	simple_lock(&reaper_lock);

	if (thread_reaper_call == NULL) {
		thread_call_setup(&thread_reaper_call_data,	thread_reaper, NULL);
		thread_reaper_call = &thread_reaper_call_data;
	}

	while ((thread = (thread_t) dequeue_head(&reaper_queue)) != THREAD_NULL) {
		simple_unlock(&reaper_lock);

		/*
		 * wait for run bit to clear
		 */
		thread_lock(thread);
		if (thread->state & TH_RUN)
			panic("thread reaper: TH_RUN");
		thread_unlock(thread);
		splx(s);

		thread_doreap(thread);

		s = splsched();
		simple_lock(&reaper_lock);
	}

	simple_unlock(&reaper_lock);
	splx(s);
}

#if	MACH_HOST
/*
 *	thread_assign:
 *
 *	Change processor set assignment.
 *	Caller must hold an extra reference to the thread (if this is
 *	called directly from the ipc interface, this is an operation
 *	in progress reference).  Caller must hold no locks -- this may block.
 */

kern_return_t
thread_assign(
	thread_act_t	thr_act,
	processor_set_t	new_pset)
{
	thread_t	thread;

	if (thr_act == THR_ACT_NULL || new_pset == PROCESSOR_SET_NULL)
		return(KERN_INVALID_ARGUMENT);
	thread = act_lock_thread(thr_act);
	if (thread == THREAD_NULL) {
		act_unlock_thread(thr_act);
		return(KERN_INVALID_ARGUMENT);
	}

	thread_freeze(thread);
	thread_doassign(thread, new_pset, TRUE);
	act_unlock_thread(thr_act);
	return(KERN_SUCCESS);
}

/*
 *	thread_freeze:
 *
 *	Freeze thread's assignment.  Prelude to assigning thread.
 *	Only one freeze may be held per thread.  
 */
void
thread_freeze(
	thread_t	thread)
{
	spl_t	s;

	/*
	 *	Freeze the assignment, deferring to a prior freeze.
	 */

	s = splsched();
	thread_lock(thread);
	while (thread->may_assign == FALSE) {
		thread->assign_active = TRUE;
		thread_sleep_simple_lock((event_t) &thread->assign_active,
					simple_lock_addr(thread->lock), TRUE);
		thread_lock(thread);
	}
	thread->may_assign = FALSE;
	thread_unlock(thread);
	splx(s);

}

/*
 *	thread_unfreeze: release freeze on thread's assignment.
 */
void
thread_unfreeze(
	thread_t	thread)
{
	spl_t	s;

	s = splsched();
	thread_lock(thread);
	thread->may_assign = TRUE;
	if (thread->assign_active) {
		thread->assign_active = FALSE;
		thread_unlock(thread);
		splx(s);
		thread_wakeup((event_t)&thread->assign_active);
		return;
	}
	thread_unlock(thread);
	splx(s);
}

/*
 *	thread_doassign:
 *
 *	Actually do thread assignment.  thread_will_assign must have been
 *	called on the thread.  release_freeze argument indicates whether
 *	to release freeze on thread.
 *
 *	Called with "appropriate" thread-related locks held on thread (see
 *	act_lock_thread()).  Returns with thread unlocked.
 */

void
thread_doassign(
	register thread_t		thread,
	register processor_set_t	new_pset,
	boolean_t			release_freeze)
{
	register boolean_t		old_empty, new_empty;
	register processor_set_t	pset;
	boolean_t			recompute_pri = FALSE;
	int				max_priority;
	spl_t				s;
	thread_act_t			thr_act = thread->top_act;
	
	/*
	 *	Check for silly no-op.
	 */
	pset = thread->processor_set;
	if (pset == new_pset) {
		if (release_freeze)
			thread_unfreeze(thread);
		return;
	}
	/*
	 *	Suspend the thread and stop it if it's not the current thread.
	 */
	thread_hold(thr_act);
	act_locked_act_reference(thr_act);
	act_unlock_thread(thr_act);
	if (thread != current_thread()) {
		if (thread_stop_wait(thread) == FALSE ){
			(void)act_lock_thread(thr_act);
			thread_release(thr_act);
			act_locked_act_deallocate(thr_act);
			act_unlock_thread(thr_act);
			if (release_freeze )
				thread_unfreeze(thread);
			return;
		}
	}
	/*
	 *	Had to release thread-related locks before acquiring pset
	 *	locks.
	 */

	/*
	 *	Lock both psets now, use ordering to avoid deadlocks.
	 */
Restart:
	if (pset < new_pset) {
	    pset_lock(pset);
	    pset_lock(new_pset);
	} else {
	    pset_lock(new_pset);
	    pset_lock(pset);
	}

	/*
	 *	Check if new_pset is ok to assign to.  If not, reassign
	 *	to default_pset.
	 */
	if (!new_pset->active) {
	    pset_unlock(pset);
	    pset_unlock(new_pset);
	    new_pset = &default_pset;
	    goto Restart;
	}

	/*
	 *	Grab the thread lock and move the thread.
	 *	Then drop the lock on the old pset and the thread's
	 *	reference to it.
	 */

	s = splsched();
	thread_lock(thread);

	thread_change_psets(thread, pset, new_pset);

	old_empty = pset->empty;
	new_empty = new_pset->empty;

	pset_unlock(pset);
	pset_deallocate(pset);

        /*
	 *	Reset policy and priorities if needed. 
 	 *
	 *	There are three rules for threads under assignment:
	 *
         *      (1) If the new pset has the old policy enabled, keep the
         *          old policy. Otherwise, use the default policy for the pset.
         *      (2) The new limits will be the pset limits for the new policy.
         *      (3) The new base will be the same as the old base unless either
         *              (a) the new policy changed to the pset default policy;
         *                  in this case, the new base is the default policy
         *                  base,
         *          or
         *              (b) the new limits are different from the old limits;
         *                  in this case, the new base is the new limits.
         */
	/*** ??? fix me to fit into MK Scheduling Framework ***/
	max_priority = pset_max_priority(new_pset, thread->policy);
	if ((thread->policy & new_pset->policies) == 0) {
		thread->policy = new_pset->policy_default;
		thread->sched_data =
			pset_sched_data(new_pset, thread->policy);
		thread->unconsumed_quantum = thread->sched_data;
		thread->priority =
			pset_base_priority(new_pset, thread->policy);
		max_priority = pset_max_priority(new_pset, thread->policy);
		recompute_pri = TRUE;
	} 
	else if (thread->max_priority != max_priority) {
		thread->priority = max_priority;
                recompute_pri = TRUE;
	}

	thread->max_priority = max_priority;
	if ((thread->depress_priority >= 0) &&
		(thread->depress_priority > thread->max_priority)) {
                        thread->depress_priority = thread->max_priority;
	}

	pset_unlock(new_pset);

	if (recompute_pri)
		compute_priority(thread, TRUE);

	if (release_freeze) {
		boolean_t need_wakeup = FALSE;
		thread->may_assign = TRUE;
		if (thread->assign_active) {
			thread->assign_active = FALSE;
			need_wakeup = TRUE;
		}
		thread_unlock(thread);
		splx(s);
		if (need_wakeup)
			thread_wakeup((event_t)&thread->assign_active);
	} else {
		thread_unlock(thread);
		splx(s);
	}
	if (thread != current_thread())
		thread_unstop(thread);
	/*
	 *	Figure out hold status of thread.  Threads assigned to empty
	 *	psets must be held.  Therefore:
	 *		If old pset was empty release its hold.
	 *		Release our hold from above unless new pset is empty.
	 */

	(void)act_lock_thread(thr_act);
	if (old_empty)
		thread_release(thr_act);
	if (!new_empty)
		thread_release(thr_act);
	act_locked_act_deallocate(thr_act);
	act_unlock_thread(thr_act);

	/*
	 *	If current_thread is assigned, context switch to force
	 *	assignment to happen.  This also causes hold to take
	 *	effect if the new pset is empty.
	 */
	if (thread == current_thread()) {
		s = splsched();
		mp_disable_preemption();
		ast_on(AST_BLOCK);
		mp_enable_preemption();
		splx(s);
	}
}

#else	/* MACH_HOST */

kern_return_t
thread_assign(
	thread_act_t	thr_act,
	processor_set_t	new_pset)
{
#ifdef	lint
	thread++; new_pset++;
#endif	/* lint */
	return(KERN_FAILURE);
}
#endif	/* MACH_HOST */

/*
 *	thread_assign_default:
 *
 *	Special version of thread_assign for assigning threads to default
 *	processor set.
 */
kern_return_t
thread_assign_default(
	thread_act_t	thr_act)
{
	return (thread_assign(thr_act, &default_pset));
}

/*
 *	thread_get_assignment
 *
 *	Return current assignment for this thread.
 */	    
kern_return_t
thread_get_assignment(
	thread_act_t	thr_act,
	processor_set_t	*pset)
{
	thread_t	thread;

	if (thr_act == THR_ACT_NULL)
		return(KERN_INVALID_ARGUMENT);
	thread = act_lock_thread(thr_act);
	if (thread == THREAD_NULL) {
		act_unlock_thread(thr_act);
		return(KERN_INVALID_ARGUMENT);
	}
	*pset = thread->processor_set;
	act_unlock_thread(thr_act);
	pset_reference(*pset);
	return(KERN_SUCCESS);
}

/*
 *	thread_wire:
 *
 *	Specify that the target thread must always be able
 *	to run and to allocate memory.
 */
kern_return_t
thread_wire(
	host_t		host,
	thread_act_t	thr_act,
	boolean_t	wired)
{
	spl_t		s;
	thread_t	thread;
	extern void vm_page_free_reserve(int pages);

	if (thr_act == THR_ACT_NULL || host == HOST_NULL)
		return (KERN_INVALID_ARGUMENT);
	thread = act_lock_thread(thr_act);
	if (thread ==THREAD_NULL) {
		act_unlock_thread(thr_act);
		return(KERN_INVALID_ARGUMENT);
	}

	/*
	 * This implementation only works for the current thread.
	 * See stack_privilege.
	 */
	if (thr_act != current_act())
	    return KERN_INVALID_ARGUMENT;

	s = splsched();
	thread_lock(thread);

	if (wired) {
	    if (thread->vm_privilege == FALSE) 
		    vm_page_free_reserve(1);	/* XXX */
	    thread->vm_privilege = TRUE;
	} else {
	    if (thread->vm_privilege == TRUE) 
		    vm_page_free_reserve(-1);	/* XXX */
	    thread->vm_privilege = FALSE;
	}

	thread_unlock(thread);
	splx(s);
	act_unlock_thread(thr_act);

	/*
	 * Make the thread unswappable.
	 */
	thread_swappable(thr_act, FALSE);

	return KERN_SUCCESS;
}

/*
 *	thread_collect_scan:
 *
 *	Attempt to free resources owned by threads.
 */

void
thread_collect_scan(void)
{
	/* This code runs very quickly! */
}

boolean_t thread_collect_allowed = TRUE;
unsigned thread_collect_last_tick = 0;
unsigned thread_collect_max_rate = 0;		/* in ticks */

/*
 *	consider_thread_collect:
 *
 *	Called by the pageout daemon when the system needs more free pages.
 */

void
consider_thread_collect(void)
{
	/*
	 *	By default, don't attempt thread collection more frequently
	 *	than once a second (one scheduler tick).
	 */

	if (thread_collect_max_rate == 0)
		thread_collect_max_rate = 2;		/* sched_tick is a 1 second resolution 2 here insures at least 1 second interval */

	if (thread_collect_allowed &&
	    (sched_tick >
	     (thread_collect_last_tick + thread_collect_max_rate))) {
		thread_collect_last_tick = sched_tick;
		thread_collect_scan();
	}
}

#if	MACH_DEBUG
#if	STACK_USAGE

vm_size_t
stack_usage(
	register vm_offset_t stack)
{
	int i;

	for (i = 0; i < KERNEL_STACK_SIZE/sizeof(unsigned int); i++)
	    if (((unsigned int *)stack)[i] != STACK_MARKER)
		break;

	return KERNEL_STACK_SIZE - i * sizeof(unsigned int);
}

/*
 *	Machine-dependent code should call stack_init
 *	before doing its own initialization of the stack.
 */

static void
stack_init(
	   register vm_offset_t stack,
	   unsigned int bytes)
{
	if (stack_check_usage) {
	    int i;

	    for (i = 0; i < bytes / sizeof(unsigned int); i++)
		((unsigned int *)stack)[i] = STACK_MARKER;
	}
}

/*
 *	Machine-dependent code should call stack_finalize
 *	before releasing the stack memory.
 */

void
stack_finalize(
	register vm_offset_t stack)
{
	if (stack_check_usage) {
	    vm_size_t used = stack_usage(stack);

	    simple_lock(&stack_usage_lock);
	    if (used > stack_max_usage)
		stack_max_usage = used;
	    simple_unlock(&stack_usage_lock);
	    if (used > stack_max_use) {
		printf("stack usage = %x\n", used);
		panic("stack overflow");
	    }
	}
}

#endif	/*STACK_USAGE*/
#endif /* MACH_DEBUG */

kern_return_t
host_stack_usage(
	host_t		host,
	vm_size_t	*reservedp,
	unsigned int	*totalp,
	vm_size_t	*spacep,
	vm_size_t	*residentp,
	vm_size_t	*maxusagep,
	vm_offset_t	*maxstackp)
{
#if !MACH_DEBUG
        return KERN_NOT_SUPPORTED;
#else
	unsigned int total;
	vm_size_t maxusage;

	if (host == HOST_NULL)
		return KERN_INVALID_HOST;

	simple_lock(&stack_usage_lock);
	maxusage = stack_max_usage;
	simple_unlock(&stack_usage_lock);

	stack_statistics(&total, &maxusage);

	*reservedp = 0;
	*totalp = total;
	*spacep = *residentp = total * round_page(KERNEL_STACK_SIZE);
	*maxusagep = maxusage;
	*maxstackp = 0;
	return KERN_SUCCESS;

#endif /* MACH_DEBUG */
}

/*
 * Return info on stack usage for threads in a specific processor set
 */
kern_return_t
processor_set_stack_usage(
	processor_set_t	pset,
	unsigned int	*totalp,
	vm_size_t	*spacep,
	vm_size_t	*residentp,
	vm_size_t	*maxusagep,
	vm_offset_t	*maxstackp)
{
#if !MACH_DEBUG
        return KERN_NOT_SUPPORTED;
#else
	unsigned int total;
	vm_size_t maxusage;
	vm_offset_t maxstack;

	register thread_t *threads;
	register thread_t thread;

	unsigned int actual;	/* this many things */
	unsigned int i;

	vm_size_t size, size_needed;
	vm_offset_t addr;

	if (pset == PROCESSOR_SET_NULL)
		return KERN_INVALID_ARGUMENT;

	size = 0; addr = 0;

	for (;;) {
		pset_lock(pset);
		if (!pset->active) {
			pset_unlock(pset);
			return KERN_INVALID_ARGUMENT;
		}

		actual = pset->thread_count;

		/* do we have the memory we need? */

		size_needed = actual * sizeof(thread_t);
		if (size_needed <= size)
			break;

		/* unlock the pset and allocate more memory */
		pset_unlock(pset);

		if (size != 0)
			kfree(addr, size);

		assert(size_needed > 0);
		size = size_needed;

		addr = kalloc(size);
		if (addr == 0)
			return KERN_RESOURCE_SHORTAGE;
	}

	/* OK, have memory and the processor_set is locked & active */

	threads = (thread_t *) addr;
	for (i = 0, thread = (thread_t) queue_first(&pset->threads);
	     i < actual;
	     i++,
	     thread = (thread_t) queue_next(&thread->pset_threads)) {
		thread_reference(thread);
		threads[i] = thread;
	}
	assert(queue_end(&pset->threads, (queue_entry_t) thread));

	/* can unlock processor set now that we have the thread refs */
	pset_unlock(pset);

	/* calculate maxusage and free thread references */

	total = 0;
	maxusage = 0;
	maxstack = 0;
	for (i = 0; i < actual; i++) {
		int cpu;
		thread_t thread = threads[i];
		vm_offset_t stack = 0;

		/*
		 *	thread->kernel_stack is only accurate if the
		 *	thread isn't swapped and is not executing.
		 *
		 *	Of course, we don't have the appropriate locks
		 *	for these shenanigans.
		 */

		stack = thread->kernel_stack;

		for (cpu = 0; cpu < NCPUS; cpu++)
			if (cpu_data[cpu].active_thread == thread) {
				stack = active_stacks[cpu];
				break;
			}

		if (stack != 0) {
			total++;

			if (stack_check_usage) {
				vm_size_t usage = stack_usage(stack);

				if (usage > maxusage) {
					maxusage = usage;
					maxstack = (vm_offset_t) thread;
				}
			}
		}

		thread_deallocate(thread);
	}

	if (size != 0)
		kfree(addr, size);

	*totalp = total;
	*residentp = *spacep = total * round_page(KERNEL_STACK_SIZE);
	*maxusagep = maxusage;
	*maxstackp = maxstack;
	return KERN_SUCCESS;

#endif	/* MACH_DEBUG */
}


/*
 * We consider a thread not preemptable if it is marked as either
 * suspended or waiting.
 */

boolean_t thread_not_preemptable(
	thread_t	thread)
{

	/* XXX - when scheduling framework and such is done, the
	   thread state check can be eliminated */

	if ((thread->state & (TH_WAIT|TH_SUSP)) || thread->preempt)
		return (TRUE);
	else
		return (FALSE);
}


/*
 * 	thread_set_sched
 *
 *      Set scheduling policy and parameters for the given thread.
 *      Policy must be a policy which is enabled for the
 *      processor set.
 *      (This should replace `thread_set_policy()' with the addition
 *      of the MK Scheduling Framework to the kernel.)
 */
kern_return_t
thread_set_sched(
	thread_act_t			thr_act,
	policy_t				policy_id,
	sched_attr_t			sched_attr,
	mach_msg_type_number_t	sched_attrCnt)
{
	thread_t				thread;
	processor_set_t			pset;
	kern_return_t			result = KERN_SUCCESS;
	sched_policy_t			*policy;
	sf_return_t				sfr;
	boolean_t				do_dispatch = FALSE;
	spl_t					s;

	if (thr_act == THR_ACT_NULL)
		return (KERN_INVALID_ARGUMENT);

	thread = act_lock_thread(thr_act);
	if (	thread == THREAD_NULL								||
			(sched_attrCnt * sizeof(int)) !=
				sched_policy[policy_id].sched_attributes_size		) {
		act_unlock_thread(thr_act);
		return(KERN_INVALID_ARGUMENT);
	}

	if (invalid_policy(policy_id)) {
		act_unlock_thread(thr_act);
		return(KERN_INVALID_POLICY);
	}

	/* coordinate changes to thread scheduling state with others */
	s = splsched();
	thread_lock(thread);

	/* see if thread's policy is to be changed */
	if (thread->policy != policy_id) {
		/* it is; see if the target is executing */
		if (thread != current_thread()) {	/*** fix for SMP ***/
			/* it is not; detach from old policy */
			policy = &sched_policy[thread->policy];
			sfr = policy->sp_ops.sp_thread_detach(policy, thread);
			if (sfr != SF_SUCCESS)
				panic("thread_set_sched: sp_thread_detach");

			/* attach to this policy */
			policy = &sched_policy[policy_id];
			sfr = policy->sp_ops.sp_thread_attach(policy, thread);
			if (sfr != SF_SUCCESS) {
				thread_unlock(thread);
				splx(s);
				act_unlock_thread(thr_act);
				return(KERN_FAILURE);
			}

			/* remember to dispatch thread after setting parms */
			do_dispatch = TRUE;
		}
		else {
			/* target is currently executing */
			/* defer policy change until thread leaves processor */
			thread->pending_policy = policy_id;
			bcopy((char *)sched_attr,
			      (char *)thread->pending_sched_attr,
			      (sched_attrCnt * sizeof(int)));

			/* try to get off processor to effect change */
			thread_unlock(thread);
			splx(s);
			act_unlock_thread(thr_act);
			thread_block((void (*)(void)) 0);

			/* by now, new attributes have been installed */
			act_lock_thread(thr_act);
			/*** check to see it's same thread? ***/

			s = splsched();
			thread_lock(thread);
			result = (thread->change_sfr == SF_SUCCESS)?
						KERN_SUCCESS : KERN_FAILURE;
			thread_unlock(thread);
			splx(s);
			act_unlock_thread(thr_act);
			return(result);
		}

	}

	/* call policy-specific routine to install new scheduling parameters */
	policy = &sched_policy[policy_id];
	sfr = policy->sp_ops.sp_thread_set(
						policy, thread, (sp_attributes_t)sched_attr);

	/* dispatch thread under new policy, if appropriate */
	if (do_dispatch == TRUE && sfr == SF_SUCCESS) {
		sfr = policy->sp_ops.sp_thread_dispatch(policy, thread);
	}

	thread_unlock(thread);
	splx(s);

	if (sfr != SF_SUCCESS)
		result = KERN_FAILURE;

	act_unlock_thread(thr_act);
	return(result);
}


/*
 * 	thread_get_sched
 *
 *      Get scheduling policy and parameters for the given thread.
 *      (This was added as part of the MK Scheduling Framework.)
 */
kern_return_t
thread_get_sched(
	thread_act_t			thr_act,
	policy_t				*policy_id_out,
	sched_attr_t			sched_attr,
	mach_msg_type_number_t	*sched_attrCnt)
{
	thread_t				thread;
	kern_return_t			result = KERN_SUCCESS;
	policy_t				policy_id;
	sched_policy_t			*policy;
	sf_return_t				sfr;
	int						size;
	spl_t					s;

	if (thr_act == THR_ACT_NULL)
		return (KERN_INVALID_ARGUMENT);

	thread = act_lock_thread(thr_act);
	if (thread == THREAD_NULL) {
		act_unlock_thread(thr_act);
		*policy_id_out = POLICY_NULL;
		return(KERN_INVALID_ARGUMENT);
	}

	size = *sched_attrCnt * sizeof(int);

	/* coordinate changes to thread scheduling state with others */
	s = splsched();
	thread_lock(thread);

	policy_id = thread->policy;
	if (size < sched_policy[policy_id].sched_attributes_size) {
		thread_unlock(thread);
		splx(s);
		act_unlock_thread(thr_act);
		*policy_id_out = policy_id;
		return(KERN_INVALID_ARGUMENT);
	}

	/* note policy for caller */
	*policy_id_out = policy_id;

	/* call policy-specific routine */
	policy = &sched_policy[policy_id];
	sfr = policy->sp_ops.sp_thread_get(
						policy, thread, (sp_attributes_t)sched_attr, size);

	if (sfr != SF_SUCCESS)
		result = KERN_FAILURE;

	*sched_attrCnt = policy->sched_attributes_size / sizeof(int);

	thread_unlock(thread);
	splx(s);
	act_unlock_thread(thr_act);
	return(result);
}

boolean_t
thread_get_funneled(
	void)
{
	return((current_thread()->funnel_state & TH_FN_OWNED) == TH_FN_OWNED);
}

boolean_t
thread_set_funneled(
	boolean_t	funneled)
{
	thread_t	cur_thread;
	boolean_t	funnel_state_prev;

	cur_thread = current_thread();
	funnel_state_prev = ((cur_thread->funnel_state & TH_FN_OWNED) == TH_FN_OWNED);

	if (funnel_state_prev != funneled) {
		if (funneled == TRUE) {
			mutex_lock(&funnel_lock);
			cur_thread->funnel_state |= TH_FN_OWNED;
		} else {
			cur_thread->funnel_state &= ~TH_FN_OWNED;
			mutex_unlock(&funnel_lock);
		}
	}
	return(funnel_state_prev);
}

void
thread_set_cont_arg(int arg)
{
  thread_t th = current_thread();
  th->cont_arg = arg; 
}

int
thread_get_cont_arg(void)
{
  thread_t th = current_thread();
  return(th->cont_arg); 
}

/*
 * Export routines to other components for things that are done as macros
 * within the osfmk component.
 */
#undef thread_should_halt
boolean_t
thread_should_halt(
	thread_shuttle_t th)
{
	return(thread_should_halt_fast(th));
}
