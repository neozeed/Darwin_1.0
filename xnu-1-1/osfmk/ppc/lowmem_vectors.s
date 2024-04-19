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
 * Low-memory exception vector code for PowerPC MACH
 *
 * These are the only routines that are ever run with
 * VM instruction translation switched off.
 *
 * The PowerPC is quite strange in that rather than having a set
 * of exception vectors, the exception handlers are installed
 * in well-known addresses in low memory. This code must be loaded
 * at ZERO in physical memory. The simplest way of doing this is
 * to load the kernel at zero, and specify this as the first file
 * on the linker command line.
 *
 * When this code is loaded into place, it is loaded at virtual
 * address KERNELBASE, which is mapped to zero (physical).
 *
 * This code handles all powerpc exceptions and is always entered
 * in supervisor mode with translation off. It saves the minimum
 * processor state before switching back on translation and
 * jumping to the approprate routine.
 *
 * Vectors from 0x100 to 0x3fff occupy 0x100 bytes each (64 instructions)
 *
 * We use some of this space to decide which stack to use, and where to
 * save the context etc, before	jumping to a generic handler.
 */

#include <assym.s>
#include <debug.h>
#include <cpus.h>
#include <db_machine_commands.h>
#include <mach_rt.h>
	
#include <mach_debug.h>
#include <ppc/asm.h>
#include <ppc/proc_reg.h>
#include <ppc/exception.h>
#include <ppc/Performance.h>
#include <mach/ppc/vm_param.h>
#include <ppc/POWERMAC/mp/MPPlugIn.h>

#define RUPTTRC 0
#define CHECKSAVE 0
#define PERFTIMES 0
#define ESPDEBUG 0

#define	VECTOR_SEGMENT	.section __VECTORS, __interrupts

			VECTOR_SEGMENT


			.globl EXT(ExceptionVectorsStart)

EXT(ExceptionVectorsStart):							/* Used if relocating the exception vectors */
baseR:												/* Used so we have more readable code */

/* 
 * System reset - call debugger
 */
			. = 0xf0
			.globl	EXT(ResetHandler)
EXT(ResetHandler):
			.long	0x0
			.long	0x0
			.long	0x0

			. = 0x100
.L_handler100:
			mtsprg	2,r13			/* Save R13 */
			mtsprg	3,r11			/* Save R11 */
			lwz		r13,lo16(EXT(ResetHandler)-EXT(ExceptionVectorsStart)+RESETHANDLER_TYPE)(br0)	; Get reset type
			mfcr	r11
			cmpi	cr0,r13,RESET_HANDLER_START
			bne		resetexc

			li		r11,RESET_HANDLER_NULL
			stw		r11,lo16(EXT(ResetHandler)-EXT(ExceptionVectorsStart)+RESETHANDLER_TYPE)(br0)	; Clear reset type

			lwz		r4,lo16(EXT(ResetHandler)-EXT(ExceptionVectorsStart)+RESETHANDLER_CALL)(br0)
			lwz		r3,lo16(EXT(ResetHandler)-EXT(ExceptionVectorsStart)+RESETHANDLER_ARG)(br0)
			mtlr	r4
			blr

resetexc:
			mtcr	r11
			mfsprg	r13,1							/* Get the exception save area */
			li		r11,T_RESET						/* Set 'rupt code */
			b		.L_exception_entry				/* Join common... */

/*
 *			This is the data used by the exception trace code
 */

			. = 0x180
.L_TraceData:

			.globl traceMask
traceMask:									/* Allowable trace types indexed by  vector >> 8 */
#if DEBUG
/*			.long	0x02000000 	*/			/* Only alignment exceptions enabled */
			.long	0xFFFFFFFF 				/* All enabled */
/*			.long	0xFBBFFFFF	*/			/* EXT and DEC disabled */
/*			.long	0xFFBFFFFF	*/			/* DEC disabled */
#else
			.long	0x00000000				; All disabled on non-debug systems
#endif

			.globl EXT(traceCurr)
EXT(traceCurr):	.long	traceTableBeg-EXT(ExceptionVectorsStart)	/* The next trace entry to use */

			.globl EXT(traceStart)
EXT(traceStart):	.long	traceTableBeg-EXT(ExceptionVectorsStart)	/* Start of the trace table */

			.globl EXT(traceEnd)
EXT(traceEnd):	
			.long	traceTableEnd-EXT(ExceptionVectorsStart)	/* End (wrap point) of the trace */

			.globl traceMsnd

traceMsnd:	.long	0										/* Saved mask while in debugger */

/*
 * 			Machine check (physical bus error) - call debugger
 */

			. = 0x200
.L_handler200:
			mtsprg	2,r13							/* Save R13 */
			mtsprg	3,r11							/* Save R11 */
			mfsprg	r13,1							/* Get the exception save area */
			li		r11,T_MACHINE_CHECK				/* Set 'rupt code */
			b		.L_exception_entry				/* Join common... */

/*
 * 			Data access - page fault, invalid memory rights for operation
 */

			. = 0x300
.L_handler300:
			mtsprg	2,r13							/* Save R13 */
			mtsprg	3,r11							/* Save R11 */
			mfsprg	r13,1							/* Get the exception save area */
			li		r11,T_DATA_ACCESS				/* Set 'rupt code */
			b		.L_exception_entry				/* Join common... */

/*
 * 			Instruction access - as for data access
 */

			. = 0x400
.L_handler400:
			mtsprg	2,r13							/* Save R13 */
			mtsprg	3,r11							/* Save R11 */
			mfsprg	r13,1							/* Get the exception save area */
			li		r11,T_INSTRUCTION_ACCESS		/* Set 'rupt code */
			b		.L_exception_entry				/* Join common... */

/*
 * 			External interrupt
 */

			. = 0x500
.L_handler500:
			mtsprg	2,r13							/* Save R13 */
			mtsprg	3,r11							/* Save R11 */
			mfsprg	r13,1							/* Get the exception save area */
			li		r11,T_INTERRUPT					/* Set 'rupt code */
			b		.L_exception_entry				/* Join common... */

/*
 * 			Alignment - many reasons
 */

			. = 0x600
.L_handler600:
			mtsprg	2,r13							/* Save R13 */
			mtsprg	3,r11							/* Save R11 */
			mfsprg	r13,1							/* Get the exception save area */
			li		r11,T_ALIGNMENT					/* Set 'rupt code */
			b		.L_exception_entry				/* Join common... */

/*
 * 			Program - floating point exception, illegal inst, priv inst, user trap
 */

			. = 0x700
.L_handler700:
			mtsprg	2,r13							/* Save R13 */
			mtsprg	3,r11							/* Save R11 */
			mfsprg	r13,1							/* Get the exception save area */
			li		r11,T_PROGRAM					/* Set 'rupt code */
			b		.L_exception_entry				/* Join common... */

/*
 * 			Floating point disabled
 */

			. = 0x800
.L_handler800:
			mtsprg	2,r13							/* Save R13 */
			mtsprg	3,r11							/* Save R11 */
			mfsprg	r13,1							/* Get the exception save area */
			li		r11,T_FP_UNAVAILABLE			/* Set 'rupt code */
			b		.L_exception_entry				/* Join common... */


/*
 * 			Decrementer - DEC register has passed zero.
 */

			. = 0x900
.L_handler900:
			mtsprg	2,r13							/* Save R13 */
			mtsprg	3,r11							/* Save R11 */
			mfsprg	r13,1							/* Get the exception save area */
			li		r11,T_DECREMENTER				/* Set 'rupt code */
			b		.L_exception_entry				/* Join common... */

/*
 * 			I/O controller interface error - MACH does not use this
 */

			. = 0xA00
.L_handlerA00:
			mtsprg	2,r13							/* Save R13 */
			mtsprg	3,r11							/* Save R11 */
			mfsprg	r13,1							/* Get the exception save area */
			li		r11,T_IO_ERROR					/* Set 'rupt code */
			b		.L_exception_entry				/* Join common... */

/*
 * 			Reserved
 */

			. = 0xB00
.L_handlerB00:
			mtsprg	2,r13							/* Save R13 */
			mtsprg	3,r11							/* Save R11 */
			mfsprg	r13,1							/* Get the exception save area */
			li		r11,T_RESERVED					/* Set 'rupt code */
			b		.L_exception_entry				/* Join common... */

/*
 * 			System call - generated by the sc instruction
 */

			. = 0xC00
.L_handlerC00:
			cmplwi	r0,0x7FF2						; Ultra fast path cthread info call?
			blt+	notufp							; Not ultra fast...
			cmplwi	r0,0x7FF3						; Ultra fast path fp/vec facility state?
			bgt+	notufp							; Not ultra fast...
			mfsprg	r3,0							; Get the per_proc_area
			beq-	isvecfp							; This is the facility stat call
			lwz		r3,UAW(r3)						; Get the assist word
			rfi										; All done, scream back... (no need to restore CR or R11, they are volatile)
;
isvecfp:	lwz		r3,spcFlags(r3)					; Get the facility status
			rfi										; Bail back...
;
			.align	5
notufp:		mtsprg	3,r11							/* Save R11 */
			mtsprg	2,r13							/* Save R13 */
			li		r11,T_SYSTEM_CALL				/* Set 'rupt code */
			mfsprg	r13,1							/* Get the exception save area */
			b		.L_exception_entry				/* Join common... */

/*
 * 			Trace - generated by single stepping
 */

			. = 0xD00
.L_handlerD00:
			mtsprg	2,r13							/* Save R13 */
			mtsprg	3,r11							/* Save R11 */
			mfsprg	r13,1							/* Get the exception save area */
			li		r11,T_TRACE						/* Set 'rupt code */
			b		.L_exception_entry				/* Join common... */

/*
 * 			Floating point assist
 */

			. = 0xe00
.L_handlerE00:
			mtsprg	2,r13							/* Save R13 */
			mtsprg	3,r11							/* Save R11 */
			mfsprg	r13,1							/* Get the exception save area */
			li		r11,T_FP_ASSIST					/* Set 'rupt code */
			b		.L_exception_entry				/* Join common... */


/*
 *			Performance monitor interruption
 */

 			. = 0xF00
PMIhandler:
			mtsprg	2,r13							/* Save R13 */
			mtsprg	3,r11							/* Save R11 */
			mfsprg	r13,1							/* Get the exception save area */
			li		r11,T_PERF_MON					/* Set 'rupt code */
			b		.L_exception_entry				/* Join common... */
	

/*
 *			VMX exception
 */

 			. = 0xF20
VMXhandler:
			mtsprg	2,r13							/* Save R13 */
			mtsprg	3,r11							/* Save R11 */
			mfsprg	r13,1							/* Get the exception save area */
			li		r11,T_VMX						/* Set 'rupt code */
			b		.L_exception_entry				/* Join common... */

	

/*
 * Instruction translation miss - we inline this code.
 * Upon entry (done for us by the machine):
 *     srr0 :	 addr of instruction that missed
 *     srr1 :	 bits 0-3   = saved CR0
 *                    4     = lru way bit
 *                    16-31 = saved msr
 *     msr[tgpr] = 1  (so gpr0-3 become our temporary variables)
 *     imiss:	 ea that missed
 *     icmp :	 the compare value for the va that missed
 *     hash1:	 pointer to first hash pteg
 *     hash2:	 pointer to 2nd hash pteg
 *
 * Register usage:
 *     tmp0:	 saved counter
 *     tmp1:	 junk
 *     tmp2:	 pointer to pteg
 *     tmp3:	 current compare value
 *
 * This code is taken from the 603e User's Manual with
 * some bugfixes and minor improvements to save bytes and cycles
 */

	. = 0x1000
.L_handler1000:
	mfspr	tmp2,	hash1
	mfctr	tmp0				/* use tmp0 to save ctr */
	mfspr	tmp3,	icmp

.L_imiss_find_pte_in_pteg:
	li	tmp1,	8			/* count */
	subi	tmp2,	tmp2,	8		/* offset for lwzu */
	mtctr	tmp1				/* count... */
	
.L_imiss_pteg_loop:
	lwz	tmp1,	8(tmp2)			/* check pte0 for match... */
	addi	tmp2,	tmp2,	8
	cmpw	cr0,	tmp1,	tmp3
#if 0	
	bdnzf+	cr0,	.L_imiss_pteg_loop
#else	
	bc	0,2,	.L_imiss_pteg_loop
#endif	
	beq+	cr0,	.L_imiss_found_pte

	/* Not found in PTEG, we must scan 2nd then give up */

	andi.	tmp1,	tmp3,	MASK(PTE0_HASH_ID)
	bne-	.L_imiss_do_no_hash_exception		/* give up */

	mfspr	tmp2,	hash2
	ori	tmp3,	tmp3,	MASK(PTE0_HASH_ID)
	b	.L_imiss_find_pte_in_pteg

.L_imiss_found_pte:

	lwz	tmp1,	4(tmp2)				/* get pte1_t */
	andi.	tmp3,	tmp1,	MASK(PTE1_WIMG_GUARD)	/* Fault? */
	bne-	.L_imiss_do_prot_exception		/* Guarded - illegal */

	/* Ok, we've found what we need to, restore and rfi! */

	mtctr	tmp0					/* restore ctr */
	mfsrr1	tmp3
	mfspr	tmp0,	imiss
	mtcrf	0x80,	tmp3				/* Restore CR0 */
	mtspr	rpa,	tmp1				/* set the pte */
	ori	tmp1,	tmp1,	MASK(PTE1_REFERENCED)	/* set referenced */
	tlbli	tmp0
	sth	tmp1,	6(tmp2)
	rfi
	
.L_imiss_do_prot_exception:
	/* set up srr1 to indicate protection exception... */
	mfsrr1	tmp3
	andi.	tmp2,	tmp3,	0xffff
	addis	tmp2,	tmp2,	MASK(SRR1_TRANS_PROT) >> 16
	b	.L_imiss_do_exception
	
.L_imiss_do_no_hash_exception:
	/* clean up registers for protection exception... */
	mfsrr1	tmp3
	andi.	tmp2,	tmp3,	0xffff
	addis	tmp2,	tmp2,	MASK(SRR1_TRANS_HASH) >> 16
	
	/* And the entry into the usual instruction fault handler ... */
.L_imiss_do_exception:

	mtctr	tmp0					/* Restore ctr */
	mtsrr1	tmp2					/* Set up srr1 */
	mfmsr	tmp0					
	xoris	tmp0,	tmp0,	MASK(MSR_TGPR)>>16	/* no TGPR */
	mtcrf	0x80,	tmp3				/* Restore CR0 */
	mtmsr	tmp0					/* reset MSR[TGPR] */
	b	.L_handler400				/* Instr Access */
	
/*
 * Data load translation miss
 *
 * Upon entry (done for us by the machine):
 *     srr0 :	 addr of instruction that missed
 *     srr1 :	 bits 0-3   = saved CR0
 *                    4     = lru way bit
 *                    5     = 1 if store
 *                    16-31 = saved msr
 *     msr[tgpr] = 1  (so gpr0-3 become our temporary variables)
 *     dmiss:	 ea that missed
 *     dcmp :	 the compare value for the va that missed
 *     hash1:	 pointer to first hash pteg
 *     hash2:	 pointer to 2nd hash pteg
 *
 * Register usage:
 *     tmp0:	 saved counter
 *     tmp1:	 junk
 *     tmp2:	 pointer to pteg
 *     tmp3:	 current compare value
 *
 * This code is taken from the 603e User's Manual with
 * some bugfixes and minor improvements to save bytes and cycles
 */

	. = 0x1100
.L_handler1100:
	mfspr	tmp2,	hash1
	mfctr	tmp0				/* use tmp0 to save ctr */
	mfspr	tmp3,	dcmp

.L_dlmiss_find_pte_in_pteg:
	li	tmp1,	8			/* count */
	subi	tmp2,	tmp2,	8		/* offset for lwzu */
	mtctr	tmp1				/* count... */
	
.L_dlmiss_pteg_loop:
	lwz	tmp1,	8(tmp2)			/* check pte0 for match... */
	addi	tmp2,	tmp2,	8
	cmpw	cr0,	tmp1,	tmp3
#if 0 /* How to write this correctly? */	
	bdnzf+	cr0,	.L_dlmiss_pteg_loop
#else	
	bc	0,2,	.L_dlmiss_pteg_loop
#endif	
	beq+	cr0,	.L_dmiss_found_pte

	/* Not found in PTEG, we must scan 2nd then give up */

	andi.	tmp1,	tmp3,	MASK(PTE0_HASH_ID)	/* already at 2nd? */
	bne-	.L_dmiss_do_no_hash_exception		/* give up */

	mfspr	tmp2,	hash2
	ori	tmp3,	tmp3,	MASK(PTE0_HASH_ID)
	b	.L_dlmiss_find_pte_in_pteg

.L_dmiss_found_pte:

	lwz	tmp1,	4(tmp2)				/* get pte1_t */

	/* Ok, we've found what we need to, restore and rfi! */

	mtctr	tmp0					/* restore ctr */
	mfsrr1	tmp3
	mfspr	tmp0,	dmiss
	mtcrf	0x80,	tmp3				/* Restore CR0 */
	mtspr	rpa,	tmp1				/* set the pte */
	ori	tmp1,	tmp1,	MASK(PTE1_REFERENCED)	/* set referenced */
	tlbld	tmp0					/* load up tlb */
	sth	tmp1,	6(tmp2)				/* sth is faster? */
	rfi
	
	/* This code is shared with data store translation miss */
	
.L_dmiss_do_no_hash_exception:
	/* clean up registers for protection exception... */
	mfsrr1	tmp3
	/* prepare to set DSISR_WRITE_BIT correctly from srr1 info */
	rlwinm	tmp1,	tmp3,	9,	6,	6
	addis	tmp1,	tmp1,	MASK(DSISR_HASH) >> 16

	/* And the entry into the usual data fault handler ... */

	mtctr	tmp0					/* Restore ctr */
	andi.	tmp2,	tmp3,	0xffff			/* Clean up srr1 */
	mtsrr1	tmp2					/* Set srr1 */
	mtdsisr	tmp1
	mfspr	tmp2,	dmiss
	mtdar	tmp2
	mfmsr	tmp0
	xoris	tmp0,	tmp0,	MASK(MSR_TGPR)>>16	/* no TGPR */
	mtcrf	0x80,	tmp3				/* Restore CR0 */
	sync						/* Needed on some */
	mtmsr	tmp0					/* reset MSR[TGPR] */
	b	.L_handler300				/* Data Access */
	
/*
 * Data store translation miss (similar to data load)
 *
 * Upon entry (done for us by the machine):
 *     srr0 :	 addr of instruction that missed
 *     srr1 :	 bits 0-3   = saved CR0
 *                    4     = lru way bit
 *                    5     = 1 if store
 *                    16-31 = saved msr
 *     msr[tgpr] = 1  (so gpr0-3 become our temporary variables)
 *     dmiss:	 ea that missed
 *     dcmp :	 the compare value for the va that missed
 *     hash1:	 pointer to first hash pteg
 *     hash2:	 pointer to 2nd hash pteg
 *
 * Register usage:
 *     tmp0:	 saved counter
 *     tmp1:	 junk
 *     tmp2:	 pointer to pteg
 *     tmp3:	 current compare value
 *
 * This code is taken from the 603e User's Manual with
 * some bugfixes and minor improvements to save bytes and cycles
 */

	. = 0x1200
.L_handler1200:
	mfspr	tmp2,	hash1
	mfctr	tmp0				/* use tmp0 to save ctr */
	mfspr	tmp3,	dcmp

.L_dsmiss_find_pte_in_pteg:
	li	tmp1,	8			/* count */
	subi	tmp2,	tmp2,	8		/* offset for lwzu */
	mtctr	tmp1				/* count... */
	
.L_dsmiss_pteg_loop:
	lwz	tmp1,	8(tmp2)			/* check pte0 for match... */
	addi	tmp2,	tmp2,	8

		cmpw	cr0,	tmp1,	tmp3
#if 0 /* I don't know how to write this properly */	
	bdnzf+	cr0,	.L_dsmiss_pteg_loop
#else	
	bc	0,2,	.L_dsmiss_pteg_loop
#endif	
	beq+	cr0,	.L_dsmiss_found_pte

	/* Not found in PTEG, we must scan 2nd then give up */

	andi.	tmp1,	tmp3,	MASK(PTE0_HASH_ID)	/* already at 2nd? */
	bne-	.L_dmiss_do_no_hash_exception		/* give up */

	mfspr	tmp2,	hash2
	ori	tmp3,	tmp3,	MASK(PTE0_HASH_ID)
	b	.L_dsmiss_find_pte_in_pteg

.L_dsmiss_found_pte:

	lwz	tmp1,	4(tmp2)				/* get pte1_t */
	andi.	tmp3,	tmp1,	MASK(PTE1_CHANGED)	/* unchanged, check? */
	beq-	.L_dsmiss_check_prot			/* yes, check prot */

.L_dsmiss_resolved:
	/* Ok, we've found what we need to, restore and rfi! */

	mtctr	tmp0					/* restore ctr */
	mfsrr1	tmp3
	mfspr	tmp0,	dmiss
	mtcrf	0x80,	tmp3				/* Restore CR0 */
	mtspr	rpa,	tmp1				/* set the pte */
	tlbld	tmp0					/* load up tlb */
	rfi
	
.L_dsmiss_check_prot:
	/* PTE is unchanged, we must check that we can write */
	rlwinm.	tmp3,	tmp1,	30,	0,	1	/* check PP[1] */
	bge-	.L_dsmiss_check_prot_user_kern
	andi.	tmp3,	tmp1,	1			/* check PP[0] */
	beq+	.L_dsmiss_check_prot_ok
	
.L_dmiss_do_prot_exception:
	/* clean up registers for protection exception... */
	mfsrr1	tmp3
	/* prepare to set DSISR_WRITE_BIT correctly from srr1 info */
	rlwinm	tmp1,	tmp3,	9,	6,	6
	addis	tmp1,	tmp1,	MASK(DSISR_PROT) >> 16

	/* And the entry into the usual data fault handler ... */

	mtctr	tmp0					/* Restore ctr */
	andi.	tmp2,	tmp3,	0xffff			/* Clean up srr1 */
	mtsrr1	tmp2					/* Set srr1 */
	mtdsisr	tmp1
	mfspr	tmp2,	dmiss
	mtdar	tmp2
	mfmsr	tmp0
	xoris	tmp0,	tmp0,	MASK(MSR_TGPR)>>16	/* no TGPR */
	mtcrf	0x80,	tmp3				/* Restore CR0 */
	sync						/* Needed on some */
	mtmsr	tmp0					/* reset MSR[TGPR] */
	b	.L_handler300				/* Data Access */
	
/* NB - if we knew we were on a 603e we could test just the MSR_KEY bit */
.L_dsmiss_check_prot_user_kern:
	mfsrr1	tmp3
	andi.	tmp3,	tmp3,	MASK(MSR_PR)
	beq+	.L_dsmiss_check_prot_kern
	mfspr	tmp3,	dmiss				/* check user privs */
	mfsrin	tmp3,	tmp3				/* get excepting SR */
	andis.	tmp3,	tmp3,	0x2000			/* Test SR ku bit */
	beq+	.L_dsmiss_check_prot_ok
	b	.L_dmiss_do_prot_exception

.L_dsmiss_check_prot_kern:
	mfspr	tmp3,	dmiss				/* check kern privs */
	mfsrin	tmp3,	tmp3
	andis.	tmp3,	tmp3,	0x4000			/* Test SR Ks bit */
	bne-	.L_dmiss_do_prot_exception

.L_dsmiss_check_prot_ok:
	/* Ok, mark as referenced and changed before resolving the fault */
	ori	tmp1,	tmp1,	(MASK(PTE1_REFERENCED)|MASK(PTE1_CHANGED))
	sth	tmp1,	6(tmp2)
	b	.L_dsmiss_resolved
	
/*
 * 			Instruction address breakpoint
 */

			. = 0x1300
.L_handler1300:
			mtsprg	2,r13							/* Save R13 */
			mtsprg	3,r11							/* Save R11 */
			mfsprg	r13,1							/* Get the exception save area */
			li		r11,T_INSTRUCTION_BKPT			/* Set 'rupt code */
			b		.L_exception_entry				/* Join common... */

/*
 * 			System management interrupt
 */

			. = 0x1400
.L_handler1400:
			mtsprg	2,r13							/* Save R13 */
			mtsprg	3,r11							/* Save R11 */
			mfsprg	r13,1							/* Get the exception save area */
			li		r11,T_SYSTEM_MANAGEMENT			/* Set 'rupt code */
			b		.L_exception_entry				/* Join common... */

;
; 			Altivec Java Mode Assist interrupt
;

			. = 0x1600
.L_handler1600:
			mtsprg	2,r13							/* Save R13 */
			mtsprg	3,r11							/* Save R11 */
			mfsprg	r13,1							/* Get the exception save area */
			li		r11,T_ALTIVEC_ASSIST			/* Set 'rupt code */
			b		.L_exception_entry				/* Join common... */

/*
 * There is now a large gap of reserved traps
 */

/*
 * 			Run mode/ trace exception - single stepping on 601 processors
 */

			. = 0x2000
.L_handler2000:
			mtsprg	2,r13							/* Save R13 */
			mtsprg	3,r11							/* Save R11 */
			mfsprg	r13,1							/* Get the exception save area */
			li		r11,T_RUNMODE_TRACE				/* Set 'rupt code */
			b		.L_exception_entry				/* Join common... */

/*
 * .L_exception_entry(type)
 *
 * This is the common exception handling routine called by any
 * type of system exception.
 *
 * ENTRY:	via a system exception handler, thus interrupts off, VM off.
 *              r3 has been saved in sprg3 and now contains a number
 *              representing the exception's origins
 *
 */
	
			.data
			.align	ALIGN
#ifdef	__ELF__
			.type	EXT(exception_entry),@object
			.size	EXT(exception_entry), 4
#endif
			.globl	EXT(exception_entry)
EXT(exception_entry):
			.long	.L_exception_entry-EXT(ExceptionVectorsStart) /* phys addr of fn */
				
			VECTOR_SEGMENT
			.align	2

.L_exception_entry:
			
/*
 *
 *	Here we will save off a mess of registers, the special ones and R0-R12.  We use the DCBZ
 *	instruction to clear and allcoate a line in the cache.  This way we won't take any cache
 *	misses, so these stores won't take all that long.
 *
 *
 */

			dcbz	0,r13							/* Allocate the first line of the savearea in the cache */			
			
			stw		r1,saver1(r13)					/* Save this one */
			li		r1,32							/* Point to the second line */
			stw		r0,saver0(r13)					/* Save this one */
			dcbz	r1,r13							/* Reserve our line in cache */
			
#if PERFTIMES && DEBUG
			mftb	r1								; Get the time of interruption
			stw		r1,0x17C(br0)					; Save the time of interruption
#endif
			
			stw		r2,saver2(r13)					/* Save this one */
			stw		r3,saver3(r13)					/* Save this one */
			stw		r6,saver6(r13)					/* Save this one */
			stw		r4,saver4(r13)					/* Save this one */

			stw		r8,saver8(r13)					/* Save this one */
			mfsrr0	r6								/* Get the interruption SRR0 */
			stw		r7,saver7(r13)					/* Save this one */
			mfsrr1	r7								/* Get the interrupt SRR1 */
			stw		r6,savesrr0(r13)				/* Save the SRR0 */
			stw		r5,saver5(r13)					/* Save this one */
			mfsprg	r6,2							/* Get interrupt time R13 */
			stw		r7,savesrr1(r13)				/* Save SRR1 */
			mfsprg	r8,3							/* Get 'rupt time R11 */
			stw		r6,saver13(r13)					/* Save 'rupt R1 */
			la		r6,saver14(r13)					/* Point to the next cache line */
			stw		r8,saver11(r13)					/* Save 'rupt time R11 */

			dcbz	0,r6							/* Allocate in cache */
			
			stw		r9,saver9(r13)					/* Save this one */
			mfcr	r7								/* Get the CR */
#if 0
			cmplwi	r13,0x1000						; (TEST/DEBUG)
			bgt+	notpage0yy						; (TEST/DEBUG)
			li		r9,1							; (TEST/DEBUG)
			lwz		r9,0(br0)						; (TEST/DEBUG)
			dcbst	br0,r9							; (TEST/DEBUG)
notpage0aa:	b		notpage0aa						; (TEST/DEBUG)

notpage0yy:	lwz		r9,0(br0)						; (TEST/DEBUG)
			mr.		r9,r9							; (TEST/DEBUG)
notpage0bb:	bne-	notpage0bb						; (TEST/DEBUG)
#endif

			la		r9,saver30(r13)					/* Point to the trailing end */
			stw		r10,saver10(r13)				/* Save this one */
			mflr	r8								/* Get the LR */
			stw		r12,saver12(r13)				/* Save this one */
			
			dcbz	0,r9							/* Allocate the last in the area */
			
			stw		r14,saver14(r13)				/* Save this one */
			la		r10,saver22(r13)				/* Point to the next block to save into */
			stw		r15,saver15(r13)				/* Save this one */
			stw		r7,savecr(r13)					/* Save 'rupt CR */
			mfctr	r6								/* Get the CTR */
			stw		r16,saver16(r13)				/* Save this one */
			stw		r8,savelr(r13)					/* Save 'rupt LR */
		
			dcbz	0,r10							/* Allocate next save area line */
			
			stw		r17,saver17(r13)				/* Save this one */
			stw		r18,saver18(r13)				/* Save this one */
			stw		r6,savectr(r13)					/* Save 'rupt CTR */
			mfxer	r7								/* Get the XER */
			stw		r19,saver19(r13)				/* Save this one */
			lis		r12,HIGH_ADDR(KERNEL_SEG_REG0_VALUE)	/* Get the high half of the kernel SR0 value */
			mfdar	r6								/* Get the 'rupt DAR */
			stw		r20,saver20(r13)				/* Save this one */
#if 0
			mfsr	r10,sr0							; (TEST/DEBUG)
			mfsr	r20,sr1							; (TEST/DEBUG)
			stw		r10,savesr0(r13)				; (TEST/DEBUG)
			mfsr	r10,sr2							; (TEST/DEBUG)
			mfsr	r19,sr3							; (TEST/DEBUG)
			stw		r20,savesr1(r13)				; (TEST/DEBUG)
			mfsr	r14,sr4							; (TEST/DEBUG)
			mfsr	r15,sr5							; (TEST/DEBUG)
			stw		r10,savesr2(r13)				; (TEST/DEBUG)
			mfsr	r16,sr6							; (TEST/DEBUG)
			mfsr	r17,sr7							; (TEST/DEBUG)
			stw		r19,savesr3(r13)				; (TEST/DEBUG)			
			mfsr	r10,sr8							; (TEST/DEBUG)
			mfsr	r20,sr9							; (TEST/DEBUG)
			stw		r14,savesr4(r13)				; (TEST/DEBUG)
			mfsr	r14,sr10						; (TEST/DEBUG)
			mfsr	r19,sr11						; (TEST/DEBUG)
			stw		r15,savesr5(r13)				; (TEST/DEBUG)
			mfsr	r15,sr13						; (TEST/DEBUG)
			stw		r16,savesr6(r13)				; (TEST/DEBUG)
			mfsr	r16,sr14						; (TEST/DEBUG)
			stw		r17,savesr7(r13)				; (TEST/DEBUG)
			mfsr	r17,sr15						; (TEST/DEBUG)
			
			stw		r10,savesr8(r13)				; (TEST/DEBUG)
			stw		r20,savesr9(r13)				; (TEST/DEBUG)
			stw		r14,savesr10(r13)				; (TEST/DEBUG)
			stw		r19,savesr11(r13)				; (TEST/DEBUG)
			stw		r15,savesr13(r13)				; (TEST/DEBUG)
			stw		r16,savesr14(r13)				; (TEST/DEBUG)
			stw		r17,savesr15(r13)				; (TEST/DEBUG)
#endif

			mtsr	sr0,r12							/* Set the kernel SR0 */
			stw		r21,saver21(r13)				/* Save this one */
			addis	r12,r12,0x0010					; Point to the second segment of kernel
			stw		r7,savexer(r13)					/* Save the 'rupt XER */
			mtsr	sr1,r12							/* Set the kernel SR1 */
			stw		r30,saver30(r13)				/* Save this one */
			addis	r12,r12,0x0010					; Point to the third segment of kernel
			stw		r31,saver31(r13)				/* Save this one */
			mtsr	sr2,r12							/* Set the kernel SR2 */
			stw		r22,saver22(r13)				/* Save this one */
			addis	r12,r12,0x0010					; Point to the third segment of kernel
			la		r10,savedar(r13)				/* Point to exception info block */
			stw		r23,saver23(r13)				/* Save this one */
			mtsr	sr3,r12							/* Set the kernel SR3 */
			stw		r24,saver24(r13)				/* Save this one */
			stw		r25,saver25(r13)				/* Save this one */
			mfdsisr	r7								/* Get the 'rupt DSISR */
			stw		r26,saver26(r13)				/* Save this one */
			
			dcbz	0,r10							/* Allocate exception info line */
		
			stw		r27,saver27(r13)				/* Save this one */
			stw		r28,saver28(r13)				/* Save this one */
			stw		r29,saver29(r13)				/* Save this one */
			mfsr	r14,sr14						; Get the copyin/out segment register
			stw		r6,savedar(r13)					/* Save the 'rupt DAR */
			stw		r7,savedsisr(r13)				/* Save the 'rupt code DSISR */
			stw		r11,saveexception(r13)			/* Save the exception code */
			stw		r14,savesr14(r13)				; Save copyin/copyout

			lis		r8,HIGH_ADDR(EXT(saveanchor))	/* Get the high part of the anchor */
			mfpvr	r20								/* Get the PVR */
			ori		r8,r8,LOW_ADDR(EXT(saveanchor))	/* Bottom half of the anchor */
			li		r21,0x3612						/* Mask of processors with a PIR */
			rlwinm	r20,r20,16,16,31				/* Isolate the CPU type */
			srw		r21,r21,r20						/* Isolate the PIR valid bit */
			cmplwi	cr1,r20,PROCESSOR_VERSION_Max	; Do we have Altivec?
			rlwinm	r21,r21,0,31,31					/* Isolate the PIR valid */
			
			li		r19,0							; Assume no Altivec
			blt		cr1,noavec						; No possible AltiVec here...
			mfspr	r19,vrsave						; Get the VRSAVE register

noavec:		cmplwi	cr6,r21,1						/* Check if PIR is valid */
			stw		r19,savevrsave(r13)				; Save the vector register usage flags

#if 0
			lwz		r14,savesrr0(r13)				/* (TEST/DEBUG) */
			cmplw	cr7,r5,r5						/* (TEST/DEBUG) */
			rlwinm.	r14,r14,0,0,15					/* (TEST/DEBUG) */
			cmplwi	cr2,r11,T_INTERRUPT				/* (TEST/DEBUG) */
			beq-	noeattrace2						/* (TEST/DEBUG) */
			cmplwi	r11,T_SYSTEM_CALL				/* (TEST/DEBUG) */
			rlwinm	r14,r0,1,0,31					/* (TEST/DEBUG) */
			beq		cr2,eatatjoes					/* (TEST/DEBUG) */
			bne+	noeattrace2						/* (TEST/DEBUG) */
			cmplwi	r14,0x001B						/* (TEST/DEBUG) */
			bne+	noeattrace2						/* (TEST/DEBUG) */

eatatjoes:	cmplwi	cr7,r13,0						/* (TEST/DEBUG) */

noeattrace2:
#else
			cmplw	cr7,r13,r13						/* (TEST/DEBUG) */
#endif
#if RUPTTRC
			lwz		r9,0(br0)						/* (TEST/DEBUG) */
			lis		r14,0x7FFF						/* (TEST/DEBUG) */
			mr.		r9,r9							/* (TEST/DEBUG) */
			beq+	nono1							/* (TEST/DEBUG) */
			bne		cr7,nono1						/* (TEST/DEBUG) */
			
			mfdec	r9								/* (TEST/DEBUG) */
			or		r14,r14,r9						/* (TEST/DEBUG) */
			mtdec	r14								/* (TEST/DEBUG) */
			li		r14,0x20						/* (TEST/DEBUG) */
		
			lwarx	r15,0,r14						; ?

mpwait0:	lwarx	r15,0,r14						/* (TEST/DEBUG) */
			mr.		r15,r15							/* (TEST/DEBUG) */
			bne-	mpwait0							/* (TEST/DEBUG) */
			stwcx.	r14,0,r14						/* (TEST/DEBUG) */
			bne-	mpwait0							/* (TEST/DEBUG) */

			stw		r0,0x280(br0)					/* (TEST/DEBUG) */
			stmw	r1,0x284(br0)					/* (TEST/DEBUG) */
			bne		cr6,nopir0						/* (TEST/DEBUG) */
			mfspr	r4,pir							/* (TEST/DEBUG) */
			b		gotpir0							/* (TEST/DEBUG) */
nopir0:		li		r4,0							/* (TEST/DEBUG) */
gotpir0:											/* (TEST/DEBUG) */
			lis		r31,HIGH_ADDR(EXT(dbgDispLL))	/* (TEST/DEBUG) */
			rlwinm	r4,r4,24,4,7					/* (TEST/DEBUG) */
			li		r3,0							/* (TEST/DEBUG) */
			rlwimi	r4,r11,30,8,31					/* (TEST/DEBUG) */
			mfsrr0	r5								/* (TEST/DEBUG) */
			oris	r4,r4,0x3030					/* (TEST/DEBUG) */
			ori		r31,r31,LOW_ADDR(EXT(dbgDispLL))	/* (TEST/DEBUG) */
			ori		r4,r4,0x3030					/* (TEST/DEBUG) */
			mtlr	r31								/* (TEST/DEBUG) */
			blrl									/* (TEST/DEBUG) */
			
			bne		cr6,nopir1						/* (TEST/DEBUG) */
			mfspr	r4,pir							/* (TEST/DEBUG) */
			b		gotpir1							/* (TEST/DEBUG) */
nopir1:		li		r4,0							/* (TEST/DEBUG) */
gotpir1:											/* (TEST/DEBUG) */
			lis		r31,HIGH_ADDR(EXT(dbgDispLL))	/* (TEST/DEBUG) */
			rlwinm	r4,r4,24,4,7					/* (TEST/DEBUG) */
			lwz		r5,savelr(r13)					/* (TEST/DEBUG) */
			li		r3,0							/* (TEST/DEBUG) */
			oris	r4,r4,0x3030					/* (TEST/DEBUG) */
			ori		r31,r31,LOW_ADDR(EXT(dbgDispLL))	/* (TEST/DEBUG) */
			ori		r4,r4,0x3130					/* (TEST/DEBUG) */
			mtlr	r31								/* (TEST/DEBUG) */
			blrl									/* (TEST/DEBUG) */
		
			bne		cr6,nopir2						/* (TEST/DEBUG) */
			mfspr	r4,pir							/* (TEST/DEBUG) */
			b		gotpir2							/* (TEST/DEBUG) */
nopir2:		li		r4,0							/* (TEST/DEBUG) */
gotpir2:											/* (TEST/DEBUG) */
			lis		r31,HIGH_ADDR(EXT(dbgDispLL))	/* (TEST/DEBUG) */
			rlwinm	r4,r4,24,4,7					/* (TEST/DEBUG) */
			li		r3,0							/* (TEST/DEBUG) */
			mr		r5,r13							/* (TEST/DEBUG) */
			oris	r4,r4,0x3030					/* (TEST/DEBUG) */
			ori		r31,r31,LOW_ADDR(EXT(dbgDispLL))	/* (TEST/DEBUG) */
			ori		r4,r4,0x3131					/* (TEST/DEBUG) */
			mtlr	r31								/* (TEST/DEBUG) */
			blrl									/* (TEST/DEBUG) */
			lmw		r1,0x284(br0)					/* (TEST/DEBUG) */
			lwz		r0,0x280(br0)					/* (TEST/DEBUG) */
			
			li		r15,0							/* (TEST/DEBUG) */
			stw		r15,0x20(br0)					/* (TEST/DEBUG) */
			mtdec	r9								/* (TEST/DEBUG) */
nono1:
#endif


/* 
 *			Everything is saved at this point, except for FPRs, and VMX registers
 *
 *			Time for a new save area.  Allocate the trace table entry now also
 *			Note that we haven't touched R0-R5 yet.  Except for R1, that's in the save
 */

#if 0
			mfsrr1	r20								; (TEST/DEBUG)
			mfsrr0	r7								; (TEST/DEBUG)
			lis		r9,0x8000						; (TEST/DEBUG) 
			mfmsr	r25								; (TEST/DEBUG)
			ori		r15,r25,0x10					; (TEST/DEBUG)
			mtmsr	r15								; (TEST/DEBUG)
			isync									; (TEST/DEBUG)
			ori		r9,r9,lo16(0x870C)				; (TEST/DEBUG) 
			stw		r11,0(r9)						; (TEST/DEBUG)
			stw		r7,0(r9)						; (TEST/DEBUG)
			mtmsr	r25								; (TEST/DEBUG)
			isync									; (TEST/DEBUG)
			mtsrr1	r20								; (TEST/DEBUG)
			mtsrr0	r7								; (TEST/DEBUG)
#endif


			lwarx	r9,0,r8							; ?

lllck:		lwarx	r9,0,r8							/* Grab the lock value */
			li		r7,1							/* Use part of the delay time */
			mr.		r9,r9							/* Is it locked? */
			bne-	lllcks							/* Yeah, wait for it to clear... */
			stwcx.	r7,0,r8							/* Try to seize that there durn lock */
			beq+	lllckd							/* Got it... */
			b		lllck							/* Collision, try again... */
			
lllcks:		lwz		r9,SVlock(r8)					/* Get that lock in here */
			mr.		r9,r9							/* Is it free yet? */
			beq+	lllck							/* Yeah, try for it again... */
			b		lllcks							/* Sniff away... */
			
lllckd:		isync									/* Purge any speculative executions here */
			rlwinm	r7,r11,30,0,31					/* Save 'rupt code shifted right 2 */
#if 1
			lwz		r14,LOW_ADDR(traceMask-EXT(ExceptionVectorsStart))(br0)	/* Get the trace mask */
#else
			li		r14,-1							/* (TEST/DEBUG) */
#endif
			addi	r7,r7,10						/* Adjust for CR5_EQ position */	
			lwz		r15,SVfree(r8)					/* Get the head of the save area list */			
			lwz		r25,SVinuse(r8)					/* Get the in use count */			
			rlwnm	r7,r14,r7,22,22					/* Set CR5_EQ bit position to 0 if tracing allowed */
			lwz		r20,LOW_ADDR(EXT(traceCurr)-EXT(ExceptionVectorsStart))(br0)	/* Pick up the current trace entry */
			mtcrf	0x04,r7							/* Set CR5 to show trace or not */

			lwz		r14,SACalloc(r15)				/* Pick up the allocation bits */
			addi	r25,r25,1						/* Bump up the in use count for the new savearea */
			lwz		r21,LOW_ADDR(EXT(traceEnd)-EXT(ExceptionVectorsStart))(br0)	/* Grab up the end of it all */
			mr.		r14,r14							/* Can we use the first one? */
			blt		use1st							/* Yeah... */
			
			andis.	r14,r14,0x8000					/* Show we used the second and remember if it was the last */
			addi	r10,r15,0x0800					/* Point to the first one */
			b		gotsave							/* We have the area now... */

use1st:		andis.	r14,r14,0x4000					/* Mark first gone and remember if empty */
			mr		r10,r15							/* Set the save area */
			
gotsave:	stw		r14,SACalloc(r15)				/* Put back the allocation bits */
			bne		nodqsave						/* There's still an empty slot, don't dequeue... */

			lwz		r16,SACnext(r15)				/* Get the next in line */
			stw		r16,SVfree(r8)					/* Dequeue our now empty save area block */

nodqsave:	addi	r22,r20,LTR_size				/* Point to the next trace entry */
			stw		r25,SVinuse(r8)					/* Set the in use count */			
			li		r17,0							/* Clear this for the lock */
			cmplw	r22,r21							/* Do we need to wrap the trace table? */
			stw		r17,SAVprev(r10)				/* Clear back pointer for the newly allocated guy */
			mtsprg	1,r10							/* Get set for the next 'rupt */
			bne+	gotTrcEnt						/* We got a trace entry... */
			
			lwz		r22,LOW_ADDR(EXT(traceStart)-EXT(ExceptionVectorsStart))(br0)	/* Wrap back to the top */

gotTrcEnt:	bne-	cr5,skipTrace1					/* Don't want to trace this kind... */
	
			stw		r22,LOW_ADDR(EXT(traceCurr)-EXT(ExceptionVectorsStart))(br0)	/* Set the next entry for the next guy */
			
#if ESPDEBUG
			li		r22,0x180						; (TEST/DEBUG)
			dcbst	br0,r22							; (TEST/DEBUG)
			sync									; (TEST/DEBUG)
#endif
			
			dcbz	0,r20							/* Allocate cache for the entry */
			
skipTrace1:	sync									/* Make sure all stores are done */
			stw		r17,SVlock(r8)					/* Unlock both save and trace areas */

#if RUPTTRC
			lis		r14,0x7FFF						/* (TEST/DEBUG) */
			mfdec	r17								/* (TEST/DEBUG) */
			or		r14,r14,r17						/* (TEST/DEBUG) */
			mtdec	r14								/* (TEST/DEBUG) */
			li		r14,0x20						/* (TEST/DEBUG) */
		
			lwarx	r16,0,r14						; ?

mpwait1:	lwarx	r16,0,r14						/* (TEST/DEBUG) */
			mr.		r16,r16							/* (TEST/DEBUG) */
			bne-	mpwait1							/* (TEST/DEBUG) */
			stwcx.	r14,0,r14						/* (TEST/DEBUG) */
			bne-	mpwait1							/* (TEST/DEBUG) */

			stw		r0,0x280(br0)					/* (TEST/DEBUG) */
			stmw	r1,0x284(br0)					/* (TEST/DEBUG) */
			bne		cr7,nono2						/* (TEST/DEBUG) */
			lwz		r0,0(br0)						/* (TEST/DEBUG) */
			mr.		r0,r0							/* (TEST/DEBUG) */
			beq+	nono2							/* (TEST/DEBUG) */
			bne		cr6,nopir3						/* (TEST/DEBUG) */
			mfspr	r4,pir							/* (TEST/DEBUG) */
			b		gotpir3							/* (TEST/DEBUG) */
nopir3:		li		r4,0							/* (TEST/DEBUG) */
gotpir3:											/* (TEST/DEBUG) */
			lis		r31,HIGH_ADDR(EXT(dbgDispLL))	/* (TEST/DEBUG) */
			rlwinm	r4,r4,24,4,7					/* (TEST/DEBUG) */
			li		r3,0							/* (TEST/DEBUG) */
			mr		r5,r10							/* (TEST/DEBUG) */
			oris	r4,r4,0x3030					/* (TEST/DEBUG) */
			ori		r31,r31,LOW_ADDR(EXT(dbgDispLL))	/* (TEST/DEBUG) */
			ori		r4,r4,0x3230					/* (TEST/DEBUG) */
			mtlr	r31								/* (TEST/DEBUG) */
			blrl									/* (TEST/DEBUG) */
nono2:
			lmw		r1,0x284(br0)					/* (TEST/DEBUG) */
			lwz		r0,0x280(br0)					/* (TEST/DEBUG) */
			li		r16,0							/* (TEST/DEBUG) */
			stw		r16,0x20(br0)					/* (TEST/DEBUG) */
			mtdec	r17								/* (TEST/DEBUG) */

#endif

		

/*
 *			At this point, we can take another exception and lose nothing.
 *
 *			We still have the current savearea pointed to by R13, the next by R10 and
 *			sprg1.  R20 contains the pointer to a trace entry and CR5_eq says
 *			to do the trace or not.
 *
 *			Note that R13 was chosen as the save area pointer because the SIGP,
 *			firmware, and DSI/ISI handlers aren't supposed to touch anything
 *			over R12. But, actually, the DSI/ISI stuff does.
 *
 *
 *			Let's cut that trace entry now.
 */

			bne-	cr5,skipTrace2					/* Don't want to trace this kind... */

			li		r14,32							/* Second line of entry */

getTB:		mftbu	r16								/* Get the upper timebase */
			mftb	r17								/* Get the lower timebase */
			mftbu	r18								/* Get the upper one again  */
			cmplw	r16,r18							/* Did the top tick? */
			bne-	getTB							/* Yeah, need to get it again... */
		
			dcbz	r14,r20							/* Zap the second half */
			
			stw		r16,LTR_timeHi(r20)				/* Set the upper part of TB */
			bne		cr6,nopir4						/* Is there a processor ID number on this guy? */
			mfspr	r19,pir							/* Get the processor address */
			b		gotpir4							/* Got it... */
nopir4:		li		r19,0							/* Assume processor 0 for those underprivileged folks */
gotpir4:											
			lwz		r1,saver1(r13)					/* Get back interrupt time R1 */
			stw		r17,LTR_timeLo(r20)				/* Set the lower part of TB */
			rlwinm	r19,r19,0,27,31					/* Cut the junk */
			stw		r0,LTR_r0(r20)					/* Save off register 0 */			
			sth		r19,LTR_cpu(r20)				/* Stash the cpu address */
			stw		r1,LTR_r1(r20)					/* Save off register 1 */			
			stw		r2,LTR_r2(r20)					/* Save off register 2 */			
			stw		r3,LTR_r3(r20)					/* Save off register 3 */	
			lwz		r16,savecr(r13)					/* We don't remember the CR anymore, get it */
			stw		r4,LTR_r4(r20)					/* Save off register 4 */
			mfsrr0	r17								/* Get this back, it's still good */
			stw		r5,LTR_r5(r20)					/* Save off register 5 */	
			mfsrr1	r18								/* This is still good in here also */
			
			stw		r16,LTR_cr(r20)					/* Save the CR (or dec) */
			stw		r17,LTR_srr0(r20)				/* Save the SSR0 */
			stw		r18,LTR_srr1(r20)				/* Save the SRR1 */
			mfdar	r17								/* Get this back */

			mflr	r16								/* Get the LR */
			stw		r17,LTR_dar(r20)				/* Save the DAR */
			mfctr	r17								/* Get the CTR */
			stw		r16,LTR_lr(r20)					/* Save the LR */
#if 0
			lis		r17,HIGH_ADDR(EXT(saveanchor))	; (TEST/DEBUG)
			ori		r17,r17,LOW_ADDR(EXT(saveanchor))	; (TEST/DEBUG)
			lwz		r16,SVcount(r17)				; (TEST/DEBUG)
			lwz		r17,SVinuse(r17)				; (TEST/DEBUG)
			rlwimi	r17,r16,16,0,15					; (TEST/DEBUG)
#endif
			stw		r17,LTR_ctr(r20)				/* Save off the CTR */
			stw		r13,LTR_save(r20)				/* Save the savearea */
			sth		r11,LTR_excpt(r20)				/* Save the exception type */
#if ESPDEBUG
			addi	r17,r20,32						; (TEST/DEBUG)
			dcbst	br0,r20							; (TEST/DEBUG)
			dcbst	br0,r17							; (TEST/DEBUG)
			sync									; (TEST/DEBUG)
#endif

/*
 *			We're done with the trace, except for maybe modifying the exception
 *			code later on. So, that means that we need to save R20 and CR5, but
 *			R0 to R5 are clear now.
 *			
 *			So, let's finish setting up the kernel registers now.
 */

skipTrace2:	

#if PERFTIMES && DEBUG
			li		r3,68							; Indicate interrupt
			mr		r4,r11							; Get code to log
			mr		r5,r13							; Get savearea to log
			mr		r8,r0							; Save R0
			bl		EXT(dbgLog2)					; Cut log entry
			mr		r0,r8							; Restore R0
#endif

			mfsprg	r2,0							/* Get the per processor block */

#if 0
			lis		r19,0xF300						/* (TEST/DEBUG) */
			ori		r19,r19,0x0020					/* (TEST/DEBUG) */
			dcbi	0,r19							/* (TEST/DEBUG) */
			sync									/* (TEST/DEBUG) */
			eieio									/* (TEST/DEBUG) */
			lwz		r18,0x000C(r19)					/* (TEST/DEBUG) */
			eieio									/* (TEST/DEBUG) */
			dcbi	0,r19							/* (TEST/DEBUG) */
			rlwinm.	r4,r18,0,19,19					/* (TEST/DEBUG) */
			rlwinm	r18,r18,0,20,18					/* (TEST/DEBUG) */
			sync									/* (TEST/DEBUG) */
			eieio									/* (TEST/DEBUG) */
			beq+	nonmi							/* (TEST/DEBUG) */

			stw		r18,0x0008(r19)					/* (TEST/DEBUG) */
			dcbi	0,r19							/* (TEST/DEBUG) */
			sync									/* (TEST/DEBUG) */
			eieio									/* (TEST/DEBUG) */
			BREAKPOINT_TRAP							/* (TEST/DEBUG) */

nonmi:												/* (TEST/DEBUG) */
#endif
		
#if 0
			lis		r4,hi16(EXT(hash_table_base))	; (TEST/DEBUG)
			ori		r4,r4,lo16(EXT(hash_table_base))	; (TEST/DEBUG)
			lwz		r8,0(r4)						; (TEST/DEBUG)
			lwz		r4,4(r4)						; (TEST/DEBUG)
			add		r8,r4,r8						; (TEST/DEBUG)
			
yyyck:		lwz		r12,4(r8)						; (TEST/DEBUG)
			rlwimn.	r12,r12,0,24,31					; (TEST/DEBUG)
			bne+	yyyok							; (TEST/DEBUG)
			BREAKPOINT_TRAP							; (TEST/DEBUG)

yyyok:		addi	r8,r8,0x40						; (TEST/DEBUG)
			subic	r4,r4,0x40						; (TEST/DEBUG)
			bgt+	yyyck							; (TEST/DEBUG)
#endif

#if CHECKSAVE

			lis		r4,0x7FFF						/* (TEST/DEBUG) */
			mfdec	r12								/* (TEST/DEBUG) */
			or		r4,r4,r12						/* (TEST/DEBUG) */
			mtdec	r4								/* (TEST/DEBUG) */
			li		r4,0x20							/* (TEST/DEBUG) */
		
			lwarx	r8,0,r4							; ?

mpwait2:	lwarx	r8,0,r4							/* (TEST/DEBUG) */
			mr.		r8,r8							/* (TEST/DEBUG) */
			bne-	mpwait2							/* (TEST/DEBUG) */
			stwcx.	r4,0,r4							/* (TEST/DEBUG) */
			bne-	mpwait2							/* (TEST/DEBUG) */

			isync									/* (TEST/DEBUG) */
			lwz		r4,0xD80(br0)					/* (TEST/DEBUG) */
			mr.		r4,r4							/* (TEST/DEBUG) */
			li		r4,1							/* (TEST/DEBUG) */
			bne-	doncheksv						/* (TEST/DEBUG) */
		
			lis		r8,HIGH_ADDR(EXT(saveanchor))	/* (TEST/DEBUG) */
			ori		r8,r8,LOW_ADDR(EXT(saveanchor))	/* (TEST/DEBUG) */
		
			stw		r4,0xD80(br0)					/* (TEST/DEBUG) */

			lwarx	r4,0,r8							; ?

mpwait2x:	lwarx	r4,0,r8							/* (TEST/DEBUG) */
			mr.		r4,r4							/* (TEST/DEBUG) */
			bne-	mpwait2x						/* (TEST/DEBUG) */
			stwcx.	r8,0,r8							/* (TEST/DEBUG) */
			bne-	mpwait2x						/* (TEST/DEBUG) */

			isync									/* (TEST/DEBUG) */

#if 0
			rlwinm	r4,r13,0,0,19					/* (TEST/DEBUG) */
			lwz		r21,SACflags(r4)				/* (TEST/DEBUG) */
			rlwinm	r22,r21,24,24,31				/* (TEST/DEBUG) */
			cmplwi	r22,0x00EE						/* (TEST/DEBUG) */
			lwz		r22,SACvrswap(r4)				/* (TEST/DEBUG) */
			bne-	currbad							/* (TEST/DEBUG) */
			andis.	r21,r21,hi16(sac_perm)			/* (TEST/DEBUG) */
			bne-	currnotbad						/* (TEST/DEBUG) */
			mr.		r22,r22							/* (TEST/DEBUG) */
			bne+	currnotbad						/* (TEST/DEBUG) */
			
currbad:	lis		r23,hi16(EXT(debugbackpocket))	/* (TEST/DEBUG) */
			ori		r23,r23,lo16(EXT(debugbackpocket))	/* (TEST/DEBUG) */
			stw		r23,SVfree(r8)					/* (TEST/DEBUG) */

			mfsprg	r25,1							/* (TEST/DEBUG) */
			mtsprg	1,r23							/* (TEST/DEBUG) */
			lwz		r26,SACalloc(r23)				/* (TEST/DEBUG) */
			rlwinm	r26,r26,0,1,31					/* (TEST/DEBUG) */
			stw		r26,SACalloc(r23)				/* (TEST/DEBUG) */

			sync									/* (TEST/DEBUG) */
			li		r28,0							/* (TEST/DEBUG) */
			stw		r28,0x20(br0)					/* (TEST/DEBUG) */
			stw		r28,0(r8)						/* (TEST/DEBUG) */
			BREAKPOINT_TRAP							/* (TEST/DEBUG) */

currnotbad:			
#endif
		
			lwz		r28,SVcount(r8)					/* (TEST/DEBUG) */
#if RUPTTRC
			stw		r0,0x280(br0)					/* (TEST/DEBUG) */
			stmw	r1,0x284(br0)					/* (TEST/DEBUG) */
			bne		cr7,nono90						/* (TEST/DEBUG) */
			lwz		r0,0(br0)						/* (TEST/DEBUG) */
			mr.		r0,r0							/* (TEST/DEBUG) */
			beq+	nono90							/* (TEST/DEBUG) */
			bne		cr6,nopir5						/* (TEST/DEBUG) */
			mfspr	r4,pir							/* (TEST/DEBUG) */
			b		gotpir0							/* (TEST/DEBUG) */
nopir5:		li		r4,0							/* (TEST/DEBUG) */
gotpir5:											/* (TEST/DEBUG) */
			lis		r31,HIGH_ADDR(EXT(dbgDispLL))	/* (TEST/DEBUG) */
			rlwinm	r4,r4,24,4,7					/* (TEST/DEBUG) */
			li		r3,0							/* (TEST/DEBUG) */
			mr		r5,r28							/* (TEST/DEBUG) */
			oris	r4,r4,0x3036					/* (TEST/DEBUG) */
			ori		r31,r31,LOW_ADDR(EXT(dbgDispLL))	/* (TEST/DEBUG) */
			ori		r4,r4,0x3636					/* (TEST/DEBUG) */
			mtlr	r31								/* (TEST/DEBUG) */
			blrl									/* (TEST/DEBUG) */
nono90:
			lmw		r1,0x284(br0)					/* (TEST/DEBUG) */
			lwz		r0,0x280(br0)					/* (TEST/DEBUG) */
#endif
			lwz		r21,SVinuse(r8)					/* (TEST/DEBUG) */
			lwz		r23,SVmin(r8)					/* (TEST/DEBUG) */
			sub		r22,r28,r21						/* (TEST/DEBUG) */
			cmpw	r22,r23							/* (TEST/DEBUG) */
			bge+	cksave0							/* (TEST/DEBUG) */
			
			li		r4,0							/* (TEST/DEBUG) */
			stw		r4,0x20(br0)					/* (TEST/DEBUG) */
			stw		r4,0(r8)						/* (TEST/DEBUG) */
			BREAKPOINT_TRAP							/* (TEST/DEBUG) */
			
cksave0:	lwz		r28,SVfree(r8)					/* (TEST/DEBUG) */
			li		r24,0							/* (TEST/DEBUG) */
			li		r29,1							/* (TEST/SAVE) */
			
cksave0a:	mr.		r28,r28							/* (TEST/DEBUG) */
			beq-	cksave3							/* (TEST/DEBUG) */
			
			rlwinm.	r21,r28,0,4,19					/* (TEST/DEBUG) */
			bne+	cksave1							/* (TEST/DEBUG) */
			
			li		r4,0							/* (TEST/DEBUG) */
			stw		r4,0x20(br0)					/* (TEST/DEBUG) */
			stw		r4,0(r8)						/* (TEST/DEBUG) */
			BREAKPOINT_TRAP							/* (TEST/DEBUG) */
			
cksave1:	rlwinm.	r21,r28,0,21,3					/* (TEST/DEBUG) */
			beq+	cksave2							/* (TEST/DEBUG) */
			
			li		r4,0							/* (TEST/DEBUG) */
			stw		r4,0x20(br0)					/* (TEST/DEBUG) */
			stw		r4,0(r8)						/* (TEST/DEBUG) */
			BREAKPOINT_TRAP							/* (TEST/DEBUG) */
			
cksave2:	lwz		r25,SACalloc(r28)				/* (TEST/DEBUG) */
			lbz		r26,SACflags+2(r28)				/* (TEST/DEBUG) */
			lbz		r21,SACflags+3(r28)				/* (TEST/DEBUG) */
			cmplwi	r26,0x00EE						/* (TEST/DEBUG) */
			stb		r29,SACflags+3(r28)				/* (TEST/DEBUG) */
			beq+	cksave2z
			
			li		r4,0							/* (TEST/DEBUG) */
			stw		r4,0x20(br0)					/* (TEST/DEBUG) */
			stw		r4,0(r8)						/* (TEST/DEBUG) */
			BREAKPOINT_TRAP							/* (TEST/DEBUG) */

cksave2z:	mr.		r21,r21							/* (TEST/DEBUG) */
			beq+	cksave2a						/* (TEST/DEBUG) */
			
			li		r4,0							/* (TEST/DEBUG) */
			stw		r4,0x20(br0)					/* (TEST/DEBUG) */
			stw		r4,0(r8)						/* (TEST/DEBUG) */
			BREAKPOINT_TRAP							/* (TEST/DEBUG) */

cksave2a:	rlwinm	r26,r25,1,31,31					/* (TEST/DEBUG) */
			rlwinm	r27,r25,2,31,31					/* (TEST/DEBUG) */
			add		r24,r24,r26						/* (TEST/DEBUG) */
			add		r24,r24,r27						/* (TEST/DEBUG) */
			lwz		r28,SACnext(r28)				/* (TEST/DEBUG) */
			b		cksave0a						/* (TEST/DEBUG) */
			
cksave3:	cmplw	r24,r22							/* (TEST/DEBUG) */
			beq+	cksave4							/* (TEST/DEBUG) */
			
			li		r4,0							/* (TEST/DEBUG) */
			stw		r4,0x20(br0)					/* (TEST/DEBUG) */
			stw		r4,0(r8)						/* (TEST/DEBUG) */
			BREAKPOINT_TRAP							/* (TEST/DEBUG) */
			
cksave4:	lwz		r28,SVfree(r8)					/* (TEST/DEBUG) */
			li		r24,0							/* (TEST/DEBUG) */

cksave5:	mr.		r28,r28							/* (TEST/DEBUG) */
			beq-	cksave6							/* (TEST/DEBUG) */
			stb		r24,SACflags+3(r28)				/* (TEST/DEBUG) */
			lwz		r28,SACnext(r28)				/* (TEST/DEBUG) */
			b		cksave5							/* (TEST/DEBUG) */

cksave6:	

			li		r4,0							/* (TEST/DEBUG) */
			stw		r4,0xD80(br0)					/* (TEST/DEBUG) */
			stw		r4,0(r8)						/* (TEST/DEBUG) */

doncheksv:
			li		r4,0							/* (TEST/DEBUG) */
			stw		r4,0x20(br0)					/* (TEST/DEBUG) */			
			mtdec	r12								/* (TEST/DEBUG) */
#endif

#if 0
			lhz		r4,0x980(br0)					; (TEST/DEBUG) 
			xoris	r4,r4,lo16(0xFFF8)				; (TEST/DEBUG)
			mr.		r4,r4							; (TEST/DEBUG)
			beq-	dontkillmedead					; (TEST/DEBUG)
			mfmsr	r12								; (TEST/DEBUG) 
			ori		r4,r12,0x2000					; (TEST/DEBUG) 	
			mtmsr	r4								; (TEST/DEBUG) 
			isync									; (TEST/DEBUG) 
			stfd	f13,0x980(br0)					; (TEST/DEBUG) 
			mtmsr	r12								; (TEST/DEBUG) 
			isync									; (TEST/DEBUG) 
			lhz		r4,0x980(br0)					; (TEST/DEBUG) 
			xoris	r4,r4,lo16(0xFFF8)				; (TEST/DEBUG)
			mr.		r4,r4							; (TEST/DEBUG)
			bne+	dontkillmedead					; (TEST/DEBUG)
			BREAKPOINT_TRAP							; (TEST/DEBUG)
			
dontkillmedead:										; (TEST/DEBUG)
#endif

			lis		r4,HIGH_ADDR(EXT(MPspec))		/* Get the MP control block */
			dcbt	0,r2							/* We'll need the per_proc in a sec */
			cmplwi	cr0,r11,T_INTERRUPT				/* Do we have an external interrupt? */
			ori		r4,r4,LOW_ADDR(EXT(MPspec))		/* Get the bottom half of the MP control block */
			bne+	notracex						/* Not an external... */

/*
 *			Here we check to see if there was a interprocessor signal 
 */

			lwz		r4,MPSSIGPhandler(r4)			/* Get the address of the SIGP interrupt filter */
			lhz		r3,PP_CPU_FLAGS(r2)				/* Get the CPU flags */
			cmplwi	cr1,r4,0						/* Check if signal filter is initialized yet */
			andi.	r3,r3,LOW_ADDR(SIGPactive)		/* See if this processor has started up */
			mtlr	r4								/* Load up filter address */
			beq-	cr1,notracex					/* We don't have a filter yet... */			
			beq-	notracex						/* This processor hasn't started filtering yet... */
			
			blrl									/* Filter the interrupt */
		
			mfsprg	r2,0							/* Make sure we have the per processor block */			
			cmplwi	cr0,r3,kMPIOInterruptPending	/* See what the filter says */
			li		r11,T_INTERRUPT					/* Assume we have a regular external 'rupt */
			beq+	modRupt							/* Yeah, we figured it would be... */
			li		r11,T_SIGP						/* Assume we had a signal processor interrupt */
			bgt+	modRupt							/* Yeah, at this point we would assume so... */
			li		r11,T_IN_VAIN					/* Nothing there actually, so eat it */
			
modRupt:	stw		r11,PP_SAVE_EXCEPTION_TYPE(r2)	/* Set that it was either in vain or a SIGP */
			stw		r11,saveexception(r13)			/* Save the exception code here also */
			bne-	cr5,notracex					/* Jump if no tracing... */
			sth		r11,LTR_excpt(r20)				/* Save the exception type */

notracex:	

#if 0		
			bne		cr6,nopir6						/* (TEST/DEBUG) */
			mfspr	r7,pir							/* (TEST/DEBUG) */
			b		gotpir6							/* (TEST/DEBUG) */
nopir6:		li		r7,0							/* (TEST/DEBUG) */
gotpir6:											/* (TEST/DEBUG) */
			lis		r6,HIGH_ADDR(EXT(RuptCtrs))		/* (TEST/DEBUG) */
			rlwinm	r7,r7,8,23,23					/* (TEST/DEBUG) */
			lis		r12,HIGH_ADDR(EXT(GratefulDeb))	/* (TEST/DEBUG) */
			rlwimi	r7,r7,1,22,22					/* (TEST/DEBUG) */
			ori		r6,r6,LOW_ADDR(EXT(RuptCtrs))	/* (TEST/DEBUG) */
			rlwinm	r1,r11,2,0,29					/* (TEST/DEBUG) */
			add		r6,r6,r7						/* (TEST/DEBUG) */
			ori		r12,r12,LOW_ADDR(EXT(GratefulDeb))	/* (TEST/DEBUG) */
			lwz		r21,(47*16)+8(r6)				/* (TEST/DEBUG) */
			lwz		r22,(47*16)+12(r6)				/* (TEST/DEBUG) */
			add		r1,r1,r6						/* (TEST/DEBUG) */
			mftb	r24								/* (TEST/DEBUG) */
			sub		r22,r24,r22						/* (TEST/DEBUG) */
			lwz		r4,4(r6)						/* (TEST/DEBUG) */
			cmplw	cr2,r22,r21						/* (TEST/DEBUG) */
			lwz		r7,4(r1)						/* (TEST/DEBUG) */
			lwz		r21,8(r6)						/* (TEST/DEBUG) */
			blt+	cr2,nottime						/* (TEST/DEBUG) */
			stw		r24,(47*16)+12(r6)				/* (TEST/DEBUG) */
			
nottime:	addi	r4,r4,1							/* (TEST/DEBUG) */
			lwz		r22,8(r1)						/* (TEST/DEBUG) */
			addi	r7,r7,1							/* (TEST/DEBUG) */
			stw		r4,4(r6)						/* (TEST/DEBUG) */
			lwz		r3,0(r6)						/* (TEST/DEBUG) */
			mr.		r21,r21							/* (TEST/DEBUG) */
			stw		r7,4(r1)						/* (TEST/DEBUG) */
			mtlr	r12								/* (TEST/DEBUG) */
			lwz		r1,0(r1)						/* (TEST/DEBUG) */
			beq-	nottimed1						/* (TEST/DEBUG) */
			blt+	cr2,isnttime1					/* (TEST/DEBUG) */
						
nottimed1:	mr.		r3,r3							/* (TEST/DEBUG) */
			bgelrl+									/* (TEST/DEBUG) */

isnttime1:	mr.		r22,r22							/* (TEST/DEBUG) */
			beq-	nottimed2						/* (TEST/DEBUG) */
			blt+	cr2,isnttime2					/* (TEST/DEBUG) */
			
nottimed2:	mr.		r3,r1							/* (TEST/DEBUG) */
			mtlr	r12								/* (TEST/DEBUG) */
			mr		r4,r7							/* (TEST/DEBUG) */
			bgelrl+									/* (TEST/DEBUG) */
			mr		r3,r11							/* (TEST/DEBUG) */
			
isnttime2:	cmplwi	r11,T_DATA_ACCESS				/* (TEST/DEBUG) */
			lis		r12,HIGH_ADDR(EXT(GratefulDeb))	/* (TEST/DEBUG) */
			bne+	nodsidisp						/* (TEST/DEBUG) */
			mr.		r22,r22							/* (TEST/DEBUG) */
			beq-	nottimed3						/* (TEST/DEBUG) */
			blt+	cr2,nodsidisp					/* (TEST/DEBUG) */

nottimed3:	li		r3,5							/* (TEST/DEBUG) */
			ori		r12,r12,LOW_ADDR(EXT(GratefulDeb))	/* (TEST/DEBUG) */
			lwz		r4,savesrr0(r13)				/* (TEST/DEBUG) */
			mtlr	r12								/* (TEST/DEBUG) */
			blrl									/* (TEST/DEBUG) */
			
			lis		r12,HIGH_ADDR(EXT(GratefulDeb))	/* (TEST/DEBUG) */
			ori		r12,r12,LOW_ADDR(EXT(GratefulDeb))	/* (TEST/DEBUG) */
			lis		r3,9							/* (TEST/DEBUG) */
			ori		r3,r3,5							/* (TEST/DEBUG) */
			mtlr	r12								/* (TEST/DEBUG) */
			lwz		r4,savedar(r13)					/* (TEST/DEBUG) */
			blrl									/* (TEST/DEBUG) */

nodsidisp:	cmplwi	r11,T_INSTRUCTION_ACCESS		/* (TEST/DEBUG) */
			lis		r12,HIGH_ADDR(EXT(GratefulDeb))	/* (TEST/DEBUG) */
			bne+	noisidisp						/* (TEST/DEBUG) */
			mr.		r22,r22							/* (TEST/DEBUG) */
			beq-	nottimed4						/* (TEST/DEBUG) */
			blt+	cr2,noisidisp					/* (TEST/DEBUG) */

nottimed4:	li		r3,6							/* (TEST/DEBUG) */
			ori		r12,r12,LOW_ADDR(EXT(GratefulDeb))	/* (TEST/DEBUG) */
			lwz		r4,savesrr0(r13)				/* (TEST/DEBUG) */
			mtlr	r12								/* (TEST/DEBUG) */
			blrl									/* (TEST/DEBUG) */

noisidisp:	mr		r3,r11							/* (TEST/DEBUG) */		
#endif

#if 0
			cmplwi	r11,T_PROGRAM					/* (TEST/DEBUG) */
			lis		r12,HIGH_ADDR(EXT(GratefulDeb))	/* (TEST/DEBUG) */
			bne+	nopgmdisp						/* (TEST/DEBUG) */
			li		r3,7							/* (TEST/DEBUG) */
			ori		r12,r12,LOW_ADDR(EXT(GratefulDeb))	/* (TEST/DEBUG) */
			lwz		r4,savesrr0(r13)				/* (TEST/DEBUG) */
			mtlr	r12								/* (TEST/DEBUG) */
			blrl									/* (TEST/DEBUG) */

nopgmdisp:	mr		r3,r11							/* (TEST/DEBUG) */		
#endif

#if RUPTTRC
		
			lis		r6,0x7FFF						/* (TEST/DEBUG) */
			mfdec	r7								/* (TEST/DEBUG) */
			or		r6,r6,r7						/* (TEST/DEBUG) */
			mtdec	r6								/* (TEST/DEBUG) */
			li		r6,0x20							/* (TEST/DEBUG) */
		
			lwarx	r1,0,r6							; ?

mpwait3:	lwarx	r1,0,r6							/* (TEST/DEBUG) */
			mr.		r1,r1							/* (TEST/DEBUG) */
			bne-	mpwait3							/* (TEST/DEBUG) */
			stwcx.	r6,0,r6							/* (TEST/DEBUG) */
			bne-	mpwait3							/* (TEST/DEBUG) */
			
			stw		r0,0x280(br0)					/* (TEST/DEBUG) */
			stmw	r1,0x284(br0)					/* (TEST/DEBUG) */
			bne		cr7,nono3						/* (TEST/DEBUG) */
			lwz		r0,0(br0)						/* (TEST/DEBUG) */
			mr.		r0,r0							/* (TEST/DEBUG) */
			beq+	nono3							/* (TEST/DEBUG) */
			bne		cr6,nopir7						/* (TEST/DEBUG) */
			mfspr	r4,pir							/* (TEST/DEBUG) */
			b		gotpir7							/* (TEST/DEBUG) */
nopir7:		li		r4,0							/* (TEST/DEBUG) */
gotpir7:											/* (TEST/DEBUG) */
			lis		r31,HIGH_ADDR(EXT(dbgDispLL))	/* (TEST/DEBUG) */
			rlwinm	r4,r4,24,4,7					/* (TEST/DEBUG) */
			li		r3,0							/* (TEST/DEBUG) */
			mr		r5,r11							/* (TEST/DEBUG) */
			oris	r4,r4,0x3030					/* (TEST/DEBUG) */
			ori		r31,r31,LOW_ADDR(EXT(dbgDispLL))	/* (TEST/DEBUG) */
			ori		r4,r4,0x3330					/* (TEST/DEBUG) */
			mtlr	r31								/* (TEST/DEBUG) */
			blrl									/* (TEST/DEBUG) */
			
			bne		cr6,nopir8						/* (TEST/DEBUG) */
			mfspr	r4,pir							/* (TEST/DEBUG) */
			b		gotpir8							/* (TEST/DEBUG) */
nopir8:		li		r4,0							/* (TEST/DEBUG) */
gotpir8:											/* (TEST/DEBUG) */
			lis		r31,HIGH_ADDR(EXT(dbgDispLL))	/* (TEST/DEBUG) */
			rlwinm	r4,r4,24,4,7					/* (TEST/DEBUG) */
			li		r3,0							/* (TEST/DEBUG) */
			lwz		r5,0x280(br0)					/* (TEST/DEBUG) */
			oris	r4,r4,0x3030					/* (TEST/DEBUG) */
			ori		r31,r31,LOW_ADDR(EXT(dbgDispLL))	/* (TEST/DEBUG) */
			ori		r4,r4,0x3430					/* (TEST/DEBUG) */
			mtlr	r31								/* (TEST/DEBUG) */
			blrl									/* (TEST/DEBUG) */
nono3:
			lmw		r1,0x284(br0)					/* (TEST/DEBUG) */
			lwz		r0,0x280(br0)					/* (TEST/DEBUG) */
			li		r6,0							/* (TEST/DEBUG) */
			stw		r6,0x20(br0)					/* (TEST/DEBUG) */
			mtdec	r7								/* (TEST/DEBUG) */

#endif

			li		r21,0							; Assume no processor register for now
			lis		r12,hi16(EXT(hw_counts))		; Get the high part of the interrupt counters
			bne-	cr6,nopirhere					; Jump if this processor does not have a PIR...
			mfspr	r21,pir							; Get the PIR	

nopirhere:	ori		r12,r12,lo16(EXT(hw_counts))	; Get the low part of the interrupt counters
			rlwinm	r21,r21,8,20,23					; Get index to processor counts
			mtcrf	0x80,r0							/* Set our CR0 to the high nybble of the request code */
			rlwinm	r6,r0,1,0,31					/* Move sign bit to the end */
			cmplwi	cr1,r11,T_SYSTEM_CALL			/* Did we get a system call? */
			crandc	cr0_lt,cr0_lt,cr0_gt			/* See if we have R0 equal to 0b10xx...x */
			add		r12,r12,r21						; Point to the processor count area
			cmplwi	cr3,r11,T_IN_VAIN				/* Was this all in vain? All for nothing? */
			lwzx	r22,r12,r11						; Get the old value
			cmplwi	cr2,r6,1						/* See if original R0 had the CutTrace request code in it */
			addi	r22,r22,1						; Count this one
			cmplwi	cr4,r11,T_SIGP					/* Indicate if we had a SIGP 'rupt */
			stwx	r22,r12,r11						; Store it back
			
			beq-	cr3,EatRupt						/* Interrupt was all for nothing... */
			cmplwi	cr3,r11,T_MACHINE_CHECK			; Did we get a machine check?
			bne+	cr1,noCutT						/* Not a system call... */
			bnl+	cr0,noCutT						/* R0 not 0b10xxx...x, can't be any kind of magical system call... */
			beq-	cr2,isCutTrace					/* This is a CutTrace system call */
			
/*
 *			Here's where we call the firmware.  If it returns T_IN_VAIN, that means
 *			that it has handled the interruption.  Remember: thou shalt not trash R13
 *			or R20 while you are away.  Anything else is ok.
 */


			lis		r1,HIGH_ADDR(EXT(FirmwareCall))	/* Top half of firmware call handler */
			ori		r1,r1,LOW_ADDR(EXT(FirmwareCall))	/* Bottom half of it */
			lwz		r3,saver3(r13)					/* Restore the first parameter, the rest are ok already */
			mtlr	r1								/* Get it in the link register */
			blrl									/* Call the handler */

#if RUPTTRC
			lis		r7,0x7FFF						/* (TEST/DEBUG) */
			mfdec	r6								/* (TEST/DEBUG) */
			or		r7,r7,r6						/* (TEST/DEBUG) */
			mtdec	r7								/* (TEST/DEBUG) */
			li		r7,0x20							/* (TEST/DEBUG) */
		
			lwarx	r1,0,r7							; ?

mpwait4:	lwarx	r1,0,r7							/* (TEST/DEBUG) */
			mr.		r1,r1							/* (TEST/DEBUG) */
			bne-	mpwait4							/* (TEST/DEBUG) */
			stwcx.	r7,0,r7							/* (TEST/DEBUG) */
			bne-	mpwait4							/* (TEST/DEBUG) */
			
			stw		r0,0x280(br0)					/* (TEST/DEBUG) */
			stmw	r1,0x284(br0)					/* (TEST/DEBUG) */
			bne		cr7,nono4						/* (TEST/DEBUG) */
			lwz		r0,0(br0)						/* (TEST/DEBUG) */
			mr.		r0,r0							/* (TEST/DEBUG) */
			beq+	nono4							/* (TEST/DEBUG) */
			
			bne		cr6,nopir9						/* (TEST/DEBUG) */
			mfspr	r4,pir							/* (TEST/DEBUG) */
			b		gotpir9							/* (TEST/DEBUG) */
nopir9:		li		r4,0							/* (TEST/DEBUG) */
gotpir9:											/* (TEST/DEBUG) */
			lis		r31,HIGH_ADDR(EXT(dbgDispLL))	/* (TEST/DEBUG) */
			rlwinm	r4,r4,24,4,7					/* (TEST/DEBUG) */
			mr		r5,r3							/* (TEST/DEBUG) */
			li		r3,0							/* (TEST/DEBUG) */
			oris	r4,r4,0x3030					/* (TEST/DEBUG) */
			ori		r31,r31,LOW_ADDR(EXT(dbgDispLL))	/* (TEST/DEBUG) */
			ori		r4,r4,0x3530					/* (TEST/DEBUG) */
			mtlr	r31								/* (TEST/DEBUG) */
			blrl									/* (TEST/DEBUG) */
			mtdec	r25								/* (TEST/DEBUG) */
nono4:
			lmw		r1,0x284(br0)					/* (TEST/DEBUG) */
			lwz		r0,0x280(br0)					/* (TEST/DEBUG) */
			li		r7,0							/* (TEST/DEBUG) */
			stw		r7,0x20(br0)					/* (TEST/DEBUG) */
			mtdec	r6								/* (TEST/DEBUG) */
#endif

			cmplwi	r3,T_IN_VAIN					/* Was it handled? */
			mfsprg	r2,0							/* Restore the per_processor area */
			beq+	EatRupt							/* Interrupt was handled... */
			mr		r11,r3							/* Put the 'rupt code in the right register */
			b		noSIGP							/* Go to the normal system call handler */
			
isCutTrace:				
			li		r7,-32768						/* Get a 0x8000 for the exception code */
			bne-	cr5,EatRupt						/* Tracing is disabled... */
			sth		r7,LTR_excpt(r20)				/* Modify the exception type to a CutTrace */
			b		EatRupt							/* Time to go home... */

/*			We are here 'cause we didn't have a CutTrace system call */

noCutT:		beq-	cr3,MachineCheck				; Whoa... Machine check...
			bne+	cr4,noSIGP						/* Skip away if we didn't get a SIGP... */
		
			lis		r6,HIGH_ADDR(EXT(MPsignalFW))	/* Top half of SIGP handler */
			ori		r6,r6,LOW_ADDR(EXT(MPsignalFW))	/* Bottom half of it */
			mtlr	r6								/* Get it in the link register */
			
			blrl									/* Call the handler - we'll only come back if this is an AST,  */
													/* 'cause FW can't handle that */
			mfsprg	r2,0							/* Restore the per_processor area */
;
;			The following interrupts are the only ones that can be redriven
;			by the higher level code or emulation routines.
;

Redrive:	cmplwi	cr0,r3,T_IN_VAIN				/* Did the signal handler eat the signal? */
			mr		r11,r3							/* Move it to the right place */
			beq+	cr0,EatRupt						/* Bail now if the signal handler processed the signal... */


/*
 *			Here's where we check for the other fast-path exceptions: translation exceptions,
 *			emulated instructions, etc.
 */

noSIGP:		cmplwi	cr3,r11,T_ALTIVEC_ASSIST		; Check for an Altivec denorm assist
			cmplwi	cr1,r11,T_PROGRAM				/* See if we got a program exception */
			cmplwi	cr2,r11,T_INSTRUCTION_ACCESS	/* Check on an ISI */
			bne+	cr3,noAltivecAssist				; It is not an assist...
			b		EXT(AltivecAssist)				; It is an assist...

noAltivecAssist:
			bne+	cr1,noEmulate					; No emulation here...
			b		EXT(Emulate)					; Go try to emulate...

noEmulate:	cmplwi	cr3,r11,T_CSWITCH				/* Are we context switching */
			cmplwi	r11,T_DATA_ACCESS				/* Check on a DSI */
			beq-	cr2,DSIorISI					/* It's a PTE fault... */
			beq-	cr3,conswtch					/* It's a context switch... */
			bne+	PassUp							/* It's not a PTE fault... */

/*
 *			This call will either handle the fault, in which case it will not
 *			return, or return to pass the fault up the line.
 */

DSIorISI:
			lis		r7,HIGH_ADDR(EXT(handlePF))		/* Top half of DSI handler */
			ori		r7,r7,LOW_ADDR(EXT(handlePF))	/* Bottom half of it */
			mtlr	r7								/* Get it in the link register */
			mr		r3,r11							/* Move the 'rupt code */
			
			blrl									/* See if we can handle this fault  */

#if RUPTTRC
			bne		cr7,nononono2					/* (TEST/DEBUG) */
			lwz		r7,0(br0)						/* (TEST/DEBUG) */
			mr.		r7,r7							/* (TEST/DEBUG) */
			beq+	nononono2						/* (TEST/DEBUG) */
#if 0			
			lis		r7,0x7FFF						/* (TEST/DEBUG) */
			mfdec	r2								/* (TEST/DEBUG) */
			or		r7,r7,r2						/* (TEST/DEBUG) */
			mtdec	r7								/* (TEST/DEBUG) */
			li		r7,0x20							/* (TEST/DEBUG) */
			
			lwarx	r0,0,r7							; ?

yesyesyes2:	lwarx	r0,0,r7							/* (TEST/DEBUG) */
			mr.		r0,r0							/* (TEST/DEBUG) */
			li		r0,1							/* (TEST/DEBUG) */
			bne-	yesyesyes2						/* (TEST/DEBUG) */
			stwcx.	r0,0,r7							/* (TEST/DEBUG) */
			bne-	yesyesyes2						/* (TEST/DEBUG) */
#endif
			stw		r0,0x280(br0)					/* (TEST/DEBUG) */
			stmw	r1,0x284(br0)					/* (TEST/DEBUG) */
			mr		r20,r3
			bne		cr6,nopir10						/* (TEST/DEBUG) */
			mfspr	r4,pir							/* (TEST/DEBUG) */
			b		gotpir10						/* (TEST/DEBUG) */
nopir10:	li		r4,0							/* (TEST/DEBUG) */
gotpir10:											/* (TEST/DEBUG) */
			lis		r31,HIGH_ADDR(EXT(dbgDispLL))	/* (TEST/DEBUG) */
			rlwinm	r4,r4,24,4,7					/* (TEST/DEBUG) */
			li		r3,0							/* (TEST/DEBUG) */
			or		r4,r4,r20						/* (TEST/DEBUG) */
			lwz		r5,savedar(r13)					/* (TEST/DEBUG) */
			oris	r4,r4,0x3030					/* (TEST/DEBUG) */
			ori		r31,r31,LOW_ADDR(EXT(dbgDispLL))	/* (TEST/DEBUG) */
			ori		r4,r4,0x3630					/* (TEST/DEBUG) */
			mtlr	r31								/* (TEST/DEBUG) */
			blrl									/* (TEST/DEBUG) */

#if 0
			lis		r31,HIGH_ADDR(EXT(dbgRegs))		/* (TEST/DEBUG) */
			ori		r31,r31,LOW_ADDR(EXT(dbgRegs))	/* (TEST/DEBUG) */
			mtlr	r31								/* (TEST/DEBUG) */
			li		r3,0							/* (TEST/DEBUG) */
			blrl									/* (TEST/DEBUG) */
#endif
			lmw		r1,0x284(br0)					/* (TEST/DEBUG) */
			lwz		r0,0x280(br0)					/* (TEST/DEBUG) */
			li		r0,0							/* (TEST/DEBUG) */
			stw		r0,0x20(br0)					/* (TEST/DEBUG) */
			mtdec	r2								/* (TEST/DEBUG) */
nononono2:											/* (TEST/DEBUG) */			
#endif

			lwz		r0,savesrr1(r13)				; Get the MSR in use at exception time
			mfsprg	r2, 0							/* Get back per_proc */
			cmplwi	cr1,r3,T_IN_VAIN				; Was it handled?
			andi.	r4,r0,lo16(MASK(MSR_RI))		; See if the recover bit is on
			mr		r11,r3							/* Make sure we can find this later */
			beq+	cr1,EatRupt						; Yeah, just blast back to the user... 
			andc	r0,r0,r4						; Remove the recover bit
			beq+	PassUp							; Not on, normal case...
			lwz		r4,savesrr0(r13)				; Get the failing instruction address
			lwz		r5,savecr(r13)					; Get the condition register
			stw		r0,savesrr1(r13)				; Save the result MSR
			addi	r4,r4,4							; Skip failing instruction
			rlwinm	r5,r5,0,3,1						; Clear CR0_EQ to let emulation code know we failed
			stw		r4,savesrr0(r13)				; Save instruction address
			stw		r4,savecr(r13)					; And the resume CR
			b		EatRupt							; Resume emulated code

/*
 *			Here is where we handle the context switch firmware call.  The old 
 *			context has been saved, and the new savearea in in saver3.  We'll just
 *			muck around with the savearea pointers, and then join the exit routine 
 */
conswtch:	lwz		r28,SAVflags(r13)				/* The the flags of the current */
			mr		r29,r13							/* Save the save */
			rlwinm	r30,r13,0,0,19					/* Get the start of the savearea block */
			lwz		r5,saver3(r13)					/* Switch to the new savearea */
			oris	r28,r28,HIGH_ADDR(SAVattach)	/* Turn on the attached flag */
			lwz		r30,SACvrswap(r30)				/* get real to virtual translation */
			mr		r13,r5							/* Switch saveareas */
			xor		r27,r29,r30						/* Flip to virtual */
			stw		r28,SAVflags(r29)				/* Stash it back */
			stw		r27,saver3(r5)					/* Push the new savearea to the switch to routine */
			b		EatRupt							/* Start 'er up... */

;
;			Handle machine check here.
;
; ?
;
MachineCheck:
			lwz		r27,savesrr1(r13)				; ?
			rlwinm.	r11,r27,0,dcmck,dcmck			; ?
			beq+	notDCache						; ?
			
			mfspr	r11,msscr0						; ?
			dssall									; ?
			sync
			
			lwz		r27,savesrr1(r13)				; ?

hiccup:		cmplw	r27,r27							; ?
			bne-	hiccup							; ?
			isync									; ?
			
			oris	r11,r11,hi16(dl1hwfm)			; ?
			mtspr	msscr0,r11						; ?
			
rstbsy:		mfspr	r11,msscr0						; ?
			
			rlwinm.	r11,r11,0,dl1hwf,dl1hwf			; ?
			bne		rstbsy							; ?
			
			sync									; ?

			li		r11,T_IN_VAIN					; ?
			b		EatRupt							; ?

			
notDCache:
;
;			Check if the failure was in 
;			ml_probe_read.  If so, this is expected, so modify the PC to
;			ml_proble_read_mck and then eat the exception.
;
			lwz		r30,savesrr0(r13)				; Get the failing PC
			lis		r28,hi16(EXT(ml_probe_read_mck))	; High order part
			lis		r27,hi16(EXT(ml_probe_read))	; High order part
			ori		r28,r28,lo16(EXT(ml_probe_read_mck))	; Get the low part
			ori		r27,r27,lo16(EXT(ml_probe_read))	; Get the low part
			cmplw	r30,r28							; Check highest possible
			cmplw	cr1,r30,r27						; Check lowest
			bge-	PassUp							; Outside of range
			blt-	cr1,PassUp						; Outside of range
;
;			We need to fix up the BATs here because the probe
;			routine messed them all up... As long as we are at it,
;			fix up to return directly to caller of probe.
;
		
			lwz		r30,saver5(r13)					; Get proper DBAT values
			lwz		r28,saver6(r13)
			lwz		r27,saver7(r13)
			lwz		r11,saver8(r13)
			lwz		r18,saver9(r13)
			
			sync
			mtdbatu	0,r30							; Restore DBAT 0 high
			mtdbatl	0,r28							; Restore DBAT 0 low
			mtdbatu	1,r27							; Restore DBAT 1 high
			mtdbatu	2,r11							; Restore DBAT 2 high
			mtdbatu	3,r18							; Restore DBAT 3 high 
			sync

			lwz		r28,savelr(r13)					; Get return point
			lwz		r27,saver0(r13)					; Get the saved MSR
			li		r30,0							; Get a failure RC
			stw		r28,savesrr0(r13)				; Set the return point
			stw		r27,savesrr1(r13)				; Set the continued MSR
			stw		r30,saver3(r13)					; Set return code
			li		r11,T_IN_VAIN					; Set new interrupt code
			b		EatRupt							; Yum, yum, eat it all up...

/*
 *			Here's where we come back from some instruction emulator.  If we come back with
 *			T_IN_VAIN, the emulation is done and we should just reload state and directly
 *			go back to the interrupted code. Otherwise, we'll check to see if
 *			we need to redrive with a different interrupt, i.e., DSI.
 */
 
			.align	5
			.globl	EXT(EmulExit)

LEXT(EmulExit)

			cmplwi	r11,T_IN_VAIN					/* Was it emulated? */
			lis		r1,hi16(SAVredrive)				; Get redrive request
			mfsprg	r2,0							; Restore the per_proc area
			beq+	EatRupt							/* Yeah, just blast back to the user... */
			lwz		r4,SAVflags(r13)				; Pick up the flags

			and.	r0,r4,r1						; Check if redrive requested
			andc	r4,r4,r1						; Clear redrive

			beq+	PassUp							; No redrive, just keep on going...

			lwz		r3,saveexception(r13)			; Restore exception code
			stw		r4,SAVflags(r13)				; Set the flags
			b		Redrive							; Redrive the exception...
		
/* 			Jump into main handler code switching on VM at the same time */

/* 			We assume kernel data is mapped contiguously in physical
 * 			memory, otherwise we'd need to switch on (at least) virtual data.
 *			SRs are already set up.
 */
PassUp:		lwz		r2,PP_PHYS_EXCEPTION_HANDLERS(r2)	/* Pick up the exception handler base */
			lwzx	r6,r2,r11						/* Get the actual exception handler address */

PassUpDeb:	lwz		r8,SAVflags(r13)				/* Get the flags */
			mtsrr0	r6								/* Set up the handler address */
			oris	r8,r8,HIGH_ADDR(SAVattach)		/* Since we're passing it up, attach it */
			rlwinm	r5,r13,0,0,19					/* Back off to the start of savearea block */
			
			mfmsr	r3								/* Get our MSR */
			stw		r8,SAVflags(r13)				/* Pass up the flags */
			rlwinm	r3,r3,0,MSR_BE_BIT+1,MSR_SE_BIT-1	/* Clear all but the trace bits */
			li		r2,MSR_SUPERVISOR_INT_OFF		/* Get our normal MSR value */
			lwz		r5,SACvrswap(r5)				/* Get real to virtual conversion */			
			or		r2,r2,r3						/* Keep the trace bits if they're on */
			mr		r3,r11							/* Pass the exception code in the paramter reg */
			mtsrr1	r2								/* Set up our normal MSR value */
			xor		r4,r13,r5						/* Pass up the virtual address of context savearea */

#if RUPTTRC		
			lis		r20,0x7FFF						/* (TEST/DEBUG) */
			mfdec	r25								/* (TEST/DEBUG) */
			or		r20,r20,r25						/* (TEST/DEBUG) */
			mtdec	r20								/* (TEST/DEBUG) */
			li		r20,0x20						/* (TEST/DEBUG) */
		
			lwarx	r21,0,r20						; ?

mpwait5:	lwarx	r21,0,r20						/* (TEST/DEBUG) */
			mr.		r21,r21							/* (TEST/DEBUG) */
			bne-	mpwait5							/* (TEST/DEBUG) */
			stwcx.	r20,0,r20						/* (TEST/DEBUG) */
			bne-	mpwait5							/* (TEST/DEBUG) */
			
			stw		r0,0x280(br0)					/* (TEST/DEBUG) */
			stmw	r1,0x284(br0)					/* (TEST/DEBUG) */
			bne		cr7,nono9						/* (TEST/DEBUG) */
			lwz		r0,0(br0)						/* (TEST/DEBUG) */
			mr.		r0,r0							/* (TEST/DEBUG) */
			beq+	nono9							/* (TEST/DEBUG) */
			mr		r30,r4							/* (TEST/DEBUG) */
			mr		r24,r5							/* (TEST/DEBUG) */
			
			bne		cr6,nopir11						/* (TEST/DEBUG) */
			mfspr	r4,pir							/* (TEST/DEBUG) */
			b		gotpir11						/* (TEST/DEBUG) */
nopir11:	li		r4,0							/* (TEST/DEBUG) */
gotpir11:											/* (TEST/DEBUG) */
			lis		r31,HIGH_ADDR(EXT(dbgDispLL))	/* (TEST/DEBUG) */
			rlwinm	r4,r4,24,4,7					/* (TEST/DEBUG) */
			li		r3,0							/* (TEST/DEBUG) */
			mr		r5,r6							/* (TEST/DEBUG) */
			oris	r4,r4,0x3030					/* (TEST/DEBUG) */
			ori		r31,r31,LOW_ADDR(EXT(dbgDispLL))	/* (TEST/DEBUG) */
			ori		r4,r4,0x3930					/* (TEST/DEBUG) */
			mtlr	r31								/* (TEST/DEBUG) */
			blrl									/* (TEST/DEBUG) */
			
			bne		cr6,nopir12						/* (TEST/DEBUG) */
			mfspr	r4,pir							/* (TEST/DEBUG) */
			b		gotpir12						/* (TEST/DEBUG) */
nopir12:	li		r4,0							/* (TEST/DEBUG) */
gotpir12:											/* (TEST/DEBUG) */
			lis		r31,HIGH_ADDR(EXT(dbgDispLL))	/* (TEST/DEBUG) */
			rlwinm	r4,r4,24,4,7					/* (TEST/DEBUG) */
			li		r3,0							/* (TEST/DEBUG) */
			mr		r5,r30							/* (TEST/DEBUG) */
			oris	r4,r4,0x3031					/* (TEST/DEBUG) */
			ori		r31,r31,LOW_ADDR(EXT(dbgDispLL))	/* (TEST/DEBUG) */
			ori		r4,r4,0x3130					/* (TEST/DEBUG) */
			mtlr	r31								/* (TEST/DEBUG) */
			blrl									/* (TEST/DEBUG) */
		
			bne		cr6,nopir13						/* (TEST/DEBUG) */
			mfspr	r4,pir							/* (TEST/DEBUG) */
			b		gotpir13						/* (TEST/DEBUG) */
nopir13:	li		r4,0							/* (TEST/DEBUG) */
gotpir13:											/* (TEST/DEBUG) */
			lis		r31,HIGH_ADDR(EXT(dbgDispLL))	/* (TEST/DEBUG) */
			rlwinm	r4,r4,24,4,7					/* (TEST/DEBUG) */
			li		r3,0							/* (TEST/DEBUG) */
			xor		r5,r30,r24						/* (TEST/DEBUG) */
			oris	r4,r4,0x3031					/* (TEST/DEBUG) */
			ori		r31,r31,LOW_ADDR(EXT(dbgDispLL))	/* (TEST/DEBUG) */
			ori		r4,r4,0x3131					/* (TEST/DEBUG) */
			mtlr	r31								/* (TEST/DEBUG) */
			blrl									/* (TEST/DEBUG) */
			
			bne		cr6,nopir14						/* (TEST/DEBUG) */
			mfspr	r4,pir							/* (TEST/DEBUG) */
			b		gotpir14						/* (TEST/DEBUG) */
nopir14:	li		r4,0							/* (TEST/DEBUG) */
gotpir14:											/* (TEST/DEBUG) */
			lis		r31,HIGH_ADDR(EXT(dbgDispLL))	/* (TEST/DEBUG) */
			rlwinm	r4,r4,24,4,7					/* (TEST/DEBUG) */
			li		r3,0							/* (TEST/DEBUG) */
			mr		r5,r25							/* (TEST/DEBUG) */
			oris	r4,r4,0x3031					/* (TEST/DEBUG) */
			ori		r31,r31,LOW_ADDR(EXT(dbgDispLL))	/* (TEST/DEBUG) */
			ori		r4,r4,0x3132					/* (TEST/DEBUG) */
			mtlr	r31								/* (TEST/DEBUG) */
			blrl									/* (TEST/DEBUG) */
			
			bne		cr6,nopir15						/* (TEST/DEBUG) */
			mfspr	r4,pir							/* (TEST/DEBUG) */
			b		gotpir15						/* (TEST/DEBUG) */
nopir15:	li		r4,0							/* (TEST/DEBUG) */
gotpir15:											/* (TEST/DEBUG) */
			lis		r31,HIGH_ADDR(EXT(dbgDispLL))	/* (TEST/DEBUG) */
			rlwinm	r4,r4,24,4,7					/* (TEST/DEBUG) */
			li		r3,0							/* (TEST/DEBUG) */
			mfsrr0	r5								/* (TEST/DEBUG) */
			oris	r4,r4,0x3031					/* (TEST/DEBUG) */
			ori		r31,r31,LOW_ADDR(EXT(dbgDispLL))	/* (TEST/DEBUG) */
			ori		r4,r4,0x3133					/* (TEST/DEBUG) */
			mtlr	r31								/* (TEST/DEBUG) */
			blrl									/* (TEST/DEBUG) */
nono9:
			lmw		r1,0x284(br0)					/* (TEST/DEBUG) */
			lwz		r0,0x280(br0)					/* (TEST/DEBUG) */
			li		r20,0							/* (TEST/DEBUG) */
			stw		r20,0x20(br0)					/* (TEST/DEBUG) */
			mtdec	r25								/* (TEST/DEBUG) */
#endif

			rfi										/* Launch the exception handler */

			.long	0								/* Leave these here gol durn it! */
			.long	0
			.long	0
			.long	0
			.long	0
			.long	0
			.long	0
			.long	0

/*
 *			This routine is the only place where we return from an interruption.
 *			Anyplace else is wrong.  Even if I write the code, it's still wrong
 *			Feel free to come by and slap me if I do do it. Even though I may
 *			have had a good reason to do it.
 *
 *			All we need to remember here is that R13 must point to the savearea
 *			that has the context we need to load up. Translation and interruptions
 *			must be disabled.
 *
 *			This code always loads the context in the savearea pointed to
 *			by R13.  In the process, it throws away the savearea.  If there 
 *			is any tomfoolery with savearea stacks, it must be taken care of 
 *			before we get here.
 */
 
EatRupt:	mr		r31,r13							/* Move the savearea pointer to the far end of the register set */

EatRupt2:	mfsprg	r2,0							/* Get the per_proc block */
	
/*
 *			First we see if we are able to free the new savearea.
 *			If it is not attached to anything, put it on the free list.
 *			This is real dangerous, we haven't restored context yet...
 *			So, the free savearea chain lock must stay until the bitter end!
 */
			
#if RUPTTRC
			lis		r18,0x7FFF						/* (TEST/DEBUG) */
			mfdec	r25								/* (TEST/DEBUG) */
			or		r18,r18,r25						/* (TEST/DEBUG) */
			mtdec	r18								/* (TEST/DEBUG) */
			li		r18,0x20						/* (TEST/DEBUG) */
		
			lwarx	r19,0,r18						; ?

mpwait6:	lwarx	r19,0,r18						/* (TEST/DEBUG) */
			mr.		r19,r19							/* (TEST/DEBUG) */
			bne-	mpwait6							/* (TEST/DEBUG) */
			stwcx.	r18,0,r18						/* (TEST/DEBUG) */
			bne-	mpwait6							/* (TEST/DEBUG) */
			
			stw		r0,0x280(br0)					/* (TEST/DEBUG) */
			stmw	r1,0x284(br0)					/* (TEST/DEBUG) */
			bne		cr7,nono5						/* (TEST/DEBUG) */
			lwz		r0,0(br0)						/* (TEST/DEBUG) */
			mr.		r0,r0							/* (TEST/DEBUG) */
			beq+	nono5							/* (TEST/DEBUG) */

			bne		cr6,nopir16						/* (TEST/DEBUG) */
			mfspr	r4,pir							/* (TEST/DEBUG) */
			b		gotpir16						/* (TEST/DEBUG) */
nopir16:	li		r4,0							/* (TEST/DEBUG) */
gotpir16:											/* (TEST/DEBUG) */
			rlwinm	r24,r31,0,0,19					/* (TEST/DEBUG) */
			mr		r5,r31							/* (TEST/DEBUG) */
			mr		r30,r31							/* (TEST/DEBUG) */
			lwz		r24,SACvrswap(r24)				/* (TEST/DEBUG) */
			lis		r31,HIGH_ADDR(EXT(dbgDispLL))	/* (TEST/DEBUG) */
			rlwinm	r4,r4,24,4,7					/* (TEST/DEBUG) */
			li		r3,0							/* (TEST/DEBUG) */
			oris	r4,r4,0x3030					/* (TEST/DEBUG) */
			ori		r31,r31,LOW_ADDR(EXT(dbgDispLL))	/* (TEST/DEBUG) */
			ori		r4,r4,0x3730					/* (TEST/DEBUG) */
			mtlr	r31								/* (TEST/DEBUG) */
			blrl									/* (TEST/DEBUG) */

		
			bne		cr6,nopir17						/* (TEST/DEBUG) */
			mfspr	r4,pir							/* (TEST/DEBUG) */
			b		gotpir17						/* (TEST/DEBUG) */
nopir17:	li		r4,0							/* (TEST/DEBUG) */
gotpir17:											/* (TEST/DEBUG) */
			lis		r31,HIGH_ADDR(EXT(dbgDispLL))	/* (TEST/DEBUG) */
			rlwinm	r4,r4,24,4,7					/* (TEST/DEBUG) */
			li		r3,0							/* (TEST/DEBUG) */
			xor		r5,r30,r24						/* (TEST/DEBUG) */
			oris	r4,r4,0x3030					/* (TEST/DEBUG) */
			ori		r31,r31,LOW_ADDR(EXT(dbgDispLL))	/* (TEST/DEBUG) */
			ori		r4,r4,0x3731					/* (TEST/DEBUG) */
			mtlr	r31								/* (TEST/DEBUG) */
			blrl									/* (TEST/DEBUG) */
			
			bne		cr6,nopir18						/* (TEST/DEBUG) */
			mfspr	r4,pir							/* (TEST/DEBUG) */
			b		gotpir18						/* (TEST/DEBUG) */
nopir18:	li		r4,0							/* (TEST/DEBUG) */
gotpir18:											/* (TEST/DEBUG) */
			lis		r31,HIGH_ADDR(EXT(dbgDispLL))	/* (TEST/DEBUG) */
			rlwinm	r4,r4,24,4,7					/* (TEST/DEBUG) */
			li		r3,0							/* (TEST/DEBUG) */
			mfsprg	r5,1							/* (TEST/DEBUG) */
			oris	r4,r4,0x3030					/* (TEST/DEBUG) */
			ori		r31,r31,LOW_ADDR(EXT(dbgDispLL))	/* (TEST/DEBUG) */
			ori		r4,r4,0x3830					/* (TEST/DEBUG) */
			mtlr	r31								/* (TEST/DEBUG) */
			blrl									/* (TEST/DEBUG) */

			bne		cr6,nopir19						/* (TEST/DEBUG) */
			mfspr	r4,pir							/* (TEST/DEBUG) */
			b		gotpir19						/* (TEST/DEBUG) */
nopir19:	li		r4,0							/* (TEST/DEBUG) */
gotpir19:											/* (TEST/DEBUG) */
			lis		r31,HIGH_ADDR(EXT(dbgDispLL))	/* (TEST/DEBUG) */
			rlwinm	r4,r4,24,4,7					/* (TEST/DEBUG) */
			li		r3,0							/* (TEST/DEBUG) */
			lwz		r5,savesrr0(r30)				/* (TEST/DEBUG) */
			oris	r4,r4,0x3031					/* (TEST/DEBUG) */
			ori		r31,r31,LOW_ADDR(EXT(dbgDispLL))	/* (TEST/DEBUG) */
			ori		r4,r4,0x3130					/* (TEST/DEBUG) */
			mtlr	r31								/* (TEST/DEBUG) */
			blrl									/* (TEST/DEBUG) */

			bne		cr6,nopir20						/* (TEST/DEBUG) */
			mfspr	r4,pir							/* (TEST/DEBUG) */
			b		gotpir20						/* (TEST/DEBUG) */
nopir20:	li		r4,0							/* (TEST/DEBUG) */
gotpir20:											/* (TEST/DEBUG) */
			lis		r31,HIGH_ADDR(EXT(dbgDispLL))	/* (TEST/DEBUG) */
			rlwinm	r4,r4,24,4,7					/* (TEST/DEBUG) */
			li		r3,0							/* (TEST/DEBUG) */
			lwz		r5,savesrr1(r30)				/* (TEST/DEBUG) */
			oris	r4,r4,0x3031					/* (TEST/DEBUG) */
			ori		r31,r31,LOW_ADDR(EXT(dbgDispLL))	/* (TEST/DEBUG) */
			ori		r4,r4,0x3131					/* (TEST/DEBUG) */
			mtlr	r31								/* (TEST/DEBUG) */
			blrl									/* (TEST/DEBUG) */

			bne		cr6,nopir21						/* (TEST/DEBUG) */
			mfspr	r4,pir							/* (TEST/DEBUG) */
			b		gotpir21						/* (TEST/DEBUG) */
nopir21:	li		r4,0							/* (TEST/DEBUG) */
gotpir21:											/* (TEST/DEBUG) */
			lis		r31,HIGH_ADDR(EXT(dbgDispLL))	/* (TEST/DEBUG) */
			rlwinm	r4,r4,24,4,7					/* (TEST/DEBUG) */
			li		r3,0							/* (TEST/DEBUG) */
			lwz		r5,savelr(r30)					/* (TEST/DEBUG) */
			oris	r4,r4,0x3031					/* (TEST/DEBUG) */
			ori		r31,r31,LOW_ADDR(EXT(dbgDispLL))	/* (TEST/DEBUG) */
			ori		r4,r4,0x3132					/* (TEST/DEBUG) */
			mtlr	r31								/* (TEST/DEBUG) */
			blrl									/* (TEST/DEBUG) */
nono5:
			lmw		r1,0x284(br0)					/* (TEST/DEBUG) */
			lwz		r0,0x280(br0)					/* (TEST/DEBUG) */
			li		r19,0							/* (TEST/DEBUG) */
			stw		r19,0x20(br0)					/* (TEST/DEBUG) */
			mtdec	r25								/* (TEST/DEBUG) */
			
#endif


/*
 *			It's dangerous here.  We haven't restored anything from the current savearea yet.
 *			And, we mark it the active one.  So, if we get an exception in here, it is
 *			unrecoverable.  Unless we mess up, we can't get any kind of exception.  So,
 *			it is important to assay this code as only the purest of gold.
 *
 *			But first, see if there is a savearea hanging off of quickfret.  If so, 
 *			we release that one first and then come back for the other.  We should rarely
 *			see one, they appear when FPU or VMX context is discarded by either returning
 *			to a higher exception level, or explicitly.
 *
 *			A word about QUICKFRET: Multiple saveareas may be queued for release.  It is
 *			the responsibility of the queuer to insure that the savearea is not multiply
 *			queued and that the appropriate inuse bits are reset.
 */

#define TRCSAVE 0

#if TRCSAVE
			lwz		r30,saver0(r31)					; (TEST/DEBUG) Get users R0
			lwz		r20,saveexception(r31)			; (TEST/DEBUG) Returning from trace?
			xor		r30,r20,r30						; (TEST/DEBUG) Make code
			rlwinm	r30,r30,1,0,31					; (TEST/DEBUG) Make an easy test
			cmplwi	cr5,r30,0x61					; (TEST/DEBUG) See if this is a trace
#endif
 
			mr		r18,r31							/* Save the savearea pointer */
			lwz		r19,PP_QUICKFRET(r2)			/* Get the quick release savearea */

			la		r20,savesrr0(r18)				/* Point to the first thing we look at */
			li		r0,0							/* Get a zero */
			dcbt	0,r20							/* Touch in the first thing in the real savearea */
			la		r21,savesr0(r18)				/* Point to the first thing we restore */
			lis		r30,HIGH_ADDR(EXT(saveanchor))	/* Get the high part of the anchor */
			stw		r0,PP_QUICKFRET(r2)				/* Clear quickfret pointer */
			ori		r30,r30,LOW_ADDR(EXT(saveanchor))	/* Bottom half of the anchor */
			dcbt	0,r21							/* Touch in the first thing */

#if TRCSAVE
			beq-	cr5,trkill0						; (TEST/DEBUG) Do not trace this type
			lwz		r14,LOW_ADDR(traceMask-EXT(ExceptionVectorsStart))(br0)	; (TEST/DEBUG) Get the trace mask
			mr.		r14,r14							; (TEST/DEBUG) Is it stopped?
			beq-	trkill0							; (TEST/DEBUG) yes...
			bl		cte								; (TEST/DEBUG) Trace this
			stw		r18,LTR_r1(r20)					; (TEST/DEBUG) Normal savearea
			stw		r19,LTR_r2(r20)					; (TEST/DEBUG) Quickfret savearea
trkill0:
#endif

			lwarx	r22,0,r30						; ?

rtlck:		lwarx	r22,0,r30						/* Grab the lock value */
			li		r23,1							/* Use part of the delay time */
			mr.		r22,r22							/* Is it locked? */
			bne-	rtlcks							/* Yeah, wait for it to clear... */
			stwcx.	r23,0,r30						/* Try to seize that there durn lock */
			beq+	fretagain						; Got it...
			b		rtlck							/* Collision, try again... */
			
rtlcks:		lwz		r22,SVlock(r30)					/* Get that lock in here */
			mr.		r22,r22							/* Is it free yet? */
			beq+	rtlck							/* Yeah, try for it again... */
			b		rtlcks							/* Sniff away... */

;
;			Lock gotten, toss the saveareas
;
fretagain:	
#if TRCSAVE
			beq-	cr5,trkill1						; (TEST/DEBUG) Do not trace this type
			lwz		r14,LOW_ADDR(traceMask-EXT(ExceptionVectorsStart))(br0)	; (TEST/DEBUG) Get the trace mask
			mr.		r14,r14							; (TEST/DEBUG) Is it stopped?
			beq-	trkill1							; (TEST/DEBUG) yes...
			li		r0,1							; (TEST/DEBUG) ID number
			bl		cte								; (TEST/DEBUG) Trace this
			stw		r18,LTR_r1(r20)					; (TEST/DEBUG) Normal savearea
			stw		r19,LTR_r2(r20)					; (TEST/DEBUG) Quickfret savearea
trkill1:
#endif
			
			mr.		r18,r18							; Are we actually done here?
			beq-	donefret						; Yeah...
			mr.		r31,r19							; Is there a quickfret to do?
			beq+	noqfrt							; Nope...
			lwz		r19,SAVqfret(r19)				; Yes, get the next in line
#if TRCSAVE
			beq-	cr5,trkill2						; (TEST/DEBUG) Do not trace this type
			lwz		r14,LOW_ADDR(traceMask-EXT(ExceptionVectorsStart))(br0)	; (TEST/DEBUG) Get the trace mask
			mr.		r14,r14							; (TEST/DEBUG) Is it stopped?
			beq-	trkill2							; (TEST/DEBUG) yes...
			li		r0,2							; (TEST/DEBUG) ID number
			bl		cte								; (TEST/DEBUG) Trace this
			stw		r18,LTR_r1(r20)					; (TEST/DEBUG) Normal savearea
			stw		r19,LTR_r2(r20)					; (TEST/DEBUG) next quickfret savearea
			stw		r31,LTR_r3(r20)					; (TEST/DEBUG) Current one to toss
trkill2:
#endif
			b		doqfrt							; Go do it...

noqfrt:		mr		r31,r18							; Set the area to release
			li		r18,0							; Show we have done it
#if TRCSAVE
			beq-	cr5,trkill3						; (TEST/DEBUG) Do not trace this type
			lwz		r14,LOW_ADDR(traceMask-EXT(ExceptionVectorsStart))(br0)	; (TEST/DEBUG) Get the trace mask
			mr.		r14,r14							; (TEST/DEBUG) Is it stopped?
			beq-	trkill3							; (TEST/DEBUG) yes...
			li		r0,3							; (TEST/DEBUG) ID number
			bl		cte								; (TEST/DEBUG) Trace this
			stw		r18,LTR_r1(r20)					; (TEST/DEBUG) Normal savearea
			stw		r19,LTR_r2(r20)					; (TEST/DEBUG) next quickfret savearea
			stw		r31,LTR_r3(r20)					; (TEST/DEBUG) Current one to toss
trkill3:
#endif

doqfrt:		li		r0,0							; Get a constant 0
			lis		r26,0x8000						/* Build a bit mask and assume first savearea */
			stw		r0,SAVqfret(r31)				; Make sure back chain is unlinked
			lwz		r28,SAVflags(r31)				; Get the flags for the old active one
#if TRCSAVE
			beq-	cr5,trkill4						; (TEST/DEBUG) Do not trace this type
			lwz		r14,LOW_ADDR(traceMask-EXT(ExceptionVectorsStart))(br0)	; (TEST/DEBUG) Get the trace mask
			mr.		r14,r14							; (TEST/DEBUG) Is it stopped?
			beq-	trkill4							; (TEST/DEBUG) yes...
			li		r0,4							; (TEST/DEBUG) ID number
			bl		cte								; (TEST/DEBUG) Trace this
			stw		r18,LTR_r1(r20)					; (TEST/DEBUG) Normal savearea
			stw		r19,LTR_r2(r20)					; (TEST/DEBUG) next quickfret savearea
			stw		r31,LTR_r3(r20)					; (TEST/DEBUG) Current one to toss
			stw		r28,LTR_r4(r20)					; (TEST/DEBUG) Save current flags
trkill4:
#endif			
			rlwinm	r25,r31,21,31,31				/* Get position of savearea in block */
			andis.	r28,r28,HIGH_ADDR(SAVinuse)		/* See if we need to free it */
			srw		r26,r26,r25						/* Get bit position to deallocate */
			rlwinm	r29,r31,0,0,19					/* Round savearea pointer to even page address */
					
			bne-	fretagain						/* Still in use, we can't free this one... */

			lwz		r23,SACalloc(r29)				/* Get the allocation for this block */
			lwz		r24,SVinuse(r30)				/* Get the in use count */
			mr		r28,r23							; (TEST/DEBUG) save for trace
			or		r23,r23,r26						/* Turn on our bit */
			subi	r24,r24,1						/* Show that this one is free */
			cmplw	r23,r26							/* Is our's the only one free? */
			stw		r23,SACalloc(r29)				/* Save it out */
			bne+	rstrest							/* Nope, then the block is already on the free list */

			lwz		r22,SVfree(r30)					/* Get the old head of the free list */
			stw		r29,SVfree(r30)					/* Point the head at us now */
			stw		r22,SACnext(r29)				; Point us to the old last
	
rstrest:	stw		r24,SVinuse(r30)				/* Set the in use count */
#if TRCSAVE
			beq-	cr5,trkill5						; (TEST/DEBUG) Do not trace this type
			lwz		r14,LOW_ADDR(traceMask-EXT(ExceptionVectorsStart))(br0)	; (TEST/DEBUG) Get the trace mask
			mr.		r14,r14							; (TEST/DEBUG) Is it stopped?
			beq-	trkill5							; (TEST/DEBUG) yes...
			li		r0,5							; (TEST/DEBUG) ID number
			bl		cte								; (TEST/DEBUG) Trace this
			stw		r18,LTR_r1(r20)					; (TEST/DEBUG) Normal savearea
			stw		r19,LTR_r2(r20)					; (TEST/DEBUG) Next quickfret savearea
			stw		r31,LTR_r3(r20)					; (TEST/DEBUG) Current one to toss
			stw		r28,LTR_srr1(r20)				; (TEST/DEBUG) Save the original allocation
			stw		r23,LTR_dar(r20)				; (TEST/DEBUG) Save the new allocation
			stw		r24,LTR_save(r20)				; (TEST/DEBUG) Save the new in use count
			stw		r22,LTR_lr(r20)					; (TEST/DEBUG) Save the old top of free list
			stw		r29,LTR_ctr(r20)				; (TEST/DEBUG) Save the new top of free list
trkill5:
#endif			
			b		fretagain						; Go finish up the rest...

;
;			Build the SR values depending upon destination.  If we are going to the kernel,
;			the SRs are almost all the way set up. SR14 (or the currently used copyin/out register)
;			must be set to whatever it was at the last exception because it varies.  All the rest
;			have been set up already.
;
;			If we are going into user space, we need to check a bit more. SR0, SR1, SR2, and
;			SR14 (current implementation) must be restored always.  The others must be set if
;			they are different that what was loaded last time (i.e., tasks have switched).  
;			We check the last loaded address space ID and if the same, we skip the loads.  
;			This is a performance gain because SR manipulations are slow.
;

donefret:	lwz		r26,savesrr1(r31)				; Get destination state flags
			lwz		r15,PP_USERSPACE(r2)			; Pick up the user space ID we may launch
			rlwinm.	r17,r26,0,MSR_PR_BIT,MSR_PR_BIT	; See if we are going to user or system
			lwz		r16,PP_LASTSPACE(r2)			; Pick up the last loaded SR value
			oris	r13,r15,hi16(SEG_REG_PROT)		; Get the protection bits correct
			
			cmplw	cr3,r15,r15						; Set that we do not need to stop streams
			addis	r14,r13,0x0010					; Generate next SR value
			beq-	gotokern						; We are going into kernel state, SRs all set up...
			
			mtsr	sr0,r13							; Set SR0
			addis	r13,r14,0x0010					; Generate next SR value
			cmplw	cr3,r15,r16						; See if most of the SRs are already loaded
			mtsr	sr1,r14							; Set SR1
			addis	r14,r13,0x0010					; Generate next SR value
			mtsr	sr2,r13							; Set SR2
			addis	r13,r14,0x0010					; Generate next SR value
			mtsr	sr3,r14							; Set SR3
			
			beq+	cr3,noloadsr					; SRs have not changed, no reload...

			addis	r14,r13,0x0010					; Generate next SR value
			mtsr	sr4,r13							; Set SR4
			addis	r13,r14,0x0010					; Generate next SR value
			mtsr	sr5,r14							; Set SR5
			addis	r14,r13,0x0010					; Generate next SR value
			mtsr	sr6,r13							; Set SR6
			addis	r13,r14,0x0010					; Generate next SR value
			mtsr	sr7,r14							; Set SR7
			addis	r14,r13,0x0010					; Generate next SR value
			mtsr	sr8,r13							; Set SR8
			addis	r13,r14,0x0010					; Generate next SR value
			mtsr	sr9,r14							; Set SR9
			addis	r14,r13,0x0010					; Generate next SR value
			mtsr	sr10,r13						; Set SR10
			addis	r13,r14,0x0010					; Generate next SR value
			mtsr	sr11,r14						; Set SR11
			addis	r14,r13,0x0010					; Generate next SR value
			mtsr	sr12,r13						; Set SR12
			addis	r13,r14,0x0010					; Generate next SR value
			mtsr	sr13,r14						; Set SR13
			addis	r14,r13,0x0010					; Generate next SR value
			mtsr	sr14,r13						; Set SR14
			mtsr	sr15,r14						; Set SR15
			stw		r15,PP_LASTSPACE(r2)			; Set the last loaded SR value
			b		ngotokern						; All done with user state SRs...

noloadsr:	oris	r14,r15,hi16(SEG_REG_PROT|0x00E00000)	; Build SR14 (copyin/out) value
			b		loadsr14						; All done with user state SRs (except for SR14)...

gotokern:	lwz		r14,savesr14(r31)				; Get the copyin/out register at interrupt time

loadsr14:	mtsr	sr14,r14						; Set SR14
			
ngotokern:	lwz		r25,savesrr0(r31)				/* Get the SRR0 to use */
			la		r28,saver6(r31)					/* Point to the next line to use */
			lwz		r0,saver0(r31)					/* Restore */			
			dcbt	0,r28							/* Touch it in */
			lwz		r1,saver1(r31)					/* Restore */	
			lwz		r2,saver2(r31)					/* Restore */	
			la		r28,saver14(r31)				/* Point to the next line to get */
			lwz		r3,saver3(r31)					/* Restore */
			mtsrr0	r25								/* Restore the SRR0 now */
			lwz		r4,saver4(r31)					/* Restore */
			lwz		r5,saver5(r31)					/* Restore */
			mtsrr1	r26								/* Restore the SRR1 now */
			lwz		r6,saver6(r31)					/* Restore */			
			
			dcbt	0,r28							/* Touch that next line on in */
			
			lwz		r7,saver7(r31)					/* Restore */			
			lwz		r8,saver8(r31)					/* Restore */			
			lwz		r9,saver9(r31)					/* Restore */			
			lwz		r10,saver10(r31)				/* Restore */			
			lwz		r11,saver11(r31)				/* Restore */			
			lwz		r12,saver12(r31)				/* Restore */			
			lwz		r13,saver13(r31)				/* Restore */			
			la		r28,saver22(r31)				/* Point to the next line to do */
			lwz		r14,saver14(r31)				/* Restore */	
			lwz		r15,saver15(r31)				/* Restore */			
			
			dcbt	0,r28							/* Touch in another line of context */
			
			lwz		r16,saver16(r31)				/* Restore */
			lwz		r17,saver17(r31)				/* Restore */
			lwz		r18,saver18(r31)				/* Restore */	
			lwz		r19,saver19(r31)				/* Restore */	
			lwz		r20,saver20(r31)				/* Restore */	
			lwz		r21,saver21(r31)				/* Restore */
			la		r28,saver30(r31)				/* Point to the final line */
			lwz		r22,saver22(r31)				/* Restore */

			dcbt	0,r28							/* Suck it in */

			lwz		r23,saver23(r31)				/* Restore */
			mfpvr	r27								; Get the processor version
			lwz		r24,saver24(r31)				/* Restore */			
			rlwinm	r27,r27,16,16,31				; Get the processor type
			lwz		r25,saver25(r31)				/* Restore */			
			cmplwi	cr1,r27,PROCESSOR_VERSION_Max	; Do we have Altivec? */			
			lwz		r26,saver26(r31)				/* Restore */			
			lwz		r27,saver27(r31)				/* Restore */			

			dcbt	0,r28							/* Get the final line */
			lwz		r28,savecr(r31)					/* Get CR to restore */
			blt		cr1,noavec4						; No vector on this machine
			lwz		r29,savevrsave(r31)				; Get the vrsave
			beq+	cr3,noavec3						; SRs have not changed, no need to stop the streams...
			dssall									; Kill all data streams
													; The streams should be suspended
													; already, and we do a bunch of 
													; dependent loads and a sync later
													; so we should be cool.
		
noavec3:	mtspr	vrsave,r29						; Set the vrsave

noavec4:	lwz		r29,savexer(r31)				/* Get XER to restore */
			mtcr	r28								/* Restore the CR */
			lwz		r28,savelr(r31)					/* Get LR to restore */
			mtxer	r29								/* Restore the XER */
			lwz		r29,savectr(r31)				/* Get the CTR to restore */
			mtlr	r28								/* Restore the LR */
			lwz		r28,saver30(r31)				/* Restore */
			mtctr	r29								/* Restore the CTR */
			lwz		r29,saver31(r31)				/* Restore */
			mtsprg	2,r28							/* Save R30 */
			lwz		r28,saver28(r31)				/* Restore */			
			mtsprg	3,r29							/* Save R31 */
			lwz		r29,saver29(r31)				/* Restore */

#if PERFTIMES && DEBUG
			stmw	r1,0x280(br0)					; Save all registers
			mfcr	r20								; Save the CR
			mflr	r21								; Save the LR
			mfsrr0	r9								; Save SRR0
			mfsrr1	r11								; Save SRR1
			mr		r8,r0							; Save R0
			li		r3,69							; Indicate interrupt
			mr		r4,r11							; Set MSR to log
			mr		r5,r31							; Get savearea to log
			bl		EXT(dbgLog2)					; Cut log entry
			mr		r0,r8							; Restore R0
			mtsrr0	r9								; Restore SRR0
			mtsrr1	r11								; Restore SRR1
			mtlr	r21								; Restore the LR
			mtcr	r20								; Restore the CR
			lmw		r1,0x280(br0)					; Restore all the rest
#endif

			li		r31,0							/* Get set to clear lock */
			sync									/* Make sure it's all out there */
			stw		r31,SVlock(r30)					/* Unlock it */
			mfsprg	r30,2							/* Restore R30 */
			mfsprg	r31,3							/* Restore R31 */

			rfi										/* Click heels three times and think very hard that there's no place like home */

			.long	0								/* For old 601 bug */
			.long	0
			.long	0
			.long	0
			.long	0
			.long	0
			.long	0
			.long	0



	
/*
 * exception_exit(savearea *)
 *
 *
 * ENTRY :	IR and/or DR and/or interruptions can be on
 *			R3 points to the physical address of a savearea
 */
	
			.align	5
			.globl	EXT(exception_exit)
			
			nop										; Align ISYNC to last line in cache
			nop

LEXT(exception_exit)

			mfmsr	r30								/* Get the current MSR */
			mr		r31,r3							/* Get the savearea in the right register */
			andi.	r30,r30,0x7FCF					/* Turn off externals, IR, and DR */
			lis		r1,hi16(SAVredrive)				; Get redrive request
			mtmsr	r30								/* Translation and all off */
			isync									/* Toss prefetch */

			mfsprg	r2,0							; Get the per_proc block
			lwz		r4,SAVflags(r3)					; Pick up the flags
			mr		r13,r3							; Put savearea here also

			and.	r0,r4,r1						; Check if redrive requested
			andc	r4,r4,r1						; Clear redrive
			
			dcbt	br0,r2							; We will need this in just a sec

			beq+	EatRupt							; No redrive, just exit...

			lwz		r3,saveexception(r13)			; Restore exception code
			stw		r4,SAVflags(r13)				; Set the flags
			b		Redrive							; Redrive the exception...
		
;
;			Make trace entry for lowmem_vectors internal debug
;
#if TRCSAVE
cte:
			lwz		r20,LOW_ADDR(EXT(traceCurr)-EXT(ExceptionVectorsStart))(br0)	; Pick up the current trace entry
			lwz		r16,LOW_ADDR(EXT(traceEnd)-EXT(ExceptionVectorsStart))(br0)	; Grab up the end of it all
			addi	r17,r20,LTR_size				; Point to the next trace entry
			cmplw	r17,r16							; Do we need to wrap the trace table?
			li		r15,32							; Second line of entry
			bne+	ctenwrap						; We got a trace entry...			
			lwz		r17,LOW_ADDR(EXT(traceStart)-EXT(ExceptionVectorsStart))(br0)	; Wrap back to the top
	
ctenwrap:	stw		r17,LOW_ADDR(EXT(traceCurr)-EXT(ExceptionVectorsStart))(br0)	; Set the next entry for the next guy		
			
			dcbz	0,r20							; Allocate cache for the entry
			dcbz	r15,r20							; Zap the second half

ctegetTB:	mftbu	r16								; Get the upper timebase
			mftb	r17								; Get the lower timebase
			mftbu	r15								; Get the upper one again
			cmplw	r16,r15							; Did the top tick?
			bne-	ctegetTB						; Yeah, need to get it again...
			
			li		r15,0x111						; Get the special trace ID code
			stw		r0,LTR_r0(r20)					; Save R0 (usually used as an ID number
			stw		r16,LTR_timeHi(r20)				; Set the upper part of TB
			mflr	r16								; Get the return point
			stw		r17,LTR_timeLo(r20)				; Set the lower part of TB
			sth		r15,LTR_excpt(r20)				; Save the exception type
			stw		r16,LTR_srr0(r20)				; Save the return point
			blr										; Leave...
#endif

/*
 *		Start of the trace table
 */
 
 			.align	12						/* Align to 4k boundary */
	
traceTableBeg:						/* Start of trace table */
/*			.fill	2048,4,0		   Make an 8k trace table for now */
			.fill	13760,4,0		/* Make an .trace table for now */
/*			.fill	240000,4,0		   Make an .trace table for now */
traceTableEnd:						/* End of trace table */
	
			.globl EXT(ExceptionVectorsEnd)
EXT(ExceptionVectorsEnd):	/* Used if relocating the exception vectors */
#ifndef HACKALERTHACKALERT
/* 
 *		This .long needs to be here because the linker gets confused and tries to 
 *		include the final label in a section in the next section if there is nothing 
 *		after it
 */
	.long	0						/* (HACK/HACK/HACK) */
#endif

	.data
	.align	ALIGN
	.globl	EXT(exception_end)
EXT(exception_end):
	.long	EXT(ExceptionVectorsEnd) -EXT(ExceptionVectorsStart) /* phys fn */


