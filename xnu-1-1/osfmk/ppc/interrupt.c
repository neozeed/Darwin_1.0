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
 * @APPLE_FREE_COPYRIGHT@
 */
#include <kern/misc_protos.h>
#include <kern/assert.h>
#include <kern/thread.h>
#include <kern/counters.h>
#include <ppc/misc_protos.h>
#include <ppc/proc_reg.h>
#include <ppc/exception.h>
#include <ppc/savearea.h>
#include <pexpert/pexpert.h>
#if	NCPUS > 1
#include <ppc/POWERMAC/mp/MPPlugIn.h>
#endif /* NCPUS > 1 */
#include <sys/kdebug.h>

struct ppc_saved_state * interrupt(
        int type,
        struct ppc_saved_state *ssp,
	unsigned int dsisr,
	unsigned int dar)
{
	int	current_cpu;

	disable_preemption();

	switch (type) {
	case T_DECREMENTER:
	        KERNEL_DEBUG_CONSTANT(MACHDBG_CODE(DBG_MACH_EXCP_DECI, 0) | DBG_FUNC_NONE,
		       isync_mfdec(), ((savearea *)ssp)->save_srr0, 0, 0, 0);

	        if (pcsample_enable)
		  {
		    if (find_user_regs(current_act()))
		      add_pcsamples (user_pc(current_act()));
		  }
		rtclock_intr(0,ssp, 0);
		break;

	case T_INTERRUPT:
		/* Call the platform interrupt routine */
		counter_always(c_incoming_interrupts++);

		current_cpu = cpu_number();

	        KERNEL_DEBUG_CONSTANT(MACHDBG_CODE(DBG_MACH_EXCP_INTR, 0) | DBG_FUNC_START,
		       current_cpu, ((savearea *)ssp)->save_srr0, 0, 0, 0);

		per_proc_info[current_cpu].interrupt_handler(
			per_proc_info[current_cpu].interrupt_target, 
			per_proc_info[current_cpu].interrupt_refCon,
			per_proc_info[current_cpu].interrupt_nub, 
			per_proc_info[current_cpu].interrupt_source);

	        KERNEL_DEBUG_CONSTANT(MACHDBG_CODE(DBG_MACH_EXCP_INTR, 0) | DBG_FUNC_END,
		       0, 0, 0, 0, 0);

		break;

	case T_SIGP:
		/* Did the other processor signal us? */ 
		cpu_signal_handler();
		break;
			
	default:
#if     MACH_KDP || MACH_KDB
		(void)Call_Debugger(type, ssp);
#else
		panic("Invalid interrupt type %x\n", type);
#endif
		break;
	}

	enable_preemption();
	return ssp;
}
