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

#ifndef	_PPC_THREAD_ACT_H_
#define	_PPC_THREAD_ACT_H_

#include <mach_kgdb.h>
#include <mach/boolean.h>
#include <mach/ppc/vm_types.h>
#include <mach/thread_status.h>
#include <kern/lock.h>


/*
 * Kernel state structure
 *
 * This holds the kernel state that is saved and restored across context
 * switches. This is kept at the top of the kernel stack.
 *
 * XXX Some state is saved only because it is not saved on entry to the
 * kernel from user mode. This needs to be straightened out.
 */

/*
 * PPC process control block
 *
 * In the continuation model, the PCB holds the user context. It is used
 * on entry to the kernel from user mode, either by system call or trap,
 * to store the necessary user registers and other state.
 *
 *	Note that this structure overlays a savearea.  Make sure that these
 *	guys are updated in concert with that.
 */
struct pcb
{
	struct ppc_saved_state ss;
	struct ppc_exception_state es;
	struct ppc_float_state fs;
	unsigned int gas1[6];				/* Force alignment with savearea */
	struct ppc_vector_state vec;

};

typedef struct pcb *pcb_t;

/*
 * Maps state flavor to number of words in the state:
 */
extern unsigned int state_count[];

#define USER_REGS(ThrAct)	(&(ThrAct)->mact.pcb->ss)

#define	user_pc(ThrAct)		((ThrAct)->mact.pcb->ss.srr0)

#define act_machine_state_ptr(ThrAct)	(thread_state_t)USER_REGS(ThrAct)

typedef struct MachineThrAct {
	/*
	 * pointer to process control block control blocks.  Potentially
	 * one for each active facility context.  They may point to the
	 * same saveareas.
	 */
	pcb_t 			pcb;			/* The "normal" savearea */
	pcb_t			FPU_pcb;		/* The floating point savearea */
	pcb_t			FPU_lvl;		/* The floating point context level */
	unsigned int	FPU_cpu;		/* The last processor to enable floating point */
	pcb_t 			VMX_pcb;		/* The VMX savearea */
	pcb_t 			VMX_lvl;		/* The VMX context level */
	unsigned int	VMX_cpu;		/* The last processor to enable vector */
	unsigned int	ksp;			/* points to TOP OF STACK or zero */
	unsigned int	bbDescAddr;		/* Points to Blue Box descriptor area in kernel (page aligned) */
	unsigned int	bbUserDA;		/* Points to Blue Box descriptor area in user (page aligned) */
	unsigned int	bbTableStart;	/* Start of Blue Box trap table start */
	unsigned int	bbPendRupt;		/* Number of pending interruptions */
	unsigned int	specFlags;		/* Special flags */
#define ignoreZeroFault 0x80000000
#define floatUsed 0x40000000
#define vectorUsed 0x20000000
#ifdef	MACH_BSD
	unsigned long	cthread_self;	/* for use of cthread package */
#endif

} MachineThrAct, *MachineThrAct_t;

extern struct ppc_saved_state * find_user_regs(thread_act_t act);
extern struct ppc_float_state * find_user_fpu(thread_act_t act);

#endif	/* _PPC_THREAD_ACT_H_ */
