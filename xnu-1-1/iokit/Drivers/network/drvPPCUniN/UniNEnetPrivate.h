/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 1998-1999 Apple Computer
 *
 * Interface for hardware dependent (relatively) code 
 * for the UniN Ethernet chip 
 *
 * HISTORY
 *
 */


#include "UniNEnet.h"
#include "UniNEnetMII.h"

//
// Hacks, get rid of this!
//
extern "C"
{
	extern unsigned	splimp(void);
	extern void	splx(unsigned level);
	extern unsigned	splhigh(void);
}


void WriteUniNRegister( IOPPCAddress ioEnetBase, u_int32_t reg_offset, u_int32_t data);
volatile u_int32_t ReadUniNRegister( IOPPCAddress ioEnetBase, u_int32_t reg_offset);

static __inline__ u_int16_t EndianSwap16(volatile u_int16_t y)
{
    u_int16_t           result;
    volatile u_int16_t  x;

    x = y;   
    __asm__ volatile("lhbrx %0, 0, %1" : "=r" (result) : "r" (&x) : "r0");
    return result;
}

static __inline__ u_int32_t EndianSwap32(u_int32_t y)
{
    u_int32_t            result;
    volatile u_int32_t   x;

    x = y;
    __asm__ volatile("lwbrx %0, 0, %1" : "=r" (result) : "r" (&x) : "r0");
    return result;
}


