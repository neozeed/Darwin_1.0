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
 *	File:		kern/clock.h
 *	Purpose:	Data structures for the kernel alarm clock
 *			facility. This file is used only by kernel
 *			level clock facility routines.
 */

#ifndef	_KERN_CLOCK_H_
#define	_KERN_CLOCK_H_

#include <libkern/OSBase.h>

#include <mach/message.h>
#include <mach/clock_types.h>
#include <mach/mach_types.h>

#ifdef MACH_KERNEL_PRIVATE
#include <ipc/ipc_port.h>

/*
 * Actual clock alarm structure. Used for user clock_sleep() and
 * clock_alarm() calls. Alarms are allocated from the alarm free
 * list and entered in time priority order into the active alarm
 * chain of the target clock.
 */
struct	alarm {
	struct	alarm	*al_next;		/* next alarm in chain */
	struct	alarm	*al_prev;		/* previous alarm in chain */
	int				al_status;		/* alarm status */
	mach_timespec_t	al_time;		/* alarm time */
	struct {				/* message alarm data */
		int				type;		/* alarm type */
		ipc_port_t		port;		/* alarm port */
		mach_msg_type_name_t
						port_type;	/* alarm port type */
		struct	clock	*clock;		/* alarm clock */
		void			*data;		/* alarm data */
	} al_alrm;
#define al_type		al_alrm.type
#define al_port		al_alrm.port
#define al_port_type	al_alrm.port_type
#define al_clock	al_alrm.clock
#define al_data		al_alrm.data
	int				al_policy;		/* sched policy to notify */
	long			al_seqno;		/* alarm sequence number */
};
typedef struct alarm	alarm_data_t;

/* alarm status */
#define ALARM_FREE	0		/* alarm is on free list */
#define	ALARM_SLEEP	1		/* active clock_sleep() */
#define ALARM_CLOCK	2		/* active clock_alarm() */
#define ALARM_DONE	4		/* alarm has expired */

/*
 * Clock operations list structure. Contains vectors to machine
 * dependent clock routines. The routines c_config, c_init, and
 * c_gettime must be implemented for every clock device.
 */
struct	clock_ops {
	int		(*c_config)(void);		/* configuration */

	int		(*c_init)(void);		/* initialize */

	kern_return_t	(*c_gettime)(	/* get time */
				mach_timespec_t			*cur_time);

	kern_return_t	(*c_settime)(	/* set time */
				mach_timespec_t			*clock_time);

	kern_return_t	(*c_getattr)(	/* get attributes */
				clock_flavor_t			flavor,
				clock_attr_t			attr,
				mach_msg_type_number_t	*count);

	kern_return_t	(*c_setattr)(	/* set attributes */
				clock_flavor_t			flavor,
				clock_attr_t			attr,
				mach_msg_type_number_t	count);

	void		(*c_setalrm)(		/* set next alarm */
				mach_timespec_t			*alarm_time);
};
typedef struct clock_ops	*clock_ops_t;
typedef struct clock_ops	clock_ops_data_t;

/*
 * Actual clock object data structure. Contains the machine
 * dependent operations list, clock operations ports, and a
 * chain of pending alarms.
 */
struct	clock {
	clock_ops_t			cl_ops;			/* operations list */
	struct ipc_port		*cl_service;	/* service port */
	struct ipc_port		*cl_control;	/* control port */
	struct	{							/* alarm chain head */
		struct alarm 	*al_next;
	} cl_alarm;
};
typedef struct clock		clock_data_t;

/*
 * Configure the clock system.
 */
extern void		clock_config(void);
/*
 * Initialize the clock system.
 */
extern void		clock_init(void);

/*
 * Initialize the clock ipc service facility.
 */
extern void		clock_service_create(void);

/*
 * Service clock alarm interrupts. Called from machine dependent
 * layer at splclock(). The clock_id argument specifies the clock,
 * and the clock_time argument gives that clock's current time.
 */
extern void		clock_alarm_intr(
					clock_id_t		clock_id,
					mach_timespec_t	*clock_time);

/*
 * Set a clock alarm for a scheduling policy.
 */
extern kern_return_t	clock_alarm_sp(
							alarm_type_t	alarm_type,
							mach_timespec_t	alarm_time,
							long			*alarm_id,
							int				policy,
							void			*alarm_data,
							alarm_t			alarm);

/*
 * Cancel a clock alarm on behalf of a scheduling policy.
 */
extern kern_return_t	clock_alarm_cancel_sp(
							long			alarm_id);

extern kern_return_t	clock_sleep_internal(
							clock_t			clock,
							sleep_type_t	sleep_type,
							mach_timespec_t	*sleep_time);

typedef void		(*clock_timer_func_t)(
						AbsoluteTime		timestamp);

extern void			clock_set_timer_func(
						clock_timer_func_t	func);

extern void			clock_set_timer_deadline(
						AbsoluteTime		deadline);

#endif /* MACH_KERNEL_PRIVATE */

#define MACH_TIMESPEC_SEC_MAX		(0 - 1)
#define MACH_TIMESPEC_NSEC_MAX		(NSEC_PER_SEC - 1)

#define MACH_TIMESPEC_MAX	((mach_timespec_t) {				\
									MACH_TIMESPEC_SEC_MAX,		\
									MACH_TIMESPEC_NSEC_MAX } )
#define MACH_TIMESPEC_ZERO	((mach_timespec_t) { 0, 0 } )

#define ADD_MACH_TIMESPEC_NSEC(t1, nsec)		\
  do {											\
	(t1)->tv_nsec += (clock_res_t)(nsec);		\
	if ((clock_res_t)(nsec) > 0 &&				\
			(t1)->tv_nsec >= NSEC_PER_SEC) {	\
		(t1)->tv_nsec -= NSEC_PER_SEC;			\
		(t1)->tv_sec += 1;						\
	}											\
	else if ((clock_res_t)(nsec) < 0 &&			\
				 (t1)->tv_nsec < 0) {			\
		(t1)->tv_nsec += NSEC_PER_SEC;			\
		(t1)->tv_sec -= 1;						\
	}											\
  } while (0)

extern mach_timespec_t	clock_get_system_value(void);

extern mach_timespec_t	clock_get_calendar_value(void);

extern void				clock_set_calendar_value(
							mach_timespec_t		value);

extern void				clock_adjust_calendar(
							clock_res_t			nsec);

extern void				clock_initialize_calendar(void);

extern mach_timespec_t	clock_get_calendar_offset(void);

typedef unsigned long long	abstime_scalar_t;

#define AbsoluteTime_to_scalar(x)	\
						(*(abstime_scalar_t *)(x))

/* t1 < = > t2 */
#define CMP_ABSOLUTETIME(t1, t2)				\
	(AbsoluteTime_to_scalar(t1) >				\
		AbsoluteTime_to_scalar(t2)? (int)+1 :	\
	 (AbsoluteTime_to_scalar(t1) <				\
		AbsoluteTime_to_scalar(t2)? (int)-1 : 0))

/* t1 += t2 */
#define ADD_ABSOLUTETIME(t1, t2)				\
	(AbsoluteTime_to_scalar(t1) +=				\
				AbsoluteTime_to_scalar(t2))

/* t1 -= t2 */
#define SUB_ABSOLUTETIME(t1, t2)				\
	(AbsoluteTime_to_scalar(t1) -=				\
				AbsoluteTime_to_scalar(t2))

#define ADD_ABSOLUTETIME_TICKS(t1, ticks)		\
	(AbsoluteTime_to_scalar(t1) +=				\
						(integer_t)(ticks))

extern void				clock_get_timebase_info(
							natural_t			*delta,
							natural_t			*abs_to_ns_num,
							natural_t			*abs_to_ns_denom,
							natural_t			*proc_to_abs_num,
							natural_t			*proc_to_abs_denom);

extern void				clock_get_uptime(
							AbsoluteTime		*result);

extern void				clock_interval_to_deadline(
							natural_t			interval,
							natural_t			scale_factor,
							AbsoluteTime		*result);

extern void				clock_interval_to_absolutetime_interval(
							natural_t			interval,
							natural_t			scale_factor,
							AbsoluteTime		*result);

extern void				clock_absolutetime_interval_to_deadline(
							AbsoluteTime		abstime,
							AbsoluteTime		*result);

extern void				clock_deadline_for_periodic_event(
							AbsoluteTime		interval,
							AbsoluteTime		abstime,
							AbsoluteTime		*deadline);

extern void				clock_delay_for_interval(
							natural_t			interval,
							natural_t			scale_factor);

extern void				clock_delay_until(
							AbsoluteTime		deadline);

extern void				absolutetime_to_nanoseconds(
							AbsoluteTime		abstime,
							UInt64				*result);

extern void             nanoseconds_to_absolutetime(
							UInt64				nanoseconds,
							AbsoluteTime		*result);

#endif	/* _KERN_CLOCK_H_ */
