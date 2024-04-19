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
/*
 *	File:	kern/sync_sema.c
 *	Author:	Joseph CaraDonna
 *
 *	Contains RT distributed semaphore synchronization services.
 */
/*
 *	Semaphores: Rule of Thumb
 *
 *  	A non-negative semaphore count means that the blocked threads queue is
 *  	empty.  A semaphore count of negative n means that the blocked threads
 *  	queue contains n blocked threads.
 */

#include <kern/misc_protos.h>
#include <kern/sync_sema.h>
#include <kern/spl.h>
#include <kern/ipc_kobject.h>
#include <kern/ipc_sync.h>
#include <kern/thread.h>
#include <kern/clock.h>
#include <ipc/ipc_port.h>
#include <ipc/ipc_space.h>
#include <kern/host.h>
#include <kern/wait_queue.h>

unsigned int semaphore_event;
#define SEMAPHORE_EVENT ((event_t)&semaphore_event)

/*
 * forward declaration
 */
void	semaphore_dereference(
			semaphore_t		semaphore);

/*
 *	Routine:	semaphore_create
 *
 *	Creates a semaphore.
 *	The port representing the semaphore is returned as a parameter.
 */
kern_return_t
semaphore_create(
	task_t			task,
	semaphore_t		*new_semaphore,
	int				policy,
	int				value)
{
	semaphore_t		 s = SEMAPHORE_NULL;

	*new_semaphore = SEMAPHORE_NULL;


	if (task == TASK_NULL || value < 0 || policy > SYNC_POLICY_MAX)
		return KERN_INVALID_ARGUMENT;

	s = (semaphore_t) kalloc (sizeof(struct semaphore));

	if (s == SEMAPHORE_NULL)
		return KERN_RESOURCE_SHORTAGE; 

	wait_queue_init(&s->wait_queue, policy); /* also inits lock */
	s->count = value;
	s->ref_count = 1;

	/*
	 *  Create and initialize the semaphore port
	 */
	s->port	= ipc_port_alloc_kernel();
	if (s->port == IP_NULL) {	
		/* This will deallocate the semaphore */	
		semaphore_dereference(s);
		return KERN_RESOURCE_SHORTAGE; 
	}

	ipc_kobject_set (s->port, (ipc_kobject_t) s, IKOT_SEMAPHORE);

	/*
	 *  Associate the new semaphore with the task by adding
	 *  the new semaphore to the task's semaphore list.
	 *
	 *  Associate the task with the new semaphore by having the
	 *  semaphores task pointer point to the owning task's structure.
	 */
	task_lock(task);
	enqueue_head(&task->semaphore_list, (queue_entry_t) s);
	task->semaphores_owned++;
	task_unlock(task);
	s->owner = task;

	/*
	 *  Activate semaphore
	 */
	s->active = TRUE;
	*new_semaphore = s;

	return KERN_SUCCESS;
}		  

/*
 *	Routine:	semaphore_destroy
 *
 *	Destroys a semaphore.  This call will only succeed if the
 *	specified task is the SAME task name specified at the semaphore's
 *	creation.
 *
 *	All threads currently blocked on the semaphore are awoken.  These
 *	threads will return with the KERN_TERMINATED error.
 */
kern_return_t
semaphore_destroy(
	task_t			task,
	semaphore_t		semaphore)
{
	int				old_count;
	thread_t		thread;
	spl_t			spl_level;


	if (task == TASK_NULL || semaphore == SEMAPHORE_NULL)
		return KERN_INVALID_ARGUMENT;

	if (semaphore->owner != task)
		return KERN_INVALID_RIGHT;

	
	spl_level = splsched();
	wait_queue_lock(&semaphore->wait_queue);

	if (!semaphore->active) {
		wait_queue_unlock(&semaphore->wait_queue);
		splx(spl_level);
		return KERN_TERMINATED;
	}

	/*
	 *  Deactivate semaphore
	 */
	semaphore->active = FALSE;

	/*
	 *  Wakeup blocked threads  
	 */
	old_count = semaphore->count;
	semaphore->count = 0;

	if (old_count < 0) {
		wait_queue_wakeup_all_locked(&semaphore->wait_queue,
					     SEMAPHORE_EVENT,
					     THREAD_RESTART,
					     TRUE);		/* unlock? */
	} else {
		wait_queue_unlock(&semaphore->wait_queue);
	}

	/*
	 *  Disown semaphore
	 */
	task_lock(task);
	remqueue(&task->semaphore_list, (queue_entry_t) semaphore);
	task->semaphores_owned--;
	task_unlock(task);

	splx(spl_level);

	/*
	 *  Deallocate
	 *
	 *  Drop the semaphore reference, which inturn deallocates the
	 *  semaphore structure if the reference count goes to zero.
	 */
	ipc_port_dealloc_kernel(semaphore->port);
	semaphore_dereference(semaphore);

	return KERN_SUCCESS;
}

/*
 *	Routine:	semaphore_signal_thread
 *
 *	If the specified thread_act is blocked on the semaphore, it is
 *	woken up.  Otherwise the caller gets KERN_NOT_WAITING and the
 *	semaphore is unchanged.
 */
kern_return_t
semaphore_signal_thread(
	semaphore_t		semaphore,
	thread_act_t	thread_act)
{
	kern_return_t		ret = KERN_NOT_WAITING;
	spl_t			spl_level;

	if (semaphore == SEMAPHORE_NULL)
		return KERN_INVALID_ARGUMENT;

	spl_level = splsched();
	wait_queue_lock(&semaphore->wait_queue);

	if (!semaphore->active) {
		wait_queue_unlock(&semaphore->wait_queue);
		splx(spl_level);
		return KERN_TERMINATED;
	}
	if (semaphore->count < 0) {
		ret = wait_queue_wakeup_thread_locked(&semaphore->wait_queue,
						      SEMAPHORE_EVENT,
						      thread_act->thread,
						      THREAD_AWAKENED,
						      TRUE);  /* unlock? */
	} else {
		wait_queue_unlock(&semaphore->wait_queue);
	}
	splx(spl_level);

	return(ret);
}

/*
 *	Routine:	semaphore_signal
 *
 *	Increments the semaphore count by one.  If any threads are blocked
 *	on the semaphore ONE is woken up.
 */
kern_return_t
semaphore_signal(
	semaphore_t		semaphore)
{
	spl_t			spl_level;

	if (semaphore == SEMAPHORE_NULL)
		return KERN_INVALID_ARGUMENT;

	spl_level = splsched();
	wait_queue_lock(&semaphore->wait_queue);

	if (!semaphore->active) {
		wait_queue_unlock(&semaphore->wait_queue);
		splx(spl_level);
		return KERN_TERMINATED;
	}
	if (semaphore->count < 0) {
		if (wait_queue_wakeup_one_locked(&semaphore->wait_queue,
						 SEMAPHORE_EVENT,
						 THREAD_AWAKENED,
						 FALSE) == KERN_NOT_WAITING) {

			semaphore->count = 1;  /* waiters have busted out */
					       /* becomes a pre-signal. */
		}
	} else {
		semaphore->count++;
	}
	wait_queue_unlock(&semaphore->wait_queue);
	splx(spl_level);

	return KERN_SUCCESS;
}

/*
 *	Routine:	semaphore_signal_all
 *
 *	Awakens ALL threads currently blocked on the semaphore.
 *	The semaphore count returns to zero.
 */
kern_return_t
semaphore_signal_all(
	semaphore_t		semaphore)
{
	int old_count;
	spl_t spl_level;

	if (semaphore == SEMAPHORE_NULL)
		return KERN_INVALID_ARGUMENT;

	spl_level = splsched();
	wait_queue_lock(&semaphore->wait_queue);

	if (!semaphore->active) {
		wait_queue_unlock(&semaphore->wait_queue);
		splx(spl_level);
		return KERN_TERMINATED;
	}

	old_count = semaphore->count;
	semaphore->count = 0;  /* always reset */

	if (old_count < 0) {
		wait_queue_wakeup_all_locked(&semaphore->wait_queue,
					     SEMAPHORE_EVENT,
					     THREAD_AWAKENED,
					     TRUE);		/* unlock? */
	} else {
		wait_queue_unlock(&semaphore->wait_queue);
	}
	splx(spl_level);

	return KERN_SUCCESS;
}


/*
 *	Routine:	semaphore_wait_common
 *
 *  Wait on a semaphore.  If the semaphore has been pre-posted, it just
 *	claims one count, and returns success.  Otherwise, set up to block.
 */
kern_return_t
semaphore_wait_common(
	semaphore_t			semaphore,
	mach_timespec_t 	*wait_time)
{
	AbsoluteTime		abstime, nsinterval;
	int					wait_result;
	spl_t				spl_level;

	if (semaphore == SEMAPHORE_NULL)
		return KERN_INVALID_ARGUMENT;

	if (wait_time != 0) {
		if (BAD_MACH_TIMESPEC(wait_time))
			return KERN_INVALID_VALUE;

		clock_interval_to_absolutetime_interval(
							wait_time->tv_sec, NSEC_PER_SEC, &abstime);
		clock_interval_to_absolutetime_interval(
							wait_time->tv_nsec, 1, &nsinterval);
		ADD_ABSOLUTETIME(&abstime, &nsinterval);
		clock_absolutetime_interval_to_deadline(abstime, &abstime);
	}

 restart:
	spl_level = splsched();
	wait_queue_lock(&semaphore->wait_queue);

	if (!semaphore->active) {
		wait_queue_unlock(&semaphore->wait_queue);
		splx(spl_level);
		return KERN_TERMINATED;
	}

	if (semaphore->count > 0) {
		semaphore->count--;
		wait_queue_unlock(&semaphore->wait_queue);
		splx(spl_level);
		return KERN_SUCCESS;
	}

	if (wait_time != 0 && wait_time->tv_sec == 0 && wait_time->tv_nsec == 0) {
		wait_queue_unlock(&semaphore->wait_queue);
		splx(spl_level);
		return KERN_OPERATION_TIMED_OUT;
	}

	semaphore->count = -1;	/* we don't keep an actual count */

	wait_queue_assert_wait_locked(&semaphore->wait_queue,
				      SEMAPHORE_EVENT,
				      THREAD_ABORTSAFE,
				      TRUE); /* unlock? */
	/* semaphore->wait_queue is unlocked */

	splx(spl_level);

	if (wait_time != 0)
		thread_set_timer_deadline(abstime);

	wait_result = thread_block((void (*)(void))0);
	
	if (wait_time != 0) {
		thread_cancel_timer();
	}

	switch (wait_result) {
		case THREAD_AWAKENED:
			return KERN_SUCCESS;

		case THREAD_TIMED_OUT:
			return KERN_OPERATION_TIMED_OUT;

		case THREAD_INTERRUPTED:
			return KERN_ABORTED;

		case THREAD_RESTART:
			goto restart;

		default:
			panic("semaphore_wait_common\n");
			break;
		}
}


/*
 *	Routine:	semaphore_wait
 *
 *	Decrements the semaphore count by one.  If the count is negative
 *	after the decrement, the calling thread blocks.
 *
 *	Assumes: Never called from interrupt context.
 */
kern_return_t
semaphore_wait(
	semaphore_t			semaphore)
{	
	return (semaphore_wait_common(semaphore, (mach_timespec_t *) 0));
}

/*
 *	Routine:	semaphore_timedwait
 *
 *	Decrements the semaphore count by one.  If the count is negative
 *	after the decrement, the calling thread blocks.  
 *	Sleep for max 'wait_time' (0=>forever)
 *
 *	Assumes: Never called from interrupt context.
 */
kern_return_t
semaphore_timedwait(
	semaphore_t			semaphore,
	mach_timespec_t		wait_time)
{
	return (semaphore_wait_common(semaphore, &wait_time));
}

/*
 *	Routine:	semaphore_reference
 *
 *	Take out a reference on a semaphore.  This keeps the data structure
 *	in existence (but the semaphore may be deactivated).
 */
void
semaphore_reference(
	semaphore_t		semaphore)
{
	spl_t			spl_level;

	spl_level = splsched();
	wait_queue_lock(&semaphore->wait_queue);

	semaphore->ref_count++;

	wait_queue_unlock(&semaphore->wait_queue);
	splx(spl_level);
}

/*
 *	Routine:	semaphore_dereference
 *
 *	Release a reference on a semaphore.  If this is the last reference,
 *	the semaphore data structure is deallocated.
 */
void
semaphore_dereference(
	semaphore_t		semaphore)
{
	int				ref_count;
	spl_t			spl_level;

	if (semaphore != NULL) {
	    spl_level = splsched();
	    wait_queue_lock(&semaphore->wait_queue);

	    ref_count = --(semaphore->ref_count);

	    wait_queue_unlock(&semaphore->wait_queue);
	    splx(spl_level);

	    if (ref_count == 0) {
			assert(wait_queue_empty(&semaphore->wait_queue));
			kfree((vm_offset_t) semaphore, sizeof(struct semaphore));
	    }
	}
}
