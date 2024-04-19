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

#ifndef _I82557INLINE_H
#define _I82557INLINE_H

//---------------------------------------------------------------------------
// CSR macros.

#define CSR_VALUE(name, x)	(((x) & name ## _MASK) >> name ## _SHIFT)
#define CSR_FIELD(name, x)	(((x) << name ## _SHIFT) & name ## _MASK)
#define CSR_MASK(name, x)	((x) << name ## _SHIFT)
#define BIT(x)				(1 << (x))

//---------------------------------------------------------------------------
// Flush write buffers.

#ifdef	__ppc__
#define IOSync()	__asm__ ("eieio")
#else
#define IOSync()
#endif

//---------------------------------------------------------------------------
// CSR read & write.

#ifdef	__BIG_ENDIAN__

#ifdef	__ppc__

static inline
UInt8
OSReadLE8(volatile void * base)
{
	return *(volatile UInt8 *)base;
}

static inline
UInt16
OSReadLE16(volatile void * base)
{
    UInt16 result;
    __asm__ volatile("lhbrx %0, 0, %1"
                     : "=r" (result)
                     : "r" (base)
                     : "r0");
    return result;
}

static inline
UInt32
OSReadLE32(volatile void * base)
{
    UInt32 result;
    __asm__ volatile("lwbrx %0, 0, %1"
                     : "=r" (result)
                     : "r" (base)
                     : "r0");
    return result;
}

static inline
void
OSWriteLE8(volatile void * base, UInt8 data)
{
	*(volatile UInt8 *)base = data;
	IOSync();
}

static inline
void
OSWriteLE16(volatile void * base, UInt16 data)
{
    __asm__ volatile("sthbrx %0, 0, %1"
                     :
                     : "r" (data), "r" (base)
                     : "r0");
	IOSync();
}

static inline
void
OSWriteLE32(volatile void * base, UInt32 data)
{
    __asm__ volatile("stwbrx %0, 0, %1"
                     :
                     : "r" (data), "r" (base)
                     : "r0" );
	IOSync();
}

#else
#error Unknown big-endian processor type
#endif /* __ppc__ */

#else	/* little endian processor */

static inline
UInt8
OSReadLE8(volatile void * base)
{
	return *(volatile UInt8 *)base;
}

static inline
UInt16
OSReadLE16(volatile void * base)
{
	return *(volatile UInt16 *)base;
}

static inline
UInt32
OSReadLE32(volatile void * base)
{
	return *(volatile UInt32 *)base;
}

static inline
void
OSWriteLE8(volatile void * base, UInt8 data)
{
	*(volatile UInt8 *)base = data;
}

static inline
void
OSWriteLE16(volatile void * base, UInt16 data)
{
	*(volatile UInt16 *)base = data;
}

static inline
void
OSWriteLE32(volatile void * base, UInt32 data)
{
	*(volatile UInt32 *)base = data;
}

#endif	/* __BIG_ENDIAN__ */

//---------------------------------------------------------------------------
// Set/clear bit(s) macros.

#define __SET(n) \
static inline void \
OSSetLE##n(volatile void * base, UInt##n bit) \
{ \
	OSWriteLE##n(base, (OSReadLE##n(base) | (bit))); \
}

#define __CLR(n) \
static inline void \
OSClearLE##n(volatile void * base, UInt##n bit) \
{ \
	OSWriteLE##n(base, (OSReadLE##n(base) & ~(bit))); \
}
	
__SET(8)
__SET(16)
__SET(32)

__CLR(8)
__CLR(16)
__CLR(32)

#endif /* !_I82557INLINE_H */
