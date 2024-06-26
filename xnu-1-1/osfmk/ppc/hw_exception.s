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

/* Low level routines dealing with exception entry and exit.
 * There are various types of exception:
 *
 *    Interrupt, trap, system call and debugger entry. Each has it's own
 *    handler since the state save routine is different for each. The
 *    code is very similar (a lot of cut and paste).
 *
 *    The code for the FPU disabled handler (lazy fpu) is in cswtch.s
 */

#include <debug.h>
#include <mach_assert.h>
#include <mach/exception_types.h>
#include <mach/ppc/vm_param.h>

#include <assym.s>

#include <ppc/asm.h>
#include <ppc/proc_reg.h>
#include <ppc/trap.h>
#include <ppc/exception.h>
#include <ppc/spl.h>


#define VERIFYSAVE 0
#define FPVECDBG 0
	
/*
 * thandler(type)
 *
 * ENTRY:	VM switched ON
 *			Interrupts  OFF
 *			R3 contains exception code
 *			R4 points to the saved context (virtual address)
 *			Everything is saved in savearea
 */

/*
 * If pcb.ksp == 0 then the kernel stack is already busy,
 *                 we save ppc_saved state below the current stack pointer,
 *		   leaving enough space for the `red zone' in case the
 *		   trapped thread was in the middle of saving state below
 *		   its stack pointer.
 *
 * otherwise       we save a ppc_saved_state in the pcb, and switch to
 * 		   the kernel stack (setting pcb.ksp to 0)
 *
 * on return, we do the reverse, the last state is popped from the pcb
 * and pcb.ksp is set to the top of stack                  
 */


#if DEBUG

/* TRAP_SPACE_NEEDED is the space assumed free on the kernel stack when
 * another trap is taken. We need at least enough space for a saved state
 * structure plus two small backpointer frames, and we add a few
 * hundred bytes for the space needed by the C (which may be less but
 * may be much more). We're trying to catch kernel stack overflows :-)
 */

#define TRAP_SPACE_NEEDED	FM_REDZONE+(2*FM_SIZE)+256

#endif /* DEBUG */

			.text

ENTRY(thandler, TAG_NO_FRAME_USED)	/* What tag should this have?! */

			mfsprg	r25,0						/* Get the per_proc */
		
			lwz		r1,PP_ISTACKPTR(r25)		; Get interrupt stack pointer
	

			lwz		r6,PP_CPU_DATA(r25)			/* Get point to cpu specific data */
			cmpwi	cr0,r1,0					; Are we on interrupt stack?					
			lwz		r6,CPU_ACTIVE_THREAD(r6)	/* Get the pointer to the currently active thread */
			beq-	cr0,EXT(ihandler)			; Yes, not allowed except when debugger
												; is active.  We will let the ihandler do this...
			lwz		r9,THREAD_TOP_ACT(r6)		/* Point to the active activation */
			lwz		r26,ACT_MACT_BDA(r9)		/* Pick up the pointer to the blue box data area */
			lwz		r8,ACT_MACT_PCB(r9)			/* Get the last savearea used */
			mr.		r26,r26						/* Do we have Blue Box Assist active? */
			lwz		r1,ACT_MACT_KSP(r9)			/* Get the stack */
			bnel-	checkassist					/* See if we should assist this */
			stw		r4,ACT_MACT_PCB(r9)			/* Point to our savearea */
			stw		r8,SAVprev(r4)				/* Queue the new save area in the front */
			
#if VERIFYSAVE
			bl		versave						; (TEST/DEBUG)
#endif
			
			cmpwi	cr1,r1,	0					/* zero implies already on kstack */
			stw		r9,SAVact(r4)				/* Point the savearea at its activation */
			bne		cr1,.L_kstackfree			/* This test is also used below */		
			lwz		r1,saver1(r4)				/* Get the stack at 'rupt time */


/* On kernel stack, allocate stack frame and check for overflow */
#if DEBUG
/*
 * Test if we will overflow the Kernel Stack. We 
 * check that there is at least TRAP_SPACE_NEEDED bytes
 * free on the kernel stack
*/

			lwz		r7,THREAD_KERNEL_STACK(r6)
			addi	r7,r7,TRAP_SPACE_NEEDED
			cmp		cr0,r1,r7
			bng-	EXT(ihandler)
#endif /* DEBUG */

			subi	r1,r1,FM_REDZONE			/* Back up stack and leave room for a red zone */ 

.L_kstackfree:
#if 0
			lis		r0,HIGH_ADDR(CutTrace)		/* (TEST/DEBUG) */
			oris	r0,r0,LOW_ADDR(CutTrace)	/* (TEST/DEBUG) */
			sc									/* (TEST/DEBUG) */
#endif
			lwz		r7,savesrr1(r4)				/* Pick up the entry MSR */
			mfmsr	r8							/* Get the kernel's MSR state */
			li		r0,0						/* Make this 0 */
			rlwimi	r8,r7,0,MSR_EE_BIT,MSR_EE_BIT	/* Copy in the interrupt mask from the save area */
			
			beq		cr1,.L_state_on_kstack		/* using above test for pcb/stack */

			stw		r0,ACT_MACT_KSP(r9)			/* Show that we have taken the stack */

.L_state_on_kstack:	
			rlwinm.	r6,r7,0,MSR_VEC_BIT,MSR_VEC_BIT	; Was vector on?
			lwz		r6,saver1(r4)				/* Grab interrupt time stack */
			beq+	tvecoff						; Vector off, do not save vrsave...
			mfspr	r7,vrsave					; Get the VRSAVE register
			stw		r7,liveVRS(r25)				; Set the live value

tvecoff:	subi	r1,r1,FM_SIZE				/* Push a header onto the current stack */
			stw		r6,FM_BACKPTR(r1)			/* Link backwards */

#if	DEBUG
/* If debugging, we need two frames, the first being a dummy
 * which links back to the trapped routine. The second is
 * that which the C routine below will need
 */
			lwz		r7,savesrr0(r4)				/* Get the point of interruption */
			stw		r7,FM_LR_SAVE(r1)			/* save old instr ptr as LR value */
			stwu	r1,	-FM_SIZE(r1)			/* and make new frame */
#endif /* DEBUG */


/* call trap handler proper, with
 *   ARG0 = type		(not yet, holds pcb ptr)
 *   ARG1 = saved_state ptr	(already there)
 *   ARG2 = dsisr		(already there)
 *   ARG3 = dar			(already there)
 */

			lwz		r3,saveexception(r4)		/* Get the exception code */
			lwz		r5,savedsisr(r4)			/* Get the saved DSISR */
			lwz		r6,savedar(r4)				/* Get the DAR */
	
			mtmsr	r8							/* Turn on interrupts if enabled at 'rupt time */

/* syscall exception might warp here if there's nothing left
 * to do except generate a trap
 */

.L_call_trap:	
#if 0
			lis		r0,HIGH_ADDR(CutTrace)		/* (TEST/DEBUG) */
			oris	r0,r0,LOW_ADDR(CutTrace)	/* (TEST/DEBUG) */
			sc									/* (TEST/DEBUG) */
#endif

			bl	EXT(trap)

/*
 * Ok, return from C function
 *
 * This is also the point where new threads come when they are created.
 * The new thread is setup to look like a thread that took an 
 * interrupt and went immediatly into trap.
 *
 */

thread_return:

			lwz		r4,SAVprev(r3)				/* Pick up the previous savearea */
			mfmsr	r7							/* Get the MSR */
			lwz		r11,SAVflags(r3)			/* Get the flags of the current savearea */
			rlwinm	r7,r7,0,MSR_EE_BIT+1,MSR_EE_BIT-1	/* Clear the interrupt enable mask */
			mtmsr	r7							/* Disable for interrupts */
		
			mfsprg	r10,0						/* Restore the per_proc info */
			
			lwz		r8,savesrr1(r3)				; Get the MSR we are going to
			lwz		r1,PP_CPU_DATA(r10)			/* Get the CPU data area */
			rlwinm	r11,r11,0,15,13				/* Clear the syscall flag */
			lwz		r1,CPU_ACTIVE_THREAD(r1)	/* and the active thread */
			rlwinm.	r8,r8,0,MSR_PR_BIT,MSR_PR_BIT	; Are we going to the user?
			lwz		r8,THREAD_TOP_ACT(r1)		/* Now find the current activation */
			stw		r11,SAVflags(r3)			/* Save back the flags (with reset stack cleared) */

#if 0
			lis		r0,HIGH_ADDR(CutTrace)		/* (TEST/DEBUG) */
			oris	r0,r0,LOW_ADDR(CutTrace)	/* (TEST/DEBUG) */
			sc									/* (TEST/DEBUG) */
#endif
			stw		r4,ACT_MACT_PCB(r8)			/* Point to the previous savearea (or 0 if none) */

			beq-	chkfac						; We are not leaving the kernel yet...

			lwz		r5,THREAD_KERNEL_STACK(r1)	/* Get the base pointer to the stack */
			addi	r5,r5,KERNEL_STACK_SIZE-FM_SIZE	/* Reset to empty */
			stw		r5,ACT_MACT_KSP(r8)			/* Save the empty stack pointer */
			b		chkfac						/* Go end it all... */



/*
 * shandler(type)
 *
 * ENTRY:	VM switched ON
 *			Interrupts  OFF
 *			R3 contains exception code
 *			R4 points to the saved context (virtual address)
 *			Everything is saved in savearea
 */

/*
 * If pcb.ksp == 0 then the kernel stack is already busy,
 *                 this is an error - jump to the debugger entry
 * 
 * otherwise       depending upon the type of
 *                 syscall, look it up in the kernel table
 *		   		   or pass it to the server.
 *
 * on return, we do the reverse, the state is popped from the pcb
 * and pcb.ksp is set to the top of stack.
 */

ENTRY(shandler, TAG_NO_FRAME_USED)

			mfsprg	r25,0						/* Get the per proc area */
			lwz		r0,saver0(r4)				/* Get the original syscall number */
			lwz		r17,PP_ISTACKPTR(r25)		; Get interrupt stack pointer
			andi.	r15,r0,0x7000				/* Isolate the fastpath code */
			lwz		r16,PP_CPU_DATA(r25)		/* Assume we need this */
			mr.		r17,r17						; Are we on interrupt stack?
			lwz		r7,savesrr1(r4)				; Get the SRR1 value
			beq-	EXT(ihandler)				; On interrupt stack, not allowed...
			lwz		r16,CPU_ACTIVE_THREAD(r16)	/* Get the thread pointer */


			rlwinm.	r6,r7,0,MSR_VEC_BIT,MSR_VEC_BIT	; Was vector on?
			beq+	svecoff						; Vector off, do not save vrsave...
			mfspr	r7,vrsave					; Get the VRSAVE register
			stw		r7,liveVRS(r25)				; Set the live value

svecoff:	lwz		r13,THREAD_TOP_ACT(r16)		/* Pick up the active thread */
			cmplwi	r15,0x7000					/* Do we have a fast path trap? */
			lwz		r14,ACT_MACT_PCB(r13)		/* Now point to the PCB */
			beql+	fastpath					/* We think it's a fastpath... */

			lwz		r1,ACT_MACT_KSP(r13)		/* Get the kernel stack pointer */
#if DEBUG
			mr.		r1,r1						/* Are we already on the kernel stack? */
			li		r3,T_SYSTEM_CALL			/* Yup, pretend we had an interrupt... */
			beq-	EXT(ihandler)				/* Bad boy, bad boy... What'cha gonna do when they come for you? */
#endif /* DEBUG */

			stw		r4,ACT_MACT_PCB(r13)		/* Point to our savearea */
			li		r0,0						/* Clear this out */
			stw		r14,SAVprev(r4)				/* Queue the new save area in the front */
			stw		r13,SAVact(r4)				/* Point the savearea at its activation */
			
#if VERIFYSAVE
			bl		versave						; (TEST/DEBUG)
#endif			
			
			mr		r30,r4						/* Save pointer to the new context savearea */
			lwz		r15,saver1(r4)				/* Grab interrupt time stack */
			stw		r0,ACT_MACT_KSP(r13)		/* Mark stack as busy with 0 val */
			stw		r15,FM_BACKPTR(r1)			/* Link backwards */
		
#if	DEBUG
	/* If debugging, we need two frames, the first being a dummy
	 * which links back to the trapped routine. The second is
	 * that which the C routine below will need
	 */
			lwz		r8,savesrr0(r30)			/* Get the point of interruption */
			stw		r8,FM_LR_SAVE(r1)			/* save old instr ptr as LR value */
			stwu	r1,	-FM_SIZE(r1)			/* and make new frame */
#endif /* DEBUG */

			mfmsr	r0							/* Get the MSR */
			lwz		r15,SAVflags(r4)			/* Get the savearea flags */
			ori		r0,r0,lo16(MASK(MSR_EE))	/* Turn on interruption enabled bit */
			oris	r15,r15,SAVsyscall >> 16 	/* Mark that it this is a syscall */
			stwu	r1,-(FM_SIZE+ARG_SIZE)(r1)	/* Make a stack frame */
			stw		r15,SAVflags(r30)			/* Save syscall marker */

			mtmsr	r0							/* Enable interruptions */


			/* Call a function that can print out our syscall info */
			/* Note that we don't care about any volatiles yet */
			mr		r4,r30
			bl		EXT(syscall_trace)	
	
			lwz		r0,saver0(r30)				/* Get the system call selector */
			mr.		r0,r0						/* What kind is it? */
			blt-	.L_kernel_syscall			/* -ve syscall - go to kernel */
												/* +ve syscall - go to server */
			cmpwi	cr0,	r0,0x7FFA
			beq-	.L_notify_interrupt_syscall
												/* +ve syscall - go to server */
#ifdef MACH_BSD
			mr		r3,r30						/* Get PCB/savearea */
			lwz		r4,saver4(r30)  		/* Restore r4 */
			lwz		r5,saver5(r30)  		/* Restore r5 */
			lwz		r6,saver6(r30)  		/* Restore r6 */
			lwz		r7,saver7(r30)  		/* Restore r7 */
			lwz		r8,saver8(r30)  		/* Restore r8 */
			lwz		r9,saver9(r30)  		/* Restore r9 */
			lwz		r10,saver10(r30)  		/* Restore r10 */
			bl		EXT(unix_syscall)			/* Check out unix... */
#endif

.L_call_server_syscall_exception:		
			li		r3,EXC_SYSCALL				/* doexception(EXC_SYSCALL, num, 1) */

.L_call_server_exception:
			mr		r4,r0						/* Set syscall selector */
			li		r5,1
			b		EXT(doexception)			/* Go away, never to return... */

/* The above, but with EXC_MACH_SYSCALL */
.L_call_server_mach_syscall:
			li		r3,EXC_MACH_SYSCALL
			b		.L_call_server_exception	/* Join the common above... */

.L_notify_interrupt_syscall:
			lwz		r3,saver3(r30)				; Get the new PC address to pass in
			bl		EXT(syscall_notify_interrupt)
			b		.L_syscall_return
	


/* Once here, we know that the syscall was -ve
 * we should still have r1=ksp,
 * r16		= pointer to current thread,
 * r13		= pointer to top activation,
 * r0		= syscall number
 * r30		= pointer to saved state (in pcb)
 */
.L_kernel_syscall:	

/*
 * Are we allowed to do mach system calls?
 */

			addis	r31,0,HIGH_ADDR(EXT(realhost))
			lwz		r29,ACT_MACH_EXC_PORT(r13)	/* See if we have one of these ports */
			ori		r31,r31,LOW_ADDR(EXT(realhost))
			lwz		r31,HOST_SELF(r31)			/* Find ourselves */

/* If thread_exception_port == realhost->hostname do syscall */	
			cmpw	cr1,r29,r31					/* Are we the host? */
			mr.		r29,r29						/* Is our port null? */
			beq+	cr1,.L_syscall_do_mach_syscall	/* We are the host... */
			bne+	.L_call_server_mach_syscall	/* Not host and not null, call the handler... */

			lwz		r29,ACT_TASK(r13)			/* Get our task */
			lwz		r29,TASK_MACH_EXC_PORT(r29)	/* Now get the exception port for it */

/* If task_exception_port == realhost->hostname do syscall */	
			cmpw	cr1,r29,r31					/* Is this the host? */
			mr.		r29,r29						/* Is it null? */
			beq+	cr1,.L_syscall_do_mach_syscall	/* We are the host... */
			bne+	.L_call_server_mach_syscall	/* Not host and not null, call the handler... */

/* else the syscall has failed, treat as priv instruction trap,
 * set SRR1 to indicate privileged instruction
 */
			
			li		r3,T_PROGRAM
			lwz		r29,savesrr1(r30)
			mr		r4,r30
			li		r5,0
			oris	r29,r29,MASK(SRR1_PRG_PRV_INS)>>16
			li		r6,0
			stw		r29,savesrr1(r30)
			b		.L_call_trap

/* When here, we know that we're allowed to do a mach syscall,
 * and still have syscall number in r0, pcb pointer in r30
 */

.L_syscall_do_mach_syscall:
			neg	r31,	r0		/* Make number +ve and put in r31*/

#ifdef	MACHO_SYSCALL_BEGIN
/*
 * There's a bit of a problem with supporting both ELF and
 * MACHO generated binaries. Each has their own location for
 * storing the 8th and above arguments on the stack.
 *
 * Current the MACHO generated system calls have a BIT
 * set telling the kernel its a MACHO style ABI.
 */

			li	r29,MACHO_SYSCALL_BEGIN
			and.	r25,r31,r29
			beq	not_macho

			andc	r31,r31,r29		/* Clear the bit to get real trap */
			li	r25,FM_MACHO_ARG0-4
			b	continue2

not_macho:
			li	r25,FM_ELF_ARG0-4

continue2:
#endif

	/* If out of range, call server with syscall exception */
	addis	r29,	0,	HIGH_CADDR(EXT(mach_trap_count))
	addi	r29,	r29,	LOW_ADDR(EXT(mach_trap_count))
	lwz	r29,	0(r29)

	cmp	cr0,	r31,	r29
	bge-	cr0,	.L_call_server_syscall_exception

	addis	r29,	0,	HIGH_CADDR(EXT(mach_trap_table))
	addi	r29,	r29,	LOW_ADDR(EXT(mach_trap_table))
	
	/* multiply the trap number to get offset into table */
	slwi	r31,	r31,	MACH_TRAP_OFFSET_POW2

	/* r31 now holds offset into table of our trap entry,
	 * add on the table base, and it then holds pointer to entry
	 */
	add	r31,	r31,	r29

	/* If the function is kern_invalid, prepare to send an exception.
	   This is messy, but parallels the x86.  We need it for task_by_pid,
	   at least.  */
	lis	r29,	HIGH_CADDR(EXT(kern_invalid))
	addi	r29,	r29,	LOW_ADDR(EXT(kern_invalid))
	lwz	r0,	MACH_TRAP_FUNCTION(r31)
	cmp	cr0,	r0,	r29
	beq-	.L_call_server_syscall_exception

	/* get arg count. If argc > 8 then not all args were in regs,
	 * so we must perform copyin.
	 */
	lwz	r29,	MACH_TRAP_ARGC(r31)
	cmpwi	cr0,	r29,	8
	ble+	.L_syscall_got_args

/* argc > 8  - perform a copyin */
/* if the syscall came from kernel space, we can just copy */

			lwz		r0,savesrr1(r30)				/* Pick up exception time MSR */
			andi.	r0,r0,MASK(MSR_PR)				/* Check the priv bit */
			bne+	.L_syscall_arg_copyin			/* We're not priviliged... */

/* we came from a privilaged task, just do a copy */
/* get user's stack pointer */

			lwz		r28,saver1(r30)					/* Get the stack pointer */

			subi	r29,r29,8						/* Get the number of arguments to copy */

#ifdef	MACHO_SYSCALL_BEGIN
			add		r28,r28,r25						/* Point to source - 4 */
#else
			addi	r28,r28,COPYIN_ARG0_OFFSET-4	/* Point to source - 4 */
#endif
			addi	r27,r1,FM_ARG0-4				/* Point to sink - 4 */

.L_syscall_copy_word_loop:
			addic.	r29,r29,-1						/* Count down the number of arguments left */
			lwz		r0,4(r28)						/* Pick up the argument from the stack */
			addi	r28,r28,4						/* Point to the next source */
			stw		r0,4(r27)						/* Store the argument */
			addi	r27,r27,4						/* Point to the next sink */
			bne+	.L_syscall_copy_word_loop		/* Move all arguments... */
			b		.L_syscall_got_args				/* Go call it now... */


/* we came from a user task, pay the price of a real copyin */	
/* set recovery point */

.L_syscall_arg_copyin:
			addis	r28,0,HIGH_CADDR(.L_syscall_copyin_recover)
			addi	r28,r28,LOW_ADDR(.L_syscall_copyin_recover)
			stw		r28,THREAD_RECOVER(r16) 		/* R16 still holds thread ptr */

/* We can manipulate the COPYIN segment register quite easily
 * here, but we've also got to make sure we don't go over a
 * segment boundary - hence some mess.
 * Registers from 12-29 are free for our use.
 */
	

			lwz		r28,saver1(r30)					/* Get the stack pointer */
			subi	r29,r29,8						/* Get the number of arguments to copy */
			mfsr	r10,SR_UNUSED_BY_KERN			; Get the user state SR value

#ifdef	MACHO_SYSCALL_BEGIN
			add		r28,r28,r25						/* Set source in user land */
			addi	r28,r28,4
#else
			addi	r28,r28,COPYIN_ARG0_OFFSET	/* Set source in user land */
#endif

/* set up SR_COPYIN to allow us to copy, we may need to loop
 * around if we change segments. We know that this previously
 * pointed to user space, so the sid doesn't need setting.
 */
.L_syscall_copyin_seg_loop:
			
			rlwimi	r10,r28,24,8,11					; Blast the segment number into the SR
			rlwinm	r26,r28,0,4,31					; Clear the segment number from source address
			mtsr	SR_COPYIN,r10					; Set the copyin SR
			isync

			oris	r26,r26,(SR_COPYIN_NUM << (28-16))	; Insert the copyin segment number into source address
	
/* Make r27 point to address-4 of where we will store copied args */
			addi	r27,r1,FM_ARG0-4
	
.L_syscall_copyin_word_loop:
			
			lwz		r0,0(r26)						/* MAY CAUSE PAGE FAULT! */
			subi	r29,r29,1						; Decrement count
			addi	r26,r26,4						; Bump input
			stw		r0,4(r27)						; Save the copied in word
			mr.		r29,r29							; Are they all moved?
			addi	r27,r27,4						; Bump output
			beq+	.L_syscall_copyin_done			; Escape if we are done...
	
			rlwinm.	r0,r26,0,4,29					; Did we just step into a new segment?		
			addi	r28,r28,4						; Bump up user state address also
			bne+	.L_syscall_copyin_word_loop		; We are still on the same segment...
			b		.L_syscall_copyin_seg_loop		/* On new segment! remap */

/* Don't bother restoring SR_COPYIN, we can leave it trashed */
/* clear thread recovery as we're done touching user data */

.L_syscall_copyin_done:	
			li		r0,0
			stw		r0,THREAD_RECOVER(r16) /* R16 still holds thread ptr */

.L_syscall_got_args:
			lwz		r8,ACT_TASK(r13)		/* Get our task */
			lis		r10,hi16(EXT(c_syscalls_mach))	/* Get top half of counter address */
			lwz		r7,TASK_SYSCALLS_MACH(r8)		; Get the current count
			lwz		r3,saver3(r30)  		/* Restore r3 */
			addi	r7,r7,1					; Bump it
			ori		r10,r10,lo16(EXT(c_syscalls_mach)) /* Get low half of counter address */
			stw		r7,TASK_SYSCALLS_MACH(r8)		; Save it
			lwz		r4,saver4(r30)  		/* Restore r4 */
			lwz		r9,0(r10)				/* Get counter */	
			lwz		r5,saver5(r30)  		/* Restore r5 */
			lwz		r6,saver6(r30)  		/* Restore r6 */
			addi	r9,r9,1					/* Add 1 */
			lwz		r7,saver7(r30)  		/* Restore r7 */
			lwz		r8,saver8(r30)  		/* Restore r8 */
			stw		r9,0(r10)				/* Save it back	*/
			lwz		r9,saver9(r30)  		/* Restore r9 */
			lwz		r10,saver10(r30)  		/* Restore r10 */

			lwz		r0,MACH_TRAP_FUNCTION(r31)

/* calling this function, all the callee-saved registers are
 * still valid except for r30 and r31 which are in the PCB
 * r30 holds pointer to saved state (ie. pcb)
 * r31 is scrap
 */
			mtctr	r0
			bctrl							/* perform the actual syscall */

/* 'standard' syscall returns here - INTERRUPTS ARE STILL ON */

/* r3 contains value that we're going to return to the user
 */

/*
 * Ok, return from C function, ARG0 = return value
 *
 * get the active thread's PCB pointer and thus pointer to user state
 * saved state is still in R30 and the active thread is in R16	.	
 */

/* Store return value into saved state structure, since
 * we need to pick up the value from here later - the
 * syscall may perform a thread_set_syscall_return
 * followed by a thread_exception_return, ending up
 * at thread_syscall_return below, with SS_R3 having
 * been set up already
 */

/* When we are here, r16 should point to the current thread,
 *                   r30 should point to the current pcb
 */

/* save off return value, we must load it
 * back anyway for thread_exception_return
 * TODO NMGS put in register?
 */
.L_syscall_return:	
			mr		r31,r16								/* Move the current thread pointer */
			stw		r3,saver3(r30)						/* Stash the return code */
	
			/* Call a function that records the end of */
			/* the mach system call */
			mr		r4,r30
			bl		EXT(syscall_trace_end)	
	
#if 0
			lis		r0,HIGH_ADDR(CutTrace)				/* (TEST/DEBUG) */
			mr		r4,r31								/* (TEST/DEBUG) */
			oris	r0,r0,LOW_ADDR(CutTrace)			/* (TEST/DEBUG) */
			mr		r5,r30								/* (TEST/DEBUG) */
			sc											/* (TEST/DEBUG) */
#endif

.L_thread_syscall_ret_check_ast:	
			mfmsr	r12									/* Get the current MSR */
			rlwinm	r12,r12,0,MSR_EE_BIT+1,MSR_EE_BIT-1	/* Turn of interruptions enable bit */
			mtmsr	r12									/* Turn interruptions off */
			
			mfsprg	r10,0								/* Get the per_processor block */

/* Check to see if there's an outstanding AST */
		
			lwz		r4,PP_NEED_AST(r10)
			lwz		r4,0(r4)
			cmpi	cr0,r4,	0
			beq		cr0,.L_syscall_no_ast

/* Yes there is, call ast_taken 
 * pretending that the user thread took an AST exception here,
 * ast_taken will save all state and bring us back here
 */

#if	DEBUG
/* debug assert - make sure that we're not returning to kernel */
			lwz		r3,savesrr1(r30)
			andi.	r3,r3,MASK(MSR_PR)
			bne+	0f									/* returning to user level, check */
			
			BREAKPOINT_TRAP
0:		
#endif	/* DEBUG */
	
			li	r3,	0
			li	r4,	AST_ALL
			li	r5,	1
			bl	EXT(ast_taken)
			
			b	.L_thread_syscall_ret_check_ast

/* thread_exception_return returns to here, almost all
 * registers intact. It expects a full context restore
 * of what it hasn't restored itself (ie. what we use).
 *
 * In particular for us,
 * we still have     r31 points to the current thread,
 *                   r30 points to the current pcb
 */
 
.L_syscall_no_ast:
.L_thread_syscall_return:

			mr		r3,r30						; Get savearea to the correct register for common exit
			lwz		r8,THREAD_TOP_ACT(r31)		/* Now find the current activation */

			lwz		r11,SAVflags(r30)			/* Get the flags */
			lwz		r5,THREAD_KERNEL_STACK(r31)	/* Get the base pointer to the stack */
			rlwinm	r11,r11,0,15,13				/* Clear the syscall flag */
			lwz		r4,SAVprev(r30)				; Get the previous save area
			stw		r11,SAVflags(r30)			/* Stick back the flags */
			addi	r5,r5,KERNEL_STACK_SIZE-FM_SIZE	/* Reset to empty */
			stw		r4,ACT_MACT_PCB(r8)			; Save previous save area
			stw		r5,ACT_MACT_KSP(r8)			/* Save the empty stack pointer */
		
			b		chkfac						; Go end it all...


.L_syscall_copyin_recover:

	/* This is the catcher for any data faults in the copyin
	 * of arguments from the user's stack.
	 * r30 still holds a pointer to the PCB
	 *
	 * call syscall_error(EXC_BAD_ACCESS, EXC_PPC_VM_PROT_READ, sp, ssp),
	 *
	 * we already had a frame so we can do this
	 */	
	
			li		r3,EXC_BAD_ACCESS
			li		r4,EXC_PPC_VM_PROT_READ
			lwz		r5,saver1(r30)
			mr		r6,r30
		
			bl		EXT(syscall_error)
			b		.L_syscall_return


/*
 * thread_exception_return()
 *
 * Return to user mode directly from within a system call.
 */

ENTRY(thread_exception_return, TAG_NO_FRAME_USED)

.L_thread_exc_ret_check_ast:	

			mfmsr	r3							/* Get the MSR */
			rlwinm	r3,r3,0,MSR_EE_BIT+1,MSR_EE_BIT-1	/* Clear EE */
			mtmsr	r3							/* Disable interrupts */

/* Check to see if there's an outstanding AST */
/* We don't bother establishing a call frame even though CHECK_AST
   can invoke ast_taken(), because it can just borrow our caller's
   frame, given that we're not going to return.  
*/
		
			mfsprg	r10,0						/* Get the per_processor block */
			lwz		r4,PP_NEED_AST(r10)
			lwz		r4,0(r4)
			cmpi	cr0,r4,	0
			beq		cr0,.L_exc_ret_no_ast
		
	/* Yes there is, call ast_taken 
	 * pretending that the user thread took an AST exception here,
	 * ast_taken will save all state and bring us back here
	 */
	
#if	DEBUG
	/* debug assert - make sure that we're not returning to kernel
	 *
	 * get the active thread's PCB pointer and thus pointer to user state
	 */
			lwz		r30,PP_CPU_DATA(r10)
			lwz		r30,CPU_ACTIVE_THREAD(r30)
			lwz		r30,THREAD_TOP_ACT(r30)
			lwz		r30,ACT_MACT_PCB(r30)
		
			lwz		r3,savesrr1(r30)
			andi.	r3,r3,MASK(MSR_PR)
			bne+	ret_user2			/* returning to user level, check */

			BREAKPOINT_TRAP
ret_user2:		
#endif	/* DEBUG */

			li		r3,0
			li		r4,AST_ALL
			li		r5,1
			
			bl		EXT(ast_taken)
			b	.L_thread_exc_ret_check_ast	/* check for a second AST (rare)*/
	
/* arriving here, interrupts should be disabled */
/* Get the active thread's PCB pointer to restore regs
 */
.L_exc_ret_no_ast:
			
			lwz		r31,PP_CPU_DATA(r10)
			lwz		r31,CPU_ACTIVE_THREAD(r31)
			lwz		r30,THREAD_TOP_ACT(r31)
			lwz		r30,ACT_MACT_PCB(r30)
		
/* If the MSR_SYSCALL_MASK isn't set, then we came from a trap,
 * so warp into the return_from_trap (thread_return) routine,
 * which takes PCB pointer in R3, not in r30!
 */
			lwz		r0,SAVflags(r30)
			mr		r3,r30								/* Copy pcb pointer into r3 in case */
			andis.	r0,r0,SAVsyscall>>16				/* Are we returning from a syscall? */
			beq-	cr0,thread_return					/* Nope, must be a thread return... */
			b		.L_thread_syscall_return
		
/*
 * thread_bootstrap_return()
 *
 */

ENTRY(thread_bootstrap_return, TAG_NO_FRAME_USED)

			mfmsr	r12									/* Get the current MSR */
			rlwinm	r12,r12,0,MSR_EE_BIT+1,MSR_EE_BIT-1	/* Turn of interruptions enable bit */
			mtmsr	r12									/* Turn interruptions off */
		
			mfsprg	r10,0								/* Get the per_processor block */


#if 0
			lis		r0,HIGH_ADDR(CutTrace)				/* (TEST/DEBUG) */
			oris	r0,r0,LOW_ADDR(CutTrace)			/* (TEST/DEBUG) */
			sc											/* (TEST/DEBUG) */
#endif
	
			lwz		r31,PP_CPU_DATA(r10)
			
			lwz		r31,CPU_ACTIVE_THREAD(r31)
			lwz		r31,THREAD_TOP_ACT(r31)

.L_bootstrap_ret_check_ast:
			mfmsr	r12									/* Get the current MSR */
			rlwinm	r12,r12,0,MSR_EE_BIT+1,MSR_EE_BIT-1	/* Turn off interruptions enable bit */
			mtmsr	r12									/* Turn interruptions off */
			mfsprg	r10,0								/* Get the per_processor block */
/* Check for any outstanding ASTs and deal with them */
			lwz		r4,PP_NEED_AST(r10)
			lwz		r4,0(r4)
			cmpi	cr0,r4,	0
			beq		cr0,.L_bootstrap_ret_no_ast
	
/* we have a pending AST, now is the time to jump, args as below
 * void ast_taken(boolean_t preemption, ast_t mask, boolean_t interrupts)
 */
			li		r3,	0
			li		r4,AST_ALL
			li		r5,1

			bl	EXT(ast_taken)
			b		.L_bootstrap_ret_check_ast			/* Check for another AST (rare) */
	

/* Back from dealing with ASTs, if there were any.
 * r31 still holds pointer to ACT.
 * 
 * Time to deal with kloading or kloaded threads
 */
.L_bootstrap_ret_no_ast:
			lwz		r3,ACT_KLOADING(r31)
			li		r0,0
			cmpi	cr0,r3,	0
			li		r9,1
			beq+	.L_bootstrap_ret_no_kload
			stw		r0,ACT_KLOADING(r31)
			lwz		r3,PP_ACTIVE_KLOADED(r10)
			stw		r0,ACT_KLOADED(r31)
			stw		r31,0(r3)

/* Ok, we're all set, jump to thread_return as if we
 * were just coming back from a trap (ie. r3 set up to point to pcb)
 */	
.L_bootstrap_ret_no_kload:

			lwz		r3,ACT_MACT_PCB(r31)		/* Point to our saved context */
#if 0
			lis		r0,HIGH_ADDR(CutTrace)		/* (TEST/DEBUG) */
			oris	r0,r0,LOW_ADDR(CutTrace)	/* (TEST/DEBUG) */
			sc									/* (TEST/DEBUG) */
#endif
	
			b		thread_return
	
/*
 * ihandler(type)
 *
 * ENTRY:	VM switched ON
 *			Interrupts  OFF
 *			R3 contains exception code
 *			R4 points to the saved context (virtual address)
 *			Everything is saved in savearea
 *
 */

ENTRY(ihandler, TAG_NO_FRAME_USED)	/* What tag should this have?! */

/*
 * get the value of istackptr, if it's zero then we're already on the
 * interrupt stack, otherwise it points to a saved_state structure
 * at the top of the interrupt stack.
 */

			lwz		r10,savesrr1(r4)			/* Get SRR0 */
			mfsprg	r25,0						/* Get the per_proc block */
			li		r14,0						/* Zero this for now */
			lwz		r16,PP_CPU_DATA(r25)		/* Assume we need this */
			rlwinm.	r10,r10,0,MSR_VEC_BIT,MSR_VEC_BIT	; Was vector on?
			lwz		r1,	PP_ISTACKPTR(r25)		/* Get the interrupt stack */
			li		r13,0						/* Zero this for now */
			lwz		r16,CPU_ACTIVE_THREAD(r16)	/* Get the thread pointer */

			beq+	ivecoff						; Vector off, do not save vrsave...
			mfspr	r7,vrsave					; Get the VRSAVE register
			stw		r7,liveVRS(r25)				; Set the live value

ivecoff:	li		r0,0						/* Get a constant 0 */
			cmplwi	cr1,r16,0					/* Are we still booting? */
			mr.		r1,r1						/* Is it active? */
			beq-	cr1,ihboot1					/* We're still coming up... */
			lwz		r13,THREAD_TOP_ACT(r16)		/* Pick up the active thread */
			lwz		r14,ACT_MACT_PCB(r13)		/* Now point to the PCB */

ihboot1:	lwz		r9,saver1(r4)				/* Pick up the 'rupt time stack */
			stw		r14,SAVprev(r4)				/* Queue the new save area in the front */
			stw		r13,SAVact(r4)				/* Point the savearea at its activation */
			beq-	cr1,ihboot4					/* We're still coming up... */
			stw		r4,ACT_MACT_PCB(r13)		/* Point to our savearea */

ihboot4:	bne		.L_istackfree				/* Nope... */

/* We're already on the interrupt stack, get back the old
 * stack pointer and make room for a frame
 */

			subi	r1,r9,FM_REDZONE			/* Back up beyond the red zone */
			b		ihsetback					/* Go  set up the back chain... */
	
.L_istackfree:
			lwz		r10,SAVflags(r4)			
			stw		r0,PP_ISTACKPTR(r25)		/* Mark the stack in use */
			oris	r10,r10,HIGH_ADDR(SAVrststk)	/* Indicate we reset stack when we return from this one */
			stw		r10,SAVflags(r4)			/* Stick it back */		
	
	/*
	 * To summarise, when we reach here, the state has been saved and
	 * the stack is marked as busy. We now generate a small
	 * stack frame with backpointers to follow the calling
	 * conventions. We set up the backpointers to the trapped
	 * routine allowing us to backtrace.
	 */
	
ihsetback:	subi	r1,r1,FM_SIZE				/* Make a new frame */
			stw		r9,FM_BACKPTR(r1)			/* point back to previous stackptr */
		
#if VERIFYSAVE
			bl		versave						; (TEST/DEBUG)
#endif

#if	DEBUG
/* If debugging, we need two frames, the first being a dummy
 * which links back to the trapped routine. The second is
 * that which the C routine below will need
 */
			lwz		r5,savesrr0(r4)				/* Get interrupt address */
			stw		r5,FM_LR_SAVE(r1)			/* save old instr ptr as LR value */
			stwu	r1,-FM_SIZE(r1)				/* Make another new frame for C routine */
#endif /* DEBUG */

			lwz		r5,savedsisr(r4)			/* Get the DSISR */
			lwz		r6,savedar(r4)				/* Get the DAR */
			
			bl	EXT(interrupt)


/* interrupt() returns a pointer to the saved state in r3
 *
 * Ok, back from C. Disable interrupts while we restore things
 */
			.globl EXT(ihandler_ret)

LEXT(ihandler_ret)								/* Marks our return point from debugger entry */

			mfmsr	r0							/* Get our MSR */
			rlwinm	r0,r0,0,MSR_EE_BIT+1,MSR_EE_BIT-1	/* Flip off the interrupt enabled bit */
			mtmsr	r0							/* Make sure interrupts are disabled */
			mfsprg	r10,0						/* Get the per_proc block */
		
			lwz		r8,PP_CPU_DATA(r10)			/* Get the CPU data area */
			lwz		r7,SAVflags(r3)				/* Pick up the flags */
			lwz		r8,CPU_ACTIVE_THREAD(r8)	/* and the active thread */
			lwz		r9,SAVprev(r3)				/* Get previous save area */
			cmplwi	cr1,r8,0					/* Are we still initializing? */
			lwz		r12,savesrr1(r3)			/* Get the MSR we will load on return */
			beq-	cr1,ihboot2					/* Skip if we are still in init... */
			lwz		r8,THREAD_TOP_ACT(r8)		/* Pick up the active thread */

ihboot2:	andis.	r11,r7,HIGH_ADDR(SAVrststk)	/* Is this the first on the stack? */
			beq-	cr1,ihboot3					/* Skip if we are still in init... */
			stw		r9,ACT_MACT_PCB(r8)			/* Point to previous context savearea */

ihboot3:	mr		r4,r3						/* Move the savearea pointer */
			beq		.L_no_int_ast2				/* Get going if not the top o' stack... */


/* We're the last frame on the stack. Restore istackptr to empty state.
 *
 * Check for ASTs if one of the below is true:	
 *    returning to user mode
 *    returning to a kloaded server
 */
			lwz		r9,PP_INTSTACK_TOP_SS(r10)	/* Get the empty stack value */
			andc	r7,r7,r11					/* Remove the stack reset bit in case we pass this one */
			stw		r9,PP_ISTACKPTR(r25)		/* Save that saved state ptr */
			andi.	r6,r12,MASK(MSR_PR)			/* Are we going to problem state? (Sorry, ancient IBM term for non-privileged) */
			stw		r7,SAVflags(r4)				/* Save the flags */
			beq-	.L_no_int_ast				; In kernel space, no AST check...

			lwz		r11,PP_NEED_AST(r10)		/* Get the AST request address */
			lwz		r11,0(r11)					/* Get the request */
			li		r3,T_AST					/* Assume the worst */
			mr.		r11,r11						/* Are there any pending? */
			beq		.L_no_int_ast				/* Nope... */

/*
 * There is a pending AST. Massage things to make it look like
 * we took a trap and jump into the trap handler.  To do this
 * we essentially pretend to return from the interrupt but
 * at the last minute jump into the trap handler with an AST
 * trap instead of performing an rfi.
 */

			stw		r3,saveexception(r4)		/* Set the exception code to T_AST */
			b		EXT(thandler)				/* hyperspace into AST trap */

.L_no_int_ast:	
			mr		r3,r4						; Get into the right register for common code
.L_no_int_ast2:	
			rlwinm	r7,r7,0,15,13				/* Clear the syscall bit */
			li		r4,0						; Assume for a moment that we are in init
			stw		r7,SAVflags(r3)				/* Set the flags */
			beq-	cr1,chkfac					; Jump away if we are in init...
			lwz		r4,ACT_MACT_PCB(r8)			; Get the new level marker


;
;			This section is common to all exception exits.  It throws away vector
;			and floating point saveareas as the exception level of a thread is
;			exited.  
;
;			It also enables the facility if its context is live
;			Requires:
;				R3  = Savearea to be released (virtual)
;				R4  = New top of savearea stack (could be 0)
;				R8  = pointer to activation
;				R10 = per_proc block
;
chkfac:		mr.		r8,r8						; Are we still in boot?
			beq-	chkenax						; Yeah, skip it all...
			
			lwz		r20,ACT_MACT_FPUlvl(r8)		; Get the FPU level
			lwz		r12,savesrr1(r3)			; Get the current MSR
			cmplw	r20,r3						; Are we returning from the active level?
			lwz		r23,PP_FPU_THREAD(r10)		; Get floating point owner
			rlwinm	r12,r12,0,MSR_FP_BIT+1,MSR_FP_BIT-1	; Turn off floating point for now
			cmplw	cr1,r23,r8					; Are we the facility owner?
			lhz		r26,PP_CPU_NUMBER(r10)		; Get the current CPU number
			beq-	chkfpfree					; Leaving active level, can not possibly enable...
			bne-	cr1,chkvec					; Not our facility, nothing to do here...

#if FPVECDBG
			lis		r0,HIGH_ADDR(CutTrace)		; (TEST/DEBUG)
			li		r2,0x3301					; (TEST/DEBUG)
			oris	r0,r0,LOW_ADDR(CutTrace)	; (TEST/DEBUG)
			sc									; (TEST/DEBUG)
#endif	

			lwz		r24,ACT_MACT_FPUcpu(r8)		; Get the CPU this context was enabled on last
			cmplw	r4,r20						; Are we going to be in the right level?
			cmplw	cr1,r24,r26					; Are we on the right CPU?
			li		r0,0						; Get a constant 0
			beq+	cr1,chkfpnlvl				; Right CPU...
			
			stw		r0,PP_FPU_THREAD(r10)		; Show facility unowned so we do not get back here
			b		chkvec						; Go check out the vector facility...
			
chkfpnlvl:	bne-	chkvec						; Different level, can not enable...
			lwz		r24,ACT_MACT_FPU(r8)		; Get the floating point save area
			ori		r12,r12,lo16(MASK(MSR_FP))	; Enable facility
			mr.		r24,r24						; Does the savearea exist?
			li		r0,1						; Get set to invalidate
			beq-	chkvec						; Nothing to invalidate...
			lwz		r25,SAVlvlfp(r24)			; Get the level of top savearea
			cmplw	r4,r25						; Is the top one ours?
			bne+	chkvec						; Not ours...
			stw		r0,SAVlvlfp(r24)			; Invalidate the first one

#if 0
			mfmsr	r0						; (TEST/DEBUG)
			ori		r0,r0,0x2000			; (TEST/DEBUG)
			mtmsr	r0						; (TEST/DEBUG)
			isync							; (TEST/DEBUG)
			
			stfd	f0,savevr0(r3)			; (TEST/DEBUG)
			stfd	f1,savevr0+8(r3)		; (TEST/DEBUG)
			stfd	f2,savevr0+0x10(r3)		; (TEST/DEBUG)
			stfd	f3,savevr0+0x18(r3)		; (TEST/DEBUG)
			stfd	f4,savevr0+0x20(r3)		; (TEST/DEBUG)
			stfd	f5,savevr0+0x28(r3)		; (TEST/DEBUG)
			stfd	f6,savevr0+0x30(r3)		; (TEST/DEBUG)
			stfd	f7,savevr0+0x38(r3)		; (TEST/DEBUG)
			stfd	f8,savevr0+0x40(r3)		; (TEST/DEBUG)
			stfd	f9,savevr0+0x48(r3)		; (TEST/DEBUG)
			stfd	f10,savevr0+0x50(r3)	; (TEST/DEBUG)
			stfd	f11,savevr0+0x58(r3)	; (TEST/DEBUG)
			stfd	f12,savevr0+0x60(r3)	; (TEST/DEBUG)
			stfd	f13,savevr0+0x68(r3)	; (TEST/DEBUG)
			stfd	f14,savevr0+0x70(r3)	; (TEST/DEBUG)
			stfd	f15,savevr0+0x78(r3)	; (TEST/DEBUG)
			stfd	f16,savevr0+0x80(r3)	; (TEST/DEBUG)
			stfd	f17,savevr0+0x88(r3		; (TEST/DEBUG)
			stfd	f18,savevr0+0x90(r3)	; (TEST/DEBUG)
			stfd	f19,savevr0+0x98(r3)	; (TEST/DEBUG)
			stfd	f20,savevr0+0xA0(r3)	; (TEST/DEBUG)
			stfd	f21,savevr0+0xA8(r3)	; (TEST/DEBUG)
			stfd	f22,savevr0+0xB0(r3)	; (TEST/DEBUG)
			stfd	f23,savevr0+0xB8(r3)	; (TEST/DEBUG)
			stfd	f24,savevr0+0xC0(r3)	; (TEST/DEBUG)
			stfd	f25,savevr0+0xC8(r3)	; (TEST/DEBUG)
			stfd	f26,savevr0+0xD0(r3)	; (TEST/DEBUG)
			stfd	f27,savevr0+0xD8(r3)	; (TEST/DEBUG)
			stfd	f28,savevr0+0xE0(r3)	; (TEST/DEBUG)
			stfd	f29,savevr0+0xE8(r3)	; (TEST/DEBUG)
			stfd	f30,savevr0+0xF0(r3)	; (TEST/DEBUG)
			stfd	f31,savevr0+0xF8(r3)	; (TEST/DEBUG)

			li		r2,64					; (TEST/DEBUG)
			la		r20,savevr0(r3)			; (TEST/DEBUG)
			la		r21,savefp0(r24)		; (TEST/DEBUG)
			
ckmurderdeath2:
			lwz		r22,0(r20)				; (TEST/DEBUG)
			subic.	r2,r2,1					; (TEST/DEBUG)
			lwz		r23,0(r21)				; (TEST/DEBUG)
			addi	r20,r20,4				; (TEST/DEBUG)
			cmplw	cr1,r22,r23				; (TEST/DEBUG)
			addi	r21,r21,4				; (TEST/DEBUG)
			bne-	cr1,diekilldead2		; (TEST/DEBUG)
			bne+	ckmurderdeath2			; (TEST/DEBUG)
			b		dontdiekilldead2		; (TEST/DEBUG)
			
diekilldead2:								; (TEST/DEBUG)
			mr		r4,r24					; (TEST/DEBUG)
			BREAKPOINT_TRAP					; (TEST/DEBUG)
			
dontdiekilldead2:
			lfd		f0,savevr0(r3)			; (TEST/DEBUG)
			lfd		f1,savevr0+8(r3)		; (TEST/DEBUG)
#endif



			b		chkvec						; Go check out the vector facility...

chkfpfree:	li		r0,0						; Clear a register
			lwz		r24,ACT_MACT_FPU(r8)		; Get the floating point save area
			
			bne-	cr1,chkfpnfr				; Not our facility, do not clear...
			stw		r0,PP_FPU_THREAD(r10)		; Clear floating point owner
chkfpnfr:

#if FPVECDBG
			lis		r0,HIGH_ADDR(CutTrace)		; (TEST/DEBUG)
			li		r2,0x3302					; (TEST/DEBUG)
			oris	r0,r0,LOW_ADDR(CutTrace)	; (TEST/DEBUG)
			sc									; (TEST/DEBUG)
#endif	

			mr.		r24,r24						; Do we even have a savearea?
			beq+	chkvec						; Nope...
			
#if FPVECDBG
			rlwinm.	r0,r24,0,0,15				; (TEST/DEBUG)
			bne+	notbadxxx1					; (TEST/DEBUG)
			BREAKPOINT_TRAP						; (TEST/DEBUG)
notbadxxx1:										; (TEST/DEBUG)			
			lis		r0,HIGH_ADDR(CutTrace)		; (TEST/DEBUG)
			li		r2,0x3303					; (TEST/DEBUG)
			oris	r0,r0,LOW_ADDR(CutTrace)	; (TEST/DEBUG)
			sc									; (TEST/DEBUG)
#endif	

			lwz		r25,SAVlvlfp(r24)			; Get the level of top savearea
			cmplwi	r25,1						; Is the top area invalid?
			cmplw	cr1,r25,r3					; Is it for the returned from context?
			beq		fptoss						; It is invalid...
			bne		cr1,chkvec					; Not for the returned context...
			
fptoss:		lwz		r25,SAVprefp(r24)			; Get previous savearea
#if FPVECDBG
			lis		r0,HIGH_ADDR(CutTrace)		; (TEST/DEBUG)
			li		r2,0x3304					; (TEST/DEBUG)
			oris	r0,r0,LOW_ADDR(CutTrace)	; (TEST/DEBUG)
			mr		r5,r25						; (TEST/DEBUG)
			sc									; (TEST/DEBUG)
#endif	
			mr.		r25,r25						; Is there one?
			stw		r25,ACT_MACT_FPU(r8)		; Set the new pointer
			beq		fptoplvl					; Nope, we are at the top...
#if FPVECDBG		
			rlwinm.	r0,r25,0,0,15				; (TEST/DEBUG)
			bne+	notbadxxx2					; (TEST/DEBUG)
			BREAKPOINT_TRAP						; (TEST/DEBUG)
notbadxxx2:										; (TEST/DEBUG)			
#endif		
			lwz		r25,SAVlvlfp(r25)			; Get the new level

fptoplvl:	lwz		r19,SAVflags(r24)			; Get the savearea flags
#if FPVECDBG
			rlwinm.	r0,r19,0,1,1				; (TEST/DEBUG)
			bne+	donotdie3					; (TEST/DEBUG)
			BREAKPOINT_TRAP						; (TEST/DEBUG)
donotdie3:										; (TEST/DEBUG)
#endif

#if FPVECDBG
			lis		r0,HIGH_ADDR(CutTrace)		; (TEST/DEBUG)
			li		r2,0x3305					; (TEST/DEBUG)
			oris	r0,r0,LOW_ADDR(CutTrace)	; (TEST/DEBUG)
			sc									; (TEST/DEBUG)
#endif	
			rlwinm	r22,r24,0,0,19				; Round down to the base savearea block
			rlwinm	r19,r19,0,2,0				; Remove the floating point in use flag
			stw		r25,ACT_MACT_FPUlvl(r8)		; Set the new top level
			andis.	r0,r19,hi16(SAVinuse)		; Still in use?
			stw		r19,SAVflags(r24)			; Set the savearea flags
			bne-	chkvec						; Yes, go check out vector...
#if FPVECDBG
			lis		r0,HIGH_ADDR(CutTrace)		; (TEST/DEBUG)
			li		r2,0x3306					; (TEST/DEBUG)
			oris	r0,r0,LOW_ADDR(CutTrace)	; (TEST/DEBUG)
			sc									; (TEST/DEBUG)
#endif	
#if FPVECDBG		
			rlwinm.	r0,r24,0,0,15				; (TEST/DEBUG)
			bne+	notbadxxx3					; (TEST/DEBUG)
			BREAKPOINT_TRAP						; (TEST/DEBUG)
notbadxxx3:										; (TEST/DEBUG)			
#endif		
			lwz		r23,SACvrswap(r22)			; Get the conversion from virtual to real
			lwz		r20,PP_QUICKFRET(r10)		; Get the old quick fret head
			xor		r23,r24,r23					; Convert to physical
			stw		r20,SAVqfret(r24)			; Back chain the quick release queue
			stw		r23,PP_QUICKFRET(r10)		; Anchor it

;
;			Check out vector stuff (and translate savearea to physical for exit)
;
chkvec:		
			lwz		r20,ACT_MACT_VMXlvl(r8)		; Get the vector level
			lwz		r23,PP_VMX_THREAD(r10)		; Get vector owner
			cmplw	r20,r3						; Are we returning from the active level?
			rlwinm	r12,r12,0,MSR_VEC_BIT+1,MSR_VEC_BIT-1	; Turn off vector for now
			cmplw	cr1,r23,r8					; Are we the facility owner?
			beq-	chkvecfree					; Leaving active level, can not possibly enable...
			bne-	cr1,setena					; Not our facility, nothing to do here...

#if FPVECDBG
			lis		r0,HIGH_ADDR(CutTrace)		; (TEST/DEBUG)
			li		r2,0x3401					; (TEST/DEBUG)
			oris	r0,r0,LOW_ADDR(CutTrace)	; (TEST/DEBUG)
			sc									; (TEST/DEBUG)
#endif	

			lwz		r24,ACT_MACT_VMXcpu(r8)		; Get the CPU this context was enabled on last
			cmplw	r4,r20						; Are we going to be in the right level?
			cmplw	cr1,r24,r26					; Are we on the right CPU?
			li		r0,0						; Get a constant 0
			beq+	cr1,chkvecnlvl				; Right CPU...
			
			stw		r0,PP_VMX_THREAD(r10)		; Show facility unowned so we do not get back here
			b		setena						; Go actually exit...
			
chkvecnlvl:	bne-	setena						; Different level, can not enable...
			lwz		r24,ACT_MACT_VMX(r8)		; Get the vector save area
			oris	r12,r12,hi16(MASK(MSR_VEC))	; Enable facility
			mr.		r24,r24						; Does the savearea exist?
			li		r0,1						; Get set to invalidate
			beq-	setena						; Nothing to invalidate...
			lwz		r25,SAVlvlvec(r24)			; Get the level of top savearea
			cmplw	r4,r25						; Is the top one ours?
			bne+	setena						; Not ours...
			stw		r0,SAVlvlvec(r24)			; Invalidate the first one
			b		setena						; Actually exit...

chkvecfree:	li		r0,0						; Clear a register
			lwz		r24,ACT_MACT_VMX(r8)		; Get the vector save area
			bne-	cr1,chkvecnfr				; Not our facility, do not clear...
			stw		r0,PP_VMX_THREAD(r10)		; Clear vector owner
chkvecnfr:
			
#if FPVECDBG
			lis		r0,HIGH_ADDR(CutTrace)		; (TEST/DEBUG)
			li		r2,0x3402					; (TEST/DEBUG)
			oris	r0,r0,LOW_ADDR(CutTrace)	; (TEST/DEBUG)
			sc									; (TEST/DEBUG)
#endif	

			mr.		r24,r24						; Do we even have a savearea?
			beq+	setena						; Nope...
			
#if FPVECDBG
			lis		r0,HIGH_ADDR(CutTrace)		; (TEST/DEBUG)
			li		r2,0x3403					; (TEST/DEBUG)
			oris	r0,r0,LOW_ADDR(CutTrace)	; (TEST/DEBUG)
			sc									; (TEST/DEBUG)
#endif	
			lwz		r25,SAVlvlvec(r24)			; Get the level
			cmplwi	r25,1						; Is the top area invalid?
			cmplw	cr1,r25,r3					; Is it for the returned from context?
			beq		vectoss						; It is invalid...
			bne		cr1,setena					; Not for the returned context...
			
vectoss:	lwz		r25,SAVprevec(r24)			; Get previous savearea
#if FPVECDBG
			lis		r0,HIGH_ADDR(CutTrace)		; (TEST/DEBUG)
			li		r2,0x3504					; (TEST/DEBUG)
			oris	r0,r0,LOW_ADDR(CutTrace)	; (TEST/DEBUG)
			mr		r5,r25						; (TEST/DEBUG)
			sc									; (TEST/DEBUG)
#endif	
			mr.		r25,r25						; Is there one?
			stw		r25,ACT_MACT_VMX(r8)		; Set the new pointer
			beq		vectoplvl					; Nope, we are at the top...
			lwz		r25,SAVlvlvec(r25)			; Get the new level

vectoplvl:	lwz		r19,SAVflags(r24)			; Get the savearea flags

#if FPVECDBG
			lis		r0,HIGH_ADDR(CutTrace)		; (TEST/DEBUG)
			li		r2,0x3405					; (TEST/DEBUG)
			oris	r0,r0,LOW_ADDR(CutTrace)	; (TEST/DEBUG)
			sc									; (TEST/DEBUG)
#endif	
			rlwinm	r22,r24,0,0,19				; Round down to the base savearea block
			rlwinm	r19,r19,0,3,1				; Remove the vector in use flag
			stw		r25,ACT_MACT_VMXlvl(r8)		; Set the new top level
			andis.	r0,r19,hi16(SAVinuse)		; Still in use?
			stw		r19,SAVflags(r24)			; Set the savearea flags
			bne-	setena						; Yes, all done...
#if FPVECDBG
			lis		r0,HIGH_ADDR(CutTrace)		; (TEST/DEBUG)
			li		r2,0x3406					; (TEST/DEBUG)
			oris	r0,r0,LOW_ADDR(CutTrace)	; (TEST/DEBUG)
			sc									; (TEST/DEBUG)
#endif			
			lwz		r23,SACvrswap(r22)			; Get the conversion from virtual to real
			lwz		r20,PP_QUICKFRET(r10)		; Get the old quick fret head
			xor		r23,r24,r23					; Convert to physical
			stw		r20,SAVqfret(r24)			; Back chain the quick release queue
			stw		r23,PP_QUICKFRET(r10)		; Anchor it

setena:		stw		r12,savesrr1(r3)			; Turn facility on or off
	
chkenax:	lwz		r6,SAVflags(r3)				; Pick up the flags of the old savearea

	
#if DEBUG
			lwz		r20,SAVact(r3)				; (TEST/DEBUG) Make sure our restore
			lwz		r21,PP_CPU_DATA(r10)		; (TEST/DEBUG)         context is associated
			lwz		r21,CPU_ACTIVE_THREAD(r21)	; (TEST/DEBUG)         with the current act.
			cmpwi	r21,0						; (TEST/DEBUG)
			beq-	yeswereok					; (TEST/DEBUG)
			lwz		r21,THREAD_TOP_ACT(r21)		; (TEST/DEBUG)
			cmplw	r21,r20						; (TEST/DEBUG)
			beq+	yeswereok					; (TEST/DEBUG)
			BREAKPOINT_TRAP						; (TEST/DEBUG)

yeswereok:
#endif
	
			rlwinm	r5,r3,0,0,19				; Round savearea down to page bndry
			rlwinm	r6,r6,0,1,31				; Mark savearea free
			lwz		r5,SACvrswap(r5)			; Get the conversion from virtual to real
			stw		r6,SAVflags(r3)				; Set savearea flags
			xor		r3,r3,r5					; Flip to physical address
			b		EXT(exception_exit)			; We are all done now...



/*
 *			Here's where we handle the fastpath stuff
 *			We'll do what we can here because registers are already
 *			loaded and it will be less confusing that moving them around.
 *			If we need to though, we'll branch off somewhere's else.
 *
 *			Registers when we get here:
 *
 *				r0  = syscall number
 *				r4  = savearea/pcb
 *				r13 = activation
 *				r14 = previous savearea (if any)
 *				r16 = thread
 *				r25 = per_proc
 */

fastpath:	cmplwi	cr3,r0,0x7FF1				; Is it CthreadSetSelfNumber? 	
			bnelr-	cr3							; Not a fast path...

/*
 * void cthread_set_self(cproc_t p)
 *
 * set's thread state "user_value"
 *
 * This op is invoked as follows:
 *	li r0, CthreadSetSelfNumber	// load the fast-trap number
 *	sc				// invoke fast-trap
 *	blr
 *
 */

CthreadSetSelfNumber:

			lwz		r5,saver3(r4)				/* Retrieve the self number */
			stw		r5,CTHREAD_SELF(r13)		/* Remember it */
			stw		r5,UAW(r25)					/* Prime the per_proc_info with it */


			.globl	EXT(fastexit)
EXT(fastexit):
			lwz		r8,SAVflags(r4)				/* Pick up the flags */
			rlwinm	r9,r4,0,0,19				/* Round down to the base savearea block */
			rlwinm	r8,r8,0,1,31				/* Clear the attached bit */
			lwz		r9,SACvrswap(r9)			/* Get the conversion from virtual to real */
			stw		r8,SAVflags(r4)				/* Set the flags */
			xor		r3,r4,r9					/* Switch savearea to physical addressing */
			b		EXT(exception_exit)			/* Go back to the caller... */


/*
 *			Here's where we check for a hit on the Blue Box Assist
 *			Most registers are non-volatile, so be careful here. If we don't 
 *			recognize the trap instruction we go back for regular processing.
 *			Otherwise we transfer to the assist code.
 */
 
checkassist:
			lwz		r23,savesrr1(r4)			/* Get the interrupted MSR */
			lwz		r24,ACT_MACT_BTS(r9)		/* Get the table start */
			rlwinm.	r23,r23,0,MSR_PR_BIT,MSR_PR_BIT	/* Are we in userland? */
			lwz		r27,savesrr0(r4)			/* Get trapped address */
			beqlr-								/* No assist in kernel mode... */

checkassistBP:									/* Safe place to breakpoint */

			sub		r24,r27,r24					/* See how far into it we are */
			rlwinm	r24,r24,PKTDSHIFT,0,27		/* Get displacement to control block */
			cmplwi	r24,BBMAXTRAP*PKTDSIZE		/* Do we fit in the list? */
			bgtlr-								/* Nope, it's a regular trap... */
			b		EXT(atomic_switch_trap)		/* Go to the assist... */

#if	MACH_KDB
/*
 *			Here's where we jump into the debugger.  This is called from
 *			either an MP signal from another processor, or a command-power NMI
 *			on the main processor.
 *
 *			Note that somewhere in our stack should be a return into the interrupt
 *			handler.  If there isn't, we'll crash off the end of the stack, actually,
 *			it'll just quietly return. hahahahaha.
 */

ENTRY(kdb_kintr, TAG_NO_FRAME_USED)
	
			lis		r9,HIGH_ADDR(EXT(ihandler_ret))	/* Top part of interrupt return */
			lis		r10,HIGH_ADDR(EXT(intercept_ret))	/* Top part of intercept return */
			ori		r9,r9,LOW_ADDR(EXT(ihandler_ret))	/* Bottom part of interrupt return */
			ori		r10,r10,LOW_ADDR(EXT(intercept_ret))	/* Bottom part of intercept return */
			
			lwz		r8,0(r1)				/* Get our caller's stack frame */

srchrets:	mr.		r8,r8					/* Have we reached the end of our rope? */
			beqlr-							/* Yeah, just bail... */
			lwz		r7,FM_LR_SAVE(r8)		/* The whoever called them */
			cmplw	cr0,r9,r7				/* Was it the interrupt handler? */
			beq		srchfnd					/* Yeah... */
			lwz		r8,0(r8)				/* Chain back to the previous frame */
			b		srchrets				/* Ok, check again... */
			
srchfnd:	stw		r10,FM_LR_SAVE(r8)		/* Modify return to come to us instead */
			blr								/* Finish up and get back here... */
			
/*
 *			We come here when we've returned all the way to the interrupt handler.
 *			That way we can enter the debugger with the registers and stack which
 *			existed at the point of interruption.
 *
 *			R3 points to the saved state at entry
 */
 
 ENTRY(intercept_ret, TAG_NO_FRAME_USED)

			lis		r6,HIGH_ADDR(EXT(kdb_trap))	/* Get the top part of the KDB enter routine */
			mr		r5,r3					/* Move saved state to the correct parameter */
			ori		r6,r6,LOW_ADDR(EXT(kdb_trap))	/* Get the last part of the KDB enter routine */
			li		r4,0					/* Set a code of 0 */
			mr		r13,r3					/* Save the saved state pointer in a non-volatile */
			mtlr	r6						/* Set the routine address */
			li		r3,-1					/* Show we had an interrupt type */
	
			blrl							/* Go enter KDB */

			mr		r3,r13					/* Put the saved state where expected */
			b		EXT(ihandler_ret)		/* Go return from the interruption... */

#endif			

#if VERIFYSAVE			
;
;			Savearea chain verification
;
		
versave:	

#if 0
;
;			Make sure that only the top FPU savearea is marked invalid
;

			lis		r28,hi16(EXT(default_pset))		; (TEST/DEBUG)
			lis		r27,hi16(EXT(DebugWork))		; (TEST/DEBUG)
			ori		r28,r28,lo16(EXT(default_pset))	; (TEST/DEBUG)
			ori		r27,r27,lo16(EXT(DebugWork))	; (TEST/DEBUG)
			li		r20,0							; (TEST/DEBUG)
			lwz		r26,0(r27)						; (TEST/DEBUG)
			lwz		r27,psthreadcnt(r28)			; (TEST/DEBUG)
			mr.		r26,r26							; (TEST/DEBUG)
			lwz		r28,psthreads(r28)				; (TEST/DEBUG)
			bnelr-									; (TEST/DEBUG)
			
fcknxtth:	mr.		r27,r27							; (TEST/DEBUG)
			beqlr-									; (TEST/DEBUG)
			
			lwz		r26,THREAD_TOP_ACT(r28)			; (TEST/DEBUG)

fckact:		mr.		r26,r26							; (TEST/DEBUG)
			bne+	fckact2							; (TEST/DEBUG)
			
			lwz		r28,THREAD_PSTHRN(r28)			; (TEST/DEBUG) Next in line
			subi	r27,r27,1						; (TEST/DEBUG)
			b		fcknxtth						; (TEST/DEBUG) 
	
fckact2:	lwz		r20,ACT_MACT_FPU(r26)			; (TEST/DEBUG) Get FPU chain
			mr.		r20,r20							; (TEST/DEBUG) Are there any?
			beq+	fcknact							; (TEST/DEBUG) No...
			
fckact3:	lwz		r20,SAVprefp(r20)				; (TEST/DEBUG) Get next in list
			mr.		r20,r20							; (TEST/DEBUG) Check next savearea
			beq+	fcknact							; (TEST/DEBUG) No...
			
			lwz		r29,SAVlvlfp(r20)				; (TEST/DEBUG) Get the level

			cmplwi	r29,1							; (TEST/DEBUG) Is it invalid??
			bne+	fckact3							; (TEST/DEBUG) Nope...
			
			lis		r27,hi16(EXT(DebugWork))		; (TEST/DEBUG)
			ori		r27,r27,lo16(EXT(DebugWork))	; (TEST/DEBUG)
			stw		r27,0(r27)						; (TEST/DEBUG)
			BREAKPOINT_TRAP							; (TEST/DEBUG)

fcknact:	lwz		r26,ACT_LOWER(r26)				; (TEST/DEBUG) Next activation
			b		fckact							; (TEST/DEBUG)
#endif

#if 1
;
;			Make sure there are no circular links in the float chain
;			And that FP is marked busy in it.
;			And the only the top is marked invalid.
;			And that the owning PCB is correct.
;

			lis		r28,hi16(EXT(default_pset))		; (TEST/DEBUG)
			lis		r27,hi16(EXT(DebugWork))		; (TEST/DEBUG)
			ori		r28,r28,lo16(EXT(default_pset))	; (TEST/DEBUG)
			ori		r27,r27,lo16(EXT(DebugWork))	; (TEST/DEBUG)
			li		r20,0							; (TEST/DEBUG)
			lwz		r26,0(r27)						; (TEST/DEBUG)
			lwz		r27,psthreadcnt(r28)			; (TEST/DEBUG)
			mr.		r26,r26							; (TEST/DEBUG)
			lwz		r28,psthreads(r28)				; (TEST/DEBUG)
			bnelr-									; (TEST/DEBUG)
			
fcknxtth:	mr.		r27,r27							; (TEST/DEBUG)
			beqlr-									; (TEST/DEBUG)
			
			lwz		r26,THREAD_TOP_ACT(r28)			; (TEST/DEBUG)

fckact:		mr.		r26,r26							; (TEST/DEBUG)
			bne+	fckact2							; (TEST/DEBUG)
			
			lwz		r28,THREAD_PSTHRN(r28)			; (TEST/DEBUG) Next in line
			subi	r27,r27,1						; (TEST/DEBUG)
			b		fcknxtth						; (TEST/DEBUG) 
	
fckact2:	lwz		r20,ACT_MACT_FPU(r26)			; (TEST/DEBUG) Get FPU chain
			li		r29,1							; (TEST/DEBUG)
			li		r22,0							; (TEST/DEBUG)

fckact3:	mr.		r20,r20							; (TEST/DEBUG) Are there any?
			beq+	fckact5							; (TEST/DEBUG) No...
			
			addi	r22,r22,1						; (TEST/DEBUG) Count chain depth
			
			lwz		r21,SAVflags(r20)				; (TEST/DEBUG) Get the flags
			rlwinm.	r21,r21,0,1,1					; (TEST/DEBUG) FP busy?
			bne+	fckact3a						; (TEST/DEBUG) Yeah...
			lis		r27,hi16(EXT(DebugWork))		; (TEST/DEBUG)
			ori		r27,r27,lo16(EXT(DebugWork))	; (TEST/DEBUG)
			stw		r27,0(r27)						; (TEST/DEBUG)
			BREAKPOINT_TRAP							; (TEST/DEBUG) Die
			
fckact3a:	cmplwi	r22,1							; (TEST/DEBUG) At first SA?
			beq+	fckact3b						; (TEST/DEBUG) Yeah, invalid is ok...
			lwz		r21,SAVlvlfp(r20)				; (TEST/DEBUG) Get level
			cmplwi	r21,1							; (TEST/DEBUG) Is it invalid?
			bne+	fckact3b						; (TEST/DEBUG) Nope, it is ok...
			lis		r27,hi16(EXT(DebugWork))		; (TEST/DEBUG)
			ori		r27,r27,lo16(EXT(DebugWork))	; (TEST/DEBUG)
			stw		r27,0(r27)						; (TEST/DEBUG)
			BREAKPOINT_TRAP							; (TEST/DEBUG) Die
			
fckact3b:	lwz		r21,SAVact(r20)					; (TEST/DEBUG) Get the owner
			cmplw	r21,r26							; (TEST/DEBUG) Correct activation?
			beq+	fckact3c						; (TEST/DEBUG) Yup...
			lis		r27,hi16(EXT(DebugWork))		; (TEST/DEBUG)
			ori		r27,r27,lo16(EXT(DebugWork))	; (TEST/DEBUG)
			stw		r27,0(r27)						; (TEST/DEBUG)
			BREAKPOINT_TRAP							; (TEST/DEBUG) Die

fckact3c:											; (TEST/DEBUG)
			lbz		r21,SAVflags+3(r20)				; (TEST/DEBUG) Pick up the test byte
			mr.		r21,r21							; (TEST/DEBUG) marked?
			beq+	fckact4							; (TEST/DEBUG) No, good...
			
			lis		r27,hi16(EXT(DebugWork))		; (TEST/DEBUG)
			ori		r27,r27,lo16(EXT(DebugWork))	; (TEST/DEBUG)
			stw		r27,0(r27)						; (TEST/DEBUG)
			BREAKPOINT_TRAP							; (TEST/DEBUG)
			
fckact4:	stb		r29,SAVflags+3(r20)				; (TEST/DEBUG) Set the test byte
			lwz		r20,SAVprefp(r20)				; (TEST/DEBUG) Next in list
			b		fckact3							; (TEST/DEBUG) Try it...

fckact5:	lwz		r20,ACT_MACT_FPU(r26)			; (TEST/DEBUG) Get FPU chain
			li		r29,0							; (TEST/DEBUG)

fckact6:	mr.		r20,r20							; (TEST/DEBUG) Are there any?
			beq+	fcknact							; (TEST/DEBUG) No...
			
			stb		r29,SAVflags+3(r20)				; (TEST/DEBUG) Clear the test byte
			lwz		r20,SAVprefp(r20)				; (TEST/DEBUG) Next in list
			b		fckact6							; (TEST/DEBUG) Try it...
			
fcknact:	lwz		r26,ACT_LOWER(r26)				; (TEST/DEBUG) Next activation
			b		fckact							; (TEST/DEBUG)
#endif


#if 0
;
;			Make sure in use count matches found savearea.  This is
;			not always accurate.  There is a variable "fuzz" factor in count.

			lis		r28,hi16(EXT(default_pset))		; (TEST/DEBUG)
			lis		r27,hi16(EXT(DebugWork))		; (TEST/DEBUG)
			ori		r28,r28,lo16(EXT(default_pset))	; (TEST/DEBUG)
			ori		r27,r27,lo16(EXT(DebugWork))	; (TEST/DEBUG)
			li		r20,0							; (TEST/DEBUG)
			lwz		r26,0(r27)						; (TEST/DEBUG)
			lwz		r27,psthreadcnt(r28)			; (TEST/DEBUG)
			mr.		r26,r26							; (TEST/DEBUG)
			lwz		r28,psthreads(r28)				; (TEST/DEBUG)
			bnelr-									; (TEST/DEBUG)
			
cknxtth:	mr.		r27,r27							; (TEST/DEBUG)
			beq-	cktotal							; (TEST/DEBUG)
			
			lwz		r26,THREAD_TOP_ACT(r28)			; (TEST/DEBUG)

ckact:		mr.		r26,r26							; (TEST/DEBUG)
			bne+	ckact2							; (TEST/DEBUG)
			
			lwz		r28,THREAD_PSTHRN(r28)			; (TEST/DEBUG) Next in line
			subi	r27,r27,1						; (TEST/DEBUG)
			b		cknxtth							; (TEST/DEBUG) 
			
ckact2:		lwz		r29,ACT_MACT_PCB(r26)			; (TEST/DEBUG)
			
cknorm:		mr.		r29,r29							; (TEST/DEBUG)
			beq-	cknormd							; (TEST/DEBUG)
			
			addi	r20,r20,1						; (TEST/DEBUG) Count normal savearea
			
			lwz		r29,SAVprev(r29)				; (TEST/DEBUG)
			b		cknorm							; (TEST/DEBUG)
			
cknormd:	lwz		r29,ACT_MACT_FPU(r26)			; (TEST/DEBUG)

ckfpu:		mr.		r29,r29							; (TEST/DEBUG)
			beq-	ckfpud							; (TEST/DEBUG)
			
			lwz		r21,SAVflags(r29)				; (TEST/DEBUG)
			rlwinm.	r21,r21,0,0,0					; (TEST/DEBUG) See if already counted
			bne-	cknfpu							; (TEST/DEBUG)
			
			addi	r20,r20,1						; (TEST/DEBUG) Count fpu savearea
			
cknfpu:		lwz		r29,SAVprefp(r29)				; (TEST/DEBUG)
			b		ckfpu							; (TEST/DEBUG)
			
ckfpud:		lwz		r29,ACT_MACT_VMX(r26)			; (TEST/DEBUG)

ckvmx:		mr.		r29,r29							; (TEST/DEBUG)
			beq-	ckvmxd							; (TEST/DEBUG)
			
			lwz		r21,SAVflags(r29)				; (TEST/DEBUG)
			rlwinm.	r21,r21,0,0,1					; (TEST/DEBUG) See if already counted
			bne-	cknvmx							; (TEST/DEBUG)
			
			addi	r20,r20,1						; (TEST/DEBUG) Count vector savearea
			
cknvmx:		lwz		r29,SAVprevec(r29)				; (TEST/DEBUG)
			b		ckvmx							; (TEST/DEBUG)
			
ckvmxd:		lwz		r26,ACT_LOWER(r26)				; (TEST/DEBUG) Next activation
			b		ckact							; (TEST/DEBUG)

cktotal:	lis		r28,hi16(EXT(saveanchor))		; (TEST/DEBUG)
			lis		r27,hi16(EXT(real_ncpus))		; (TEST/DEBUG)
			ori		r28,r28,lo16(EXT(saveanchor))	; (TEST/DEBUG)
			ori		r27,r27,lo16(EXT(real_ncpus))	; (TEST/DEBUG)

			lwz		r21,SVinuse(r28)				; (TEST/DEBUG)
			lwz		r27,0(r27)						; (TEST/DEBUG) Get the number of CPUs
			sub.	r29,r21,r20						; (TEST/DEBUG) Get number accounted for
			blt-	badsave							; (TEST/DEBUG) Have too many in use...
			sub		r26,r29,r27						; (TEST/DEBUG) Should be 1 unaccounted for for each processor
			cmpwi	r26,10							; (TEST/DEBUG) Allow a 10 area slop factor
			bltlr+									; (TEST/DEBUG)
			
badsave:	lis		r27,hi16(EXT(DebugWork))		; (TEST/DEBUG)
			ori		r27,r27,lo16(EXT(DebugWork))	; (TEST/DEBUG)
			stw		r27,0(r27)						; (TEST/DEBUG)
			BREAKPOINT_TRAP							; (TEST/DEBUG)
#endif
#endif	
