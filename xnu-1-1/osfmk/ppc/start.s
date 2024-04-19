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
#include <cpus.h>
#include <mach_kdb.h>
#include <mach_kdp.h>
#include <mach_kgdb.h>

#include <ppc/asm.h>
#include <ppc/proc_reg.h>
#include <mach/ppc/vm_param.h>
#include <assym.s>
	
/*
 * Interrupt and bootup stack for initial processor
 */

	.file	"start.s"
	
	.data

		/* Align on page boundry */
	.align  PPC_PGSHIFT
		/* Red zone for interrupt stack, one page (will be unmapped)*/
	.set	., .+PPC_PGBYTES
		/* intstack itself */

     .globl  EXT(FixedStackStart)
EXT(FixedStackStart):
     
	 .globl  EXT(intstack)
EXT(intstack):
	.set	., .+INTSTACK_SIZE*NCPUS
	
	/* Debugger stack - used by the debugger if present */
	/* NOTE!!! Keep the debugger stack right after the interrupt stack */

#if MACH_KDP || MACH_KDB
    .globl  EXT(debstack)
EXT(debstack):
	.set	., .+KERNEL_STACK_SIZE*NCPUS
     
	 .globl  EXT(FixedStackEnd)
EXT(FixedStackEnd):

	.align	ALIGN
    .globl  EXT(intstack_top_ss)
EXT(intstack_top_ss):
	.long	EXT(intstack)+INTSTACK_SIZE-SS_SIZE			/* intstack_top_ss points to the top of interrupt stack */

	.align	ALIGN
    .globl  EXT(debstack_top_ss)	
EXT(debstack_top_ss):

	.long	EXT(debstack)+KERNEL_STACK_SIZE-SS_SIZE		/* debstack_top_ss points to the top of debug stack */

    .globl  EXT(debstackptr)
EXT(debstackptr):	
	.long	EXT(debstack)+KERNEL_STACK_SIZE-SS_SIZE

#endif /* MACH_KDP || MACH_KDB */

/*
 * All CPUs start here.
 *
 * This code is called from SecondaryLoader
 *
 * Various arguments are passed via a table:
 *   ARG0 = pointer to other startup parameters
 */
	.text
	
ENTRY(_start_cpu,TAG_NO_FRAME_USED)
			li		r30,1
			b		allstart
			
ENTRY(_start,TAG_NO_FRAME_USED)
			li		r30,0
allstart:
			mr		r31,r3						/* Save away arguments */

			li		r7,MSR_VM_OFF				/* Get real mode MSR */
			mtmsr	r7							/* Set the real mode SRR */
			isync					

;			Map in the first 256Mb in both instruction and data BATs

			li		r7,((0x7FF<<2)|2)  					; Set up for V=R 256MB in supervisor space
			li      r8,((2<<3)|2)						; Physical address = 0, coherent, R/W
			li		r9,0								; Clear out a register
			
			mtsprg	0,r9								; Insure the SPRGs are clear
			mtsprg	1,r9
			mtsprg	2,r9
			mtsprg	3,r9

			sync
			isync
			mtdbatu 0,r7								; Map bottom 256MB
			mtdbatl 0,r8								; Map bottom 256MB
			mtdbatu 1,r9								; Invalidate maps
			mtdbatl 1,r9								; Invalidate maps
			mtdbatu 2,r9								; Invalidate maps
			mtdbatl 2,r9								; Invalidate maps
			mtdbatu 3,r9								; Invalidate maps
			mtdbatl 3,r9								; Invalidate maps
			sync
			isync
			mtibatu 0,r7								; Map bottom 256MB
			mtibatl 0,r8								; Map bottom 256MB
			mtibatu 1,r9								; Invalidate maps
			mtibatl 1,r9								; Invalidate maps
			mtibatu 2,r9								; Invalidate maps
			mtibatl 2,r9								; Invalidate maps
			mtibatu 3,r9								; Invalidate maps
			mtibatl 3,r9								; Invalidate maps
			sync
			isync

			mfpvr	r10
			rlwinm	r11,r10,16,16,31
			cmplwi	r11,PROCESSOR_VERSION_Max			; Do we have Altivec?
			blt		novmx								; Nope...

; ?
			rlwinm	r13,r10,0,16,31						; ?
			cmplwi	r13, 0x0200							; ?
			bge	notMaxV1								; ?

			sync
			mfspr	r11,msscr0							; ?
			mfspr	r12,msscr1							; ?
			
			oris	r11,r11,hi16(shden)					; ?
			
			andis.	r0,r11,hi16(emodem)					; ?
			bne		inmaxbus							; ?
			ori		r11,r11,lo16(tfstm)					; ?
			b		nomaxbus							; ?

inmaxbus:	oris	r12,r12,hi16(cqdm)					; ?

nomaxbus:	rlwinm	r12,r12,0,csqe+1,csqs-1				; ?
			oris	r12,r12,0x2000						; ?
			
			sync
			mtspr	msscr0,r11							; ?
			mtspr	msscr1,r12							; ?
			sync
			isync

notMaxV1:
; ?
			cmplwi	r13,0x0202							; ?
			bge	notMaxV2_0								; ?

			lis	r11, 0xffff
			ori	r11, r11, 0xfffe
			mtspr	iabr, r11
			isync

notMaxV2_0:

			li		r0,0								; Clear out a register
			
			lis		r7,hi16(MSR_VEC_ON)					; Get real mode MSR + FP + Altivec
			ori		r7,r7,lo16(MSR_VM_OFF|MASK(MSR_FP))	; Get real mode MSR + FP + Altivec
			mtmsr	r7									; Set the real mode SRR */
			isync										; Make sure it has happened									
		
			lis		r5,hi16(EXT(QNaNbarbarian))			; Altivec initializer
			ori		r5,r5,lo16(EXT(QNaNbarbarian))		; Altivec initializer

			mtspr	vrsave,r0							; Set that no VRs are used yet */
			
			vspltisw	v1,0							; Clear a register
			lvx		v0,br0,r5							; Initialize VR0
			mtvscr	v1									; Clear the vector status register
			vor		v2,v0,v0							; Copy into the next register
			vor		v1,v0,v0							; Copy into the next register
			vor		v3,v0,v0							; Copy into the next register
			vor		v4,v0,v0							; Copy into the next register
			vor		v5,v0,v0							; Copy into the next register
			vor		v6,v0,v0							; Copy into the next register
			vor		v7,v0,v0							; Copy into the next register
			vor		v8,v0,v0							; Copy into the next register
			vor		v9,v0,v0							; Copy into the next register
			vor		v10,v0,v0							; Copy into the next register
			vor		v11,v0,v0							; Copy into the next register
			vor		v12,v0,v0							; Copy into the next register
			vor		v13,v0,v0							; Copy into the next register
			vor		v14,v0,v0							; Copy into the next register
			vor		v15,v0,v0							; Copy into the next register
			vor		v16,v0,v0							; Copy into the next register
			vor		v17,v0,v0							; Copy into the next register
			vor		v18,v0,v0							; Copy into the next register
			vor		v19,v0,v0							; Copy into the next register
			vor		v20,v0,v0							; Copy into the next register
			vor		v21,v0,v0							; Copy into the next register
			vor		v22,v0,v0							; Copy into the next register
			vor		v23,v0,v0							; Copy into the next register
			vor		v24,v0,v0							; Copy into the next register
			vor		v25,v0,v0							; Copy into the next register
			vor		v26,v0,v0							; Copy into the next register
			vor		v27,v0,v0							; Copy into the next register
			vor		v28,v0,v0							; Copy into the next register
			vor		v29,v0,v0							; Copy into the next register
			vor		v30,v0,v0							; Copy into the next register
			vor		v31,v0,v0							; Copy into the next register

			cmplwi	r30,1								; Are we the boot processor?
			mfspr	r11,msscr0							; Get the memory system control register
			dssall										; Force all data stream stuff to stop
			oris	r11,r11,hi16(dl1hwfm)				; Turn on the hardware flush request
			sync
			beq		invl1								; Not boot, invalidate L1...
			
			mtspr	msscr0,r11							; Start the flush operation
			
rstbsy:		mfspr	r11,msscr0							; Get the control register again
			
			rlwinm.	r11,r11,0,dl1hwf,dl1hwf				; Has the flush request been reset yet?
			bne		rstbsy								; No, flush is still in progress...
			
			sync										; Make sure all flushes have been committed
			b		setmck								; Go set mck handling...
			
invl1:		mfspr	r8,hid0								; Set the HID0 bits for enable, and invalidate
			ori		r8,r8,lo16(0x0000CC00)				; Do an isync just incase the cache was off 
			sync
			isync										; Start the invalidate 
			mtspr	hid0,r8								; To finish the invalidate clear the inval bits
			li		r9,lo16(0x00000C00)
			andc	r8,r8,r9							; End the invalidate
			mtspr	hid0,r8								; Make sure all is done
			sync										; Make sure it is really done
			sync
			
setmck:		mfspr	r11,hid0							; ?
			ori		r11,r11,hi16(eiecm)					; ?
			mtspr	hid0,r11							; ?
			isync
			
#if 0
;			(TEST/DEBUG) Flush and turn off L2 Cache so I can display memory with the durn ESP unit
			b		flushl2								; (TEST/DEBUG) Jump to start

			.align	5									; (TEST/DEBUG) Force alignment to cache
			nop
			nop
			nop
			nop
			nop
flushl2:	mfspr	r11,l2cr							; (TEST/DEBUG) Get L2 control register
			ori		r11,r11,0x0800						; (TEST/DEBUG) Turn on L2 hardware flush request
			sync										; (TEST/DEBUG) 

;			Cache boundary here 
;			The following flush and cache disable are all in the same line

			mtspr	l2cr,r11							; (TEST/DEBUG) Start the flush
			
flushl2d:	mfspr	r11,l2cr							; (TEST/DEBUG) Get the control reg again
			rlwinm.	r8,r11,0,20,20						; (TEST/DEBUG) Have we finished?
			bne		flushl2d							; (TEST/DEBUG) Nope...
			sync										; (TEST/DEBUG) 
			rlwinm	r11,r11,0,1,31						; (TEST/DEBUG) Turn off L2
			mtspr	l2cr,r11							; (TEST/DEBUG) Finish it
			sync										; (TEST/DEBUG) Quiet down

;			Cache boundary here 
;			Invalidate the L2 cache, just for funsies...

			oris	r8,r11,0x0020						; (TEST/DEBUG) Turn on invalidate request
			mtspr	l2cr,r8								; (TEST/DEBUG) Start invalidation
			sync										; (TEST/DEBUG)

invl2:		mfspr	r8,l2cr								; (TEST/DEBUG) Get the L2 control register
			rlwinm.	r8,r8,0,31,31						; (TEST/DEBUG) Is the invalidation still going on?
			bne+	invl2								; (TEST/DEBUG) Yes, keep going...
			
			mtspr	l2cr,r11							; (TEST/DEBUG) Turn off invalidate request, leaving L2 disabled
			sync
;			Cache boundary here 
#endif			
			b		invcache							; (TEST/DEBUG) Now, go invalidate level 1...


novmx:
/*
 *			We need to have our memory coherent, but we may not be here.  If we just switch the 
 *			BATs, we could end up with memory paradoxes, i.e., cache entries left over from
 *			when the memory was incoherent, just babbling away...  To prevent this, we have to get
 *			rid of all non-coherent lines.  Since we just loaded the system, we could have tons
 *			of lines with and unknown address ranges, so we would have to do a whole slew of DCBSTs
 *			to force them all out.  We can't just flash invalidate, 'cause we'd lose a few lines.
 *			So, we go translate off, load up some safe lines (from the ROM), and then flash invalidate 
 *			the caches.  We don't do this for a 601 'cause there ain't no DBATs or flash invalidate.
 */
			
			
realcode:	lis		r8,0xFFF0							/* Point to the ROM */
			addis	r9,r8,0x0002						/* Point 128k later */
			subi	r8,r8,32							/* Start back one */
			
readROM:	lwz		r10,32(r8)							/* Get a line into cache */
			addi	r8,r8,32
			cmplw	cr0,r8,r9							/* See if we're done */
			blt+	readROM								/* Do it all... */
			
			sync

		
invcache:
			mfspr	r8,hid0								; Turn on the L1 and invalidate it
			ori		r8,r8,lo16(0x0000CC00)				; Set the HID0 bits for enable, and invalidate
			isync										; Do an isync just incase the cache was off
			mtspr	hid0,r8								; Start the invalidate
			li		r9,lo16(0x00000C00)					; To finish the invalidate clear the inval bits
			andc	r8,r8,r9
			mtspr	hid0,r8								; End the invalidate
			sync										; Make sure all is done
			sync										; Make sure it is really done 

/* */
/*			Clear out the TLB.  They be garbage after hard reset. */
/* */

			lis		r12,hi16(EXT(tlb_system_lock))		/* Get the TLBIE lock */
			li		r0,512								/* Get number of TLB entries (overestimate at 512 entries) */
			ori		r12,r12,lo16(EXT(tlb_system_lock))	/* Grab up the bottom part */
			li		r3,0								/* Start at 0 */

			lwarx	r5,0,r12							; ?

itlbhang:	lwarx	r5,0,r12							/* Get the TLBIE lock */
			mr.		r5,r5								/* Is it locked? */
			bne-	itlbhang							/* It's locked, go wait... */
			stwcx.	r0,0,r12							/* Try to get it */
			bne-	itlbhang							/* We was beat... */

			mtctr	r0									/* Set the CTR */
			
IRpurgeTLB:	tlbie	r3									/* Purge this entry */
			addi	r3,r3,4096							/* Next page */
			bdnz	IRpurgeTLB							/* Do 'em all... */
			
			sync										/* Make sure all TLB purges are done */

			mfpvr	r10
			rlwinm	r10,r10,16,16,31

			eieio										/* Make sure that the tlbie happens first */
			
			cmplwi	r10,PROCESSOR_VERSION_603			/* Got a 603 */
			beq		donttsync							/* Yeah, don't sync... */

			cmplwi	r10,PROCESSOR_VERSION_603e			/* Got a 603e */
			beq		donttsync							/* Yeah, don't sync... */

			cmplwi	r10,PROCESSOR_VERSION_750			/* Got a 750 */
			beq		donttsync							/* Yeah, don't sync... */

			tlbsync										/* Make sure on other processors also */
			sync										/* Make sure the TLBSYNC is done */
	
donttsync:	stw		r5,0(r12)							; Unlock the TLBIE interlock	

			li		r0,MSR_SUPERVISOR_INT_OFF|MASK(MSR_FP)		/* Make sure we don't have ints enabled */
			mtmsr	r0									/* Set the standard MSR values */
			isync
			
			lis		r5,HIGH_ADDR(EXT(FloatInit))		/* Get top of floating point init value */
			ori		r5,r5,LOW_ADDR(EXT(FloatInit))		/* Slam bottom */
			lfd		f0,0(r5)							/* Initialize FP0 */
			fmr		f1,f0								/* Ours in not */					
			fmr		f2,f0								/* to wonder why, */
			fmr		f3,f0								/* ours is but to */
			fmr		f4,f0								/* do or die! */
			fmr		f5,f0						
			fmr		f6,f0						
			fmr		f7,f0						
			fmr		f8,f0						
			fmr		f9,f0						
			fmr		f10,f0						
			fmr		f11,f0						
			fmr		f12,f0						
			fmr		f13,f0						
			fmr		f14,f0						
			fmr		f15,f0						
			fmr		f16,f0						
			fmr		f17,f0						
			fmr		f18,f0						
			fmr		f19,f0						
			fmr		f20,f0						
			fmr		f21,f0						
			fmr		f22,f0						
			fmr		f23,f0						
			fmr		f24,f0						
			fmr		f25,f0						
			fmr		f26,f0						
			fmr		f27,f0						
			fmr		f28,f0						
			fmr		f29,f0						
			fmr		f30,f0						
			fmr		f31,f0						
		
			li		r0,	MSR_SUPERVISOR_INT_OFF			/* Make sure we don't have FP enabled */
			mtmsr	r0									/* Set the standard MSR values */
			isync

#if 0
			li		r3,0				/* (TEST/DEBUG) */
			bl		EXT(fwSCCinit)		/* (TEST/DEBUG) */
#endif
#if 0
			li		r3,1				/* (TEST/DEBUG) */
			bl		EXT(fwSCCinit)		/* (TEST/DEBUG) */
#endif

			lis		r20,HIGH_ADDR(fwdisplock)	/* Get address of the firmware display lock */
			li		r19,0						/* Zorch a register */
			ori		r20,r20,LOW_ADDR(fwdisplock)	/* Get address of the firmware display lock */
			stw		r19,0(r20)					/* Make sure the lock is free */

#if 0
			li		r3,0				/* (TEST/DEBUG) */
			lis		r4,0x6D65			/* (TEST/DEBUG) 'memg' */
			ori		r4,r4,0x6D67		/* (TEST/DEBUG) */
			li		r5,0				/* (TEST/DEBUG) */
			bl		EXT(dbgDispLL)		/* (TEST/DEBUG) */
#endif
			cmpi	cr0, r30, 1
			beq		callcpu
	/* move onto interrupt stack */

			lis		r29,HIGH_ADDR(EXT(intstack_top_ss))
			ori		r29,r29,LOW_ADDR(EXT(intstack_top_ss))
#if 0
			li		r3,0				/* (TEST/DEBUG) */
			lis		r4,0x7232			/* (TEST/DEBUG) 'r29a' */
			ori		r4,r4,0x3961		/* (TEST/DEBUG) */
			mr		r5,r29				/* (TEST/DEBUG) */
			bl		EXT(dbgDispLL)		/* (TEST/DEBUG) */
#endif
			lwz		r29,0(r29)
#if 0
			li		r3,0				/* (TEST/DEBUG) */
			lis		r4,0x7232			/* (TEST/DEBUG) 'r29b' */
			ori		r4,r4,0x3962		/* (TEST/DEBUG) */
			mr		r5,r29				/* (TEST/DEBUG) */
			bl		EXT(dbgDispLL)		/* (TEST/DEBUG) */
#endif

	li	r28,	0
	stw	r28,	FM_BACKPTR(r29) /* store a null frame backpointer */

	/* move onto new stack */
	
#if 0
			li		r3,0				/* (TEST/DEBUG) */
			lis		r4,0x676F			/* (TEST/DEBUG) 'gogo' */
			ori		r4,r4,0x676F		/* (TEST/DEBUG) */
			mr		r5,r29				/* (TEST/DEBUG) */
			bl		EXT(dbgDispLL)		/* (TEST/DEBUG) */
#endif

			mr		r1,r29
			mr		r3,r31				/* Restore any arguments we may have trashed */

			bl	EXT(ppc_init)

			/* Should never return */

			BREAKPOINT_TRAP

callcpu:
			/* move onto interrupt stack */
			lwz		r29,PP_INTSTACK_TOP_SS(r31)
			li		r28,	0
			stw		r28,	FM_BACKPTR(r29) /* store a null frame backpointer */

			/* move onto new stack */
			mr		r1,r29
			mr		r3,r31				/* Restore any arguments we may have trashed */

			bl	EXT(ppc_init_cpu)

			/* Should never return */

			BREAKPOINT_TRAP

