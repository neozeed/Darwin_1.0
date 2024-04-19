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
 	File:		PseudoKernel.c

 	Contains:	BlueBox PseudoKernel calls

 	Copyright:	1997 by Apple Computer, Inc., all rights reserved

*/

#include <mach/mach_types.h>
#include <mach/kern_return.h>
#include <kern/host.h>
#include <kern/task.h>
#include <kern/thread.h>
#include <ppc/PseudoKernel.h>
#include <ppc/exception.h>
#include <vm/vm_kern.h>

void bbSetRupt(ReturnHandler *rh, thread_act_t ct);
void DumpTheSave(struct savearea *save);			/* (TEST/DEBUG) */

/*
** Function:	NotifyInterruption
**
** Inputs:
**		ppcInterrupHandler	- interrupt handler to execute
**		interruptStatePtr	- current interrupt state
**
** Outputs:
**
** Notes:
**
*/
kern_return_t syscall_notify_interrupt (UInt32 ppcInterruptHandler) {
  
    UInt32		interruptState; 
    task_t		task;
    spl_t 		s;
	thread_act_t 	act, fact;
	thread_t	thread;
	bbRupt		*bbr;
	int			i;

	task = current_task();					/* Figure out who our task is */

	task_lock(task);						/* Lock our task */
	mutex_lock(&task->act_list_lock);		/* Lock the list so it can't change on us */
	
	fact = (thread_act_t)task->thr_acts.next;	/* Get the first activation on task */
	act = 0;								/* Pretend we didn't find it yet */
	
	for(i = 0; i < task->thr_act_count; i++) {	/* Scan the whole list */
		if(fact->mact.bbDescAddr) {			/* Is this the Blue thread? */
			act = fact;						/* Yeah... */
			break;							/* Bail the loop... */
		}
		fact = (thread_act_t)fact->thr_acts.next;	/* Go to the next one */
	}
	
	mutex_unlock(&task->act_list_lock);

	if(!act) {								/* Couldn't find a bluebox */
		task_unlock(task);					/* Release task lock */
		return KERN_FAILURE;				/* No tickie, no shirtee... */
	}
	
	act_lock_thread(act);					/* Make sure this stays 'round */
	task_unlock(task);						/* Safe to release now */
	
	if(act->mact.bbPendRupt >= 16) {		/* Have we hit the arbitrary maximum? */
		act_unlock_thread(act);				/* Unlock the activation */
		return KERN_RESOURCE_SHORTAGE;		/* Too many pending right now */
	}
	
	if(!(bbr = (bbRupt *)kalloc(sizeof(bbRupt)))) {	/* Get a return handler control block */
		act_unlock_thread(act);				/* Unlock the activation */
		return KERN_RESOURCE_SHORTAGE;		/* No storage... */
	}
	
	act->mact.bbPendRupt++;					/* Count this 'rupt */
	bbr->rh.handler = bbSetRupt;			/* Set interruption routine */
	bbr->ruptRtn = ppcInterruptHandler;		/* Set address to vector to */

	bbr->rh.next = act->handlers;			/* Put our interrupt at the start of the list */
	act->handlers = &bbr->rh;

	s = splsched();							/* No talking in class */
	act_set_apc(act);						/* Set an APC AST */
	splx(s);								/* Ok, you can talk now */

	act_unlock_thread(act);					/* Unlock the activation */
	return KERN_SUCCESS;					/* We're done... */
}

/* 
 *	This guy is fired off asynchronously to actually do the 'rupt.
 *	We will find the user state savearea and modify it.  If we can't,
 *	we just leave after releasing our work area
 */

void bbSetRupt(ReturnHandler *rh, thread_act_t act) {

	savearea 	*sv;
	BDA_t		*bda;
	bbRupt		*bbr;
	UInt32		interruptState;
	
	bbr = (bbRupt *)rh;						/* Make our area convenient */

	if(!(bda = (BDA_t *)act->mact.bbDescAddr)) {	/* Is BlueBox still enabled? */
		kfree((vm_offset_t)bbr, sizeof(bbRupt));	/* No, release the control block */
		return;
	}
	
	act->mact.bbPendRupt--;					/* Uncount this 'rupt */

	if(!(sv = (savearea *)find_user_regs(act))) {	/* Find the user state registers */
		kfree((vm_offset_t)bbr, sizeof(bbRupt));	/* Couldn't find 'em, release the control block */
		return;
	}
		
    interruptState = (bda->InterruptControlWord & kInterruptStateMask) >> kInterruptStateShift; 

    switch (interruptState) {
		
		case kInUninitialized:
			break;
				
		case kInPseudoKernel:
		case kOutsideBlue:
			bda->InterruptControlWord = bda->InterruptControlWord | 
				((bda->postIntMask >> kCR2ToBackupShift) & kBackupCR2Mask);
			break;
			
		case kInSystemContext:
			sv->save_cr |= bda->postIntMask;
			break;
			
		case kInAlternateContext:
			bda->InterruptControlWord = bda->InterruptControlWord |
				((bda->postIntMask >> kCR2ToBackupShift) & kBackupCR2Mask);
			bda->InterruptControlWord = (bda->InterruptControlWord & ~kInterruptStateMask) | 
				(kInPseudoKernel << kInterruptStateShift);
				
			bda->ihs_pc = sv->save_srr0;		/* Save the current PC */
			sv->save_srr0 = bbr->ruptRtn;		/* Set the new one */
			bda->ihs_gpr2 = sv->save_r2;		/* Save the original R2 */
			break;
			
		case kInExceptionHandler:
			bda->InterruptControlWord = bda->InterruptControlWord |
				((bda->postIntMask >> kCR2ToBackupShift) & kBackupCR2Mask);
			break;
			
		default:
			break;
	}

	kfree((vm_offset_t)bbr, sizeof(bbRupt));	/* Release the control block */
	return;

}

kern_return_t enable_bluebox(
      host_t host,
	  void *TWI_TableStart,									/* Start of TWI table */
	  char *Desc_TableStart									/* Start of descriptor table */
	 ) {
	
	thread_t 		th;
	vm_offset_t		kerndescaddr, physdescaddr;
	kern_return_t 	ret;
	int 			i;
	
	th = current_thread();									/* Get our thread */					

	if (host == HOST_NULL) return KERN_INVALID_HOST;
	
	if(!is_suser()) return KERN_FAILURE;					/* We will only do this for the superuser */
	if(th->top_act->mact.bbDescAddr) return KERN_FAILURE;	/* Bail if already authorized... */
	if((unsigned int) Desc_TableStart & (PAGE_SIZE - 1))	/* Is the descriptor page aligned? */ 
		return KERN_FAILURE;								/* No, kick 'em out... */

	ret = vm_map_wire(th->top_act->map, 					/* Kernel wire the descriptor in the user's map */
		(vm_offset_t)Desc_TableStart,
		(vm_offset_t)Desc_TableStart + PAGE_SIZE,
		VM_PROT_READ | VM_PROT_WRITE,
		FALSE);															
		
	if(ret != KERN_SUCCESS) {								/* Couldn't wire it, spit on 'em... */
		return KERN_FAILURE;	
	}
		
	physdescaddr = 											/* Get the physical address of the page */
		pmap_extract(th->top_act->map->pmap, (vm_offset_t) Desc_TableStart);

	ret =  kmem_alloc_pageable(kernel_map, &kerndescaddr, PAGE_SIZE);	/* Find a virtual address to use */
	if(ret != KERN_SUCCESS) {								/* Could we get an address? */
		(void) vm_map_unwire(th->top_act->map,				/* No, unwire the descriptor */
			(vm_offset_t)Desc_TableStart,
			(vm_offset_t)Desc_TableStart + PAGE_SIZE,
			TRUE);
		return KERN_FAILURE;								/* Split... */
	}
	
	(void) pmap_enter(kernel_pmap, 							/* Map this into the kernel */
		kerndescaddr, physdescaddr, VM_PROT_READ|VM_PROT_WRITE, 
		TRUE);
		
	th->top_act->mact.bbDescAddr = (unsigned int)kerndescaddr;	/* Set kernel address of the table */
	th->top_act->mact.bbUserDA = (unsigned int)Desc_TableStart;	/* Set user address of the table */
	th->top_act->mact.bbTableStart = (unsigned int)TWI_TableStart;	/* Set address of the trap table */
	th->top_act->mact.bbPendRupt = 0;						/* Clean pending 'rupt count */
		
	return KERN_SUCCESS;

}

kern_return_t disable_bluebox( host_t host ) {
	
	thread_act_t 	act;
	
	act = current_act();									/* Get our thread */					

	if (host == HOST_NULL) return KERN_INVALID_HOST;
	
	if(!is_suser()) return KERN_FAILURE;					/* We will only do this for the superuser */
	if(!act->mact.bbDescAddr) return KERN_FAILURE;			/* Bail if not authorized... */

	(void) vm_map_unwire(act->map,							/* Unwire the descriptor in user's address space */
		(vm_offset_t)act->mact.bbUserDA,
		(vm_offset_t)act->mact.bbUserDA + PAGE_SIZE,
		FALSE);
		
	kmem_free(kernel_map, (vm_offset_t)act->mact.bbDescAddr, PAGE_SIZE);	/* Release the page */
	
	act->mact.bbDescAddr = 0;								/* Clear kernel pointer to it */
	act->mact.bbUserDA = 0;									/* Clear user pointer to it */
	act->mact.bbTableStart = 0;								/* Clear start of table address */
	act->mact.bbPendRupt = 0;								/* Clean pending 'rupt count */
	return KERN_SUCCESS;
}
