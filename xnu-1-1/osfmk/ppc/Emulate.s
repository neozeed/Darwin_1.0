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
 	Emulate.s 

	Emulate instructions and traps.

	Lovingly crafted by Bill Angell using traditional methods and only natural or recycled materials.
	No animal products are used other than rendered otter bile and deep fried pork lard.

*/

#include <cpus.h>
#include <ppc/asm.h>
#include <ppc/proc_reg.h>
#include <ppc/exception.h>
#include <mach/machine/vm_param.h>
#include <assym.s>


;			General stuff what happens here:
;				1)	All general context saved, interrupts off, translation off
;				2)	Vector and floating point disabled, but there may be live context.
;					This code is responsible for saving and restoring what is used. This
;					includes exception states, java mode, etc.
;				3)	No attempt is made to resolve page faults.  PTE misses are handled
;					automatically, but actual faults (ala copyin/copyout) are not. If 
;					a fault does occur, the exception that caused entry to the emulation
;					routine is remapped to either an instruction or data miss (depending
;					upon the stage detected) and redrived through the exception handler.
;					The only time that an instruction fault can happen is when a different
;					processor removes a mapping between our original fault and when we
;					fetch the assisted instruction. For an assisted instruction, data
;					faults should not occur (except in the MP case).  For a purely
;					emulated instruction, faults can occur.
;
;


			.align	5
			.globl	EXT(Emulate)

LEXT(Emulate)

			b		EXT(EmulExit)					; Just return for now...

