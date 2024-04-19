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
 * @OSF_COPYRIGHT@
 * 
 */

/***
 *** ??? The following lines were picked up when code was incorporated
 *** into this file from `kern/syscall_subr.c.'  These should be moved
 *** with the code if it moves again.  Otherwise, they should be trimmed,
 *** based on the files included above.
 ***/

#include <mach/boolean.h>
#include <mach/thread_switch.h>
#include <ipc/ipc_port.h>
#include <ipc/ipc_space.h>
#include <kern/counters.h>
#include <kern/ipc_kobject.h>
#include <kern/processor.h>
#include <kern/sched.h>
#include <kern/sched_prim.h>
#include <kern/spl.h>
#include <kern/task.h>
#include <kern/thread.h>
#include <kern/ast.h>
#include <mach/policy.h>

#include <kern/syscall_subr.h>
#include <mach/mach_host_server.h>
#include <mach/mach_syscalls.h>

/***
 *** ??? End of lines picked up when code was incorporated
 *** into this file from `kern/syscall_subr.c.'
 ***/

#include <kern/sf.h>
#include <kern/mk_sp.h>
#include <kern/misc_protos.h>
#include <kern/spl.h>
#include <kern/sched.h>
#include <kern/sched_prim.h>
#include <kern/assert.h>
#include <kern/thread.h>
#include <mach/mach_host_server.h>

/* Forwards */
void	_mk_sp_thread_depress_priority(
			sf_object_t			policy,
			mach_msg_timeout_t	depress_time);

/***
 *** ??? The next two files supply the prototypes for `thread_set_policy()'
 *** and `thread_policy.'  These routines cannot stay here if they are
 *** exported Mach system calls.
 ***/
#include <mach/thread_act_server.h>
#include <mach/host_priv_server.h>

/*
 * Vector containing standard scheduling policy operations
 */
sp_ops_t	mk_sp_ops = {
		    _mk_sp_init,
		    _mk_sp_enable_processor_set,
		    _mk_sp_disable_processor_set,
		    _mk_sp_enable_processor,
		    _mk_sp_disable_processor,
		    _mk_sp_thread_update_mpri,
		    _mk_sp_thread_unblock,
		    _mk_sp_thread_done,
		    _mk_sp_thread_begin,
		    _mk_sp_thread_dispatch,
		    _mk_sp_thread_attach,
		    _mk_sp_thread_detach,
		    _mk_sp_thread_processor,
		    _mk_sp_thread_processor_set,
		    _mk_sp_thread_set,
		    _mk_sp_thread_get,
		    _mk_sp_db_print_sched_stats,
		    _mk_sp_swtch_pri,
		    _mk_sp_thread_switch,
		    _mk_sp_thread_depress_abort,
		    _mk_sp_thread_depress_timeout,
		    _mk_sp_task_attach,
		    _mk_sp_task_detach,
		    _mk_sp_task_policy,
		    _mk_sp_task_set_policy,
		    _mk_sp_task_set_sched,
		    _mk_sp_task_get_sched,
		    _mk_sp_thread_runnable,
		    _mk_sp_alarm_expired
};

/* Forwards */
kern_return_t	thread_policy_common(
					thread_t		thread,
					int				policy,
					int				data,
					processor_set_t	pset);

void		_mk_sp_thread_dump(
					sched_thread_t	thread);

/*
 * Local variables
 */
#ifdef	MACH_ASSERT
boolean_t	mk_sp_first_thread = TRUE;
#endif	/* MACH_ASSERT */

/*
 * Counters to monitor policy-related events
 */
mach_counter_t	c_mk_sp_init = 0,
		c_mk_sp_enable_processor_set = 0,
		c_mk_sp_disable_processor_set = 0,
		c_mk_sp_enable_processor = 0,
		c_mk_sp_disable_processor = 0,
		c_mk_sp_thread_update_mpri = 0,
		c_mk_sp_thread_unblock = 0,
		c_mk_sp_thread_done = 0,
		c_mk_sp_thread_begin = 0,
		c_mk_sp_thread_dispatch = 0,
		c_mk_sp_thread_attach = 0,
		c_mk_sp_thread_detach = 0,
		c_mk_sp_thread_processor = 0,
		c_mk_sp_thread_processor_set = 0,
		c_mk_sp_thread_set = 0,
		c_mk_sp_thread_get = 0,
		c_mk_sp_db_print_sched_stats = 0,
		c_mk_sp_swtch_pri = 0,
		c_mk_sp_thread_switch = 0,
		c_mk_sp_thread_depress_abort = 0,
		c_mk_sp_thread_depress_timeout = 0,
		c_mk_sp_task_attach = 0,
		c_mk_sp_task_detach = 0,
		c_mk_sp_task_policy = 0,
		c_mk_sp_task_set_policy = 0,
		c_mk_sp_task_set_sched = 0,
		c_mk_sp_task_get_sched = 0,
		c_mk_sp_thread_runnable = 0,
		c_mk_sp_alarm_expired = 0;

/*
 * Table to convert computed priorities into allocated priority range.
 */
int	convert_pri[NRQS];

/*
 * Lowest (normal -- i.e., not idle and not depressed) priority
 */
int	lpri = -1;

natural_t	min_quantum_ms;

/*
 * Standard operations for MK Scheduling Policy
 */

sf_return_t
_mk_sp_init(
	sf_object_t			policy,
	int					policy_id,
	int					priority_mask_length,
	sf_priority_mask_t	priority_mask)
{
	int					i;
	int					first_metapri;
	int					last_metapri;

	counter(c_mk_sp_init++);

	/*
	 * Initialize fields in the structure
	 */
	policy->policy_id = policy_id;
	policy->priority_mask_length = priority_mask_length;
	policy->priority_mask = *priority_mask;
	/*** ??? should something more dynamic be done here for mask? ***/
	policy->sched_attributes_size = sizeof (mk_sp_attribute_struct_t);
	policy->sched_info_size = sizeof (mk_sp_info_struct_t);

	/*
	 * Initialize table that maps computed priority values into
	 * allocated metapriority values.
	 */

	/* Find first metapriority value allocated to this policy */
	first_metapri = MAXPRI - ffsbit(priority_mask->bitmap);
	/*** ??? how to test for no set bits? ***/

	/* Map computed priorities higher than this to first metapriority */
	for (i = MAXPRI; i > first_metapri; i--)
		convert_pri[i] = first_metapri;
 
	/* For each remaining computed priority value... */
	for (i = first_metapri; i >= 0; i--) {
		/* ... is it a metapriority value assigned to this policy ?*/
		if (testbit(MAXPRI - i, priority_mask->bitmap)) {
			/* Yes; just use it */
			convert_pri[i] = i;
			last_metapri = i;
		}
		else {
			/* No, bump it up to the last value assigned */
			convert_pri[i] = last_metapri;
		}
	}

	/* Remember lowest "normal" priority available to this policy */
	lpri = convert_pri[LPRI];

	min_quantum_ms = (1000 / hz) * min_quantum;

#if 0
	/* Initialize operation entries in "jump table" */
	/***
	 *** ??? These must be done elsewhere.  Otherwise, there is a
	 *** circularity: How does the framework find this init operation?
	 ***/
	policy->sp_ops = mk_sp_ops;

	policy->sp_ops.sp_enable_processor_set = sp_mk_enable_processor_set;
	policy->sp_ops.sp_disable_processor_set = sp_mk_disable_processor_set;
	policy->sp_ops.sp_enable_processor = sp_mk_enable_processor;
	policy->sp_ops.sp_disable_processor = sp_mk_disable_processor;
	policy->sp_ops.sp_thread_unblock = sp_mk_thread_unblock;
	policy->sp_ops.sp_thread_done = sp_mk_thread_done;
	policy->sp_ops.sp_thread_begin = sp_mk_thread_begin;
	policy->sp_ops.sp_thread_attach = sp_mk_thread_attach;
	policy->sp_ops.sp_thread_detach = sp_mk_thread_detach;
	policy->sp_ops.sp_thread_processor = sp_mk_thread_processor;
	policy->sp_ops.sp_thread_processor_set = sp_mk_thread_processor_set;
	policy->sp_ops.sp_thread_set = sp_mk_thread_set;
	policy->sp_ops.sp_thread_get = sp_mk_thread_get;
	policy->sp_ops.sp_db_print_sched_stats = sp_mk_db_print_sched_stats;
#endif

	return(SF_SUCCESS);
}

sf_return_t
_mk_sp_enable_processor_set(
	sf_object_t			policy,
	processor_set_t		processor_set)
{
	kern_return_t		result;

	counter(c_mk_sp_enable_processor_set++);

	result = processor_set_policy_enable(processor_set, policy->policy_id);
	assert(result == KERN_SUCCESS);

	return(SF_SUCCESS);
}

sf_return_t
_mk_sp_disable_processor_set(
	sf_object_t			policy,
	processor_set_t		processor_set)
{
	kern_return_t		result;

	counter(c_mk_sp_disable_processor_set++);

	result = processor_set_policy_disable(
								processor_set, policy->policy_id, TRUE);
	assert(result == KERN_SUCCESS);

	return(SF_SUCCESS);
}

sf_return_t
_mk_sp_enable_processor(
	sf_object_t		policy,
	processor_t		processor)
{
	counter(c_mk_sp_enable_processor++);

	panic("not yet implemented???");

	return(SF_FAILURE);
}

sf_return_t
_mk_sp_disable_processor(
	sf_object_t		policy,
	processor_t		processor)
{
	counter(c_mk_sp_disable_processor++);

	panic("not yet implemented???");

	return(SF_FAILURE);
}

sf_return_t
_mk_sp_thread_update_mpri(
	sf_object_t			policy,
	sched_thread_t		thread)
{
	mk_sp_info_t		sp_info;

	counter(c_mk_sp_thread_update_mpri++);

	/* get pointer to scheduling policy-specific data */
	assert(thread->sp_info != SP_INFO_NULL);
	sp_info = (mk_sp_info_t)thread->sp_info;

	if (sp_info->sched_stamp != sched_tick)
		update_priority(thread);

	return(SF_SUCCESS);
}

sf_return_t
_mk_sp_thread_unblock(
	sf_object_t			policy,
	sched_thread_t		thread)
{
	mk_sp_info_t		sp_info;

	counter(c_mk_sp_thread_unblock++);

	/* get pointer to scheduling policy-specific data */
	assert(thread->sp_info != SP_INFO_NULL);
	sp_info = (mk_sp_info_t)thread->sp_info;

	counter(sp_info->c_mk_sp_thread_unblock++);

	/*
	 * The following assert is removed because we're hacking the
	 * MK_SP_BLOCKED bit and removing it early to get SMP to work
	 * with this gosh darned scheduler.  The whole danged thing is lousy and 
	 * needs to be removed.  Hopefully for now, no one tries to
	 * unblock a really runnable guy - HACKALERT!!!
	 */		
				
	/* check for legal thread states */
#if 0
	assert(sp_info->th_state == MK_SP_BLOCKED ||
	       sp_info->th_state == MK_SP_ATTACHED); /* ??? work to remove*/
#endif
	/* indicate thread is now runnable */
	sp_info->th_state = MK_SP_RUNNABLE;

	/* place thread at end of appropriate run queue */
	thread_setrun(thread, TRUE, TAIL_Q);

	return(SF_SUCCESS);
}

sf_return_t
_mk_sp_thread_done(
	sf_object_t			policy,
	sched_thread_t		old_thread)
{
	processor_t			myprocessor = cpu_to_processor(cpu_number());
	mk_sp_info_t		sp_info;

	counter(c_mk_sp_thread_done++);

	/*
	 * A running thread is being taken off a processor:
	 *
	 *   - update the thread's `unconsumed_quantum' field
	 *   - update the thread's state field
	 */

	/* get pointer to scheduling policy-specific data */
	assert(old_thread->sp_info != SP_INFO_NULL);
	sp_info = (mk_sp_info_t)old_thread->sp_info;

	counter(sp_info->c_mk_sp_thread_done++);
	assert((sp_info->c_mk_sp_thread_done
		   == sp_info->c_mk_sp_thread_begin + 1) ||
	       (sp_info->c_mk_sp_thread_done
		   == sp_info->c_mk_sp_thread_begin));

	sp_info->unconsumed_quantum = myprocessor->quantum;

	/* check thread's current state */
	assert(mk_sp_first_thread || (sp_info->th_state & MK_SP_RUNNABLE));
#ifdef	MACH_ASSERT
	mk_sp_first_thread = FALSE;
#endif	/* MACH_ASSERT */

	/* update thread's state field based on reason for stopping now */
	/*** ??? is this condition correct? should it be more selective? ***/
	/*** ??? if ((old_thread->reason != AST_NONE) ||
	    (old_thread->wait_event != NO_EVENT)) { ***/
	if (old_thread->state & TH_WAIT) {
		sp_info->th_state = MK_SP_BLOCKED;
	}

	return(SF_SUCCESS);
}

sf_return_t
_mk_sp_thread_begin(
	sf_object_t			policy,
	sched_thread_t		thread)
{

	processor_t			myprocessor = cpu_to_processor(cpu_number());
	processor_set_t		pset;
	mk_sp_info_t		sp_info;

	counter(c_mk_sp_thread_begin++);

	pset = myprocessor->processor_set;
	/*
	 * The designated thread is about to begin execution:
	 *
	 *   - update the processor's `quantum' field
	 */

	/* get pointer to scheduling policy-specific data */
	assert(thread->sp_info != SP_INFO_NULL);
	sp_info = (mk_sp_info_t)thread->sp_info;

	counter(sp_info->c_mk_sp_thread_begin++);
	assert((sp_info->c_mk_sp_thread_done
		   == sp_info->c_mk_sp_thread_begin) ||
	       (sp_info->c_mk_sp_thread_done + 1
		   == sp_info->c_mk_sp_thread_begin));

	/* check for legal thread state */
	assert(sp_info->th_state == MK_SP_RUNNABLE);

	if (thread->policy & (POLICY_RR|POLICY_FIFO))
		myprocessor->quantum = sp_info->unconsumed_quantum;
	else
		myprocessor->quantum = (thread->bound_processor ?
										min_quantum : pset->set_quantum);

	return(SF_SUCCESS);
}

sf_return_t
_mk_sp_thread_dispatch(
	sf_object_t			policy,
	sched_thread_t		old_thread)
{
	mk_sp_info_t	sp_info;

	counter(c_mk_sp_thread_dispatch++);

	/* get pointer to scheduling policy-specific data */
	assert(old_thread->sp_info != SP_INFO_NULL);
	sp_info = (mk_sp_info_t)old_thread->sp_info;

	counter(sp_info->c_mk_sp_thread_dispatch++);

	if (sp_info->th_state & MK_SP_RUNNABLE) {
		if (old_thread->reason & AST_QUANTUM) {
			thread_setrun(old_thread, FALSE, TAIL_Q);
			sp_info->unconsumed_quantum = sp_info->sched_data;
		}
		else
			thread_setrun(old_thread, FALSE, HEAD_Q);
	}

	if (sp_info->th_state & MK_SP_ATTACHED) {
		/* indicate thread is now runnable */
		sp_info->th_state = MK_SP_RUNNABLE;

		/* place thread at end of appropriate run queue */
		thread_setrun(old_thread, FALSE, TAIL_Q);
	}

	return(SF_SUCCESS);
}

/*
 * Thread must already be locked.
 */
sf_return_t
_mk_sp_thread_attach(
	sf_object_t			policy,
	sched_thread_t		thread)
{
	mk_sp_info_t		sp_info;

	counter(c_mk_sp_thread_attach++);

	/* Make sure there is a place for policy-specific state */
	assert(thread->sp_info != SP_INFO_NULL);

	sp_info = (mk_sp_info_t)thread->sp_info;
	sp_info->th_state = MK_SP_ATTACHED;
	/*** ??? Do these need to be done here? ***/
	/*** ??? If so, could fill from template for speed -- maybe ***/
	sp_info->priority = MINPRI;		/*** ??? ***/
	sp_info->max_priority = BASEPRI_USER;
	/*** ??? sp_info->sched_pri =
			(later - compute_priority); (for now, at least) ***/
	sp_info->sched_data = 0;
	sp_info->policy = policy->policy_id;	/*** ??? ***/
	sp_info->depress_priority = -1;
	sp_info->cpu_usage = 0;
	sp_info->sched_usage = 0;
	sp_info->sched_stamp = 0;		/*** ??? ***/
	sp_info->unconsumed_quantum = 0;	/*** ??? ***/

#ifdef	MACH_ASSERT
	sp_info->c_mk_sp_thread_attach = 1;
	sp_info->c_mk_sp_thread_detach = 0;
	sp_info->c_mk_sp_thread_begin = 0;
	sp_info->c_mk_sp_thread_done = 0;
	sp_info->c_mk_sp_thread_dispatch = 0;
	sp_info->c_mk_sp_thread_unblock = 0;
	sp_info->c_mk_sp_thread_set = 0;
	sp_info->c_mk_sp_thread_get = 0;
#endif	/* MACH_ASSERT */

	/* Reflect this policy in thread data structure */
	thread->policy = policy->policy_id;

	return(SF_SUCCESS);
}

/*
 * Check to make sure that thread is removed from run
 * queues and active execution; and clean up the
 * policy-specific data structure for thread.
 *
 * Thread must already be locked.
 */
sf_return_t
_mk_sp_thread_detach(
	sf_object_t			policy,
	sched_thread_t		thread)
{
	mk_sp_info_t		sp_info;
	struct run_queue	*rq;

	assert(thread->policy == policy->policy_id);

	counter(c_mk_sp_thread_detach++);
	/* get pointer to scheduling policy-specific data */
	assert(thread->sp_info != SP_INFO_NULL);
	sp_info = (mk_sp_info_t)thread->sp_info;

	counter(sp_info->c_mk_sp_thread_detach++);

	/* make sure that the thread has stopped executing */
	/*** ??? look at machine's version of RUN bit, too? ***/
	/***
	 *** Fix for SMP case.
	 ***/

	/* make sure that the thread is no longer on any run queue */
	if (thread->runq != RUN_QUEUE_NULL) {
		rq = rem_runq(thread);
		if (rq == RUN_QUEUE_NULL) {
			panic("mk_sp_thread_detach: missed thread");
		}
	}

	/* (optionally) clear policy-specific data structure for the thread */

	if (sp_info->depress_priority >= 0) {
		sp_info->priority = sp_info->depress_priority;
		sp_info->depress_priority = -1;
		if (thread_call_cancel(&thread->depress_timer))
			thread_call_enter(&thread->depress_timer);
	}

	/* clear the thread's policy field */
	thread->policy = POLICY_NULL;

	return(SF_SUCCESS);
}

sf_return_t
_mk_sp_thread_processor(
	sf_object_t			policy,
	sched_thread_t		*thread,
	processor_t			processor)
{
	counter(c_mk_sp_thread_processor++);

	panic("not yet implemented???");

	return(SF_FAILURE);
}

sf_return_t
_mk_sp_thread_processor_set(
	sf_object_t			policy,
	sched_thread_t		thread,
	processor_set_t		processor_set)
{
	counter(c_mk_sp_thread_processor_set++);

	pset_add_thread(processor_set, thread);

	return(SF_SUCCESS);
}

sf_return_t
_mk_sp_thread_set(
	sf_object_t			policy,
	sched_thread_t		thread,
	sp_attributes_t		policy_attributes)
{
	mk_sp_info_t		sp_info;
	mk_sp_attributes_t	attributes;

	counter(c_mk_sp_thread_set++);

	/* check that attributes are for this policy */
	assert(policy_attributes->policy_id == policy->policy_id);

	attributes = (mk_sp_attributes_t)policy_attributes;

	/* get pointer to scheduling policy-specific data */
	assert(thread->sp_info != SP_INFO_NULL);
	sp_info = (mk_sp_info_t)thread->sp_info;

	counter(sp_info->c_mk_sp_thread_set++);

	/* update scheduling parameters */
	sp_info->priority = attributes->priority;
	sp_info->max_priority = attributes->max_priority;
	sp_info->sched_data = attributes->sched_data;
	sp_info->unconsumed_quantum = attributes->unconsumed_quantum;

	/*
	 * No check is needed against the pset limit.  If the task could raise
	 * its priority above the limit, then it must have had permisssion.
	 */

	/*
	 * Determine thread's state.  (It may be an "older" thread
	 * that has just been associated with this policy.)
	 */
	if (thread->state & TH_WAIT) {
	    sp_info->th_state = MK_SP_BLOCKED;
	}

	/* recompute priority */
	sp_info->sched_stamp = sched_tick;
	compute_priority(thread, TRUE);

	return(SF_SUCCESS);
}

sf_return_t
_mk_sp_thread_get(
	sf_object_t				policy,
	sched_thread_t			thread,
	sp_attributes_t			policy_attributes,
	sp_attributes_size_t	size)
	/*
	 * Assumes that thread is locked.
	 */
	/***
	 *** ??? Does it assume `splsched()'?
	 ***/
{
	mk_sp_info_t		sp_info;
	mk_sp_attributes_t	attributes;

	counter(c_mk_sp_thread_get++);

	/* check size of attribute structure provided by caller */
	if (policy->sched_attributes_size > size)
		return(SF_FAILURE);

	attributes = (mk_sp_attributes_t)policy_attributes;


	/* get pointer to scheduling policy-specific data */
	assert(thread->sp_info != SP_INFO_NULL);
	sp_info = (mk_sp_info_t)thread->sp_info;

	counter(sp_info->c_mk_sp_thread_get++);

	/* copy scheduling attributes */
	attributes->policy_id = policy->policy_id;
	attributes->priority = sp_info->priority;
	attributes->max_priority = sp_info->max_priority;
	attributes->sched_data = sp_info->sched_data;
	attributes->unconsumed_quantum = sp_info->unconsumed_quantum;

	return(SF_SUCCESS);
}

int
_mk_sp_db_print_sched_stats(
	sf_object_t			policy,
	sched_thread_t		thread)
{
	mk_sp_info_t		sp_info;

	counter(c_mk_sp_db_print_sched_stats++);

	/* get pointer to scheduling policy-specific data */
	assert(thread->sp_info != SP_INFO_NULL);
	sp_info = (mk_sp_info_t)thread->sp_info;

	/* print one field -- `sched_pri' */
	return (thread->sched_pri);
}

/*
 *	thread_priority:
 *
 *	Set priority (and possibly max priority) for thread.
 */
kern_return_t
thread_priority(
	thread_act_t		thr_act,
	int			priority,
	boolean_t		set_max)
{
	kern_return_t		result;
	thread_t		thread;

	if (thr_act == THR_ACT_NULL)
		return (KERN_INVALID_ARGUMENT);

	thread = act_lock_thread(thr_act);
	if (thread == THREAD_NULL || invalid_pri(priority)) {
		act_unlock_thread(thr_act);
		return (KERN_INVALID_ARGUMENT);
	}

	result = thread_priority_locked(thread, priority, set_max);
	act_unlock_thread(thr_act);

	return (result);
}

/*
 *	thread_priority_locked:
 *
 *	Kernel-internal work function for thread_priority().  Called
 *	with thread "properly locked" to ensure synchrony with RPC
 *	(see act_lock_thread()).
 */
kern_return_t
thread_priority_locked(
	thread_t		thread,
	int			priority,
	boolean_t		set_max)
{
	kern_return_t		result = KERN_SUCCESS;
	mk_sp_info_t		sched_info;
	spl_t			s;

	s = splsched();
	thread_lock(thread);

	/* get pointer to scheduling policy-specific data */
	assert(thread->sp_info != SP_INFO_NULL);
	sched_info = (mk_sp_info_t)thread->sp_info;

	/*
	 *	Check for violation of max priority
	 */
	if (priority > sched_info->max_priority)
		result = KERN_FAILURE;
	else {
		/*
		 *	Set priorities.  If a depression is in progress,
		 *	change the priority to restore.
		 */
		if (sched_info->depress_priority >= 0) {
			sched_info->depress_priority = priority;
		}
		else {
			sched_info->priority = priority;
			compute_priority(thread, TRUE);

			/*
			 * If the current thread has changed its
			 * priority let the ast code decide whether
			 * a different thread should run.
			 */
			if (thread == current_thread())
				ast_on(AST_BLOCK);
		}

		if (set_max)
			sched_info->max_priority = priority;
	}

	thread_unlock(thread);
	splx(s);

	return(result);
}

/*
 *	thread_set_own_priority:
 *
 *	Internal use only; sets the priority of the calling thread.
 *	Will adjust max_priority if necessary.
 */
void
thread_set_own_priority(
	int		priority)
{
	thread_t	thread = current_thread();
	mk_sp_info_t	sched_info;
	spl_t		s;

	s = splsched();
	thread_lock(thread);

	/* get pointer to scheduling policy-specific data */
	assert(thread->sp_info != SP_INFO_NULL);
	sched_info = (mk_sp_info_t)thread->sp_info;

	if (priority > sched_info->max_priority)
		sched_info->max_priority = priority;
	sched_info->priority = priority;
	compute_priority(thread, TRUE);

	thread_unlock(thread);
	splx(s);
}

/*
 *	thread_max_priority:
 *
 *	Reset the max priority for a thread.
 */
kern_return_t
thread_max_priority(
	thread_act_t	thr_act,
	processor_set_t	pset,
	int		max_priority)
{
	thread_t	thread;
	kern_return_t	result = KERN_SUCCESS;

	if (thr_act == THR_ACT_NULL)
		return(KERN_INVALID_ARGUMENT);

	thread = act_lock_thread(thr_act);
	if (thread == THREAD_NULL || pset == PROCESSOR_SET_NULL ||
			invalid_pri(max_priority)) {
		act_unlock_thread(thr_act);
		return(KERN_INVALID_ARGUMENT);
	}

	result = thread_max_priority_locked(thread, pset, max_priority);
	act_unlock_thread(thr_act);

	return (result);
}

/*
 *	thread_max_priority_locked:
 *
 *	Work function for thread_max_priority.
 *
 *	Called with all RPC-related locks held for thread (see
 *	act_lock_thread()).
 */
kern_return_t
thread_max_priority_locked(
	thread_t	thread,
	processor_set_t	pset,
	int		max_priority)
{
	kern_return_t	result = KERN_SUCCESS;
	mk_sp_info_t	sched_info;
	spl_t		s;

	s = splsched();
	thread_lock(thread);

	/* get pointer to scheduling policy-specific data */
	assert(thread->sp_info != SP_INFO_NULL);
	sched_info = (mk_sp_info_t)thread->sp_info;

#if	MACH_HOST
	/*
	 *	Check for wrong processor set.
	 */
	if (pset != thread->processor_set)
		result = KERN_FAILURE;
	else {
#endif	/* MACH_HOST */
		sched_info->max_priority = max_priority;

		/*
		 *	Reset priority if it violates new max priority
		 */
		if (max_priority < sched_info->priority) {
			sched_info->priority = max_priority;

			compute_priority(thread, TRUE);
		}
		else if (sched_info->depress_priority >= 0 &&
			 	max_priority < sched_info->depress_priority)
			sched_info->depress_priority = max_priority;
#if	MACH_HOST
	}
#endif	/* MACH_HOST */

	thread_unlock(thread);
	splx(s);

	return(result);
}

/*
 *	thread_policy_common:
 *
 *	Set scheduling policy for thread. If pset == PROCESSOR_SET_NULL,
 * 	policy will be checked to make sure it is enabled.
 */
kern_return_t
thread_policy_common(
	thread_t	thread,
	int		policy,
	int		data,
	processor_set_t	pset)
{
	kern_return_t	result = KERN_SUCCESS;
	mk_sp_info_t	sched_info;
	register int	temp;
	spl_t		s;

	if (thread == THREAD_NULL || invalid_policy(policy))
		return(KERN_INVALID_ARGUMENT);

	s = splsched();
	thread_lock(thread);

	/* get pointer to scheduling policy-specific data */
	assert(thread->sp_info != SP_INFO_NULL);
	sched_info = (mk_sp_info_t)thread->sp_info;

	/*
	 *	Check if changing policy.
	 */
	if (policy == thread->policy) {
	    /*
	     *	Just changing data.  This is meaningless for
	     *	timeshareing, quantum for fixed priority (but
	     *	has no effect until current quantum runs out).
	     */
	    if (policy == POLICY_FIFO || policy == POLICY_RR) {
		temp = data * 1000;
		if (temp % tick)
		    temp += tick;
		sched_info->sched_data = temp/tick;
		sched_info->unconsumed_quantum = temp/tick;
	    }
	}
	else {
	    /*
	     *	Changing policy.  Check if new policy is allowed.
	     */
	    if (pset == PROCESSOR_SET_NULL && 
			(thread->processor_set->policies & policy) == 0) {
		result = KERN_FAILURE;
	    }
	    else {
		if (pset != thread->processor_set) {
		    result = KERN_FAILURE;
		}
		else {
		    /*
		     *	Changing policy.  Save data and calculate new
		     *	priority.
		     */
		    thread->policy = policy;
		    sched_info->policy = policy;
		    if (policy == POLICY_FIFO || policy == POLICY_RR) {
			temp = data * 1000;
			if (temp % tick)
			    temp += tick;
			sched_info->sched_data = temp/tick;
			sched_info->unconsumed_quantum = temp/tick;
		    }
		    compute_priority(thread, TRUE);
		}
	    }
	}

	thread_unlock(thread);
	splx(s);

	return(result);
}

/*
 *	thread_set_policy
 *
 *	Set scheduling policy and parameters, both base and limit, for 
 *	the given thread. Policy can be any policy implemented by the
 *	processor set, whether enabled or not. 
 */
kern_return_t
thread_set_policy(
	thread_act_t			thr_act,
	processor_set_t			pset,
	policy_t			policy,
	policy_base_t			base,
	mach_msg_type_number_t		base_count,
	policy_limit_t			limit,
	mach_msg_type_number_t		limit_count)
{
	thread_t			thread;
	int 				max, bas, dat;
	kern_return_t			result = KERN_SUCCESS;

	if (thr_act == THR_ACT_NULL || pset == PROCESSOR_SET_NULL)
		return (KERN_INVALID_ARGUMENT);

	thread = act_lock_thread(thr_act);
	if (thread == THREAD_NULL) {
		act_unlock_thread(thr_act);
		return(KERN_INVALID_ARGUMENT);
	}

	if (pset != thread->processor_set) {
		act_unlock_thread(thr_act);
		return(KERN_FAILURE);
	}

	switch (policy) {
	case POLICY_RR:
	{
                policy_rr_base_t rr_base = (policy_rr_base_t) base;
                policy_rr_limit_t rr_limit = (policy_rr_limit_t) limit;

                if (base_count != POLICY_RR_BASE_COUNT ||
                    limit_count != POLICY_RR_LIMIT_COUNT) {
                        result = KERN_INVALID_ARGUMENT;
                        break;
                }
		dat = rr_base->quantum;
		bas = rr_base->base_priority;
		max = rr_limit->max_priority;
		if (invalid_pri(bas) || invalid_pri(max)) {
			result = KERN_INVALID_ARGUMENT;
			break;
		}
		break;
	}

	case POLICY_FIFO:
	{
                policy_fifo_base_t fifo_base = (policy_fifo_base_t) base;
                policy_fifo_limit_t fifo_limit = (policy_fifo_limit_t) limit;

                if (base_count != POLICY_FIFO_BASE_COUNT ||
                    limit_count != POLICY_FIFO_LIMIT_COUNT) {
                        result = KERN_INVALID_ARGUMENT;
                        break;
                }
		dat = 0;
		bas = fifo_base->base_priority;
		max = fifo_limit->max_priority;
		if (invalid_pri(bas) || invalid_pri(max)) {
			result = KERN_INVALID_ARGUMENT;
                        break;
                }
                break;
	}

	case POLICY_TIMESHARE:
        {
                policy_timeshare_base_t ts_base =
                                        (policy_timeshare_base_t) base;
                policy_timeshare_limit_t ts_limit =
                                        (policy_timeshare_limit_t) limit;

                if (base_count != POLICY_TIMESHARE_BASE_COUNT ||
                    limit_count != POLICY_TIMESHARE_LIMIT_COUNT) {
                        result = KERN_INVALID_ARGUMENT;
                        break;
                }
		dat = 0;
		bas = ts_base->base_priority;
		max = ts_limit->max_priority;
		if (invalid_pri(bas) || invalid_pri(max)) {
			result = KERN_INVALID_ARGUMENT;
                        break;
                }
                break;
	}

	default:
		result = KERN_INVALID_POLICY;
	}

	if (result != KERN_SUCCESS) {
		act_unlock_thread(thr_act);
		return(result);
	}

	/*	
	 *	We've already checked the inputs for thread_max_priority,
	 *	so it's safe to ignore the return value.
	 */
	(void) thread_max_priority_locked(thread, pset, max);	
	result = thread_priority_locked(thread, bas, FALSE);
	if (result == KERN_SUCCESS)
		result = thread_policy_common(thread, policy, dat, pset);
	act_unlock_thread(thr_act);
	return(result);
}


/*
 * 	thread_policy
 *
 *	Set scheduling policy and parameters, both base and limit, for
 *	the given thread. Policy must be a policy which is enabled for the
 *	processor set. Change contained threads if requested. 
 */
kern_return_t
thread_policy(
	thread_act_t			thr_act,
        policy_t			policy,
        policy_base_t			base,
	mach_msg_type_number_t		count,
        boolean_t			set_limit)
{
	thread_t			thread;
        processor_set_t			pset;
	mk_sp_info_t			sched_info;
        kern_return_t			result = KERN_SUCCESS;
	policy_limit_t			limit;
        policy_rr_limit_data_t		rr_limit;
        policy_fifo_limit_data_t	fifo_limit;
        policy_timeshare_limit_data_t	ts_limit;
	int				limcount;
	
	if (thr_act == THR_ACT_NULL)
		return (KERN_INVALID_ARGUMENT);

	thread = act_lock_thread(thr_act);
	pset = thread->processor_set;
	if (thread == THREAD_NULL || pset == PROCESSOR_SET_NULL) {
		act_unlock_thread(thr_act);
		return(KERN_INVALID_ARGUMENT);
	}

	if (invalid_policy(policy) || (pset->policies & policy) == 0) {
		act_unlock_thread(thr_act);
		return(KERN_INVALID_POLICY);
	}

	/* get pointer to scheduling policy-specific data */
	assert(thread->sp_info != SP_INFO_NULL);
	sched_info = (mk_sp_info_t)thread->sp_info;

	if (set_limit) {
		/*
	 	 * 	Set scheduling limits to base priority.
		 */
		switch (policy) {
		case POLICY_RR:
		{
                        policy_rr_base_t rr_base;

                        if (count != POLICY_RR_BASE_COUNT) {
                                result = KERN_INVALID_ARGUMENT;
				break;
			}
                        limcount = POLICY_RR_LIMIT_COUNT;
                        rr_base = (policy_rr_base_t) base;
                        rr_limit.max_priority = rr_base->base_priority;
                        limit = (policy_limit_t) &rr_limit;
			break;
		}

		case POLICY_FIFO:
                {
                       	policy_fifo_base_t fifo_base;

                       	if (count != POLICY_FIFO_BASE_COUNT) {
                                result = KERN_INVALID_ARGUMENT;
				break;
			}
                       	limcount = POLICY_FIFO_LIMIT_COUNT;
                        fifo_base = (policy_fifo_base_t) base;
                        fifo_limit.max_priority = fifo_base->base_priority;
                        limit = (policy_limit_t) &fifo_limit;
			break;
		}

		case POLICY_TIMESHARE:
                {
                        policy_timeshare_base_t ts_base;

                        if (count != POLICY_TIMESHARE_BASE_COUNT) {
                                result = KERN_INVALID_ARGUMENT;
				break;
			}
                        limcount = POLICY_TIMESHARE_LIMIT_COUNT;
                        ts_base = (policy_timeshare_base_t) base;
                        ts_limit.max_priority = ts_base->base_priority;
                        limit = (policy_limit_t) &ts_limit;
			break;
		}

		default:
			result = KERN_INVALID_POLICY;
			break;
		}

	} else {
		/*
		 *	Use current scheduling limits. Ensure that the
		 *	new base priority will not exceed current limits.
		 */
                switch (policy) {
                case POLICY_RR:
                {
                        policy_rr_base_t rr_base;

                        if (count != POLICY_RR_BASE_COUNT) {
                                result = KERN_INVALID_ARGUMENT;
				break;
			}
                        limcount = POLICY_RR_LIMIT_COUNT;
                        rr_base = (policy_rr_base_t) base;
			if (rr_base->base_priority >
					sched_info->max_priority) {
				result = KERN_POLICY_LIMIT;
				break;
			}
                        rr_limit.max_priority = sched_info->max_priority;
                        limit = (policy_limit_t) &rr_limit;
                        break;
		}

                case POLICY_FIFO:
                {
                        policy_fifo_base_t fifo_base;

                        if (count != POLICY_FIFO_BASE_COUNT) {
                                result = KERN_INVALID_ARGUMENT;
				break;
			}
                        limcount = POLICY_FIFO_LIMIT_COUNT;
                        fifo_base = (policy_fifo_base_t) base;
			if (fifo_base->base_priority >
					sched_info->max_priority) {
				result = KERN_POLICY_LIMIT;
				break;
			}
                        fifo_limit.max_priority = sched_info->max_priority;
                        limit = (policy_limit_t) &fifo_limit;
                        break;
		}

                case POLICY_TIMESHARE:
                {
                        policy_timeshare_base_t ts_base;

                        if (count != POLICY_TIMESHARE_BASE_COUNT) {
                                result = KERN_INVALID_ARGUMENT;
				break;
			}
                        limcount = POLICY_TIMESHARE_LIMIT_COUNT;
                        ts_base = (policy_timeshare_base_t) base;
			if (ts_base->base_priority >
					sched_info->max_priority) {
				result = KERN_POLICY_LIMIT;
				break;
			}
                        ts_limit.max_priority = sched_info->max_priority;
                        limit = (policy_limit_t) &ts_limit;
                        break;
		}

                default:
			result = KERN_INVALID_POLICY;
			break;
                }

	}

	act_unlock_thread(thr_act);

	if (result == KERN_SUCCESS)
	    result = thread_set_policy(thr_act, pset,
					 policy, base, count, limit, limcount);

	return(result);
}


/*
 *	Define shifts for simulating (5/8)**n
 */

shift_data_t	wait_shift[32] = {
	{1,1},{1,3},{1,-3},{2,-7},{3,5},{3,-5},{4,-8},{5,7},
	{5,-7},{6,-10},{7,10},{7,-9},{8,-11},{9,12},{9,-11},{10,-13},
	{11,14},{11,-13},{12,-15},{13,17},{13,-15},{14,-17},{15,19},{16,18},
	{16,-19},{17,22},{18,20},{18,-20},{19,26},{20,22},{20,-22},{21,-27}};

/*
 *	do_priority_computation:
 *
 *	Calculate new priority for thread based on its base priority plus
 *	accumulated usage.  PRI_SHIFT and PRI_SHIFT_2 convert from
 *	usage to priorities.  SCHED_SHIFT converts for the scaling
 *	of the sched_usage field by SCHED_SCALE.  This scaling comes
 *	from the multiplication by sched_load (thread_timer_delta)
 *	in sched.h.  sched_load is calculated as a scaled overload
 *	factor in compute_mach_factor (mach_factor.c).
 */
#ifdef	PRI_SHIFT_2
#if	PRI_SHIFT_2 > 0
#define do_priority_computation(sp_info, pri)						\
	MACRO_BEGIN														\
	(pri) = (sp_info)->priority		/* start with base priority */	\
	    - ((sp_info)->sched_usage >> (PRI_SHIFT + SCHED_SHIFT))		\
	    - ((sp_info)->sched_usage >> (PRI_SHIFT_2 + SCHED_SHIFT));	\
	if ((pri) < lpri) (pri) = lpri;									\
	(pri) = convert_pri[(pri)];										\
	MACRO_END
#else	/* PRI_SHIFT_2 */
#define do_priority_computation(sp_info, pri)						\
	MACRO_BEGIN														\
	(pri) = (sp_info)->priority		/* start with base priority */	\
	    - ((sp_info)->sched_usage >> (PRI_SHIFT + SCHED_SHIFT))		\
	    + ((sp_info)->sched_usage >> (SCHED_SHIFT - PRI_SHIFT_2));	\
	if ((pri) < lpri) (pri) = lpri;									\
	(pri) = convert_pri[(pri)];										\
	MACRO_END
#endif	/* PRI_SHIFT_2 */
#else	/* defined(PRI_SHIFT_2) */
#define do_priority_computation(sp_info, pri)						\
	MACRO_BEGIN														\
	(pri) = (sp_info)->priority		/* start with base priority */	\
	    - ((sp_info)->sched_usage >> (PRI_SHIFT + SCHED_SHIFT));	\
	if ((pri) < lpri) (pri) = lpri;									\
	(pri) = convert_pri[(pri)];										\
	MACRO_END
#endif	/* defined(PRI_SHIFT_2) */

/*
 *	compute_priority:
 *
 *	Compute the effective priority of the specified thread.
 *	The effective priority computation is as follows:
 *
 *	Take the base priority for this thread and add
 *	to it an increment derived from its cpu_usage.
 *
 *	The thread *must* be locked by the caller. 
 */

void
compute_priority(
	register thread_t	thread,
	boolean_t			resched)
{
	mk_sp_info_t		sp_info = (mk_sp_info_t)thread->sp_info;
	register int		pri;

	/* make sure policy-specific data are present */
	assert(sp_info != SP_INFO_NULL);

	if (thread->policy == POLICY_TIMESHARE) {
	    do_priority_computation(sp_info, pri);
	    if (sp_info->depress_priority < 0)
			set_pri(thread, pri, resched);
	    else
			sp_info->depress_priority = pri;
	}
	else
	    set_pri(thread, sp_info->priority, resched);
}

/*
 *	compute_my_priority:
 *
 *	Version of compute priority for current thread or thread
 *	being manipulated by scheduler (going on or off a runq).
 *	Only used for priority updates.  Policy or priority changes
 *	must call compute_priority above.  Caller must have thread
 *	locked and know it is timesharing and not depressed.
 */

void
compute_my_priority(
	register thread_t	thread)
{
	mk_sp_info_t	sp_info = (mk_sp_info_t)thread->sp_info;
	register int	temp_pri;

	/* make sure policy-specific data are present */
	assert(thread->sp_info != SP_INFO_NULL);

	do_priority_computation(sp_info,temp_pri);
	assert(thread->runq == RUN_QUEUE_NULL);
	thread->sched_pri = temp_pri;
}

struct mk_sp_usage {
	natural_t	cpu_delta, sched_delta;
	natural_t	sched_tick, ticks;
	natural_t	cpu_usage, sched_usage,
				aged_cpu, aged_sched;
	thread_t	thread;
} idled_info, loaded_info;

/*
 *	update_priority
 *
 *	Cause the priority computation of a thread that has been 
 *	sleeping or suspended to "catch up" with the system.  Thread
 *	*MUST* be locked by caller.  If thread is running, then this
 *	can only be called by the thread on itself.
 */
void
update_priority(
	register thread_t		thread)
{
	mk_sp_info_t			sp_info = (mk_sp_info_t)thread->sp_info;
	register unsigned int	ticks;
	register shift_t		shiftp;

	/* make sure policy-specific data are present */
	assert(sp_info != SP_INFO_NULL);

	ticks = sched_tick - sp_info->sched_stamp;
	assert(ticks != 0);

	/*
	 *	If asleep for more than 30 seconds forget all
	 *	cpu_usage, else catch up on missed aging.
	 *	5/8 ** n is approximated by the two shifts
	 *	in the wait_shift array.
	 */
	sp_info->sched_stamp += ticks;
	thread_timer_delta(thread);
	if (ticks >  30) {
		sp_info->cpu_usage = 0;
		sp_info->sched_usage = 0;
	}
	else {
		struct mk_sp_usage *sp_usage;

		sp_info->cpu_usage += thread->cpu_delta;
		sp_info->sched_usage += thread->sched_delta;

		if (thread->state & TH_IDLE)
			sp_usage = &idled_info;
		else
		if (thread == loaded_info.thread)
			sp_usage = &loaded_info;
		else
			sp_usage = NULL;

		if (sp_usage != NULL) {
			sp_usage->cpu_delta = thread->cpu_delta;
			sp_usage->sched_delta = thread->sched_delta;
			sp_usage->sched_tick = sp_info->sched_stamp;
			sp_usage->ticks = ticks;
			sp_usage->cpu_usage = sp_info->cpu_usage;
			sp_usage->sched_usage = sp_info->sched_usage;
			sp_usage->thread = thread;
		}

		shiftp = &wait_shift[ticks];
		if (shiftp->shift2 > 0) {
		    sp_info->cpu_usage =
						(sp_info->cpu_usage >> shiftp->shift1) +
						(sp_info->cpu_usage >> shiftp->shift2);
		    sp_info->sched_usage =
						(sp_info->sched_usage >> shiftp->shift1) +
						(sp_info->sched_usage >> shiftp->shift2);
		}
		else {
		    sp_info->cpu_usage =
						(sp_info->cpu_usage >> shiftp->shift1) -
						(sp_info->cpu_usage >> -(shiftp->shift2));
		    sp_info->sched_usage =
						(sp_info->sched_usage >> shiftp->shift1) -
						(sp_info->sched_usage >> -(shiftp->shift2));
		}

		if (sp_usage != NULL) {
			sp_usage->aged_cpu = sp_info->cpu_usage;
			sp_usage->aged_sched = sp_info->sched_usage;
		}
	}
	thread->cpu_delta = 0;
	thread->sched_delta = 0;

	/*
	 *	Recompute priority if appropriate.
	 */
	if (	thread->policy == POLICY_TIMESHARE		&&
			sp_info->depress_priority < 0			) {
		register int		new_pri;
		run_queue_t			runq;

		do_priority_computation(sp_info, new_pri);
		runq = rem_runq(thread);
		thread->sched_pri = new_pri;
		if (runq != RUN_QUEUE_NULL)
			thread_setrun(thread, TRUE, TAIL_Q);
	}
}

/*
 *	`mk_sp_swtch_pri()' attempts to context switch (logic in
 *	thread_block no-ops the context switch if nothing would happen).
 *	A boolean is returned that indicates whether there is anything
 *	else runnable.
 *
 *	This boolean can be used by a thread waiting on a
 *	lock or condition:  If FALSE is returned, the thread is justified
 *	in becoming a resource hog by continuing to spin because there's
 *	nothing else useful that the processor could do.  If TRUE is
 *	returned, the thread should make one more check on the
 *	lock and then be a good citizen and really suspend.
 */

void
_mk_sp_swtch_pri(
	sf_object_t			policy,
	int					pri)
{
	register thread_t	self = current_thread();

#ifdef	lint
	pri++;
#endif	/* lint */

	counter(c_mk_sp_swtch_pri++);
	/*
	 *	XXX need to think about depression duration.
	 *	XXX currently using min quantum.
	 */
	_mk_sp_thread_depress_priority(policy, min_quantum_ms);

	counter(c_swtch_pri_block++);
	thread_block((void (*)(void)) 0);

	_mk_sp_thread_depress_abort(policy, self);
}

/*
 *	thread_switch:
 *
 *	Context switch.  User may supply thread hint.
 *
 *	Fixed priority threads that call this get what they asked for
 *	even if that violates priority order.
 */
kern_return_t
_mk_sp_thread_switch(
	sf_object_t				policy,
	thread_act_t			hint_act,
	int						option,
	mach_msg_timeout_t		option_time)
{
    register thread_t		self = current_thread();
    register processor_t	myprocessor;
    mk_sp_info_t			sp_info;
	int						s;

    counter(c_mk_sp_thread_switch++);

    /*
     *	Process option.
     */
    switch (option) {

	case SWITCH_OPTION_NONE:
	    /*
	     *	Nothing to do.
	     */
	    break;

	case SWITCH_OPTION_DEPRESS:
	    /*
	     *	Depress priority for a given time.
	     */
	    _mk_sp_thread_depress_priority(policy, option_time);
	    break;

	case SWITCH_OPTION_WAIT:
	    assert_wait_timeout(option_time, THREAD_ABORTSAFE);
	    break;

	default:
	    return (KERN_INVALID_ARGUMENT);
    }
    
    /*
     *	Check and use thr_act hint if appropriate.  It is not
     *  appropriate to give a hint that shares the current shuttle.
     *
     *  JMM - Is it really appropriate to give a hint that isn't
     *  the top activation for a thread?
     */
    if (hint_act->thread != self) {
    	register thread_t		thread = hint_act->thread;
	
		/*
		 *	Check if the thread is in the right pset. Then
		 *	pull it off its run queue.  If it
		 *	doesn't come, then it's not eligible.
		 */
		if (thread) {
			s = splsched();
			thread_lock(thread);

			if (	thread->processor_set == self->processor_set	&&
					rem_runq(thread) != RUN_QUEUE_NULL					) {
				/*
				 *	Hah, got it!!
				 */
				if (thread->policy & (POLICY_FIFO|POLICY_RR)) {
					myprocessor = current_processor();

					assert(thread->sp_info != SP_INFO_NULL);
					sp_info = (mk_sp_info_t)thread->sp_info;

					myprocessor->quantum = sp_info->unconsumed_quantum;
					myprocessor->first_quantum = TRUE;
				}
				thread_unlock(thread);
				splx(s);
				act_unlock_thread(hint_act);
				
				counter(c_thread_switch_handoff++);
				thread_run((void(*)(void)) 0, thread);

				goto out;
			}

			thread_unlock(thread);
			splx(s);
		}

		act_unlock_thread(hint_act);
    }

    /*
     *	No handoff hint supplied, or hint was wrong.  Call thread_block() in
     *	hopes of running something else.  If nothing else is runnable,
     *	thread_block will detect this.  WARNING: thread_switch with no
     *	option will not do anything useful if the thread calling it is the
     *	highest priority thread (can easily happen with a collection
     *	of timesharing threads).
     */
	mp_disable_preemption();
    myprocessor = current_processor();
    if (	option != SWITCH_OPTION_NONE					||
			myprocessor->processor_set->runq.count > 0		||
			myprocessor->runq.count > 0							) {
		myprocessor->first_quantum = FALSE;
		mp_enable_preemption();
	  
		counter(c_thread_switch_block++);
		thread_block((void (*)(void)) 0);
	}
	else
		mp_enable_preemption();

out:
	if (option == SWITCH_OPTION_WAIT)
		thread_cancel_timer();
	else if (option == SWITCH_OPTION_DEPRESS)
		_mk_sp_thread_depress_abort(policy, self);

    return (KERN_SUCCESS);
}

/*
 *	mk_sp_thread_depress_priority
 *
 *	Depress thread's priority to lowest possible for specified period.
 *	Intended for use when thread wants a lock but doesn't know which
 *	other thread is holding it.  As with thread_switch, fixed
 *	priority threads get exactly what they asked for.  Users access
 *	this by the SWITCH_OPTION_DEPRESS option to thread_switch.  A Time
 *      of zero will result in no timeout being scheduled.
 */
void
_mk_sp_thread_depress_priority(
	sf_object_t				policy,
	mach_msg_timeout_t		interval)
{
	register thread_t		self = current_thread();
    mk_sp_info_t			sp_info;
	AbsoluteTime			deadline;
	boolean_t				release = FALSE;
    spl_t					s;

    s = splsched();
    thread_lock(self);

	if (self->policy == policy->policy_id) {
		/* get pointer to scheduling policy-specific data */
		assert(self->sp_info != SP_INFO_NULL);
		sp_info = (mk_sp_info_t)self->sp_info;

		/*
		 * If we haven't already saved the priority to be restored
		 * (depress_priority), then save it.
		 */
		if (sp_info->depress_priority < 0)
			sp_info->depress_priority = sp_info->priority;
		else if (thread_call_cancel(&self->depress_timer))
			release = TRUE;

		self->sched_pri = sp_info->priority = DEPRESSPRI;

		if (interval != 0) {
			clock_interval_to_deadline(
								interval, 1000*NSEC_PER_USEC, &deadline);
			thread_call_enter_delayed(&self->depress_timer, deadline);
			if (!release)
				self->ref_count++;
			else
				release = FALSE;
		}
	}

    thread_unlock(self);
    splx(s);

	if (release)
		thread_deallocate(self);
}	

/*
 *	mk_sp_thread_depress_timeout:
 *
 *	Timeout routine for priority depression.
 */
void
_mk_sp_thread_depress_timeout(
	sf_object_t				policy,
	register thread_t		thread)
{
    mk_sp_info_t			sp_info;
    spl_t					s;

    counter(c_mk_sp_thread_depress_timeout++);

    s = splsched();
    thread_lock(thread);
	if (thread->policy == policy->policy_id) {
		/* get pointer to scheduling policy-specific data */
		assert(thread->sp_info != SP_INFO_NULL);
		sp_info = (mk_sp_info_t)thread->sp_info;

		/*
		 *	If we lose a race with mk_sp_thread_depress_abort,
		 *	then depress_priority might be -1.
		 */
		if (	sp_info->depress_priority >= 0						&&
				!thread_call_is_delayed(&thread->depress_timer, NULL)		) {
			sp_info->priority = sp_info->depress_priority;
			sp_info->depress_priority = -1;
			compute_priority(thread, FALSE);
		}
		else
		if (sp_info->depress_priority == -2) {
			/*
			 * Thread was temporarily undepressed by thread_suspend, to
			 * be redepressed in special_handler as it blocks.  We need to
			 * prevent special_handler from redepressing it, since depression
			 * has timed out:
			 */
			sp_info->depress_priority = -1;
		}
	}
	thread_unlock(thread);
	splx(s);
}

/*
 *	mk_sp_thread_depress_abort:
 *
 *	Prematurely abort priority depression if there is one.
 */
kern_return_t
_mk_sp_thread_depress_abort(
	sf_object_t				policy,
	register thread_t		thread)
{
    mk_sp_info_t			sp_info;
    kern_return_t 			result = KERN_SUCCESS;
	boolean_t				release = FALSE;
    spl_t					s;

    s = splsched();
    thread_lock(thread);

	if (thread->policy == policy->policy_id) {
		/* get pointer to scheduling policy-specific data */
		assert(thread->sp_info != SP_INFO_NULL);
		sp_info = (mk_sp_info_t)thread->sp_info;

		if (sp_info->depress_priority >= 0) {
			if (thread_call_cancel(&thread->depress_timer))
				release = TRUE;
			sp_info->priority = sp_info->depress_priority;
			sp_info->depress_priority = -1;
			compute_priority(thread, FALSE);
		}
		else
			result = KERN_NOT_DEPRESSED;
	}

    thread_unlock(thread);
    splx(s);

	if (release)
		thread_deallocate(thread);

    return (result);
}

/*
 *	mk_sp_thread_runnable:
 *
 *	Return TRUE iff policy believes thread is runnable
 */
boolean_t
_mk_sp_thread_runnable(
	sf_object_t			policy,
	sched_thread_t		thread)
{
	mk_sp_info_t		sp_info;

	counter(c_mk_sp_thread_runnable++);

	/* get pointer to scheduling policy-specific data */
	assert(thread->sp_info != SP_INFO_NULL);
	sp_info = (mk_sp_info_t)thread->sp_info;

	counter(sp_info->c_mk_sp_thread_runnable++);

	return (sp_info->th_state == MK_SP_RUNNABLE);
}

sf_return_t
_mk_sp_alarm_expired(
	sf_object_t			policy,
	long				alarm_seqno,
	kern_return_t		result,
	int					alarm_type,
	mach_timespec_t		wakeup_time,
	void				*alarm_data)
{
	counter(c_mk_sp_alarm_expired++);

	/* this should NEVER happen */
	assert(0);

	return (SF_FAILURE);
}


void
_mk_sp_thread_dump(
	sched_thread_t		thread)		/* thread */
{
	mk_sp_info_t		sp_info;
	int			count;

	/* get pointer to scheduling policy-specific data */
	assert(thread->sp_info != SP_INFO_NULL);
	sp_info = (mk_sp_info_t)thread->sp_info;

	/* dump the data in a pretty format */
	printf("\nthread shuttle(0x%x):\n", thread);

	printf("\tpolicy: ");
	switch(thread->policy) {
		case POLICY_TIMESHARE:
			printf("POLICY_TIMESHARE\n");
			break;

		case POLICY_RR:
			printf("POLICY_RR\n");
			break;

		case POLICY_FIFO:
			printf("POLICY_FIFO\n");
			break;

		default:
			printf("NOT an MK Standard Policy\n");
			return;
	}

	printf("\tth_state: ");
	if (sp_info->th_state == 0) {
		printf("NULL");
	}
	else {
		count = 0;
		if (sp_info->th_state & MK_SP_ATTACHED) {
			printf("MK_SP_ATTACHED ");
			count++;
		}
		if (sp_info->th_state & MK_SP_RUNNABLE) {
			if (count > 0) printf(" | ");
			printf("MK_SP_RUNNABLE ");
			count++;
		}
		if (sp_info->th_state & MK_SP_BLOCKED) {
			if (count > 0) printf(" | ");
			printf("MK_SP_BLOCKED ");
			count++;
		}
	}
	printf("\n");

	printf("\tpriority: %d\n", sp_info->priority);
	printf("\tmax_priority: %d\n", sp_info->max_priority);
	printf("\tsched_data: %d\n", sp_info->sched_data);
	printf("\tdepress_priority: %d\n", sp_info->depress_priority);
	printf("\tcpu_usage: %d\n", sp_info->cpu_usage);
	printf("\tsched_usage: %d\n", sp_info->sched_usage);
	printf("\tsched_stamp: %d\n", sp_info->sched_stamp);
	printf("\tunconsumed_quantum: %d\n", sp_info->unconsumed_quantum);
	printf("\tsched_pri: %d\n", thread->sched_pri);

#ifdef	MACH_ASSERT
	printf("\tc_mk_sp_thread_attach: %d\n",
	       sp_info->c_mk_sp_thread_attach);
	printf("\tc_mk_sp_thread_detach: %d\n",
	       sp_info->c_mk_sp_thread_detach);
	printf("\tc_mk_sp_thread_begin: %d\n",
	       sp_info->c_mk_sp_thread_begin);
	printf("\tc_mk_sp_thread_done: %d\n",
	       sp_info->c_mk_sp_thread_done);
	printf("\tc_mk_sp_thread_dispatch: %d\n",
	       sp_info->c_mk_sp_thread_dispatch);
	printf("\tc_mk_sp_thread_unblock: %d\n",
	       sp_info->c_mk_sp_thread_unblock);
	printf("\tc_mk_sp_thread_set: %d\n",
	       sp_info->c_mk_sp_thread_set);
	printf("\tc_mk_sp_thread_get: %d\n",
	       sp_info->c_mk_sp_thread_get);
#endif	/* MACH_ASSERT */

	return;
}
