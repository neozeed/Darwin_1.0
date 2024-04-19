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
 *	File:	ppc/cpu.c
 *
 *	cpu specific  routines
 */

#include <kern/machine.h>
#include <kern/misc_protos.h>
#include <kern/thread.h>
#include <kern/processor.h>
#include <mach/machine.h>
#include <mach/processor_info.h>
#include <mach/mach_types.h>
#include <ppc/proc_reg.h>
#include <ppc/misc_protos.h>
#include <ppc/machine_routines.h>
#include <ppc/machine_cpu.h>
#include <ppc/exception.h>
#include <pexpert/pexpert.h>
#include <pexpert/ppc/powermac.h>

/* TODO: BOGUS TO BE REMOVED */
int real_ncpus = 1;

int wncpu = NCPUS;
resethandler_t	resethandler_target;

#define MMCR0_SUPPORT_MASK 0xf83f1fff
#define MMCR1_SUPPORT_MASK 0xffc00000
#define MMCR2_SUPPORT_MASK 0x80000000

extern int debugger_pending[NCPUS];	
extern int debugger_is_slave[NCPUS];
extern int debugger_holdoff[NCPUS];
extern int debugger_sync;


kern_return_t
cpu_control(
	int			slot_num,
	processor_info_t	info,
	unsigned int    	count)
{
        cpu_type_t        cpu_type;
        cpu_subtype_t     cpu_subtype;
	processor_pm_regs_t  perf_regs;
	processor_control_cmd_t cmd;
	boolean_t oldlevel;

	cpu_type = machine_slot[slot_num].cpu_type;
	cpu_subtype = machine_slot[slot_num].cpu_subtype;
	cmd = (processor_control_cmd_t) info;

	if (count < PROCESSOR_CONTROL_CMD_COUNT)
	  return(KERN_FAILURE);

	if ( cpu_type != cmd->cmd_cpu_type ||
	     cpu_subtype != cmd->cmd_cpu_subtype)
	  return(KERN_FAILURE);

	switch (cmd->cmd_op)
	  {
	  case PROCESSOR_PM_CLR_PMC:       /* Clear Performance Monitor Counters */
	    switch (cpu_subtype)
	      {
	      case CPU_SUBTYPE_POWERPC_604:
		{
		  oldlevel = ml_set_interrupts_enabled(FALSE);    /* disable interrupts */
		  mtpmc1(0x0);
		  mtpmc2(0x0);
		  ml_set_interrupts_enabled(oldlevel);     /* enable interrupts */
		  return(KERN_SUCCESS);
		}
	      case CPU_SUBTYPE_POWERPC_604e:
	      case CPU_SUBTYPE_POWERPC_750:
	      case CPU_SUBTYPE_POWERPC_Max:
		{
		  oldlevel = ml_set_interrupts_enabled(FALSE);    /* disable interrupts */
		  mtpmc1(0x0);
		  mtpmc2(0x0);
		  mtpmc3(0x0);
		  mtpmc4(0x0);
		  ml_set_interrupts_enabled(oldlevel);     /* enable interrupts */
		  return(KERN_SUCCESS);
		}
	      default:
		return(KERN_FAILURE);
	      } /* cpu_subtype */
	  case PROCESSOR_PM_SET_REGS:      /* Set Performance Monitor Registors */
	    switch (cpu_subtype)
	      {
	      case CPU_SUBTYPE_POWERPC_604:
		if (count <  (PROCESSOR_CONTROL_CMD_COUNT 
			       + PROCESSOR_PM_REGS_COUNT_POWERPC_604))
		  return(KERN_FAILURE);
		else
		  {
		    perf_regs = (processor_pm_regs_t)cmd->cmd_pm_regs;
		    oldlevel = ml_set_interrupts_enabled(FALSE);    /* disable interrupts */
		    mtmmcr0(PERFMON_MMCR0(perf_regs) & MMCR0_SUPPORT_MASK);
		    mtpmc1(PERFMON_PMC1(perf_regs));
		    mtpmc2(PERFMON_PMC2(perf_regs));
		    ml_set_interrupts_enabled(oldlevel);     /* enable interrupts */
		    return(KERN_SUCCESS);
		  }
	      case CPU_SUBTYPE_POWERPC_604e:
	      case CPU_SUBTYPE_POWERPC_750:
		if (count <  (PROCESSOR_CONTROL_CMD_COUNT +
		       PROCESSOR_PM_REGS_COUNT_POWERPC_750))
		  return(KERN_FAILURE);
		else
		  {
		    perf_regs = (processor_pm_regs_t)cmd->cmd_pm_regs;
		    oldlevel = ml_set_interrupts_enabled(FALSE);    /* disable interrupts */
		    mtmmcr0(PERFMON_MMCR0(perf_regs) & MMCR0_SUPPORT_MASK);
		    mtpmc1(PERFMON_PMC1(perf_regs));
		    mtpmc2(PERFMON_PMC2(perf_regs));
		    mtmmcr1(PERFMON_MMCR1(perf_regs) & MMCR1_SUPPORT_MASK);
		    mtpmc3(PERFMON_PMC3(perf_regs));
		    mtpmc4(PERFMON_PMC4(perf_regs));
		    ml_set_interrupts_enabled(oldlevel);     /* enable interrupts */
		    return(KERN_SUCCESS);
		  }
	      case CPU_SUBTYPE_POWERPC_Max:
		if (count <  (PROCESSOR_CONTROL_CMD_COUNT +
		       PROCESSOR_PM_REGS_COUNT_POWERPC_Max))
		  return(KERN_FAILURE);
		else
		  {
		    perf_regs = (processor_pm_regs_t)cmd->cmd_pm_regs;
		    oldlevel = ml_set_interrupts_enabled(FALSE);    /* disable interrupts */
		    mtmmcr0(PERFMON_MMCR0(perf_regs) & MMCR0_SUPPORT_MASK);
		    mtpmc1(PERFMON_PMC1(perf_regs));
		    mtpmc2(PERFMON_PMC2(perf_regs));
		    mtmmcr1(PERFMON_MMCR1(perf_regs) & MMCR1_SUPPORT_MASK);
		    mtpmc3(PERFMON_PMC3(perf_regs));
		    mtpmc4(PERFMON_PMC4(perf_regs));
		    mtmmcr2(PERFMON_MMCR2(perf_regs) & MMCR2_SUPPORT_MASK);
		    ml_set_interrupts_enabled(oldlevel);     /* enable interrupts */
		    return(KERN_SUCCESS);
		  }
	      default:
		return(KERN_FAILURE);
	      } /* switch cpu_subtype */
	  case PROCESSOR_PM_SET_MMCR:
	    switch (cpu_subtype)
	      {
	      case CPU_SUBTYPE_POWERPC_604:
		if (count < (PROCESSOR_CONTROL_CMD_COUNT +
		       PROCESSOR_PM_REGS_COUNT_POWERPC_604))
		  return(KERN_FAILURE);
		else
		  {
		    perf_regs = (processor_pm_regs_t)cmd->cmd_pm_regs;
		    mtmmcr0(PERFMON_MMCR0(perf_regs) & MMCR0_SUPPORT_MASK);
		    return(KERN_SUCCESS);
		  }
	      case CPU_SUBTYPE_POWERPC_604e:
	      case CPU_SUBTYPE_POWERPC_750:
		if (count < (PROCESSOR_CONTROL_CMD_COUNT +
		      PROCESSOR_PM_REGS_COUNT_POWERPC_750))
		  return(KERN_FAILURE);
		else
		  {
		    perf_regs = (processor_pm_regs_t)cmd->cmd_pm_regs;
		    oldlevel = ml_set_interrupts_enabled(FALSE);    /* disable interrupts */
		    mtmmcr0(PERFMON_MMCR0(perf_regs) & MMCR0_SUPPORT_MASK);
		    mtmmcr1(PERFMON_MMCR1(perf_regs) & MMCR1_SUPPORT_MASK);
		    ml_set_interrupts_enabled(oldlevel);     /* enable interrupts */
		    return(KERN_SUCCESS);
		  }
	      case CPU_SUBTYPE_POWERPC_Max:
		if (count < (PROCESSOR_CONTROL_CMD_COUNT +
		      PROCESSOR_PM_REGS_COUNT_POWERPC_Max))
		  return(KERN_FAILURE);
		else
		  {
		    perf_regs = (processor_pm_regs_t)cmd->cmd_pm_regs;
		    oldlevel = ml_set_interrupts_enabled(FALSE);    /* disable interrupts */
		    mtmmcr0(PERFMON_MMCR0(perf_regs) & MMCR0_SUPPORT_MASK);
		    mtmmcr1(PERFMON_MMCR1(perf_regs) & MMCR1_SUPPORT_MASK);
		    mtmmcr2(PERFMON_MMCR2(perf_regs) & MMCR2_SUPPORT_MASK);
		    ml_set_interrupts_enabled(oldlevel);     /* enable interrupts */
		    return(KERN_SUCCESS);
		  }
	      default:
		return(KERN_FAILURE);
	      } /* cpu_subtype */
	  default:
	    return(KERN_FAILURE);
	  } /* switch cmd_op */
}

kern_return_t
cpu_info(
	processor_flavor_t	flavor,
	int			slot_num,
	processor_info_t	info,
	unsigned int    	*count)
{
        cpu_subtype_t     cpu_subtype;
	processor_pm_regs_t  perf_regs;
	boolean_t oldlevel;

	cpu_subtype = machine_slot[slot_num].cpu_subtype;

	switch (flavor)
	  {
	  case PROCESSOR_PM_REGS_INFO:
	    {
	      perf_regs = (processor_pm_regs_t) info;

	      switch (cpu_subtype)
		{
		case CPU_SUBTYPE_POWERPC_604:
		  {
		    if (*count < PROCESSOR_PM_REGS_COUNT_POWERPC_604)
		      return(KERN_FAILURE);
	      
		    oldlevel = ml_set_interrupts_enabled(FALSE);    /* disable interrupts */
		    PERFMON_MMCR0(perf_regs) = mfmmcr0();
		    PERFMON_PMC1(perf_regs)  = mfpmc1();
		    PERFMON_PMC2(perf_regs)  = mfpmc2();
		    ml_set_interrupts_enabled(oldlevel);     /* enable interrupts */

		    *count = PROCESSOR_PM_REGS_COUNT_POWERPC_604;
		    return(KERN_SUCCESS);
		  }
		case CPU_SUBTYPE_POWERPC_604e:
		case CPU_SUBTYPE_POWERPC_750:
		  {
		    if (*count < PROCESSOR_PM_REGS_COUNT_POWERPC_750)
		      return(KERN_FAILURE);
	      
		    oldlevel = ml_set_interrupts_enabled(FALSE);    /* disable interrupts */
		    PERFMON_MMCR0(perf_regs) = mfmmcr0();
		    PERFMON_PMC1(perf_regs)  = mfpmc1();
		    PERFMON_PMC2(perf_regs)  = mfpmc2();
		    PERFMON_MMCR1(perf_regs) = mfmmcr1();
		    PERFMON_PMC3(perf_regs)  = mfpmc3();
		    PERFMON_PMC4(perf_regs)  = mfpmc4();
		    ml_set_interrupts_enabled(oldlevel);     /* enable interrupts */

		    *count = PROCESSOR_PM_REGS_COUNT_POWERPC_750;
		    return(KERN_SUCCESS);
		  }
		case CPU_SUBTYPE_POWERPC_Max:
		  {
		    if (*count < PROCESSOR_PM_REGS_COUNT_POWERPC_Max)
		      return(KERN_FAILURE);
	      
		    oldlevel = ml_set_interrupts_enabled(FALSE);    /* disable interrupts */
		    PERFMON_MMCR0(perf_regs) = mfmmcr0();
		    PERFMON_PMC1(perf_regs)  = mfpmc1();
		    PERFMON_PMC2(perf_regs)  = mfpmc2();
		    PERFMON_MMCR1(perf_regs) = mfmmcr1();
		    PERFMON_PMC3(perf_regs)  = mfpmc3();
		    PERFMON_PMC4(perf_regs)  = mfpmc4();
		    PERFMON_MMCR2(perf_regs) = mfmmcr2();
		    ml_set_interrupts_enabled(oldlevel);     /* enable interrupts */

		    *count = PROCESSOR_PM_REGS_COUNT_POWERPC_Max;
		    return(KERN_SUCCESS);
		  }
		default:
		  return(KERN_FAILURE);
		} /* switch cpu_subtype */
	    } /* PROCESSOR_PM_REGS_INFO */
	  default:
	    return(KERN_INVALID_ARGUMENT);
	  } /* flavor */
}

cpu_init()
{
	int	cpu;
	cpu_subtype_t	t;

	cpu = cpu_number();

	machine_slot[cpu].running = TRUE;
	machine_slot[cpu].cpu_type = CPU_TYPE_POWERPC;

	switch (PROCESSOR_VERSION) {
		case PROCESSOR_VERSION_601:             /* 601 */
			t = CPU_SUBTYPE_POWERPC_601; break;
		case PROCESSOR_VERSION_603:             /* ? */
			t = CPU_SUBTYPE_POWERPC_603; break;
		case PROCESSOR_VERSION_604:             /* ? */
			t = CPU_SUBTYPE_POWERPC_604; break;
		case 5:                                 /* ? */
			t = CPU_SUBTYPE_POWERPC_602; break;
		case PROCESSOR_VERSION_603e:            /* ? */
			t = CPU_SUBTYPE_POWERPC_603e; break;
		case 7:                                 /* ? */
			t = CPU_SUBTYPE_POWERPC_603ev; break;
		case PROCESSOR_VERSION_604e:            /* ? */
		case PROCESSOR_VERSION_604ev:           /* ? */
			__asm__ volatile("mtspr 1023,%0" : : "r" (cpu));	/* Set logical CPU into the PIR */
			t = CPU_SUBTYPE_POWERPC_604e; break;
		case PROCESSOR_VERSION_750:             /* ? */
			t = CPU_SUBTYPE_POWERPC_750; break;
		case PROCESSOR_VERSION_Max:            /* Max */
			__asm__ volatile("mtspr 1023,%0" : : "r" (cpu));	/* Set logical CPU into the PIR */
			t = CPU_SUBTYPE_POWERPC_Max; break;
		default:
			t = CPU_SUBTYPE_POWERPC_ALL; break;
	}
	machine_slot[cpu].cpu_subtype = t;

}

void
cpu_machine_init(
	void)
{
	/* TODO: realese mutex lock reset_handler_lock */

	PE_cpu_machine_init(per_proc_info[cpu_number()].cpu_id);
}

kern_return_t
cpu_register(
	int *target_cpu
)
{
	int cpu;

	/* 
	 * TODO: 
	 * - Run cpu_register() in exclusion mode 
	 */

	*target_cpu = -1;
	for(cpu=0; cpu < wncpu; cpu++) {
		if(!machine_slot[cpu].is_cpu) {
			machine_slot[cpu].is_cpu = TRUE;
			*target_cpu = cpu;
			break;
		}
	}
	if (*target_cpu != -1) {
		real_ncpus++;
		return KERN_SUCCESS;
	} else
		return KERN_FAILURE;
}

kern_return_t
cpu_start(
	int cpu)
{
	struct per_proc_info	*proc_info;
	kern_return_t		ret;

	extern void (*exception_handlers[])(void);
	extern vm_offset_t	intstack;
	extern vm_offset_t	debstack;

	if (cpu == master_cpu) {
 	  PE_cpu_machine_init(per_proc_info[cpu].cpu_id);
	  
	  return KERN_SUCCESS;
	}

	proc_info = &per_proc_info[cpu];

	proc_info->cpu_number = cpu;
	proc_info->cpu_flags = 0;
	proc_info->istackptr = (vm_offset_t)&intstack + (INTSTACK_SIZE*(cpu+1)) - sizeof (struct ppc_saved_state);
	proc_info->intstack_top_ss = proc_info->istackptr;
#if     MACH_KDP || MACH_KDB
	proc_info->debstackptr = (vm_offset_t)&debstack + (KERNEL_STACK_SIZE*(cpu+1)) - sizeof (struct ppc_saved_state);
	proc_info->debstack_top_ss = proc_info->debstackptr;
#endif  /* MACH_KDP || MACH_KDB */
	proc_info->phys_exception_handlers = 
				kvtophys((vm_offset_t)&exception_handlers);
	proc_info->get_interrupts_enabled = fake_get_interrupts_enabled;
	proc_info->set_interrupts_enabled = fake_set_interrupts_enabled;
	proc_info->virt_per_proc_info = (unsigned int)proc_info;
	proc_info->active_kloaded = (unsigned int)&active_kloaded[cpu];
	proc_info->cpu_data = (unsigned int)&cpu_data[cpu];
	proc_info->active_stacks = (unsigned int)&active_stacks[cpu];
	proc_info->need_ast = (unsigned int)&need_ast[cpu];
	proc_info->FPU_thread = 0;

	if (cpu != cpu_number()) {
		extern void _start_cpu(void);

		if (proc_info->start_paddr == EXCEPTION_VECTOR(T_RESET)) {

			/* TODO: get mutex lock reset_handler_lock */

			resethandler_target.type = RESET_HANDLER_START;
			resethandler_target.call_paddr = kvtophys((vm_offset_t)_start_cpu); 
			resethandler_target.arg__paddr = kvtophys((vm_offset_t)proc_info);
			
			ml_phys_write((vm_offset_t)&ResetHandler + 0,
				      resethandler_target.type);
			ml_phys_write((vm_offset_t)&ResetHandler + 4,
				      resethandler_target.call_paddr);
			ml_phys_write((vm_offset_t)&ResetHandler + 8,
				      resethandler_target.arg__paddr);
					  
			__asm__ volatile("sync");				/* Commit to storage */
			__asm__ volatile("isync");				/* Wait a second */
		}

		ret = PE_cpu_start(proc_info->cpu_id, 
					proc_info->start_paddr, (vm_offset_t)proc_info);

		if (ret != KERN_SUCCESS && 
		    proc_info->start_paddr == EXCEPTION_VECTOR(T_RESET)) {

			/* TODO: realese mutex lock reset_handler_lock */
		}
	}
	else
		ret = KERN_SUCCESS;

	return(ret);
}

kern_return_t
cpu_signal_handler_register()
{
}


/*
 *	Here is where we implement the receiver of the signaling protocol.
 *	We wait for the signal status area to be passed to us. Then we snarf
 *	up the status, the sender, and the 3 potential parms. Next we release
 *	the lock and signal the other guy.
 */

void cpu_signal_handler(void) {

	unsigned int holdStat, holdParm0, holdParm1, holdParm2, mtype;
	struct per_proc_info *pproc;					/* Area for my per_proc address */
	int cpu;
	
	cpu = cpu_number();								/* Get the CPU number */
	pproc = &per_proc_info[cpu];					/* Point to our block */

/*
 *	Since we've been signaled, wait just under 1ms for the signal lock to pass
 */
	if(!hw_lock_mbits(&pproc->MPsigpStat, MPsigpMsgp, (MPsigpBusy | MPsigpPass),
	  (MPsigpBusy | MPsigpPass), (powermac_info.bus_clock_rate_hz >> 7))) {
		panic("cpu_signal_handler: Lock pass timed out\n");
	}
	
	holdStat = pproc->MPsigpStat;					/* Snarf stat word */
	holdParm0 = pproc->MPsigpParm0;					/* Snarf parameter */
	holdParm1 = pproc->MPsigpParm1;					/* Snarf parameter */
	holdParm2 = pproc->MPsigpParm2;					/* Snarf parameter */
	
	__asm__ volatile("isync");						/* Make sure we don't unlock until memory is in */

	pproc->MPsigpStat = holdStat & ~(MPsigpMsgp | MPsigpFunc);	/* Release lock */

	switch ((holdStat & MPsigpFunc) >> 8) {			/* Decode function code */

		case MPsigpIdle:							/* Was function cancelled? */
			return;									/* Yup... */
			
		case MPsigpSigp:							/* Signal Processor message? */
			
			switch (holdParm0) {					/* Decode SIGP message order */

				case SIGPast:						/* Should we do an AST? */
#if 0
					ast_check();					/* Yes, do it */
#else
					kprintf("cpu_signal_handler: AST request disabled\n");
#endif
					return;							/* All done... */
	
				case SIGPdebug:						/* Enter the debugger? */		

					debugger_is_slave[cpu]++;		/* Bump up the count to show we're here */
					hw_atomic_sub(&debugger_sync, 1);	/* Show we've received the 'rupt */
					__asm__ volatile("tw 4,r3,r3");	/* Enter the debugger */
					return;							/* All done now... */
					
				case SIGPwake:						/* Wake up CPU */
					return;							/* No need to do anything, the interrupt does it all... */
					
				default:
#if DEBUG
					kprintf("cpu_signal_handler: unknown SIGP message order - %08X\n", holdParm0);
#endif			
					return;
			
			}
	
		default:
#if DEBUG
			kprintf("cpu_signal_handler: unknown SIGP function - %08X\n", (holdStat & MPsigpFunc) >> 8);
#endif			
			return;
	
	}
	panic("cpu_signal_handler: we should never get here\n");
}

/*
 *	Here is where we send a message to another processor.  So far we only have two:
 *	SIGPast and SIGPdebug.  SIGPast is used to preempt and kick off threads (this is
 *	currently disabled). SIGPdebug is used to enter the debugger.
 *
 *	We set up the SIGP function to indicate that this is a simple message and set the
 *	order code (MPsigpParm0) to SIGPast or SIGPdebug). After finding the per_processor
 *	block for the target, we lock the message block. Then we set the parameter(s). 
 *	Next we change the lock (also called "busy") to "passing" and finally signal
 *	the other processor. Note that we only wait about 1ms to get the message lock.  
 *	If we time out, we return failure to our caller. It is their responsibility to
 *	recover.
 */

kern_return_t cpu_signal(int target, int signal) {	/* Signal the target CPU */

	unsigned int holdStat, holdParm0, holdParm1, holdParm2, mtype;
	struct per_proc_info *tpproc, *mpproc;			/* Area for per_proc addresses */
	int cpu;

#if DEBUG
	if(target > NCPUS) panic("cpu_signal: invalid target CPU - %08X\n", target);
#endif

	cpu = cpu_number();								/* Get our CPU number */
	if(target == cpu) return KERN_FAILURE;			/* Don't play with ourselves */
	if(!machine_slot[target].running) return KERN_FAILURE;	/* These guys are too young */	

	mpproc = &per_proc_info[cpu];					/* Point to our block */
	tpproc = &per_proc_info[target];				/* Point to the target's block */
	
	if(!hw_lock_mbits(&tpproc->MPsigpStat, MPsigpMsgp, 0, MPsigpBusy, 
	  (powermac_info.bus_clock_rate_hz >> 7))) {	/* Try to lock the message block */
		return KERN_FAILURE;						/* Timed out, take your ball and go home... */
	}

	holdStat = MPsigpBusy | MPsigpPass | (MPsigpSigp << 8) | cpu;	/* Set up the signal status word */
	tpproc->MPsigpParm0 = signal;					/* Set message order */
	tpproc->MPsigpParm1 = 0;						/* Clear additional parms for yuks */
	tpproc->MPsigpParm2 = 0;						/* Clear additional parms for yuks */
	
	__asm__ volatile("sync");						/* Make sure it's all there */
	
	tpproc->MPsigpStat = holdStat;					/* Set status and pass the lock */
	__asm__ volatile("eieio");						/* I'm a paraniod freak */
	
	PE_cpu_signal(mpproc->cpu_id, tpproc->cpu_id);	/* Kick the other processor */

	return KERN_SUCCESS;							/* All is goodness and rainbows... */
}

/*
 * TODO
 */
void init_ast_check(processor_t processor) 
{}

void cause_ast_check(processor_t processor)
{}       

void
switch_to_shutdown_context(
	thread_t        thread,
	void            (*doshutdown)(processor_t),
	processor_t     processor)
{ 
	printf("switch_to_shutdown_processor: not implemented\n");
}
 
