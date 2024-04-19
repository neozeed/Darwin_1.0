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

#ifndef	_MK_SP_H_
#define _MK_SP_H_

/*
* Include Files
*/

#include <kern/counters.h>

/*
 * Scheduling policy operation prototypes
 */

sf_return_t	_mk_sp_init(
				sf_object_t			policy,
				int					policy_id,
				int					priority_mask_length,
				sf_priority_mask	 *priority_mask);

sf_return_t	_mk_sp_enable_processor_set(
				sf_object_t			policy,
				processor_set_t		processor_set);

sf_return_t	_mk_sp_disable_processor_set(
				sf_object_t			policy,
				processor_set_t		processor_set);

sf_return_t	_mk_sp_enable_processor(
				sf_object_t			policy,
				processor_t			processor);

sf_return_t	_mk_sp_disable_processor(
				sf_object_t			policy,
				processor_t			processor);

sf_return_t	_mk_sp_thread_update_mpri(
				sf_object_t			policy,
				sched_thread_t		thread);

sf_return_t	_mk_sp_thread_unblock(
				sf_object_t			policy,
				sched_thread_t		thread);

sf_return_t	_mk_sp_thread_done(
				sf_object_t			policy,
				sched_thread_t		old_thread);

sf_return_t	_mk_sp_thread_begin(
				sf_object_t			policy,
				sched_thread_t		new_thread);

sf_return_t	_mk_sp_thread_dispatch(
				sf_object_t			policy,
				sched_thread_t		old_thread);

sf_return_t	_mk_sp_thread_attach(
				sf_object_t			policy,
				sched_thread_t		thread);

sf_return_t	_mk_sp_thread_detach(
				sf_object_t			policy,
				sched_thread_t		thread);

sf_return_t	_mk_sp_thread_processor(
				sf_object_t			policy,
				sched_thread_t		*thread,
				processor_t			processor);

sf_return_t	_mk_sp_thread_processor_set(
				sf_object_t			policy,
				sched_thread_t		thread,
				processor_set_t		processor_set);

sf_return_t	_mk_sp_thread_set(
				sf_object_t			policy,
				sched_thread_t		thread,
				sp_attributes_t		policy_attributes);

sf_return_t	_mk_sp_thread_get(
				sf_object_t				policy,
				sched_thread_t			thread,
				sp_attributes_t			policy_attributes,
				sp_attributes_size_t	size);

int			_mk_sp_db_print_sched_stats(
				sf_object_t			policy,
				sched_thread_t		thread);

void		_mk_sp_swtch_pri(
				sf_object_t			policy,
				int					pri);

kern_return_t	_mk_sp_thread_switch(
					sf_object_t			policy,
					thread_act_t		hint_act,
					int					option,
					mach_msg_timeout_t	option_time);

kern_return_t	_mk_sp_thread_depress_abort(
					sf_object_t			policy,
					sched_thread_t		thread);

void		_mk_sp_thread_depress_timeout(
					sf_object_t			policy,
					thread_t			thread);

sf_return_t	_mk_sp_task_attach(
					sf_object_t			policy,
					task_t				task);

sf_return_t	_mk_sp_task_detach(
					sf_object_t			policy,
					task_t				task);

kern_return_t	_mk_sp_task_policy(
					sf_object_t				policy,
					task_t					task,
					policy_t				policy_id,
					policy_base_t			base,
					mach_msg_type_number_t	count,
					boolean_t				set_limit,
					boolean_t				change,
					policy_limit_t			*limit_ptr,
					int						*lc_ptr);

kern_return_t	_mk_sp_task_set_policy(
					sf_object_t				policy,
					task_t					task,
					processor_set_t			pset,
					policy_t				policy_id,
					policy_base_t			base,
					mach_msg_type_number_t	base_count,
					policy_limit_t			limit,
					mach_msg_type_number_t	limit_count,
					boolean_t				change);

kern_return_t	_mk_sp_task_set_sched(
					sf_object_t				policy,
					task_t					task,
					policy_t				policy_id,
					sched_attr_t			sched_attr,
					mach_msg_type_number_t	sched_attrCnt,
					boolean_t				set_limit,
					boolean_t				change);

kern_return_t	_mk_sp_task_get_sched(
					sf_object_t				policy,
					task_t					task,
					policy_t				policy_id,
					sched_attr_t			sched_attr,
					mach_msg_type_number_t	sched_attrCnt,
					mach_msg_type_number_t	sched_attr_size);

boolean_t	_mk_sp_thread_runnable(
					sf_object_t			policy,
					sched_thread_t		thread);

sf_return_t	_mk_sp_alarm_expired(
					sf_object_t			policy,
					long				alarm_seqno,
					kern_return_t		result,
					int					alarm_type,
					mach_timespec_t		wakeup_time,
					void				*alarm_data);

/*
 * Type definitions
 */
typedef int	mk_sp_state_t;

#define	MK_SP_ATTACHED	( 0x0001 )
#define	MK_SP_RUNNABLE	( 0x0002 )
#define	MK_SP_BLOCKED	( 0x0004 )

/*
 * MK Scheduling Policy per-thread scheduling information
 */

typedef struct mk_sp_info {
	mk_sp_state_t	th_state;		/* thread state */
	int				priority;		/* thread's priority *//*** ???base?**/
	int				max_priority;	/* maximum priority */
	int				sched_data;		/* for use by policy */
	int				policy;			/* scheduling policy */
	int				depress_priority; /* depressed from this priority */
	unsigned int	cpu_usage;		/* exp. decaying cpu usage [%cpu] */
	unsigned int	sched_usage;	/* load-weighted cpu usage [sched] */
	unsigned int	sched_stamp;	/* last time priority was updated */
	int				unconsumed_quantum; /* leftover quantum (RR/FIFO) */
#ifdef	MACH_ASSERT
	/* counters tracking number of calls to policy routines */
	unsigned int	c_mk_sp_thread_attach;
	unsigned int	c_mk_sp_thread_detach;
	unsigned int	c_mk_sp_thread_begin;
	unsigned int	c_mk_sp_thread_done;
	unsigned int	c_mk_sp_thread_dispatch;
	unsigned int	c_mk_sp_thread_unblock;
	unsigned int	c_mk_sp_thread_set;
	unsigned int	c_mk_sp_thread_get;
	unsigned int	c_mk_sp_thread_runnable;
	unsigned int	c_mk_sp_alarm_expired;
#endif	/* MACH_ASSERT */
} mk_sp_info_struct_t, *mk_sp_info_t;

#define	MK_SP_INFO_NULL		((mk_sp_info_t) NULL)

/*
 * Definitions of standard scheduling operations for this policy
 */
extern sp_ops_t		mk_sp_ops;

extern mach_counter_t	c_mk_sp_init,
			c_mk_sp_enable_processor_set,
			c_mk_sp_disable_processor_set,
			c_mk_sp_enable_processor,
			c_mk_sp_disable_processor,
			c_mk_sp_thread_update_mpri,
			c_mk_sp_thread_unblock,
			c_mk_sp_thread_done,
			c_mk_sp_thread_begin,
			c_mk_sp_thread_dispatch,
			c_mk_sp_thread_attach,
			c_mk_sp_thread_detach,
			c_mk_sp_thread_processor,
			c_mk_sp_thread_processor_set,
			c_mk_sp_thread_set,
			c_mk_sp_thread_get,
			c_mk_sp_db_print_sched_stats,
			c_mk_sp_swtch_pri,
			c_mk_sp_thread_switch,
			c_mk_sp_thread_depress_abort,
			c_mk_sp_thread_depress_timeout,
			c_mk_sp_task_attach,
			c_mk_sp_task_detach,
			c_mk_sp_task_policy,
			c_mk_sp_task_set_policy,
			c_mk_sp_task_set_sched,
			c_mk_sp_task_get_sched,
			c_mk_sp_thread_runnable,
			c_mk_sp_alarm_expired;

#endif	/* _MK_SP_H_ */
