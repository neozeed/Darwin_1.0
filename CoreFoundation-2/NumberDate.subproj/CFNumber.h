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
/*	CFNumber.h
	Copyright 1999-2000, Apple, Inc. All rights reserved.
*/

#if !defined(__COREFOUNDATION_CFNUMBER__)
#define __COREFOUNDATION_CFNUMBER__ 1

#include <CoreFoundation/CFBase.h>

#if defined(__cplusplus)
extern "C" {
#endif

typedef const struct __CFBoolean * CFBooleanRef;

CF_EXPORT
const CFBooleanRef kCFBooleanTrue;
CF_EXPORT
const CFBooleanRef kCFBooleanFalse;

CF_EXPORT
CFTypeID CFBooleanGetTypeID(void);

CF_EXPORT
Boolean CFBooleanGetValue(CFBooleanRef boolean);

typedef enum {
    /* Types from MacTypes.h */
    kCFNumberSInt8Type = 1,
    kCFNumberSInt16Type = 2,
    kCFNumberSInt32Type = 3,
    kCFNumberSInt64Type = 4,
    kCFNumberFloat32Type = 5,
    kCFNumberFloat64Type = 6,	/* 64-bit IEEE 754 */
    /* Basic C types */
    kCFNumberCharType = 7,
    kCFNumberShortType = 8,
    kCFNumberIntType = 9,
    kCFNumberLongType = 10,
    kCFNumberLongLongType = 11,
    kCFNumberFloatType = 12,
    kCFNumberDoubleType = 13,
    /* Other */
    kCFNumberCFIndexType = 14,
    kCFNumberMaxType = 14
} CFNumberType;

typedef const struct __CFNumber * CFNumberRef;

CF_EXPORT
const CFNumberRef kCFNumberPositiveInfinity;
CF_EXPORT
const CFNumberRef kCFNumberNegativeInfinity;
CF_EXPORT
const CFNumberRef kCFNumberNaN;

CF_EXPORT
CFTypeID CFNumberGetTypeID(void);

/*
	Creates a CFNumber with the given value. The type of number pointed
	to by the valuePtr is specified by type. If type is a floating point
	type and the value represents one of the infinities or NaN, the
	well-defined CFNumber for that value is returned. If either of
	valuePtr or type is an invalid value, the result is undefined.
*/
CF_EXPORT
CFNumberRef CFNumberCreate(CFAllocatorRef allocator, CFNumberType theType, const void *valuePtr);

/*
	Returns the storage format of the CFNumber's value.  Note that
	this is not necessarily the type provided in CFNumberCreate().
*/
CF_EXPORT
CFNumberType CFNumberGetType(CFNumberRef number);

/*
	Returns the size in bytes of the type of the number.
*/
CF_EXPORT
CFIndex CFNumberGetByteSize(CFNumberRef number);

/*
	Returns TRUE if the type of the CFNumber's value is one of
	the defined floating point types.
*/
CF_EXPORT
Boolean CFNumberIsFloatType(CFNumberRef number);

/*
	Copies the CFNumber's value into the space pointed to by
	valuePtr, as the specified type. If conversion needs to take
	place, the conversion rules follow human expectation and not
	C's promotion and truncation rules. If the conversion is
	lossy, or the value is out of range, FALSE is returned. Best
	attempt at conversion will still be in *valuePtr.
*/
CF_EXPORT
Boolean CFNumberGetValue(CFNumberRef number, CFNumberType theType, void *valuePtr);

/*
	Compares the two CFNumber instances. If conversion of the
	types of the values is needed, the conversion and comparison
	follow human expectations and not C's promotion and comparison
	rules. Negative zero compares less than positive zero.
	Positive infinity compares greater than everything except
	itself, to which it compares equal. Negative infinity compares
	less than everything except itself, to which it compares equal.
	Unlike standard practice, if both numbers are NaN, then they
	compare equal; if only one of the numbers is NaN, then the NaN
	compares greater than the other number if it is negative, and
	smaller than the other number if it is positive. (Note that in
	CFEqual() with two CFNumbers, if either or both of the numbers
	is NaN, FALSE is returned.)
*/
CF_EXPORT
CFComparisonResult CFNumberCompare(CFNumberRef number, CFNumberRef otherNumber, void *context);

#if defined(__cplusplus)
}
#endif

#endif /* ! __COREFOUNDATION_CFNUMBER__ */

