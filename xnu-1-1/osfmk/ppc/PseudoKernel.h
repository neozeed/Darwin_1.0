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
	File:		PseudoKernelPriv.h

	Contains:	Interfaces for the TruBlue environment's PseudoKernel

	Copyright:	) 1996-1997 by Apple Computer, Inc., all rights reserved.

*/

#define bbMaxTrap 16
#define bbExitTrap 14

/* Support firmware PseudoKernel FastTrap architectural extension */

#define bbPKTDShift	2					// 4 * ( 1 << shift ) = sizeof(PKTD_t)

struct PseudoKernelTrapDescriptor {
	UInt32				pc;			// address of where we want to return to
	UInt32				reg;			// place to save(GPR2) or restore(LR) registers
	UInt32				msr;			// msr FE0, BE, SE and FE1 bits to restore on exit
	UInt32				newState;		// context state when we return
};
typedef struct PseudoKernelTrapDescriptor PseudoKernelTrapDescriptor;
typedef PseudoKernelTrapDescriptor * PseudoKernelTrapDescriptorPtr;
typedef PseudoKernelTrapDescriptor PKTD_t;

struct BlueBoxData {
	PKTD_t				PKTD[bbMaxTrap];
	UInt32				InterruptControlWord;	// holds context state and backup CR2 bits
	UInt32				InterruptPendingPC;		// PC of where to handle interruptions */
	UInt32				ihs_pc;			// Interrupt Hold State
	UInt32				ihs_gpr2;		// Interrupt Hold State
	UInt32				testIntMask;		// Mask to check for a pending interrupt
	UInt32				postIntMask;		// Mask to post an interrupt
};
typedef struct BlueBoxData BlueBoxData;
typedef BlueBoxData BDA_t;
	
enum {
							// The following define the UInt32 gInterruptState
	kInUninitialized	=	0,		// State not yet initialized
	kInPseudoKernel		=	1,		// Currently executing within pseudo kernel
	kInSystemContext	=	2,		// Currently executing within the system (emulator) context
	kInAlternateContext	=	3,		// Currently executing within an alternate (native) context
	kInExceptionHandler	=	4,		// Currently executing an exception handler
	kOutsideBlue		=	5,		// Currently executing outside of the Blue thread
	kNotifyPending		=	6,		// Pending Notify Interrupt

	kInterruptStateMask	=	0x000F0000,	// Mask to extract interrupt state from gInterruptState
	kInterruptStateShift	=	16,		// Shift count to align interrupt state

	kBackupCR2Mask		=	0x0000000F,	// Mask to extract backup CR2 from gInterruptState
	kCR2ToBackupShift	=	31-11,		// Shift count to align CR2 into the backup CR2 of gInterruptState
							//  (and vice versa)
	kCR2Mask		=	0x00F00000	// Mask to extract CR2 from the PPC CR register 
};

struct bbRupt {
	struct ReturnHandler	rh;			/* Return handler address */
	unsigned long			ruptRtn;	/* Interrupt PC */
};
typedef struct bbRupt bbRupt;
