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

#include <mach_kdb.h>
#include <mach_kdp.h>
#include <debug.h>
#include <cpus.h>
#include <kern/thread.h>
#include <kern/exception.h>
#include <kern/syscall_sw.h>
#include <kern/cpu_data.h>
#include <kern/debug.h>
#include <mach/thread_status.h>
#include <vm/vm_fault.h>
#include <vm/vm_kern.h> 	/* For kernel_map */
#include <ppc/misc_protos.h>
#include <ppc/trap.h>
#include <ppc/exception.h>
#include <ppc/proc_reg.h>	/* for SR_xxx definitions */
#include <ppc/pmap.h>
#include <ppc/mem.h>
#include <ppc/fpu_protos.h>

#include <sys/kdebug.h>

#if	MACH_KDB
#include <ddb/db_watch.h>
#include <ddb/db_run.h>
#include <ddb/db_break.h>
#include <ddb/db_trap.h>

boolean_t let_ddb_vm_fault = FALSE;
boolean_t	debug_all_traps_with_kdb = FALSE;
extern struct db_watchpoint *db_watchpoint_list;
extern boolean_t db_watchpoints_inserted;
extern boolean_t db_breakpoints_inserted;


#endif	/* MACH_KDB */

extern int debugger_active[NCPUS];

#if DEBUG || MACH_ASSERT
#define TRAP_ALL 		-1
#define TRAP_ALIGNMENT		0x01
#define TRAP_DATA		0x02
#define TRAP_INSTRUCTION 	0x04
#define TRAP_AST		0x08
#define TRAP_TRACE		0x10
#define TRAP_PROGRAM		0x20
#define TRAP_EXCEPTION		0x40
#define TRAP_UNRESOLVED		0x80
#define TRAP_SYSCALL		0x100	/* all syscalls */
#define TRAP_HW			0x200	/* all in hw_exception.s */
#define TRAP_MACH_SYSCALL	0x400
#define TRAP_SERVER_SYSCALL	0x800

int trapdebug=0; /* TRAP_SERVER_SYSCALL;*/

#define TRAP_DEBUG(LEVEL, A) {if ((trapdebug & LEVEL)==LEVEL){printf A;}}
#else
#define TRAP_DEBUG(LEVEL, A)
#endif

/*
 * XXX don't pass VM_PROT_EXECUTE to vm_fault(), execute permission is implied
 * in either R or RW (note: the pmap module knows this).  This is done for the
 * benefit of programs that execute out of their data space (ala lisp).
 * If we didn't do this in that scenerio, the ITLB miss code would call us
 * and we would call vm_fault() with RX permission.  However, the space was
 * probably vm_allocate()ed with just RW and vm_fault would fail.  The "right"
 * solution to me is to have the un*x server always allocate data with RWX for
 * compatibility with existing binaries.
 */

#define	PROT_EXEC	(VM_PROT_READ)
#define PROT_RO		(VM_PROT_READ)
#define PROT_RW		(VM_PROT_READ|VM_PROT_WRITE)

/* A useful macro to update the ppc_exception_state in the PCB
 * before calling doexception
 */
#define UPDATE_PPC_EXCEPTION_STATE {					      \
	thread_act_t thr_act = current_act();				      \
	struct ppc_exception_state *es = &thr_act->mact.pcb->es;	      \
	es->dar = dar;							      \
	es->dsisr = dsisr;						      \
	es->exception = trapno / T_VECTOR_SIZE;	/* back to powerpc */ \
}

static void unresolved_kernel_trap(int trapno,
				   struct ppc_saved_state *ssp,
				   unsigned int dsisr,
				   unsigned int dar,
				   char *message);

struct ppc_saved_state *trap(int trapno,
			     struct ppc_saved_state *ssp,
			     unsigned int dsisr,
			     unsigned int dar)
{
	int exception=0;
	int code;
	int subcode;
	vm_map_t map;
        unsigned int sp;
	unsigned int space,space2;
	unsigned int offset;
	thread_act_t thr_act = current_act();
#ifdef MACH_BSD
	time_value_t tv;
#endif /* MACH_BSD */


	TRAP_DEBUG(TRAP_ALL,("NMGS TRAP %d srr0=0x%08x, srr1=0x%08x\n",
		     trapno/4,ssp->srr0,ssp->srr1));

#if DEBUG
	{
		/* make sure we're not near to overflowing kernel stack */
		int sp;
		mp_disable_preemption();						/* Make sure we don't preempt */
#ifdef	__ELF__
		__asm__ volatile("mr	%0, 1" : "=r" (sp));
#else
		__asm__ volatile("mr	%0, r1" : "=r" (sp));
#endif
		if (sp < 
		    (cpu_data[cpu_number()].active_thread->kernel_stack +
		     sizeof(struct ppc_saved_state)+256)) {
			printf("TRAP - LOW ON KERNEL STACK!\n");
		}	
	mp_enable_preemption();								/* Preemption is cool now */
	}
#endif /* DEBUG */
	/* Handle kernel traps first */

	if (!USER_MODE(ssp->srr1)) {
		/*
		 * Trap came from system task, ie kernel or collocated server
		 */
	      	switch (trapno) {

		case T_PREEMPT:								/* Handle a preempt trap */
			mp_disable_preemption();					/* since preemption is not enabled */
		  	ast_off(AST_URGENT);		 				/* clear urgent and let it sched */
		  	mp_enable_preemption();						/* normally... */
			break;								/* Do nothing, just check... */

			/*
			 * These trap types should never be seen by trap()
			 * in kernel mode, anyway.
			 * Some are interrupts that should be seen by
			 * interrupt() others just don't happen because they
			 * are handled elsewhere. Some could happen but are
			 * considered to be fatal in kernel mode.
			 */
		case T_DECREMENTER:
		case T_IN_VAIN:								/* Shouldn't ever see this, lowmem_vectors eats it */
		case T_RESET:
		case T_MACHINE_CHECK:
		case T_SYSTEM_MANAGEMENT:
		case T_ALTIVEC_ASSIST:
		case T_INTERRUPT:
		case T_FP_UNAVAILABLE:
		case T_IO_ERROR:
		case T_RESERVED:
		default:
			unresolved_kernel_trap(trapno, ssp, dsisr, dar, NULL);
			break;
			
		case T_TRACE:
		case T_RUNMODE_TRACE:
		case T_INSTRUCTION_BKPT:
#if	MACH_KDP || MACH_KDB
			if (!Call_Debugger(trapno, ssp))
#endif                   
				unresolved_kernel_trap(trapno, ssp, dsisr, dar, NULL);
			break;

		case T_PROGRAM:
			if (ssp->srr1 & MASK(SRR1_PRG_TRAP)) {
#if MACH_KDP || MACH_KDB
				if (!Call_Debugger(trapno, ssp))
#endif 
					unresolved_kernel_trap(trapno, ssp, dsisr, dar, NULL);
			} else {
				unresolved_kernel_trap(trapno, ssp, 
							dsisr, dar, NULL);
			}
			break;
		case T_ALIGNMENT:
			TRAP_DEBUG(TRAP_ALIGNMENT,
				   ("NMGS KERNEL ALIGNMENT_ACCESS, "
				     "DAR=0x%08x, DSISR = 0x%B\n",
				     dar, dsisr,
			     "\20\2HASH\5PROT\6IO_SPC\7WRITE\12WATCH\14EIO"));
			if (alignment(dsisr, dar, ssp)) {
				unresolved_kernel_trap(trapno, ssp, dsisr, dar, NULL);
			}
			break;
		case T_DATA_ACCESS:
			TRAP_DEBUG(TRAP_DATA,
				   ("NMGS KERNEL DATA_ACCESS, DAR=0x%08x,"
				     "DSISR = 0x%B\n", dar, dsisr,
			     "\20\2HASH\5PROT\6IO_SPC\7WRITE\12WATCH\14EIO"));

#if	MACH_KDB
			mp_disable_preemption();
			if (debug_mode
#if	NCPUS > 1
			    && debugger_active[cpu_number()]
#endif	/* NCPUS > 1 */
			    && !let_ddb_vm_fault) {
				/*
				 * Force kdb to handle this one.
				 */
				kdb_trap(trapno, ssp);
			}
			mp_enable_preemption();
#endif	/* MACH_KDB */

			/* simple case : not SR_COPYIN segment, from kernel */
			if ((dar >> 28) != SR_COPYIN_NUM) {
				map = kernel_map;

				offset = dar;
				

/*
 *	Note: Some ROM device drivers will access page 0 when they start.  The IOKit will 
 *	set a flag to tell us to ignore any access fault on page 0.  After the driver is
 *	opened, it will clear the flag.
 */
				if((0 == (dar & -PAGE_SIZE)) && 	/* Check for access of page 0 and */
				  ((thr_act->mact.specFlags) & ignoreZeroFault)) {
									/* special case of ignoring page zero faults */
					ssp->srr0 += 4;			/* Point to next instruction */
					break;
				}

				TRAP_DEBUG(TRAP_DATA,
					   ("SYSTEM FAULT FROM 0x%08x\n",
					    offset));

				code = vm_fault(map,
						trunc_page(offset),
						dsisr & MASK(DSISR_WRITE) ?
							PROT_RW : PROT_RO,
						FALSE);
				if (code != KERN_SUCCESS) {
					unresolved_kernel_trap(trapno, ssp, dsisr, dar, NULL);
				}
				else { 
					((savearea *)ssp)->save_flags |= SAVredrive;	/* Tell low-level to re-try fault */
					((savearea *)ssp)->save_dsisr |= MASK(DSISR_HASH);	/* Make sure this is marked as a miss */
				}
				break;
			}

			/* If we get here, the fault was due to a copyin/out */

			map = thr_act->map;

			/* Mask out SR_COPYIN and mask in original segment */

			offset = (dar & 0x0fffffff) |
				((mfsrin(dar)<<8) & 0xF0000000);

			TRAP_DEBUG(TRAP_DATA,
				   ("sr=0x%08x, dar=0x%08x, sp=0x%x\n",
				    mfsrin(dar), dar,map->pmap->space));
			assert((mfsrin(dar) & 0x000FFFFF) ==
			       map->pmap->space);
			TRAP_DEBUG(TRAP_DATA,
				   ("KERNEL FAULT FROM 0x%x,0x%08x\n",
				    map->pmap->space, offset));

			code = vm_fault(map,
					trunc_page(offset),
					dsisr & MASK(DSISR_WRITE) ?
					PROT_RW : PROT_RO,
					FALSE);

			/* If we failed, there should be a recovery
			 * spot to rfi to.
			 */
			if (code != KERN_SUCCESS) {
				TRAP_DEBUG(TRAP_DATA,
					   ("FAULT FAILED- srr0=0x%08x,"
					     "pcb-srr0=0x%08x\n",
					     ssp->srr0,
					     thr_act->mact.pcb->ss.srr0));

				if (thr_act->thread->recover) {
				
#if 0
					__asm__ volatile("tweq r1,r1");	/* (TEST/DEBUG) */
#endif

					act_lock_thread(thr_act);
					ssp->srr0 = thr_act->thread->recover;
					thr_act->thread->recover =
							(vm_offset_t)NULL;
					act_unlock_thread(thr_act);
				} else {
					unresolved_kernel_trap(trapno, ssp, dsisr, dar, "copyin/out has no recovery point");
				}
			}
			else { 
				((savearea *)ssp)->save_flags |= SAVredrive;	/* Tell low-level to re-try fault */
				((savearea *)ssp)->save_dsisr |= MASK(DSISR_HASH);	/* Make sure this is marked as a miss */
			}
			
			break;
			
		case T_INSTRUCTION_ACCESS:

			TRAP_DEBUG(TRAP_INSTRUCTION,
				   ("NMGS KERNEL INSTRUCTION ACCESS,"
				     "DSISR = 0x%B\n", dsisr,
			     "\20\2HASH\5PROT\6IO_SPC\7WRITE\12WATCH\14EIO"));

#if	MACH_KDB
			if (debug_mode
#if	NCPUS > 1
			    && debugger_active[cpu_number()]
#endif	/* NCPUS > 1 */
			    && !let_ddb_vm_fault) {
				/*
				 * Force kdb to handle this one.
				 */
				kdb_trap(trapno, ssp);
			}
#endif	/* MACH_KDB */

			/* Same as for data access, except fault type
			 * is PROT_EXEC and addr comes from srr0
			 */
			map = thr_act->map;
			
			code = vm_fault(map, trunc_page(ssp->srr0),
					PROT_EXEC, FALSE);
			if (code != KERN_SUCCESS) {
				unresolved_kernel_trap(trapno, ssp, dsisr, dar, NULL);
			}
			else { 
				((savearea *)ssp)->save_flags |= SAVredrive;	/* Tell low-level to re-try fault */
				ssp->srr1 |= MASK(DSISR_HASH);					/* Make sure this is marked as a miss */
			}
			break;

		/* Usually shandler handles all the system calls, but the
		 * atomic thread switcher may throwup (via thandler) and
		 * have to pass it up to the exception handler.
		 */

		case T_SYSTEM_CALL:
			unresolved_kernel_trap(trapno, ssp, dsisr, dar, NULL);
			break;

		case T_AST:
			TRAP_DEBUG(TRAP_AST,
				   ("T_AST FROM KERNEL MODE\n"));
			ast_taken(FALSE, AST_ALL, 0);
			break;
		}
	} else {
#ifdef MACH_BSD
		{
		void get_procrustime(time_value_t *);

			get_procrustime(&tv);
		}
#endif /* MACH_BSD */

	
		/*
		 * Trap came from user task
		 */

	      	switch (trapno) {
		case T_PREEMPT:									/* Handle a preempt trap */
			break;										/* Do nothing, just check... */

			/*
			 * These trap types should never be seen by trap()
			 * Some are interrupts that should be seen by
			 * interrupt() others just don't happen because they
			 * are handled elsewhere.
			 */
		case T_DECREMENTER:
		case T_IN_VAIN:								/* Shouldn't ever see this, lowmem_vectors eats it */
		case T_MACHINE_CHECK:
		case T_INTERRUPT:
		case T_FP_UNAVAILABLE:
		case T_SYSTEM_MANAGEMENT:
		case T_RESERVED:
		case T_IO_ERROR:
			
		default:
			panic("Unexpected exception type");
			break;

		case T_RESET:
			halt_all_cpus(TRUE); 	     /* never returns */

		case T_ALIGNMENT:
			TRAP_DEBUG(TRAP_ALIGNMENT,
				   ("NMGS USER ALIGNMENT_ACCESS, "
				     "DAR=0x%08x, DSISR = 0x%B\n", dar, dsisr,
				     "\20\2HASH\5PROT\6IO_SPC\7WRITE\12WATCH\14EIO"));
			if (alignment(dsisr, dar, ssp)) {
				code = EXC_PPC_UNALIGNED;
				exception = EXC_BAD_ACCESS;
				subcode = dar;
			}
			break;

		case T_TRACE:			/* Real PPC chips */
		case T_INSTRUCTION_BKPT:	/* 603  PPC chips */
		case T_RUNMODE_TRACE:		/* 601  PPC chips */
			TRAP_DEBUG(TRAP_TRACE,("NMGS TRACE TRAP\n"));

			exception = EXC_BREAKPOINT;
			code = EXC_PPC_TRACE;
			subcode = ssp->srr0;
			break;

		case T_PROGRAM:
			TRAP_DEBUG(TRAP_PROGRAM,
				   ("NMGS PROGRAM TRAP\n"));
			if (ssp->srr1 & MASK(SRR1_PRG_FE)) {
				TRAP_DEBUG(TRAP_PROGRAM,
					   ("FP EXCEPTION\n"));
				fpu_save();
				UPDATE_PPC_EXCEPTION_STATE;
				exception = EXC_ARITHMETIC;
				code = EXC_ARITHMETIC;
			
				mp_disable_preemption();
				subcode = current_act()->mact.FPU_pcb->fs.fpscr;
				mp_enable_preemption();
			} 	
			else if (ssp->srr1 & MASK(SRR1_PRG_ILL_INS)) {
				
				TRAP_DEBUG(TRAP_PROGRAM,
					   ("ILLEGAL INSTRUCTION\n"));

				UPDATE_PPC_EXCEPTION_STATE
				exception = EXC_BAD_INSTRUCTION;
				code = EXC_PPC_UNIPL_INST;
				subcode = ssp->srr0;
			} else if (ssp->srr1 & MASK(SRR1_PRG_PRV_INS)) {
				TRAP_DEBUG(TRAP_PROGRAM,
					   ("PRIVILEGED INSTRUCTION\n"));

				UPDATE_PPC_EXCEPTION_STATE;
				exception = EXC_BAD_INSTRUCTION;
				code = EXC_PPC_PRIVINST;
				subcode = ssp->srr0;
			} else if (ssp->srr1 & MASK(SRR1_PRG_TRAP)) {
				unsigned int inst;

				if (copyin((char *) ssp->srr0, (char *) &inst, 4 ))
					panic("copyin failed\n");
				UPDATE_PPC_EXCEPTION_STATE;
				if (inst == 0x7FE00008) {
					exception = EXC_BREAKPOINT;
					code = EXC_PPC_BREAKPOINT;
				} else {
					exception = EXC_SOFTWARE;
					code = EXC_PPC_TRAP;
				}
				subcode = ssp->srr0;
			}
			break;
			
		case T_ALTIVEC_ASSIST:
			UPDATE_PPC_EXCEPTION_STATE;
			exception = EXC_ARITHMETIC;
			code = EXC_PPC_ALTIVECASSIST;
			subcode = ssp->srr0;
			break;

		case T_DATA_ACCESS:
			TRAP_DEBUG(TRAP_DATA,
				   ("NMGS USER DATA_ACCESS, DAR=0x%08x, DSISR = 0x%B\n", dar, dsisr,"\20\2HASH\5PROT\6IO_SPC\7WRITE\12WATCH\14EIO"));
			
			map = thr_act->map;
			
			code = vm_fault(map, trunc_page(dar),
				 dsisr & MASK(DSISR_WRITE) ? PROT_RW : PROT_RO,
				 FALSE);
			if (code != KERN_SUCCESS) {
				
				TRAP_DEBUG(TRAP_DATA,("FAULT FAILED!\n"));
				UPDATE_PPC_EXCEPTION_STATE;
				exception = EXC_BAD_ACCESS;
				subcode = dar;
			}
			else { 
				((savearea *)ssp)->save_flags |= SAVredrive;	/* Tell low-level to re-try fault */
				((savearea *)ssp)->save_dsisr |= MASK(DSISR_HASH);	/* Make sure this is marked as a miss */
			}
			break;
			
		case T_INSTRUCTION_ACCESS:
			TRAP_DEBUG(TRAP_INSTRUCTION,("NMGS USER INSTRUCTION ACCESS, DSISR = 0x%B\n", dsisr,"\20\2HASH\5PROT\6IO_SPC\7WRITE\12WATCH\14EIO"));

			/* Same as for data access, except fault type
			 * is PROT_EXEC and addr comes from srr0
			 */
			map = thr_act->map;
			
			code = vm_fault(map, trunc_page(ssp->srr0),
					PROT_EXEC, FALSE);
			if (code != KERN_SUCCESS) {
				

				TRAP_DEBUG(TRAP_INSTRUCTION,
					   ("INSTR FAULT FAILED!\n"));
				UPDATE_PPC_EXCEPTION_STATE;
				exception = EXC_BAD_ACCESS;
				subcode = ssp->srr0;
			}
			else { 
				((savearea *)ssp)->save_flags |= SAVredrive;	/* Tell low-level to re-try fault */
				ssp->srr1 |= MASK(DSISR_HASH);					/* Make sure this is marked as a miss */
			}
			break;

		case T_AST:
			TRAP_DEBUG(TRAP_AST,("T_AST FROM USER MODE\n"));
			ast_taken(FALSE, AST_ALL, 0);
			break;
			
		}
#ifdef MACH_BSD
		{
		void bsd_uprofil(time_value_t *, unsigned int);

		bsd_uprofil(&tv, ssp->srr0);
		}
#endif /* MACH_BSD */
	}

	if (exception) {
		TRAP_DEBUG(TRAP_EXCEPTION,
			   ("doexception (0x%x, 0x%x, 0x%x\n",
			    exception,code,subcode));
				
#if 0
		__asm__ volatile("tweq r1,r1");			/* (TEST/DEBUG) */
#endif		
				
		doexception(exception, code, subcode);
		return ssp;
	}
	/* AST delivery
	 * Check to see if we need an AST, if so take care of it here
	 */

	mp_disable_preemption();							/* We can't preempt here */
	while (ast_needed(cpu_number()) && USER_MODE(ssp->srr1))	{
		mp_enable_preemption();							/* ast_taken doesn't expect preemption disabled */
		ast_taken(FALSE, AST_ALL, 0);
		mp_disable_preemption();						/* Disable pretemption for check */
	}
	mp_enable_preemption();								/* Preemption be cool again */

	return ssp;
}

/* This routine is called from assembly before each and every system call.
 * It must preserve r3.
 */

extern int syscall_trace(int, struct ppc_saved_state *);


extern int pmdebug;

int syscall_trace(int retval, struct ppc_saved_state *ssp)
{
	int i, argc;

	int kdarg[3];
	/* Always prepare to trace mach system calls */
	if (kdebug_enable && (ssp->r0 & 0x80000000)) {
	  /* Mach trap */
	  kdarg[0]=0;
	  kdarg[1]=0;
	  kdarg[2]=0;
	  argc = mach_trap_table[-(ssp->r0)].mach_trap_arg_count;
	  if (argc > 3)
	    argc = 3;
	  for (i=0; i < argc; i++)
	    kdarg[i] = (int)*(&ssp->r3 + i);
	  KERNEL_DEBUG_CONSTANT(MACHDBG_CODE(DBG_MACH_EXCP_SC, (-(ssp->r0))) | DBG_FUNC_START,
		       kdarg[0], kdarg[1], kdarg[2], 0, 0);
	}	    

#if MACH_ASSERT
	if (trapdebug & TRAP_SYSCALL)
		trapdebug |= (TRAP_MACH_SYSCALL|TRAP_SERVER_SYSCALL);

	if (!(trapdebug & (TRAP_MACH_SYSCALL|TRAP_SERVER_SYSCALL)))
		return retval;
	if (ssp->r0 & 0x80000000) {
		/* Mach trap */
		if (!(trapdebug & TRAP_MACH_SYSCALL))
			return retval;

		printf("0x%08x : %30s (",
		       ssp->srr0, mach_trap_table[-(ssp->r0)].mach_trap_name);
		argc = mach_trap_table[-(ssp->r0)].mach_trap_arg_count;
		for (i = 0; i < argc; i++)
			printf("%08x ",*(&ssp->r3 + i));
		printf(")\n");
	} else {
		if (!(trapdebug & TRAP_SERVER_SYSCALL))
			return retval;
		printf("0x%08x : UNIX %3d (", ssp->srr0, ssp->r0);
		argc = 4; /* print 4 of 'em! */
		for (i = 0; i < argc; i++)
			printf("%08x ",*(&ssp->r3 + i));
		printf(")\n");
	}
#endif /* MACH_ASSERT */
	return retval;
}

/* This routine is called from assembly after each mach system call
 * It must preserve r3.
 */

extern int syscall_trace_end(int, struct ppc_saved_state *);

int syscall_trace_end(int retval, struct ppc_saved_state *ssp)
{
	if (kdebug_enable && (ssp->r0 & 0x80000000)) {
	  /* Mach trap */
	  KERNEL_DEBUG_CONSTANT(MACHDBG_CODE(DBG_MACH_EXCP_SC,(-(ssp->r0))) | DBG_FUNC_END,
		       retval, 0, 0, 0, 0);
	}	    
	return retval;
}

/*
 * called from syscall if there is an error
 */

int syscall_error(
	int exception,
	int code,
	int subcode,
	struct ppc_saved_state *ssp)
{
	register thread_t thread;

	thread = current_thread();

	if (thread == 0)
	    panic("syscall error in boot phase");

	if (!USER_MODE(ssp->srr1))
		panic("system call called from kernel");

	doexception(exception, code, subcode);

	return 0;
}

/* Pass up a server syscall/exception */
void
doexception(
	    int exc,
	    int code,
	    int sub)
{
	exception_data_type_t   codes[EXCEPTION_CODE_MAX];

	codes[0] = code;	
	codes[1] = sub;
	exception(exc, codes, 2);
}

char *trap_type[] = {
	"0x000   Interrupt in vain",
	"0x100   System reset",
	"0x200   Machine check",
	"0x300   Data access",
	"0x400   Instruction access",
	"0x500   External interrupt",
	"0x600   Alignment",
	"0x700   Program",
	"0x800   Floating point",
	"0x900   Decrementer",
	"0xA00   I/O controller interface",
	"0xB00   INVALID EXCEPTION",
	"0xC00   System call exception",
	"0xD00   Trace",
	"0xE00   FP assist",
	"0xF20   VMX",
	"0xF00   INVALID EXCEPTION",
	"0x1000  Instruction PTE miss",
	"0x1100  Data load PTE miss",
	"0x1200  Data store PTE miss",
	"0x1300  Instruction Breakpoint",
	"0x1400  System Management",
	"0x1500  INVALID EXCEPTION",
	"0x1600  Altivec Assist",
	"0x1700  INVALID EXCEPTION",
	"0x1800  INVALID EXCEPTION",
	"0x1900  INVALID EXCEPTION",
	"0x1A00  INVALID EXCEPTION",
	"0x1B00  INVALID EXCEPTION",
	"0x1C00  INVALID EXCEPTION",
	"0x1D00  INVALID EXCEPTION",
	"0x1E00  INVALID EXCEPTION",
	"0x1F00  INVALID EXCEPTION",
	"0x2000  Run Mode/Trace"
};
int TRAP_TYPES = sizeof (trap_type) / sizeof (trap_type[0]);

void unresolved_kernel_trap(int trapno,
			    struct ppc_saved_state *ssp,
			    unsigned int dsisr,
			    unsigned int dar,
			    char *message)
{
	unsigned int* stackptr;
	char *trap_name;
	int i, lr;
	extern void print_backtrace(struct ppc_saved_state *);
	extern unsigned int debug_mode, disableDebugOuput;

	disableDebugOuput = FALSE;
	debug_mode++;
	if ((unsigned)trapno <= T_MAX)
		trap_name = trap_type[trapno / T_VECTOR_SIZE];
	else
		trap_name = "???? unrecognized exception";
	if (message == NULL)
		message = trap_name;

	printf("\n\nUnresolved kernel trap: %s DSISR=0x%08x DAR=0x%08x PC=0x%08x\n"
	       "generating stack backtrace prior to panic:\n\n",
	       trap_name, dsisr, dar, ssp->srr0);

	printf("backtrace: %08x ", ssp->srr0);

	stackptr = (unsigned int *)(ssp->r1);

#ifdef	__ELF__
	lr = 1;
#else
	lr = 2;
#endif

	print_backtrace(ssp);

#if	MACH_KDB || MACH_KDP
	(void *)Call_Debugger(trapno, ssp);
#endif
	panic(message);
}

#if	MACH_KDB
void
thread_kdb_return(void)
{
	register thread_act_t	thr_act = current_act();
	register thread_t	cur_thr = current_thread();
	register struct ppc_saved_state *regs = USER_REGS(thr_act);

	Call_Debugger(thr_act->mact.pcb->es.exception, regs);
#if		MACH_LDEBUG
	assert(cur_thr->mutex_count == 0); 
#endif		/* MACH_LDEBUG */
	check_simple_locks();
	thread_exception_return();
	/*NOTREACHED*/
}
#endif	/* MACH_KDB */
