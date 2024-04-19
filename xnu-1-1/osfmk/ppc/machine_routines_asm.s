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
#include <ppc/asm.h>
#include <ppc/proc_reg.h>
#include <cpus.h>
#include <assym.s>
#include <debug.h>
#include <mach/ppc/vm_param.h>
#include <ppc/exception.h>
	
/* PCI config cycle probing
 *
 *	boolean_t ml_probe_read(vm_offset_t paddr, unsigned int *val)
 *
 *	Read the memory location at physical address paddr.
 *  This is a part of a device probe, so there is a good chance we will
 *  have a machine check here. So we have to be able to handle that.
 *  We assume that machine checks are enabled both in MSR and HIDs
 */

;			Force a line boundry here
			.align	5
			.globl	EXT(ml_probe_read)

LEXT(ml_probe_read)

			mfmsr	r0								; Save the current MSR
			neg		r10,r3							; Number of bytes to end of page
			rlwinm	r2,r0,0,MSR_EE_BIT+1,MSR_EE_BIT-1	; Clear interruptions
			rlwinm.	r10,r10,0,20,31					; Clear excess junk and test for page bndry
			mr		r12,r3							; Save the load address
			cmplwi	cr1,r10,4						; At least 4 bytes left in page?
			rlwinm	r2,r2,0,MSR_DR_BIT+1,MSR_IR_BIT-1	; Clear translation			
			li		r3,0							; Assume failure just for now
			beq-	mprdoit							; We are right on the boundary...
			bltlr-									; No, just return failure...

mprdoit:	mtmsr	r2								; Translation and interrupts off
			isync									; Make sure it is done
			
;
;			We need to insure that there is no more than 1 BAT register that
;			can get a hit. There could be repercussions beyond the ken
;			of mortal man. It is best not to tempt fate.
;
			li		r10,0							; Clear a register
			mfdbatu	r5,0							; Save DBAT 0 high
			mfdbatl	r6,0							; Save DBAT 0 low
			mfdbatu	r7,1							; Save DBAT 1 high
			mfdbatu	r8,2							; Save DBAT 2 high
			mfdbatu	r9,3							; Save DBAT 3 high 
			
			sync									; Make sure all is well

			mtdbatu	1,r10							; Invalidate DBAT 1 
			mtdbatu	2,r10							; Invalidate DBAT 2 
			mtdbatu	3,r10							; Invalidate DBAT 3  
			
			rlwinm	r10,r12,0,0,14					; Round down to a 128k boundary
			ori		r11,r10,0x32					; Set uncached, coherent, R/W
			ori		r10,r10,2						; Make the upper half (128k, valid supervisor)
			mtdbatl	0,r11							; Set lower BAT first
			mtdbatu	0,r10							; Now the upper
			sync									; Just make sure
			
			ori		r11,r2,lo16(MASK(MSR_DR))		; Turn on data translation
			mtmsr	r11								; Do it for real
			isync									; Make sure of it
			
			eieio									; Make sure of all previous accesses
			sync									; Make sure it is all caught up
			
			lwz		r11,0(r12)						; Get it and maybe machine check here
			
			eieio									; Make sure of ordering again
			sync									; Get caught up yet again
			isync									; Do not go further till we are here
			
			mtdbatu	0,r5							; Restore DBAT 0 high
			mtdbatl	0,r6							; Restore DBAT 0 low
			mtdbatu	1,r7							; Restore DBAT 1 high
			mtdbatu	2,r8							; Restore DBAT 2 high
			mtdbatu	3,r9							; Restore DBAT 3 high 
			sync
			
			li		r3,1							; We made it
			
			mtmsr	r0								; Restore translation and exceptions
			isync									; Toss speculations
			
			stw		r11,0(r4)						; Save the loaded value
			blr										; Return...
			
;			Force a line boundry here. This means we will be able to check addresses better
			.align	5
			.globl	EXT(ml_probe_read_mck)
LEXT(ml_probe_read_mck)

/* Read physical address
 *
 *	unsigned int ml_phys_read(vm_offset_t paddr)
 *
 *	Read the word at physical address paddr. Memory should not be cache inhibited.
 */

;			Force a line boundry here
			.align	5
			.globl	EXT(ml_phys_read)

LEXT(ml_phys_read)

			mfmsr	r0								; Save the current MSR
			rlwinm	r4,r0,0,MSR_EE_BIT+1,MSR_EE_BIT-1	; Clear interruptions
			rlwinm	r4,r4,0,MSR_DR_BIT+1,MSR_IR_BIT-1	; Clear translation	
			mtmsr	r4								; Translation and interrupts off
			isync									; Make sure about it
			
			lwz		r3,0(r3)						; Get the word
			sync

			mtmsr	r0								; Restore translation and rupts
			isync
			blr

/* Write physical address
 *
 *	void ml_phys_write(vm_offset_t paddr, unsigned int data)
 *
 *	Write the word at physical address paddr. Memory should not be cache inhibited.
 */

;			Force a line boundry here
			.align	5
			.globl	EXT(ml_phys_write)

LEXT(ml_phys_write)

			mfmsr	r0								; Save the current MSR
			rlwinm	r5,r0,0,MSR_EE_BIT+1,MSR_EE_BIT-1	; Clear interruptions
			rlwinm	r5,r5,0,MSR_DR_BIT+1,MSR_IR_BIT-1	; Clear translation	
			mtmsr	r5								; Translation and interrupts off
			isync									; Make sure about it
			
			stw		r4,0(r3)						; Set the word
			sync

			mtmsr	r0								; Restore translation and rupts
			isync
			blr


/* set interrupts enabled or disabled
 *
 *	boolean_t set_interrupts_enabled(boolean_t enable)
 *
 *	Set EE bit to "enable" and return old value as boolean
 */

;			Force a line boundry here
			.align	5
			.globl	EXT(set_interrupts_enabled)

LEXT(set_interrupts_enabled)

			mfmsr	r5								; Get the current MSR
			mr		r4,r3							; Save the old value
			rlwinm	r3,r5,17,31,31					; Set return value
			rlwimi	r5,r4,15,16,16					; Insert new EE bit
			mtmsr	r5								; Slam enablement
			blr										; Like leave...
			
