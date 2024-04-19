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

#ifndef	_KERN_SF_H_
#define _KERN_SF_H_

/*
 * The calls most likely to change are: policy_thread_done and
 * policy_thread_begin.  They're the policy calls related to 
 * context switching. I'm not satisfied with what I have now and
 * these are the forms I'm trying next.
 * 
 * I still have to merge the data type names from my different sandboxes
 * and I don't really talk about locking except for the run queue locking.
 * 
 * A good example of the data type problem is sched_thread_t and thread_t.
 * Right now, they're equivalent BUT eventually, i expect that policies
 * will only use sched_thread_t which will only contain the scheduling
 * attributes and the framework will know how to convert sched_thread_t
 * to thread_t (policy-mechanism seperation).
 * 
 * There is a big change for run queues: there is a single lock for an
 * entire run queue array structure (instead of a lock per queue header).
 * It's OK for a policy to reorganize a particular queue BUT it has to
 * disable the queue header (sched_queue_disable).  Since a queue header
 * isn't shared by multiple policies and the framework won't touch the
 * queue header if it's disabled, the policy can do anything it wants
 * without taking out a global lock.
 * 
 * The only run queue primitives provided are the really fast ones:
 * insert at the head (sched_queue_preempt), insert at the tail
 * and if the queue was empty check for preemption 
 * (sched_queue_add_preempt), just insert at the tail
 * (sched_queue_add_only), and remove (sched_queue_remove).  Everything
 * else needs to be done by first disabling the queue header (and then
 * you can do whatever you want to the queue).
 * 
 * BTW, the convention here is:
 * 
 *    policy_xxx - calls from the framework into policies (via the
 * 	pointers in the policy object)
 * 
 *    sched_xxx - scheduling mechanisms provided by the framework
 *         which can be called by policies.
 * 
 * ----------
 * 
 * Initializes an instance of a scheduling policy assigning it the
 * corresponding policy_id and run queue headers.
 * 
 * policy_init(
 *     sched_policy_object	 *policy,
 *     int policy_id,				/ * policy number * /
 *     int priority_mask_length,			/ * length of a priority mask * /
 *     sched_priority_mask *priority_mask );	/ * mask for this policy * /
 * 
 * Enable/disable a scheduling policy on a processor [set]
 * 
 * policy_enable_processor_set(
 *     sched_policy_object *policy,		/ * policy * /
 *     processor_set_t processor_set );		/ * processor set * /
 * 
 * policy_disable_processor_set(
 *     sched_policy_object *policy,
 *     processor_set_t processor_set);
 * 
 * policy_enable_processor(
 *     sched_policy_object *policy,		/ * policy * /
 *     processor_t processor );			/ * processor set * /
 * 
 * policy_disable_processor(
 *     sched_policy_object *policy,
 *     processor_t processor);
 * 
 * Notifies the policy that the thread has become runnable
 * 
 * policy_thread_unblock(
 *     sched_policy_object *policy,
 *     sched_thread_t thread )
 * 
 * Notifies the policy that the current thread is done or
 * a new thread has been selected to run
 * 
 * policy_thread_done(
 *     sched_policy_object *policy,
 *     sched_thread_t *old_thread );
 * 
 * policy_thread_begin(
 *     sched_policy_object *policy,
 *     sched_thread_t *new_thread );
 * 
 * Attach/detach a thread from the scheduling policy
 * 
 * policy_thread_attach(
 *     sched_policy_object *policy,
 *     sched_thread_t *thread );
 * 
 * policy_thread_detach(
 *     sched_policy_object *policy,
 *     sched_thread_t *thread );
 * 
 * Set the thread's processor [set]
 * 
 * policy_thread_processor(
 *     sched_policy_object *policy,
 *     sched_thread_t *thread,
 *     processor_t processor );
 * 
 * policy_thread_processor_set(
 *     sched_policy_object *policy,
 *     sched_thread_t *thread,
 *     processor_set_t processor_set);
 * 
 * Set/get a thread's scheduling attributes
 * 
 * policy_thread_set(
 *     sched_policy_object *policy,
 *     sched_thread_t thread,
 *     sched_policy_attributes policy_attributes);
 * 
 * policy_thread_get(
 *     sched_policy_object *policy,
 *     sched_thread_t thread,
 *     sched_policy_attributes *policy_attributes);
 * 
 * Scheduling Framework Interfaces
 * 
 * [en/dis]able particular run queue headers on a processor [set],
 * 
 * Lock the run queues, update the mask, unlock the run queues.  If
 * enabling, check preemption.
 * 
 * sched_queue_enable(
 *     run_queue_t		runq,
 *     sched_priority_mask *mask );
 * 
 * sched_queue_disable(
 *     run_queue_t		runq,
 *     sched_priority_mask *mask );
 * 
 * Lock the run queues, insert the thread at the head, unlock the
 * run queues and preempt (if possible).
 * 
 * sched_queue_preempt(
 *     meta_priority_t	priority,
 *     thread_t		thread,
 *     run_queue_t		run_queues );
 * 
 * Lock the run queues, add the thread to the tail, unlock the run queues
 * and preempt if appropriate.
 * 
 * sched_queue_add_preempt(
 *     meta_priority_t	priority,
 *     thread_t		thread,
 *     run_queue_t		run_queues );
 * 
 * Lock the run queues, add the thread to the tail, unlock the queues
 * but don't check for preemption.
 * 
 * sched_queue_add_only(
 *     meta_priority_t	priority,
 *     thread_t		thread,
 *     run_queue_t		run_queues );
 * 
 * Lock the run queues, remove the entry the thread, unlock the run queues.
 * 
 * sched_queue_remove(
 *     thread_t		thread );
 */

#include <kern/kern_types.h>
#include <kern/sched.h>
#include <mach/thread_switch.h>
#include <mach/mach_types.h>

/*
 * Type definitions and constants for MK Scheduling Framework
 */
typedef int	sf_return_t;

/* successful completion */
#define SF_SUCCESS					0

/* error codes */
#define SF_FAILURE					1
#define SF_KERN_RESOURCE_SHORTAGE	2

/* Scheduler Framework Object -- i.e., a scheduling policy */
typedef struct sf_policy	*sf_object_t;

/* Scheduler Framework thread definition is just a thread (for now) */
typedef thread_t			sched_thread_t;

/* Scheduler Framework priority mask definition */
typedef struct {
	int bitmap[NRQBM];
} sf_priority_mask, *sf_priority_mask_t;

/* Scheduler Framework meta-priority definition is just an int (for now) */
typedef int	meta_priority_t;

/**********
 *
 * Scheduling Attributes
 *
 **********/

struct sp_attributes {
	int			policy_id;	/* index of SP for kernel */
};

typedef int	sp_attributes_size_t;

#define SP_ATTRIBUTES_NULL	((sp_attributes_t) 0)

/**********
 *
 * Scheduling Information
 *
 **********/

/*** ??? typedef void	*sp_info_t; ***/
typedef int	sp_info_size_t;

#define SP_INFO_NULL		((sp_info_t) 0)

/*
 * maximum number of scheduling policies that the Scheduling Framework
 * will host (picked arbitrarily)
 */
#define MAX_SCHED_POLS		( 10 )

/**********
 *
 * Scheduling Framework Interfaces
 *
 **********/

/* Initialize Framework and selected policies */
void		sf_init(void);

/* [En/Dis]able particular run queue headers on a processor [set] */

/* Lock run queues, update mask, unlock run queues.  If enabling, check preemption */
sf_return_t	(*sf_queue_enable)(
		    run_queue_t		runq,
		    sf_priority_mask	*mask);

sf_return_t	(*sf_queue_disable)(
		    run_queue_t		runq,
		    sf_priority_mask	*mask);

/* Lock run queues, insert thread at head, unlock run queues and preempt (if possible) */
sf_return_t	(*sf_queue_preempt)(
		    meta_priority_t	priority,
		    thread_t		thread,
		    run_queue_t		run_queues);

/* Lock run queues, add thread to tail, unlock run queues and preempt if appropriate */
sf_return_t	(*sf_queue_add_preempt)(
		    meta_priority_t	priority,
		    thread_t		thread,
		    run_queue_t		run_queues);

/* Lock run queues, add thread to tail, unlock queues but don't check for preemption */
sf_return_t	(*sf_queue_add_only)(
		    meta_priority_t	priority,
		    thread_t		thread,
		    run_queue_t		run_queues);

/* Lock run queues, remove thread entry, unlock run queues */
sf_return_t	(*sf_queue_remove)(
		    thread_t		thread);

/**********
 *
 * Scheduling Policy Interfaces
 *
 **********/

/*
 * Operation list for scheduling policies.  (Modeled after the
 * device operations `.../mach_kernel/device/conf.h.')
 *
 * Key to some abbreviations:
 *     sp = scheduling policy
 *     sf = scheduling framework
 */
typedef struct sched_policy_ops {
    /* Initialize an instance of a scheduling policy */
    sf_return_t	(*sp_init)(
		sf_object_t			policy,
		int					policy_id,
		int					priority_mask_length,
		sf_priority_mask	*priority_mask);

    /* Enable/disable a scheduling policy on a processor [set] */
    sf_return_t	(*sp_enable_processor_set)(
		sf_object_t			policy,
		processor_set_t		processor_set);

    sf_return_t	(*sp_disable_processor_set)(
		sf_object_t			policy,
		processor_set_t		processor_set);

    sf_return_t	(*sp_enable_processor)(
		sf_object_t			policy,
		processor_t			processor);

    sf_return_t	(*sp_disable_processor)(
		sf_object_t			policy,
		processor_t			processor);

    /* Allow the policy to update the meta-priority of a running thread */
    sf_return_t	(*sp_thread_update_mpri)(
		sf_object_t			policy,
		sched_thread_t		thread);

    /* Notify the policy that a thread has become runnable */
    sf_return_t	(*sp_thread_unblock)(
		sf_object_t			policy,
		sched_thread_t		thread);

    /* Notify the policy that the current thread is done */
    /*** ??? Should this call take a `reason' argument? ***/
    sf_return_t	(*sp_thread_done)(
		sf_object_t			policy,
		sched_thread_t		old_thread);

    /* Notify the policy that a new thread has been selected to run */
    sf_return_t	(*sp_thread_begin)(
		sf_object_t			policy,
		sched_thread_t		new_thread);

    /* Notify the policy that an old thread is ready to be requeued */
    sf_return_t	(*sp_thread_dispatch)(
		sf_object_t			policy,
		sched_thread_t		old_thread);

    /* Attach/detach a thread from the scheduling policy */
    sf_return_t	(*sp_thread_attach)(
		sf_object_t			policy,
		sched_thread_t		thread);

    sf_return_t	(*sp_thread_detach)(
		sf_object_t			policy,
		sched_thread_t		thread);

    /* Set the thread's processor [set] */
    sf_return_t	(*sp_thread_processor)(
		sf_object_t			policy,
		sched_thread_t		*thread,
		processor_t			processor);

    sf_return_t	(*sp_thread_processor_set)(
		sf_object_t			policy,
		sched_thread_t		thread,
		processor_set_t		processor_set);

    /* Set/get a thread's scheduling attributes */
    sf_return_t	(*sp_thread_set)(
		sf_object_t			policy,
		sched_thread_t		thread,
		sp_attributes_t		policy_attributes);

    sf_return_t	(*sp_thread_get)(
		sf_object_t				policy,
		sched_thread_t			thread,
		sp_attributes_t			policy_attributes,
		sp_attributes_size_t	size);

    /* Print scheduling info for a given thread */
    int	(*sp_db_print_sched_stats)(
		sf_object_t			policy,
		sched_thread_t		thread);

    /***
     *** ??? Hopefully, many of the following operations are only
     *** temporary.  Consequently, they haven't been forced to take
     *** the same form as the others just yet.  That should happen
     *** for all of those that end up being permanent additions to the
     *** list of standard operations.
     ***/

    /* `swtch_pri()' routine -- attempt to give up processor */
    void (*sp_swtch_pri)(
		sf_object_t			policy,
		int					pri);

    /* `thread_switch()' routine -- context switch w/ optional hint */
    kern_return_t (*sp_thread_switch)(
		sf_object_t			policy,
		thread_act_t		hint_act,
		int					option,
		mach_msg_timeout_t	option_time);

    /* `thread_depress_abort()' routine -- prematurely abort depression */
    kern_return_t (*sp_thread_depress_abort)(
		sf_object_t			policy,
		sched_thread_t		thread);

    /* `thread_depress_timeout()' routine -- timeout on depression */
    void	(*sp_thread_depress_timeout)(
		sf_object_t			policy,
		thread_t			thread);

    /* `task_attach()' routine -- associate task with scheduling policy */
    sf_return_t (*sp_task_attach)(
		sf_object_t			policy,
		task_t				task);

    /* `task_detach()' routine -- disassociate task from scheduling policy */
    sf_return_t (*sp_task_detach)(
		sf_object_t			policy,
		task_t				task);

    /* `task_policy()' routine -- set scheduling policy and parameters */
    kern_return_t (*sp_task_policy)(
		sf_object_t				policy,
		task_t					task,
		policy_t				policy_id,
		policy_base_t			base,
		mach_msg_type_number_t	count,
		boolean_t				set_limit,
		boolean_t				change,
		policy_limit_t			*limit_ptr,
		int						*lc_ptr);

    /* `task_set_policy()' routine -- set scheduling policy and parameters */
    kern_return_t (*sp_task_set_policy)(
		sf_object_t				policy,
		task_t					task,
		processor_set_t			pset,
		policy_t				policy_id,
		policy_base_t			base,
		mach_msg_type_number_t	base_count,
		policy_limit_t			limit,
		mach_msg_type_number_t	limit_count,
		boolean_t				change);

    kern_return_t (*sp_task_set_sched)(
		sf_object_t				policy,
		task_t					task,
		policy_t				policy_id,
		sched_attr_t			sched_attr,
		mach_msg_type_number_t	sched_attrCnt,
		boolean_t				set_limit,
		boolean_t				change);

    kern_return_t (*sp_task_get_sched)(
		sf_object_t				policy,
		task_t					task,
		policy_t				policy_id,
		sched_attr_t			sched_attr,
		mach_msg_type_number_t	sched_attrCnt,
		mach_msg_type_number_t	sched_attr_size);

    boolean_t (*sp_thread_runnable)(
		sf_object_t			policy,
		sched_thread_t		thread);

    sf_return_t	(*sp_alarm_expired)(
		sf_object_t			policy,
		long				alarm_seqno,
		kern_return_t		result,
		int					alarm_type,
		mach_timespec_t		wakeup_time,
		void				*alarm_data);

} sp_ops_t;

/**********
 *
 * Scheduling Policy
 *
 **********/

typedef struct sf_policy {
	int					policy_id;				/* policy number */
	int					priority_mask_length;	/* length of a priority mask */
	sf_priority_mask	priority_mask;			/* mask for this policy */

	/***
	 *** ??? does this have to be so general?  why not just
	 *** have room in this struct for the priority mask?  then
	 *** there is no need for an allocation and less indirection
	 *** is required for normal access.
	 ***/

	/*
	 * This is the size of the scheduling parameters
	 * data structure for this scheduling policy, in
	 * bytes.
	 *
	 * These are the parameters that are actually
	 * communicated -- and meaningful -- outside
	 * the policy.  (This is as opposed to the
	 * internal state, which has some overlap with
	 * the scheduling parameters.  The size of the
	 * internal state is given by `sched_info_size'
	 * in this structure.)
	 *
	 * By convention, the first field in the sched
	 * parameters structure is the scheduling policy
	 * ID.  This makes the parameters more readily
	 * self-describing.
	 */
	sp_attributes_size_t	sched_attributes_size;

	/*
	 * This is the size of the scheduling policy's
	 * internal state that describes the state of a
	 * thread, in bytes.
	 */
	sp_info_size_t			sched_info_size;

	/*
	 * policy-specific versions
	 * of standard sched ops
	 */
	sp_ops_t				sp_ops;
} sched_policy_t;

#define SCHED_POLICY_NULL	((sched_policy_t *) 0)

#define policy_id_to_sched_policy(policy_id)		\
	(((policy_id) != POLICY_NULL)?					\
			&sched_policy[(policy_id)] : SCHED_POLICY_NULL)

/*
 * Declaration for array of scheduling policies known to kernel
 */
extern sched_policy_t	sched_policy[MAX_SCHED_POLS];

/*
 * Maximum sizes for scheduling-policy-specific information and 
 * attributes, calculated dynamically based on the scheduling
 * policies that were initialized at system startup time.
 */
extern	sp_info_size_t		max_sched_info_size;
extern 	sp_attributes_size_t 	max_sched_attributes_size;

#endif	/* _KERN_SF_H_ */
