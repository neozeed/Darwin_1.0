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
/*	CFStorage.h
	Copyright 1999-2000, Apple, Inc. All rights reserved.
*/

/*
CFStorage stores an array of arbitrary-sized values. There are no callbacks;
all that is provided about the values is the size, and the appropriate number
of bytes are copied in and out of the CFStorage.

CFStorage uses a balanced tree to store the values, and is most appropriate
for situations where potentially a large number values (more than a hundred
bytes' worth) will be stored and there will be a lot of editing (insertions and deletions).

Getting to an item is O(log n), although caching the last result often reduces this
to a constant time.

The overhead of CFStorage is 48 bytes. There is no per item overhead; the 
non-leaf nodes in the tree cost 20 bytes each, and the worst case extra
capacity (unused space in the leaves) is 12%, typically much less.

Because CFStorage does not necessarily use a single block of memory to store the values,
when you ask for a value, you get back the pointer to the value and optionally
the range of other values that are consecutive and thus reachable as if the
storage was a single block.
*/

#if !defined(__COREFOUNDATION_CFSTORAGE__)
#define __COREFOUNDATION_CFSTORAGE__ 1

#include <CoreFoundation/CFBase.h>

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct __CFStorage *CFStorageRef;

typedef void (*CFStorageApplierFunction)(const void *val, void *context);

/* Create a storage. Each element will take up valueSize bytes.
*/
CF_EXPORT CFStorageRef CFStorageCreate(CFAllocatorRef alloc, CFIndex valueSizeInBytes);

/* Makes space for range.length values at location range.location. Use CFStorageReplaceValues() to set those values.
*/
CF_EXPORT void CFStorageInsertValues(CFStorageRef storage, CFRange range);

/* Deletes the values in the specified range.
*/
CF_EXPORT void CFStorageDeleteValues(CFStorageRef storage, CFRange range);

/* Return the number of values in the storage.
*/
CF_EXPORT CFIndex CFStorageGetCount(CFStorageRef storage);

/* Returns pointer to the specified value. You can use this pointer to get or set the value.
   The range parameter indicates the range of values around the asked value which can be reached 
   linearly. Pass NULL if you don't care about it.
*/
CF_EXPORT void *CFStorageGetValueAtIndex(CFStorageRef storage, CFIndex index, CFRange *validConsecutiveValueRange);

/* Get the values in the specified range into the provided buffer.
*/
CF_EXPORT void CFStorageGetValues(CFStorageRef storage, CFRange range, void *values);

/* Apply the function to each of the values, one at a time. A pointer to the value is given to the apply function.
*/
CF_EXPORT void CFStorageApplyFunction(CFStorageRef storage, CFRange range, CFStorageApplierFunction applier, void *context);

/* Replace the values in the specified range with the provided values.
*/
CF_EXPORT void CFStorageReplaceValues(CFStorageRef storage, CFRange range, const void *values);

/* Private stuff...
*/
CF_EXPORT CFIndex __CFStorageGetCapacity(CFStorageRef storage);
CF_EXPORT CFIndex __CFStorageGetValueSize(CFStorageRef storage);


#if defined(__cplusplus)
}
#endif

#endif /* ! __COREFOUNDATION_CFSTORAGE__ */

