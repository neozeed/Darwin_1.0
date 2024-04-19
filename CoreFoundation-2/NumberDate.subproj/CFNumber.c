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
/*	CFNumber.c
	Copyright 1999-2000, Apple, Inc. All rights reserved.
	Responsibility: Ali Ozer
*/

#include "CFInternal.h"
#include <CoreFoundation/CFNumber.h>

/* Various assertions
*/
#define __CFAssertIsBoolean(cf) __CFGenericValidateType(cf, __kCFBooleanTypeID)
#define __CFAssertIsNumber(cf) __CFGenericValidateType(cf, __kCFNumberTypeID)
#define __CFAssertIsValidNumberType(type) CFAssert2((type > 0 && type <= kCFNumberMaxType && __CFNumberCanonicalType[type]), __kCFLogAssertion, "%s(): bad CFNumber type %d", __PRETTY_FUNCTION__, type);
#define __CFInvalidNumberStorageType(type) CFAssert2(TRUE, __kCFLogAssertion, "%s(): bad CFNumber storage type %d", __PRETTY_FUNCTION__, type);

/* The IEEE bit patterns... Also have:
0x7f800000		float +Inf
0x7fc00000		float NaN
0xff800000		float -Inf
*/
#define BITSFORDOUBLENAN	((UInt64)0x7ff8000000000000LL)
#define BITSFORDOUBLEPOSINF	((UInt64)0x7ff0000000000000LL)
#define BITSFORDOUBLENEGINF	((UInt64)0xfff0000000000000LL)

struct __CFBoolean {
    CFRuntimeBase base;
};

static struct __CFBoolean __kCFBooleanTrue = {
	{NULL, 0}
};

static struct __CFBoolean __kCFBooleanFalse = {
	{NULL, 0}
};

const CFBooleanRef kCFBooleanTrue = &__kCFBooleanTrue;
const CFBooleanRef kCFBooleanFalse = &__kCFBooleanFalse;

CFTypeID CFBooleanGetTypeID(void) {
    return __kCFBooleanTypeID;
}

Boolean __CFBooleanEqual(CFTypeRef cf1, CFTypeRef cf2) {
    CFBooleanRef boolean1 = (CFBooleanRef)cf1;
    CFBooleanRef boolean2 = (CFBooleanRef)cf2;
    return (boolean1 == boolean2);
}

CFHashCode __CFBooleanHash(CFTypeRef cf) {
    CFBooleanRef boolean = (CFBooleanRef)cf;
    return (boolean == kCFBooleanTrue) ? 1 : 0;
}

CFStringRef __CFBooleanCopyDescription(CFTypeRef cf) {
    CFBooleanRef boolean = (CFBooleanRef)cf;
    return CFRetain((boolean == kCFBooleanTrue) ? CFSTR("TRUE") : CFSTR("FALSE"));
}

CFAllocatorRef __CFBooleanGetAllocator(CFTypeRef cf) {
    return kCFAllocatorSystemDefault;
}

void __CFBooleanDeallocate(CFTypeRef cf) {
}

Boolean CFBooleanGetValue(CFBooleanRef boolean) {
    CF_OBJC_FUNCDISPATCH0(Boolean, Boolean, boolean, "boolValue");
    return (boolean == kCFBooleanTrue) ? TRUE : FALSE;
}


/*** CFNumber ***/

typedef union {
    SInt32 valSInt32;
    SInt64 valSInt64;
    Float32 valFloat32;
    Float64 valFloat64;
} __CFNumberValue;

struct __CFNumber {		/* Only as many bytes as necessary are allocated; allocator follows the union */
    CFRuntimeBase base;
    __CFNumberValue value;
};

static struct __CFNumber __kCFNumberNaN = {
        {NULL, 0},
        {0}
};

static struct __CFNumber __kCFNumberNegativeInfinity = {
        {NULL, 0},
    	{0}
};

static struct __CFNumber __kCFNumberPositiveInfinity = {
        {NULL, 0},
    	{0}
};

const CFNumberRef kCFNumberNaN = &__kCFNumberNaN;
const CFNumberRef kCFNumberNegativeInfinity = &__kCFNumberNegativeInfinity;
const CFNumberRef kCFNumberPositiveInfinity = &__kCFNumberPositiveInfinity;


/* Eight bits in base:
    Bit 7: use system default allocator (if 1; otherwise allocator is stored explicitly)
    Bits 6..0: type (bits 4..0 is CFNumberType)
*/
enum {
    __kCFNumberUseSystemDefaultAllocator = 0x080,
    __kCFNumberIsPositiveInfinity = (0x20 | kCFNumberFloat64Type),
    __kCFNumberIsNegativeInfinity = (0x40 | kCFNumberFloat64Type),
    __kCFNumberIsNaN = (0x60 | kCFNumberFloat64Type)
};


/* ??? These tables should be changed on different architectures, depending on the actual sizes of basic C types such as int, long, float; also size of CFIndex. We can probably compute these tables at runtime.
*/

/* Canonical types are the types that the implementation knows how to deal with. There should be one for each type that is distinct; so this table basically is a type equivalence table. All functions that take a type from the outside world should call __CFNumberGetCanonicalTypeForType() before doing anything with it.
*/
static const unsigned char __CFNumberCanonicalType[kCFNumberMaxType + 1] = {
    0, kCFNumberSInt8Type, kCFNumberSInt16Type, kCFNumberSInt32Type, kCFNumberSInt64Type, kCFNumberFloat32Type, kCFNumberFloat64Type,
    kCFNumberSInt8Type, kCFNumberSInt16Type, kCFNumberSInt32Type, kCFNumberSInt32Type, kCFNumberSInt64Type, kCFNumberFloat32Type, kCFNumberFloat64Type,
    kCFNumberSInt32Type
};

/* This table determines what storage format is used for any given type.
   !!! These are the only types that can occur in the types field of a CFNumber.
   !!! If the number or kind of types returned by this array changes, also need to fix NSNumber and NSCFNumber.
*/
static const unsigned char __CFNumberStorageType[kCFNumberMaxType + 1] = {
    0, kCFNumberSInt32Type, kCFNumberSInt32Type, kCFNumberSInt32Type, kCFNumberSInt64Type, kCFNumberFloat32Type, kCFNumberFloat64Type,
    kCFNumberSInt32Type, kCFNumberSInt32Type, kCFNumberSInt32Type, kCFNumberSInt32Type, kCFNumberSInt64Type, kCFNumberFloat32Type, kCFNumberFloat64Type,
    kCFNumberSInt32Type
};

// Returns the type that is used to store the specified type
CF_INLINE CFNumberType __CFNumberGetStorageTypeForType(CFNumberType type) {
    return __CFNumberStorageType[type];
}

// Returns the canonical type used to represent the specified type
CF_INLINE CFNumberType __CFNumberGetCanonicalTypeForType(CFNumberType type) {
    return __CFNumberCanonicalType[type];
}

// Extracts and returns the type out of the CFNumber
CF_INLINE CFNumberType __CFNumberGetType(CFNumberRef num) {
    return __CFBitfieldValue(num->base._info, 4, 0);
}

// Returns TRUE if the argument type is float or double
CF_INLINE Boolean __CFNumberTypeIsFloat(CFNumberType type) {
    return (type == kCFNumberFloat64Type) || (type == kCFNumberFloat32Type);
}

// Returns the number of bytes necessary to store the specified type
// Needs to handle all canonical types
CF_INLINE CFIndex __CFNumberSizeOfType(CFNumberType type) {
    switch (type) {
        case kCFNumberSInt8Type:	return sizeof(SInt8);
        case kCFNumberSInt16Type:	return sizeof(SInt16);
        case kCFNumberSInt32Type:	return sizeof(SInt32);
        case kCFNumberSInt64Type:	return sizeof(SInt64);
        case kCFNumberFloat32Type:	return sizeof(Float32);
        case kCFNumberFloat64Type:	return sizeof(Float64);
        default:			return 0;
    }
}

// Copies an external value of a given type into the appropriate slot in the union (does no type conversion)
// Needs to handle all canonical types
#define SET_VALUE(valueUnion, type, valuePtr)	\
    switch (type) {				\
        case kCFNumberSInt8Type:	(valueUnion)->valSInt32 = *(SInt8 *)(valuePtr); break;	\
        case kCFNumberSInt16Type:	(valueUnion)->valSInt32 = *(SInt16 *)(valuePtr); break;	\
        case kCFNumberSInt32Type:	(valueUnion)->valSInt32 = *(SInt32 *)(valuePtr); break;	\
        case kCFNumberSInt64Type:	(valueUnion)->valSInt64 = *(SInt64 *)(valuePtr); break;	\
        case kCFNumberFloat32Type:	(valueUnion)->valFloat32 = *(Float32 *)(valuePtr); break;	\
        case kCFNumberFloat64Type:	(valueUnion)->valFloat64 = *(Float64 *)(valuePtr); break;	\
        default: break;	\
    }

// Casts the specified value into the specified type and copies it into the provided memory
// Needs to handle all canonical types
#define GET_VALUE(value, type, resultPtr)	\
    switch (type) {				\
        case kCFNumberSInt8Type:	*(SInt8 *)(valuePtr) = (SInt8)value; break;	\
        case kCFNumberSInt16Type:	*(SInt16 *)(valuePtr) = (SInt16)value; break;	\
        case kCFNumberSInt32Type:	*(SInt32 *)(valuePtr) = (SInt32)value; break;	\
        case kCFNumberSInt64Type:	*(SInt64 *)(valuePtr) = (SInt64)value; break;	\
        case kCFNumberFloat32Type:	*(Float32 *)(valuePtr) = (Float32)value; break;	\
        case kCFNumberFloat64Type:	*(Float64 *)(valuePtr) = (Float64)value; break;	\
	default: break;	\
    }

// Extracts the stored type out of the union and copies it in the desired type into the provided memory
// Needs to handle all storage types
CF_INLINE void __CFNumberGetValue(const __CFNumberValue *value, CFNumberType numberType, CFNumberType typeToGet, void *valuePtr) {
    switch (numberType) {
        case kCFNumberSInt32Type:	GET_VALUE(value->valSInt32, typeToGet, resultPtr); break;	
        case kCFNumberSInt64Type:	GET_VALUE(value->valSInt64, typeToGet, resultPtr); break;	
        case kCFNumberFloat32Type:	GET_VALUE(value->valFloat32, typeToGet, resultPtr); break;
        case kCFNumberFloat64Type:	GET_VALUE(value->valFloat64, typeToGet, resultPtr); break;
	default: break;	\
    }
}

// Sees if two value union structs have the same value (will do type conversion)
CF_INLINE Boolean __CFNumberEqualValue(const __CFNumberValue *value1, CFNumberType type1, const __CFNumberValue *value2, CFNumberType type2) {
    if (__CFNumberTypeIsFloat(type1) || __CFNumberTypeIsFloat(type2)) {
        Float64 d1, d2;
        __CFNumberGetValue(value1, type1, kCFNumberFloat64Type, &d1);
        __CFNumberGetValue(value2, type2, kCFNumberFloat64Type, &d2);
        return d1 == d2;
    } else {
        SInt64 i1, i2;
        __CFNumberGetValue(value1, type1, kCFNumberSInt64Type, &i1);
        __CFNumberGetValue(value2, type2, kCFNumberSInt64Type, &i2);
        return i1 == i2;
    }
}

CF_INLINE Boolean __CFNumberHasCustomAllocator(CFNumberRef number) {
    return (number->base._info & __kCFNumberUseSystemDefaultAllocator) ? FALSE : TRUE;
}

CFAllocatorRef __CFNumberGetAllocator(CFTypeRef cf) {
    CFNumberRef number = (CFNumberRef)cf;
    if (__CFNumberHasCustomAllocator(number)) {
        return *(CFAllocatorRef *)((const UInt8 *)number + sizeof(CFRuntimeBase) + __CFNumberSizeOfType(__CFNumberGetType(number)));
    } else {
        return kCFAllocatorSystemDefault;
    }
}

CFTypeID CFNumberGetTypeID(void) {
    return __kCFNumberTypeID;
}

Boolean __CFNumberEqual(CFTypeRef cf1, CFTypeRef cf2) {
    CFNumberRef number1 = (CFNumberRef)cf1;
    CFNumberRef number2 = (CFNumberRef)cf2;
    return __CFNumberEqualValue(&(number1->value), __CFNumberGetType(number1), &(number2->value), __CFNumberGetType(number2));
}

CFHashCode __CFNumberHash(CFTypeRef cf) {
    CFNumberRef number = (CFNumberRef)cf;
    switch (__CFNumberGetType(cf)) {
        case kCFNumberSInt32Type: return (CFHashCode)number->value.valSInt32;
        case kCFNumberSInt64Type: return (CFHashCode)number->value.valSInt64;
        case kCFNumberFloat32Type: return (CFHashCode)number->value.valFloat32;
        case kCFNumberFloat64Type: return (CFHashCode)number->value.valFloat64;
        default: 
	    __CFInvalidNumberStorageType(__CFNumberGetType(cf));
	    return 0;
    }
}

CFStringRef __CFNumberCopyDescription(CFTypeRef cf) {
    CFNumberRef number = (CFNumberRef)cf;
    switch (__CFNumberGetType(number)) {
        case kCFNumberSInt32Type: return CFStringCreateWithFormat(NULL, NULL, CFSTR("%d"), number->value.valSInt32); break;
        case kCFNumberSInt64Type: return CFStringCreateWithFormat(NULL, NULL, CFSTR("%qd"), number->value.valSInt64); break;
        case kCFNumberFloat32Type: return CFStringCreateWithFormat(NULL, NULL, CFSTR("%f"), number->value.valFloat32); break;
        case kCFNumberFloat64Type: return CFStringCreateWithFormat(NULL, NULL, CFSTR("%lf"), number->value.valFloat64); break;
	default:
            __CFInvalidNumberStorageType(__CFNumberGetType(number));
            return NULL;
    }
}

void __CFNumberDeallocate(CFTypeRef cf) {
    CFNumberRef number = (CFNumberRef)cf;
    Boolean hasCustomAllocator = __CFNumberHasCustomAllocator(number);
    CFAllocatorRef allocator = hasCustomAllocator ? __CFNumberGetAllocator(number) : kCFAllocatorSystemDefault;
    CFAllocatorDeallocate(allocator, (void *)number);
    if (hasCustomAllocator) CFRelease(allocator);
}

CFNumberRef CFNumberCreate(CFAllocatorRef allocator, CFNumberType type, const void *valuePtr) {
    CFNumberRef num;
    Boolean useSystemAllocator;
    CFNumberType equivType, storageType;

    if (allocator == NULL) allocator = CFAllocatorGetDefault();
    useSystemAllocator = (allocator == kCFAllocatorSystemDefault);

    equivType = __CFNumberGetCanonicalTypeForType(type);
    storageType = __CFNumberGetStorageTypeForType(type);

    num = CFAllocatorAllocate(allocator, sizeof(CFRuntimeBase) + __CFNumberSizeOfType(storageType) + (useSystemAllocator ? 0 : sizeof(CFAllocatorRef)), 0);
    if (NULL == num) {
	return NULL;
    }
    __CFGenericInitBase((void *)num, __CFNumberIsa(), __kCFNumberTypeID);
    __CFSetLastAllocationEventName(num, "CFNumber");
    SET_VALUE((__CFNumberValue *)&(num->value), equivType, valuePtr);
    if (!useSystemAllocator) *(CFAllocatorRef *)((UInt8 *)num + sizeof(CFRuntimeBase) + __CFNumberSizeOfType(storageType)) = CFRetain(allocator);
    __CFBitfieldSetValue(((struct __CFNumber *)num)->base._info, 7, 0, (useSystemAllocator ? __kCFNumberUseSystemDefaultAllocator : 0) | storageType);
    return num;
}

CFNumberType CFNumberGetType(CFNumberRef number) {
    CF_OBJC_FUNCDISPATCH0(Number, CFNumberType, number, "_cfNumberType");

    __CFAssertIsNumber(number);
    return __CFNumberGetType(number);
}

CFIndex CFNumberGetByteSize(CFNumberRef number) {
    __CFAssertIsNumber(number);
    return __CFNumberSizeOfType(CFNumberGetType(number));
}

Boolean CFNumberIsFloatType(CFNumberRef number) {
    __CFAssertIsNumber(number);
    return __CFNumberTypeIsFloat(CFNumberGetType(number));
}

Boolean	CFNumberGetValue(CFNumberRef number, CFNumberType type, void *valuePtr) {
    UInt8 localMemory[sizeof(__CFNumberValue)];
    __CFNumberValue localValue;
    CFNumberType numType;
    CFNumberType storageTypeForType;

    CF_OBJC_FUNCDISPATCH2(Number, Boolean, number, "_getValue:forType:", valuePtr, __CFNumberGetCanonicalTypeForType(type));
    
    __CFAssertIsNumber(number);
    __CFAssertIsValidNumberType(type);

    storageTypeForType = __CFNumberGetStorageTypeForType(type);
    type = __CFNumberGetCanonicalTypeForType(type);
    if (!valuePtr) valuePtr = &localMemory;

    numType = __CFNumberGetType(number);
    __CFNumberGetValue((__CFNumberValue *)&(number->value), numType, type, valuePtr);

    // If the types match, then we're fine!
    if (numType == storageTypeForType) return TRUE;

    // Test to see if the returned value is intact...
    SET_VALUE(&localValue, type, valuePtr);
    return __CFNumberEqualValue(&localValue, storageTypeForType, &(number->value), numType);
}

CFComparisonResult CFNumberCompare(CFNumberRef number1, CFNumberRef number2, void *context) {
    CFNumberType type1, type2;

    CF_OBJC_FUNCDISPATCH1(Number, CFComparisonResult, number1, "compare:", number2);
    CF_OBJC_FUNCDISPATCH1(Number, CFComparisonResult, number2, "_reverseCompare:", number1);

    __CFAssertIsNumber(number1);
    __CFAssertIsNumber(number2);

    type1 = __CFNumberGetType(number1);
    type2 = __CFNumberGetType(number2);

    if (__CFNumberTypeIsFloat(type1) || __CFNumberTypeIsFloat(type2)) {
        Float64 d1, d2;
        __CFNumberGetValue(&(number1->value), type1, kCFNumberFloat64Type, &d1);
        __CFNumberGetValue(&(number2->value), type2, kCFNumberFloat64Type, &d2);
        return (d1 > d2) ? kCFCompareGreaterThan : ((d1 < d2) ? kCFCompareLessThan : kCFCompareEqualTo);
    } else {
        SInt64 i1, i2;
        __CFNumberGetValue(&(number1->value), type1, kCFNumberSInt64Type, &i1);
        __CFNumberGetValue(&(number2->value), type2, kCFNumberSInt64Type, &i2);
        return (i1 > i2) ? kCFCompareGreaterThan : ((i1 < i2) ? kCFCompareLessThan : kCFCompareEqualTo);
    }

}


void __CFInitializeNumbersAndBooleans(void) {
    // Numbers
    UInt64 nan = BITSFORDOUBLENAN;
    UInt64 posInf = BITSFORDOUBLEPOSINF;
    UInt64 negInf = BITSFORDOUBLENEGINF;

    __CFGenericInitBase(&__kCFNumberNaN, __CFNumberIsa(), __kCFNumberTypeID);
    __CFBitfieldSetValue(__kCFNumberNaN.base._info, 7, 0, __kCFNumberUseSystemDefaultAllocator | __kCFNumberIsNaN);
    __kCFNumberNaN.value.valFloat64 = *((double *)&nan);
    __CFGenericInitBase(&__kCFNumberNegativeInfinity, __CFNumberIsa(), __kCFNumberTypeID);
    __CFBitfieldSetValue(__kCFNumberNegativeInfinity.base._info, 7, 0, __kCFNumberUseSystemDefaultAllocator | __kCFNumberIsNegativeInfinity);
    __kCFNumberNegativeInfinity.value.valFloat64 = *((double *)&negInf);
    __CFGenericInitBase(&__kCFNumberPositiveInfinity, __CFNumberIsa(), __kCFNumberTypeID);
    __CFBitfieldSetValue(__kCFNumberPositiveInfinity.base._info, 7, 0, __kCFNumberUseSystemDefaultAllocator | __kCFNumberIsPositiveInfinity);
    __kCFNumberPositiveInfinity.value.valFloat64 = *((double *)&posInf);

    // Booleans
    __CFGenericInitBase(&__kCFBooleanTrue, __CFBooleanIsa(), __kCFBooleanTypeID);
    __CFGenericInitBase(&__kCFBooleanFalse, __CFBooleanIsa(), __kCFBooleanTypeID);
}


