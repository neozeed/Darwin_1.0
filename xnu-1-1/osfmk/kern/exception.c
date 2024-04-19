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

#include <mach_kdb.h>

#include <mach/boolean.h>
#include <mach/kern_return.h>
#include <mach/message.h>
#include <mach/port.h>
#include <mach/mig_errors.h>
#include <mach/thread_status.h>
#include <mach/exception_types.h>
#include <ipc/port.h>
#include <ipc/ipc_entry.h>
#include <ipc/ipc_object.h>
#include <ipc/ipc_notify.h>
#include <ipc/ipc_space.h>
#include <ipc/ipc_pset.h>
#include <ipc/ipc_machdep.h>
#include <kern/etap_macros.h>
#include <kern/counters.h>
#include <kern/ipc_tt.h>
#include <kern/task.h>
#include <kern/thread.h>
#include <kern/thread_swap.h>
#include <kern/processor.h>
#include <kern/sched.h>
#include <kern/sched_prim.h>
#include <kern/host.h>
#include <kern/misc_protos.h>
#include <string.h>
#include <mach/exc.h>            /* JMM - will become exception.h */
#include <machine/machine_rpc.h>

#if	MACH_KDB
#include <ddb/db_trap.h>
#endif	/* MACH_KDB */

/*
 * Forward declarations
 */
void exception_try_task(
	exception_type_t	exception,
	exception_data_t	code,
	mach_msg_type_number_t	codeCnt);

void exception_no_server(void);

kern_return_t alert_exception_try_task(
	exception_type_t	exception,
	exception_data_t	code,
	int			codeCnt);

#if	MACH_KDB

#include <ddb/db_output.h>

#if iPSC386 || iPSC860
boolean_t debug_user_with_kdb = TRUE;
#else
boolean_t debug_user_with_kdb = FALSE;
#endif

#endif	/* MACH_KDB */

unsigned long c_thr_exc_raise = 0;
unsigned long c_thr_exc_raise_state = 0;
unsigned long c_thr_exc_raise_state_id = 0;
unsigned long c_tsk_exc_raise = 0;
unsigned long c_tsk_exc_raise_state = 0;
unsigned long c_tsk_exc_raise_state_id = 0;


#ifdef MACHINE_FAST_EXCEPTION	/* from <machine/thread.h> if at all */
/*
 * This is the fast, MD code, with lots of stuff in-lined.
 */

extern int switch_act_swapins;

#ifdef i386
/*
 * Temporary: controls the syscall copyin optimization.
 * If TRUE, the exception function will copy in the first n
 * words from the stack of the user thread and store it in
 * the saved state, so that the server doesn't have to do
 * this.
 */
boolean_t syscall_exc_copyin = TRUE;
#endif

/*
 *	Routine:	exception
 *	Purpose:
 *		The current thread caught an exception.
 *		We make an up-call to the thread's exception server.
 *	Conditions:
 *		Nothing locked and no resources held.
 *		Called from an exception context, so
 *		thread_exception_return and thread_kdb_return
 *		are possible.
 *	Returns:
 *		Doesn't return.
 */

void
exception(
	exception_type_t	exception,
	exception_data_t	code,
	mach_msg_type_number_t	codeCnt	)
{
	thread_t		self = current_thread();
	thread_act_t		a_self = self->top_act;
	thread_act_t		cln_act;
	ipc_port_t		exc_port;
	int			i;
	struct exception_action *excp = &a_self->exc_actions[exception];
	int			flavor;
	kern_return_t		kr;

	assert(exception != EXC_RPC_ALERT);

	self->ith_scatter_list = MACH_MSG_BODY_NULL;

	/*
	 *	Optimized version of retrieve_thread_exception.
	 */

	act_lock(a_self);
	assert(a_self->ith_self != IP_NULL);
	exc_port = excp->port;
	if (!IP_VALID(exc_port)) {
		act_unlock(a_self);
		exception_try_task(exception, code, codeCnt);
		/*NOTREACHED*/
		return;
	}
	flavor = excp->flavor;

#ifdef i386
	/* For this flavor, we must copy in the first few procedure call
	 * args from the user's stack, since that is part of the important
	 * state in a syscall exception (this is for performance -- we
	 * can do the copyin much faster than the server, even if it is
	 * kernel-loaded):
	 */
	if (flavor == i386_SAVED_STATE) {
		struct i386_saved_state *statep = (struct i386_saved_state *)
					act_machine_state_ptr(self->top_act);
		statep->argv_status = FALSE;
		if (syscall_exc_copyin && copyin((char *)statep->uesp,
			   (char *)statep->argv,
			   i386_SAVED_ARGV_COUNT * sizeof(int)) == 0) {
			/* Indicate success for the server: */
			statep->argv_status = TRUE;
		}
	}
#endif

	ETAP_EXCEPTION_PROBE(EVENT_BEGIN, self, exception, code);

	ip_lock(exc_port);
	act_unlock(a_self);
	if (!ip_active(exc_port)) {
		ip_unlock(exc_port);
		exception_try_task(exception, code, codeCnt);
		/*NOTREACHED*/
		return;
	}

	/*
	 * Hold a reference to the port over the exception_raise_* calls
	 * so it can't be destroyed.  This seems like overkill, but keeps
	 * the port from disappearing between now and when
	 * ipc_object_copyin_from_kernel is finally called.
	 */
	ip_reference(exc_port);	
	exc_port->ip_srights++;
	ip_unlock(exc_port);

	switch (excp->behavior) {
	case EXCEPTION_STATE: {
	    mach_msg_type_number_t state_cnt;
	    {
		natural_t state[ THREAD_MACHINE_STATE_MAX ];

		state_cnt = state_count[flavor];
		kr = thread_getstatus(a_self, flavor, state, &state_cnt);
		if (kr == KERN_SUCCESS) {
		    kr = exception_raise_state(exc_port, exception,
				code, codeCnt,
				&flavor,
				state, state_cnt,
				state, &state_cnt);
		    if (kr == MACH_MSG_SUCCESS)
			kr = thread_setstatus(a_self, flavor, state, state_cnt);
		}
	    }

	    if (kr == KERN_SUCCESS ||
		kr == MACH_RCV_PORT_DIED) {
		    ETAP_EXCEPTION_PROBE(EVENT_END, self, exception, code);
                    thread_exception_return();
		    /* NOTREACHED*/
		    return;
	    }
	    } break;

	case EXCEPTION_DEFAULT:
		c_thr_exc_raise++;
		kr = exception_raise(exc_port,
				retrieve_act_self_fast(a_self),
				retrieve_task_self_fast(a_self->task),
				exception,
				code, codeCnt);

		if (kr == KERN_SUCCESS ||
		    kr == MACH_RCV_PORT_DIED) {
			ETAP_EXCEPTION_PROBE(EVENT_END, self, exception, code);
                        thread_exception_return();
			/* NOTREACHED*/
			return;
		}
		break;

	case EXCEPTION_STATE_IDENTITY: {
		mach_msg_type_number_t state_cnt;
		natural_t state[ THREAD_MACHINE_STATE_MAX ];

		c_thr_exc_raise_state_id++;
		state_cnt = state_count[flavor];
		kr = thread_getstatus(a_self, flavor, state, &state_cnt);
		if (kr == KERN_SUCCESS) {
		    kr = exception_raise_state_identity(exc_port,
				retrieve_act_self_fast(a_self),
				retrieve_task_self_fast(a_self->task),
				exception,
				code, codeCnt,
				&flavor,
				state, state_cnt,
				state, &state_cnt);
		    if (kr == MACH_MSG_SUCCESS)
			kr = thread_setstatus(a_self, flavor, state, state_cnt);
		}

		if (kr == KERN_SUCCESS ||
		    kr == MACH_RCV_PORT_DIED) {
                        ETAP_EXCEPTION_PROBE(EVENT_END, self, exception, code);
                        thread_exception_return();
			/* NOTREACHED*/
			return;
		}
		} break;
	default:
		panic ("bad behavior!");
	}/* switch */

	/*
	 * When a task is being terminated, it's no longer ripped
	 * directly out of the rcv from its "kill me" message, and
	 * so returns here.  The following causes it to return out
	 * to the glue code and clean itself up.
	 */
	if ((self->top_act && !self->top_act->active) ||
	    (self->state & TH_ABORT)) {
                ETAP_EXCEPTION_PROBE(EVENT_END, self, exception, code);
		thread_exception_return();
	}

	exception_try_task(exception, code, codeCnt);
	/* NOTREACHED */
}

/*
 * We only use the machine-independent exception() routine
 * if a faster MD version isn't available.
 */
#else	/* MACHINE_FAST_EXCEPTION */
/*
 *	If continuations are not used/supported, the NOTREACHED comments
 *	below are incorrect.  The exception function is expected to return.
 *	So the return statements along the slow paths are important.
 */

/*
 *	Routine:	exception
 *	Purpose:
 *		The current thread caught an exception.
 *		We make an up-call to the thread's exception server.
 *	Conditions:
 *		Nothing locked and no resources held.
 *		Called from an exception context, so
 *		thread_exception_return and thread_kdb_return
 *		are possible.
 *	Returns:
 *		Doesn't return.
 */

void
exception(
	exception_type_t	exception,
	exception_data_t	code,
	mach_msg_type_number_t  codeCnt)
{
	thread_t		self = current_thread();
	thread_act_t		a_self = self->top_act;
	ipc_port_t		exc_port;
	int			i;
	struct exception_action *excp = &a_self->exc_actions[exception];
	int			flavor;
	kern_return_t		kr;

	assert(exception != EXC_RPC_ALERT);

	if (exception == KERN_SUCCESS)
		panic("exception");

	self->ith_scatter_list = MACH_MSG_BODY_NULL;

	/*
	 *	Optimized version of retrieve_thread_exception.
	 */

	act_lock(a_self);
	assert(a_self->ith_self != IP_NULL);
	exc_port = excp->port;
	if (!IP_VALID(exc_port)) {
		act_unlock(a_self);
		exception_try_task(exception, code, codeCnt);
		/*NOTREACHED*/
		return;
	}
	flavor = excp->flavor;

	ip_lock(exc_port);
	act_unlock(a_self);
	if (!ip_active(exc_port)) {
		ip_unlock(exc_port);
		exception_try_task(exception, code, codeCnt);
		/*NOTREACHED*/
		return;
	}

	/*
	 * Hold a reference to the port over the exception_raise_* calls
	 * so it can't be destroyed.  This seems like overkill, but keeps
	 * the port from disappearing between now and when
	 * ipc_object_copyin_from_kernel is finally called.
	 */
	ip_reference(exc_port);	
	exc_port->ip_srights++;
	ip_unlock(exc_port);

	switch (excp->behavior) {
	case EXCEPTION_STATE: {
		mach_msg_type_number_t state_cnt;

		c_thr_exc_raise_state++;
		{
			natural_t state[ THREAD_MACHINE_STATE_MAX ];

			state_cnt = state_count[flavor];
			kr = thread_getstatus(a_self, flavor, 
					      (thread_state_t)state,
					      &state_cnt);
			if (kr == KERN_SUCCESS) {
			    kr = exception_raise_state(exc_port, exception,
					code, codeCnt,
					&flavor,
					state, state_cnt,
					state, &state_cnt);
			    if (kr == MACH_MSG_SUCCESS)
				kr = thread_setstatus(a_self, flavor, 
						      (thread_state_t)state,
						      state_cnt);
			}
		}

		if (kr == KERN_SUCCESS ||
		    kr == MACH_RCV_PORT_DIED) {
			thread_exception_return();
			/*NOTREACHED*/
			return;
		}
		} break;

	case EXCEPTION_DEFAULT:
		c_thr_exc_raise++;
		kr = exception_raise(exc_port,
				retrieve_act_self_fast(a_self),
				retrieve_task_self_fast(a_self->task),
				exception,
				code, codeCnt);

		if (kr == KERN_SUCCESS ||
		    kr == MACH_RCV_PORT_DIED) {
			thread_exception_return();
			/*NOTREACHED*/
			return;
		}
		break;

	case EXCEPTION_STATE_IDENTITY: {
		mach_msg_type_number_t state_cnt;
		natural_t state[ THREAD_MACHINE_STATE_MAX ];

		c_thr_exc_raise_state_id++;
		state_cnt = state_count[flavor];
		kr = thread_getstatus(a_self, flavor,
				      (thread_state_t)state,
				      &state_cnt);
		if (kr == KERN_SUCCESS) {
		    kr = exception_raise_state_identity(exc_port,
				retrieve_act_self_fast(a_self),
				retrieve_task_self_fast(a_self->task),
				exception,
				code, codeCnt,
				&flavor,
				state, state_cnt,
				state, &state_cnt);
		    if (kr == MACH_MSG_SUCCESS)
			kr = thread_setstatus(a_self, flavor,
					      (thread_state_t)state,
					      state_cnt);
		}

		if (kr == KERN_SUCCESS ||
		    kr == MACH_RCV_PORT_DIED) {
			thread_exception_return();
			/*NOTREACHED*/
			return;
		}
		} break;
	default:
		panic ("bad behavior!");
	}/* switch */

	/*
	 * When a task is being terminated, it's no longer ripped
	 * directly out of the rcv from its "kill me" message, and
	 * so returns here.  The following causes it to return out
	 * to the glue code and clean itself up.
	 */
	if ((self->top_act && !self->top_act->active) ||
	    (self->state & TH_ABORT)) {
		thread_exception_return();
		/*NOTREACHED*/
	}

	exception_try_task(exception, code, codeCnt);
	/* NOTREACHED */
	return;
}
#endif /* defined MACHINE_FAST_EXCEPTION */

/*
 *	Routine:	exception_try_task
 *	Purpose:
 *		The current thread caught an exception.
 *		We make an up-call to the task's exception server.
 *	Conditions:
 *		Nothing locked and no resources held.
 *		Called from an exception context, so
 *		thread_exception_return and thread_kdb_return
 *		are possible.
 *	Returns:
 *		Doesn't return.
 */

void
exception_try_task(
	exception_type_t	exception,
	exception_data_t	code,
	mach_msg_type_number_t  codeCnt)
{
	thread_act_t	a_self = current_act();
	thread_t 	self = a_self->thread;
	register task_t task = a_self->task;
	register 	ipc_port_t exc_port;
	int 		flavor, i;
	kern_return_t 	kr;

	assert(exception != EXC_RPC_ALERT);

	self->ith_scatter_list = MACH_MSG_BODY_NULL;

	/*
	 *	Optimized version of retrieve_task_exception.
	 */

	itk_lock(task);
	assert(task->itk_self != IP_NULL);
	exc_port = task->exc_actions[exception].port;
	if (exception == EXC_MACH_SYSCALL && exc_port == realhost.host_self) {
		itk_unlock(task);
		restart_mach_syscall();		/* magic ! */
		/* NOTREACHED */
	}
	if (!IP_VALID(exc_port)) {
		itk_unlock(task);
		exception_no_server();
		/*NOTREACHED*/
		return;
	}
	flavor = task->exc_actions[exception].flavor;

	ip_lock(exc_port);
	itk_unlock(task);
	if (!ip_active(exc_port)) {
		ip_unlock(exc_port);
		exception_no_server();
		/*NOTREACHED*/
		return;
	}

	/*
	 * Hold a reference to the port over the exception_raise_* calls
	 * (see longer comment in exception())
	 */
	ip_reference(exc_port);
	exc_port->ip_srights++;
	ip_unlock(exc_port);

	switch (task->exc_actions[exception].behavior) {
	case EXCEPTION_STATE: {
	    mach_msg_type_number_t state_cnt;
	    {
		natural_t state[ THREAD_MACHINE_STATE_MAX ];

		c_tsk_exc_raise_state++;
		state_cnt = state_count[flavor];
		kr = thread_getstatus(a_self, flavor, state, &state_cnt);
		if (kr == KERN_SUCCESS) {
		    kr = exception_raise_state(exc_port, exception,
				code, codeCnt,
				&flavor,
				state, state_cnt,
				state, &state_cnt);
		    if (kr == MACH_MSG_SUCCESS)
			kr = thread_setstatus(a_self, flavor, state, state_cnt);
		}
	    }

	    if (kr == KERN_SUCCESS ||
		kr == MACH_RCV_PORT_DIED) {
		    ETAP_EXCEPTION_PROBE(EVENT_END, self, exception, code);
                    thread_exception_return();
		    /* NOTREACHED*/
		    return;
	    }
	    } break;

	case EXCEPTION_DEFAULT:
		c_tsk_exc_raise++;
		kr = exception_raise(exc_port,
				retrieve_act_self_fast(a_self),
				retrieve_task_self_fast(a_self->task),
				exception, code, codeCnt);

		if (kr == KERN_SUCCESS ||
		    kr == MACH_RCV_PORT_DIED) {
			thread_exception_return();
			/*NOTREACHED*/
			return;
		}
		break;

	case EXCEPTION_STATE_IDENTITY: {
		mach_msg_type_number_t state_cnt;
		natural_t state[ THREAD_MACHINE_STATE_MAX ];

		c_tsk_exc_raise_state_id++;
		state_cnt = state_count[flavor];
		kr = thread_getstatus(a_self, flavor, state, &state_cnt);
		if (kr == KERN_SUCCESS) {
		    kr = exception_raise_state_identity(exc_port,
				retrieve_act_self_fast(a_self),
				retrieve_task_self_fast(a_self->task),
				exception,
				code, codeCnt,
				&flavor,
				state, state_cnt,
				state, &state_cnt);
		    if (kr == KERN_SUCCESS)
			kr = thread_setstatus(a_self, flavor, state, state_cnt);
		}

		if (kr == MACH_MSG_SUCCESS || kr == MACH_RCV_PORT_DIED) {
		    thread_exception_return();
		    /*NOTREACHED*/
		    return;
		}
		} break;

	default:
		panic ("bad behavior!");
	}/* switch */

	exception_no_server();
	/*NOTREACHED*/
}

/*
 *	Routine:	exception_no_server
 *	Purpose:
 *		The current thread took an exception,
 *		and no exception server took responsibility
 *		for the exception.  So good bye, charlie.
 *	Conditions:
 *		Nothing locked and no resources held.
 *		Called from an exception context, so
 *		thread_kdb_return is possible.
 *	Returns:
 *		Doesn't return.
 */

void
exception_no_server(void)
{
	register thread_t self = current_thread();

	/*
	 *	If this thread is being terminated, cooperate.
	 *
	 * When a task is dying, it's no longer ripped
	 * directly out of the rcv from its "kill me" message, and
	 * so returns here.  The following causes it to return out
	 * to the glue code and clean itself up.
	 */
	if ((thread_should_halt(self)) || (self->state & TH_ABORT)) {
		thread_exception_return();
		panic("exception_no_server - 1");
	}


	if (self->top_act->task == kernel_task)
		panic("kernel task terminating\n");

#if	MACH_KDB
	if (debug_user_with_kdb) {
		/*
		 *	Debug the exception with kdb.
		 *	If kdb handles the exception,
		 *	then thread_kdb_return won't return.
		 */
		db_printf("No exception server, calling kdb...\n");
#if	iPSC860
		db_printf("Dropping into ddb, avoiding thread_kdb_return\n");
		gimmeabreak();
#endif
		thread_kdb_return();
	}
#endif	/* MACH_KDB */

	/*
	 *	All else failed; terminate task.
	 */

	(void) task_terminate(self->top_act->task);
	thread_terminate_self();
	/*NOTREACHED*/
	panic("exception_no_server: returning!");
}

#ifdef MACHINE_FAST_EXCEPTION	/* from <machine/thread.h> if at all */
/*
 *	Routine:	alert_exception
 *	Purpose:
 *		The current thread caught an exception.
 *		We make an up-call to the thread's exception server.
 *	Conditions:
 *		Nothing locked and no resources held.
 *	Returns:
 *		KERN_RPC_TERMINATE_ORPHAN - if orphan should be terminated
 *		KERN_RPC_CONTINUE_ORPHAN - if orphan should be allowed to
 *			continue execution
 */

kern_return_t
alert_exception(
	exception_type_t	exception,
	exception_data_t	code,
	int			codeCnt	)
{
	thread_t		self = current_thread();
	thread_act_t		a_self = self->top_act;
	thread_act_t		cln_act;
	ipc_port_t		exc_port;
	int			i;
	struct exception_action *excp = &a_self->exc_actions[exception];
	int			flavor;
	kern_return_t		kr;

	assert(exception == EXC_RPC_ALERT);

	self->ith_scatter_list = MACH_MSG_BODY_NULL;

	/*
	 *	Optimized version of retrieve_thread_exception.
	 */

	act_lock(a_self);
	assert(a_self->ith_self != IP_NULL);
	exc_port = excp->port;
	if (!IP_VALID(exc_port)) {
		act_unlock(a_self);
		return(alert_exception_try_task(exception, code, codeCnt));
	}
	flavor = excp->flavor;

#ifdef i386
	/* For this flavor, we must copy in the first few procedure call
	 * args from the user's stack, since that is part of the important
	 * state in a syscall exception (this is for performance -- we
	 * can do the copyin much faster than the server, even if it is
	 * kernel-loaded):
	 */
	if (flavor == i386_SAVED_STATE) {
		struct i386_saved_state *statep = (struct i386_saved_state *)
					act_machine_state_ptr(self->top_act);
		statep->argv_status = FALSE;
		if (syscall_exc_copyin && copyin((char *)statep->uesp,
			   (char *)statep->argv,
			   i386_SAVED_ARGV_COUNT * sizeof(int)) == 0) {
			/* Indicate success for the server: */
			statep->argv_status = TRUE;
		}
	}
#endif

	ip_lock(exc_port);
	act_unlock(a_self);
	if (!ip_active(exc_port)) {
		ip_unlock(exc_port);
		return(alert_exception_try_task(exception, code, codeCnt));
	}

	/*
	 * Hold a reference to the port over the exception_raise_* calls
	 * so it can't be destroyed.  This seems like overkill, but keeps
	 * the port from disappearing between now and when
	 * ipc_object_copyin_from_kernel is finally called.
	 */
	ip_reference(exc_port);
	/* CHECKME! */
	/* exc_port->ip_srights++; ipc_object_copy_from_kernel does this */
	ip_unlock(exc_port);

	switch (excp->behavior) {
	case EXCEPTION_STATE: {
	    mach_msg_type_number_t state_cnt;
	    {
		natural_t state[ THREAD_MACHINE_STATE_MAX ];

		state_cnt = state_count[flavor];
		kr = thread_getstatus(a_self, flavor, state, &state_cnt);
		if (kr == KERN_SUCCESS) {
		    kr = exception_raise_state(exc_port, exception,
				code, codeCnt,
				&flavor,
				state, state_cnt,
				state, &state_cnt);
		    if (kr == MACH_MSG_SUCCESS)
			kr = thread_setstatus(a_self, flavor, state, state_cnt);
		}
	    }
	    ip_lock(exc_port);
	    ip_release(exc_port);	
	    ip_unlock(exc_port);

	    if (kr == KERN_SUCCESS ||
		kr == MACH_RCV_PORT_DIED) {
		    return(KERN_RPC_TERMINATE_ORPHAN);
	    }
	    } break;

	case EXCEPTION_DEFAULT:
		c_thr_exc_raise++;
		kr = exception_raise(exc_port,
				retrieve_act_self_fast(a_self),
				retrieve_task_self_fast(a_self->task),
				exception,
				code, codeCnt);
		ip_lock(exc_port);
		ip_release(exc_port);	
		ip_unlock(exc_port);

		if (kr == KERN_SUCCESS ||
		    kr == MACH_RCV_PORT_DIED) {
			return(KERN_RPC_TERMINATE_ORPHAN);
		}
		break;

	case EXCEPTION_STATE_IDENTITY: {
		mach_msg_type_number_t state_cnt;
		natural_t state[ THREAD_MACHINE_STATE_MAX ];

		c_thr_exc_raise_state_id++;
		state_cnt = state_count[flavor];
		kr = thread_getstatus(a_self, flavor, state, &state_cnt);
		if (kr == KERN_SUCCESS) {
		    kr = exception_raise_state_identity(exc_port,
				retrieve_act_self_fast(a_self),
				retrieve_task_self_fast(a_self->task),
				exception,
				code, codeCnt,
				&flavor,
				state, state_cnt,
				state, &state_cnt);
		    if (kr == MACH_MSG_SUCCESS)
			kr = thread_setstatus(a_self, flavor, state, state_cnt);
		}
		ip_lock(exc_port);
		ip_release(exc_port);	
		ip_unlock(exc_port);

		if (kr == KERN_SUCCESS ||
		    kr == MACH_RCV_PORT_DIED) {
			return(KERN_RPC_TERMINATE_ORPHAN);
		}
		} break;
	default:
		panic ("bad behavior!");
	}/* switch */

	/*
	 * When a task is being terminated, it's no longer ripped
	 * directly out of the rcv from its "kill me" message, and
	 * so returns here.  The following causes it to return out
	 * to the glue code and clean itself up.
	 */
	if (self->top_act && !self->top_act->active)
		return(KERN_RPC_TERMINATE_ORPHAN);

	return(alert_exception_try_task(exception, code, codeCnt));
}
#else	/* MACHINE_FAST_EXCEPTION */

/*
 *	Routine:	alert_exception
 *	Purpose:
 *		The current thread caught an exception.
 *		We make an up-call to the thread's exception server.
 *	Conditions:
 *		Nothing locked and no resources held.
 *	Returns:
 *		KERN_RPC_TERMINATE_ORPHAN - if orphan should be terminated
 *		KERN_RPC_CONTINUE_ORPHAN - if orphan should be allowed to
 *			continue execution
 */

kern_return_t
alert_exception(
	exception_type_t	exception,
	exception_data_t	code,
	int			codeCnt	)
{
	thread_t		self = current_thread();
	thread_act_t		a_self = self->top_act;
	ipc_port_t		exc_port;
	int			i;
	struct exception_action *excp = &a_self->exc_actions[exception];
	int			flavor;
	kern_return_t		kr;

	assert(exception == EXC_RPC_ALERT);

	self->ith_scatter_list = MACH_MSG_BODY_NULL;

	/*
	 *	Optimized version of retrieve_thread_exception.
	 */

	act_lock(a_self);
	assert(a_self->ith_self != IP_NULL);
	exc_port = excp->port;
	if (!IP_VALID(exc_port)) {
		act_unlock(a_self);
		return(alert_exception_try_task(exception, code, codeCnt));
	}
	flavor = excp->flavor;

	ip_lock(exc_port);
	act_unlock(a_self);
	if (!ip_active(exc_port)) {
		ip_unlock(exc_port);
		return(alert_exception_try_task(exception, code, codeCnt));
	}

	/*
	 * Hold a reference to the port over the exception_raise_* calls
	 * so it can't be destroyed.  This seems like overkill, but keeps
	 * the port from disappearing between now and when
	 * ipc_object_copyin_from_kernel is finally called.
	 */
	ip_reference(exc_port);	
	/* CHECKME! */
	/* exc_port->ip_srights++; ipc_object_copy_from_kernel does this */
	ip_unlock(exc_port);

	switch (excp->behavior) {
	case EXCEPTION_STATE: {
		mach_msg_type_number_t state_cnt;
		rpc_subsystem_t subsystem = ((ipc_port_t)exc_port)->ip_subsystem;

		c_thr_exc_raise_state++;
		if (flavor == MACHINE_THREAD_STATE &&
				subsystem &&
			    is_fast_space(exc_port->ip_receiver)) {
			natural_t *statep;
			/* Requested flavor is the same format in which
			 * we save state on this machine, so no copy is
			 * necessary.  Obtain direct pointer to saved state:
			 */
			statep = act_machine_state_ptr(self->top_act);
			state_cnt = MACHINE_THREAD_STATE_COUNT;
			kr = exception_raise_state(exc_port, exception,
				    code, codeCnt,
				    &flavor,
				    statep, state_cnt,
				    statep, &state_cnt);
			/* Server is required to return same flavor: */
			assert(flavor == MACHINE_THREAD_STATE);
		} else {
			natural_t state[ THREAD_MACHINE_STATE_MAX ];

			state_cnt = state_count[flavor];
			kr = thread_getstatus(a_self, flavor,
					      (thread_state_t)state,
					      &state_cnt);
			if (kr == KERN_SUCCESS) {
			    kr = exception_raise_state(exc_port, exception,
					code, codeCnt,
					&flavor,
					state, state_cnt,
					state, &state_cnt);
			    if (kr == MACH_MSG_SUCCESS)
				kr = thread_setstatus(a_self, flavor,
						      (thread_state_t)state,
						      state_cnt);
			}
		}
		ip_lock(exc_port);
		ip_release(exc_port);	
		ip_unlock(exc_port);

		if (kr == KERN_SUCCESS ||
		    kr == MACH_RCV_PORT_DIED) {
			return(KERN_RPC_TERMINATE_ORPHAN);
		}
		} break;

	case EXCEPTION_DEFAULT:
		c_thr_exc_raise++;
		kr = exception_raise(exc_port,
				retrieve_act_self_fast(a_self),
				retrieve_task_self_fast(a_self->task),
				exception,
				code, codeCnt);
		ip_lock(exc_port);
		ip_release(exc_port);	
		ip_unlock(exc_port);

		if (kr == KERN_SUCCESS ||
		    kr == MACH_RCV_PORT_DIED) {
			return(KERN_RPC_TERMINATE_ORPHAN);
		}
		break;

	case EXCEPTION_STATE_IDENTITY: {
		mach_msg_type_number_t state_cnt;
		natural_t state[ THREAD_MACHINE_STATE_MAX ];

		c_thr_exc_raise_state_id++;
		state_cnt = state_count[flavor];
		kr = thread_getstatus(a_self, flavor,
				      (thread_state_t)state,
				      &state_cnt);
		if (kr == KERN_SUCCESS) {
		    kr = exception_raise_state_identity(exc_port,
				retrieve_act_self_fast(a_self),
				retrieve_task_self_fast(a_self->task),
				exception,
				code, codeCnt,
				&flavor,
				state, state_cnt,
				state, &state_cnt);
		    if (kr == MACH_MSG_SUCCESS)
			    kr = thread_setstatus(a_self, flavor,
						  (thread_state_t)state,
						  state_cnt);
		}
		ip_lock(exc_port);
		ip_release(exc_port);	
		ip_unlock(exc_port);

		if (kr == KERN_SUCCESS ||
		    kr == MACH_RCV_PORT_DIED) {
			return(KERN_RPC_TERMINATE_ORPHAN);
		}
		} break;
	default:
		panic ("bad behavior!");
	}/* switch */

	/*
	 * When a task is being terminated, it's no longer ripped
	 * directly out of the rcv from its "kill me" message, and
	 * so returns here.  The following causes it to return out
	 * to the glue code and clean itself up.
	 */
	if (thread_should_halt(self)) {
		return(KERN_RPC_TERMINATE_ORPHAN);
	}

	return(alert_exception_try_task(exception, code, codeCnt));
}
#endif /* defined MACHINE_FAST_EXCEPTION */

/*
 *	Routine:	alert_exception_try_task
 *	Purpose:
 *		The current thread caught an exception.
 *		We make an up-call to the task's exception server.
 *	Conditions:
 *		Nothing locked and no resources held.
 *	Returns:
 *		KERN_RPC_TERMINATE_ORPHAN - if orphan should be terminated
 *		KERN_RPC_CONTINUE_ORPHAN - if orphan should be allowed to
 *			continue execution
 */

kern_return_t
alert_exception_try_task(
	exception_type_t	exception,
	exception_data_t	code,
	int			codeCnt	)
{
	thread_act_t	a_self = current_act();
	thread_t self = a_self->thread;
	register task_t task = a_self->task;
	register ipc_port_t exc_port;
	int flavor;
	kern_return_t kr;

	assert(exception == EXC_RPC_ALERT);

	self->ith_scatter_list = MACH_MSG_BODY_NULL;

	/*
	 *	Optimized version of retrieve_task_exception.
	 */

	itk_lock(task);
	assert(task->itk_self != IP_NULL);
	exc_port = task->exc_actions[exception].port;
	if (!IP_VALID(exc_port)) {
		itk_unlock(task);
		return(KERN_RPC_CONTINUE_ORPHAN);
	}
	flavor = task->exc_actions[exception].flavor;

	ip_lock(exc_port);
	itk_unlock(task);
	if (!ip_active(exc_port)) {
		ip_unlock(exc_port);
		return(KERN_RPC_CONTINUE_ORPHAN);
	}

	/*
	 * Hold a reference to the port over the exception_raise_* calls
	 * (see longer comment in exception())
	 */
	ip_reference(exc_port);
	/* CHECKME! */
	/* exc_port->ip_srights++; */
	ip_unlock(exc_port);

	switch (task->exc_actions[exception].behavior) {
	case EXCEPTION_STATE: {
		mach_msg_type_number_t state_cnt;
		natural_t state[ THREAD_MACHINE_STATE_MAX ];

		c_tsk_exc_raise_state++;
		state_cnt = state_count[flavor];
		kr = thread_getstatus(a_self, flavor, state, &state_cnt);
		if (kr == KERN_SUCCESS) {
		    kr = exception_raise_state(exc_port, exception,
				code, codeCnt,
				&flavor,
				state, state_cnt,
				state, &state_cnt);
		    if (kr == KERN_SUCCESS)
			kr = thread_setstatus(a_self, flavor, state, state_cnt);
		}
		ip_lock(exc_port);
		ip_release(exc_port);
		ip_unlock(exc_port);

		if (kr == MACH_MSG_SUCCESS || kr == MACH_RCV_PORT_DIED) {
		    return(KERN_RPC_TERMINATE_ORPHAN);
		}
		} break;

	case EXCEPTION_DEFAULT:
		c_tsk_exc_raise++;
		kr = exception_raise(exc_port,
				retrieve_act_self_fast(a_self),
				retrieve_task_self_fast(a_self->task),
				exception, code, codeCnt);
		ip_lock(exc_port);
		ip_release(exc_port);
		ip_unlock(exc_port);

		if (kr == KERN_SUCCESS || kr == MACH_RCV_PORT_DIED) {
		    return(KERN_RPC_TERMINATE_ORPHAN);
		}
		break;

	case EXCEPTION_STATE_IDENTITY: {
		mach_msg_type_number_t state_cnt;
		natural_t state[ THREAD_MACHINE_STATE_MAX ];

		c_tsk_exc_raise_state_id++;
		state_cnt = state_count[flavor];
		kr = thread_getstatus(a_self, flavor, state, &state_cnt);
		if (kr == KERN_SUCCESS) {
		    kr = exception_raise_state_identity(exc_port,
				retrieve_act_self_fast(a_self),
				retrieve_task_self_fast(a_self->task),
				exception,
				code, codeCnt,
				&flavor,
				state, state_cnt,
				state, &state_cnt);
		    if (kr == KERN_SUCCESS)
			kr = thread_setstatus(a_self, flavor, state, state_cnt);
		}
		ip_lock(exc_port);
		ip_release(exc_port);
		ip_unlock(exc_port);

		if (kr == MACH_MSG_SUCCESS || kr == MACH_RCV_PORT_DIED) {
		    return(KERN_RPC_TERMINATE_ORPHAN);
		}
		} break;

	default:
		panic ("bad behavior!");
	}/* switch */

	return(KERN_RPC_CONTINUE_ORPHAN);
}
