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

/*
 *
 *	This file contains routines to check whether an ast is needed.
 *
 *	ast_check() - check whether ast is needed for interrupt or context
 *	switch.  Usually called by clock interrupt handler.
 *
 */

#include <cputypes.h>
#include <cpus.h>
#include <platforms.h>
#include <dipc.h>
#include <task_swapper.h>

#include <kern/ast.h>
#include <kern/counters.h>
#include <kern/cpu_number.h>
#include <kern/misc_protos.h>
#include <kern/queue.h>
#include <kern/sched.h>
#include <kern/sched_prim.h>
#include <kern/thread.h>
#include <kern/thread_act.h>
#include <kern/thread_swap.h>
#include <kern/processor.h>
#include <kern/spl.h>
#include <mach/policy.h>
#if	DIPC
#include <dipc/dipc_funcs.h>
#endif	/* DIPC */
#if	TASK_SWAPPER
#include <kern/task_swap.h>
#endif	/* TASK_SWAPPER */

volatile ast_t need_ast[NCPUS];

void
ast_init(void)
{
#ifndef	MACHINE_AST
	register int i;

	for (i=0; i<NCPUS; i++) {
		need_ast[i] = AST_NONE;
	}
#endif	/* MACHINE_AST */
}

void
ast_taken(
	boolean_t		preemption,
	ast_t			mask,
	boolean_t		interrupt
)
{
	register thread_t	self = current_thread();
	register processor_t	mypr;
	register ast_t		reasons;
	register int		mycpu;
	thread_act_t		act = self->top_act;
#ifdef	MACH_BSD
	extern void		bsd_ast(thread_act_t);
#endif

	mp_disable_preemption();
	mycpu = cpu_number();
	reasons = need_ast[mycpu] & mask;
	need_ast[mycpu] &= ~reasons;
	mp_enable_preemption();

	ml_set_interrupts_enabled(interrupt);

	/*
	 *	These actions must not block.
	 */

#if	DIPC
	if (reasons & AST_DIPC)
		dipc_ast();
#endif	/* DIPC */

#ifdef	MACH_BSD
	if (reasons & AST_BSD)
		bsd_ast(act);
	if (reasons & AST_BSD_INIT) {
		thread_ast_clear(act,AST_BSD_INIT);
		bsdinit_task();
	}
#endif

	/*
	 *	Make darn sure that we don't call thread_halt_self
	 *	or thread_block from the idle thread.
	 */

	/* XXX - this isn't currently right for the HALT case... */

	mp_disable_preemption();
	mypr = current_processor();
	if (self == mypr->idle_thread) {
#if	NCPUS == 1
	    if (reasons & AST_URGENT) {
		if (!preemption)
		    panic("ast_taken: AST_URGENT for idle_thr w/o preemption");
	    }
#endif
	    mp_enable_preemption();
	    return;
	}
	mp_enable_preemption();

#if	TASK_SWAPPER
	/* must be before AST_APC */
	if (reasons & AST_SWAPOUT) {
		spl_t s;
		swapout_ast();
		s = splsched();
		mp_disable_preemption();
		mycpu = cpu_number();
		if (need_ast[mycpu] & AST_APC) {
			/* generated in swapout_ast() to get suspended */
			reasons |= AST_APC;		/* process now ... */
			need_ast[mycpu] &= ~AST_APC;	/* ... and not later */
		}
		mp_enable_preemption();
		splx(s);
	}
#endif	/* TASK_SWAPPER */

	/* migration APC hook */
	if (reasons & AST_APC) {
		act_execute_returnhandlers();
		return;	/* auto-retry will catch anything new */
	}

	/* 
	 *	thread_block needs to know if the thread's quantum 
	 *	expired so the thread can be put on the tail of
	 *	run queue. One of the previous actions might well
	 *	have woken a high-priority thread, so we also use
	 *	csw_needed check.
	 */
	{   
	    if ((reasons &= AST_PREEMPT) == 0) {
		    mp_disable_preemption();
		    mypr = current_processor();
		    if (csw_needed(self, mypr)) {
			    reasons = (mypr->first_quantum
				       ? AST_BLOCK
				       : AST_QUANTUM);
		    }
		    mp_enable_preemption();
	    }
	    if (reasons) {
		    counter(c_ast_taken_block++);
  		    thread_block_reason((void (*)(void))0, reasons);
	    }
	}
}

void
ast_check(void)
{
	register int		mycpu;
	register processor_t	myprocessor;
	register thread_t	thread = current_thread();
	spl_t			s = splsched();

	mp_disable_preemption();
	mycpu = cpu_number();

	/*
	 *	Check processor state for ast conditions.
	 */
	myprocessor = cpu_to_processor(mycpu);
	switch(myprocessor->state) {
	    case PROCESSOR_OFF_LINE:
	    case PROCESSOR_IDLE:
	    case PROCESSOR_DISPATCHING:
		/*
		 *	No ast.
		 */
	    	break;

#if	NCPUS > 1
	    case PROCESSOR_ASSIGN:
	    case PROCESSOR_SHUTDOWN:
	        /*
		 * 	Need ast to force action thread onto processor.
		 *
		 * XXX  Should check if action thread is already there.
		 */
		ast_on(AST_BLOCK);
		break;
#endif	/* NCPUS > 1 */

	    case PROCESSOR_RUNNING:
		/*
		 *	Propagate thread ast to processor.  If we already
		 *	need an ast, don't look for more reasons.
		 */
		ast_propagate(current_act()->ast);
		if (ast_needed(mycpu))
			break;

		/*
		 *	Context switch check.
		 */
		if (csw_needed(thread, myprocessor)) {
			ast_on((myprocessor->first_quantum ?
			       AST_BLOCK : AST_QUANTUM));
		}
		break;

	    default:
	        panic("ast_check: Bad processor state");
	}
	mp_enable_preemption();
	splx(s);
}

/*
 * JMM - Temporary exports to other components
 */
#undef ast_on
#undef ast_off

void
ast_on(ast_t reason)
{
	ast_on_fast(reason);
}

void
ast_off(ast_t reason)
{
	ast_off_fast(reason);
}
