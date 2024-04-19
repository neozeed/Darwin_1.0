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
/*	CFByteOrder.h
	Copyright 1995-2000, Apple, Inc. All rights reserved.
*/

#if !defined(__COREFOUNDATION_CFBYTEORDER__)
#define __COREFOUNDATION_CFBYTEORDER__ 1

#if !defined(__BIG_ENDIAN__) && !defined(__LITTLE_ENDIAN__)
#error Do not know the endianess of this architecture
#endif

#include <CoreFoundation/CFBase.h>

#if defined(__cplusplus)
extern "C" {
#endif

typedef enum __CFByteOrder {
    CFByteOrderUnknown,
    CFByteOrderLittleEndian,
    CFByteOrderBigEndian
} CFByteOrder;

CF_INLINE CFByteOrder CFByteOrderGetCurrent(void) {
    UInt32 x = (CFByteOrderBigEndian << 24) | CFByteOrderLittleEndian;
    return (CFByteOrder)*((UInt8 *)&x);
}

CF_INLINE UInt16 CFSwapInt16(UInt16 arg) {
#if defined(__i386__)
    UInt16 result;
    __asm__ volatile("rorw $8,%0" : "=r" (result) : "0" (arg));
    return result;
#elif defined(__ppc__) && defined(__GNUC__)
    UInt16 result;
    __asm__ volatile("lhbrx %0, 0, %1" : "=r" (result) : "r" (&arg) : "r0");
    return result;
#elif 0
    UInt16 result;
    result = ((arg<<8) & 0xFF00) | ((arg>>8) & 0x00FF);
    return result;
#else
    union CFSwap {
	UInt16 sv;
	UInt8 uc[2];
    } result, *argp = (union CFSwap *)&arg;
    result.uc[0] = argp->uc[1];
    result.uc[1] = argp->uc[0];
    return result.sv;
#endif
}

CF_INLINE UInt32 CFSwapInt32(UInt32 arg) {
#if defined(__i386__)
    UInt32 result;
    __asm__ volatile("bswap %0" : "=r" (result) : "0" (arg));
    return result;
#elif defined(__ppc__) && defined(__GNUC__)
    UInt32 result;
    __asm__ volatile("lwbrx %0, 0, %1" : "=r" (result) : "r" (&arg) : "r0");
    return result;
#elif 0
    UInt32 result;
    result =	((arg<<24) & 0xFF000000) |
		((arg<<8)  & 0x00FF0000) |
		((arg>>8)  & 0x0000FF00) |
		((arg>>24) & 0x000000FF);
    return result;
#else
    union CFSwap {
	UInt32 sv;
	UInt8 uc[4];
    } result, *argp = (union CFSwap *)&arg;
    result.uc[0] = argp->uc[3];
    result.uc[1] = argp->uc[2];
    result.uc[2] = argp->uc[1];
    result.uc[3] = argp->uc[0];
    return result.sv;
#endif
}

CF_INLINE UInt64 CFSwapInt64(UInt64 arg) {
#if 0
    UInt64 result;
    result =	((arg<<56) & 0xFF00000000000000ULL) |
		((arg<<40) & 0x00FF000000000000ULL) |
		((arg<<24) & 0x0000FF0000000000ULL) |
		((arg<<8)  & 0x000000FF00000000ULL) |
		((arg>>8)  & 0x00000000FF000000ULL) |
		((arg>>24) & 0x0000000000FF0000ULL) |
		((arg>>40) & 0x000000000000FF00ULL) |
		((arg>>56) & 0x00000000000000FFULL);
    return result;
#else
    union CFSwap {
	UInt64 sv;
	UInt8 uc[8];
    } result, *argp = (union CFSwap *)&arg;
    result.uc[0] = argp->uc[7];
    result.uc[1] = argp->uc[6];
    result.uc[2] = argp->uc[5];
    result.uc[3] = argp->uc[4];
    result.uc[4] = argp->uc[3];
    result.uc[5] = argp->uc[2];
    result.uc[6] = argp->uc[1];
    result.uc[7] = argp->uc[0];
    return result.sv;
#endif
}

CF_INLINE UInt16 CFSwapInt16BigToHost(UInt16 arg) {
#if defined(__BIG_ENDIAN__)
    return arg;
#else
    return CFSwapInt16(arg);
#endif
}

CF_INLINE UInt32 CFSwapInt32BigToHost(UInt32 arg) {
#if defined(__BIG_ENDIAN__)
    return arg;
#else
    return CFSwapInt32(arg);
#endif
}

CF_INLINE UInt64 CFSwapInt64BigToHost(UInt64 arg) {
#if defined(__BIG_ENDIAN__)
    return arg;
#else
    return CFSwapInt64(arg);
#endif
}

CF_INLINE UInt16 CFSwapInt16HostToBig(UInt16 arg) {
#if defined(__BIG_ENDIAN__)
    return arg;
#else
    return CFSwapInt16(arg);
#endif
}

CF_INLINE UInt32 CFSwapInt32HostToBig(UInt32 arg) {
#if defined(__BIG_ENDIAN__)
    return arg;
#else
    return CFSwapInt32(arg);
#endif
}

CF_INLINE UInt64 CFSwapInt64HostToBig(UInt64 arg) {
#if defined(__BIG_ENDIAN__)
    return arg;
#else
    return CFSwapInt64(arg);
#endif
}

CF_INLINE UInt16 CFSwapInt16LittleToHost(UInt16 arg) {
#if defined(__LITTLE_ENDIAN__)
    return arg;
#else
    return CFSwapInt16(arg);
#endif
}

CF_INLINE UInt32 CFSwapInt32LittleToHost(UInt32 arg) {
#if defined(__LITTLE_ENDIAN__)
    return arg;
#else
    return CFSwapInt32(arg);
#endif
}

CF_INLINE UInt64 CFSwapInt64LittleToHost(UInt64 arg) {
#if defined(__LITTLE_ENDIAN__)
    return arg;
#else
    return CFSwapInt64(arg);
#endif
}

CF_INLINE UInt16 CFSwapInt16HostToLittle(UInt16 arg) {
#if defined(__LITTLE_ENDIAN__)
    return arg;
#else
    return CFSwapInt16(arg);
#endif
}

CF_INLINE UInt32 CFSwapInt32HostToLittle(UInt32 arg) {
#if defined(__LITTLE_ENDIAN__)
    return arg;
#else
    return CFSwapInt32(arg);
#endif
}

CF_INLINE UInt64 CFSwapInt64HostToLittle(UInt64 arg) {
#if defined(__LITTLE_ENDIAN__)
    return arg;
#else
    return CFSwapInt64(arg);
#endif
}

typedef struct {UInt32 v;} CFSwappedFloat32;
typedef struct {UInt64 v;} CFSwappedFloat64;

CF_INLINE CFSwappedFloat32 CFConvertFloat32HostToSwapped(Float32 arg) {
    union CFSwap {
	Float32 v;
	CFSwappedFloat32 sv;
    } result;
    result.v = arg;
#if defined(__LITTLE_ENDIAN__)
    result.sv.v = CFSwapInt32(result.sv.v);
#endif
    return result.sv;
}

CF_INLINE Float32 CFConvertFloat32SwappedToHost(CFSwappedFloat32 arg) {
    union CFSwap {
	Float32 v;
	CFSwappedFloat32 sv;
    } result;
    result.sv = arg;
#if defined(__LITTLE_ENDIAN__)
    result.sv.v = CFSwapInt32(result.sv.v);
#endif
    return result.v;
}

CF_INLINE CFSwappedFloat64 CFConvertFloat64HostToSwapped(Float64 arg) {
    union CFSwap {
	Float64 v;
	CFSwappedFloat64 sv;
    } result;
    result.v = arg;
#if defined(__LITTLE_ENDIAN__)
    result.sv.v = CFSwapInt64(result.sv.v);
#endif
    return result.sv;
}

CF_INLINE Float64 CFConvertFloat64SwappedToHost(CFSwappedFloat64 arg) {
    union CFSwap {
	Float64 v;
	CFSwappedFloat64 sv;
    } result;
    result.sv = arg;
#if defined(__LITTLE_ENDIAN__)
    result.sv.v = CFSwapInt64(result.sv.v);
#endif
    return result.v;
}

CF_INLINE CFSwappedFloat32 CFConvertFloatHostToSwapped(float arg) {
    union CFSwap {
	float v;
	CFSwappedFloat32 sv;
    } result;
    result.v = arg;
#if defined(__LITTLE_ENDIAN__)
    result.sv.v = CFSwapInt32(result.sv.v);
#endif
    return result.sv;
}

CF_INLINE float CFConvertFloatSwappedToHost(CFSwappedFloat32 arg) {
    union CFSwap {
	float v;
	CFSwappedFloat32 sv;
    } result;
    result.sv = arg;
#if defined(__LITTLE_ENDIAN__)
    result.sv.v = CFSwapInt32(result.sv.v);
#endif
    return result.v;
}

CF_INLINE CFSwappedFloat64 CFConvertDoubleHostToSwapped(double arg) {
    union CFSwap {
	double v;
	CFSwappedFloat64 sv;
    } result;
    result.v = arg;
#if defined(__LITTLE_ENDIAN__)
    result.sv.v = CFSwapInt64(result.sv.v);
#endif
    return result.sv;
}

CF_INLINE double CFConvertDoubleSwappedToHost(CFSwappedFloat64 arg) {
    union CFSwap {
	double v;
	CFSwappedFloat64 sv;
    } result;
    result.sv = arg;
#if defined(__LITTLE_ENDIAN__)
    result.sv.v = CFSwapInt64(result.sv.v);
#endif
    return result.v;
}

#if defined(__cplusplus)
}
#endif

#endif /* ! __COREFOUNDATION_CFBYTEORDER__ */

