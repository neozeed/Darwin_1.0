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
/*	CFHashing.c
	Copyright 1998-2000, Apple, Inc. All rights reserved.
	Responsibility: Christopher Kane
*/

#include <CoreFoundation/CFSet.h>
#include <CoreFoundation/CFBag.h>
#include <CoreFoundation/CFDictionary.h>
#include "CFInternal.h"

/* Hashing size tables.  The number of buckets is a power of two:
        #buckets(x) = 4 * (1 << x)
The corresponding maximum load or capacity is given by:
        maxcap(x) = floor((#buckets(x) * 17.0 + 12.0) / 24.0)
or approximately 70.8%.  'x' ranges from 0 to 28.
*/

static const CFIndex __CFHashTableCapacities[29] = {
    3, 6, 11, 23, 45, 91, 181, 363, 725, 1451, 2901, 5803, 11605,
    23211, 46421, 92843, 185685, 371371, 742741, 1485483, 2970965,
    5941931, 11883861, 23767723, 47535445, 95070891, 190141781,
    380283563, 760567125
};

CF_INLINE CFIndex __CFHashingRoundUpCapacity(CFIndex capacity) {
    CFIndex idx;
    for (idx = 0; idx < 29 && __CFHashTableCapacities[idx] < capacity; idx++);
    return __CFHashTableCapacities[idx];
}

CF_INLINE CFIndex __CFHashingNumBucketsForCapacity(CFIndex capacity) {
    CFIndex idx;
    for (idx = 0; idx < 29 && __CFHashTableCapacities[idx] < capacity; idx++);
    return 4 * (1 << idx);
}

struct __CFHashBucket {
    UInt32 _info;
    const void *_key;
};

/* Bits 31-30 of _info are used for bucket state */
/* Bits 29-0 of _info are used to cache the low-order bits of the key's hash */

enum {
    __kCFHashBucketEmpty = 0,
    __kCFHashBucketDeleted = 1,
    __kCFHashBucketOccupied = 3
};

CF_INLINE UInt32 __CFHashBucketState(const struct __CFHashBucket *bucket) {
    return __CFBitfieldValue(bucket->_info, 31, 30);
}

CF_INLINE CFHashCode __CFHashBucketKeyHash(const struct __CFHashBucket *bucket) {
    return __CFBitfieldValue(bucket->_info, 29, 0);
}

CF_INLINE void __CFHashBucketSetStateAndKeyHash(struct __CFHashBucket *bucket, UInt32 s, CFHashCode h) {
    bucket->_info = (s << 30) | __CFBitfieldValue(h, 29, 0);
}

CF_INLINE const void *__CFHashBucketKey(const struct __CFHashBucket *bucket) {
    return bucket->_key;
}

CF_INLINE void __CFHashBucketSetKey(struct __CFHashBucket *bucket, void *v) {
    bucket->_key = v;
}

static void __CFHashFindBuckets(const void *hashing, UInt32 hashingType, const void *key, CFHashCode keyHash, const void **match, const void **nomatch);

/* CFSet */

#if defined(__MACH__) && defined(__ppc__)
// Nasty performance hack: we mark these callbacks as "faulted" on PPC
const CFSetCallBacks kCFTypeSetCallBacks = {0, (void *)((unsigned)__CFTypeCollectionRetain+1), (void *)((unsigned)__CFTypeCollectionRelease+1), (void *)((unsigned)CFCopyDescription+1), (void *)((unsigned)CFEqual+1), (void *)((unsigned)CFHash+1)};
const CFSetCallBacks kCFCopyStringSetCallBacks = {0, (void *)((unsigned)CFStringCreateCopy+1), (void *)((unsigned)__CFTypeCollectionRelease+1), (void *)((unsigned)CFCopyDescription+1), (void *)((unsigned)CFEqual+1), (void *)((unsigned)CFHash+1)};
#else
const CFSetCallBacks kCFTypeSetCallBacks = {0, __CFTypeCollectionRetain, __CFTypeCollectionRelease, CFCopyDescription, CFEqual, CFHash};
const CFSetCallBacks kCFCopyStringSetCallBacks = {0, (void *)CFStringCreateCopy, __CFTypeCollectionRelease, CFCopyDescription, CFEqual, CFHash};
#endif

struct __CFSetBucket {	/* must start with the fields from CFHashBucket */
    UInt32 _info;
    const void *_key;
};

struct __CFSet {
    CFRuntimeBase _base;
    CFAllocatorRef _allocator;
    void *_context;	/* private */
    CFIndex _count;	/* number of objects */
    CFIndex _capacity;	/* maximum number of objects */
    CFIndex _bucketsNum;	/* number of slots */
    CFSetCallBacks _callbacks;
    struct __CFSetBucket *_buckets;
};

CF_INLINE CFIndex __CFSetCount(CFSetRef set) {
    return set->_count;
}

CF_INLINE void __CFSetSetCount(CFMutableSetRef set, CFIndex v) {
    /* for a CFSet, _bucketsUsed == _count */
}

CF_INLINE CFIndex __CFSetCapacity(CFSetRef set) {
    return set->_capacity;
}

CF_INLINE void __CFSetSetCapacity(CFMutableSetRef set, CFIndex v) {
    set->_capacity = v;
}

CF_INLINE CFIndex __CFSetNumBucketsUsed(CFSetRef set) {
    return set->_count;
}

CF_INLINE void __CFSetSetNumBucketsUsed(CFMutableSetRef set, CFIndex v) {
    set->_count = v;
}

CF_INLINE CFIndex __CFSetNumBuckets(CFSetRef set) {
    return set->_bucketsNum;
}

CF_INLINE void __CFSetSetNumBuckets(CFMutableSetRef set, CFIndex v) {
    set->_bucketsNum = v;
}

CFTypeID CFSetGetTypeID(void) {
    return __kCFSetTypeID;
}

Boolean __CFSetEqual(CFTypeRef cf1, CFTypeRef cf2) {
    CFSetRef set1 = (CFSetRef)cf1;
    CFSetRef set2 = (CFSetRef)cf2;
    struct __CFSetBucket *buckets;
    CFIndex idx;
    CFIndex cnt, nbuckets;
    if (set1 == set2) return TRUE;
    cnt = __CFSetCount(set1);
    if (cnt != __CFSetCount(set2)) return FALSE;
    /* Need both callbacks faulted for comparison purposes */
    if (set1->_callbacks.equal != set2->_callbacks.equal) return FALSE;
    if (0 == cnt) return TRUE;	/* after function comparison! */
    buckets = set1->_buckets;
    nbuckets = __CFSetNumBuckets(set1);
    for (idx = 0; idx < nbuckets; idx++)
	if (__kCFHashBucketOccupied == __CFHashBucketState((struct __CFHashBucket *)&buckets[idx]))
	    if (1 != CFSetGetCountOfValue(set2, buckets[idx]._key))
		return FALSE;
    return TRUE;
}

CFHashCode __CFSetHash(CFTypeRef cf) {
    CFSetRef set = (CFSetRef)cf;
    return __CFSetCount(set);
}

CFStringRef __CFSetCopyDescription(CFTypeRef cf) {
    CFSetRef set = (CFSetRef)cf;
    CFMutableStringRef result;
    CFIndex idx;
    CFIndex cnt, nbuckets;
    struct __CFSetBucket *buckets;
    cnt = __CFSetCount(set);
    buckets = set->_buckets;
    nbuckets = __CFSetNumBuckets(set);
    result = CFStringCreateMutable(set->_allocator, 0);
    CFStringAppendFormat(result, NULL, CFSTR("<CFSet 0x%x [0x%x]>{count = %u, capacity = %u, objects = (\n"), (UInt32)cf, (UInt32)set->_allocator, cnt, __CFSetCapacity(set));
    for (idx = 0; idx < nbuckets; idx++) {
	if (__kCFHashBucketOccupied == __CFHashBucketState((struct __CFHashBucket *)&buckets[idx])) {
	    CFStringRef desc = NULL;
	    const void *item = buckets[idx]._key;
	    if (NULL != set->_callbacks.copyDescription) {
		desc = (CFStringRef)INVOKE_CALLBACK2(set->_callbacks.copyDescription, item, set->_context);
	    }
	    if (NULL != desc) {
		CFStringAppendFormat(result, NULL, CFSTR("\t%u : %@\n"), idx, desc);
		CFRelease(desc);
	    } else {
		CFStringAppendFormat(result, NULL, CFSTR("\t%u : <0x%x>\n"), idx, (UInt32)item);
	    }
	}
    }
    CFStringAppend(result, CFSTR(")}"));
    return result;
}

CFAllocatorRef __CFSetGetAllocator(CFTypeRef cf) {
    CFSetRef set = (CFSetRef)cf;
    return set->_allocator;
}

void __CFSetDeallocate(CFTypeRef cf) {
    CFMutableSetRef set = (CFMutableSetRef)cf;
    CFAllocatorRef allocator = set->_allocator;
    if (__CFMutableVariety(set) != kCFMutable) {
	__CFSetMutableVariety(set, kCFFixedMutable);
    }
    CFSetRemoveAllValues(set);
    if (__CFMutableVariety(set) == kCFMutable) {
	CFAllocatorDeallocate(allocator, set->_buckets);
    }
    CFAllocatorDeallocate(allocator, set);
    CFRelease(allocator);
}

CFMutableSetRef _CFSetInit(CFAllocatorRef allocator, UInt32 flags, CFIndex capacity, const void **values, CFIndex numValues, const CFSetCallBacks *callBacks) {
    CFMutableSetRef memory;
    UInt32 size;
    SInt32 idx;
    __CFGenericValidateMutabilityFlags(flags);
    CFAssert2(0 <= capacity, __kCFLogAssertion, "%s(): capacity (%d) cannot be less than zero", __PRETTY_FUNCTION__, capacity);
    CFAssert3(kCFFixedMutable != __CFMutableVarietyFromFlags(flags) || numValues <= capacity, __kCFLogAssertion, "%s(): for kCFFixedMutable type, capacity (%d) must be greater than or equal to number of initial elements (%d)", __PRETTY_FUNCTION__, capacity, numValues);
    CFAssert2(0 <= numValues, __kCFLogAssertion, "%s(): numValues (%d) cannot be less than zero", __PRETTY_FUNCTION__, numValues);
    size = sizeof(struct __CFSet);
    if (__CFMutableVarietyFromFlags(flags) != kCFMutable) {
	size += sizeof(struct __CFSetBucket) * __CFHashingNumBucketsForCapacity(capacity);
    }
    allocator = (NULL == allocator) ? CFRetain(CFAllocatorGetDefault()) : CFRetain(allocator);
    memory = CFAllocatorAllocate(allocator, size, 0);
    if (NULL == memory) {
	CFRelease(allocator);
	return NULL;
    }
    __CFGenericInitBase(memory, __CFSetIsa(), __kCFSetTypeID);
    memory->_allocator = allocator;
    memory->_context = NULL;
    switch (__CFMutableVarietyFromFlags(flags)) {
    case kCFMutable:
	__CFSetSetCapacity(memory, __CFHashingRoundUpCapacity(1));
	__CFSetSetNumBuckets(memory, __CFHashingNumBucketsForCapacity(__CFHashingRoundUpCapacity(1)));
	__CFSetLastAllocationEventName(memory, "CFSet (mutable-variable)");
	memory->_buckets = CFAllocatorAllocate(allocator, __CFSetNumBuckets(memory) * sizeof(struct __CFSetBucket), 0);
	if (NULL == memory->_buckets) {
	    CFAllocatorDeallocate(allocator, memory);
	    CFRelease(allocator);
	    return NULL;
	}
	__CFSetLastAllocationEventName(memory->_buckets, "CFSet (store)");
	break;
    case kCFFixedMutable:
	/* Don't round up capacity */
	__CFSetSetCapacity(memory, capacity);
	__CFSetSetNumBuckets(memory, __CFHashingNumBucketsForCapacity(capacity));
	memory->_buckets = (struct __CFSetBucket *)((SInt8 *)memory + sizeof(struct __CFSet));
	__CFSetLastAllocationEventName(memory, "CFSet (mutable-fixed)");
	break;
    case kCFImmutable:
	/* Don't round up capacity */
	__CFSetSetCapacity(memory, capacity);
	__CFSetSetNumBuckets(memory, __CFHashingNumBucketsForCapacity(capacity));
	memory->_buckets = (struct __CFSetBucket *)((SInt8 *)memory + sizeof(struct __CFSet));
	__CFSetLastAllocationEventName(memory, "CFSet (immutable)");
	break;
    }
    __CFSetSetNumBucketsUsed(memory, 0);
    __CFSetSetCount(memory, 0);
    for (idx = __CFSetNumBuckets(memory); idx--;) {
	__CFHashBucketSetStateAndKeyHash((struct __CFHashBucket *)&(memory->_buckets[idx]), __kCFHashBucketEmpty, 0);
    }
    if (callBacks) {
	memory->_callbacks.retain = callBacks->retain;
	memory->_callbacks.release = callBacks->release;
	memory->_callbacks.copyDescription = callBacks->copyDescription;
	memory->_callbacks.equal = callBacks->equal;
	memory->_callbacks.hash = callBacks->hash;
	FAULT_CALLBACK((void **)&(memory->_callbacks.retain));
	FAULT_CALLBACK((void **)&(memory->_callbacks.release));
	FAULT_CALLBACK((void **)&(memory->_callbacks.copyDescription));
	FAULT_CALLBACK((void **)&(memory->_callbacks.hash));
	FAULT_CALLBACK((void **)&(memory->_callbacks.equal));
    } else {
	memory->_callbacks.retain = 0;
	memory->_callbacks.release = 0;
	memory->_callbacks.copyDescription = 0;
	memory->_callbacks.equal = 0;
	memory->_callbacks.hash = 0;
    }
    if (__CFMutableVarietyFromFlags(flags) != kCFMutable) {
        __CFSetMutableVariety(memory, kCFFixedMutable);
    } else {
        __CFSetMutableVariety(memory, kCFMutable);
    }
    for (idx = 0; idx < numValues; idx++)
	CFSetAddValue(memory, values[idx]);
    __CFSetMutableVariety(memory, __CFMutableVarietyFromFlags(flags));
    return memory;
}

void _CFSetSetContext(CFSetRef set, void *context) {
    ((struct __CFSet *)set)->_context = context;
}

CFSetRef CFSetCreate(CFAllocatorRef allocator, const void **values, CFIndex numValues, const CFSetCallBacks *callBacks) {
   return _CFSetInit(allocator, kCFImmutable, numValues, values, numValues, callBacks);
}

CFMutableSetRef CFSetCreateMutable(CFAllocatorRef allocator, CFIndex capacity, const CFSetCallBacks *callBacks) {
   return _CFSetInit(allocator, (0 == capacity) ? kCFMutable : kCFFixedMutable, capacity, NULL, 0, callBacks);
}

CFSetRef CFSetCreateCopy(CFAllocatorRef allocator, CFSetRef set) {
#warning CF: this currently only allows CFSets, and not NSSet subclasses, to be passed as the set argument
    CFMutableSetRef newSet;
    CFIndex cnt;
    const void **list, *buffer[256];
    __CFGenericValidateType(set, __kCFSetTypeID);
    cnt = CFSetGetCount(set);
    allocator = (NULL == allocator) ? CFRetain(CFAllocatorGetDefault()) : CFRetain(allocator);
    list = (cnt <= 256) ? buffer : CFAllocatorAllocate(allocator, cnt * sizeof(void *), 0);
    if (list != buffer) __CFSetLastAllocationEventName(list, "CFSet (temp)");
    CFSetGetValues(set, list);
    newSet = _CFSetInit(allocator, kCFImmutable, cnt, list, cnt, &(set->_callbacks));
    if (list != buffer) CFAllocatorDeallocate(allocator, list);
    CFRelease(allocator);
    return newSet;
}

CFMutableSetRef CFSetCreateMutableCopy(CFAllocatorRef allocator, CFIndex capacity, CFSetRef set) {
#warning CF: this currently only allows CFSets, and not NSSet subclasses, to be passed as the set argument
    CFMutableSetRef newSet;
    CFIndex cnt;
    const void **list, *buffer[256];
    __CFGenericValidateType(set, __kCFSetTypeID);
    cnt = CFSetGetCount(set);
    allocator = (NULL == allocator) ? CFRetain(CFAllocatorGetDefault()) : CFRetain(allocator);
    list = (cnt <= 256) ? buffer : CFAllocatorAllocate(allocator, cnt * sizeof(void *), 0);
    if (list != buffer) __CFSetLastAllocationEventName(list, "CFSet (temp)");
    CFSetGetValues(set, list);
    newSet = _CFSetInit(allocator, (0 == capacity) ? kCFMutable : kCFFixedMutable, capacity, list, cnt, &(set->_callbacks));
    if (list != buffer) CFAllocatorDeallocate(allocator, list);
    CFRelease(allocator);
    return newSet;
}

CFIndex CFSetGetCount(CFSetRef set) {
    CF_OBJC_FUNCDISPATCH0(Set, CFIndex, set, "count");
    __CFGenericValidateType(set, __kCFSetTypeID);
    return __CFSetCount(set);
}

CFIndex CFSetGetCountOfValue(CFSetRef set, const void *value) {
    CFHashCode keyHash;
    const struct __CFSetBucket *match;
    CF_OBJC_FUNCDISPATCH1(Set, CFIndex, set, "countForObject:", (id)value);
    __CFGenericValidateType(set, __kCFSetTypeID);
    if (0 == __CFSetCount(set)) return 0;
    keyHash = set->_callbacks.hash ? (CFHashCode)INVOKE_CALLBACK2(set->_callbacks.hash, value, set->_context) : (CFHashCode)value;
    keyHash &= __CFBitfieldMaxValue(29, 0);
    __CFHashFindBuckets(set, __kCFSetTypeID, value, keyHash, (const void **)&match, NULL);
    if (!match) return 0;
    return 1;
}

Boolean CFSetContainsValue(CFSetRef set, const void *value) {
    __CFGenericValidateType(set, __kCFSetTypeID);
    return (CFSetGetCountOfValue(set, value) != 0) ? TRUE : FALSE;
}

const void *CFSetGetValue(CFSetRef set, const void *value) {
    CFHashCode keyHash;
    const struct __CFSetBucket *match;
    CF_OBJC_FUNCDISPATCH1(Set, const void *, set, "member:", value);
    __CFGenericValidateType(set, __kCFSetTypeID);
    if (0 == __CFSetCount(set)) return NULL;
    keyHash = set->_callbacks.hash ? (CFHashCode)INVOKE_CALLBACK2(set->_callbacks.hash, value, set->_context) : (CFHashCode)value;
    keyHash &= __CFBitfieldMaxValue(29, 0);
    __CFHashFindBuckets(set, __kCFSetTypeID, value, keyHash, (const void **)&match, NULL);
    if (!match) return NULL;
    return match->_key;
}

Boolean CFSetGetValueIfPresent(CFSetRef set, const void *candidate, const void **value) {
    CFHashCode keyHash;
    const struct __CFSetBucket *match;
    CF_OBJC_FUNCDISPATCH2(Set, Boolean, set, "_getValue:forObj:", (id *)value, candidate);
    __CFGenericValidateType(set, __kCFSetTypeID);
    if (0 == __CFSetCount(set)) return FALSE;
    keyHash = set->_callbacks.hash ? (CFHashCode)INVOKE_CALLBACK2(set->_callbacks.hash, candidate, set->_context) : (CFHashCode)candidate;
    keyHash &= __CFBitfieldMaxValue(29, 0);
    __CFHashFindBuckets(set, __kCFSetTypeID, candidate, keyHash, (const void **)&match, NULL);
    if (!match) return FALSE;
    if (value) *value = match->_key;
    return TRUE;
}

void CFSetGetValues(CFSetRef set, const void **values) {
    struct __CFSetBucket *buckets;
    CFIndex idx;
    CFIndex cnt, nbuckets;
    CF_OBJC_FUNCDISPATCH1(Set, void, set, "getObjects:", (id *)values);
    __CFGenericValidateType(set, __kCFSetTypeID);
    buckets = set->_buckets;
    nbuckets = __CFSetNumBuckets(set);
    for (idx = 0; idx < nbuckets; idx++)
	if (__kCFHashBucketOccupied == __CFHashBucketState((struct __CFHashBucket *)&buckets[idx]))
	    for (cnt = 1; cnt--;)
		*values++ = buckets[idx]._key;
}

void CFSetApplyFunction(CFSetRef set, CFSetApplierFunction applier, void *context) {
    struct __CFSetBucket *buckets;
    CFIndex idx;
    CFIndex cnt, nbuckets;
    FAULT_CALLBACK((void **)&(applier));
    CF_OBJC_FUNCDISPATCH2(Set, void, set, "_applyValues:context:", applier, context);
    __CFGenericValidateType(set, __kCFSetTypeID);
    buckets = set->_buckets;
    nbuckets = __CFSetNumBuckets(set);
    for (idx = 0; idx < nbuckets; idx++)
	if (__kCFHashBucketOccupied == __CFHashBucketState((struct __CFHashBucket *)&buckets[idx]))
	    for (cnt = 1; cnt--;)
		INVOKE_CALLBACK2(applier, buckets[idx]._key, context);
}

static void __CFSetGrow(CFMutableSetRef set, CFIndex numNewValues) {
    struct __CFSetBucket *oldbuckets = set->_buckets;
    SInt32 idx, oldnbuckets = __CFSetNumBuckets(set);
    CFIndex oldCount = __CFSetCount(set);
    CFIndex capacity = __CFHashingRoundUpCapacity(oldCount + numNewValues);
    __CFSetSetCapacity(set, capacity);
    __CFSetSetNumBuckets(set, __CFHashingNumBucketsForCapacity(capacity));
    set->_buckets = CFAllocatorAllocate(set->_allocator, __CFSetNumBuckets(set) * sizeof(struct __CFSetBucket), 0);
    if (NULL == set->_buckets) __CFAbort();
    __CFSetLastAllocationEventName(set->_buckets, "CFSet (store)");
    for (idx = __CFSetNumBuckets(set); idx--;) {
	__CFHashBucketSetStateAndKeyHash((struct __CFHashBucket *)&(set->_buckets[idx]), __kCFHashBucketEmpty, 0);
    }
    for (idx = 0; idx < oldnbuckets; idx++) {
	if (__kCFHashBucketOccupied == __CFHashBucketState((struct __CFHashBucket *)&oldbuckets[idx])) {
	    struct __CFSetBucket *match, *nomatch;
	    CFHashCode keyHash = __CFHashBucketKeyHash((struct __CFHashBucket *)&oldbuckets[idx]);
	    __CFHashFindBuckets(set, __kCFSetTypeID, oldbuckets[idx]._key, keyHash, (const void **)&match, (const void **)&nomatch);
	    CFAssert3(!match, __kCFLogAssertion, "%s(): two objects (0x%x, 0x%x) now hash to the same slot; mutable object changed while in table or hash value is not immutable", __PRETTY_FUNCTION__, oldbuckets[idx]._key, match->_key);
	    __CFHashBucketSetStateAndKeyHash((struct __CFHashBucket *)nomatch, __kCFHashBucketOccupied, keyHash);
	    nomatch->_key = oldbuckets[idx]._key;
	}
    }
    CFAssert1(__CFSetCount(set) == oldCount, __kCFLogAssertion, "%s(): set count differs after rehashing; error", __PRETTY_FUNCTION__);
    CFAllocatorDeallocate(set->_allocator, oldbuckets);
}

void CFSetAddValue(CFMutableSetRef set, const void *value) {
    CFHashCode keyHash;
    struct __CFSetBucket *match, *nomatch;
    CF_OBJC_FUNCDISPATCH1(Set, void, set, "addObject:", value);
    switch (__CFMutableVariety(set)) {
    case kCFMutable:
	if (__CFSetNumBucketsUsed(set) == __CFSetCapacity(set))
	    __CFSetGrow(set, 1);
	break;
    case kCFFixedMutable:
	if (__CFSetCount(set) == __CFSetCapacity(set)) __CFAbort();
	break;
    case kCFImmutable:
    default: __CFAbort();
    }
    keyHash = set->_callbacks.hash ? (CFHashCode)INVOKE_CALLBACK2(set->_callbacks.hash, value, set->_context) : (CFHashCode)value;
    keyHash &= __CFBitfieldMaxValue(29, 0);
    __CFHashFindBuckets(set, __kCFSetTypeID, value, keyHash, (const void **)&match, (const void **)&nomatch);
    if (!match) {
	__CFHashBucketSetStateAndKeyHash((struct __CFHashBucket *)nomatch, __kCFHashBucketOccupied, keyHash);
	if (set->_callbacks.retain) {
	    nomatch->_key = (void *)INVOKE_CALLBACK3(set->_callbacks.retain, set->_allocator, value, set->_context);
	} else {
	    nomatch->_key = value;
	}
	__CFSetSetNumBucketsUsed(set, __CFSetNumBucketsUsed(set) + 1);
	__CFSetSetCount(set, __CFSetCount(set) + 1);
    }
}

void CFSetReplaceValue(CFMutableSetRef set, const void *value) {
    CFHashCode keyHash;
    struct __CFSetBucket *match;
    CF_OBJC_FUNCDISPATCH1(Set, void, set, "_replaceObject:", value);
    switch (__CFMutableVariety(set)) {
    case kCFMutable:
    case kCFFixedMutable:
	break;
    case kCFImmutable:
    default: __CFAbort();
    }
    keyHash = set->_callbacks.hash ? (CFHashCode)INVOKE_CALLBACK2(set->_callbacks.hash, value, set->_context) : (CFHashCode)value;
    keyHash &= __CFBitfieldMaxValue(29, 0);
    __CFHashFindBuckets(set, __kCFSetTypeID, value, keyHash, (const void **)&match, NULL);
    if (match) {
	const void *newValue;
	if (set->_callbacks.retain) {
	    newValue = (void *)INVOKE_CALLBACK3(set->_callbacks.retain, set->_allocator, value, set->_context);
	} else {
	    newValue = value;
	}
	if (set->_callbacks.release)
	    INVOKE_CALLBACK3(set->_callbacks.release, set->_allocator, match->_key, set->_context);
	match->_key = newValue;
    }
}

void CFSetSetValue(CFMutableSetRef set, const void *value) {
    CFHashCode keyHash;
    struct __CFSetBucket *match, *nomatch;
    CF_OBJC_FUNCDISPATCH1(Set, void, set, "_setObject:", value);
    switch (__CFMutableVariety(set)) {
    case kCFMutable:
	if (__CFSetNumBucketsUsed(set) == __CFSetCapacity(set))
	    __CFSetGrow(set, 1);
	break;
    case kCFFixedMutable:
	break;
    case kCFImmutable:
    default: __CFAbort();
    }
    keyHash = set->_callbacks.hash ? (CFHashCode)INVOKE_CALLBACK2(set->_callbacks.hash, value, set->_context) : (CFHashCode)value;
    keyHash &= __CFBitfieldMaxValue(29, 0);
    __CFHashFindBuckets(set, __kCFSetTypeID, value, keyHash, (const void **)&match, (const void **)&nomatch);
    if (!match) {
	if (kCFFixedMutable == __CFMutableVariety(set) && __CFSetCount(set) == __CFSetCapacity(set)) __CFAbort();
	__CFHashBucketSetStateAndKeyHash((struct __CFHashBucket *)nomatch, __kCFHashBucketOccupied, keyHash);
	if (set->_callbacks.retain) {
	    nomatch->_key = (void *)INVOKE_CALLBACK3(set->_callbacks.retain, set->_allocator, value, set->_context);
	} else {
	    nomatch->_key = value;
	}
	__CFSetSetNumBucketsUsed(set, __CFSetNumBucketsUsed(set) + 1);
	__CFSetSetCount(set, __CFSetCount(set) + 1);
    } else {
	const void *newValue;
	if (set->_callbacks.retain) {
	    newValue = (void *)INVOKE_CALLBACK3(set->_callbacks.retain, set->_allocator, value, set->_context);
	} else {
	    newValue = value;
	}
	if (set->_callbacks.release)
	    INVOKE_CALLBACK3(set->_callbacks.release, set->_allocator, match->_key, set->_context);
	match->_key = newValue;
    }
}

void CFSetRemoveValue(CFMutableSetRef set, const void *value) {
    CFHashCode keyHash;
    struct __CFSetBucket *match;
    CF_OBJC_FUNCDISPATCH1(Set, void, set, "removeObject:", value);
    switch (__CFMutableVariety(set)) {
    case kCFMutable:
    case kCFFixedMutable:
	break;
    case kCFImmutable:
    default: __CFAbort();
    }
    keyHash = set->_callbacks.hash ? (CFHashCode)INVOKE_CALLBACK2(set->_callbacks.hash, value, set->_context) : (CFHashCode)value;
    keyHash &= __CFBitfieldMaxValue(29, 0);
    __CFHashFindBuckets(set, __kCFSetTypeID, value, keyHash, (const void **)&match, NULL);
    if (match) {
	__CFHashBucketSetStateAndKeyHash((struct __CFHashBucket *)match, __kCFHashBucketDeleted, 0);
	if (set->_callbacks.release)
	    INVOKE_CALLBACK3(set->_callbacks.release, set->_allocator, match->_key, set->_context);
	match->_key = NULL;
	__CFSetSetNumBucketsUsed(set, __CFSetNumBucketsUsed(set) - 1);
	__CFSetSetCount(set, __CFSetCount(set) - 1);
    }
}

void CFSetRemoveAllValues(CFMutableSetRef set) {
    struct __CFSetBucket *buckets;
    SInt32 idx, nbuckets;
    CF_OBJC_FUNCDISPATCH0(Set, void, set, "removeAllObjects");
    buckets = set->_buckets;
    nbuckets = __CFSetNumBuckets(set);
    switch (__CFMutableVariety(set)) {
    case kCFMutable:
    case kCFFixedMutable:
	break;
    case kCFImmutable:
    default: __CFAbort();
    }
    for (idx = 0; idx < nbuckets; idx++) {
	if (__kCFHashBucketOccupied == __CFHashBucketState((struct __CFHashBucket *)&buckets[idx])) {
	    if (set->_callbacks.release)
		INVOKE_CALLBACK3(set->_callbacks.release, set->_allocator, buckets[idx]._key, set->_context);
	    buckets[idx]._key = NULL;
	}
	__CFHashBucketSetStateAndKeyHash((struct __CFHashBucket *)&buckets[idx], __kCFHashBucketEmpty, 0);
    }
    __CFSetSetNumBucketsUsed(set, 0);
    __CFSetSetCount(set, 0);
}

/* CFBag */

#if defined(__MACH__) && defined(__ppc__)
// Nasty performance hack: we mark these callbacks as "faulted" on PPC
const CFBagCallBacks kCFTypeBagCallBacks = {0, (void *)((unsigned)__CFTypeCollectionRetain+1), (void *)((unsigned)__CFTypeCollectionRelease+1), (void *)((unsigned)CFCopyDescription+1), (void *)((unsigned)CFEqual+1), (void *)((unsigned)CFHash+1)};
const CFBagCallBacks kCFCopyStringBagCallBacks = {0, (void *)((unsigned)CFStringCreateCopy+1), (void *)((unsigned)__CFTypeCollectionRelease+1), (void *)((unsigned)CFCopyDescription+1), (void *)((unsigned)CFEqual+1), (void *)((unsigned)CFHash+1)};
#else
const CFBagCallBacks kCFTypeBagCallBacks = {0, __CFTypeCollectionRetain, __CFTypeCollectionRelease, CFCopyDescription, CFEqual, CFHash};
const CFBagCallBacks kCFCopyStringBagCallBacks = {0, (void *)CFStringCreateCopy, __CFTypeCollectionRelease, CFCopyDescription, CFEqual, CFHash};
#endif

struct __CFBagBucket {	/* must start with the fields from CFHashBucket */
    UInt32 _info;
    void *_key;
    CFIndex _count;
};

struct __CFBag {
    CFRuntimeBase _base;
    CFAllocatorRef _allocator;
    CFIndex _count;	/* number of objects */
    CFIndex _capacity;	/* maximum number of objects */
    CFIndex _bucketsUsed;/* number of slots used */
    CFIndex _bucketsNum;	/* number of slots */
    CFBagCallBacks _callbacks;
    struct __CFBagBucket *_buckets;
};

CF_INLINE CFIndex __CFBagCount(CFBagRef bag) {
    return bag->_count;
}

CF_INLINE void __CFBagSetCount(CFMutableBagRef bag, CFIndex v) {
    bag->_count = v;
}

CF_INLINE CFIndex __CFBagCapacity(CFBagRef bag) {
    return bag->_capacity;
}

CF_INLINE void __CFBagSetCapacity(CFMutableBagRef bag, CFIndex v) {
    bag->_capacity = v;
}

CF_INLINE CFIndex __CFBagNumBucketsUsed(CFBagRef bag) {
    return bag->_bucketsUsed;
}

CF_INLINE void __CFBagSetNumBucketsUsed(CFMutableBagRef bag, CFIndex v) {
    bag->_bucketsUsed = v;
}

CF_INLINE CFIndex __CFBagNumBuckets(CFBagRef bag) {
    return bag->_bucketsNum;
}

CF_INLINE void __CFBagSetNumBuckets(CFMutableBagRef bag, CFIndex v) {
    bag->_bucketsNum = v;
}

CFTypeID CFBagGetTypeID(void) {
    return __kCFBagTypeID;
}

Boolean __CFBagEqual(CFTypeRef cf1, CFTypeRef cf2) {
    CFBagRef bag1 = (CFBagRef)cf1;
    CFBagRef bag2 = (CFBagRef)cf2;
    struct __CFBagBucket *buckets;
    CFIndex idx;
    CFIndex cnt, nbuckets;
    if (bag1 == bag2) return TRUE;
    cnt = __CFBagCount(bag1);
    if (cnt != __CFBagCount(bag2)) return FALSE;
    /* Need both callbacks faulted for comparison purposes */
    if (bag1->_callbacks.equal != bag2->_callbacks.equal) return FALSE;
    if (0 == cnt) return TRUE;
    buckets = bag1->_buckets;
    nbuckets = __CFBagNumBuckets(bag1);
    for (idx = 0; idx < nbuckets; idx++)
	if (__kCFHashBucketOccupied == __CFHashBucketState((struct __CFHashBucket *)&buckets[idx]))
	    if (buckets[idx]._count != CFBagGetCountOfValue(bag2, buckets[idx]._key))
		return FALSE;
    return TRUE;
}

CFHashCode __CFBagHash(CFTypeRef cf) {
    CFBagRef bag = (CFBagRef)cf;
    return __CFBagCount(bag);
}

CFStringRef __CFBagCopyDescription(CFTypeRef cf) {
    CFBagRef bag = (CFBagRef)cf;
    CFMutableStringRef result;
    CFIndex idx;
    CFIndex cnt, nbuckets;
    struct __CFBagBucket *buckets;
    cnt = __CFBagCount(bag);
    buckets = bag->_buckets;
    nbuckets = __CFBagNumBuckets(bag);
    result = CFStringCreateMutable(bag->_allocator, 0);
    CFStringAppendFormat(result, NULL, CFSTR("<CFBag 0x%x [0x%x]>{count = %u, capacity = %u, objects = (\n"), (UInt32)cf, bag->_allocator, cnt, __CFBagCapacity(bag));
    for (idx = 0; idx < nbuckets; idx++) {
	if (__kCFHashBucketOccupied == __CFHashBucketState((struct __CFHashBucket *)&buckets[idx])) {
	    CFStringRef desc = NULL;
	    const void *item = buckets[idx]._key;
	    if (NULL != bag->_callbacks.copyDescription) {
		desc = (CFStringRef)INVOKE_CALLBACK1(bag->_callbacks.copyDescription, item);
	    }
	    if (NULL != desc) {
		CFStringAppendFormat(result, NULL, CFSTR("\t%u : %@ (%u)\n"), idx, desc, buckets[idx]._count);
		CFRelease(desc);
	    } else {
		CFStringAppendFormat(result, NULL, CFSTR("\t%u : <0x%x> (%u)\n"), idx, (UInt32)item, buckets[idx]._count);
	    }
	}
    }
    CFStringAppend(result, CFSTR(")}"));
    return result;
}

CFAllocatorRef __CFBagGetAllocator(CFTypeRef cf) {
    CFBagRef bag = (CFBagRef)cf;
    return bag->_allocator;
}

void __CFBagDeallocate(CFTypeRef cf) {
    CFMutableBagRef bag = (CFMutableBagRef)cf;
    CFAllocatorRef allocator = bag->_allocator;
    if (__CFMutableVariety(bag) != kCFMutable) {
	__CFSetMutableVariety(bag, kCFFixedMutable);
    }
    CFBagRemoveAllValues(bag);
    if (__CFMutableVariety(bag) == kCFMutable) {
	CFAllocatorDeallocate(allocator, bag->_buckets);
    }
    CFAllocatorDeallocate(allocator, bag);
    CFRelease(allocator);
}

CFMutableBagRef _CFBagInit(CFAllocatorRef allocator, UInt32 flags, CFIndex capacity, const void **values, CFIndex numValues, const CFBagCallBacks *callBacks) {
    CFMutableBagRef memory;
    UInt32 size;
    SInt32 idx;
    __CFGenericValidateMutabilityFlags(flags);
    CFAssert2(0 <= capacity, __kCFLogAssertion, "%s(): capacity (%d) cannot be less than zero", __PRETTY_FUNCTION__, capacity);
    CFAssert3(kCFFixedMutable != __CFMutableVarietyFromFlags(flags) || numValues <= capacity, __kCFLogAssertion, "%s(): for kCFFixedMutable type, capacity (%d) must be greater than or equal to number of initial elements (%d)", __PRETTY_FUNCTION__, capacity, numValues);
    CFAssert2(0 <= numValues, __kCFLogAssertion, "%s(): numValues (%d) cannot be less than zero", __PRETTY_FUNCTION__, numValues);
    size = sizeof(struct __CFBag);
    if (__CFMutableVarietyFromFlags(flags) != kCFMutable) {
       size += sizeof(struct __CFBagBucket) * __CFHashingNumBucketsForCapacity(capacity);
    }
    allocator = (NULL == allocator) ? CFRetain(CFAllocatorGetDefault()) : CFRetain(allocator);
    memory = CFAllocatorAllocate(allocator, size, 0);
    if (NULL == memory) {
	CFRelease(allocator);
	return NULL;
    }
    __CFGenericInitBase(memory, NULL, __kCFBagTypeID);
    memory->_allocator = allocator;
    switch (__CFMutableVarietyFromFlags(flags)) {
    case kCFMutable:
	__CFBagSetCapacity(memory, __CFHashingRoundUpCapacity(1));
	__CFBagSetNumBuckets(memory, __CFHashingNumBucketsForCapacity(__CFHashingRoundUpCapacity(1)));
	__CFSetLastAllocationEventName(memory, "CFBag (mutable-variable)");
	memory->_buckets = CFAllocatorAllocate(allocator, __CFBagNumBuckets(memory) * sizeof(struct __CFBagBucket), 0);
	if (NULL == memory->_buckets) {
	    CFAllocatorDeallocate(allocator, memory);
	    CFRelease(allocator);
	    return NULL;
	}
	__CFSetLastAllocationEventName(memory->_buckets, "CFBag (store)");
	break;
    case kCFFixedMutable:
	/* Don't round up capacity */
	__CFBagSetCapacity(memory, capacity);
	__CFBagSetNumBuckets(memory, __CFHashingNumBucketsForCapacity(capacity));
	memory->_buckets = (struct __CFBagBucket *)((SInt8 *)memory + sizeof(struct __CFBag));
	__CFSetLastAllocationEventName(memory, "CFBag (mutable-fixed)");
	break;
    case kCFImmutable:
	/* Don't round up capacity */
	__CFBagSetCapacity(memory, capacity);
	__CFBagSetNumBuckets(memory, __CFHashingNumBucketsForCapacity(capacity));
	memory->_buckets = (struct __CFBagBucket *)((SInt8 *)memory + sizeof(struct __CFBag));
	__CFSetLastAllocationEventName(memory, "CFBag (immutable)");
	break;
    }
    __CFBagSetNumBucketsUsed(memory, 0);
    __CFBagSetCount(memory, 0);
    for (idx = __CFBagNumBuckets(memory); idx--;) {
	__CFHashBucketSetStateAndKeyHash((struct __CFHashBucket *)&(memory->_buckets[idx]), __kCFHashBucketEmpty, 0);
    }
    if (callBacks) {
	memory->_callbacks.retain = callBacks->retain;
	memory->_callbacks.release = callBacks->release;
	memory->_callbacks.copyDescription = callBacks->copyDescription;
	memory->_callbacks.equal = callBacks->equal;
	memory->_callbacks.hash = callBacks->hash;
        FAULT_CALLBACK((void **)&(memory->_callbacks.retain));
        FAULT_CALLBACK((void **)&(memory->_callbacks.release));
        FAULT_CALLBACK((void **)&(memory->_callbacks.copyDescription));
        FAULT_CALLBACK((void **)&(memory->_callbacks.hash));
        FAULT_CALLBACK((void **)&(memory->_callbacks.equal));
    } else {
	memory->_callbacks.retain = 0;
	memory->_callbacks.release = 0;
	memory->_callbacks.copyDescription = 0;
	memory->_callbacks.equal = 0;
	memory->_callbacks.hash = 0;
    }
    if (__CFMutableVarietyFromFlags(flags) != kCFMutable) {
        __CFSetMutableVariety(memory, kCFFixedMutable);
    } else {
        __CFSetMutableVariety(memory, kCFMutable);
    }
    for (idx = 0; idx < numValues; idx++)
	CFBagAddValue(memory, values[idx]);
    __CFSetMutableVariety(memory, __CFMutableVarietyFromFlags(flags));
    return memory;
}

CFBagRef CFBagCreate(CFAllocatorRef allocator, const void **values, CFIndex numValues, const CFBagCallBacks *callBacks) {
   return _CFBagInit(allocator, kCFImmutable, numValues, values, numValues, callBacks);
}

CFMutableBagRef CFBagCreateMutable(CFAllocatorRef allocator, CFIndex capacity, const CFBagCallBacks *callBacks) {
   return _CFBagInit(allocator, (0 == capacity) ? kCFMutable : kCFFixedMutable, capacity, NULL, 0, callBacks);
}

CFBagRef CFBagCreateCopy(CFAllocatorRef allocator, CFBagRef bag) {
    CFMutableBagRef newBag;
    CFIndex cnt;
    const void **list, *buffer[256];
    __CFGenericValidateType(bag, __kCFBagTypeID);
    cnt = __CFBagCount(bag);
    allocator = (NULL == allocator) ? CFRetain(CFAllocatorGetDefault()) : CFRetain(allocator);
    list = (cnt <= 256) ? buffer : CFAllocatorAllocate(allocator, cnt * sizeof(void *), 0);
    if (list != buffer) __CFSetLastAllocationEventName(list, "CFBag (temp)");
    CFBagGetValues(bag, list);
    newBag = _CFBagInit(allocator, kCFImmutable, cnt, list, cnt, &(bag->_callbacks));
    if (list != buffer) CFAllocatorDeallocate(allocator, list);
    CFRelease(allocator);
    return newBag;
}

CFMutableBagRef CFBagCreateMutableCopy(CFAllocatorRef allocator, CFIndex capacity, CFBagRef bag) {
    CFMutableBagRef newBag;
    CFIndex cnt;
    const void **list, *buffer[256];
    __CFGenericValidateType(bag, __kCFBagTypeID);
    cnt = __CFBagCount(bag);
    allocator = (NULL == allocator) ? CFRetain(CFAllocatorGetDefault()) : CFRetain(allocator);
    list = (cnt <= 256) ? buffer : CFAllocatorAllocate(allocator, cnt * sizeof(void *), 0);
    if (list != buffer) __CFSetLastAllocationEventName(list, "CFBag (temp)");
    CFBagGetValues(bag, list);
    newBag = _CFBagInit(allocator, (0 == capacity) ? kCFMutable : kCFFixedMutable, capacity, list, cnt, &(bag->_callbacks));
    if (list != buffer) CFAllocatorDeallocate(allocator, list);
    CFRelease(allocator);
    return newBag;
}

CFIndex CFBagGetCount(CFBagRef bag) {
    return __CFBagCount(bag);
}

CFIndex CFBagGetCountOfValue(CFBagRef bag, const void *value) {
    CFHashCode keyHash;
    const struct __CFBagBucket *match;
    if (0 == __CFBagCount(bag)) return 0;
    keyHash = bag->_callbacks.hash ? (CFHashCode)INVOKE_CALLBACK1(bag->_callbacks.hash, value) : (CFHashCode)value;
    keyHash &= __CFBitfieldMaxValue(29, 0);
    __CFHashFindBuckets(bag, __kCFBagTypeID, value, keyHash, (const void **)&match, NULL);
    if (!match) return 0;
    return match->_count;
}

Boolean CFBagContainsValue(CFBagRef bag, const void *value) {
    return (CFBagGetCountOfValue(bag, value) != 0) ? TRUE : FALSE;
}

const void *CFBagGetValue(CFBagRef bag, const void *value) {
    CFHashCode keyHash;
    const struct __CFBagBucket *match;
    if (0 == __CFBagCount(bag)) return NULL;
    keyHash = bag->_callbacks.hash ? (CFHashCode)INVOKE_CALLBACK1(bag->_callbacks.hash, value) : (CFHashCode)value;
    keyHash &= __CFBitfieldMaxValue(29, 0);
    __CFHashFindBuckets(bag, __kCFBagTypeID, value, keyHash, (const void **)&match, NULL);
    if (!match) return NULL;
    return match->_key;
}

Boolean CFBagGetValueIfPresent(CFBagRef bag, const void *candidate, const void **value) {
    CFHashCode keyHash;
    const struct __CFBagBucket *match;
    if (0 == __CFBagCount(bag)) return FALSE;
    keyHash = bag->_callbacks.hash ? (CFHashCode)INVOKE_CALLBACK1(bag->_callbacks.hash, candidate) : (CFHashCode)candidate;
    keyHash &= __CFBitfieldMaxValue(29, 0);
    __CFHashFindBuckets(bag, __kCFBagTypeID, candidate, keyHash, (const void **)&match, NULL);
    if (!match) return FALSE;
    if (value) *value = match->_key;
    return TRUE;
}

void CFBagGetValues(CFBagRef bag, const void **values) {
    struct __CFBagBucket *buckets = bag->_buckets;
    SInt32 idx, cnt, nbuckets = __CFBagNumBuckets(bag);
    for (idx = 0; idx < nbuckets; idx++)
	if (__kCFHashBucketOccupied == __CFHashBucketState((struct __CFHashBucket *)&buckets[idx]))
	    for (cnt = buckets[idx]._count; cnt--;)
		*values++ = buckets[idx]._key;
}

void CFBagApplyFunction(CFBagRef bag, CFBagApplierFunction applier, void *context) {
    struct __CFBagBucket *buckets = bag->_buckets;
    SInt32 idx, cnt, nbuckets = __CFBagNumBuckets(bag);
    FAULT_CALLBACK((void **)&(applier));
    for (idx = 0; idx < nbuckets; idx++)
	if (__kCFHashBucketOccupied == __CFHashBucketState((struct __CFHashBucket *)&buckets[idx]))
	    for (cnt = buckets[idx]._count; cnt--;)
		INVOKE_CALLBACK2(applier, buckets[idx]._key, context);
}

static void __CFBagGrow(CFMutableBagRef bag, CFIndex numNewValues) {
    struct __CFBagBucket *oldbuckets = bag->_buckets;
    SInt32 idx, oldnbuckets = __CFBagNumBuckets(bag);
    CFIndex oldCount = __CFBagCount(bag);
    CFIndex capacity = __CFHashingRoundUpCapacity(oldCount + numNewValues);
    __CFBagSetCapacity(bag, capacity);
    __CFBagSetNumBuckets(bag, __CFHashingNumBucketsForCapacity(capacity));
    bag->_buckets = CFAllocatorAllocate(bag->_allocator, __CFBagNumBuckets(bag) * sizeof(struct __CFBagBucket), 0);
    if (NULL == bag->_buckets) __CFAbort();
    __CFSetLastAllocationEventName(bag->_buckets, "CFBag (store)");
    for (idx = __CFBagNumBuckets(bag); idx--;) {
	__CFHashBucketSetStateAndKeyHash((struct __CFHashBucket *)&(bag->_buckets[idx]), __kCFHashBucketEmpty, 0);
    }
    for (idx = 0; idx < oldnbuckets; idx++) {
	if (__kCFHashBucketOccupied == __CFHashBucketState((struct __CFHashBucket *)&oldbuckets[idx])) {
	    struct __CFBagBucket *match, *nomatch;
	    CFHashCode keyHash = __CFHashBucketKeyHash((struct __CFHashBucket *)&oldbuckets[idx]);
	    __CFHashFindBuckets(bag, __kCFBagTypeID, oldbuckets[idx]._key, keyHash, (const void **)&match, (const void **)&nomatch);
	    CFAssert3(!match, __kCFLogAssertion, "%s(): two objects (0x%x, 0x%x) now hash to the same slot; mutable object changed while in table or hash value is not immutable", __PRETTY_FUNCTION__, oldbuckets[idx]._key, match->_key);
	    __CFHashBucketSetStateAndKeyHash((struct __CFHashBucket *)nomatch, __kCFHashBucketOccupied, keyHash);
	    nomatch->_key = oldbuckets[idx]._key;
	    nomatch->_count = oldbuckets[idx]._count;
	}
    }
    CFAssert1(__CFBagCount(bag) == oldCount, __kCFLogAssertion, "%s(): bag count differs after rehashing; error", __PRETTY_FUNCTION__);
    CFAllocatorDeallocate(bag->_allocator, oldbuckets);
}

void CFBagAddValue(CFMutableBagRef bag, const void *value) {
    CFHashCode keyHash;
    struct __CFBagBucket *match, *nomatch;
    switch (__CFMutableVariety(bag)) {
    case kCFMutable:
	if (__CFBagNumBucketsUsed(bag) == __CFBagCapacity(bag))
	    __CFBagGrow(bag, 1);
	break;
    case kCFFixedMutable:
	if (__CFBagCount(bag) == __CFBagCapacity(bag)) __CFAbort();
	break;
    case kCFImmutable:
    default: __CFAbort();
    }
    keyHash = bag->_callbacks.hash ? (CFHashCode)INVOKE_CALLBACK1(bag->_callbacks.hash, value) : (CFHashCode)value;
    keyHash &= __CFBitfieldMaxValue(29, 0);
    __CFHashFindBuckets(bag, __kCFBagTypeID, value, keyHash, (const void **)&match, (const void **)&nomatch);
    if (!match) {
	__CFHashBucketSetStateAndKeyHash((struct __CFHashBucket *)nomatch, __kCFHashBucketOccupied, keyHash);
	if (bag->_callbacks.retain) {
	    nomatch->_key = (void *)INVOKE_CALLBACK2(bag->_callbacks.retain, bag->_allocator, value);
	} else {
	    nomatch->_key = (void *)value;
	}
	nomatch->_count = 1;
	__CFBagSetNumBucketsUsed(bag, __CFBagNumBucketsUsed(bag) + 1);
	__CFBagSetCount(bag, __CFBagCount(bag) + 1);
    } else {
	match->_count++;
	__CFBagSetCount(bag, __CFBagCount(bag) + 1);
    }
}

void CFBagReplaceValue(CFMutableBagRef bag, const void *value) {
    CFHashCode keyHash;
    struct __CFBagBucket *match;
    switch (__CFMutableVariety(bag)) {
    case kCFMutable:
    case kCFFixedMutable:
	break;
    case kCFImmutable:
    default: __CFAbort();
    }
    keyHash = bag->_callbacks.hash ? (CFHashCode)INVOKE_CALLBACK1(bag->_callbacks.hash, value) : (CFHashCode)value;
    keyHash &= __CFBitfieldMaxValue(29, 0);
    __CFHashFindBuckets(bag, __kCFBagTypeID, value, keyHash, (const void **)&match, NULL);
    if (match) {
	const void *newValue;
	if (bag->_callbacks.retain) {
	    newValue = (void *)INVOKE_CALLBACK2(bag->_callbacks.retain, bag->_allocator, value);
	} else {
	    newValue = value;
	}
	if (bag->_callbacks.release)
	    INVOKE_CALLBACK2(bag->_callbacks.release, bag->_allocator, match->_key);
	match->_key = (void *)newValue;
    }
}

void CFBagSetValue(CFMutableBagRef bag, const void *value) {
    CFHashCode keyHash;
    struct __CFBagBucket *match, *nomatch;
    switch (__CFMutableVariety(bag)) {
    case kCFMutable:
	if (__CFBagNumBucketsUsed(bag) == __CFBagCapacity(bag))
	    __CFBagGrow(bag, 1);
	break;
    case kCFFixedMutable:
	break;
    case kCFImmutable:
    default: __CFAbort();
    }
    keyHash = bag->_callbacks.hash ? (CFHashCode)INVOKE_CALLBACK1(bag->_callbacks.hash, value) : (CFHashCode)value;
    keyHash &= __CFBitfieldMaxValue(29, 0);
    __CFHashFindBuckets(bag, __kCFBagTypeID, value, keyHash, (const void **)&match, (const void **)&nomatch);
    if (!match) {
	if (kCFFixedMutable == __CFMutableVariety(bag) && __CFBagCount(bag) == __CFBagCapacity(bag)) __CFAbort();
	__CFHashBucketSetStateAndKeyHash((struct __CFHashBucket *)nomatch, __kCFHashBucketOccupied, keyHash);
	if (bag->_callbacks.retain) {
	    nomatch->_key = (void *)INVOKE_CALLBACK2(bag->_callbacks.retain, bag->_allocator, value);
	} else {
	    nomatch->_key = (void *)value;
	}
	nomatch->_count = 1;
	__CFBagSetNumBucketsUsed(bag, __CFBagNumBucketsUsed(bag) + 1);
	__CFBagSetCount(bag, __CFBagCount(bag) + 1);
    } else {
	const void *newValue;
	if (bag->_callbacks.retain) {
	    newValue = (void *)INVOKE_CALLBACK2(bag->_callbacks.retain, bag->_allocator, value);
	} else {
	    newValue = value;
	}
	if (bag->_callbacks.release)
	    INVOKE_CALLBACK2(bag->_callbacks.release, bag->_allocator, match->_key);
	match->_key = (void *)newValue;
    }
}

void CFBagRemoveValue(CFMutableBagRef bag, const void *value) {
    CFHashCode keyHash;
    struct __CFBagBucket *match;
    switch (__CFMutableVariety(bag)) {
    case kCFMutable:
    case kCFFixedMutable:
	break;
    case kCFImmutable:
    default: __CFAbort();
    }
    keyHash = bag->_callbacks.hash ? (CFHashCode)INVOKE_CALLBACK1(bag->_callbacks.hash, value) : (CFHashCode)value;
    keyHash &= __CFBitfieldMaxValue(29, 0);
    __CFHashFindBuckets(bag, __kCFBagTypeID, value, keyHash, (const void **)&match, NULL);
    if (match) {
	match->_count--;
	__CFBagSetCount(bag, __CFBagCount(bag) - 1);
	if (0 == match->_count) {
	    __CFHashBucketSetStateAndKeyHash((struct __CFHashBucket *)match, __kCFHashBucketDeleted, 0);
	    if (bag->_callbacks.release)
		INVOKE_CALLBACK2(bag->_callbacks.release, bag->_allocator, match->_key);
	    match->_key = NULL;
	    __CFBagSetNumBucketsUsed(bag, __CFBagNumBucketsUsed(bag) - 1);
	}
    }
}

void CFBagRemoveAllValues(CFMutableBagRef bag) {
    struct __CFBagBucket *buckets = bag->_buckets;
    SInt32 idx, nbuckets = __CFBagNumBuckets(bag);
    switch (__CFMutableVariety(bag)) {
    case kCFMutable:
    case kCFFixedMutable:
	break;
    case kCFImmutable:
    default: __CFAbort();
    }
    for (idx = 0; idx < nbuckets; idx++) {
	if (__kCFHashBucketOccupied == __CFHashBucketState((struct __CFHashBucket *)&buckets[idx])) {
	    if (bag->_callbacks.release)
		INVOKE_CALLBACK2(bag->_callbacks.release, bag->_allocator, buckets[idx]._key);
	    buckets[idx]._key = NULL;
	    buckets[idx]._count = 0;
	}
	__CFHashBucketSetStateAndKeyHash((struct __CFHashBucket *)&buckets[idx], __kCFHashBucketEmpty, 0);
    }
    __CFBagSetNumBucketsUsed(bag, 0);
    __CFBagSetCount(bag, 0);
}


/* CFDictionary */

#if defined(__MACH__) && defined(__ppc__)
// Nasty performance hack: we mark these callbacks as "faulted" on PPC
const CFDictionaryKeyCallBacks kCFTypeDictionaryKeyCallBacks = {0, (void *)((unsigned)__CFTypeCollectionRetain+1), (void *)((unsigned)__CFTypeCollectionRelease+1), (void *)((unsigned)CFCopyDescription+1), (void *)((unsigned)CFEqual+1), (void *)((unsigned)CFHash+1)};
const CFDictionaryKeyCallBacks kCFCopyStringDictionaryKeyCallBacks = {0, (void *)((unsigned)CFStringCreateCopy+1), (void *)((unsigned)__CFTypeCollectionRelease+1), (void *)((unsigned)CFCopyDescription+1), (void *)((unsigned)CFEqual+1), (void *)((unsigned)CFHash+1)};
const CFDictionaryValueCallBacks kCFTypeDictionaryValueCallBacks = {0, (void *)((unsigned)__CFTypeCollectionRetain+1), (void *)((unsigned)__CFTypeCollectionRelease+1), (void *)((unsigned)CFCopyDescription+1), (void *)((unsigned)CFEqual+1)};
#else
const CFDictionaryKeyCallBacks kCFTypeDictionaryKeyCallBacks = {0, __CFTypeCollectionRetain, __CFTypeCollectionRelease, CFCopyDescription, CFEqual, CFHash};
const CFDictionaryKeyCallBacks kCFCopyStringDictionaryKeyCallBacks = {0, (void *)CFStringCreateCopy, __CFTypeCollectionRelease, CFCopyDescription, CFEqual, CFHash};
const CFDictionaryValueCallBacks kCFTypeDictionaryValueCallBacks = {0, __CFTypeCollectionRetain, __CFTypeCollectionRelease, CFCopyDescription, CFEqual};
#endif

struct __CFDictionaryBucket {	/* must start with the fields from CFHashBucket */
    UInt32 _info;
    const void *_key;
    const void *_value;
};

struct __CFDictionary {
    CFRuntimeBase _base;
    CFAllocatorRef _allocator;
    void *_context;	/* private */
    CFIndex _count;	/* number of objects */
    CFIndex _capacity;	/* maximum number of objects */
    CFIndex _bucketsNum;	/* number of slots */
    CFDictionaryKeyCallBacks _callbacks;
    CFDictionaryValueCallBacks _vcallbacks;
    struct __CFDictionaryBucket *_buckets;
};

CF_INLINE CFIndex __CFDictionaryCount(CFDictionaryRef dict) {
    return dict->_count;
}

CF_INLINE void __CFDictionarySetCount(CFMutableDictionaryRef dict, CFIndex v) {
    /* for a CFDictionary, _bucketsUsed == _count */
}

CF_INLINE CFIndex __CFDictionaryCapacity(CFDictionaryRef dict) {
    return dict->_capacity;
}

CF_INLINE void __CFDictionarySetCapacity(CFMutableDictionaryRef dict, CFIndex v) {
    dict->_capacity = v;
}

CF_INLINE CFIndex __CFDictionaryNumBucketsUsed(CFDictionaryRef dict) {
    return dict->_count;
}

CF_INLINE void __CFDictionarySetNumBucketsUsed(CFMutableDictionaryRef dict, CFIndex v) {
    dict->_count = v;
}

CF_INLINE CFIndex __CFDictionaryNumBuckets(CFDictionaryRef dict) {
    return dict->_bucketsNum;
}

CF_INLINE void __CFDictionarySetNumBuckets(CFMutableDictionaryRef dict, CFIndex v) {
    dict->_bucketsNum = v;
}

CFTypeID CFDictionaryGetTypeID(void) {
    return __kCFDictionaryTypeID;
}

Boolean __CFDictionaryEqual(CFTypeRef cf1, CFTypeRef cf2) {
    CFDictionaryRef dict1 = (CFDictionaryRef)cf1;
    CFDictionaryRef dict2 = (CFDictionaryRef)cf2;
    struct __CFDictionaryBucket *buckets;
    CFIndex idx;
    CFIndex cnt, nbuckets;
    if (dict1 == dict2) return TRUE;
    cnt = __CFDictionaryCount(dict1);
    if (cnt != __CFDictionaryCount(dict2)) return FALSE;
    /* Need both callbacks faulted for comparison purposes */
    if (dict1->_callbacks.equal != dict2->_callbacks.equal) return FALSE;
    if (0 == cnt) return TRUE;
#warning CF: should the vcallbacks.equal functions be compared too?
    buckets = dict1->_buckets;
    nbuckets = __CFDictionaryNumBuckets(dict1);
    for (idx = 0; idx < nbuckets; idx++)
	if (__kCFHashBucketOccupied == __CFHashBucketState((struct __CFHashBucket *)&buckets[idx])) {
	    const void *value;
	    if (!CFDictionaryGetValueIfPresent(dict2, buckets[idx]._key, &value)) return FALSE;
	    if (buckets[idx]._value != value && dict1->_vcallbacks.equal && !INVOKE_CALLBACK3(dict1->_vcallbacks.equal, buckets[idx]._value, value, dict1->_context)) return FALSE;
	}
    return TRUE;
}

CFHashCode __CFDictionaryHash(CFTypeRef cf) {
    CFDictionaryRef dict = (CFDictionaryRef)cf;
    return __CFDictionaryCount(dict);
}

CFStringRef __CFDictionaryCopyDescription(CFTypeRef cf) {
    CFDictionaryRef dict = (CFDictionaryRef)cf;
    CFMutableStringRef result;
    CFIndex idx;
    CFIndex cnt, nbuckets;
    struct __CFDictionaryBucket *buckets;
    cnt = __CFDictionaryCount(dict);
    buckets = dict->_buckets;
    nbuckets = __CFDictionaryNumBuckets(dict);
    result = CFStringCreateMutable(dict->_allocator, 0);
    CFStringAppendFormat(result, NULL, CFSTR("<CFDictionary 0x%x [0x%x]>{count = %u, capacity = %u, pairs = (\n"), (UInt32)cf, dict->_allocator, cnt, __CFDictionaryCapacity(dict));
    for (idx = 0; idx < nbuckets; idx++) {
	if (__kCFHashBucketOccupied == __CFHashBucketState((struct __CFHashBucket *)&buckets[idx])) {
	    CFStringRef kDesc = NULL, vDesc = NULL;
	    if (NULL != dict->_callbacks.copyDescription) {
		kDesc = (CFStringRef)INVOKE_CALLBACK2(dict->_callbacks.copyDescription, buckets[idx]._key, dict->_context);
	    }
	    if (NULL != dict->_vcallbacks.copyDescription) {
		vDesc = (CFStringRef)INVOKE_CALLBACK2(dict->_vcallbacks.copyDescription, buckets[idx]._value, dict->_context);
	    }
	    if (NULL != kDesc && NULL != vDesc) {
		CFStringAppendFormat(result, NULL, CFSTR("\t%u : %@ = %@\n"), idx, kDesc, vDesc);
		CFRelease(kDesc);
		CFRelease(vDesc);
	    } else if (NULL != kDesc) {
		CFStringAppendFormat(result, NULL, CFSTR("\t%u : %@ = <0x%x>\n"), idx, kDesc, (UInt32)buckets[idx]._value);
		CFRelease(kDesc);
	    } else if (NULL != vDesc) {
		CFStringAppendFormat(result, NULL, CFSTR("\t%u : <0x%x> = %@\n"), idx, (UInt32)buckets[idx]._key, vDesc);
		CFRelease(vDesc);
	    } else {
		CFStringAppendFormat(result, NULL, CFSTR("\t%u : <0x%x> = <0x%x>\n"), idx, (UInt32)buckets[idx]._key, (UInt32)buckets[idx]._value);
	    }
	}
    }
    CFStringAppend(result, CFSTR(")}"));
    return result;
}

CFAllocatorRef __CFDictionaryGetAllocator(CFTypeRef cf) {
    CFDictionaryRef dict = (CFDictionaryRef)cf;
    return dict->_allocator;
}

void __CFDictionaryDeallocate(CFTypeRef cf) {
    CFMutableDictionaryRef dict = (CFMutableDictionaryRef)cf;
    CFAllocatorRef allocator = dict->_allocator;
    if (__CFMutableVariety(dict) != kCFMutable) {
	__CFSetMutableVariety(dict, kCFFixedMutable);
    }
    CFDictionaryRemoveAllValues(dict);
    if (__CFMutableVariety(dict) == kCFMutable) {
	CFAllocatorDeallocate(allocator, dict->_buckets);
    }
    CFAllocatorDeallocate(allocator, dict);
    CFRelease(allocator);
}

CFMutableDictionaryRef _CFDictionaryInit(CFAllocatorRef allocator, UInt32 flags, CFIndex capacity, const void **keys, const void **values, CFIndex numValues, const CFDictionaryKeyCallBacks *keyCallBacks, const CFDictionaryValueCallBacks *valueCallBacks) {
    CFMutableDictionaryRef memory;
    UInt32 size;
    SInt32 idx;
    __CFGenericValidateMutabilityFlags(flags);
    CFAssert2(0 <= capacity, __kCFLogAssertion, "%s(): capacity (%d) cannot be less than zero", __PRETTY_FUNCTION__, capacity);
    CFAssert3(kCFFixedMutable != __CFMutableVarietyFromFlags(flags) || numValues <= capacity, __kCFLogAssertion, "%s(): for kCFFixedMutable type, capacity (%d) must be greater than or equal to number of initial elements (%d)", __PRETTY_FUNCTION__, capacity, numValues);
    CFAssert2(0 <= numValues, __kCFLogAssertion, "%s(): numValues (%d) cannot be less than zero", __PRETTY_FUNCTION__, numValues);
    size = sizeof(struct __CFDictionary);
    if (__CFMutableVarietyFromFlags(flags) != kCFMutable) {
       size += sizeof(struct __CFDictionaryBucket) * __CFHashingNumBucketsForCapacity(capacity);
    }
    allocator = (NULL == allocator) ? CFRetain(CFAllocatorGetDefault()) : CFRetain(allocator);
    memory = CFAllocatorAllocate(allocator, size, 0);
    if (NULL == memory) {
	CFRelease(allocator);
	return NULL;
    }
    __CFGenericInitBase(memory, __CFDictionaryIsa(), __kCFDictionaryTypeID);
    memory->_allocator = allocator;
    memory->_context = NULL;
    switch (__CFMutableVarietyFromFlags(flags)) {
    case kCFMutable:
	__CFDictionarySetCapacity(memory, __CFHashingRoundUpCapacity(1));
	__CFDictionarySetNumBuckets(memory, __CFHashingNumBucketsForCapacity(__CFHashingRoundUpCapacity(1)));
	__CFSetLastAllocationEventName(memory, "CFDictionary (mutable-variable)");
	memory->_buckets = CFAllocatorAllocate(allocator, __CFDictionaryNumBuckets(memory) * sizeof(struct __CFDictionaryBucket), 0);
	if (NULL == memory->_buckets) {
	    CFAllocatorDeallocate(allocator, memory);
	    CFRelease(allocator);
	    return NULL;
	}
	__CFSetLastAllocationEventName(memory->_buckets, "CFDictionary (store)");
	break;
    case kCFFixedMutable:
	/* Don't round up capacity */
	__CFDictionarySetCapacity(memory, capacity);
	__CFDictionarySetNumBuckets(memory, __CFHashingNumBucketsForCapacity(capacity));
	memory->_buckets = (struct __CFDictionaryBucket *)((SInt8 *)memory + sizeof(struct __CFDictionary));
	__CFSetLastAllocationEventName(memory, "CFDictionary (mutable-fixed)");
	break;
    case kCFImmutable:
	/* Don't round up capacity */
	__CFDictionarySetCapacity(memory, capacity);
	__CFDictionarySetNumBuckets(memory, __CFHashingNumBucketsForCapacity(capacity));
	memory->_buckets = (struct __CFDictionaryBucket *)((SInt8 *)memory + sizeof(struct __CFDictionary));
	__CFSetLastAllocationEventName(memory, "CFDictionary (immutable)");
	break;
    }
    __CFDictionarySetNumBucketsUsed(memory, 0);
    __CFDictionarySetCount(memory, 0);
    for (idx = __CFDictionaryNumBuckets(memory); idx--;) {
	__CFHashBucketSetStateAndKeyHash((struct __CFHashBucket *)&(memory->_buckets[idx]), __kCFHashBucketEmpty, 0);
    }
    if (keyCallBacks) {
	memory->_callbacks.retain = keyCallBacks->retain;
	memory->_callbacks.release = keyCallBacks->release;
	memory->_callbacks.copyDescription = keyCallBacks->copyDescription;
	memory->_callbacks.equal = keyCallBacks->equal;
	memory->_callbacks.hash = keyCallBacks->hash;
        FAULT_CALLBACK((void **)&(memory->_callbacks.retain));
        FAULT_CALLBACK((void **)&(memory->_callbacks.release));
        FAULT_CALLBACK((void **)&(memory->_callbacks.copyDescription));
        FAULT_CALLBACK((void **)&(memory->_callbacks.hash));
        FAULT_CALLBACK((void **)&(memory->_callbacks.equal));
    } else {
	memory->_callbacks.retain = 0;
	memory->_callbacks.release = 0;
	memory->_callbacks.copyDescription = 0;
	memory->_callbacks.equal = 0;
	memory->_callbacks.hash = 0;
    }
    if (valueCallBacks) {
	memory->_vcallbacks.retain = valueCallBacks->retain;
	memory->_vcallbacks.release = valueCallBacks->release;
	memory->_vcallbacks.copyDescription = valueCallBacks->copyDescription;
	memory->_vcallbacks.equal = valueCallBacks->equal;
        FAULT_CALLBACK((void **)&(memory->_vcallbacks.retain));
        FAULT_CALLBACK((void **)&(memory->_vcallbacks.release));
        FAULT_CALLBACK((void **)&(memory->_vcallbacks.copyDescription));
        FAULT_CALLBACK((void **)&(memory->_vcallbacks.equal));
    } else {
	memory->_vcallbacks.retain = 0;
	memory->_vcallbacks.release = 0;
	memory->_vcallbacks.copyDescription = 0;
	memory->_vcallbacks.equal = 0;
    }
    if (__CFMutableVarietyFromFlags(flags) != kCFMutable) {
        __CFSetMutableVariety(memory, kCFFixedMutable);
    } else {
        __CFSetMutableVariety(memory, kCFMutable);
    }
    for (idx = 0; idx < numValues; idx++)
	CFDictionaryAddValue(memory, keys[idx], values[idx]);
    __CFSetMutableVariety(memory, __CFMutableVarietyFromFlags(flags));
    return memory;
}

void _CFDictionarySetContext(CFDictionaryRef dict, void *context) {
    ((struct __CFDictionary *)dict)->_context = context;
}

CFDictionaryRef CFDictionaryCreate(CFAllocatorRef allocator, const void **keys, const void **values, CFIndex numValues, const CFDictionaryKeyCallBacks *keyCallBacks, const CFDictionaryValueCallBacks *valueCallBacks) {
   return _CFDictionaryInit(allocator, kCFImmutable, numValues, keys, values, numValues, keyCallBacks, valueCallBacks);
}

CFMutableDictionaryRef CFDictionaryCreateMutable(CFAllocatorRef allocator, CFIndex capacity, const CFDictionaryKeyCallBacks *keyCallBacks, const CFDictionaryValueCallBacks *valueCallBacks) {
   return _CFDictionaryInit(allocator, (0 == capacity) ? kCFMutable : kCFFixedMutable, capacity, NULL, NULL, 0, keyCallBacks, valueCallBacks);
}

CFDictionaryRef CFDictionaryCreateCopy(CFAllocatorRef allocator, CFDictionaryRef dict) {
#warning CF: this currently only allows CFDictionarys, and not NSDictionary subclasses, to be passed as the dict argument
    CFMutableDictionaryRef newDict;
    CFIndex cnt;
    const void **keys, **values, *buffer1[128], *buffer2[128];
    __CFGenericValidateType(dict, __kCFDictionaryTypeID);
    cnt = CFDictionaryGetCount(dict);
    allocator = (NULL == allocator) ? CFRetain(CFAllocatorGetDefault()) : CFRetain(allocator);
    keys = (cnt <= 128) ? buffer1 : CFAllocatorAllocate(allocator, cnt * sizeof(void *), 0);
    if (keys != buffer1) __CFSetLastAllocationEventName(keys, "CFDictionary (temp)");
    values = (cnt <= 128) ? buffer2 : CFAllocatorAllocate(allocator, cnt * sizeof(void *), 0);
    if (values != buffer2) __CFSetLastAllocationEventName(values, "CFDictionary (temp)");
    CFDictionaryGetKeysAndValues(dict, keys, values);
    newDict = _CFDictionaryInit(allocator, kCFImmutable, cnt, keys, values, cnt, &(dict->_callbacks), &(dict->_vcallbacks));
    if (keys != buffer1) CFAllocatorDeallocate(allocator, keys);
    if (values != buffer2) CFAllocatorDeallocate(allocator, values);
    CFRelease(allocator);
    return newDict;
}

CFMutableDictionaryRef CFDictionaryCreateMutableCopy(CFAllocatorRef allocator, CFIndex capacity, CFDictionaryRef dict) {
#warning CF: this currently only allows CFDictionarys, and not NSDictionary subclasses, to be passed as the dict argument
    CFMutableDictionaryRef newDict;
    CFIndex cnt;
    const void **keys, **values, *buffer1[128], *buffer2[128];
    __CFGenericValidateType(dict, __kCFDictionaryTypeID);
    cnt = CFDictionaryGetCount(dict);
    allocator = (NULL == allocator) ? CFRetain(CFAllocatorGetDefault()) : CFRetain(allocator);
    keys = (cnt <= 128) ? buffer1 : CFAllocatorAllocate(allocator, cnt * sizeof(void *), 0);
    if (keys != buffer1) __CFSetLastAllocationEventName(keys, "CFDictionary (temp)");
    values = (cnt <= 128) ? buffer2 : CFAllocatorAllocate(allocator, cnt * sizeof(void *), 0);
    if (values != buffer2) __CFSetLastAllocationEventName(values, "CFDictionary (temp)");
    CFDictionaryGetKeysAndValues(dict, keys, values);
    newDict = _CFDictionaryInit(allocator, (0 == capacity) ? kCFMutable : kCFFixedMutable, capacity, keys, values, cnt, &(dict->_callbacks), &(dict->_vcallbacks));
    if (keys != buffer1) CFAllocatorDeallocate(allocator, keys);
    if (values != buffer2) CFAllocatorDeallocate(allocator, values);
    CFRelease(allocator);
    return newDict;
}

CFIndex CFDictionaryGetCount(CFDictionaryRef dict) {
    CF_OBJC_FUNCDISPATCH0(Dictionary, CFIndex, dict, "count");
    __CFGenericValidateType(dict, __kCFDictionaryTypeID);
    return __CFDictionaryCount(dict);
}

CFIndex CFDictionaryGetCountOfKey(CFDictionaryRef dict, const void *key) {
    CFHashCode keyHash;
    const struct __CFDictionaryBucket *match;
    CF_OBJC_FUNCDISPATCH1(Dictionary, CFIndex, dict, "countForKey:", (id)key);
    __CFGenericValidateType(dict, __kCFDictionaryTypeID);
    if (0 == __CFDictionaryCount(dict)) return 0;
    keyHash = dict->_callbacks.hash ? (CFHashCode)INVOKE_CALLBACK2(dict->_callbacks.hash, key, dict->_context) : (CFHashCode)key;
    keyHash &= __CFBitfieldMaxValue(29, 0);
    __CFHashFindBuckets(dict, __kCFDictionaryTypeID, key, keyHash, (const void **)&match, NULL);
    if (!match) return 0;
    return 1;
}

CFIndex CFDictionaryGetCountOfValue(CFDictionaryRef dict, const void *value) {
    Boolean (*equal)(const void *ptr1, const void *ptr2);
    struct __CFDictionaryBucket *buckets;
    CFIndex idx;
    CFIndex cnt, nbuckets;
    CF_OBJC_FUNCDISPATCH1(Dictionary, CFIndex, dict, "countForObject:", (id)value);
    __CFGenericValidateType(dict, __kCFDictionaryTypeID);
    buckets = dict->_buckets;
    nbuckets = __CFDictionaryNumBuckets(dict);
    equal = dict->_vcallbacks.equal;
    for (idx = 0, cnt = 0; idx < nbuckets; idx++)
	if (__kCFHashBucketOccupied == __CFHashBucketState((struct __CFHashBucket *)&buckets[idx]))
	    if (((UInt32)value == (UInt32)buckets[idx]._value) || (equal && INVOKE_CALLBACK3(equal, value, buckets[idx]._value, dict->_context)))
		cnt++;
    return cnt;
}

Boolean CFDictionaryContainsKey(CFDictionaryRef dict, const void *key) {
    CF_OBJC_FUNCDISPATCH1(Dictionary, Boolean, dict, "containsKey:", (id)key);
    __CFGenericValidateType(dict, __kCFDictionaryTypeID);
    return (CFDictionaryGetCountOfKey(dict, key) != 0) ? TRUE : FALSE;
}

Boolean CFDictionaryContainsValue(CFDictionaryRef dict, const void *value) {
    CF_OBJC_FUNCDISPATCH1(Dictionary, Boolean, dict, "containsObject:", (id)value);
    __CFGenericValidateType(dict, __kCFDictionaryTypeID);
    return (CFDictionaryGetCountOfValue(dict, value) != 0) ? TRUE : FALSE;
}

Boolean CFDictionaryGetKeyIfPresent(CFDictionaryRef dict, const void *key, const void **actualkey) {
    CFHashCode keyHash;
    const struct __CFDictionaryBucket *match;
#warning not bridged
    __CFGenericValidateType(dict, __kCFDictionaryTypeID);
    if (0 == __CFDictionaryCount(dict)) return FALSE;
    keyHash = dict->_callbacks.hash ? (CFHashCode)INVOKE_CALLBACK2(dict->_callbacks.hash, key, dict->_context) : (CFHashCode)key;
    keyHash &= __CFBitfieldMaxValue(29, 0);
    __CFHashFindBuckets(dict, __kCFDictionaryTypeID, key, keyHash, (const void **)&match, NULL);
    if (!match) return FALSE;
    if (actualkey) *actualkey = match->_key;
    return TRUE;
}

Boolean CFDictionaryGetValueIfPresent(CFDictionaryRef dict, const void *key, const void **value) {
    CFHashCode keyHash;
    const struct __CFDictionaryBucket *match;
    CF_OBJC_FUNCDISPATCH2(Dictionary, Boolean, dict, "_getValue:forKey:", (id *)value, (id)key);
    __CFGenericValidateType(dict, __kCFDictionaryTypeID);
    if (0 == __CFDictionaryCount(dict)) return FALSE;
    keyHash = dict->_callbacks.hash ? (CFHashCode)INVOKE_CALLBACK2(dict->_callbacks.hash, key, dict->_context) : (CFHashCode)key;
    keyHash &= __CFBitfieldMaxValue(29, 0);
    __CFHashFindBuckets(dict, __kCFDictionaryTypeID, key, keyHash, (const void **)&match, NULL);
    if (!match) return FALSE;
    if (value) *value = match->_value;
    return TRUE;
}

const void *CFDictionaryGetValue(CFDictionaryRef dict, const void *key) {
    CFHashCode keyHash;
    const struct __CFDictionaryBucket *match;
    CF_OBJC_FUNCDISPATCH1(Dictionary, void *, dict, "objectForKey", (id)key);
    __CFGenericValidateType(dict, __kCFDictionaryTypeID);
    if (0 == __CFDictionaryCount(dict)) return NULL;
    keyHash = dict->_callbacks.hash ? (CFHashCode)INVOKE_CALLBACK2(dict->_callbacks.hash, key, dict->_context) : (CFHashCode)key;
    keyHash &= __CFBitfieldMaxValue(29, 0);
    __CFHashFindBuckets(dict, __kCFDictionaryTypeID, key, keyHash, (const void **)&match, NULL);
    return match ? (const void *)(match->_value) : NULL;
}

void CFDictionaryGetKeysAndValues(CFDictionaryRef dict, const void **keys, const void **values) {
    struct __CFDictionaryBucket *buckets;
    CFIndex idx;
    CFIndex cnt, nbuckets;
    CF_OBJC_FUNCDISPATCH2(Dictionary, void, dict, "getObjects:andKeys:", (id *)values, (id *)keys);
    __CFGenericValidateType(dict, __kCFDictionaryTypeID);
    buckets = dict->_buckets;
    nbuckets = __CFDictionaryNumBuckets(dict);
    for (idx = 0; idx < nbuckets; idx++)
	if (__kCFHashBucketOccupied == __CFHashBucketState((struct __CFHashBucket *)&buckets[idx]))
	    for (cnt = 1; cnt--;) {
		if (keys) *keys++ = buckets[idx]._key;
		if (values) *values++ = buckets[idx]._value;
	    }
}

void CFDictionaryApplyFunction(CFDictionaryRef dict, CFDictionaryApplierFunction applier, void *context) {
    struct __CFDictionaryBucket *buckets;
    CFIndex idx;
    CFIndex cnt, nbuckets;
    FAULT_CALLBACK((void **)&(applier));
    CF_OBJC_FUNCDISPATCH2(Dictionary, void, dict, "_apply:context:", applier, context);
    __CFGenericValidateType(dict, __kCFDictionaryTypeID);
    buckets = dict->_buckets;
    nbuckets = __CFDictionaryNumBuckets(dict);
    for (idx = 0; idx < nbuckets; idx++)
	if (__kCFHashBucketOccupied == __CFHashBucketState((struct __CFHashBucket *)&buckets[idx]))
	    for (cnt = 1; cnt--;)
		INVOKE_CALLBACK3(applier, buckets[idx]._key, buckets[idx]._value, context);
}

static void __CFDictionaryGrow(CFMutableDictionaryRef dict, CFIndex numNewValues) {
    struct __CFDictionaryBucket *oldbuckets = dict->_buckets;
    SInt32 idx, oldnbuckets = __CFDictionaryNumBuckets(dict);
    CFIndex oldCount = __CFDictionaryCount(dict);
    CFIndex capacity = __CFHashingRoundUpCapacity(oldCount + numNewValues);
    __CFDictionarySetCapacity(dict, capacity);
    __CFDictionarySetNumBuckets(dict, __CFHashingNumBucketsForCapacity(capacity));
    dict->_buckets = CFAllocatorAllocate(dict->_allocator, __CFDictionaryNumBuckets(dict) * sizeof(struct __CFDictionaryBucket), 0);
    if (NULL == dict->_buckets) __CFAbort();
    __CFSetLastAllocationEventName(dict->_buckets, "CFDictionary (store)");
    for (idx = __CFDictionaryNumBuckets(dict); idx--;) {
	__CFHashBucketSetStateAndKeyHash((struct __CFHashBucket *)&(dict->_buckets[idx]), __kCFHashBucketEmpty, 0);
    }
    for (idx = 0; idx < oldnbuckets; idx++) {
	if (__kCFHashBucketOccupied == __CFHashBucketState((struct __CFHashBucket *)&oldbuckets[idx])) {
	    struct __CFDictionaryBucket *match, *nomatch;
	    CFHashCode keyHash = __CFHashBucketKeyHash((struct __CFHashBucket *)&oldbuckets[idx]);
	    __CFHashFindBuckets(dict, __kCFDictionaryTypeID, oldbuckets[idx]._key, keyHash, (const void **)&match, (const void **)&nomatch);
	    CFAssert3(!match, __kCFLogAssertion, "%s(): two objects (0x%x, 0x%x) now hash to the same slot; mutable object changed while in table or hash value is not immutable", __PRETTY_FUNCTION__, oldbuckets[idx]._key, match->_key);
	    __CFHashBucketSetStateAndKeyHash((struct __CFHashBucket *)nomatch, __kCFHashBucketOccupied, keyHash);
	    nomatch->_key = oldbuckets[idx]._key;
	    nomatch->_value = oldbuckets[idx]._value;
	}
    }
    CFAssert1(__CFDictionaryCount(dict) == oldCount, __kCFLogAssertion, "%s(): dictionary count differs after rehashing; error", __PRETTY_FUNCTION__);
    CFAllocatorDeallocate(dict->_allocator, oldbuckets);
}

void CFDictionaryAddValue(CFMutableDictionaryRef dict, const void *key, const void *value) {
    CFHashCode keyHash;
    struct __CFDictionaryBucket *match, *nomatch;
    CF_OBJC_FUNCDISPATCH2(Dictionary, void, dict, "_addObject:forKey:", value, key);
    switch (__CFMutableVariety(dict)) {
    case kCFMutable:
	if (__CFDictionaryNumBucketsUsed(dict) == __CFDictionaryCapacity(dict))
	    __CFDictionaryGrow(dict, 1);
	break;
    case kCFFixedMutable:
	if (__CFDictionaryCount(dict) == __CFDictionaryCapacity(dict)) __CFAbort();
	break;
    case kCFImmutable:
    default: __CFAbort();
    }
    keyHash = dict->_callbacks.hash ? (CFHashCode)INVOKE_CALLBACK2(dict->_callbacks.hash, key, dict->_context) : (CFHashCode)key;
    keyHash &= __CFBitfieldMaxValue(29, 0);
    __CFHashFindBuckets(dict, __kCFDictionaryTypeID, key, keyHash, (const void **)&match, (const void **)&nomatch);
    if (!match) {
	__CFHashBucketSetStateAndKeyHash((struct __CFHashBucket *)nomatch, __kCFHashBucketOccupied, keyHash);
	if (dict->_callbacks.retain) {
	    nomatch->_key = (void *)INVOKE_CALLBACK3(dict->_callbacks.retain, dict->_allocator, key, dict->_context);
	} else {
	    nomatch->_key = key;
	}
	if (dict->_vcallbacks.retain) {
	    nomatch->_value = (void *)INVOKE_CALLBACK3(dict->_vcallbacks.retain, dict->_allocator, value, dict->_context);
	} else {
	    nomatch->_value = value;
	}
	__CFDictionarySetNumBucketsUsed(dict, __CFDictionaryNumBucketsUsed(dict) + 1);
	__CFDictionarySetCount(dict, __CFDictionaryCount(dict) + 1);
    }
}

void CFDictionaryReplaceValue(CFMutableDictionaryRef dict, const void *key, const void *value) {
    CFHashCode keyHash;
    struct __CFDictionaryBucket *match;
    CF_OBJC_FUNCDISPATCH2(Dictionary, void, dict, "_replaceObject:forKey:", value, key);
    switch (__CFMutableVariety(dict)) {
    case kCFMutable:
    case kCFFixedMutable:
	break;
    case kCFImmutable:
    default: __CFAbort();
    }
    keyHash = dict->_callbacks.hash ? (CFHashCode)INVOKE_CALLBACK2(dict->_callbacks.hash, key, dict->_context) : (CFHashCode)key;
    keyHash &= __CFBitfieldMaxValue(29, 0);
    __CFHashFindBuckets(dict, __kCFDictionaryTypeID, key, keyHash, (const void **)&match, NULL);
    if (match) {
	const void *newValue;
	if (dict->_vcallbacks.retain) {
	    newValue = (void *)INVOKE_CALLBACK3(dict->_vcallbacks.retain, dict->_allocator, value, dict->_context);
	} else {
	    newValue = value;
	}
	if (dict->_vcallbacks.release)
	    INVOKE_CALLBACK3(dict->_vcallbacks.release, dict->_allocator, match->_value, dict->_context);
	match->_value = newValue;
    }
}

void CFDictionarySetValue(CFMutableDictionaryRef dict, const void *key, const void *value) {
    CFHashCode keyHash;
    struct __CFDictionaryBucket *match, *nomatch;
    CF_OBJC_FUNCDISPATCH2(Dictionary, void, dict, "setObject:forKey:", value, key);
    switch (__CFMutableVariety(dict)) {
    case kCFMutable:
	if (__CFDictionaryNumBucketsUsed(dict) == __CFDictionaryCapacity(dict))
	    __CFDictionaryGrow(dict, 1);
	break;
    case kCFFixedMutable:
	break;
    case kCFImmutable:
    default: __CFAbort();
    }
    keyHash = dict->_callbacks.hash ? (CFHashCode)INVOKE_CALLBACK2(dict->_callbacks.hash, key, dict->_context) : (CFHashCode)key;
    keyHash &= __CFBitfieldMaxValue(29, 0);
    __CFHashFindBuckets(dict, __kCFDictionaryTypeID, key, keyHash, (const void **)&match, (const void **)&nomatch);
    if (!match) {
	if (kCFFixedMutable == __CFMutableVariety(dict) && __CFDictionaryCount(dict) == __CFDictionaryCapacity(dict)) __CFAbort();
	__CFHashBucketSetStateAndKeyHash((struct __CFHashBucket *)nomatch, __kCFHashBucketOccupied, keyHash);
	if (dict->_callbacks.retain) {
	    nomatch->_key = (void *)INVOKE_CALLBACK3(dict->_callbacks.retain, dict->_allocator, key, dict->_context);
	} else {
	    nomatch->_key = key;
	}
	if (dict->_vcallbacks.retain) {
	    nomatch->_value = (void *)INVOKE_CALLBACK3(dict->_vcallbacks.retain, dict->_allocator, value, dict->_context);
	} else {
	    nomatch->_value = value;
	}
	__CFDictionarySetNumBucketsUsed(dict, __CFDictionaryNumBucketsUsed(dict) + 1);
	__CFDictionarySetCount(dict, __CFDictionaryCount(dict) + 1);
    } else {
	const void *newValue;
	if (dict->_vcallbacks.retain) {
	    newValue = (void *)INVOKE_CALLBACK3(dict->_vcallbacks.retain, dict->_allocator, value, dict->_context);
	} else {
	    newValue = value;
	}
	if (dict->_vcallbacks.release)
	    INVOKE_CALLBACK3(dict->_vcallbacks.release, dict->_allocator, match->_value, dict->_context);
	match->_value = newValue;
    }
}

void CFDictionaryRemoveValue(CFMutableDictionaryRef dict, const void *key) {
    CFHashCode keyHash;
    struct __CFDictionaryBucket *match;
    CF_OBJC_FUNCDISPATCH1(Dictionary, void, dict, "removeObjectForKey:", key);
    switch (__CFMutableVariety(dict)) {
    case kCFMutable:
    case kCFFixedMutable:
	break;
    case kCFImmutable:
    default: __CFAbort();
    }
    keyHash = dict->_callbacks.hash ? (CFHashCode)INVOKE_CALLBACK2(dict->_callbacks.hash, key, dict->_context) : (CFHashCode)key;
    keyHash &= __CFBitfieldMaxValue(29, 0);
    __CFHashFindBuckets(dict, __kCFDictionaryTypeID, key, keyHash, (const void **)&match, NULL);
    if (match) {
	__CFHashBucketSetStateAndKeyHash((struct __CFHashBucket *)match, __kCFHashBucketDeleted, 0);
	if (dict->_callbacks.release)
	    INVOKE_CALLBACK3(dict->_callbacks.release, dict->_allocator, match->_key, dict->_context);
	match->_key = NULL;
	if (dict->_vcallbacks.release)
	    INVOKE_CALLBACK3(dict->_vcallbacks.release, dict->_allocator, match->_value, dict->_context);
	match->_value = NULL;
	__CFDictionarySetNumBucketsUsed(dict, __CFDictionaryNumBucketsUsed(dict) - 1);
	__CFDictionarySetCount(dict, __CFDictionaryCount(dict) - 1);
    }
}

void CFDictionaryRemoveAllValues(CFMutableDictionaryRef dict) {
    struct __CFDictionaryBucket *buckets;
    SInt32 idx, nbuckets;
    CF_OBJC_FUNCDISPATCH0(Dictionary, void, dict, "removeAllObjects");
    buckets = dict->_buckets;
    nbuckets = __CFDictionaryNumBuckets(dict);
    switch (__CFMutableVariety(dict)) {
    case kCFMutable:
    case kCFFixedMutable:
	break;
    case kCFImmutable:
    default: __CFAbort();
    }
    for (idx = 0; idx < nbuckets; idx++) {
	if (__kCFHashBucketOccupied == __CFHashBucketState((struct __CFHashBucket *)&buckets[idx])) {
	    if (dict->_callbacks.release)
		INVOKE_CALLBACK3(dict->_callbacks.release, dict->_allocator, buckets[idx]._key, dict->_context);
	    buckets[idx]._key = NULL;
	    if (dict->_vcallbacks.release)
		INVOKE_CALLBACK3(dict->_vcallbacks.release, dict->_allocator, buckets[idx]._value, dict->_context);
	    buckets[idx]._value = NULL;
	}
	__CFHashBucketSetStateAndKeyHash((struct __CFHashBucket *)&buckets[idx], __kCFHashBucketEmpty, 0);
    }
    __CFDictionarySetNumBucketsUsed(dict, 0);
    __CFDictionarySetCount(dict, 0);
}


/* Hashing theory: double hashing is used, with the hash function being:
	h(keyhash, i) = (h1(keyhash) + i*h2(keyhash)) mod #buckets
where h1 and h2 are secondary hash functions.  h1 is used to compute the
starting probe position and is defined to be:
	h1(keyhash) = keyhash mod #buckets
h2 is used as the skip or increment value as probing progresses:
	h2(keyhash) = ((keyhash div 2) mod #buckets) - 1 + ((keyhash div 2) mod 2)
Since h2 also depends on the hash of the key, many more probe sequences
are possible than with, say, linear probing.  The number of buckets in
the tables is always a power of two and h2 is always odd to ensure that
the value of h2 is always relatively prime to the size of the table.
Other h2() functions (as long as the value is always odd) can be chosen.

Another way to ensure relatively primality would be to define h2 as:
	h2(keyhash) = 1 + (keyhash mod (#buckets - 1))
and make the number of buckets in the tables always prime.  But the
former method is chosen here, because it is somewhat faster.

Other possible optimizations:
- On each search, swap the found bucket contents with the starting bucket's contents
- Better h2()
- Rehash after #buckets/4 removes to cut down on number of deleted entries
- bags may used many less buckets than ordinary capacity allowed for, due to equal objects

*/

static void __CFHashFindBuckets(const void *hashing, UInt32 hashingType, const void *key, CFHashCode keyHash, const void **match, const void **nomatch) {
    const void *buckets = NULL;
    Boolean (*equator)(const void *ptr1, const void *ptr2) = NULL;
    void *context = NULL;
    UInt32 bucketSize = 0, mask = 0, start, probe, probeskip;

    if (!match && !nomatch) __CFAbort();
    if (hashingType == __kCFDictionaryTypeID) {
	buckets = ((CFMutableDictionaryRef)hashing)->_buckets;
	equator = ((CFMutableDictionaryRef)hashing)->_callbacks.equal; 
	bucketSize = sizeof(struct __CFDictionaryBucket);
	mask = __CFDictionaryNumBuckets(hashing) - 1;
	context = ((CFMutableDictionaryRef)hashing)->_context;
    } else if (hashingType == __kCFSetTypeID) {
	buckets = ((CFMutableSetRef)hashing)->_buckets;
	equator = ((CFMutableSetRef)hashing)->_callbacks.equal;
	bucketSize = sizeof(struct __CFSetBucket);
	mask = __CFSetNumBuckets(hashing) - 1;
	context = ((CFMutableSetRef)hashing)->_context;
    } else if (hashingType == __kCFBagTypeID) {
	buckets = ((CFMutableBagRef)hashing)->_buckets;
	equator = ((CFMutableBagRef)hashing)->_callbacks.equal;
	bucketSize = sizeof(struct __CFBagBucket);
	mask = __CFBagNumBuckets(hashing) - 1;
    } else {
	__CFAbort();
    }
    probe = start = keyHash & mask;
    probeskip = ((keyHash >> 1) & mask) - 1 + ((keyHash >> 1) & 1);
    if (nomatch) *nomatch = NULL;
    if (match) *match = NULL;
    for (;;) {
	const void *currentBucket = (SInt8*)buckets + probe * bucketSize;
	UInt32 currentStatus = __CFHashBucketState(currentBucket);
	if (__kCFHashBucketEmpty == currentStatus) {
	    if (nomatch && !*nomatch) *nomatch = currentBucket;
	    return;
	} else if (__kCFHashBucketDeleted == currentStatus) {
	    if (nomatch && !*nomatch) *nomatch = currentBucket;
	} else if (__kCFHashBucketOccupied == currentStatus) {
	    if (match && !*match && (__CFHashBucketKey(currentBucket) == key || (equator && __CFHashBucketKeyHash(currentBucket) == keyHash && INVOKE_CALLBACK3(equator, __CFHashBucketKey(currentBucket), key, context)))) *match = currentBucket;
	}
	if ((!match || *match) && (!nomatch || *nomatch)) return;
	probe = (probe + probeskip) & mask;
	if (start == probe) return;
    }
}

