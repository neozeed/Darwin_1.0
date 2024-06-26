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
/*	CFArray.c
	Copyright 1998-2000, Apple, Inc. All rights reserved.
	Responsibility: Christopher Kane
*/

#include <CoreFoundation/CFArray.h>
#include "CFStorage.h"
#include "CFInternal.h"

#if defined(__MACH__) && defined(__ppc__)
// Nasty performance hack: we mark these callbacks as "faulted" on PPC 
const CFArrayCallBacks kCFTypeArrayCallBacks = {0, (void *)((unsigned)__CFTypeCollectionRetain+1), (void *)((unsigned)__CFTypeCollectionRelease+1), (void *)((unsigned)CFCopyDescription+1), (void *)((unsigned)CFEqual+1)};
static const CFArrayCallBacks __kCFNullArrayCallBacks = {0, NULL, NULL, NULL, NULL};
#else
const CFArrayCallBacks kCFTypeArrayCallBacks = {0, __CFTypeCollectionRetain, __CFTypeCollectionRelease, CFCopyDescription, CFEqual};
static const CFArrayCallBacks __kCFNullArrayCallBacks = {0, NULL, NULL, NULL, NULL};
#endif

struct __CFArrayBucket {
    const void *_item;
};

struct __CFArrayImmutable {
    CFRuntimeBase _base;
    CFAllocatorRef _allocator;
    CFIndex _count;	/* number of objects */
};

struct __CFArrayFixedMutable {
    CFRuntimeBase _base;
    CFAllocatorRef _allocator;
    CFIndex _count;	/* number of objects */
    CFIndex _capacity;	/* maximum number of objects */
};

#define __CF_MAX_BUCKETS_PER_DEQUE 254

CF_INLINE CFIndex __CFArrayDequeRoundUpCapacity(CFIndex capacity) {
    if (capacity < 4) return 4;
    return __CFMin((1 << (CFLog2(capacity) + 1)), __CF_MAX_BUCKETS_PER_DEQUE);
}

struct __CFArrayDeque {
    UInt8 _leftIdx;
    UInt8 _capacity;
    UInt8 ___unused2;
    UInt8 ___unused1;
    /* struct __CFArrayBucket buckets follow here */
};

struct __CFArrayMutable {
    CFRuntimeBase _base;
    CFAllocatorRef _allocator;
    CFIndex _count;	/* number of objects */
    void *_store;	/* can be NULL when MutableDeque */
};

/* Flag bits */
enum {		/* Bits 0-1 */
    __kCFArrayImmutable = 0,
    __kCFArrayFixedMutable = 1,
    __kCFArrayMutableDeque = 2,
    __kCFArrayMutableStore = 3
};

enum {		/* Bits 2-3 */
    __kCFArrayHasNullCallBacks = 0,
    __kCFArrayHasCFTypeCallBacks = 1,
    __kCFArrayHasCustomCallBacks = 3	/* callbacks are at end of header */
};

CF_INLINE CFIndex __CFArrayGetType(CFArrayRef array) {
    return __CFBitfieldValue(((CFRuntimeBase *)array)->_info, 1, 0);
}

CF_INLINE CFIndex __CFArrayGetSizeOfType(CFIndex t) {
    CFIndex size = 0;
    switch (__CFBitfieldValue(t, 1, 0)) {
    case __kCFArrayImmutable:
        size += sizeof(struct __CFArrayImmutable);
	break;
    case __kCFArrayFixedMutable:
        size += sizeof(struct __CFArrayFixedMutable);
	break;
    case __kCFArrayMutableDeque:
    case __kCFArrayMutableStore:
        size += sizeof(struct __CFArrayMutable);
	break;
    }
    if (__CFBitfieldValue(t, 3, 2) == __kCFArrayHasCustomCallBacks) {
	size += sizeof(CFArrayCallBacks);
    }
    return size;
}

CF_INLINE CFIndex __CFArrayGetCount(CFArrayRef array) {
    return ((struct __CFArrayImmutable *)array)->_count;
}

CF_INLINE void __CFArraySetCount(CFArrayRef array, CFIndex v) {
    ((struct __CFArrayImmutable *)array)->_count = v;
}

/* Only applies to immutable, fixed-mutable, and mutable-deque-using arrays;
/* Returns the bucket holding the left-most real value in the later case. */
CF_INLINE struct __CFArrayBucket *__CFArrayGetBucketsPtr(CFArrayRef array) {
    switch (__CFArrayGetType(array)) {
    case __kCFArrayImmutable:
    case __kCFArrayFixedMutable:
	return (struct __CFArrayBucket *)((UInt8 *)array + __CFArrayGetSizeOfType(((CFRuntimeBase *)array)->_info));
    case __kCFArrayMutableDeque: {
	struct __CFArrayDeque *deque = ((struct __CFArrayMutable *)array)->_store;
        return (struct __CFArrayBucket *)((UInt8 *)deque + sizeof(struct __CFArrayDeque) + deque->_leftIdx * sizeof(struct __CFArrayBucket));
    }
    }
    return NULL;
}

/* This shouldn't be called if the array count is 0. */
CF_INLINE struct __CFArrayBucket *__CFArrayGetBucketAtIndex(CFArrayRef array, CFIndex idx) {
    switch (__CFArrayGetType(array)) {
    case __kCFArrayImmutable:
    case __kCFArrayFixedMutable:
    case __kCFArrayMutableDeque:
	return __CFArrayGetBucketsPtr(array) + idx;
    case __kCFArrayMutableStore: {
	CFStorageRef store = ((struct __CFArrayMutable *)array)->_store;
	return (struct __CFArrayBucket *)CFStorageGetValueAtIndex(store, idx, NULL);
    }
    }
    return NULL;
}

CF_INLINE const CFArrayCallBacks *__CFArrayGetCallBacks(CFArrayRef array) {
    CFArrayCallBacks *result = NULL;
    switch (__CFBitfieldValue(((CFRuntimeBase *)array)->_info, 3, 2)) {
    case __kCFArrayHasNullCallBacks:
	return &__kCFNullArrayCallBacks;
    case __kCFArrayHasCFTypeCallBacks:
	return &kCFTypeArrayCallBacks;
    case __kCFArrayHasCustomCallBacks:
	break;
    }
    switch (__CFArrayGetType(array)) {
    case __kCFArrayImmutable:
	result = (CFArrayCallBacks *)((UInt8 *)array + sizeof(struct __CFArrayImmutable));
	break;
    case __kCFArrayFixedMutable:
	result = (CFArrayCallBacks *)((UInt8 *)array + sizeof(struct __CFArrayFixedMutable));
	break;
    case __kCFArrayMutableDeque:
    case __kCFArrayMutableStore:
	result = (CFArrayCallBacks *)((UInt8 *)array + sizeof(struct __CFArrayMutable));
	break;
    }
    return result;
}

CF_INLINE Boolean __CFArrayCallBacksMatchNull(const CFArrayCallBacks *c) {
    return (NULL == c ||
	(c->retain == __kCFNullArrayCallBacks.retain &&
	 c->release == __kCFNullArrayCallBacks.release &&
	 c->copyDescription == __kCFNullArrayCallBacks.copyDescription &&
	 c->equal == __kCFNullArrayCallBacks.equal));
}

CF_INLINE Boolean __CFArrayCallBacksMatchCFType(const CFArrayCallBacks *c) {
    return (&kCFTypeArrayCallBacks == c ||
	(c->retain == kCFTypeArrayCallBacks.retain &&
	 c->release == kCFTypeArrayCallBacks.release &&
	 c->copyDescription == kCFTypeArrayCallBacks.copyDescription &&
	 c->equal == kCFTypeArrayCallBacks.equal));
}

struct _releaseContext {
    void (*release)(CFAllocatorRef, const void *);
    CFAllocatorRef allocator; 
};

static void __CFArrayStorageRelease(const void *itemptr, void *context) {
    struct _releaseContext *rc = (struct _releaseContext *)context;
    INVOKE_CALLBACK2(rc->release, rc->allocator, *(const void **)itemptr);
}

static void __CFArrayReleaseValues(CFArrayRef array, CFRange range, Boolean releaseStorageIfPossible) {
    const CFArrayCallBacks *cb = __CFArrayGetCallBacks(array);
    CFAllocatorRef allocator = ((struct __CFArrayImmutable *)array)->_allocator;
    CFIndex idx;
    switch (__CFArrayGetType(array)) {
    case __kCFArrayImmutable:
    case __kCFArrayFixedMutable:
	if (NULL != cb->release && 0 < range.length) {
	    struct __CFArrayBucket *buckets = __CFArrayGetBucketsPtr(array);
	    for (idx = 0; idx < range.length; idx++) {
		INVOKE_CALLBACK2(cb->release, allocator, buckets[idx + range.location]._item);
	    }
	}
	break;
    case __kCFArrayMutableDeque: {
	struct __CFArrayDeque *deque = ((struct __CFArrayMutable *)array)->_store;
	if (NULL != cb->release && 0 < range.length && NULL != deque) {
	    struct __CFArrayBucket *buckets = __CFArrayGetBucketsPtr(array);
	    for (idx = 0; idx < range.length; idx++) {
		INVOKE_CALLBACK2(cb->release, allocator, buckets[idx + range.location]._item);
	    }
	}
	if (releaseStorageIfPossible && 0 == range.location && __CFArrayGetCount(array) == range.length) {
	    if (NULL != deque) CFAllocatorDeallocate(allocator, deque);
	    ((struct __CFArrayMutable *)array)->_store = NULL;
	}
	break;
    }
    case __kCFArrayMutableStore: {
	CFStorageRef store = ((struct __CFArrayMutable *)array)->_store;
	if (NULL != cb->release && 0 < range.length) {
	    struct _releaseContext context;
	    context.release = cb->release;
	    context.allocator = allocator;
	    CFStorageApplyFunction(store, range, __CFArrayStorageRelease, &context);
	}
	if (releaseStorageIfPossible && 0 == range.location && __CFArrayGetCount(array) == range.length) {
	    CFRelease(store);
	    ((struct __CFArrayMutable *)array)->_store = NULL;
	    __CFBitfieldSetValue(((CFRuntimeBase *)array)->_info, 1, 0, __kCFArrayMutableDeque);
	}
	break;
    }
    }
}

CF_INLINE void __CFArrayValidateRange(CFArrayRef array, CFRange range, char *func) {
#if defined(DEBUG)
    CFAssert3(0 <= range.location && range.location <= CFArrayGetCount(array), __kCFLogAssertion, "%s(): range.location index (%d) out of bounds (0, %d)", func, range.location, CFArrayGetCount(array));
    CFAssert2(0 <= range.length, __kCFLogAssertion, "%s(): range.length (%d) cannot be less than zero", func, range.length);
    CFAssert3(range.location + range.length <= CFArrayGetCount(array), __kCFLogAssertion, "%s(): ending index (%d) out of bounds (0, %d)", func, range.location + range.length, CFArrayGetCount(array));
#endif
}

CFTypeID CFArrayGetTypeID(void) {
    return __kCFArrayTypeID;
}

Boolean __CFArrayEqual(CFTypeRef cf1, CFTypeRef cf2) {
    CFArrayRef array1 = (CFArrayRef)cf1;
    CFArrayRef array2 = (CFArrayRef)cf2;
    const CFArrayCallBacks *cb1, *cb2;
    CFIndex idx, cnt;
    if (array1 == array2) return TRUE;
    cnt = __CFArrayGetCount(array1);
    if (cnt != __CFArrayGetCount(array2)) return FALSE;
    cb1 = __CFArrayGetCallBacks(array1);
    cb2 = __CFArrayGetCallBacks(array2);
    if (cb1->equal != cb2->equal) return FALSE;
    if (0 == cnt) return TRUE;	/* after function comparison! */
    for (idx = 0; idx < cnt; idx++) {
	const void *val1 = __CFArrayGetBucketAtIndex(array1, idx)->_item;
	const void *val2 = __CFArrayGetBucketAtIndex(array2, idx)->_item;
	if (val1 != val2 && cb1->equal && !INVOKE_CALLBACK2(cb1->equal, val1, val2)) return FALSE;
    }
    return TRUE;
}

CFHashCode __CFArrayHash(CFTypeRef cf) {
    CFArrayRef array = (CFArrayRef)cf;
    return __CFArrayGetCount(array);
}

CFStringRef __CFArrayCopyDescription(CFTypeRef cf) {
    CFArrayRef array = (CFArrayRef)cf;
    CFMutableStringRef result;
    const CFArrayCallBacks *cb;
    CFAllocatorRef allocator;
    CFIndex idx, cnt;
    cnt = __CFArrayGetCount(array);
    allocator = ((struct __CFArrayImmutable *)array)->_allocator;
    result = CFStringCreateMutable(allocator, 0);
    switch (__CFArrayGetType(array)) {
    case __kCFArrayImmutable:
	CFStringAppendFormat(result, NULL, CFSTR("<CFArray %p [%p]>{type = immutable, count = %u, values = (\n"), cf, allocator, cnt);
	break;
    case __kCFArrayFixedMutable:
	CFStringAppendFormat(result, NULL, CFSTR("<CFArray %p [%p]>{type = fixed-mutable, count = %u, capacity = %u, values = (\n"), cf, allocator, cnt, ((struct __CFArrayFixedMutable *)array)->_capacity);
	break;
    case __kCFArrayMutableDeque:
	CFStringAppendFormat(result, NULL, CFSTR("<CFArray %p [%p]>{type = mutable-small, count = %u, values = (\n"), cf, allocator, cnt);
	break;
    case __kCFArrayMutableStore:
	CFStringAppendFormat(result, NULL, CFSTR("<CFArray %p [%p]>{type = mutable-large, count = %u, values = (\n"), cf, allocator, cnt);
	break;
    }
    cb = __CFArrayGetCallBacks(array);
    for (idx = 0; idx < cnt; idx++) {
	CFStringRef desc = NULL;
	const void *val = __CFArrayGetBucketAtIndex(array, idx)->_item;
	if (NULL != cb->copyDescription) {
	    desc = (CFStringRef)INVOKE_CALLBACK1(cb->copyDescription, val);
	}
	if (NULL != desc) {
	    CFStringAppendFormat(result, NULL, CFSTR("\t%u : %@\n"), idx, desc);
	    CFRelease(desc);
	} else {
	    CFStringAppendFormat(result, NULL, CFSTR("\t%u : <%p>\n"), idx, val);
	}
    }
    CFStringAppend(result, CFSTR(")}"));
    return result;
}

CFAllocatorRef __CFArrayGetAllocator(CFTypeRef cf) {
    CFArrayRef array = (CFArrayRef)cf;
    return ((struct __CFArrayImmutable *)array)->_allocator;
}

void __CFArrayDeallocate(CFTypeRef cf) {
    CFArrayRef array = (CFArrayRef)cf;
    CFAllocatorRef allocator = ((struct __CFArrayImmutable *)array)->_allocator;
    __CFArrayReleaseValues(array, CFRangeMake(0, __CFArrayGetCount(array)), TRUE);
    __CFArraySetCount(array, 0);
    CFAllocatorDeallocate(allocator, (void *)array);
    CFRelease(allocator);
}

static CFArrayRef __CFArrayInit(CFAllocatorRef allocator, UInt32 flags, CFIndex capacity, const CFArrayCallBacks *callBacks) {
    struct __CFArrayImmutable *memory;
    UInt32 size;
    __CFBitfieldSetValue(flags, 31, 2, 0);
    if (__CFArrayCallBacksMatchNull(callBacks)) {
	__CFBitfieldSetValue(flags, 3, 2, __kCFArrayHasNullCallBacks);
    } else if (__CFArrayCallBacksMatchCFType(callBacks)) {
	__CFBitfieldSetValue(flags, 3, 2, __kCFArrayHasCFTypeCallBacks);
    } else {
	__CFBitfieldSetValue(flags, 3, 2, __kCFArrayHasCustomCallBacks);
    }
    size = __CFArrayGetSizeOfType(flags);
    switch (__CFBitfieldValue(flags, 1, 0)) {
    case __kCFArrayImmutable:
    case __kCFArrayFixedMutable:
	size += capacity * sizeof(struct __CFArrayBucket);
	break;
    case __kCFArrayMutableDeque:
    case __kCFArrayMutableStore:
	break;
    }
    allocator = (NULL == allocator) ? CFRetain(CFAllocatorGetDefault()) : CFRetain(allocator);
    memory = CFAllocatorAllocate(allocator, size, 0);
    if (NULL == memory) {
	CFRelease(allocator);
	return NULL;
    }
    __CFGenericInitBase(memory, __CFArrayIsa(), __kCFArrayTypeID);
    __CFBitfieldSetValue(memory->_base._info, 7, 0, flags);
    memory->_allocator = allocator;
    __CFArraySetCount((CFArrayRef)memory, 0);
    switch (__CFBitfieldValue(flags, 1, 0)) {
    case __kCFArrayImmutable:
	__CFSetLastAllocationEventName(memory, "CFArray (immutable)");
	break;
    case __kCFArrayFixedMutable:
	__CFSetLastAllocationEventName(memory, "CFArray (mutable-fixed)");
	((struct __CFArrayFixedMutable *)memory)->_capacity = capacity;
	break;
    case __kCFArrayMutableDeque:
    case __kCFArrayMutableStore:
	__CFSetLastAllocationEventName(memory, "CFArray (mutable-variable)");
	((struct __CFArrayMutable *)memory)->_store = NULL;
	break;
    }
    if (__kCFArrayHasCustomCallBacks == __CFBitfieldValue(flags, 3, 2)) {
	const CFArrayCallBacks *cb;
	*(CFArrayCallBacks *)__CFArrayGetCallBacks((CFArrayRef)memory) = *callBacks;
	cb = __CFArrayGetCallBacks((CFArrayRef)memory);
	FAULT_CALLBACK((void **)&(cb->retain));
	FAULT_CALLBACK((void **)&(cb->release));
	FAULT_CALLBACK((void **)&(cb->copyDescription));
	FAULT_CALLBACK((void **)&(cb->equal));
    }
    return (CFArrayRef)memory;
}

CFArrayRef CFArrayCreate(CFAllocatorRef allocator, const void **values, CFIndex numValues, const CFArrayCallBacks *callBacks) {
    CFArrayRef result;
    const CFArrayCallBacks *cb;
    struct __CFArrayBucket *buckets;
    CFIndex idx;
    CFAssert2(0 <= numValues, __kCFLogAssertion, "%s(): numValues (%d) cannot be less than zero", __PRETTY_FUNCTION__, numValues);
    result = __CFArrayInit(allocator, __kCFArrayImmutable, numValues, callBacks);
    cb = __CFArrayGetCallBacks(result);
    buckets = __CFArrayGetBucketsPtr(result);
    for (idx = 0; idx < numValues; idx++) {
	if (NULL != cb->retain) {
	    buckets->_item = (void *)INVOKE_CALLBACK2(cb->retain, allocator, *values);
	} else {
	    buckets->_item = *values;
	}
	values++;
	buckets++;
    }
    __CFArraySetCount(result, numValues);
    return result;
}

CFMutableArrayRef CFArrayCreateMutable(CFAllocatorRef allocator, CFIndex capacity, const CFArrayCallBacks *callBacks) {
    CFAssert2(0 <= capacity, __kCFLogAssertion, "%s(): capacity (%d) cannot be less than zero", __PRETTY_FUNCTION__, capacity);
    return (CFMutableArrayRef)__CFArrayInit(allocator, (0 == capacity) ? __kCFArrayMutableDeque : __kCFArrayFixedMutable, capacity, callBacks);
}

CFArrayRef CFArrayCreateCopy(CFAllocatorRef allocator, CFArrayRef array) {
    CFArrayRef result;
    const CFArrayCallBacks *cb;
    struct __CFArrayBucket *buckets;
    CFIndex numValues = CFArrayGetCount(array);
    CFIndex idx;
    cb = CF_IS_OBJC(Array, array) ? &kCFTypeArrayCallBacks : __CFArrayGetCallBacks(array);
    result = __CFArrayInit(allocator, __kCFArrayImmutable, numValues, cb);
    buckets = __CFArrayGetBucketsPtr(result);
    for (idx = 0; idx < numValues; idx++) {
	const void *value = CFArrayGetValueAtIndex(array, idx);
	if (NULL != cb->retain) {
	    value = (void *)INVOKE_CALLBACK2(cb->retain, allocator, value);
	}
	buckets->_item = value;
	buckets++;
    }
    __CFArraySetCount(result, numValues);
    return result;
}

CFMutableArrayRef CFArrayCreateMutableCopy(CFAllocatorRef allocator, CFIndex capacity, CFArrayRef array) {
    CFMutableArrayRef result;
    const CFArrayCallBacks *cb;
    CFIndex idx, numValues = CFArrayGetCount(array);
    UInt32 flags;
    CFAssert3(0 == capacity || numValues <= capacity, __kCFLogAssertion, "%s(): for fixed-mutable arrays, capacity (%d) must be greater than or equal to initial number of values (%d)", __PRETTY_FUNCTION__, capacity, numValues);
    cb = CF_IS_OBJC(Array, array) ? &kCFTypeArrayCallBacks : __CFArrayGetCallBacks(array);
    flags = (0 == capacity) ? __kCFArrayMutableDeque : __kCFArrayFixedMutable;
    result = (CFMutableArrayRef)__CFArrayInit(allocator, flags, capacity, cb);
    for (idx = 0; idx < numValues; idx++) {
	const void *value = CFArrayGetValueAtIndex(array, idx);
	CFArrayAppendValue(result, value);
    }
    return result;
}

CFIndex CFArrayGetCount(CFArrayRef array) {
    CF_OBJC_FUNCDISPATCH0(Array, CFIndex, array, "count");
    __CFGenericValidateType(array, __kCFArrayTypeID);
    return __CFArrayGetCount(array);
}

CFIndex CFArrayGetCountOfValue(CFArrayRef array, CFRange range, const void *value) {
    const CFArrayCallBacks *cb;
    CFIndex idx, count = 0;
#warning CF: this ignores range
    CF_OBJC_FUNCDISPATCH1(Array, CFIndex, array, "countOccurrences:", (id)value);
#if defined(DEBUG)
    __CFGenericValidateType(array, __kCFArrayTypeID);    
    __CFArrayValidateRange(array, range, __PRETTY_FUNCTION__);
#endif
    cb = __CFArrayGetCallBacks(array);
    for (idx = 0; idx < range.length; idx++) {
	const void *item = __CFArrayGetBucketAtIndex(array, range.location + idx)->_item;
	if (value == item || (cb->equal && INVOKE_CALLBACK2(cb->equal, value, item))) {
	    count++;
	}
    }
    return count;
}

Boolean CFArrayContainsValue(CFArrayRef array, CFRange range, const void *value) {
    const CFArrayCallBacks *cb;
    CFIndex idx;
    CF_OBJC_FUNCDISPATCH2(Array, Boolean, array, "containsObject:inRange:", (id)value, range);
#if defined(DEBUG)
    __CFGenericValidateType(array, __kCFArrayTypeID);
    __CFArrayValidateRange(array, range, __PRETTY_FUNCTION__);
#endif
    cb = __CFArrayGetCallBacks(array);
    for (idx = 0; idx < range.length; idx++) {
	const void *item = __CFArrayGetBucketAtIndex(array, range.location + idx)->_item;
	if (value == item || (cb->equal && INVOKE_CALLBACK2(cb->equal, value, item))) {
	    return TRUE;
	}
    }
    return FALSE;
}

const void *CFArrayGetValueAtIndex(CFArrayRef array, CFIndex idx) {
    CF_OBJC_FUNCDISPATCH1(Array, void *, array, "objectAtIndex:", idx);
    __CFGenericValidateType(array, __kCFArrayTypeID);
    CFAssert2(0 <= idx && idx < __CFArrayGetCount(array), __kCFLogAssertion, "%s(): index (%d) out of bounds", __PRETTY_FUNCTION__, idx);
    return __CFArrayGetBucketAtIndex(array, idx)->_item;
}

void CFArrayGetValues(CFArrayRef array, CFRange range, const void **values) {
    CF_OBJC_FUNCDISPATCH2(Array, void, array, "getObjects:inRange:", (id *)values, range);
#if defined(DEBUG)
    __CFGenericValidateType(array, __kCFArrayTypeID);
    __CFArrayValidateRange(array, range, __PRETTY_FUNCTION__);
    CFAssert1(NULL != values, __kCFLogAssertion, "%s(): pointer to values may not be NULL", __PRETTY_FUNCTION__);
#endif
    if (0 < range.length) {
	switch (__CFArrayGetType(array)) {
	case __kCFArrayImmutable:
	case __kCFArrayFixedMutable: 
	case __kCFArrayMutableDeque:
	    CFMoveMemory(values, __CFArrayGetBucketsPtr(array) + range.location, range.length * sizeof(struct __CFArrayBucket));
	    break;
	case __kCFArrayMutableStore: {
	    CFStorageRef store = ((struct __CFArrayMutable *)array)->_store;
	    CFStorageGetValues(store, range, values);
	    break;
	}
	}
    }
}

void CFArrayApplyFunction(CFArrayRef array, CFRange range, CFArrayApplierFunction applier, void *context) {
    CFIndex idx;
    FAULT_CALLBACK((void **)&(applier));
    CF_OBJC_FUNCDISPATCH2(Array, void, array, "apply:context:", (void (*)())applier, context);
#if defined(DEBUG)
    __CFGenericValidateType(array, __kCFArrayTypeID);
    __CFArrayValidateRange(array, range, __PRETTY_FUNCTION__);
    CFAssert1(NULL != applier, __kCFLogAssertion, "%s(): pointer to applier function may not be NULL", __PRETTY_FUNCTION__);
#endif
    for (idx = 0; idx < range.length; idx++) {
	const void *item = __CFArrayGetBucketAtIndex(array, range.location + idx)->_item;
	INVOKE_CALLBACK2(applier, item, context);
    }
}

CFIndex CFArrayGetFirstIndexOfValue(CFArrayRef array, CFRange range, const void *value) {
    const CFArrayCallBacks *cb;
    CFIndex idx;
    CF_OBJC_FUNCDISPATCH2(Array, CFIndex, array, "indexOfObject:inRange:", (id)value, range);
#if defined(DEBUG)
    __CFGenericValidateType(array, __kCFArrayTypeID);
    __CFArrayValidateRange(array, range, __PRETTY_FUNCTION__);
#endif
    cb = __CFArrayGetCallBacks(array);
    for (idx = 0; idx < range.length; idx++) {
	const void *item = __CFArrayGetBucketAtIndex(array, range.location + idx)->_item;
	if (value == item || (cb->equal && INVOKE_CALLBACK2(cb->equal, value, item)))
	    return idx + range.location;
    }
    return -1;
}

CFIndex CFArrayGetLastIndexOfValue(CFArrayRef array, CFRange range, const void *value) {
    const CFArrayCallBacks *cb;
    CFIndex idx;
    CF_OBJC_FUNCDISPATCH2(Array, CFIndex, array, "lastIndexOfObject:inRange:", (id)value, range);
#if defined(DEBUG)
    __CFGenericValidateType(array, __kCFArrayTypeID);
    __CFArrayValidateRange(array, range, __PRETTY_FUNCTION__);
#endif
    cb = __CFArrayGetCallBacks(array);
    for (idx = range.length; idx--;) {
	const void *item = __CFArrayGetBucketAtIndex(array, range.location + idx)->_item;
	if (value == item || (cb->equal && INVOKE_CALLBACK2(cb->equal, value, item)))
	    return idx + range.location;
    }
    return -1;
}

void CFArrayAppendValue(CFMutableArrayRef array, const void *value) {
    CF_OBJC_FUNCDISPATCH1(Array, void, array, "addObject:", (id)value);
    __CFGenericValidateType(array, __kCFArrayTypeID);
    CFAssert1(__CFArrayGetType(array) != __kCFArrayImmutable, __kCFLogAssertion, "%s(): array is immutable", __PRETTY_FUNCTION__);
    CFArrayReplaceValues(array, CFRangeMake(__CFArrayGetCount(array), 0), &value, 1);
}

void CFArraySetValueAtIndex(CFMutableArrayRef array, CFIndex idx, const void *value) {
    CF_OBJC_FUNCDISPATCH2(Array, void, array, "setObject:atIndex:", (id)value, idx);
    __CFGenericValidateType(array, __kCFArrayTypeID);
    CFAssert1(__CFArrayGetType(array) != __kCFArrayImmutable, __kCFLogAssertion, "%s(): array is immutable", __PRETTY_FUNCTION__);
    CFAssert2(0 <= idx && idx <= __CFArrayGetCount(array), __kCFLogAssertion, "%s(): index (%d) out of bounds", __PRETTY_FUNCTION__, idx);
    if (idx == __CFArrayGetCount(array)) {
	CFArrayReplaceValues(array, CFRangeMake(idx, 0), &value, 1);
    } else {
	CFArrayReplaceValues(array, CFRangeMake(idx, 1), &value, 1);
    }
}

void CFArrayInsertValueAtIndex(CFMutableArrayRef array, CFIndex idx, const void *value) {
    CF_OBJC_FUNCDISPATCH2(Array, void, array, "insertObject:atIndex:", (id)value, idx);
    __CFGenericValidateType(array, __kCFArrayTypeID);
    CFAssert1(__CFArrayGetType(array) != __kCFArrayImmutable, __kCFLogAssertion, "%s(): array is immutable", __PRETTY_FUNCTION__);
    CFAssert2(0 <= idx && idx <= __CFArrayGetCount(array), __kCFLogAssertion, "%s(): index (%d) out of bounds", __PRETTY_FUNCTION__, idx);
    CFArrayReplaceValues(array, CFRangeMake(idx, 0), &value, 1);
}

void CFArrayExchangeValuesAtIndices(CFMutableArrayRef array, CFIndex idx1, CFIndex idx2) {
    const void *tmp;
    CF_OBJC_FUNCDISPATCH2(Array, void, array, "exchange::", idx1, idx2);
    __CFGenericValidateType(array, __kCFArrayTypeID);
    CFAssert2(0 <= idx1 && idx1 < __CFArrayGetCount(array), __kCFLogAssertion, "%s(): index #1 (%d) out of bounds", __PRETTY_FUNCTION__, idx1);
    CFAssert2(0 <= idx2 && idx2 < __CFArrayGetCount(array), __kCFLogAssertion, "%s(): index #2 (%d) out of bounds", __PRETTY_FUNCTION__, idx2);
    CFAssert1(__CFArrayGetType(array) != __kCFArrayImmutable, __kCFLogAssertion, "%s(): array is immutable", __PRETTY_FUNCTION__);
    tmp = __CFArrayGetBucketAtIndex(array, idx1)->_item;
    __CFArrayGetBucketAtIndex(array, idx1)->_item = __CFArrayGetBucketAtIndex(array, idx2)->_item;
    __CFArrayGetBucketAtIndex(array, idx2)->_item = tmp;
}

void CFArrayRemoveValueAtIndex(CFMutableArrayRef array, CFIndex idx) {
    CF_OBJC_FUNCDISPATCH1(Array, void, array, "removeObjectAtIndex:", idx);
    __CFGenericValidateType(array, __kCFArrayTypeID);
    CFAssert1(__CFArrayGetType(array) != __kCFArrayImmutable, __kCFLogAssertion, "%s(): array is immutable", __PRETTY_FUNCTION__);
    CFAssert2(0 <= idx && idx < __CFArrayGetCount(array), __kCFLogAssertion, "%s(): index (%d) out of bounds", __PRETTY_FUNCTION__, idx);
    CFArrayReplaceValues(array, CFRangeMake(idx, 1), NULL, 0);
}

void CFArrayRemoveAllValues(CFMutableArrayRef array) {
    CF_OBJC_FUNCDISPATCH0(Array, void, array, "removeAllObjects");
    __CFGenericValidateType(array, __kCFArrayTypeID);
    CFAssert1(__CFArrayGetType(array) != __kCFArrayImmutable, __kCFLogAssertion, "%s(): array is immutable", __PRETTY_FUNCTION__);
    __CFArrayReleaseValues(array, CFRangeMake(0, __CFArrayGetCount(array)), TRUE);
    __CFArraySetCount(array, 0);
}

static void __CFArrayConvertDequeToStore(CFMutableArrayRef array) {
    struct __CFArrayDeque *deque = ((struct __CFArrayMutable *)array)->_store;
    struct __CFArrayBucket *raw_buckets = (struct __CFArrayBucket *)((UInt8 *)deque + sizeof(struct __CFArrayDeque));
    CFStorageRef store;
    CFIndex count = CFArrayGetCount(array);
    store = CFStorageCreate(((struct __CFArrayImmutable *)array)->_allocator, sizeof(const void *));
    __CFSetLastAllocationEventName(store, "CFArray (store-storage)");
    ((struct __CFArrayMutable *)array)->_store = store;
    CFStorageInsertValues(store, CFRangeMake(0, count));
    CFStorageReplaceValues(store, CFRangeMake(0, count), raw_buckets + deque->_leftIdx);
    CFAllocatorDeallocate(((struct __CFArrayImmutable *)array)->_allocator, deque);
    __CFBitfieldSetValue(((CFRuntimeBase *)array)->_info, 1, 0, __kCFArrayMutableStore);
}

static void __CFArrayConvertStoreToDeque(CFMutableArrayRef array) {
    CFStorageRef store = ((struct __CFArrayMutable *)array)->_store;
    struct __CFArrayDeque *deque;
    struct __CFArrayBucket *raw_buckets;
    CFIndex count = CFStorageGetCount(store);// storage, not array, has correct count at this point
    // count is typically 127 here, which makes for a nearly full deque, which may realloc larger quickly -- should fix to not make the deque so tight when converting down, someday (maybe make the shrink condition smaller?)
    CFIndex capacity = __CFArrayDequeRoundUpCapacity(count);
    CFIndex size = sizeof(struct __CFArrayDeque) + capacity * sizeof(struct __CFArrayBucket);
    deque = CFAllocatorAllocate(((struct __CFArrayImmutable *)array)->_allocator, size, 0);
    __CFSetLastAllocationEventName(deque, "CFArray (store-deque)");
    deque->_leftIdx = (capacity - count) / 2;
    deque->_capacity = capacity;
    ((struct __CFArrayMutable *)array)->_store = deque;
    raw_buckets = (struct __CFArrayBucket *)((UInt8 *)deque + sizeof(struct __CFArrayDeque));
    CFStorageGetValues(store, CFRangeMake(0, count), raw_buckets + deque->_leftIdx);
    CFRelease(store);
    __CFBitfieldSetValue(((CFRuntimeBase *)array)->_info, 1, 0, __kCFArrayMutableDeque);
}

// may move deque storage, as it may need to grow deque
static void __CFArrayRepositionDequeRegions(CFMutableArrayRef array, CFRange range, CFIndex newCount) {
    // newCount elements are going to replace the range, and the result will fit in the deque
    // range in always refers to _used_ area (A + B + C)
    struct __CFArrayDeque *deque = ((struct __CFArrayMutable *)array)->_store;
    struct __CFArrayBucket *raw_buckets;
    CFIndex X, Y, A, B, C;
    CFIndex futureCnt = __CFArrayGetCount(array) - range.length + newCount;

    if (deque->_capacity < futureCnt) {
	CFIndex capacity = __CFArrayDequeRoundUpCapacity(futureCnt);
	CFIndex size = sizeof(struct __CFArrayDeque) + capacity * sizeof(struct __CFArrayBucket);
	deque = CFAllocatorReallocate(((struct __CFArrayImmutable *)array)->_allocator, deque, size, 0);
	deque->_capacity = capacity;
	((struct __CFArrayMutable *)array)->_store = deque;
    }
    X = deque->_leftIdx;
    A = range.location;
    B = range.length;
    C = __CFArrayGetCount(array) - B - A;
    Y = deque->_capacity - C - B - A - X;
    raw_buckets = (struct __CFArrayBucket *)((UInt8 *)deque + sizeof(struct __CFArrayDeque));
    if ((newCount < B && A < C) || (B <= newCount && (0 == A || Y <= X) && (newCount - B) <= X)) {
	// deleting: A is smaller, move it
	// inserting: X is big enough, just move A
	// new beginning of A is beginning of C minus length of A and new B
	CFIndex left = X + A + B - newCount - A;
	if (0 < A)
	    CFMoveMemory(raw_buckets + left, raw_buckets + X, A * sizeof(struct __CFArrayBucket));
	deque->_leftIdx = left;
    } else if ((newCount < B && C <= A) || (B <= newCount && (0 == C || X < Y) && (newCount - B) <= Y)) {
	// deleting: C is smaller, move it
	// inserting: Y is big enough, just move C
	// new beginning of C is end of A plus new B
	if (0 < C)
	    CFMoveMemory(raw_buckets + X + A + newCount, raw_buckets + X + A + B, C * sizeof(struct __CFArrayBucket));
    } else {
	// must be inserting, and neither X nor Y is large enough alone
	// have to move both regions, might as well re-center everything
        CFIndex left = (deque->_capacity - futureCnt) / 2;
	// invariant: left < X;  if that's not true, than Y must be large enough
	// move A left and C right
	if (0 < A)
	    CFMoveMemory(raw_buckets + left, raw_buckets + X, A * sizeof(struct __CFArrayBucket));
	if (0 < C)
	    CFMoveMemory(raw_buckets + left + A + newCount, raw_buckets + X + A + B, C * sizeof(struct __CFArrayBucket));
	deque->_leftIdx = left;
    }
}

void CFArrayReplaceValues(CFMutableArrayRef array, CFRange range, const void **newValues, CFIndex newCount) {
    const CFArrayCallBacks *cb;
    CFAllocatorRef allocator;
    CFIndex idx, cnt, futureCnt;
    const void **newv, *buffer[256];
    CF_OBJC_FUNCDISPATCH3(Array, void, array, "replaceObjectsInRange:withObjects:count:", range, (id *)newValues, newCount);
#if defined(DEBUG)
    __CFGenericValidateType(array, __kCFArrayTypeID);
    __CFArrayValidateRange(array, range, __PRETTY_FUNCTION__);
#endif
    CFAssert1(__CFArrayGetType(array) != __kCFArrayImmutable, __kCFLogAssertion, "%s(): array is immutable", __PRETTY_FUNCTION__);
    CFAssert2(0 <= newCount, __kCFLogAssertion, "%s(): newCount (%d) cannot be less than zero", __PRETTY_FUNCTION__, newCount);
    cnt = __CFArrayGetCount(array);
    futureCnt = cnt - range.length + newCount;
    CFAssert1((__kCFArrayFixedMutable != __CFArrayGetType(array)) || (futureCnt <= ((struct __CFArrayFixedMutable *)array)->_capacity), __kCFLogAssertion, "%s(): fixed-capacity array is full (or will overflow)", __PRETTY_FUNCTION__);
    CFAssert1(newCount <= futureCnt, __kCFLogAssertion, "%s(): internal error 1", __PRETTY_FUNCTION__);
    cb = __CFArrayGetCallBacks(array);
    allocator = ((struct __CFArrayImmutable *)array)->_allocator;
    /* Allocate a temporary copy of the values, and retain them */
    newv = (newCount <= 256) ? buffer : CFAllocatorAllocate(allocator, newCount * sizeof(void *), 0);
    if (newv != buffer) __CFSetLastAllocationEventName(newv, "CFArray (temp)");
    if (0 < newCount)
	CFMoveMemory(newv, newValues, newCount * sizeof(void *));
    if (NULL != cb->retain) {
	for (idx = 0; idx < newCount; idx++) {
	    newv[idx] = (void *)INVOKE_CALLBACK2(cb->retain, allocator, (void *)newv[idx]);
	}
    }
    /* Now, there are three regions of interest, each of which may be empty:
     *   A: the region from index 0 to one less than the range.location
     *   B: the region of the range
     *   C: the region from range.location + range.length to the end
     * Note that index 0 is not necessarily at the lowest-address edge
     * of the available storage. The values in region B need to get
     * released, and the values in regions A and C (depending) need
     * to get shifted if the number of new values is different from
     * the length of the range being replaced.
     */
    if (__kCFArrayFixedMutable == __CFArrayGetType(array)) {
	struct __CFArrayBucket *buckets = __CFArrayGetBucketsPtr(array);
#warning CF: we should treat a fixed mutable array like a deque too
	__CFArrayReleaseValues(array, range, FALSE);
	if (newCount != range.length && range.location + range.length < cnt) {
	    /* This neatly moves region C in the proper direction */
	    CFMoveMemory(buckets + range.location + newCount, buckets + range.location + range.length, (cnt - range.location - range.length) * sizeof(struct __CFArrayBucket));
	}
	if (0 < newCount) {
	    CFMoveMemory(buckets + range.location, newv, newCount * sizeof(void *));
	}
	__CFArraySetCount(array, futureCnt);
	if (newv != buffer) CFAllocatorDeallocate(allocator, newv);
	return;
    }
    __CFArrayReleaseValues(array, range, FALSE);
    // region B elements are now "dead"
    if (__kCFArrayMutableStore == __CFArrayGetType(array)) {
        CFStorageRef store = ((struct __CFArrayMutable *)array)->_store;
	// reposition regions A and C for new region B elements in gap
        if (range.length < newCount) {
            CFStorageInsertValues(store, CFRangeMake(range.location + range.length, newCount - range.length));
        } else if (newCount < range.length) {
            CFStorageDeleteValues(store, CFRangeMake(range.location + newCount, range.length - newCount));
        }
	if (futureCnt <= __CF_MAX_BUCKETS_PER_DEQUE / 2) {
	    __CFArrayConvertStoreToDeque(array);
	}
    } else if (NULL == ((struct __CFArrayMutable *)array)->_store) {
	if (__CF_MAX_BUCKETS_PER_DEQUE <= futureCnt) {
	    CFStorageRef store = CFStorageCreate(allocator, sizeof(const void *));
	    __CFSetLastAllocationEventName(store, "CFArray (store-storage)");
	    ((struct __CFArrayMutable *)array)->_store = store;
            CFStorageInsertValues(store, CFRangeMake(0, newCount));
	} else if (0 <= futureCnt) {
	    struct __CFArrayDeque *deque;
	    CFIndex capacity = __CFArrayDequeRoundUpCapacity(futureCnt);
	    CFIndex size = sizeof(struct __CFArrayDeque) + capacity * sizeof(struct __CFArrayBucket);
	    deque = CFAllocatorAllocate(allocator, size, 0);
	    __CFSetLastAllocationEventName(deque, "CFArray (store-deque)");
	    deque->_leftIdx = (capacity - newCount) / 2;
	    deque->_capacity = capacity;
	    ((struct __CFArrayMutable *)array)->_store = deque;
	}
    } else {		// Deque
	// reposition regions A and C for new region B elements in gap
	if (__CF_MAX_BUCKETS_PER_DEQUE <= futureCnt) {
	    CFStorageRef store;
	    __CFArrayConvertDequeToStore(array);
	    store = ((struct __CFArrayMutable *)array)->_store;
	    if (range.length < newCount) {
		CFStorageInsertValues(store, CFRangeMake(range.location + range.length, newCount - range.length));
	    } else if (newCount < range.length) { // this won't happen, but is here for completeness
		CFStorageDeleteValues(store, CFRangeMake(range.location + newCount, range.length - newCount));
	    }
	} else if (range.length != newCount) {
	    __CFArrayRepositionDequeRegions(array, range, newCount);
	}
    }
    // copy in new region B elements
    if (0 < newCount) {
	if (__kCFArrayMutableStore == __CFArrayGetType(array)) {
	    CFStorageRef store = ((struct __CFArrayMutable *)array)->_store;
	    CFStorageReplaceValues(store, CFRangeMake(range.location, newCount), newv);
	} else {	// Deque
	    struct __CFArrayDeque *deque = ((struct __CFArrayMutable *)array)->_store;
	    struct __CFArrayBucket *raw_buckets = (struct __CFArrayBucket *)((UInt8 *)deque + sizeof(struct __CFArrayDeque));
	    CFMoveMemory(raw_buckets + deque->_leftIdx + range.location, newv, newCount * sizeof(struct __CFArrayBucket));
	}
    }
    __CFArraySetCount(array, futureCnt);
    if (newv != buffer) CFAllocatorDeallocate(allocator, newv);
}

struct _compareContext {
    CFComparatorFunction func;
    void *context;
};

static CFComparisonResult __CFArrayCompareValues(const void *v1, const void *v2, struct _compareContext *context) {
    const void **val1 = (const void **)v1;
    const void **val2 = (const void **)v2;
    return (CFComparisonResult)(INVOKE_CALLBACK3(context->func, *val1, *val2, context->context));
}

void CFArraySortValues(CFMutableArrayRef array, CFRange range, CFComparatorFunction comparator, void *context) {
    FAULT_CALLBACK((void **)&(comparator));
    CF_OBJC_FUNCDISPATCH3(Array, void, array, "sortUsingFunction:context:range:", (int (*)())comparator, context, range);
#if defined(DEBUG)
    __CFGenericValidateType(array, __kCFArrayTypeID);
    __CFArrayValidateRange(array, range, __PRETTY_FUNCTION__);
#endif
    CFAssert1(__CFArrayGetType(array) != __kCFArrayImmutable, __kCFLogAssertion, "%s(): array is immutable", __PRETTY_FUNCTION__);
    CFAssert1(NULL != comparator, __kCFLogAssertion, "%s(): pointer to comparator function may not be NULL", __PRETTY_FUNCTION__);
    if (0 < range.length) {
	struct _compareContext ctx;
	ctx.func = comparator;
	ctx.context = context;
	switch (__CFArrayGetType(array)) {
	case __kCFArrayFixedMutable:
	case __kCFArrayMutableDeque:
	    CFQSortArray(__CFArrayGetBucketsPtr(array) + range.location, range.length, sizeof(void *), (CFComparatorFunction)__CFArrayCompareValues, &ctx);
	    break;
	case __kCFArrayMutableStore: {
	    CFStorageRef store = ((struct __CFArrayMutable *)array)->_store;
	    CFAllocatorRef allocator = ((struct __CFArrayMutable *)array)->_allocator;
	    const void **values, *buffer[256];
	    values = (range.length <= 256) ? buffer : CFAllocatorAllocate(allocator, range.length * sizeof(void *), 0);
	    if (values != buffer) __CFSetLastAllocationEventName(values, "CFArray (temp)");
	    CFStorageGetValues(store, range, values);
	    CFQSortArray(values, range.length, sizeof(void *), (CFComparatorFunction)__CFArrayCompareValues, &ctx);
	    CFStorageReplaceValues(store, range, values);
	    if (values != buffer) CFAllocatorDeallocate(allocator, values);
	    break;
	}
	}
    }
}

CFIndex CFArrayBSearchValues(CFArrayRef array, CFRange range, const void *value, CFComparatorFunction comparator, void *context) {
    Boolean isObjC = CF_IS_OBJC(Array, array);
    CFIndex idx = 0;
    FAULT_CALLBACK((void **)&(comparator));
    if (!isObjC) {
#if defined(DEBUG)
	__CFGenericValidateType(array, __kCFArrayTypeID);
	__CFArrayValidateRange(array, range, __PRETTY_FUNCTION__);
#endif
    }
    CFAssert1(NULL != comparator, __kCFLogAssertion, "%s(): pointer to comparator function may not be NULL", __PRETTY_FUNCTION__);
    if (range.length <= 0) return range.location;
    if (isObjC || __kCFArrayMutableStore == __CFArrayGetType(array)) {
	const void *item;
	SInt32 lg;
	item = CFArrayGetValueAtIndex(array, range.location + range.length - 1);
	if ((CFComparisonResult)(INVOKE_CALLBACK3(comparator, item, value, context)) < 0) {
	    return range.location + range.length;
	}
	item = CFArrayGetValueAtIndex(array, range.location);
	if ((CFComparisonResult)(INVOKE_CALLBACK3(comparator, value, item, context)) < 0) {
	    return range.location;
	}
	lg = CFLog2(range.length);
	item = CFArrayGetValueAtIndex(array, range.location + -1 + (1 << lg));
	idx = range.location + ((CFComparisonResult)(INVOKE_CALLBACK3(comparator, item, value, context)) < 0) ? range.length - (1 << lg) : -1;
        while (lg--) {
	    const void *item = CFArrayGetValueAtIndex(array, range.location + idx + (1 << lg));
	    if ((CFComparisonResult)(INVOKE_CALLBACK3(comparator, item, value, context)) < 0) {
		idx += (1 << lg);
	    }
	}
	idx++;
    } else {
	struct _compareContext ctx;
	ctx.func = comparator;
	ctx.context = context;
	idx = CFBSearch(&value, sizeof(void *), __CFArrayGetBucketsPtr(array) + range.location, range.length, (CFComparatorFunction)__CFArrayCompareValues, &ctx);
    }
    return idx + range.location;
}

void CFArrayAppendArray(CFMutableArrayRef array, CFArrayRef otherArray, CFRange otherRange) {
    CFIndex idx;
#if defined(DEBUG)
    __CFGenericValidateType(array, __kCFArrayTypeID);
    __CFGenericValidateType(otherArray, __kCFArrayTypeID);
    CFAssert1(__CFArrayGetType(array) != __kCFArrayImmutable, __kCFLogAssertion, "%s(): array is immutable", __PRETTY_FUNCTION__);
    __CFArrayValidateRange(otherArray, otherRange, __PRETTY_FUNCTION__);
#endif
    for (idx = otherRange.location; idx < otherRange.location + otherRange.length; idx++) {
	CFArrayAppendValue(array, CFArrayGetValueAtIndex(otherArray, idx));
    }
}

