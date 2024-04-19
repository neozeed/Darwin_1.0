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
/*	CFData.c
	Copyright 1998-2000, Apple, Inc. All rights reserved.
	Responsibility: Christopher Kane
*/

#include <CoreFoundation/CFData.h>
#include "CFInternal.h"

struct __CFData {
    CFRuntimeBase _base;
    CFAllocatorRef _allocator;
    CFIndex _length;	/* number of bytes */
    CFIndex _capacity;	/* maximum number of bytes */
    CFAllocatorRef _bytesDeallocator;	/* used only for immutable; if NULL, no deallocation */
    UInt8 *_bytes;
};

/* Bits 3-2 are used for mutability variation */

CF_INLINE CFIndex __CFDataLength(CFDataRef data) {
    return data->_length;
}

CF_INLINE void __CFDataSetLength(CFMutableDataRef data, CFIndex v) {
    /* for a CFData, _bytesUsed == _length */
}

CF_INLINE CFIndex __CFDataCapacity(CFDataRef data) {
    return data->_capacity;
}

CF_INLINE void __CFDataSetCapacity(CFMutableDataRef data, CFIndex v) {
    /* for a CFData, _bytesNum == _capacity */
}

CF_INLINE CFIndex __CFDataNumBytesUsed(CFDataRef data) {
    return data->_length;
}

CF_INLINE void __CFDataSetNumBytesUsed(CFMutableDataRef data, CFIndex v) {
    data->_length = v;
}

CF_INLINE CFIndex __CFDataNumBytes(CFDataRef data) {
    return data->_capacity;
}

CF_INLINE void __CFDataSetNumBytes(CFMutableDataRef data, CFIndex v) {
    data->_capacity = v;
}

CF_INLINE CFIndex __CFDataRoundUpCapacity(CFIndex capacity) {
    if (capacity < 16) return 16;
#warning CF: quite probably, this doubling should slow as the data gets larger and larger; should not use strict doubling
    return (1 << (CFLog2(capacity) + 1));
}

CF_INLINE CFIndex __CFDataNumBytesForCapacity(CFIndex capacity) {
    return capacity;
}

#if defined(DEBUG)
CF_INLINE void __CFDataValidateRange(CFDataRef data, CFRange range, char *func) {
    CFAssert2(0 <= range.location && range.location <= __CFDataLength(data), __kCFLogAssertion, "%s(): range.location index (%d) out of bounds", func, range.location);
    CFAssert2(0 <= range.length, __kCFLogAssertion, "%s(): length (%d) cannot be less than zero", func, range.length);
    CFAssert2(range.location + range.length <= __CFDataLength(data), __kCFLogAssertion, "%s(): ending index (%d) out of bounds", func, range.location + range.length);
}
#else
#define __CFDataValidateRange(a,r,f)
#endif

CFTypeID CFDataGetTypeID(void) {
    return __kCFDataTypeID;
}

Boolean __CFDataEqual(CFTypeRef cf1, CFTypeRef cf2) {
    CFDataRef data1 = (CFDataRef)cf1;
    CFDataRef data2 = (CFDataRef)cf2;
    CFIndex length;
    length = __CFDataLength(data1);
    if (length != __CFDataLength(data2)) return FALSE;
    return CFCompareMemory(data1->_bytes, data2->_bytes, length);
}

CFHashCode __CFDataHash(CFTypeRef cf) {
    CFDataRef data = (CFDataRef)cf;
    return CFHashBytes(data->_bytes, __CFMin(__CFDataLength(data), 16));
}

CFStringRef __CFDataCopyDescription(CFTypeRef cf) {
    CFDataRef data = (CFDataRef)cf;
    CFMutableStringRef result;
    CFIndex idx;
    CFIndex len;
    const UInt8 *bytes;
    len = __CFDataLength(data);
    bytes = data->_bytes;
    result = CFStringCreateMutable(data->_allocator, 0);
    CFStringAppendFormat(result, NULL, CFSTR("<CFData 0x%x [0x%x]>{length = %u, capacity = %u, bytes = 0x"), (UInt32)cf, (UInt32)data->_allocator, len, __CFDataCapacity(data));
    if (24 < len) {
        for (idx = 0; idx < 16; idx += 4) {
	    CFStringAppendFormat(result, NULL, CFSTR("%02x%02x%02x%02x"), bytes[idx], bytes[idx + 1], bytes[idx + 2], bytes[idx + 3]);
	}
        CFStringAppend(result, CFSTR(" ... "));
        for (idx = len - 8; idx < len; idx += 4) {
	    CFStringAppendFormat(result, NULL, CFSTR("%02x%02x%02x%02x"), bytes[idx], bytes[idx + 1], bytes[idx + 2], bytes[idx + 3]);
	}
    } else {
        for (idx = 0; idx < len; idx++) {
	    CFStringAppendFormat(result, NULL, CFSTR("%02x"), bytes[idx]);
	}
    }
    CFStringAppend(result, CFSTR("}"));
    return result;
}

CFAllocatorRef __CFDataGetAllocator(CFTypeRef cf) {
    CFDataRef data = (CFDataRef)cf;
    return data->_allocator;
}

void __CFDataDeallocate(CFTypeRef cf) {
    CFMutableDataRef data = (CFMutableDataRef)cf;
    CFAllocatorRef allocator = data->_allocator;
    switch (__CFMutableVariety(data)) {
    case kCFMutable:
	CFAllocatorDeallocate(allocator, data->_bytes);
	break;
    case kCFFixedMutable:
	break;
    case kCFImmutable:
	if (NULL != data->_bytesDeallocator) {
	    CFAllocatorDeallocate(data->_bytesDeallocator, data->_bytes);
	    CFRelease(data->_bytesDeallocator);
	}
	break;
    }
    CFAllocatorDeallocate(allocator, data);
    CFRelease(allocator);
}

static CFMutableDataRef __CFDataInit(CFAllocatorRef allocator, CFOptionFlags flags, CFIndex capacity, const UInt8 *bytes, CFIndex length, CFAllocatorRef bytesDeallocator) {
    CFMutableDataRef memory;
    CFIndex size;
    __CFGenericValidateMutabilityFlags(flags);
    CFAssert2(0 <= capacity, __kCFLogAssertion, "%s(): capacity (%d) cannot be less than zero", __PRETTY_FUNCTION__, capacity);
    CFAssert3(kCFFixedMutable != __CFMutableVarietyFromFlags(flags) || length <= capacity, __kCFLogAssertion, "%s(): for kCFFixedMutable type, capacity (%d) must be greater than or equal to number of initial elements (%d)", __PRETTY_FUNCTION__, capacity, length);
    CFAssert2(0 <= length, __kCFLogAssertion, "%s(): length (%d) cannot be less than zero", __PRETTY_FUNCTION__, length);
    CFAssert1(__CFMutableVarietyFromFlags(flags) == kCFImmutable || (bytesDeallocator == kCFAllocatorNull), __kCFLogAssertion, "%s(): data requested is mutable, but bytesDeallocator is provided", __PRETTY_FUNCTION__);
    size = sizeof(struct __CFData);
    __CFGenericValidateMutabilityFlags(flags);
    if (__CFMutableVarietyFromFlags(flags) != kCFMutable && (bytesDeallocator == kCFAllocatorNull)) {
	size += sizeof(UInt8) * __CFDataNumBytesForCapacity(capacity);
    }
    allocator = (NULL == allocator) ? CFRetain(CFAllocatorGetDefault()) : CFRetain(allocator);
    memory = CFAllocatorAllocate(allocator, size, 0);
    if (NULL == memory) {
	CFRelease(allocator);
	return NULL;
    }
    __CFGenericInitBase(memory, __CFDataIsa(), __kCFDataTypeID);
    __CFDataSetNumBytesUsed(memory, 0);
    __CFDataSetLength(memory, 0);
    memory->_allocator = allocator;
    switch (__CFMutableVarietyFromFlags(flags)) {
    case kCFMutable:
	__CFDataSetCapacity(memory, __CFDataRoundUpCapacity(1));
	__CFDataSetNumBytes(memory, __CFDataNumBytesForCapacity(__CFDataRoundUpCapacity(1)));
	memory->_bytes = CFAllocatorAllocate(allocator, __CFDataNumBytes(memory) * sizeof(UInt8), 0);
	if (NULL == memory->_bytes) {
	    CFAllocatorDeallocate(allocator, memory);
	    CFRelease(allocator);
	    return NULL;
	}
	memory->_bytesDeallocator = NULL;
	break;
    case kCFFixedMutable:
	/* Don't round up capacity */
	__CFDataSetCapacity(memory, capacity);
	__CFDataSetNumBytes(memory, __CFDataNumBytesForCapacity(capacity));
	memory->_bytes = (UInt8 *)((SInt8 *)memory + sizeof(struct __CFData));
	memory->_bytesDeallocator = NULL;
	break;
    case kCFImmutable:
	/* Don't round up capacity */
	__CFDataSetCapacity(memory, capacity);
	__CFDataSetNumBytes(memory, __CFDataNumBytesForCapacity(capacity));
	if (bytesDeallocator != kCFAllocatorNull) {
	    memory->_bytes = (UInt8 *)bytes;
	    memory->_bytesDeallocator = bytesDeallocator ? CFRetain(bytesDeallocator) : CFRetain(CFAllocatorGetDefault());
	    __CFDataSetNumBytesUsed(memory, length);
	    __CFDataSetLength(memory, length);
	} else {
	    memory->_bytes = (UInt8 *)((SInt8 *)memory + sizeof(struct __CFData));
	    memory->_bytesDeallocator = NULL;
	}
	break;
    }
    if (bytesDeallocator == kCFAllocatorNull) {
	if (__CFMutableVarietyFromFlags(flags) != kCFMutable) {
	    __CFSetMutableVariety(memory, kCFFixedMutable);
	} else {
	    __CFSetMutableVariety(memory, kCFMutable);
	}
	CFDataReplaceBytes(memory, CFRangeMake(0, 0), bytes, length);
    }
    __CFSetMutableVariety(memory, __CFMutableVarietyFromFlags(flags));
    return memory;
}

CFDataRef CFDataCreate(CFAllocatorRef allocator, const UInt8 *bytes, CFIndex length) {
    return __CFDataInit(allocator, kCFImmutable, length, bytes, length, kCFAllocatorNull);
}

CFDataRef CFDataCreateWithBytesNoCopy(CFAllocatorRef allocator, const UInt8 *bytes, CFIndex length, CFAllocatorRef bytesDeallocator) {
    CFAssert1((0 == length || bytes != NULL), __kCFLogAssertion, "%s(): bytes pointer cannot be NULL if length is non-zero", __PRETTY_FUNCTION__);
    return __CFDataInit(allocator, kCFImmutable, length, bytes, length, bytesDeallocator);
}

CFDataRef CFDataCreateCopy(CFAllocatorRef allocator, CFDataRef data) {
    CFIndex length = CFDataGetLength(data);
    return __CFDataInit(allocator, kCFImmutable, length, CFDataGetBytePtr(data), length, kCFAllocatorNull);
}

CFMutableDataRef CFDataCreateMutable(CFAllocatorRef allocator, CFIndex capacity) {
    return __CFDataInit(allocator, (0 == capacity) ? kCFMutable : kCFFixedMutable, capacity, NULL, 0, kCFAllocatorNull);
}

CFMutableDataRef CFDataCreateMutableCopy(CFAllocatorRef allocator, CFIndex capacity, CFDataRef data) {
    return __CFDataInit(allocator, (0 == capacity) ? kCFMutable : kCFFixedMutable, capacity, CFDataGetBytePtr(data), CFDataGetLength(data), kCFAllocatorNull);
}

CFIndex CFDataGetLength(CFDataRef data) {
    CF_OBJC_FUNCDISPATCH0(Data, CFIndex, data, "length");
    return __CFDataLength(data);
}

const UInt8 *CFDataGetBytePtr(CFDataRef data) {
    CF_OBJC_FUNCDISPATCH0(Data, const UInt8 *, data, "bytes");
    return data->_bytes;
}

UInt8 *CFDataGetMutableBytePtr(CFMutableDataRef data) {
    CF_OBJC_FUNCDISPATCH0(Data, UInt8 *, data, "mutableBytes");
    CFAssert1(__CFMutableVariety(data) == kCFMutable || __CFMutableVariety(data) == kCFFixedMutable, __kCFLogAssertion, "%s(): data is immutable", __PRETTY_FUNCTION__);
    return data->_bytes;
}

void CFDataGetBytes(CFDataRef data, CFRange range, UInt8 *buffer) {
    CF_OBJC_FUNCDISPATCH2(Data, void, data, "getBytes:range:", buffer, range);
    CFMoveMemory(buffer, data->_bytes + range.location, range.length);
}

static void __CFDataGrow(CFMutableDataRef data, CFIndex numNewValues) {
    CFIndex oldLength = __CFDataLength(data);
    CFIndex capacity = __CFDataRoundUpCapacity(oldLength + numNewValues);
    __CFDataSetCapacity(data, capacity);
    __CFDataSetNumBytes(data, __CFDataNumBytesForCapacity(capacity));
    data->_bytes = CFAllocatorReallocate(data->_allocator, data->_bytes, __CFDataNumBytes(data) * sizeof(UInt8), 0);
    if (NULL == data->_bytes) __CFAbort();
}

void CFDataSetLength(CFMutableDataRef data, CFIndex length) {
    CFIndex len;
    CF_OBJC_FUNCDISPATCH1(Data, void, data, "setLength:", length);
    CFAssert1(__CFMutableVariety(data) == kCFMutable || __CFMutableVariety(data) == kCFFixedMutable, __kCFLogAssertion, "%s(): data is immutable", __PRETTY_FUNCTION__);
    len = __CFDataLength(data);
    switch (__CFMutableVariety(data)) {
    case kCFMutable:
	if (len < length) {
#warning CF: should only grow when new length exceeds current capacity, not whenever it exceeds the current length
	    __CFDataGrow(data, length - len);
	}
	break;
    case kCFFixedMutable:
	CFAssert1(length <= __CFDataCapacity(data), __kCFLogAssertion, "%s(): fixed-capacity data is full", __PRETTY_FUNCTION__);
	break;
    }
    if (len < length) {
	CFZeroMemory(data->_bytes + len, length - len);
    }
    __CFDataSetLength(data, length);
    __CFDataSetNumBytesUsed(data, length);
}

void CFDataIncreaseLength(CFMutableDataRef data, CFIndex extraLength) {
    CF_OBJC_FUNCDISPATCH1(Data, void, data, "increaseLengthBy:", extraLength);
    CFAssert1(__CFMutableVariety(data) == kCFMutable || __CFMutableVariety(data) == kCFFixedMutable, __kCFLogAssertion, "%s(): data is immutable", __PRETTY_FUNCTION__);
    CFDataSetLength(data, __CFDataLength(data) + extraLength);
}

void CFDataAppendBytes(CFMutableDataRef data, const UInt8 *bytes, CFIndex length) {
    CF_OBJC_FUNCDISPATCH2(Data, void, data, "appendBytes:length:", bytes, length);
    CFAssert1(__CFMutableVariety(data) == kCFMutable || __CFMutableVariety(data) == kCFFixedMutable, __kCFLogAssertion, "%s(): data is immutable", __PRETTY_FUNCTION__);
    CFDataReplaceBytes(data, CFRangeMake(__CFDataLength(data), 0), bytes, length); 
}

void CFDataDeleteBytes(CFMutableDataRef data, CFRange range) {
    CF_OBJC_FUNCDISPATCH3(Data, void, data, "replaceBytesInRange:withBytes:length:", range, NULL, 0);
    CFAssert1(__CFMutableVariety(data) == kCFMutable || __CFMutableVariety(data) == kCFFixedMutable, __kCFLogAssertion, "%s(): data is immutable", __PRETTY_FUNCTION__);
    CFDataReplaceBytes(data, range, NULL, 0); 
}

void CFDataReplaceBytes(CFMutableDataRef data, CFRange range, const UInt8 *newBytes, CFIndex newLength) {
#warning CF: no ObjC dispatch here?
    CFIndex len;
    __CFGenericValidateType(data, __kCFDataTypeID);
    __CFDataValidateRange(data, range, __PRETTY_FUNCTION__);
    CFAssert1(__CFMutableVariety(data) == kCFMutable || __CFMutableVariety(data) == kCFFixedMutable, __kCFLogAssertion, "%s(): data is immutable", __PRETTY_FUNCTION__);
    CFAssert2(0 <= newLength, __kCFLogAssertion, "%s(): newLength (%d) cannot be less than zero", __PRETTY_FUNCTION__, newLength);
    len = __CFDataLength(data);
    switch (__CFMutableVariety(data)) {
    case kCFMutable:
	if (range.length < newLength && __CFDataNumBytes(data) < len - range.length + newLength) {
	    __CFDataGrow(data, newLength - range.length);
	}
	break;
    case kCFFixedMutable:
	CFAssert1(len - range.length + newLength <= __CFDataCapacity(data), __kCFLogAssertion, "%s(): fixed-capacity data is full", __PRETTY_FUNCTION__);
	break;
    }
    if (newLength != range.length && range.location + range.length < len) {
        CFMoveMemory(data->_bytes + range.location + newLength, data->_bytes + range.location + range.length, (len - range.location - range.length) * sizeof(UInt8));
    }
    if (0 < newLength) {
        CFMoveMemory(data->_bytes + range.location, newBytes, newLength * sizeof(UInt8));
    }
    __CFDataSetNumBytesUsed(data, (len - range.length + newLength));
    __CFDataSetLength(data, (len - range.length + newLength));
}

