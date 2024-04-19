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
 */
/* 
 * Mach Operating System
 * Copyright (c) 1991,1990,1989,1988 Carnegie Mellon University
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
 *	processor.c: processor and processor_set manipulation routines.
 */

#include <cpus.h>
#include <mach_host.h>

#include <mach/boolean.h>
#include <mach/policy.h>
#include <mach/processor_info.h>
#include <mach/vm_param.h>
#include <kern/cpu_number.h>
#include <kern/host.h>
#include <kern/machine.h>
#include <kern/misc_protos.h>
#include <kern/processor.h>
#include <kern/sched.h>
#include <kern/task.h>
#include <kern/thread.h>
#include <kern/ipc_host.h>
#include <kern/ipc_tt.h>
#include <ipc/ipc_port.h>
#include <kern/kalloc.h>
#include <kern/rtmalloc.h>

#if	MACH_HOST
#include <kern/zalloc.h>
zone_t	pset_zone;
#endif	/* MACH_HOST */

#include <kern/sf.h>
#include <kern/mk_sp.h>	/*** ??? fix so this can be removed ***/

/*
 * Exported interface
 */
#include <mach/mach_host_server.h>

/*
 *	Exported variables.
 */
struct processor_set default_pset;
struct processor processor_array[NCPUS];

queue_head_t		all_psets;
int			all_psets_count;
decl_mutex_data(,	all_psets_lock)

processor_t	master_processor;
processor_t	processor_ptr[NCPUS];

/* Forwards */
void	pset_init(
		processor_set_t	pset);

void	processor_init(
		register processor_t	pr,
		int			slot_num);

void	quantum_set(
		processor_set_t		pset);

kern_return_t	processor_set_base(
		processor_set_t 	pset,
		policy_t             	policy,
	        policy_base_t           base,
		boolean_t       	change);

kern_return_t	processor_set_limit(
		processor_set_t 	pset,
		policy_t		policy,
	        policy_limit_t    	limit,
		boolean_t       	change);

kern_return_t	processor_set_things(
		processor_set_t		pset,
		mach_port_t		**thing_list,
		mach_msg_type_number_t	*count,
		int			type);


/*
 *	Bootstrap the processor/pset system so the scheduler can run.
 */
void
pset_sys_bootstrap(void)
{
	register int	i;

	pset_init(&default_pset);
	default_pset.empty = FALSE;
	for (i = 0; i < NCPUS; i++) {
		/*
		 *	Initialize processor data structures.
		 *	Note that cpu_to_processor(i) is processor_ptr[i].
		 */
		processor_ptr[i] = &processor_array[i];
		processor_init(processor_ptr[i], i);
	}
	master_processor = cpu_to_processor(master_cpu);
	queue_init(&all_psets);
	mutex_init(&all_psets_lock, ETAP_THREAD_PSET_ALL);
	queue_enter(&all_psets, &default_pset, processor_set_t, all_psets);
	all_psets_count = 1;
	default_pset.active = TRUE;
	default_pset.empty = FALSE;

	/*
	 *	Note: the default_pset has a max_priority of BASEPRI_USER.
	 *	Internal kernel threads override this in kernel_thread.
	 */
}

#if	MACH_HOST
/*
 *	Rest of pset system initializations.
 */
void
pset_sys_init(void)
{
	register int	i;
	register processor_t	processor;

	/*
	 * Allocate the zone for processor sets.
	 */
	pset_zone = zinit(sizeof(struct processor_set), 128*PAGE_SIZE,
		PAGE_SIZE, "processor sets");

	/*
	 * Give each processor a control port.
	 * The master processor already has one.
	 */
	for (i = 0; i < NCPUS; i++) {
	    processor = cpu_to_processor(i);
	    if (processor != master_processor &&
		machine_slot[i].is_cpu)
	    {
		ipc_processor_init(processor);
		ipc_processor_enable(processor);
	    }
	}
}
#endif	/* MACH_HOST */

/*
 *	Initialize the given processor_set structure.
 */

void pset_init(
	register processor_set_t	pset)
{
	int	i;

	/* setup run-queues */
	simple_lock_init(&pset->runq.lock, ETAP_THREAD_PSET_RUNQ);
	pset->runq.count = 0;
	pset->runq.depress_count = 0;
	for (i = 0; i < NRQBM; i++) {
	    pset->runq.bitmap[i] = 0;
	}
	setbit(MAXPRI - IDLEPRI, pset->runq.bitmap); 
	pset->runq.highq = IDLEPRI;
	for (i = 0; i < NRQS; i++) {
	    queue_init(&(pset->runq.runq[i]));
	}

	queue_init(&pset->idle_queue);
	pset->idle_count = 0;
	simple_lock_init(&pset->idle_lock, ETAP_THREAD_PSET_IDLE);
	queue_init(&pset->processors);
	pset->processor_count = 0;
	pset->empty = TRUE;
	queue_init(&pset->tasks);
	pset->task_count = 0;
	queue_init(&pset->threads);
	pset->thread_count = 0;
	pset->ref_count = 1;
	queue_init(&pset->all_psets);
	pset->active = FALSE;
	mutex_init(&pset->lock, ETAP_THREAD_PSET);
	pset->pset_self = IP_NULL;
	pset->pset_name_self = IP_NULL;
	pset->max_priority = BASEPRI_USER;
	pset->policies = POLICY_TIMESHARE | POLICY_FIFO | POLICY_RR;
	pset->set_quantum = min_quantum;

	pset->quantum_adj_index = 0;
	simple_lock_init(&pset->quantum_adj_lock, ETAP_THREAD_PSET_QUANT);

	for (i = 0; i <= NCPUS; i++) {
	    pset->machine_quantum[i] = min_quantum;
	}

	pset->mach_factor = 0;
	pset->load_average = 0;
	pset->sched_load = SCHED_SCALE;		/* i.e. 1 */

	pset->policy_default = POLICY_TIMESHARE;
	pset->policy_limit.ts.max_priority = BASEPRI_USER;
	pset->policy_limit.rr.max_priority = BASEPRI_USER;
	pset->policy_limit.fifo.max_priority = BASEPRI_USER;
	pset->policy_base.ts.base_priority = BASEPRI_USER;
	pset->policy_base.rr.base_priority = BASEPRI_USER;
	pset->policy_base.rr.quantum = min_quantum;
	pset->policy_base.fifo.base_priority = BASEPRI_USER;
}


/*
 *	Find the pset's maximum priority (limit) for the given policy.
 *	This is for internal use only.
 */

int
pset_max_priority(
	processor_set_t pset,
	policy_t 	policy)
{
	int max = BASEPRI_USER;

	if (invalid_policy(policy) || pset == PROCESSOR_SET_NULL)
		return max;

	switch (policy) {
	case POLICY_TIMESHARE:
	     max = pset->policy_limit.ts.max_priority;
	     break;

	case POLICY_RR:
	     max = pset->policy_limit.rr.max_priority;
	     break;

	case POLICY_FIFO:
	     max = pset->policy_limit.fifo.max_priority;
	     break;
	}

	return max;
}

/*
 *	Find the pset's base priority for the given policy. This is
 *	for internal use only.
 */

int
pset_base_priority(
	processor_set_t pset,
	policy_t 	policy)
{
	int base = BASEPRI_USER;

	if (invalid_policy(policy) || pset == PROCESSOR_SET_NULL)
		return base;

	switch (policy) {
	case POLICY_TIMESHARE:
	     base = pset->policy_base.ts.base_priority;
	     break;

	case POLICY_RR:
	     base = pset->policy_base.rr.base_priority;
	     break;

	case POLICY_FIFO:
	     base = pset->policy_base.fifo.base_priority;
	     break;
	}

	return base;
}

/*
 *	Find the pset's sched data for the given policy. This is
 *	for internal use only.
 */

int
pset_sched_data(
	processor_set_t pset,
	policy_t 	policy)
{
	int temp, data = 0;

	if (invalid_policy(policy) || pset == PROCESSOR_SET_NULL)
		return data;

	switch (policy) {
	case POLICY_TIMESHARE:
	     data = 0;
	     break;

	case POLICY_RR:
	     temp = pset->policy_base.rr.quantum * 1000;
             if (temp % tick)
             	temp += tick;
             data = temp/tick;
	     break;

	case POLICY_FIFO:
	     data = 0;
	     break;
	}

	return data;
}

/*
 *	Initialize the given processor structure for the processor in
 *	the slot specified by slot_num.
 */

void
processor_init(
	register processor_t	pr,
	int			slot_num)
{
	int	i;

	/* setup run-queues */
	simple_lock_init(&pr->runq.lock, ETAP_THREAD_PROC_RUNQ);
	pr->runq.count = 0;
	pr->runq.depress_count = 0;
	for (i = 0; i < NRQBM; i++) {
	    pr->runq.bitmap[i] = 0;
	}
	setbit(MAXPRI - IDLEPRI, pr->runq.bitmap); 
	pr->runq.highq = IDLEPRI;
	for (i = 0; i < NRQS; i++) {
	    queue_init(&(pr->runq.runq[i]));
	}

	queue_init(&pr->processor_queue);
	pr->state = PROCESSOR_OFF_LINE;
	pr->next_thread = THREAD_NULL;
	pr->idle_thread = THREAD_NULL;
	pr->quantum = 0;
	pr->first_quantum = FALSE;
	pr->last_quantum = 0;
	pr->processor_set = PROCESSOR_SET_NULL;
	pr->processor_set_next = PROCESSOR_SET_NULL;
	queue_init(&pr->processors);
	simple_lock_init(&pr->lock, ETAP_THREAD_PROC);
	pr->processor_self = IP_NULL;
	pr->slot_num = slot_num;
}

/*
 *	pset_remove_processor() removes a processor from a processor_set.
 *	It can only be called on the current processor.  Caller must
 *	hold lock on current processor and processor set.
 */

void
pset_remove_processor(
	processor_set_t	pset,
	processor_t	processor)
{
	if (pset != processor->processor_set)
		panic("pset_remove_processor: wrong pset");

	queue_remove(&pset->processors, processor, processor_t, processors);
	processor->processor_set = PROCESSOR_SET_NULL;
	pset->processor_count--;
	quantum_set(pset);
}

/*
 *	pset_add_processor() adds a  processor to a processor_set.
 *	It can only be called on the current processor.  Caller must
 *	hold lock on curent processor and on pset.  No reference counting on
 *	processors.  Processor reference to pset is implicit.
 */

void
pset_add_processor(
	processor_set_t	pset,
	processor_t	processor)
{
	queue_enter(&pset->processors, processor, processor_t, processors);
	processor->processor_set = pset;
	pset->processor_count++;
	quantum_set(pset);
}

/*
 *	pset_remove_task() removes a task from a processor_set.
 *	Caller must hold locks on pset and task.  Pset reference count
 *	is not decremented; caller must explicitly pset_deallocate.
 */

void
pset_remove_task(
	processor_set_t	pset,
	task_t		task)
{
	if (pset != task->processor_set)
		return;

	queue_remove(&pset->tasks, task, task_t, pset_tasks);
	task->processor_set = PROCESSOR_SET_NULL;
	pset->task_count--;
}

/*
 *	pset_add_task() adds a  task to a processor_set.
 *	Caller must hold locks on pset and task.  Pset references to
 *	tasks are implicit.
 */

void
pset_add_task(
	processor_set_t	pset,
	task_t		task)
{
	queue_enter(&pset->tasks, task, task_t, pset_tasks);
	task->processor_set = pset;
	pset->task_count++;
	pset->ref_count++;
}

/*
 *	pset_remove_thread() removes a thread from a processor_set.
 *	Caller must hold locks on pset and thread.  Pset reference count
 *	is not decremented; caller must explicitly pset_deallocate.
 */

void
pset_remove_thread(
	processor_set_t	pset,
	thread_t	thread)
{
	queue_remove(&pset->threads, thread, thread_t, pset_threads);
	thread->processor_set = PROCESSOR_SET_NULL;
	pset->thread_count--;
}

/*
 *	pset_add_thread() adds a  thread to a processor_set.
 *	Caller must hold locks on pset and thread.  Pset references to
 *	threads are implicit.
 */

void
pset_add_thread(
	processor_set_t	pset,
	thread_t	thread)
{
	queue_enter(&pset->threads, thread, thread_t, pset_threads);
	thread->processor_set = pset;
	pset->thread_count++;
	pset->ref_count++;
}

/*
 *	thread_change_psets() changes the pset of a thread.  Caller must
 *	hold locks on both psets and thread.  The old pset must be
 *	explicitly pset_deallocat()'ed by caller.
 */

void
thread_change_psets(
	thread_t	thread,
	processor_set_t old_pset,
	processor_set_t new_pset)
{
	queue_remove(&old_pset->threads, thread, thread_t, pset_threads);
	old_pset->thread_count--;
	queue_enter(&new_pset->threads, thread, thread_t, pset_threads);
	thread->processor_set = new_pset;
	new_pset->thread_count++;
	new_pset->ref_count++;
}	

/*
 *	pset_deallocate:
 *
 *	Remove one reference to the processor set.  Destroy processor_set
 *	if this was the last reference.
 */

void
pset_deallocate(
	processor_set_t	pset)
{
	if (pset == PROCESSOR_SET_NULL)
		return;

	pset_lock(pset);
	if (--pset->ref_count > 0) {
		pset_unlock(pset);
		return;
	}
#if	!MACH_HOST
	panic("pset_deallocate: default_pset destroyed");
#endif	/* !MACH_HOST */
#if	MACH_HOST
	/*
	 *	Reference count is zero, however the all_psets list
	 *	holds an implicit reference and may make new ones.
	 *	Its lock also dominates the pset lock.  To check for this,
	 *	temporarily restore one reference, and then lock the
	 *	other structures in the right order.
	 */
	pset->ref_count = 1;
	pset_unlock(pset);
	
	mutex_lock(&all_psets_lock);
	pset_lock(pset);
	if (--pset->ref_count > 0) {
		/*
		 *	Made an extra reference.
		 */
		pset_unlock(pset);
		mutex_unlock(&all_psets_lock);
		return;
	}

	/*
	 *	Ok to destroy pset.  Make a few paranoia checks.
	 */

	if ((pset == &default_pset) || (pset->thread_count > 0) ||
	    (pset->task_count > 0) || pset->processor_count > 0) {
		panic("pset_deallocate: destroy default or active pset");
	}
	/*
	 *	Remove from all_psets queue.
	 */
	queue_remove(&all_psets, pset, processor_set_t, all_psets);
	all_psets_count--;

	pset_unlock(pset);
	mutex_unlock(&all_psets_lock);

	/*
	 *	That's it, free data structure.
	 */
	zfree(pset_zone, (vm_offset_t)pset);
#endif	/* MACH_HOST */
}

/*
 *	pset_reference:
 *
 *	Add one reference to the processor set.
 */

void
pset_reference(
	processor_set_t	pset)
{
	pset_lock(pset);
	pset->ref_count++;
	pset_unlock(pset);
}

kern_return_t
processor_info(
	register processor_t	processor,
	processor_flavor_t	flavor,
	host_t			*host,
	processor_info_t	info,
	mach_msg_type_number_t	*count)
{
	register int	i, slot_num, state;
	register processor_basic_info_t		basic_info;
	register processor_cpu_load_info_t	cpu_load_info;
	kern_return_t   kr;

	if (processor == PROCESSOR_NULL)
		return(KERN_INVALID_ARGUMENT);

	slot_num = processor->slot_num;

	switch (flavor) {

	case PROCESSOR_BASIC_INFO:
	  {
	    if (*count < PROCESSOR_BASIC_INFO_COUNT)
	      return(KERN_FAILURE);

	    basic_info = (processor_basic_info_t) info;
	    basic_info->cpu_type = machine_slot[slot_num].cpu_type;
	    basic_info->cpu_subtype = machine_slot[slot_num].cpu_subtype;
	    state = processor->state;
	    if (state == PROCESSOR_SHUTDOWN || state == PROCESSOR_OFF_LINE)
	      basic_info->running = FALSE;
	    else
	      basic_info->running = TRUE;
	    basic_info->slot_num = slot_num;
	    if (processor == master_processor) 
	      basic_info->is_master = TRUE;
	    else
	      basic_info->is_master = FALSE;

	    *count = PROCESSOR_BASIC_INFO_COUNT;
	    *host = &realhost;
	    return(KERN_SUCCESS);
	  }
	case PROCESSOR_CPU_LOAD_INFO:
	  {
	    if (*count < PROCESSOR_CPU_LOAD_INFO_COUNT)
	      return(KERN_FAILURE);

	    cpu_load_info = (processor_cpu_load_info_t) info;
	    for (i=0;i<CPU_STATE_MAX;i++)
	      cpu_load_info->cpu_ticks[i] = machine_slot[slot_num].cpu_ticks[i];

	    *count = PROCESSOR_CPU_LOAD_INFO_COUNT;
	    *host = &realhost;
	    return(KERN_SUCCESS);
	  }
	default:
	  {
	    kr=cpu_info(flavor, slot_num, info, count);
	    if (kr == KERN_SUCCESS)
		*host = &realhost;		   
	    return(kr);
	  }
	}
}

kern_return_t
processor_start(
	processor_t	processor)
{
    	if (processor == PROCESSOR_NULL)
		return(KERN_INVALID_ARGUMENT);

	/* TODO: Test if the target processor has already been started */
	/* TODO: boot_processor : for now, assume cpu 0 */
	if (processor->slot_num != 0)
	{
		thread_t	thread;   
		sched_policy_t	*policy;
		sf_return_t	sfr;
		spl_t	s;
		extern void start_cpu_thread(void);
	
		thread = kernel_thread_with_attributes( kernel_task, SP_ATTRIBUTES_NULL, start_cpu_thread, FALSE);   
		s = splsched();
		thread_lock(thread);
		thread_bind_locked(thread, processor);
		processor->next_thread = thread;
		thread->state |= TH_RUN;
		/*(void) thread_resume(thread->top_act);*/

		/* get pointer to scheduling policy "object" */
		policy = &sched_policy[thread->policy];

		sfr = policy->sp_ops.sp_thread_unblock(policy, thread);
		assert(sfr == SF_SUCCESS);
		thread_unlock(thread);
		splx(s);
	}

	return(cpu_start(processor->slot_num));
}

kern_return_t
processor_exit(
	processor_t	processor)
{
	if (processor == PROCESSOR_NULL)
		return(KERN_INVALID_ARGUMENT);

	return(processor_shutdown(processor));
}

kern_return_t
processor_control(
	processor_t		processor,
	processor_info_t	info,
	mach_msg_type_number_t	count)
{
	if (processor == PROCESSOR_NULL)
		return(KERN_INVALID_ARGUMENT);

	return(cpu_control(processor->slot_num, info, count));
}

/*
 *	Precalculate the appropriate system quanta based on load.  The
 *	index into machine_quantum is the number of threads on the
 *	processor set queue.  It is limited to the number of processors in
 *	the set.
 */

void
quantum_set(
	processor_set_t		pset)
{
#if NCPUS > 1
	register int    i, ncpus;

	ncpus = pset->processor_count;

	for (i=1; i <= ncpus; i++)
		pset->machine_quantum[i] = ((min_quantum * ncpus) + (i / 2)) / i ;

	pset->machine_quantum[0] = pset->machine_quantum[1];

	i = (pset->runq.count > ncpus) ? ncpus : pset->runq.count;
	pset->set_quantum = pset->machine_quantum[i];
#else   /* NCPUS > 1 */
	default_pset.set_quantum = min_quantum;
#endif  /* NCPUS > 1 */
}

#if	MACH_HOST
/*
 *	processor_set_create:
 *
 *	Create and return a new processor set.
 */

kern_return_t
processor_set_create(
	host_t	host,
	processor_set_t *new_set,
	processor_set_t *new_name)
{
	processor_set_t	pset;

	if (host == HOST_NULL)
		return(KERN_INVALID_ARGUMENT);
	pset = (processor_set_t) zalloc(pset_zone);
	pset_init(pset);
	pset_reference(pset);	/* for new_set out argument */
	pset_reference(pset);	/* for new_name out argument */
	ipc_pset_init(pset);
	pset->active = TRUE;

	mutex_lock(&all_psets_lock);
	queue_enter(&all_psets, pset, processor_set_t, all_psets);
	all_psets_count++;
	mutex_unlock(&all_psets_lock);

	ipc_pset_enable(pset);

	*new_set = pset;
	*new_name = pset;
	return(KERN_SUCCESS);
}

/*
 *	processor_set_destroy:
 *
 *	destroy a processor set.  Any tasks, threads or processors
 *	currently assigned to it are reassigned to the default pset.
 */
kern_return_t
processor_set_destroy(
	processor_set_t	pset)
{
	register queue_entry_t	elem;
	register queue_head_t	*list;

	if (pset == PROCESSOR_SET_NULL || pset == &default_pset)
		return(KERN_INVALID_ARGUMENT);

	/*
	 *	Handle multiple termination race.  First one through sets
	 *	active to FALSE and disables ipc access.
	 */
	pset_lock(pset);
	if (!(pset->active)) {
		pset_unlock(pset);
		return(KERN_FAILURE);
	}

	pset->active = FALSE;
	ipc_pset_disable(pset);


	/*
	 *	Now reassign everything in this set to the default set.
	 */

	if (pset->task_count > 0) {
	    list = &pset->tasks;
	    while (!queue_empty(list)) {
		elem = queue_first(list);
		task_reference((task_t) elem);
		pset_unlock(pset);
		task_assign((task_t) elem, &default_pset, FALSE);
		task_deallocate((task_t) elem);
		pset_lock(pset);
	    }
	}

	if (pset->thread_count > 0) {
	    list = &pset->threads;
	    while (!queue_empty(list)) {
		thread_t	thread;
		thread_act_t	thr_act;

		elem = queue_first(list);
		thread = (thread_t)elem;
		/*
		 * Get top activation of thread safely.
		 */
		thr_act = thread_lock_act(thread);
		/*
		 * Inline thread_assign() here to avoid silly double
		 * locking.
		 */
		thread_freeze(thread);
		thread_doassign(thread, &default_pset, TRUE);
		/*
		 * All thread-related locks released at this point.
		 */
		pset_lock(pset);
	    }
	}
	
	if (pset->processor_count > 0) {
	    list = &pset->processors;
	    while(!queue_empty(list)) {
		elem = queue_first(list);
		pset_unlock(pset);
		processor_assign((processor_t) elem, &default_pset, TRUE);
		pset_lock(pset);
	    }
	}

	pset_unlock(pset);

	/*
	 *	Destroy ipc state.
	 */
	ipc_pset_terminate(pset);

	/*
	 *	Deallocate pset's reference to itself.
	 */
	pset_deallocate(pset);
	return(KERN_SUCCESS);
}

#else	/* MACH_HOST */
	    
kern_return_t
processor_set_create(
	host_t	host,
	processor_set_t	*new_set,
	processor_set_t	*new_name)
{
#ifdef	lint
	host++; new_set++; new_name++;
#endif	/* lint */
	return(KERN_FAILURE);
}

kern_return_t
processor_set_destroy(
	processor_set_t	pset)
{
#ifdef	lint
	pset++;
#endif	/* lint */
	return(KERN_FAILURE);
}

#endif	/* MACH_HOST */

kern_return_t
processor_get_assignment(
	processor_t	processor,
	processor_set_t	*pset)
{
    	int state;

	state = processor->state;
	if (state == PROCESSOR_SHUTDOWN || state == PROCESSOR_OFF_LINE)
		return(KERN_FAILURE);

	*pset = processor->processor_set;
	pset_reference(*pset);
	return(KERN_SUCCESS);
}

kern_return_t
processor_set_info(
	processor_set_t		pset,
	int			flavor,
	host_t			*host,
	processor_set_info_t	info,
	mach_msg_type_number_t	*count)
{
	if (pset == PROCESSOR_SET_NULL)
		return(KERN_INVALID_ARGUMENT);

	if (flavor == PROCESSOR_SET_BASIC_INFO) {
		register processor_set_basic_info_t	basic_info;

		if (*count < PROCESSOR_SET_BASIC_INFO_COUNT)
			return(KERN_FAILURE);

		basic_info = (processor_set_basic_info_t) info;

		pset_lock(pset);
		basic_info->processor_count = pset->processor_count;
		basic_info->default_policy = pset->policy_default;
		pset_unlock(pset);

		*count = PROCESSOR_SET_BASIC_INFO_COUNT;
		*host = &realhost;
		return(KERN_SUCCESS);
	}
	else if (flavor == PROCESSOR_SET_TIMESHARE_DEFAULT) {
		register policy_timeshare_base_t	ts_base;

		if (*count < POLICY_TIMESHARE_BASE_COUNT)
			return(KERN_FAILURE);

		ts_base = (policy_timeshare_base_t) info;

		pset_lock(pset);
		*ts_base = pset->policy_base.ts;
		pset_unlock(pset);

		*count = POLICY_TIMESHARE_BASE_COUNT;
		*host = &realhost;
		return(KERN_SUCCESS);
	}
	else if (flavor == PROCESSOR_SET_FIFO_DEFAULT) {
		register policy_fifo_base_t		fifo_base;

		if (*count < POLICY_FIFO_BASE_COUNT)
			return(KERN_FAILURE);

		fifo_base = (policy_fifo_base_t) info;

		pset_lock(pset);
		*fifo_base = pset->policy_base.fifo;
		pset_unlock(pset);

		*count = POLICY_FIFO_BASE_COUNT;
		*host = &realhost;
		return(KERN_SUCCESS);
	}
	else if (flavor == PROCESSOR_SET_RR_DEFAULT) {
		register policy_rr_base_t		rr_base;

		if (*count < POLICY_RR_BASE_COUNT)
			return(KERN_FAILURE);

		rr_base = (policy_rr_base_t) info;

		pset_lock(pset);
		*rr_base = pset->policy_base.rr;
		pset_unlock(pset);

		*count = POLICY_RR_BASE_COUNT;
		*host = &realhost;
		return(KERN_SUCCESS);
	}
	else if (flavor == PROCESSOR_SET_TIMESHARE_LIMITS) {
		register policy_timeshare_limit_t	ts_limit;

		if (*count < POLICY_TIMESHARE_LIMIT_COUNT)
			return(KERN_FAILURE);

		ts_limit = (policy_timeshare_limit_t) info;

		pset_lock(pset);
		*ts_limit = pset->policy_limit.ts;
		pset_unlock(pset);

		*count = POLICY_TIMESHARE_LIMIT_COUNT;
		*host = &realhost;
		return(KERN_SUCCESS);
	}
	else if (flavor == PROCESSOR_SET_FIFO_LIMITS) {
		register policy_fifo_limit_t		fifo_limit;

		if (*count < POLICY_FIFO_LIMIT_COUNT)
			return(KERN_FAILURE);

		fifo_limit = (policy_fifo_limit_t) info;

		pset_lock(pset);
		*fifo_limit = pset->policy_limit.fifo;
		pset_unlock(pset);

		*count = POLICY_FIFO_LIMIT_COUNT;
		*host = &realhost;
		return(KERN_SUCCESS);
	}
	else if (flavor == PROCESSOR_SET_RR_LIMITS) {
		register policy_rr_limit_t		rr_limit;

		if (*count < POLICY_RR_LIMIT_COUNT)
			return(KERN_FAILURE);

		rr_limit = (policy_rr_limit_t) info;

		pset_lock(pset);
		*rr_limit = pset->policy_limit.rr;
		pset_unlock(pset);

		*count = POLICY_RR_LIMIT_COUNT;
		*host = &realhost;
		return(KERN_SUCCESS);
	}
	else if (flavor == PROCESSOR_SET_ENABLED_POLICIES) {
		register int				*enabled;

		if (*count < (sizeof(*enabled)/sizeof(int)))
			return(KERN_FAILURE);

		enabled = (int *) info;

		pset_lock(pset);
		*enabled = pset->policies;
		pset_unlock(pset);

		*count = sizeof(*enabled)/sizeof(int);
		*host = &realhost;
		return(KERN_SUCCESS);
	}


	*host = HOST_NULL;
	return(KERN_INVALID_ARGUMENT);
}

/*
 *	processor_set_statistics
 *
 *	Returns scheduling statistics for a processor set. 
 */
kern_return_t 
processor_set_statistics(
	processor_set_t         pset,
	int                     flavor,
	processor_set_info_t    info,
	mach_msg_type_number_t	*count)
{
        if (pset == PROCESSOR_SET_NULL)
                return (KERN_INVALID_PROCESSOR_SET);

        if (flavor == PROCESSOR_SET_LOAD_INFO) {
                register processor_set_load_info_t     load_info;

                if (*count < PROCESSOR_SET_LOAD_INFO_COUNT)
                        return(KERN_FAILURE);

                load_info = (processor_set_load_info_t) info;

                pset_lock(pset);
                load_info->task_count = pset->task_count;
                load_info->thread_count = pset->thread_count;
                load_info->mach_factor = pset->mach_factor;
                load_info->load_average = pset->load_average;
                pset_unlock(pset);

                *count = PROCESSOR_SET_LOAD_INFO_COUNT;
                return(KERN_SUCCESS);
        }

        return(KERN_INVALID_ARGUMENT);
}

/*
 *	processor_set_max_priority:
 *
 *	Specify max priority permitted on processor set.  This affects
 *	newly created and assigned threads.  Optionally change existing
 * 	ones.
 */
kern_return_t
processor_set_max_priority(
	processor_set_t	pset,
	int		max_priority,
	boolean_t	change_threads)
{
	thread_act_t	thr_act;

	if (pset == PROCESSOR_SET_NULL || invalid_pri(max_priority))
		return(KERN_INVALID_ARGUMENT);

	pset_lock(pset);
	pset->max_priority = max_priority;

	if (change_threads) {
	    register queue_head_t *list;
	    register thread_t	thread;

	    list = &pset->threads;
	    thread = (thread_t) queue_first(list);
	    while (!queue_end(list, (queue_entry_t) thread)) {
		thr_act = thread_lock_act(thread);
		/*** ??? fix me ***/
		assert(thread->sp_info != SP_INFO_NULL);
		if (((thread->policy == POLICY_TIMESHARE) ||
		     (thread->policy == POLICY_RR) ||
		     (thread->policy == POLICY_FIFO)) &&
		    (((mk_sp_info_t)thread->sp_info)->max_priority < max_priority))
		{
			thread_max_priority_locked(thread, pset, max_priority);
		}
		thread_unlock_act(thread);
		thread = (thread_t) queue_next(&thread->pset_threads);
	    }
	}

	pset_unlock(pset);

	return(KERN_SUCCESS);
}

/*
 *	processor_set_policy_enable:
 *
 *	Allow indicated policy on processor set.
 */

kern_return_t
processor_set_policy_enable(
	processor_set_t	pset,
	int		policy)
{
	if ((pset == PROCESSOR_SET_NULL) || invalid_policy(policy))
		return(KERN_INVALID_ARGUMENT);

	pset_lock(pset);
	pset->policies |= policy;
	pset_unlock(pset);
	
    	return(KERN_SUCCESS);
}

/*
 *	processor_set_policy_disable:
 *
 *	Forbid indicated policy on processor set.  Time sharing cannot
 *	be forbidden.
 */

kern_return_t
processor_set_policy_disable(
	processor_set_t	pset,
	int		policy,
	boolean_t	change_threads)
{
	thread_act_t	thr_act;

	if ((pset == PROCESSOR_SET_NULL) || policy == POLICY_TIMESHARE ||
	    invalid_policy(policy))
		return(KERN_INVALID_ARGUMENT);

	pset_lock(pset);

	/*
	 *	Check if policy enabled.  Disable if so, then handle
	 *	change_threads.
	 */
	if (pset->policies & policy) {
	    pset->policies &= ~policy;

	    if (change_threads) {
		register queue_head_t		*list;
		register thread_t		thread;
		policy_base_t			base;
		policy_timeshare_base_data_t	ts_data;

		base = (policy_base_t) &ts_data;
		ts_data.base_priority = 0;
		list = &pset->threads;
		thread = (thread_t) queue_first(list);
		while (!queue_end(list, (queue_entry_t) thread)) {
		    thr_act = thread_lock_act(thread);
		    if (thr_act && thread->policy == policy)
			thread_policy(thr_act, POLICY_TIMESHARE, base, 
			              POLICY_TIMESHARE_BASE_COUNT, FALSE);
		    thread_unlock_act(thread);
		    thread = (thread_t) queue_next(&thread->pset_threads);
		}
	    }
	}
	pset_unlock(pset);

	return(KERN_SUCCESS);
}

#define THING_TASK	0
#define THING_THREAD	1

/*
 *	processor_set_things:
 *
 *	Common internals for processor_set_{threads,tasks}
 */
kern_return_t
processor_set_things(
	processor_set_t		pset,
	mach_port_t		**thing_list,
	mach_msg_type_number_t	*count,
	int			type)
{
	unsigned int actual;	/* this many things */
	int i;
	boolean_t rt = FALSE; /* ### This boolean is FALSE, because there
			       * currently exists no mechanism to determine
			       * whether or not the reply port is an RT port
			       */

	vm_size_t size, size_needed;
	vm_offset_t addr;

	if (pset == PROCESSOR_SET_NULL)
		return KERN_INVALID_ARGUMENT;

	size = 0; addr = 0;

	for (;;) {
		pset_lock(pset);
		if (!pset->active) {
			pset_unlock(pset);
			return KERN_FAILURE;
		}

		if (type == THING_TASK)
			actual = pset->task_count;
		else
			actual = pset->thread_count;

		/* do we have the memory we need? */

		size_needed = actual * sizeof(mach_port_t);
		if (size_needed <= size)
			break;

		/* unlock the pset and allocate more memory */
		pset_unlock(pset);

		if (size != 0)
			KFREE(addr, size, rt);

		assert(size_needed > 0);
		size = size_needed;

		addr = KALLOC(size, rt);
		if (addr == 0)
			return KERN_RESOURCE_SHORTAGE;
	}

	/* OK, have memory and the processor_set is locked & active */

	switch (type) {
	    case THING_TASK: {
		task_t *tasks = (task_t *) addr;
		task_t task;

		for (i = 0, task = (task_t) queue_first(&pset->tasks);
		     i < actual;
		     i++, task = (task_t) queue_next(&task->pset_tasks)) {
			/* take ref for convert_task_to_port */
			task_reference(task);
			tasks[i] = task;
		}
		assert(queue_end(&pset->tasks, (queue_entry_t) task));
		break;
	    }

	    case THING_THREAD: {
		thread_act_t *thr_acts = (thread_act_t *) addr;
		thread_t thread;
		thread_act_t thr_act;
	    	queue_head_t *list;

		list = &pset->threads;
	    	thread = (thread_t) queue_first(list);
		i = 0;
	    	while (i < actual && !queue_end(list, (queue_entry_t)thread)) {
		  	thr_act = thread_lock_act(thread);
			if (thr_act && thr_act->ref_count > 0) {
				/* take ref for convert_act_to_port */
				act_locked_act_reference(thr_act);
				thr_acts[i] = thr_act;
				i++;
			}
			thread_unlock_act(thread);
			thread = (thread_t) queue_next(&thread->pset_threads);
		}
		if (i < actual) {
		  	actual = i;
			size_needed = actual * sizeof(mach_port_t);
		}
		break;
	    }
	}

	/* can unlock processor set now that we have the task/thread refs */
	pset_unlock(pset);

	if (actual == 0) {
		/* no things, so return null pointer and deallocate memory */
		*thing_list = 0;
		*count = 0;

		if (size != 0)
			KFREE(addr, size, rt);
	} else {
		/* if we allocated too much, must copy */

		if (size_needed < size) {
			vm_offset_t newaddr;

			newaddr = KALLOC(size_needed, rt);
			if (newaddr == 0) {
				switch (type) {
				    case THING_TASK: {
					task_t *tasks = (task_t *) addr;

					for (i = 0; i < actual; i++)
						task_deallocate(tasks[i]);
					break;
				    }

				    case THING_THREAD: {
					thread_t *threads = (thread_t *) addr;

					for (i = 0; i < actual; i++)
						thread_deallocate(threads[i]);
					break;
				    }
				}
				KFREE(addr, size, rt);
				return KERN_RESOURCE_SHORTAGE;
			}

			bcopy((char *) addr, (char *) newaddr, size_needed);
			KFREE(addr, size, rt);
			addr = newaddr;
		}

		*thing_list = (mach_port_t *) addr;
		*count = actual;

		/* do the conversion that Mig should handle */

		switch (type) {
		    case THING_TASK: {
			task_t *tasks = (task_t *) addr;

			for (i = 0; i < actual; i++)
				(*thing_list)[i] = convert_task_to_port(tasks[i]);
			break;
		    }

		    case THING_THREAD: {
			thread_act_t *thr_acts = (thread_act_t *) addr;

			for (i = 0; i < actual; i++)
			  	(*thing_list)[i] = convert_act_to_port(thr_acts[i]);
			break;
		    }
		}
	}

	return(KERN_SUCCESS);
}


/*
 *	processor_set_tasks:
 *
 *	List all tasks in the processor set.
 */
kern_return_t
processor_set_tasks(
	processor_set_t		pset,
	task_array_t		*task_list,
	mach_msg_type_number_t	*count)
{
    return(processor_set_things(pset, (mach_port_t **)task_list, count, THING_TASK));
}

/*
 *	processor_set_threads:
 *
 *	List all threads in the processor set.
 */
kern_return_t
processor_set_threads(
	processor_set_t		pset,
	thread_array_t		*thread_list,
	mach_msg_type_number_t	*count)
{
    return(processor_set_things(pset, (mach_port_t **)thread_list, count, THING_THREAD));
}

/*
 *      processor_set_base:
 *
 *      Specify per-policy base priority for a processor set.  Set processor
 *	set default policy to the given policy. This affects newly created
 *      and assigned threads.  Optionally change existing ones.
 */
kern_return_t
processor_set_base(
	processor_set_t 	pset,
	policy_t             	policy,
        policy_base_t           base,
	boolean_t       	change)
{
	int				bc, lc;
        policy_limit_t          	limit;
        policy_rr_limit_data_t 		rr_limit;
        policy_fifo_limit_data_t 	fifo_limit;
        policy_timeshare_limit_data_t 	ts_limit;
	kern_return_t			ret = KERN_SUCCESS;

        if (pset == PROCESSOR_SET_NULL)
                return(KERN_INVALID_PROCESSOR_SET);

        pset_lock(pset);

	switch (policy) {
	case POLICY_RR:
	{
		policy_rr_base_t rr_base = (policy_rr_base_t) base;

		if (invalid_pri(rr_base->base_priority)) {
			ret = KERN_INVALID_ARGUMENT;
			break;	
		}
		bc = POLICY_RR_BASE_COUNT;
		lc = POLICY_RR_LIMIT_COUNT;
		pset->policy_base.rr = *rr_base;
		rr_limit.max_priority = rr_base->base_priority;
		limit = (policy_limit_t) &rr_limit;
		break;
	}

	case POLICY_FIFO:
	{
		policy_fifo_base_t fifo_base = (policy_fifo_base_t) base;

		if (invalid_pri(fifo_base->base_priority)) {	
			ret = KERN_INVALID_ARGUMENT;
			break;
		}
		bc = POLICY_FIFO_BASE_COUNT;
		lc = POLICY_FIFO_LIMIT_COUNT;
		pset->policy_base.fifo = *fifo_base;
		fifo_limit.max_priority = fifo_base->base_priority;
                limit = (policy_limit_t) &fifo_limit;
		break;
	}

	case POLICY_TIMESHARE:
	{
		policy_timeshare_base_t ts_base = 
						(policy_timeshare_base_t) base;
		if (invalid_pri(ts_base->base_priority)) {
			ret = KERN_INVALID_ARGUMENT;
			break;	
		}
		bc = POLICY_TIMESHARE_BASE_COUNT;
		lc = POLICY_TIMESHARE_LIMIT_COUNT;
		pset->policy_base.ts = *ts_base;
		ts_limit.max_priority = ts_base->base_priority;
                limit = (policy_limit_t) &ts_limit;
		break;
	}

	default:
		ret = KERN_INVALID_POLICY;
	}

	if (ret != KERN_SUCCESS) {
		pset_unlock(pset);
		return (ret);
	}

        pset->policy_default = policy;


	/*
	 *	When changing the default policy and base priority, set the
	 *	limit priority equal to the base priority.
	 */
        if (change) {
		register queue_head_t *list;
		register task_t task;

            	list = &pset->tasks;
            	task = (task_t) queue_first(list);
            	while (!queue_end(list, (queue_entry_t) task)) {
			task_set_policy(task, pset, policy, base, bc, 
  					limit, lc, TRUE);
                	task = (task_t) queue_next(&task->pset_tasks);
            	}
        }

        pset_unlock(pset);

        return(ret);
}

/*
 *      processor_set_limit:
 *
 *      Specify per-policy limits for a processor set.  This affects
 *      newly created and assigned threads.  Optionally change existing
 *      ones.
 */
kern_return_t
processor_set_limit(
	processor_set_t 	pset,
	policy_t		policy,
        policy_limit_t    	limit,
	boolean_t       	change)
{
	int			max;
        kern_return_t		ret = KERN_SUCCESS;
	thread_act_t		thr_act;

        if (pset == PROCESSOR_SET_NULL)
                return(KERN_INVALID_PROCESSOR_SET);

        pset_lock(pset);

	switch (policy) {
	case POLICY_RR:
	{
                policy_rr_limit_t rr_limit = (policy_rr_limit_t) limit;

                max = rr_limit->max_priority;
		if (invalid_pri(max)) {
			ret = KERN_POLICY_LIMIT;
			break;
		}
		pset->policy_limit.rr = *rr_limit;
		break;
	}

	case POLICY_FIFO:
	{
	 	policy_fifo_limit_t fifo_limit = (policy_fifo_limit_t) limit;

		max = fifo_limit->max_priority;
		if (invalid_pri(max)) {
			ret = KERN_POLICY_LIMIT;
			break;
		}
		pset->policy_limit.fifo = *fifo_limit;
		break;
	}

	case POLICY_TIMESHARE:
	{
                policy_timeshare_limit_t ts_limit =
                                        (policy_timeshare_limit_t) limit;

		max = ts_limit->max_priority;
		if (invalid_pri(max)) {
			ret = KERN_POLICY_LIMIT;
			break;
		}
		pset->policy_limit.ts = *ts_limit;
		break;
	}

	default:
		ret = KERN_INVALID_POLICY;
	}

	if (ret != KERN_SUCCESS) {
		pset_unlock(pset);
		return(ret);
	}

        if (change) {
		register queue_head_t *list;
		register task_t task;
            	register thread_t thread;

		/*
		 *	Only change the policy limits for those tasks and 
		 *	threads whose policy matches our 'policy' variable.
		 */
            	list = &pset->tasks;
            	task = (task_t) queue_first(list);
            	while (!queue_end(list, (queue_entry_t) task)) {
			if (task->policy == policy) 
                        	task_max_priority(task, pset, max);
                	task = (task_t) queue_next(&task->pset_tasks);
            	}

            	list = &pset->threads;
            	thread = (thread_t) queue_first(list);
            	while (!queue_end(list, (queue_entry_t) thread)) {
			thr_act = thread_lock_act(thread);
			if (thr_act && thread->policy == policy) 
                        	thread_max_priority(thr_act, pset, max);
			thread_unlock_act(thread);
                	thread = (thread_t) queue_next(&thread->pset_threads);
            	}
        }

        pset_unlock(pset);

        return(KERN_SUCCESS);
}


/*
 *	New scheduling control interface
 */

/*
 *	processor_set_policy_control
 *
 *	Controls the scheduling attributes governing the processor set.
 *	Allows control of enabled policies, and per-policy base and limit
 *	priorities.
 */
kern_return_t
processor_set_policy_control(
	processor_set_t		pset,
	int			flavor,
	processor_set_info_t	policy_info,
	mach_msg_type_number_t	count,
	boolean_t		change)
{
	policy_t policy, i;
	policy_base_t base;
	policy_limit_t limit;
        kern_return_t ret = KERN_SUCCESS;

	if (pset == PROCESSOR_SET_NULL)
		return (KERN_INVALID_PROCESSOR_SET);

	switch (flavor) {
	case PROCESSOR_SET_ENABLED_POLICIES:
		if (count != sizeof(policy_t))
			return(KERN_INVALID_ARGUMENT);
		policy = (policy_t) *policy_info;
		for (i = POLICY_TIMESHARE ; i <= POLICY_FIFO ; i = i << 1) {
			if (policy & i) {
			    ret = processor_set_policy_enable(pset, i);
			    if (ret != KERN_SUCCESS) return ret;
			} else {
			    ret = processor_set_policy_disable(pset, i, change);
			    if (ret != KERN_SUCCESS) return ret;
			}
		}
		return ret; 

	case PROCESSOR_SET_RR_LIMITS:
		if (count != POLICY_RR_LIMIT_COUNT)
                        return(KERN_INVALID_ARGUMENT);
		limit = (policy_limit_t) policy_info;
		ret = processor_set_limit(pset, POLICY_RR, limit, change);
		return ret;

	case PROCESSOR_SET_FIFO_LIMITS:
		if (count != POLICY_FIFO_LIMIT_COUNT)
                        return(KERN_INVALID_ARGUMENT);
		limit = (policy_limit_t) policy_info;
		ret = processor_set_limit(pset, POLICY_FIFO, limit, change);
		return ret;

	case PROCESSOR_SET_TIMESHARE_LIMITS:
		if (count != POLICY_TIMESHARE_LIMIT_COUNT)
                        return(KERN_INVALID_ARGUMENT);
		limit = (policy_limit_t) policy_info;
		ret = processor_set_limit(pset, POLICY_TIMESHARE, limit,
					     change);
		return ret;

	case PROCESSOR_SET_RR_DEFAULT:
		if (count != POLICY_RR_BASE_COUNT)
                        return(KERN_INVALID_ARGUMENT);
		base = (policy_base_t) policy_info;
		ret = processor_set_base(pset, POLICY_RR, base, change);
		return ret;

	case PROCESSOR_SET_FIFO_DEFAULT:
		if (count != POLICY_FIFO_BASE_COUNT)
                        return(KERN_INVALID_ARGUMENT);
		base = (policy_base_t) policy_info;
		ret = processor_set_base(pset, POLICY_FIFO, base, change);
		return ret;

	case PROCESSOR_SET_TIMESHARE_DEFAULT:
		if (count != POLICY_TIMESHARE_BASE_COUNT)
                        return(KERN_INVALID_ARGUMENT);
		base = (policy_base_t) policy_info;
		ret = processor_set_base(pset, POLICY_TIMESHARE,
					    base, change);
		return ret;

	default:
		return (KERN_INVALID_ARGUMENT);

	}

}
