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
#include	<ppc/asm.h>
#include	<ppc/proc_reg.h>
#include	<ppc/exception.h>
#include	<mach/ppc/vm_param.h>
#include	<assym.s>

/*
** Blue Box Fast Trap entry
**
** The registers at entry are as hw_exceptions left them. Which means
** that the Blue Box data area is pointed to be R26.
**
** We exit here through the fast path exit point in hw_exceptions.  That means that
** upon exit, R4 must not change.  It is the savearea with the current user context
** to restore.
**
** Input registers are:
** r4  = Current context savearea (do not modify)
** r9  = THREAD_TOP_ACT pointer
** r24 = Offset into Call Descriptor table
** r26 = base of ACT_MACH_BDA in kernel address space
**
**
*/

ENTRY(atomic_switch_trap, TAG_NO_FRAME_USED)

/*
** functions 0-13, 15 -> Call PseudoKernel
**             	   14 -> Exit PseudoKernel
*/

			cmplwi	cr7,r24,BBEXITTRAP*PKTDSIZE	; Is this an exit?
			add		r24,r24,r26					; Point to the actual entry offset+base
			beq		cr7,.L_ExitPseudoKernel		; Yes...

/******************************************************************************
 * void                 CallPseudoKernel        ( void )
 *
 * This op provides a means of invoking the BlueBox PseudoKernel from a
 * system (68k) or native (PPC) context while changing BlueBox interruption
 * state atomically. As an added bonus, this op leaves the user state 
 * registers intact. 
 *
 * This op is invoked from the Emulator Trap dispatch table.  The kernel is
 * aware of starting address of this table.  It uses the users PC (SS_SRR0) 
 * and the Trap dispatch table address to verify the trap exception as a 
 * atomic_switch trap.  If a trap exception is verified as a atomic_switch
 * we enter here with the following registers loaded.
 *
 * Input registers are:
 * r4  = Current context savearea (do not modify)
 * r24 = Offset into PseudoKernel Trap Descriptor table
 * r26 = Base address of BlueBox Data in kernel address space 
 *
 ********************************************************************************/

			lwz		r8,BDA_INTCONTROLWORD(r26)	; Get the interruption control word (ICW)
			lis		r9,SYSCONTEXTSTATE			; Load system context constant
			rlwinm	r7,r8,0,INTSTATEMASK_B,INTSTATEMASK_E	; extract the current state from interruption control word
			lwz		r6,BDA_PKTD_NEWSTATE(r24)	; Get the new context state
			cmplw	r7,r9						; test for entry from system context
			rlwimi	r8,r6,0,INTSTATEMASK_B,INTSTATEMASK_E ; insert the new state into interruption control word
			lwz		r7,savecr(r4)				; Get the CR at interruption time
			bne		.L_CallFromAlternateContext	; We were in the alternate context...
			rlwimi	r8,r7,32-INTCR2TOBACKUPSHIFT,INTBACKUPCR2MASK_B,INTBACKUPCR2MASK_E
												; remove old backup CR2 and insert live CR2 into backup CR2 position 	

.L_CallFromAlternateContext:
			lwz		r5,saver2(r4)				; Get live R2
			lwz		r6,BDA_PKTD_PC(r24)			; Get the PC
			lwz		r7,savesrr1(r4)				; Get the interrupt-time MSR value
			stw		r8,BDA_INTCONTROLWORD(r26)	; update interruption control word
			rlwinm  r7,r7,0,MSR_FE1_BIT,MSR_FE0_BIT		; Clear BE and SE bits
			stw		r5,BDA_PKTD_REG(r24)		; Save R2 into call descriptor
			stw		r6,savesrr0(r4)				; Set the new instruction address
			stw     r7,savesrr1(r4)				; Set the updated msr
			b		EXT(fastexit)				; Go back and take the fast path exit...


/*******************************************************************************
 * void ExitPseudoKernel ( ExitPseudoKernelDescriptorPtr exitDescriptor )
 *
 * This op provides a means of exiting from the BlueBox PseudoKernel to a
 * user context while changing the BlueBox interruption state atomically.
 * It also allows the MSR's FE0,BE,SE and FE1 bits to updated for the user
 * and completes the PPC register loading.
 *
 * Input registers are:
 * r4  = Current context savearea (do not modify)
 * r24 = Offset into PseudoKernel Trap Descriptor table
 * r26 = Base address of BlueBox Data in kernel address space 
 *
*********************************************************************************/

.L_ExitPseudoKernel:
			lwz		r9,savesrr1(r4)				; Pick up the old MSR value	
			lwz		r8,savecr(r4)				; Get the live CR value
			lwz		r7,BDA_PKTD_NEWSTATE(r24)	; Get the new state
			lis		r0,SYSCONTEXTSTATE			; Get the system context constant
			lwz		r10,BDA_INTCONTROLWORD(r26)	; Get Interruption Control Word (ICW)
			lwz		r6,BDA_PKTD_PC(r24)			; Get the brand-new spanking clean inst address
			cmplw	r7,r0						; Are we going system context?
			lwz		r5,BDA_PKTD_MSR(r24)		; Pick up new MSR state
			rlwimi	r10,r7,0,INTSTATEMASK_B,INTSTATEMASK_E
												; Insert the new state into interruption control word
			beq		.L_ExitToSystemContext		; We are going to system context...
			lwz		r7,BDA_TESTINTMASK(r26)		; Get the pending interrupt mask
			lwz		r14,BDA_PKTD_INTPENDINGPC(r26)	; Get the interruption exit address
			and.	r7,r10,r7					; test for pending interrupt in backup cr2
			beq		.L_ExitUpdateRuptControlWord	; and enter alternate context if none pending
			mr		r6,r14						; otherwise, introduce entry abort pc
			b		.L_ExitNoUpdateRuptControlWord	; and prepare to reenter pseudokernel

.L_ExitToSystemContext:
			rlwimi	r8,r10,INTCR2TOBACKUPSHIFT,INTCR2MASK_B,INTCR2MASK_E
												; remove old CR2 and insert backup CR2 into live CR2 position 	
.L_ExitUpdateRuptControlWord:
			rlwimi	r9,r5,0,MSR_FE0_BIT,MSR_FE1_BIT	; Insert FE0, BE, SE and FE1 into MSR
			stw		r10,BDA_INTCONTROLWORD(r26)	; update the interrupt control word

.L_ExitNoUpdateRuptControlWord:
			lwz		r7,BDA_PKTD_REG(r24)		; Get the new CTR
			stw		r6,savesrr0(r4)				; Set the new PC
			stw		r7,savectr(r4)				; Set the new CTR
			stw		r8,savecr(r4)				; Set the new CR
			stw		r9,savesrr1(r4)				; Set the new MSR

			b		EXT(fastexit)				; Go back and take the fast path exit...

