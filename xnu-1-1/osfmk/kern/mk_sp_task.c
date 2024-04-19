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
 *** ??? The following files were added when the task system call code
 *** was moved into this file.  They should be integrated with the above
 *** files if these routine stay; or they should be moved elsewhere when
 *** the routines are moved.
 ***/
#include <mach/message.h>

#include <kern/assert.h>
#include <kern/counters.h>
#include <kern/task.h>
#include <kern/sf.h>
#include <kern/mk_sp.h>

/***
 *** ??? The next file supplies the prototypes for `task_set_policy()'
 *** and `task_policy.'  These routines cannot stay here if they are
 *** exported Mach system calls.
 ***/
#include <mach/mach_host_server.h>

sf_return_t
_mk_sp_task_attach(
	sf_object_t		pol,		/* policy */
	task_t			task)
{
	sp_attributes_t		sp_attr;
	sp_attributes_size_t	sched_attributes_size;

	counter(c_mk_sp_task_attach++);

	/* get memory to hold scheduling parameters */
	sched_attributes_size = pol->sched_attributes_size;
	sp_attr = (sp_attributes_t)kalloc(sched_attributes_size);
	if (sp_attr == 0) {
		return(SF_KERN_RESOURCE_SHORTAGE);
	}

	task->policy = pol->policy_id;
	task->sp_attributes = sp_attr;

	return(SF_SUCCESS);
}


sf_return_t
_mk_sp_task_detach(
	sf_object_t		pol,		/* policy */
	task_t			task)
{
	sp_attributes_t		sp_attr;
	sp_attributes_size_t	sched_attributes_size;

	counter(c_mk_sp_task_detach++);

	/* make sure task is associated with this policy */
	if (task->policy != pol->policy_id) return(SF_SUCCESS);

	/* free memory holding scheduling parameters */
	sched_attributes_size = pol->sched_attributes_size;
	assert(task->sp_attributes != SP_ATTRIBUTES_NULL);
	kfree((vm_offset_t) task->sp_attributes, sched_attributes_size);

	task->policy = POLICY_NULL;
	task->sp_attributes = SP_ATTRIBUTES_NULL;

	return(SF_SUCCESS);
}


/*
 *	task_max_priority
 *
 *	Set the maximum priority for a task. This does not affect threads
 *	within the task. Use thread_max_priority() to set their limits.
 */
kern_return_t
task_max_priority(
	task_t		task,
	processor_set_t	pset,
	int		max_priority)
{
	mk_sp_attributes_t	sp_attr;
    	kern_return_t		ret = KERN_SUCCESS;

    	if (task == TASK_NULL || pset == PROCESSOR_SET_NULL ||
			invalid_pri(max_priority))
            return(KERN_INVALID_ARGUMENT);

	task_lock(task);

    	/*
     	 *  Check for wrong processor set.
     	 */
    	if (pset != task->processor_set) {
		task_unlock(task);
		return(KERN_FAILURE);
    	}

	/* get pointer to scheduling policy-specific data */
	assert(task->sp_attributes != SP_ATTRIBUTES_NULL);
	sp_attr = (mk_sp_attributes_t)task->sp_attributes;

        sp_attr->max_priority = max_priority;

        /*
         *      Reset priority if it violates new max priority
         */
        if (max_priority < sp_attr->priority) {
            sp_attr->priority = max_priority;
        }

    	task_unlock(task);

    	return(ret);
}


/*
 * 	task_policy
 *
 *	Set scheduling policy and parameters, both base and limit, for
 *	the given task. Policy must be a policy which is enabled for the
 *	processor set. Change contained threads if requested. 
 */
kern_return_t
_mk_sp_task_policy(
	sf_object_t		pol,
	task_t			task,
        policy_t		policy,
        policy_base_t		base,
	mach_msg_type_number_t	count,
        boolean_t		set_limit,
        boolean_t		change,
	policy_limit_t		*limit_ptr,
	int			*lc_ptr)
{
        policy_rr_limit_data_t		rr_limit;
        policy_fifo_limit_data_t	fifo_limit;
        policy_timeshare_limit_data_t	ts_limit;
        kern_return_t			ret = KERN_SUCCESS;
	mk_sp_attributes_t		sp_attr;

	counter(c_mk_sp_task_policy++);

	/* this routine currently only works with 3 standard MK policies */
	assert((task->policy == POLICY_TIMESHARE)
	       || (task->policy == POLICY_RR)
	       || (task->policy == POLICY_FIFO));

	if (set_limit) {
		/*
	 	 * 	Set scheduling limits to base priority.
		 */
		switch (policy) {
		case POLICY_RR:
		{
			policy_rr_base_t rr_base;

			if (count != POLICY_RR_BASE_COUNT)
				return(KERN_INVALID_ARGUMENT);
			*lc_ptr = POLICY_RR_LIMIT_COUNT;
			rr_base = (policy_rr_base_t) base;
			rr_limit.max_priority = rr_base->base_priority;
			*limit_ptr = (policy_limit_t) &rr_limit;
			break;
		}

		case POLICY_FIFO:
		{
			policy_fifo_base_t fifo_base;

			if (count != POLICY_FIFO_BASE_COUNT)
				return(KERN_INVALID_ARGUMENT);
			*lc_ptr = POLICY_FIFO_LIMIT_COUNT;
			fifo_base = (policy_fifo_base_t) base;
			fifo_limit.max_priority = fifo_base->base_priority;
			*limit_ptr = (policy_limit_t) &fifo_limit;
			break;
		}

		case POLICY_TIMESHARE:
		{
			policy_timeshare_base_t ts_base;

			if (count != POLICY_TIMESHARE_BASE_COUNT)
				return(KERN_INVALID_ARGUMENT);
			*lc_ptr = POLICY_TIMESHARE_LIMIT_COUNT;
			ts_base = (policy_timeshare_base_t) base;
			ts_limit.max_priority = ts_base->base_priority;
			*limit_ptr = (policy_limit_t) &ts_limit;
			break;
		}

		default:
			return(KERN_INVALID_POLICY);
		}

	} else {
		/*
		 *	Use current scheduling limits. Ensure that the
		 *	new base priority will not exceed current limits.
		 */

		/* get pointer to scheduling policy-specific data */
		assert(task->sp_attributes != SP_ATTRIBUTES_NULL);
		sp_attr = (mk_sp_attributes_t)task->sp_attributes;

                switch (policy) {
                case POLICY_RR:
		{
			policy_rr_base_t rr_base;

			if (count != POLICY_RR_BASE_COUNT)
				return(KERN_INVALID_ARGUMENT);
			*lc_ptr = POLICY_RR_LIMIT_COUNT;
			rr_base = (policy_rr_base_t) base;
			if (rr_base->base_priority > sp_attr->max_priority) {
				ret = KERN_POLICY_LIMIT;
				break;
			}
                        rr_limit.max_priority = sp_attr->max_priority;
			*limit_ptr = (policy_limit_t) &rr_limit;
                        break;
		}

                case POLICY_FIFO:
		{
			policy_fifo_base_t fifo_base;

			if (count != POLICY_FIFO_BASE_COUNT)
				return(KERN_INVALID_ARGUMENT);
			*lc_ptr = POLICY_FIFO_LIMIT_COUNT;
			fifo_base = (policy_fifo_base_t) base;
			if (fifo_base->base_priority > sp_attr->max_priority) {
				ret = KERN_POLICY_LIMIT;
				break;
			}
                        fifo_limit.max_priority = sp_attr->max_priority;
			*limit_ptr = (policy_limit_t) &fifo_limit;
                        break;
		}

                case POLICY_TIMESHARE:
		{
			policy_timeshare_base_t ts_base;

			if (count != POLICY_TIMESHARE_BASE_COUNT)
				return(KERN_INVALID_ARGUMENT);
			*lc_ptr = POLICY_TIMESHARE_LIMIT_COUNT;
			ts_base = (policy_timeshare_base_t) base;
			if (ts_base->base_priority > sp_attr->max_priority) {
				ret = KERN_POLICY_LIMIT;
				break;
			}
                        ts_limit.max_priority = sp_attr->max_priority;
			*limit_ptr = (policy_limit_t) &ts_limit;
                        break;
		}

                default:
                        return(KERN_INVALID_POLICY);
                }

	}

	return(ret);
}

/*
 *	task_set_policy
 *
 *	Set scheduling policy and parameters, both base and limit, for 
 *	the given task. Policy can be any policy implemented by the
 *	processor set, whether enabled or not. Change contained threads
 *	if requested.
 */
kern_return_t
_mk_sp_task_set_policy(
	sf_object_t		pol,
	task_t			task,
	processor_set_t		pset,
	policy_t		policy,
	policy_base_t		base,
	mach_msg_type_number_t	base_count,
	policy_limit_t		limit,
	mach_msg_type_number_t	limit_count,
	boolean_t		change)
{
	mk_sp_attributes_t	sp_attr;
	kern_return_t		ret = KERN_SUCCESS;

	counter(c_mk_sp_task_set_policy++);

	/* check arguments */
	switch (policy) {
	case POLICY_RR:
	{
		policy_rr_base_t rr_base = (policy_rr_base_t) base;
		policy_rr_limit_t rr_limit = (policy_rr_limit_t) limit;

		if (base_count != POLICY_RR_BASE_COUNT || 
		    limit_count != POLICY_RR_LIMIT_COUNT) {
			ret = KERN_INVALID_ARGUMENT;
			break;
		}
		if (invalid_pri(rr_base->base_priority) ||
                    invalid_pri(rr_limit->max_priority)) {
			ret = KERN_INVALID_ARGUMENT;
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
			ret = KERN_INVALID_ARGUMENT;
			break;
		}
		if (invalid_pri(fifo_base->base_priority) ||
 		    invalid_pri(fifo_limit->max_priority)) {
			ret = KERN_INVALID_ARGUMENT;
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
			ret = KERN_INVALID_ARGUMENT;
			break;
		}
		if (invalid_pri(ts_base->base_priority) ||
 		    invalid_pri(ts_limit->max_priority)) {
			ret = KERN_INVALID_ARGUMENT;
                        break;
                }
                break;
	}

	default:
		ret = KERN_INVALID_POLICY;
	}

	/* stop if invalid arguments encountered */
	if (ret != KERN_SUCCESS) {
		return(ret);
	}

	/* update task policy fields */
	if ((task->policy == POLICY_TIMESHARE)
	       || (task->policy == POLICY_RR)
	       || (task->policy == POLICY_FIFO)) {
		/* policies are close relatives; take advantage of that */

		/* get pointer to scheduling policy-specific data */
		assert(task->sp_attributes != SP_ATTRIBUTES_NULL);
		sp_attr = (mk_sp_attributes_t)task->sp_attributes;

		task->policy = policy;
		sp_attr->policy_id = policy;
	}
	else {
		/* old task policy is unrelated to standard MK policies */
		/*** ??? fix me ***/
		panic("mk_sp_task_set_policy");
	}

	/* update scheduling parameters */
	switch (policy) {
	case POLICY_RR:
	{
		int temp;
		policy_rr_base_t rr_base = (policy_rr_base_t) base;
		policy_rr_limit_t rr_limit = (policy_rr_limit_t) limit;

                temp = rr_base->quantum * 1000;
                if (temp % tick)
                    temp += tick;
                sp_attr->sched_data = temp/tick;
		sp_attr->priority = rr_base->base_priority;	
		sp_attr->max_priority = rr_limit->max_priority;	
		break;
	}

	case POLICY_FIFO:
	{
		policy_fifo_base_t fifo_base = (policy_fifo_base_t) base;
		policy_fifo_limit_t fifo_limit = (policy_fifo_limit_t) limit;

		sp_attr->sched_data = 0;
                sp_attr->priority = fifo_base->base_priority;        
		sp_attr->max_priority = fifo_limit->max_priority;	
                break;
	}

	case POLICY_TIMESHARE:
	{
		policy_timeshare_base_t ts_base = 
					(policy_timeshare_base_t) base;
		policy_timeshare_limit_t ts_limit = 
					(policy_timeshare_limit_t) limit;

		sp_attr->sched_data = 0;
                sp_attr->priority = ts_base->base_priority;        
		sp_attr->max_priority = ts_limit->max_priority;	
                break;
	}

	default:
		ret = KERN_INVALID_POLICY;
	}

	return(ret);
}


kern_return_t
_mk_sp_task_set_sched(
	sf_object_t		pol,		/* policy */
	task_t			task,
	policy_t		policy,
	sched_attr_t		sched_attr,
	mach_msg_type_number_t	sched_attrCnt,
	boolean_t		set_limit,
	boolean_t		change)
{
	counter(c_mk_sp_task_set_sched++);

	panic("not yet implemented???");

	return(KERN_FAILURE);
}


kern_return_t
_mk_sp_task_get_sched(
	sf_object_t		pol,		/* policy */
	task_t			task,
	policy_t		policy,
	sched_attr_t		sched_attr,
	mach_msg_type_number_t	sched_attrCnt,
	mach_msg_type_number_t	sched_attr_size)
{
	counter(c_mk_sp_task_get_sched++);

	panic("not yet implemented???");

	return(KERN_FAILURE);
}
