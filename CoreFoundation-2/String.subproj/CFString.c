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
/*	CFString.c
	Copyright 1998-2000, Apple, Inc. All rights reserved.
	Responsibility: Ali Ozer
*/

#include <CoreFoundation/CFBase.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFDictionary.h>
#include "CFStringEncodingConverterExt.h"
#include "CFUniChar.h"
#include "CFVeryPrivate.h"
#include "CFInternal.h"
#include "CFStringBuffer.h"
#include <stdarg.h>
#include <stdio.h>
#if defined (__MACOS8__)
    #include <Script.h> // For GetScriptManagerVariable
    #include <Processes.h> // For logging
    #include <stdlib.h>
#include <UnicodeConverter.h>
#include <TextEncodingConverter.h>
#elif defined(__MACH__)
#include <sys/types.h>
#include <sys/param.h>
#include <fcntl.h>
#include <pwd.h>
#include <stdlib.h>
#include <unistd.h>
#endif
/* strncmp, etc */
#include <string.h>
#if defined(__WIN32__)
#include <windows.h>
#endif __WIN32__


CF_DEFINECONSTANTSTRING(kCFEmptyString, "")
CF_DEFINECONSTANTSTRING(kCFNSDecimalSeparatorKey, "NSDecimalSeparator")

#if defined(DEBUG)

// Special allocator used by CFSTRs to catch deallocations
static CFAllocatorRef constantStringAllocatorForDebugging = NULL;

// We put this into C & Pascal strings if we can't convert
#define CONVERSIONFAILURESTR "CFString conversion failed"

#endif


/* !!! Never do sizeof(CFString); the union is here just to make it easier to access some fields.
*/
struct __CFString {
    CFRuntimeBase base;
    union {	// In many cases the allocated structs are smaller than these
	struct {
	    SInt32 length;
	    CFAllocatorRef allocator;
        } inline1;
	struct {
	    CFAllocatorRef allocator;
        } inline2;

	struct {
	    void *buffer;
	    UInt32 length;
	    CFAllocatorRef contentsDeallocator;		// Just the dealloc func is used
	    CFAllocatorRef allocator;
	} externalImmutable1;
	struct {
	    void *buffer;
	    CFAllocatorRef contentsDeallocator;		// Just the dealloc func is used
	    CFAllocatorRef allocator;
	} externalImmutable2;
	struct {
	    void *buffer;
	    CFAllocatorRef allocator;
	} externalImmutable3;
	struct {
	    void *buffer;
	    UInt32 length;
	    UInt32 capacityFields;	// Currently only stores capacity
	    UInt32 gapEtc;		// Stores some bits, plus desired or fixed capacity
	    CFAllocatorRef allocator;
	    CFAllocatorRef contentsAllocator;	// Optional
	} externalMutable;
    } variants;
};

/* 
C = use custom allocator
I = is immutable
E = not inline contents
U = is Unicode
N = has NULL byte
L = has length byte
D = explicit deallocator for contents (for mutable objects, allocator)
X = is external mutable

Also need (only for mutable)
F = is fixed
G = has gap
Cap, DesCap = capacity

B7 B6 B5 B4 B3 B2 B1 B0
      C  U  N  L  X  I

B7 B6
 0  0   inline contents
 0  1   E (freed with default allocator)
 1  0   E (not freed)
 1  1   E D
*/

enum {
    __kCFHasAllocatorMask = 0x20,
	__kCFHasAllocator = 0x20,
    __kCFFreeContentsWhenDoneMask = 0x040,
        __kCFFreeContentsWhenDone = 0x040,
    __kCFContentsMask = 0x0C0,
	__kCFHasInlineData = 0x000,
	__kCFHasExternalDataNoFree = 0x080,		// Don't free
	__kCFHasExternalDataDefaultFree = 0x040,	// Use allocator's free function
	__kCFHasExternalDataCustomFree = 0x0C0,		// Use a specially provided free function
    __kCFHasContentsAllocatorMask = 0x0C0,
        __kCFHasContentsAllocator = 0x0C0,		// (For mutable strings) use a specially provided allocator
    __kCFHasContentsDeallocatorMask = 0x0C0,
        __kCFHasContentsDeallocator = 0x0C0,
    __kCFIsMutableMask = 0x01,
	__kCFIsMutable = 0x01,
    __kCFIsUnicodeMask = 0x10,
	__kCFIsUnicode = 0x10,
    __kCFHasNullByteMask = 0x08,
	__kCFHasNullByte = 0x08,
    __kCFHasLengthByteMask = 0x04,
	__kCFHasLengthByte = 0x04,
    __kCFIsExternalMutableMask = 0x02,		// For now we use this bit; can switch to something else
        __kCFIsExternalMutable = 0x02,
    // These are in variants.externalMutable.gapEtc
    __kCFGapMask = 0x00ffffff,
    __kCFGapBitNumber = 24,
    __kCFDesiredCapacityMask = 0x00ffffff,	// Currently gap and fixed share same bits as gap not implemented
    __kCFDesiredCapacityBitNumber = 24,
    __kCFIsFixedMask = 0x80000000,
        __kCFIsFixed = 0x80000000,
    __kCFHasGapMask = 0x40000000,
	__kCFHasGap = 0x40000000,
    __kCFCapacityProvidedExternallyMask = 0x20000000,	// Set if the external buffer is set explicitly by the developer
        __kCFCapacityProvidedExternally = 0x20000000
};


// !!! Assumptions:
// Mutable strings are not inline
// Compile-time constant strings are not inline
// Mutable strings always have explicit length (but they might also have length byte and null byte)
// If there is an explicit length, always use that instead of the length byte (length byte is useful for quickly returning pascal strings)
// Never look at the length byte for the length; use __CFStrLength or __CFStrLength2

/* The following set of functions and macros need to be updated on change to the bit configuration
*/
CF_INLINE Boolean __CFStrIsMutable(CFStringRef str)		{return (str->base._info & __kCFIsMutableMask) == __kCFIsMutable;}
CF_INLINE Boolean __CFStrIsExternalMutable(CFStringRef str)	{return (str->base._info & __kCFIsExternalMutableMask) == __kCFIsExternalMutable;}
CF_INLINE Boolean __CFStrIsInline(CFStringRef str)		{return (str->base._info & __kCFContentsMask) == __kCFHasInlineData;}
CF_INLINE Boolean __CFStrFreeContentsWhenDone(CFStringRef str)	{return (str->base._info & __kCFFreeContentsWhenDoneMask) == __kCFFreeContentsWhenDone;}
CF_INLINE Boolean __CFStrHasContentsDeallocator(CFStringRef str)	{return (str->base._info & __kCFHasContentsDeallocatorMask) == __kCFHasContentsDeallocator;}
CF_INLINE Boolean __CFStrIsUnicode(CFStringRef str)		{return (str->base._info & __kCFIsUnicodeMask) == __kCFIsUnicode;}
CF_INLINE Boolean __CFStrIsEightBit(CFStringRef str)		{return (str->base._info & __kCFIsUnicodeMask) != __kCFIsUnicode;}
CF_INLINE Boolean __CFStrHasNullByte(CFStringRef str)		{return (str->base._info & __kCFHasNullByteMask) == __kCFHasNullByte;}
CF_INLINE Boolean __CFStrHasLengthByte(CFStringRef str)		{return (str->base._info & __kCFHasLengthByteMask) == __kCFHasLengthByte;}
CF_INLINE Boolean __CFStrHasCustomAllocator(CFStringRef str)	{return (str->base._info & __kCFHasAllocatorMask) == __kCFHasAllocator;}
CF_INLINE Boolean __CFStrHasExplicitLength(CFStringRef str)	{return (str->base._info & (__kCFIsMutableMask | __kCFHasLengthByteMask)) != __kCFHasLengthByte;}	// Has explicit length if (1) mutable or (2) not mutable and no length byte

CF_INLINE SInt32 __CFStrSkipAnyLengthByte(CFStringRef str)	{return ((str->base._info & __kCFHasLengthByteMask) == __kCFHasLengthByte) ? 1 : 0;}	// Number of bytes to skip over the length byte in the contents

/* Returns ptr to the buffer (which might include the length byte)
*/
CF_INLINE const void *__CFStrContents(CFStringRef str) {
    if (__CFStrIsInline(str)) {
	return (const void *)(((UInt32)&(str->variants)) + (__CFStrHasExplicitLength(str) ? sizeof(UInt32) : 0) + (__CFStrHasCustomAllocator(str) ? sizeof(CFAllocatorRef) : 0));
    } else {	// External; pointer is always word 2
	return str->variants.externalImmutable1.buffer;
    }
}

static CFAllocatorRef *__CFStrContentsDeallocatorPtr(CFStringRef str) {
    return __CFStrHasExplicitLength(str) ? &(((CFMutableStringRef)str)->variants.externalImmutable1.contentsDeallocator) : &(((CFMutableStringRef)str)->variants.externalImmutable2.contentsDeallocator); }

// Assumption: Called with immutable strings only, and on strings that are known to have a contentsDeallocator
CF_INLINE CFAllocatorRef __CFStrContentsDeallocator(CFStringRef str) {
    return *__CFStrContentsDeallocatorPtr(str); 
}

// Assumption: Called with immutable strings only, and on strings that are known to have a contentsDeallocator
CF_INLINE void __CFStrSetContentsDeallocator(CFStringRef str, CFAllocatorRef contentsAllocator) {
    *__CFStrContentsDeallocatorPtr(str) = contentsAllocator;
}

static CFAllocatorRef *__CFStrCustomAllocatorPtr(CFStringRef str) {
    if (__CFStrIsInline(str)) {
	return (CFAllocatorRef *)((UInt8*)__CFStrContents(str) - sizeof(CFAllocatorRef));
    } else if (__CFStrIsMutable(str)) {
	return (CFAllocatorRef *)&(str->variants.externalMutable.allocator);
    } else {
	return (CFAllocatorRef *)(((UInt32)&(str->variants.externalImmutable1.length)) + (__CFStrHasExplicitLength(str) ? sizeof(SInt32) : 0) + (__CFStrHasContentsDeallocator(str) ? sizeof(void *) : 0));
    }
}

// Assumption: Called with strings that have a custom allocator
CF_INLINE CFAllocatorRef __CFStrCustomAllocator(CFStringRef str) {
    return *__CFStrCustomAllocatorPtr(str);
}

// Assumption: Called with strings that have a custom allocator
CF_INLINE void __CFStrSetCustomAllocator(CFStringRef str, CFAllocatorRef alloc) {
    *__CFStrCustomAllocatorPtr(str) = alloc;
}

// Assumption: Called with strings that have a contents allocator; also, contents allocator follows custom
CF_INLINE CFAllocatorRef __CFStrContentsAllocator(CFMutableStringRef str) {
    return *(__CFStrCustomAllocatorPtr(str) + 1);
}

// Assumption: Called with strings that have a contents allocator; also, contents allocator follows custom
CF_INLINE void __CFStrSetContentsAllocator(CFMutableStringRef str, CFAllocatorRef alloc) {
    *(__CFStrCustomAllocatorPtr(str) + 1) = alloc;
}

/* Returns length; use __CFStrLength2 if contents buffer pointer has already been computed.
*/
CF_INLINE CFIndex __CFStrLength(CFStringRef str) {
    if (__CFStrHasExplicitLength(str)) {
	if (__CFStrIsInline(str)) {
            return str->variants.inline1.length;
	} else {
            CFIndex len = str->variants.externalImmutable1.length;
            if (len == 0x0ffffff) ((CFMutableStringRef)str)->variants.externalImmutable1.length = (len = strlen(__CFStrContents(str)));	/* For compile-time constant strings */
            return len;
 	}
    } else {
	return (CFIndex)(*((UInt8 *)__CFStrContents(str)));
    }
}

CF_INLINE CFIndex __CFStrLength2(CFStringRef str, const void *buffer) {
    if (__CFStrHasExplicitLength(str)) {
        if (__CFStrIsInline(str)) {
            return str->variants.inline1.length;
        } else {
            CFIndex len = str->variants.externalImmutable1.length;
            if (len == 0x0ffffff) ((CFMutableStringRef)str)->variants.externalImmutable1.length = (len = strlen(buffer));	/* For compile-time constant strings */
            return len;
        }
    } else {
        return (CFIndex)(*((UInt8 *)buffer));
    }
}

/* Sets the external content pointer for immutable or mutable strings.
*/
CF_INLINE void __CFStrSetContentPtr(CFStringRef str, const void *p)	{((CFMutableStringRef)str)->variants.externalImmutable1.buffer = (void *)p;}
CF_INLINE void __CFStrSetInfoBits(CFStringRef str, UInt32 v)		{__CFBitfieldSetValue(((CFMutableStringRef)str)->base._info, 7, 0, v);}

CF_INLINE void __CFStrSetExplicitLength(CFStringRef str, CFIndex v) {
    if (__CFStrIsInline(str)) {
	((CFMutableStringRef)str)->variants.inline1.length = v;
    } else {
	((CFMutableStringRef)str)->variants.externalImmutable1.length = v;
    }
}

// Assumption: Called with mutable strings only
CF_INLINE Boolean __CFStrIsFixed(CFStringRef str)   			{return (str->variants.externalMutable.gapEtc & __kCFIsFixedMask) == __kCFIsFixed;}
CF_INLINE Boolean __CFStrHasContentsAllocator(CFStringRef str)		{return (str->base._info & __kCFHasContentsAllocatorMask) == __kCFHasContentsAllocator;}

CF_INLINE Boolean __CFStrCapacityProvidedExternally(CFStringRef str)   		{return (str->variants.externalMutable.gapEtc & __kCFCapacityProvidedExternallyMask) == __kCFCapacityProvidedExternally;}
CF_INLINE void __CFStrSetCapacityProvidedExternally(CFMutableStringRef str)	{str->variants.externalMutable.gapEtc |= __kCFCapacityProvidedExternally;}
CF_INLINE void __CFStrClearCapacityProvidedExternally(CFMutableStringRef str)	{str->variants.externalMutable.gapEtc &= ~__kCFCapacityProvidedExternally;}


CF_INLINE void __CFStrSetIsFixed(CFMutableStringRef str)		{str->variants.externalMutable.gapEtc |= __kCFIsFixed;}
CF_INLINE void __CFStrSetHasGap(CFMutableStringRef str)			{str->variants.externalMutable.gapEtc |= __kCFHasGap;}
CF_INLINE void __CFStrSetUnicode(CFMutableStringRef str)		{str->base._info |= __kCFIsUnicode;}
CF_INLINE void __CFStrClearUnicode(CFMutableStringRef str)		{str->base._info &= ~__kCFIsUnicode;}
CF_INLINE void __CFStrSetHasLengthAndNullBytes(CFMutableStringRef str)	{str->base._info |= (__kCFHasLengthByte | __kCFHasNullByte);}
CF_INLINE void __CFStrClearHasLengthAndNullBytes(CFMutableStringRef str)	{str->base._info &= ~(__kCFHasLengthByte | __kCFHasNullByte);}


static void *__CFStrAllocateMutableContents(CFMutableStringRef str, CFIndex size) {
    void *ptr;
    CFAllocatorRef alloc = (__CFStrHasContentsAllocator(str)) ? __CFStrContentsAllocator(str) : (__CFStrHasCustomAllocator(str) ? __CFStrCustomAllocator(str) : kCFAllocatorSystemDefault);
    ptr = CFAllocatorAllocate(alloc, size, 0);
    __CFSetLastAllocationEventName(ptr, "CFString (store)");
    return ptr;
}

static void __CFStrDeallocateMutableContents(CFMutableStringRef str, void *buffer) {
    CFAllocatorRef alloc = (__CFStrHasContentsAllocator(str)) ? __CFStrContentsAllocator(str) : (__CFStrHasCustomAllocator(str) ? __CFStrCustomAllocator(str) : kCFAllocatorSystemDefault);
    CFAllocatorDeallocate(alloc, buffer);
}


// The following set of functions should only be called on mutable strings

/* "Capacity" is stored in number of bytes, not characters. It indicates the total number of bytes in the contents buffer.
   "Desired capacity" is in number of characters; it is the client requested capacity; if fixed, it is the upper bound on the mutable string backing store.
*/
CF_INLINE CFIndex __CFStrCapacity(CFStringRef str)			{return str->variants.externalMutable.capacityFields;}
CF_INLINE void __CFStrSetCapacity(CFMutableStringRef str, CFIndex cap)	{str->variants.externalMutable.capacityFields = cap;}
CF_INLINE CFIndex __CFStrDesiredCapacity(CFStringRef str)		{return __CFBitfieldValue(str->variants.externalMutable.gapEtc, __kCFDesiredCapacityBitNumber, 0);}
CF_INLINE void __CFStrSetDesiredCapacity(CFMutableStringRef str, CFIndex size)	{__CFBitfieldSetValue(str->variants.externalMutable.gapEtc, __kCFDesiredCapacityBitNumber, 0, size);}




/* CFString specific init flags
   Note that you cannot count on the external buffer not being copied.
   Also, if you specify an external buffer, you should not change it behind the CFString's back.
*/
enum {
    __kCFThinUnicodeIfPossible = 0x1000000,		/* See if the Unicode contents can be thinned down to 8-bit */
    kCFStringPascal = 0x10000,				/* Indicating that the string data has a Pascal string structure (length byte at start) */
    kCFStringNoCopyProvidedContents = 0x20000,		/* Don't copy the provided string contents if possible; free it when no longer needed */
    kCFStringNoCopyNoFreeProvidedContents = 0x30000	/* Don't copy the provided string contents if possible; don't free it when no longer needed */
};

/* Size for temporary buffers
*/
#define MAXTMPBUFFERLEN (2048)


/* System Encoding.
*/
static CFStringEncoding __CFDefaultSystemEncoding = kCFStringEncodingInvalidId;
static CFStringEncoding __CFDefaultFileSystemEncoding = kCFStringEncodingInvalidId;
CFStringEncoding __CFDefaultEightBitStringEncoding = kCFStringEncodingInvalidId;

CFStringEncoding CFStringGetSystemEncoding(void) {

    if (__CFDefaultSystemEncoding == kCFStringEncodingInvalidId) {
        const CFStringEncodingConverter *converter = NULL;
#if defined(__MACOS8__) || defined(__MACH__)
            __CFDefaultSystemEncoding = kCFStringEncodingMacRoman; // MacRoman is built-in so always available
#elif defined(__WIN32__) // Windows
            __CFDefaultSystemEncoding = kCFStringEncodingWindowsLatin1; // WinLatin1 is built-in so always available
#else // Solaris && HP-UX ?
            __CFDefaultSystemEncoding = kCFStringEncodingISOLatin1; // Latin1 is built-in so always available
#endif
            converter = CFStringEncodingGetConverter(__CFDefaultSystemEncoding);

        if (converter->encodingClass == kCFStringEncodingConverterCheapEightBit) {
            __CFCharToUniCharFunc = converter->toUnicode;
        }
    }

    return __CFDefaultSystemEncoding;
}

// Fast version for internal use

CF_INLINE CFStringEncoding __CFStringGetSystemEncoding(void) {
    if (__CFDefaultSystemEncoding == kCFStringEncodingInvalidId) (void)CFStringGetSystemEncoding();
    return __CFDefaultSystemEncoding;
}

CFStringEncoding CFStringFileSystemEncoding(void) {
    if (__CFDefaultFileSystemEncoding == kCFStringEncodingInvalidId) {
#if !defined(__MACH__)
        __CFDefaultFileSystemEncoding = CFStringGetSystemEncoding();
#else
        __CFDefaultFileSystemEncoding = kCFStringEncodingUTF8;
#endif
    }

    return __CFDefaultFileSystemEncoding;
}

/* ??? Is returning length when no other answer is available the right thing?
*/
CFIndex CFStringGetMaximumSizeForEncoding(CFIndex length, CFStringEncoding encoding) {
    switch (encoding) {
        case kCFStringEncodingUnicode:
            return length * sizeof(UniChar);

        case kCFStringEncodingUTF8:
        case kCFStringEncodingNonLossyASCII:
            return length * 6; // 1 Unichar could expand to 6 bytes

        case kCFStringEncodingMacRoman:
        case kCFStringEncodingWindowsLatin1:
        case kCFStringEncodingISOLatin1:
        case kCFStringEncodingNextStepLatin:
        case kCFStringEncodingASCII:
            return length / sizeof(UInt8);

        default:
            return length / sizeof(UInt8);
    }
}


/* Returns whether the indicated encoding can be stored in 8-bit chars
*/
CF_INLINE Boolean __CFStrEncodingCanBeStoredInEightBit(CFStringEncoding encoding) {
    switch (encoding) {
        case kCFStringEncodingInvalidId:
        case kCFStringEncodingUnicode:
        case kCFStringEncodingUTF8:
        case kCFStringEncodingNonLossyASCII:
            return FALSE;

        case kCFStringEncodingMacRoman:
        case kCFStringEncodingWindowsLatin1:
        case kCFStringEncodingISOLatin1:
        case kCFStringEncodingNextStepLatin:
        case kCFStringEncodingASCII:
            return TRUE;

        default: return FALSE;
    }
}

/* Returns the encoding used in eight bit CFStrings (can't be any encoding which isn't 1-to-1 with Unicode)
   ??? Perhaps only ASCII fits the bill due to Unicode decomposition.
*/
CFStringEncoding __CFStringComputeEightBitStringEncoding(void) {
    if (__CFDefaultEightBitStringEncoding == kCFStringEncodingInvalidId) {
        CFStringEncoding systemEncoding = CFStringGetSystemEncoding();
	if (systemEncoding == kCFStringEncodingInvalidId) { // We're right in the middle of querying system encoding from default database. Delaying to set until system encoding is determined.
	    return kCFStringEncodingASCII;
	} else if (__CFStrEncodingCanBeStoredInEightBit(systemEncoding)) {
            __CFDefaultEightBitStringEncoding = systemEncoding;
        } else {
            __CFDefaultEightBitStringEncoding = kCFStringEncodingASCII;
        }
    }

    return __CFDefaultEightBitStringEncoding;
}

/* Returns whether the provided bytes can be stored in ASCII
*/
CF_INLINE Boolean __CFBytesInASCII(const UInt8 *bytes, CFIndex len) {
    while (len--) if ((UInt8)(*bytes++) >= 128) return FALSE;
    return TRUE;
}

/* Returns whether the provided 8-bit string in the specified encoding can be stored in an 8-bit CFString. 
*/
CF_INLINE Boolean __CFCanUseEightBitCFStringForBytes(const UInt8 *bytes, CFIndex len, CFStringEncoding encoding) {
    if (encoding == __CFStringGetEightBitStringEncoding()) return TRUE;
    if (__CFStringEncodingIsSupersetOfASCII(encoding) && __CFBytesInASCII(bytes, len)) return TRUE;
    return FALSE;
}

/* Returns whether a length byte can be tacked on to a string of the indicated length.
*/
CF_INLINE Boolean __CFCanUseLengthByte(CFIndex len) {
#define __kCFMaxPascalStrLen 255	
    return (len <= __kCFMaxPascalStrLen) ? TRUE : FALSE;
}

/* Basic string allocation; the arg is the whole size, including the control block
   Use provided allocator (if NULL, use the default)
*/
CF_INLINE CFMutableStringRef __CFStrAlloc(CFAllocatorRef alloc, CFIndex size) {
    CFMutableStringRef str = CFAllocatorAllocate(alloc ? alloc : CFAllocatorGetDefault(), size, 0);
    __CFGenericInitBase(str, __CFStringIsa(), __kCFStringTypeID);
    return str;
}

/* Various string assertions
*/
#define __CFAssertIsString(cf) __CFGenericValidateType(cf, __kCFStringTypeID)
#define __CFAssertIndexIsInStringBounds(cf, index) CFAssert3((index) >= 0 && (index) < __CFStrLength(cf), __kCFLogAssertion, "%s(): string index %d out of bounds (length %d)", __PRETTY_FUNCTION__, index, __CFStrLength(cf))
#define __CFAssertRangeIsInStringBounds(cf, index, count) CFAssert4((index) >= 0 && (index + count) <= __CFStrLength(cf), __kCFLogAssertion, "%s(): string range %d,%d out of bounds (length %d)", __PRETTY_FUNCTION__, index, count, __CFStrLength(cf))
#define __CFAssertLengthIsOK(len) CFAssert2(len < __kCFMaxLength, __kCFLogAssertion, "%s(): length %d too large", __PRETTY_FUNCTION__, len)
#define __CFAssertIsStringAndMutable(cf) {__CFGenericValidateType(cf, __kCFStringTypeID); CFAssert1(__CFStrIsMutable(cf), __kCFLogAssertion, "%s(): string not mutable", __PRETTY_FUNCTION__);}
#define __CFAssertIsStringAndExternalMutable(cf) {__CFGenericValidateType(cf, __kCFStringTypeID); CFAssert1(__CFStrIsExternalMutable(cf), __kCFLogAssertion, "%s(): string not external mutable", __PRETTY_FUNCTION__);}

#define __CFAssertIfFixedLengthIsOK(cf, reqLen) CFAssert2(!__CFStrIsFixed(cf) || (reqLen <= __CFStrDesiredCapacity(cf)), __kCFLogAssertion, "%s(): length %d too large", __PRETTY_FUNCTION__, reqLen)


/* Basic algorithm is to shrink memory when capacity is SHRINKFACTOR times the required capacity or to allocate memory when the capacity is less than GROWFACTOR times the required capacity.
Additional complications are applied in the following order:
- desiredCapacity, which is the minimum (except initially things can be at zero)
- rounding up to factor of 8
- compressing (to fit the number if 16 bits), which effectively rounds up to factor of 256
*/
#define SHRINKFACTOR(c) (c / 2)
#define GROWFACTOR(c) ((c * 3 + 1) / 2)

CF_INLINE CFIndex __CFStrNewCapacity(CFMutableStringRef str, CFIndex reqCapacity, CFIndex capacity, Boolean leaveExtraRoom) {
    if (capacity != 0 || reqCapacity != 0) {	/* If initially zero, and space not needed, leave it at that... */
        if ((capacity < reqCapacity) ||		/* We definitely need the room... */
            (!__CFStrCapacityProvidedExternally(str) && 	/* Assuming we control the capacity... */
		((reqCapacity < SHRINKFACTOR(capacity)) ||		/* ...we have too much room! */
                 (!leaveExtraRoom && (reqCapacity < capacity))))) {	/* ...we need to eliminate the extra space... */
            CFIndex newCapacity = leaveExtraRoom ? GROWFACTOR(reqCapacity) : reqCapacity;	/* Grow by 3/2 if extra room is desired */
	    CFIndex desiredCapacity = __CFStrDesiredCapacity(str);
            if (newCapacity < desiredCapacity) newCapacity = desiredCapacity;	/* Provide at least the desired capacity */
            newCapacity = (newCapacity + 7) & ~7;	/* Round up to next 8 bytes */
	    if (__CFStrHasContentsAllocator(str)) {	/* Also apply any preferred size from the allocator... */
                newCapacity = CFAllocatorGetPreferredSizeForSize(__CFStrContentsAllocator(str), newCapacity, 0);
	    }
            return newCapacity; // If packing: __CFStrUnpackNumber(__CFStrPackNumber(newCapacity));
        }
    }
    return capacity;
}

/* Reallocates the backing store of the string to accomodate the new length. Space is reserved or characters are deleted as indicated by editLocation and changeInLength. The length is updated to reflect the new state. Will also maintain a length byte and a null byte in 8-bit strings. If length cannot fit in length byte, the space will still be reserved, but will be 0. (Hence the reason the length byte should never be looked at as length unless there is no explicit length.)
*/
static void __CFStringChangeSize(CFMutableStringRef str, CFRange deleteRange, CFIndex insertLength, Boolean makeUnicode) {
    const UInt8 *curContents = __CFStrContents(str);
    CFIndex curLength = curContents ? __CFStrLength2(str, curContents) : 0;
    CFIndex newLength = curLength + insertLength - deleteRange.length;

    __CFAssertIfFixedLengthIsOK(str, newLength);

    {
    Boolean oldIsUnicode = __CFStrIsUnicode(str);
    Boolean newIsUnicode = makeUnicode || oldIsUnicode;
    Boolean useLengthAndNullBytes = !newIsUnicode && (newLength > 0);
    CFIndex numExtraBytes = useLengthAndNullBytes ? 2 : 0;	/* 2 extra bytes to keep the length byte & null... */
    CFIndex curCapacity = __CFStrCapacity(str);
    CFIndex newCapacity = __CFStrNewCapacity(str, newLength * (newIsUnicode ? sizeof(UniChar) : sizeof(UInt8)) + numExtraBytes, curCapacity, TRUE);
    Boolean allocNewBuffer = (newCapacity != curCapacity) || (!oldIsUnicode && newIsUnicode);
    UInt8 *newContents =  allocNewBuffer ? __CFStrAllocateMutableContents(str, newCapacity) : (UInt8 *)curContents;
    Boolean hasLengthAndNullBytes = __CFStrHasLengthByte(str);

    CFAssert1(hasLengthAndNullBytes == __CFStrHasNullByte(str), __kCFLogAssertion, "%s(): Invalid state in 8-bit string", __PRETTY_FUNCTION__);

    if (hasLengthAndNullBytes) curContents++;
    if (useLengthAndNullBytes) newContents++;

    if (curContents) {
        if (oldIsUnicode == newIsUnicode) {
            UInt8 *newDiv = newContents + (deleteRange.location + insertLength) * (newIsUnicode ? sizeof(UniChar) : sizeof(UInt8));
            const UInt8 *curDiv = curContents + (deleteRange.location + deleteRange.length) * (newIsUnicode ? sizeof(UniChar) : sizeof(UInt8));
            if (newContents != curContents) {	/* Move first block, before the deletion zone */
                CFMoveMemory(newContents, curContents, deleteRange.location * (newIsUnicode ? sizeof(UniChar) : sizeof(UInt8)));
            }
            if (newDiv != curDiv) {	/* Move stuff after the deletion zone */
                CFMoveMemory(newDiv, curDiv, (curLength - (deleteRange.location + deleteRange.length)) * (newIsUnicode ? sizeof(UniChar) : sizeof(UInt8)));
            }
        } else if (newIsUnicode) {	
            UInt8 *newDiv = newContents + (deleteRange.location + insertLength) * sizeof(UniChar);
            const UInt8 *curDiv = curContents + (deleteRange.location + deleteRange.length) * sizeof(UInt8);
            __CFStrConvertBytesToUnicode(curContents, (UniChar *)newContents, deleteRange.location);
            __CFStrConvertBytesToUnicode(curDiv, (UniChar *)newDiv, curLength - (deleteRange.location + deleteRange.length));
        } else {	/* This means the string is Unicode but we are thinning; we should not have gotten here... */
            CFLog(__kCFLogAssertion, CFSTR("Should not have gotten here"));	/* ??? */
        }
        if (hasLengthAndNullBytes) curContents--;	/* Undo the damage from above */
        if (allocNewBuffer) __CFStrDeallocateMutableContents(str, (void *)curContents);
    }

    if (!newIsUnicode) {
        if (useLengthAndNullBytes) {
            newContents[newLength] = 0;	/* Always have null byte, if not unicode */
            newContents--;	/* Undo the damage from above */
            newContents[0] = __CFCanUseLengthByte(newLength) ? (UInt8)newLength : 0;
	    if (!hasLengthAndNullBytes) __CFStrSetHasLengthAndNullBytes(str);
	} else {
	    if (hasLengthAndNullBytes) __CFStrClearHasLengthAndNullBytes(str);
	}
	if (oldIsUnicode) __CFStrClearUnicode(str);
   } else {	// New is unicode...
	if (!oldIsUnicode) __CFStrSetUnicode(str);
	if (hasLengthAndNullBytes) __CFStrClearHasLengthAndNullBytes(str);
    }
    __CFStrSetExplicitLength(str, newLength);

    if (allocNewBuffer) {
        __CFStrSetCapacity(str, newCapacity);
        __CFStrClearCapacityProvidedExternally(str);
        __CFStrSetContentPtr(str, newContents);
    }
    }
}

static Boolean CFStrIsUnicode(CFStringRef str) {
    CF_OBJC_FUNCDISPATCH0(String, Boolean, str, "_encodingCantBeStoredInEightBitCFString");
    return __CFStrIsUnicode(str);
}

// Can pass in NSString as replacement string

static void __CFStringReplace(CFMutableStringRef str, CFRange range, CFStringRef replacement) {
    CFIndex replacementLength = CFStringGetLength(replacement);

    __CFStringChangeSize(str, range, replacementLength, (replacementLength > 0) && CFStrIsUnicode(replacement));

    if (__CFStrIsUnicode(str)) {
        UniChar *contents = (UniChar *)__CFStrContents(str);
        CFStringGetCharacters(replacement, CFRangeMake(0, replacementLength), contents + range.location);
    } else {
        UInt8 *contents = (UInt8 *)__CFStrContents(str);
        CFStringGetBytes(replacement, CFRangeMake(0, replacementLength), __CFStringGetEightBitStringEncoding(), 0, FALSE, contents + range.location + __CFStrSkipAnyLengthByte(str), replacementLength, NULL);
    }
}

CFAllocatorRef __CFStringGetAllocator(CFTypeRef cf) {
    CFStringRef str = cf;
    return __CFStrHasCustomAllocator(str) ? __CFStrCustomAllocator(str) : kCFAllocatorSystemDefault;
}


void __CFStringDeallocate(CFTypeRef cf) {
    CFStringRef str = cf;
    Boolean hasCustomAllocator = __CFStrHasCustomAllocator(str);
    CFAllocatorRef alloc = hasCustomAllocator ? __CFStrCustomAllocator(str) : kCFAllocatorSystemDefault;

    // constantStringAllocatorForDebugging is not around unless DEBUG is defined, but neither is CFAssert2()...
    CFAssert1(alloc != constantStringAllocatorForDebugging, __kCFLogAssertion, "Tried to deallocate CFSTR(\"%@\")", str);

    if (!__CFStrIsInline(str)) {
        UInt8 *contents;
	Boolean mutable = __CFStrIsMutable(str);
        if (__CFStrFreeContentsWhenDone(str) && (contents = (UInt8 *)__CFStrContents(str))) {
            if (mutable) {
	        __CFStrDeallocateMutableContents((CFMutableStringRef)str, contents);
	    } else {
		if (__CFStrHasContentsDeallocator(str)) {
                    CFAllocatorRef contentsDeallocator = __CFStrContentsDeallocator(str);
		    CFAllocatorDeallocate(contentsDeallocator, contents);
		    CFRelease(contentsDeallocator);
		} else {
		    CFAllocatorDeallocate(alloc, contents);
		}
	    }
	}
	if (mutable && __CFStrHasContentsAllocator(str)) CFRelease(__CFStrContentsAllocator((CFMutableStringRef)str));
    }
    CFAllocatorDeallocate(alloc, (void *)str);
    if (hasCustomAllocator) CFRelease(alloc);
}


CFTypeID CFStringGetTypeID(void) {
    return __kCFStringTypeID;
}


Boolean __CFStringEqual(CFTypeRef cf1, CFTypeRef cf2) {
    CFStringRef str1 = cf1;
    CFStringRef str2 = cf2;
    const UInt8 *contents1;
    const UInt8 *contents2;
    CFIndex len1;
    CFIndex cnt;

    /* !!! We do not need IsString assertions, as the CFBase runtime assures this */
    /* !!! We do not need == test, as the CFBase runtime assures this */

    contents1 = __CFStrContents(str1);
    contents2 = __CFStrContents(str2);
    len1 = __CFStrLength2(str1, contents1);

    if (len1 != __CFStrLength2(str2, contents2)) return FALSE;

    contents1 += __CFStrSkipAnyLengthByte(str1);
    contents2 += __CFStrSkipAnyLengthByte(str2);

    if (__CFStrIsEightBit(str1) && __CFStrIsEightBit(str2)) {
        return strncmp((const char *)contents1, (const char *)contents2, len1) ? FALSE : TRUE;
    } else if (__CFStrIsEightBit(str1)) {	/* One string has Unicode contents */
        CFStringBuffer buf;

        CFStringBufferInit(&buf, str1, 0, len1);
	for (cnt = 0; cnt < len1; cnt++) {
	    if (CFStringBufferCurrentCharacter(&buf) != ((UniChar *)contents2)[cnt]) return FALSE;
            CFStringBufferIncrementLocation(&buf);
  	}
    } else if (__CFStrIsEightBit(str2)) {	/* One string has Unicode contents */
        CFStringBuffer buf;

        CFStringBufferInit(&buf, str2, 0, len1);
        for (cnt = 0; cnt < len1; cnt++) {
            if (CFStringBufferCurrentCharacter(&buf) != ((UniChar *)contents1)[cnt]) return FALSE;
            CFStringBufferIncrementLocation(&buf);
        }
    } else {					/* Both strings have Unicode contents */
        for (cnt = 0; cnt < len1; cnt++) {
            if (((UniChar *)contents1)[cnt] != ((UniChar *)contents2)[cnt]) return FALSE;
        }
    }
    return TRUE;
}


/* String hashing: Should give the same results whatever the encoding; so we hash UniChars.
If the length is less than or equal to 16, then the hash function is simply the 
following (n is the nth UniChar character, starting from 0):
   
  hash(-1) = length
  hash(n) = hash(n-1) * 257 + unichar(n);
  Hash = hash(length-1) * ((length & 31) + 1)

If the length is greater than 16, then the above algorithm applies to 
characters 0..7 and length-8..length-1; thus the first and last 8 characters.
*/
CFHashCode __CFStringHash(CFTypeRef cf) {
    CFStringRef str = cf;
    const UInt8 *contents;
    CFIndex len;
    CFIndex cnt;
    UInt32 result;

    /* !!! We do not need an IsString assertion here, as this is called by the CFBase runtime only */

    contents = __CFStrContents(str);
    len = __CFStrLength2(str, contents);
    result = len;
    if (__CFStrIsEightBit(str)) {
        CFStringBuffer buf;

        CFStringBufferInit(&buf, str, 0, len);

        if (len <= 16) {
            for (cnt = 0; cnt < len; cnt++) {
                result += (result << 8) + CFStringBufferCurrentCharacter(&buf);
                CFStringBufferIncrementLocation(&buf);
            }
        } else {
            for (cnt = 0; cnt < 8; cnt++) {
                result += (result << 8) + CFStringBufferCurrentCharacter(&buf);
                CFStringBufferIncrementLocation(&buf);
            }
            CFStringBufferSetLocation(&buf, len - 8);
            for (cnt = len - 8; cnt < len; cnt++) {
                result += (result << 8) + CFStringBufferCurrentCharacter(&buf);
                CFStringBufferIncrementLocation(&buf);
            }
        }
    } else {
        const UniChar *uContents = (UniChar *)contents;
        if (len <= 16) {
            for (cnt = 0; cnt < len; cnt++) result += (result << 8) + uContents[cnt];
        } else {
            for (cnt = 0; cnt < 8; cnt++) result += (result << 8) + uContents[cnt];
            for (cnt = len - 8; cnt < len; cnt++) result += (result << 8) + uContents[cnt];
        }
    }
    result += (result << (len & 31));
    return result;
}


CFStringRef  __CFStringCopyDescription(CFTypeRef cf) {
    /* !!! We do not need an IsString assertion here, as this is called by the CFBase runtime only */
    return CFStringCreateCopy(__CFStringGetAllocator(cf), cf);
}

#define ALLOCATORSFREEFUNC ((void *)-1)

/* contentsDeallocator indicates how to free the data if it's noCopy == TRUE:
	kCFAllocatorNull: don't free
	ALLOCATORSFREEFUNC: free with main allocator's free func (don't pass in the real func ptr here)
	NULL: default allocator
	otherwise it's the allocator that should be used (it will be explicitly stored)
   if noCopy == FALSE, then freeFunc should be ALLOCATORSFREEFUNC
   hasLengthByte, hasNullByte: refers to bytes; used only if encoding != Unicode
   possiblyExternalFormat indicates that the bytes might have BOM and be swapped
   tryToReduceUnicode means that the Unicode should be checked to see if it contains just ASCII (and reduce it if so)
   numBytes contains the actual number of bytes in "bytes", including Length byte, 
	BUT not the NULL byte at the end
   bytes should not contain BOM characters
*/
CFStringRef __CFStringCreateImmutableFunnel2(
			CFAllocatorRef alloc, const void *bytes, CFIndex numBytes, CFStringEncoding encoding, 
		 	Boolean possiblyExternalFormat, Boolean tryToReduceUnicode, Boolean hasLengthByte, Boolean hasNullByte, Boolean noCopy, 
			CFAllocatorRef contentsDeallocator) {

    CFMutableStringRef str;
    CFVarWidthCharBuffer vBuf;
    CFIndex size;
    Boolean useLengthByte = FALSE;
    Boolean useNullByte = FALSE;
    Boolean useInlineData = FALSE;
    Boolean useSystemDefaultAllocator;

    if (alloc == NULL) alloc = CFAllocatorGetDefault();
    useSystemDefaultAllocator = (alloc == kCFAllocatorSystemDefault);

    if (contentsDeallocator == ALLOCATORSFREEFUNC) {
	contentsDeallocator = alloc;
    } else if (contentsDeallocator == NULL) {
	contentsDeallocator = CFAllocatorGetDefault();
    }

    if (useSystemDefaultAllocator && numBytes == 0) {
	if (noCopy && contentsDeallocator != kCFAllocatorNull) {	// See 2365208... This change was done after Sonata; before we didn't free the bytes at all (leak).
	    CFAllocatorDeallocate(contentsDeallocator, (void *)bytes); 
	}
	return CFRetain(kCFEmptyString);	// Quick exit; won't catch all empty strings, but most
    }

    // At this point, contentsDeallocator is either same as alloc, or kCFAllocatorNull, or something else, but not NULL

    vBuf.shouldFreeChars = FALSE;	// We use this to remember to free the buffer possibly allocated by decode

    // First check to see if the data needs to be converted...
    // ??? We could be more efficient here and in some cases (Unicode data) eliminate a copy

    if ((encoding == kCFStringEncodingUnicode && possiblyExternalFormat) || (encoding != kCFStringEncodingUnicode && !__CFCanUseEightBitCFStringForBytes(bytes, numBytes, encoding))) {
        const void *realBytes = (UInt8*) bytes + (hasLengthByte ? 1 : 0);
        CFIndex realNumBytes = numBytes - (hasLengthByte ? 1 : 0);
        Boolean usingPassedInMemory = FALSE;

	vBuf.allocator = CFAllocatorGetDefault();	// We don't want to use client's allocator for temp stuff
        vBuf.chars.unicode = NULL;	// This will cause the decode function to allocate memory if necessary

        if (!__CFStringDecodeByteStream2(realBytes, realNumBytes, encoding, FALSE, &vBuf, &usingPassedInMemory)) {
	    return NULL;		// !!! Is this acceptable failure mode?
	}

        encoding = vBuf.isASCII ? kCFStringEncodingASCII : kCFStringEncodingUnicode;

        if (!usingPassedInMemory) {

            // Make the parameters fit the new situation
            numBytes = vBuf.isASCII ? vBuf.numChars : (vBuf.numChars * sizeof(UniChar));
            hasLengthByte = hasNullByte = FALSE;

            // Get rid of the original buffer if its not being used
            if (noCopy && contentsDeallocator != kCFAllocatorNull) {
                CFAllocatorDeallocate(contentsDeallocator, (void *)bytes);
            }
            contentsDeallocator = alloc;	// At this point we are using the string's allocator, as the original buffer is gone...

            // See if we can reuse any storage the decode func might have allocated
            // We do this only for Unicode, as otherwise we would not have NULL and Length bytes

            if (vBuf.shouldFreeChars && (alloc == vBuf.allocator) && encoding == kCFStringEncodingUnicode) {
                vBuf.shouldFreeChars = FALSE;	// Transferring ownership to the CFString
                bytes = CFAllocatorReallocate(vBuf.allocator, (void *)vBuf.chars.unicode, numBytes, 0);	// Tighten up the storage
                noCopy = TRUE;
            } else {
                bytes = vBuf.chars.unicode;
                noCopy = FALSE;			// Can't do noCopy anymore
                // If vBuf.shouldFreeChars is TRUE, the buffer will be freed as intended near the end of this func
            }

        }

	// At this point, all necessary input arguments have been changed to reflect the new state

    } else if (encoding == kCFStringEncodingUnicode && tryToReduceUnicode) {	// Check to see if we can reduce Unicode to ASCII
        CFIndex cnt;
        CFIndex len = numBytes / sizeof(UniChar);
        Boolean allASCII = TRUE;

        for (cnt = 0; cnt < len; cnt++) if (((const UniChar *)bytes)[cnt] > 127) {
            allASCII = FALSE;
            break;
        }

        if (allASCII) {	// Yes we can!
            UInt8 *ptr, *mem;
            hasLengthByte = __CFCanUseLengthByte(len);
            hasNullByte = TRUE;
            numBytes = (len + 1 + (hasLengthByte ? 1 : 0)) * sizeof(UInt8);	// NULL and possible length byte
            // See if we can use that temporary local buffer in vBuf...
            mem = ptr = (UInt8 *)((numBytes >= __kCFVarWidthLocalBufferSize) ? CFAllocatorAllocate(alloc, numBytes, 0) : vBuf.localBuffer);
	    if (mem != vBuf.localBuffer) __CFSetLastAllocationEventName(mem, "CFString (store)");
            if (hasLengthByte) *ptr++ = len;
            for (cnt = 0; cnt < len; cnt++) ptr[cnt] = ((const UniChar *)bytes)[cnt];
            ptr[len] = 0;
            if (noCopy && contentsDeallocator != kCFAllocatorNull) {
                CFAllocatorDeallocate(contentsDeallocator, (void *)bytes);
            }
            bytes = mem;
            encoding = kCFStringEncodingASCII;
            contentsDeallocator = alloc;	// At this point we are using the string's allocator, as the original buffer is gone...
            noCopy = (numBytes >= __kCFVarWidthLocalBufferSize);	// If we had to allocate it, make sure it's kept around
	    numBytes--;		// Should not contain the NULL byte at end...
        }

        // At this point, all necessary input arguments have been changed to reflect the new state
    }

    // Now determine the necessary size

    size = sizeof(CFRuntimeBase);    
    if (!useSystemDefaultAllocator) size += sizeof(CFAllocatorRef);	// This applies to all formats

    if (noCopy) {

        size += sizeof(void *);				// Pointer to the buffer
        if (contentsDeallocator != alloc && contentsDeallocator != kCFAllocatorNull) {
            size += sizeof(void *);	// The contentsDeallocator
        }
	if (!hasLengthByte) size += sizeof(SInt32);	// Explicit length
	useLengthByte = hasLengthByte;
	useNullByte = hasNullByte;

    } else {	// Inline data; reserve space for it

	useInlineData = TRUE;
        size += numBytes;

        if (hasLengthByte || (encoding != kCFStringEncodingUnicode && __CFCanUseLengthByte(numBytes))) {
            useLengthByte = TRUE;
            if (!hasLengthByte) size += 1;
        } else {
            size += sizeof(SInt32);	// Explicit length
        }	    
	if (hasNullByte || encoding != kCFStringEncodingUnicode) {
            useNullByte = TRUE;
            size += 1;
	}
    }

    // Finally, allocate!

    str = __CFStrAlloc(alloc, size);
    __CFSetLastAllocationEventName(str, "CFString (immutable)");

    __CFStrSetInfoBits(str,
			(!useSystemDefaultAllocator ? __kCFHasAllocator : 0) |
                        (useInlineData ? __kCFHasInlineData : (contentsDeallocator == alloc ? __kCFHasExternalDataDefaultFree : (contentsDeallocator == kCFAllocatorNull ? __kCFHasExternalDataNoFree : __kCFHasExternalDataCustomFree))) |
			((encoding == kCFStringEncodingUnicode) ? __kCFIsUnicode : 0) |
			(useNullByte ? __kCFHasNullByte : 0) |
			(useLengthByte ? __kCFHasLengthByte : 0));

    if (!useLengthByte) {
	CFIndex length = numBytes - (hasLengthByte ? 1 : 0);
	if (encoding == kCFStringEncodingUnicode) length /= sizeof(UniChar);
	__CFStrSetExplicitLength(str, length);
    }

    if (!useSystemDefaultAllocator) __CFStrSetCustomAllocator(str, CFRetain(alloc));

    if (useInlineData) {
	UInt8 *contents = (UInt8 *)__CFStrContents(str);
	if (useLengthByte && !hasLengthByte) *contents++ = numBytes;
	CFMoveMemory(contents, bytes, numBytes);
	if (useNullByte) contents[numBytes] = 0;
    } else {
	__CFStrSetContentPtr(str, bytes);
	if (contentsDeallocator != alloc && contentsDeallocator != kCFAllocatorNull) __CFStrSetContentsDeallocator(str, CFRetain(contentsDeallocator)); 
    }
    if (vBuf.shouldFreeChars) CFAllocatorDeallocate(vBuf.allocator, (void *)bytes);

    return str;
}


CFStringRef  CFStringCreateWithPascalString(CFAllocatorRef alloc, ConstStringPtr pStr, CFStringEncoding encoding) {
    CFIndex len = (CFIndex)(*(UInt8 *)pStr);
    return __CFStringCreateImmutableFunnel2(alloc, pStr, len+1, encoding, FALSE, FALSE, TRUE, FALSE, FALSE, ALLOCATORSFREEFUNC);
}


CFStringRef  CFStringCreateWithCString(CFAllocatorRef alloc, const char *cStr, CFStringEncoding encoding) {
    CFIndex len = strlen(cStr);
    return __CFStringCreateImmutableFunnel2(alloc, cStr, len, encoding, FALSE, FALSE, FALSE, TRUE, FALSE, ALLOCATORSFREEFUNC);
}

CFStringRef  CFStringCreateWithPascalStringNoCopy(CFAllocatorRef alloc, ConstStringPtr pStr, CFStringEncoding encoding, CFAllocatorRef contentsDeallocator) {
    CFIndex len = (CFIndex)(*(UInt8 *)pStr);
    return __CFStringCreateImmutableFunnel2(alloc, pStr, len+1, encoding, FALSE, FALSE, TRUE, FALSE, TRUE, contentsDeallocator);
}


CFStringRef  CFStringCreateWithCStringNoCopy(CFAllocatorRef alloc, const char *cStr, CFStringEncoding encoding, CFAllocatorRef contentsDeallocator) {
    CFIndex len = strlen(cStr);
    return __CFStringCreateImmutableFunnel2(alloc, cStr, len, encoding, FALSE, FALSE, FALSE, TRUE, TRUE, contentsDeallocator);
}


CFStringRef  CFStringCreateWithCharacters(CFAllocatorRef alloc, const UniChar *chars, CFIndex numChars) {
    return __CFStringCreateImmutableFunnel2(alloc, chars, numChars * sizeof(UniChar), kCFStringEncodingUnicode, FALSE, TRUE, FALSE, FALSE, FALSE, ALLOCATORSFREEFUNC);
}


CFStringRef  CFStringCreateWithCharactersNoCopy(CFAllocatorRef alloc, const UniChar *chars, CFIndex numChars, CFAllocatorRef contentsDeallocator) {
    return __CFStringCreateImmutableFunnel2(alloc, chars, numChars * sizeof(UniChar), kCFStringEncodingUnicode, FALSE, FALSE, FALSE, FALSE, TRUE, contentsDeallocator);
}


CFStringRef  CFStringCreateWithBytes(CFAllocatorRef alloc, const UInt8 *bytes, CFIndex numBytes, CFStringEncoding encoding, Boolean externalFormat) {
    return __CFStringCreateImmutableFunnel2(alloc, bytes, numBytes, encoding, externalFormat, TRUE, FALSE, FALSE, FALSE, ALLOCATORSFREEFUNC);
}

CFStringRef  CFStringCreateWithFormatAndArguments(CFAllocatorRef alloc, CFDictionaryRef formatOptions, CFStringRef format, va_list arguments) {
    return _CFStringCreateWithFormatAndArgumentsAux(alloc, NULL, formatOptions, format, arguments);
}

CFStringRef  _CFStringCreateWithFormatAndArgumentsAux(CFAllocatorRef alloc, CFStringRef (*copyDescFunc)(void *, CFDictionaryRef), CFDictionaryRef formatOptions, CFStringRef format, va_list arguments) {
    CFStringRef str;
    CFMutableStringRef outputString = CFStringCreateMutable(CFAllocatorGetDefault(), 0); //should use alloc if no copy/release
    CFIndex formatLen = CFStringGetLength(format);
    if (formatLen > __CFStrDesiredCapacity(outputString)) {
        __CFStrSetDesiredCapacity(outputString, formatLen);	// optimization
    }
    _CFStringAppendFormatAndArgumentsAux(outputString, copyDescFunc, formatOptions, format, arguments);
    // ??? copy/release should not be necessary here -- just make immutable, compress if possible
    str = CFStringCreateCopy(alloc, outputString);
    CFRelease(outputString);
    return str;
}

CFStringRef  CFStringCreateWithFormat(CFAllocatorRef alloc, CFDictionaryRef formatOptions, CFStringRef format, ...) {
    CFStringRef result;
    va_list argList;

    va_start(argList, format);
    result = CFStringCreateWithFormatAndArguments(alloc, formatOptions, format, argList);
    va_end(argList);

    return result;
}


CFStringRef CFStringCreateWithSubstring(CFAllocatorRef alloc, CFStringRef str, CFRange range) {
    CF_OBJC_FUNCDISPATCH1(String, CFStringRef , str, "_createSubstringWithRange:", CFRangeMake(range.location, range.length));

    __CFAssertIsString(str);
    __CFAssertRangeIsInStringBounds(str, range.location, range.length);

    if ((range.location == 0) && (range.length == __CFStrLength(str))) {	/* The substring is the whole string... */
	return CFStringCreateCopy(alloc, str);
    } else if (__CFStrIsEightBit(str)) {
	const UInt8 *contents = __CFStrContents(str);
        return __CFStringCreateImmutableFunnel2(alloc, contents + range.location + __CFStrSkipAnyLengthByte(str), range.length, __CFStringGetEightBitStringEncoding(), FALSE, FALSE, FALSE, FALSE, FALSE, ALLOCATORSFREEFUNC);
    } else {
	const UniChar *contents = __CFStrContents(str);
        return __CFStringCreateImmutableFunnel2(alloc, contents + range.location, range.length * sizeof(UniChar), kCFStringEncodingUnicode, FALSE, TRUE, FALSE, FALSE, FALSE, ALLOCATORSFREEFUNC);
    }
}


CFStringRef  CFStringCreateCopy(CFAllocatorRef alloc, CFStringRef str) {
    CF_OBJC_FUNCDISPATCH0(String, CFStringRef, str, "copy");

    __CFAssertIsString(str);
    if (!__CFStrIsMutable(str) && ((alloc ? alloc : CFAllocatorGetDefault()) == __CFStringGetAllocator(str))) {
	CFRetain(str);
	return str;
    } else if (__CFStrIsEightBit(str)) {
        const UInt8 *contents = __CFStrContents(str);
        return __CFStringCreateImmutableFunnel2(alloc, contents + __CFStrSkipAnyLengthByte(str), __CFStrLength2(str, contents), __CFStringGetEightBitStringEncoding(), FALSE, FALSE, FALSE, FALSE, FALSE, ALLOCATORSFREEFUNC);
    } else {
        const UniChar *contents = __CFStrContents(str);
        return __CFStringCreateImmutableFunnel2(alloc, contents, __CFStrLength2(str, contents) * sizeof(UniChar), kCFStringEncodingUnicode, FALSE, TRUE, FALSE, FALSE, FALSE, ALLOCATORSFREEFUNC);
    }
}



/*** Constant string stuff... ***/

/* For now we call a function to create a constant string and keep previously created constant strings in a dictionary. The keys are the 8-bit constant C-strings from the compiler; the values are the CFStrings created for them.
*/
static CFMutableDictionaryRef constantStringTable = NULL;

static CFStringRef __cStrCopyDescription(const void *ptr) {
    return CFStringCreateWithCStringNoCopy(NULL, (const char *)ptr, __CFStringGetEightBitStringEncoding(), kCFAllocatorNull);
}

static Boolean __cStrEqual(const void *ptr1, const void *ptr2) {
    return (strcmp((const char *)ptr1, (const char *)ptr2) == 0);
}

static CFHashCode __cStrHash(const void *ptr) {
    // It doesn't quite matter if we convert to Unicode correctly, as long as we do it consistently    
    const unsigned char *cStr = (const unsigned char *)ptr;
    CFIndex len = strlen(cStr);
    CFHashCode result = 0;
    if (len <= 4) {	// All chars
        unsigned cnt = len;
        while (cnt--) result += (result << 8) + *cStr++;
    } else {		// First and last 2 chars
        result += (result << 8) + cStr[0];
        result += (result << 8) + cStr[1];
        result += (result << 8) + cStr[len-2];
        result += (result << 8) + cStr[len-1];
    }
    result += (result << (len & 31));
    return result;    
}

#if defined(DEBUG)
/* We use a special allocator (which simply calls through to the default) for constant strings so that we can catch them being freed...
*/
static void *csRealloc(void *oPtr, CFIndex size, CFOptionFlags hint, void *info) {
    return CFAllocatorReallocate(NULL, oPtr, size, hint);
}

static void *csAlloc(CFIndex size, CFOptionFlags hint, void *info) {
    return CFAllocatorAllocate(NULL, size, hint);
}

static void csDealloc(void *ptr, void *info) {
     CFAllocatorDeallocate(NULL, ptr);
}

static CFStringRef csCopyDescription(const void *info) {
    return CFRetain(CFSTR("Debug allocator for CFSTRs"));
}
#endif

CFStringRef __CFStringMakeConstantString(const char *cStr) {
    CFStringRef result;
    if (constantStringTable == NULL) {
        CFDictionaryKeyCallBacks constantStringCallBacks = {0, NULL, NULL, __cStrCopyDescription, __cStrEqual, __cStrHash};
	constantStringTable = CFDictionaryCreateMutable(NULL, 0, &constantStringCallBacks, &kCFTypeDictionaryValueCallBacks);
#if defined(DEBUG)
	{
            CFAllocatorContext context = {0, NULL, NULL, NULL, csCopyDescription, csAlloc, csRealloc, csDealloc, NULL};
            constantStringAllocatorForDebugging = CFAllocatorCreate(NULL, &context);
	}
#else
#define constantStringAllocatorForDebugging NULL
#endif
    }
    if (!(result = (CFStringRef)CFDictionaryGetValue(constantStringTable, cStr))) {
	char *key;
	result = CFStringCreateWithCString(constantStringAllocatorForDebugging, cStr, __CFStringGetEightBitStringEncoding());
	if (__CFStrIsEightBit(result)) {	
	    key = (char *)__CFStrContents(result) + __CFStrSkipAnyLengthByte(result);
	} else {	// For some reason the string is not 8-bit!
	    key = CFAllocatorAllocate(NULL, strlen(cStr) + 1, 0);
	    __CFSetLastAllocationEventName(key, "CFString (constant contents)");
	    strcpy(key, cStr);	// !!! We will leak this, if the string is removed from the table (or table is freed)
	}
	CFDictionarySetValue(constantStringTable, key, result);
	CFRelease(result);
    }
    return result;
}

#if defined(__MACOS8__)

void __CFStringCleanup (void) {
    /* in case library is unloaded, release store for the constant string table */
    if (constantStringTable != NULL) {
        CFRelease(constantStringTable);
        constantStringTable = NULL;
    }
}

#endif

/* If client does not provide a minimum capacity
*/
#define DEFAULTMINCAPACITY 32

CF_INLINE CFMutableStringRef  __CFStringCreateMutableFunnel(CFAllocatorRef alloc, CFIndex maxLength, UInt32 additionalInfoBits) {
    CFMutableStringRef str;
    Boolean useSystemDefaultAllocator;
    Boolean hasExternalContentsAllocator = (additionalInfoBits & __kCFHasContentsAllocator) ? TRUE : FALSE;

    if (alloc == NULL) alloc = CFAllocatorGetDefault();
    useSystemDefaultAllocator = (alloc == kCFAllocatorSystemDefault);

    // Note that if there is an externalContentsAllocator, then we also have the storage for the string allocator...
    str = __CFStrAlloc(alloc, sizeof(CFRuntimeBase) + sizeof(void *) + sizeof(UInt32) * 3 + (hasExternalContentsAllocator ? sizeof(CFAllocatorRef) * 2 : (useSystemDefaultAllocator ? 0 : sizeof(CFAllocatorRef))));
    __CFSetLastAllocationEventName(str, "CFString (mutable)");

    __CFStrSetInfoBits(str, (!useSystemDefaultAllocator ? __kCFHasAllocator : 0) | __kCFIsMutable | additionalInfoBits);
    str->variants.externalMutable.buffer = NULL;
    __CFStrSetExplicitLength(str, 0);
    str->variants.externalMutable.gapEtc = 0;
    if (!useSystemDefaultAllocator) __CFStrSetCustomAllocator(str, CFRetain(alloc));
    if (maxLength != 0) __CFStrSetIsFixed(str);
    __CFStrSetDesiredCapacity(str, (maxLength == 0) ? DEFAULTMINCAPACITY : maxLength);
    __CFStrSetCapacity(str, 0);
    return str;
}

CFMutableStringRef CFStringCreateMutableWithExternalCharactersNoCopy(CFAllocatorRef alloc, UniChar *chars, CFIndex numChars, CFIndex capacity, CFAllocatorRef externalCharactersAllocator) {
    CFOptionFlags contentsAllocationBits = externalCharactersAllocator ? ((externalCharactersAllocator == kCFAllocatorNull) ? __kCFHasExternalDataNoFree : __kCFHasContentsAllocator) : __kCFHasExternalDataDefaultFree;
    CFMutableStringRef string = __CFStringCreateMutableFunnel(alloc, 0, contentsAllocationBits | __kCFIsExternalMutable | __kCFIsUnicode);
    if (contentsAllocationBits == __kCFHasContentsAllocator) __CFStrSetContentsAllocator(string, CFRetain(externalCharactersAllocator));
    CFStringSetExternalCharactersNoCopy(string, chars, numChars, capacity);
    return string;
}
 
CFMutableStringRef CFStringCreateMutable(CFAllocatorRef alloc, CFIndex maxLength) {
    return __CFStringCreateMutableFunnel(alloc, maxLength, __kCFHasExternalDataDefaultFree);
}

CFMutableStringRef  CFStringCreateMutableCopy(CFAllocatorRef alloc, CFIndex maxLength, CFStringRef string) {
    CFMutableStringRef newString;

    CF_OBJC_FUNCDISPATCH0(String, CFMutableStringRef, string, "mutableCopy");

    __CFAssertIsString(string);

    newString = CFStringCreateMutable(alloc, maxLength);
    __CFStringReplace(newString, CFRangeMake(0, 0), string);

    return newString;
}


CFIndex CFStringGetLength(CFStringRef str) {

    CF_OBJC_FUNCDISPATCH0(String, CFIndex, str, "length");

    __CFAssertIsString(str);
    return __CFStrLength(str);
}


UniChar CFStringGetCharacterAtIndex(CFStringRef str, CFIndex index) {
    const UInt8 *contents;

    CF_OBJC_FUNCDISPATCH1(String, UniChar, str, "characterAtIndex:", index);

    __CFAssertIsString(str);
    __CFAssertIndexIsInStringBounds(str, index);

    contents = __CFStrContents(str);
    if (__CFStrIsEightBit(str)) {
        contents += __CFStrSkipAnyLengthByte(str);

        if (contents[index] < 0x80) { // ASCII
            return (UniChar)contents[index];
        } else if (__CFCharToUniCharFunc) {
            UniChar ch;
            return (__CFCharToUniCharFunc(0, contents[index], &ch) ? ch : 0xFFFE);
        } else {
            UniChar buffer[8];
            __CFStrConvertBytesToUnicode(contents + index, buffer, 1);
            return *buffer;
        }
    }

    return ((UniChar *)contents)[index];
}


void CFStringGetCharacters(CFStringRef str, CFRange range, UniChar *buffer) {
    const UInt8 *contents;

    CF_OBJC_FUNCDISPATCH2(String, void, str, "getCharacters:range:", buffer, CFRangeMake(range.location, range.length));

    __CFAssertIsString(str);
    __CFAssertRangeIsInStringBounds(str, range.location, range.length);

    contents = __CFStrContents(str);
    if (__CFStrIsEightBit(str)) {
        __CFStrConvertBytesToUnicode(((UInt8 *)contents) + (range.location + __CFStrSkipAnyLengthByte(str)), buffer, range.length);
    } else {
	const UniChar *uContents = ((UniChar *)contents) + range.location;
        CFMoveMemory(buffer, uContents, range.length * sizeof(UniChar));
    }
}


CFIndex CFStringGetBytes(CFStringRef str, CFRange range, CFStringEncoding encoding, UInt8 lossByte, Boolean isExternalRepresentation, UInt8 *buffer, CFIndex maxBufLen, CFIndex *usedBufLen) {

    /* No objc dispatch needed here since __CFStringEncodeByteStream works with both CFString and NSString */

    if (!CF_IS_OBJC(String, str)) {	// If we can grope the ivars, let's do it...
        __CFAssertIsString(str);
        __CFAssertRangeIsInStringBounds(str, range.location, range.length);

        if (__CFStrIsEightBit(str) && ((__CFStringGetEightBitStringEncoding() == encoding) || (__CFStringGetEightBitStringEncoding() == kCFStringEncodingASCII && __CFStringEncodingIsSupersetOfASCII(encoding)))) {	// Requested encoding is equal to the encoding in string
            const unsigned char *contents = __CFStrContents(str);
            CFIndex cLength = range.length;

            if (buffer) {
                if (cLength > maxBufLen) cLength = maxBufLen;
                CFMoveMemory(buffer, contents + __CFStrSkipAnyLengthByte(str) + range.location, cLength);
            }
            if (usedBufLen) *usedBufLen = cLength;

            return cLength;
        }
    }

    return __CFStringEncodeByteStream(str, range.location, range.length, isExternalRepresentation, encoding, lossByte, buffer, maxBufLen, usedBufLen);
}


ConstStringPtr CFStringGetPascalStringPtr (CFStringRef str, CFStringEncoding encoding) {

    if (!CF_IS_OBJC(String, str)) {	/* ??? Hope the compiler optimizes this away if OBJC_MAPPINGS is not on */
        __CFAssertIsString(str);
        if (__CFStrHasLengthByte(str) && __CFStrIsEightBit(str) && ((__CFStringGetEightBitStringEncoding() == encoding) || (__CFStringGetEightBitStringEncoding() == kCFStringEncodingASCII && __CFStringEncodingIsSupersetOfASCII(encoding)))) {	// Requested encoding is equal to the encoding in string || the contents is in ASCII
	    const UInt8 *contents = __CFStrContents(str);
	    if (__CFStrHasExplicitLength(str) && (__CFStrLength2(str, contents) != (SInt32)(*contents))) return NULL;	// Invalid length byte
	    return (ConstStringPtr)contents;
	}
	// ??? Also check for encoding = SystemEncoding and perhaps bytes are all ASCII?
    }
    return NULL;
}


const char * CFStringGetCStringPtr(CFStringRef str, CFStringEncoding encoding) {

    if (encoding != __CFStringGetEightBitStringEncoding() && (kCFStringEncodingASCII != __CFStringGetEightBitStringEncoding() || !__CFStringEncodingIsSupersetOfASCII(encoding))) return NULL;
    // ??? Also check for encoding = SystemEncoding and perhaps bytes are all ASCII?

    CF_OBJC_FUNCDISPATCH0(String, const char *, str, "cString");

    __CFAssertIsString(str);

    if (__CFStrHasNullByte(str)) {
	return (const char *)__CFStrContents(str) + __CFStrSkipAnyLengthByte(str);
    } else {
	return NULL;
    }
}


const UniChar *CFStringGetCharactersPtr(CFStringRef str) {

    // ??? Need to dispatch to ObjC
    
    if (!CF_IS_OBJC(String, str)) {	/* ??? Hope the compiler optimizes this away if OBJC_MAPPINGS is not on */
        __CFAssertIsString(str);
        if (__CFStrIsUnicode(str)) return (const UniChar *)__CFStrContents(str);
    }
    return NULL;
}


Boolean CFStringGetPascalString(CFStringRef str, Str255 buffer, CFIndex bufferSize, CFStringEncoding encoding) {
    CFIndex length;
    CFIndex usedLen;

    if (CF_IS_OBJC(String, str)) {	/* ??? Hope the compiler optimizes this away if OBJC_MAPPINGS is not on */
	length = CFStringGetLength(str);
        if (!__CFCanUseLengthByte(length)) return NULL; // Can't fit into pstring
    } else {
	const UInt8 *contents;

        __CFAssertIsString(str);

        contents = __CFStrContents(str);
        length = __CFStrLength2(str, contents);

        if (!__CFCanUseLengthByte(length)) return FALSE; // Can't fit into pstring

        if (__CFStrIsEightBit(str) && ((__CFStringGetEightBitStringEncoding() == encoding) || (__CFStringGetEightBitStringEncoding() == kCFStringEncodingASCII && __CFStringEncodingIsSupersetOfASCII(encoding)))) {	// Requested encoding is equal to the encoding in string
	    if (length >= bufferSize) return FALSE;
            CFMoveMemory((void*)(1 + (const char*)buffer), (__CFStrSkipAnyLengthByte(str) + contents), length);
            *buffer = length;
	    return TRUE;
	}
    }

    if (__CFStringEncodeByteStream(str, 0, length, FALSE, encoding, FALSE, (void*)(1 + (UInt8*)buffer), bufferSize - 1, &usedLen) != length) {
#if defined(DEBUG)
	if (bufferSize > 0) {
	    strncpy((char *)buffer + 1, CONVERSIONFAILURESTR, bufferSize - 1);
	    buffer[0] = sizeof(CONVERSIONFAILURESTR) < (bufferSize - 1) ? sizeof(CONVERSIONFAILURESTR) : (bufferSize - 1);
	}
#else
	if (bufferSize > 0) buffer[0] = 0;
#endif
	return FALSE;
    }
    *buffer = usedLen;
    return TRUE;
}
                                   
Boolean CFStringGetCString(CFStringRef str, char *buffer, CFIndex bufferSize, CFStringEncoding encoding) {
    const UInt8 *contents;
    CFIndex len;

    CF_OBJC_FUNCDISPATCH1(String, Boolean, str, "getCString:", buffer);

    __CFAssertIsString(str);

    contents = __CFStrContents(str);
    len = __CFStrLength2(str, contents);

    if (__CFStrIsEightBit(str) && ((__CFStringGetEightBitStringEncoding() == encoding) || (__CFStringGetEightBitStringEncoding() == kCFStringEncodingASCII && __CFStringEncodingIsSupersetOfASCII(encoding)))) {	// Requested encoding is equal to the encoding in string
        if (len >= bufferSize) return FALSE;
	CFMoveMemory(buffer, contents + __CFStrSkipAnyLengthByte(str), len);
	buffer[len] = 0;
        return TRUE;
    } else {
        CFIndex usedLen;

        if (__CFStringEncodeByteStream(str, 0, len, FALSE, encoding, FALSE, (unsigned char*) buffer, bufferSize - 1, &usedLen) == len) {
            buffer[usedLen] = '\0';
            return TRUE;
        } else {
#if defined(DEBUG)
            strncpy(buffer, CONVERSIONFAILURESTR, bufferSize);
#else
	    if (bufferSize > 0) buffer[0] = 0;
#endif
            return FALSE;
        }
    }
}


/* ??? We need to implement some additional flags here
   ??? Also, pay attention to flag 2, which is the NS flag (which CF has as flag 16, w/opposite meaning).
*/
CFComparisonResult CFStringCompareWithOptions(CFStringRef string, CFStringRef string2, CFRange rangeToCompare, CFOptionFlags compareOptions) {
    /* No objc dispatch needed here since CFStringBuffer works with both CFString and NSString */
        CFStringBuffer strBuf1, strBuf2;
        UniChar ch1, ch2;
        Boolean caseInsensitive = (compareOptions & kCFCompareCaseInsensitive ? TRUE : FALSE);

        CFStringBufferInit(&strBuf1, string, rangeToCompare.location, rangeToCompare.location + rangeToCompare.length);
        CFStringBufferInit(&strBuf2, string2, 0, CFStringGetLength(string2));

        while (!CFStringBufferIsAtEnd(&strBuf1) && !CFStringBufferIsAtEnd(&strBuf2)) {
            ch1 = CFStringBufferCurrentCharacter(&strBuf1) ; ch2 = CFStringBufferCurrentCharacter(&strBuf2);

            if (caseInsensitive) {
                if (ch1 < 128) {
                    ch1 += ((ch1 >= 'a' && ch1 <= 'z') ? 'A' - 'a' : 0);
                } else {
                    ch1 = toulower(ch1);
                }
                if (ch2 < 128) {
                    ch2 += ((ch2 >= 'a' && ch2 <= 'z') ? 'A' - 'a' : 0);
                } else {
                    ch2 = toulower(ch2);
                }
            }
            if (ch1 < ch2) return kCFCompareLessThan; else if (ch1 > ch2) return kCFCompareGreaterThan;
            CFStringBufferIncrementLocation(&strBuf1); CFStringBufferIncrementLocation(&strBuf2);
        }
        if (!CFStringBufferIsAtEnd(&strBuf1)) {
            return kCFCompareGreaterThan;
        } else if (!CFStringBufferIsAtEnd(&strBuf2)) {
            return kCFCompareLessThan; 
        } else {
            return kCFCompareEqualTo;
        }
}


CFComparisonResult CFStringCompare(CFStringRef string, CFStringRef str2, CFOptionFlags options) {
    return CFStringCompareWithOptions(string, str2, CFRangeMake(0, CFStringGetLength(string)), options);
}

/* ??? Need to implement nonliteral search; localized find
*/
Boolean CFStringFindWithOptions(CFStringRef string, CFStringRef stringToFind, CFRange rangeToSearch, CFOptionFlags compareOptions, CFRange *result) {
    /* No objc dispatch needed here since NSStringBuffer works with both CFString and NSString */
    int step;
    UInt32 fromLoc, toLoc;	// fromLoc and toLoc are inclusive
    UInt32 cnt, findStrLen = CFStringGetLength(stringToFind);
    Boolean done = FALSE;
    Boolean caseInsensitive = (compareOptions & kCFCompareCaseInsensitive) ? TRUE : FALSE;
    UniChar tmpBuf[MAXTMPBUFFERLEN];
    UniChar *findBuf;
    UniChar ch, ch2;
    CFStringBuffer buf;
    CFAllocatorRef tmpAlloc = NULL;

    if (findStrLen > rangeToSearch.length) return FALSE;

    findBuf = (findStrLen > MAXTMPBUFFERLEN) ? CFAllocatorAllocate(tmpAlloc = CFAllocatorGetDefault(), findStrLen * sizeof(UniChar), 0) : tmpBuf;
    if (findBuf != tmpBuf) __CFSetLastAllocationEventName(findBuf, "CFString (temp)");
    CFStringGetCharacters(stringToFind, CFRangeMake(0, findStrLen), findBuf);

    if (caseInsensitive) {	/* Lower case the search string */
        for (cnt = 0; cnt < findStrLen; cnt++) {
            ch = findBuf[cnt];
            if (ch < 128) {
                if (ch >= 'A' && ch <= 'Z') findBuf[cnt] = (ch - 'A' + 'a');	/* Lower case the cheap way */
            } else {
                findBuf[cnt] = toulower(ch);
            }
        }
    }

    if (compareOptions & kCFCompareBackwards) {
        fromLoc = rangeToSearch.location + rangeToSearch.length - findStrLen;
        toLoc = rangeToSearch.location;
    } else {
        fromLoc = rangeToSearch.location;
        toLoc = rangeToSearch.location + rangeToSearch.length - findStrLen;
    }
    if (compareOptions & kCFCompareAnchored) {	// ??? This can't be here for correct Unicode compares
        toLoc = fromLoc;
    }

    step = (fromLoc <= toLoc) ? 1 : -1;
    cnt = fromLoc;
    CFStringBufferInit(&buf, string, fromLoc, rangeToSearch.location + rangeToSearch.length);

    do {
        UInt32 chCnt;
        if (caseInsensitive) {
            for (chCnt = 0; chCnt < findStrLen; chCnt++) {
                ch2 = CFStringBufferCurrentCharacter(&buf);
                if (ch2 < 128) {
                    if (ch2 >= 'A' && ch2 <= 'Z') ch2 = (ch2 - 'A' + 'a');	/* Lower case the cheap way */
                } else {
                    ch2 = toulower(ch2);
                }
                if (findBuf[chCnt] != ch2) break;
                CFStringBufferIncrementLocation(&buf);
            }
        } else {
            for (chCnt = 0; chCnt < findStrLen; chCnt++) {
                if (findBuf[chCnt] != CFStringBufferCurrentCharacter(&buf)) break;
                CFStringBufferIncrementLocation(&buf);
            }
        }
        if (chCnt == findStrLen) {
            done = TRUE;
            if (result) {
                result->location = cnt;
                result->length = findStrLen;
            }
        } else if (cnt == toLoc) {
            break;
        } else {
            cnt += step;
            CFStringBufferSetLocation(&buf, cnt);
        }
    } while (!done);

    if (findBuf != tmpBuf) CFAllocatorDeallocate(tmpAlloc, findBuf);

    return done;
}


// Functions to deal with special arrays of CFRange, CFDataRef, created by CFStringCreateArrayWithFindResults()

static const void *__rangeRetain(CFAllocatorRef allocator, const void *ptr) {
    CFRetain(*(CFDataRef *)((UInt8 *)ptr + sizeof(CFRange)));
    return ptr;
}

static void __rangeRelease(CFAllocatorRef allocator, const void *ptr) {
    CFRelease(*(CFDataRef *)((UInt8 *)ptr + sizeof(CFRange)));
}

static CFStringRef __rangeCopyDescription(const void *ptr) {
    CFRange range = *(CFRange *)ptr;
    return CFStringCreateWithFormat(NULL /* ??? allocator */, NULL, CFSTR("{%d, %d}"), range.location, range.length);
}

static Boolean	__rangeEqual(const void *ptr1, const void *ptr2) {
    CFRange range1 = *(CFRange *)ptr1;
    CFRange range2 = *(CFRange *)ptr2;
    return (range1.location == range2.location) && (range1.length == range2.length);
}


CFArrayRef CFStringCreateArrayWithFindResults(CFAllocatorRef alloc, CFStringRef string, CFStringRef stringToFind, CFRange rangeToSearch, CFOptionFlags compareOptions) {
    CFRange foundRange;
    UInt32 endIndex = rangeToSearch.location + rangeToSearch.length;
    CFMutableDataRef rangeStorage = NULL;	// Basically an array of CFRange, CFDataRef (packed)
    UInt8 *rangeStorageBytes = NULL;
    CFIndex foundCount = 0;
    CFIndex capacity = 0;		// Number of CFRange, CFDataRef element slots in rangeStorage

    if (alloc == NULL) alloc = CFAllocatorGetDefault();

    while ((rangeToSearch.length > 0) && CFStringFindWithOptions(string, stringToFind, rangeToSearch, compareOptions, &foundRange)) {
	// Determine the next range
        rangeToSearch.location = foundRange.location + foundRange.length;
        rangeToSearch.length = endIndex - rangeToSearch.location;

	// If necessary, grow the data and squirrel away the found range 
	if (foundCount >= capacity) {
	    if (rangeStorage == NULL) rangeStorage = CFDataCreateMutable(alloc, 0);
	    capacity = (capacity + 4) * 2;
	    CFDataSetLength(rangeStorage, capacity * (sizeof(CFRange) + sizeof(CFDataRef)));
	    rangeStorageBytes = (UInt8 *)CFDataGetMutableBytePtr(rangeStorage) + foundCount * (sizeof(CFRange) + sizeof(CFDataRef));
	}
	CFMoveMemory(rangeStorageBytes, &foundRange, sizeof(CFRange));	// The range
	CFMoveMemory(rangeStorageBytes + sizeof(CFRange), &rangeStorage, sizeof(CFDataRef));	// The data
	rangeStorageBytes += (sizeof(CFRange) + sizeof(CFDataRef));
	foundCount++;
    }

    if (foundCount > 0) {
	CFIndex cnt;
	CFMutableArrayRef array;
        const CFArrayCallBacks callbacks = {0, __rangeRetain, __rangeRelease, __rangeCopyDescription, __rangeEqual};

	CFDataSetLength(rangeStorage, foundCount * (sizeof(CFRange) + sizeof(CFDataRef)));	// Tighten storage up
	rangeStorageBytes = (UInt8 *)CFDataGetMutableBytePtr(rangeStorage);

        array = CFArrayCreateMutable(alloc, foundCount * sizeof(CFRange *), &callbacks);
	for (cnt = 0; cnt < foundCount; cnt++) {
	    // Each element points to the appropriate CFRange in the CFData
	    CFArrayAppendValue(array, rangeStorageBytes + cnt * (sizeof(CFRange) + sizeof(CFDataRef)));
	}
        CFRelease(rangeStorage);		// We want the data to go away when all CFRanges inside it are released...
        return array;
    } else {
        return NULL;
    }
}


CFRange CFStringFind(CFStringRef string, CFStringRef stringToFind, CFOptionFlags compareOptions) {
    CFRange foundRange;

    if (CFStringFindWithOptions(string, stringToFind, CFRangeMake(0, CFStringGetLength(string)), compareOptions, &foundRange)) {
        return foundRange;
    } else {
        return CFRangeMake(-1, 0);
    }
}

Boolean CFStringHasPrefix(CFStringRef string, CFStringRef prefix) {
    return CFStringFindWithOptions(string, prefix, CFRangeMake(0, CFStringGetLength(string)), kCFCompareAnchored, NULL);
}

Boolean CFStringHasSuffix(CFStringRef string, CFStringRef suffix) {
    return CFStringFindWithOptions(string, suffix, CFRangeMake(0, CFStringGetLength(string)), kCFCompareAnchored|kCFCompareBackwards, NULL);
}

/* Line range code */

#define CarriageReturn '\r'	/* 0x0d */
#define NewLine '\n'		/* 0x0a */
#define LineSeparator 0x2028
#define ParaSeparator 0x2029

CF_INLINE Boolean isALineSeparatorTypeCharacter(UniChar ch) {
    if (ch > CarriageReturn && ch < LineSeparator) return FALSE;	/* Quick test to cover most chars */
    return (ch == NewLine || ch == CarriageReturn || ch == LineSeparator || ch == ParaSeparator) ? TRUE : FALSE;
}

void CFStringGetLineBounds(CFStringRef string, CFRange range, CFIndex *lineBeginIndex, CFIndex *lineEndIndex, CFIndex *contentsEndIndex) {
    CFIndex len;
    CFStringBuffer buf;
    UniChar ch;

    CF_OBJC_FUNCDISPATCH4(String, void, string, "getLineStart:end:contentsEnd:forRange:", lineBeginIndex, lineEndIndex, contentsEndIndex, CFRangeMake(range.location, range.length));

    __CFAssertIsString(string);
    __CFAssertRangeIsInStringBounds(string, range.location, range.length);

    len = __CFStrLength(string);

    if (lineBeginIndex) {
        UInt32 start;
        if (range.location == 0) {
            start = 0;
        } else {
            CFStringBufferInit(&buf, string, range.location, len);
            /* Take care of the special case where start happens to fall right between \r and \n */
            ch = CFStringBufferCurrentCharacter(&buf);
            CFStringBufferDecrementLocation(&buf);
            if ((ch == NewLine) && (CFStringBufferCurrentCharacter(&buf) == CarriageReturn)) {
                CFStringBufferDecrementLocation(&buf);
            }
            while (1) {
                if (isALineSeparatorTypeCharacter(CFStringBufferCurrentCharacter(&buf))) {
                    start = CFStringBufferCurrentLocation(&buf) + 1;
                    break;
                } else if ((int)CFStringBufferCurrentLocation(&buf) <= 0) {
                    start = 0;
                    break;
                } else {
                    CFStringBufferDecrementLocation(&buf);
                }
            }
        }
        *lineBeginIndex = start;
    }

    /* Now find the ending point */
    if (lineEndIndex || contentsEndIndex) {
        UInt32 endOfContents, lineSeparatorLength = 1;	/* 1 by default */
        CFStringBufferInit(&buf, string, range.location + range.length - (range.length ? 1 : 0), len);
        /* First look at the last char in the range (if the range is zero length, the char after the range) to see if we're already on or within a end of line sequence... */
        ch = CFStringBufferCurrentCharacter(&buf);
        if (ch == NewLine) {
            endOfContents = CFStringBufferCurrentLocation(&buf);
            CFStringBufferDecrementLocation(&buf);
            if (CFStringBufferCurrentCharacter(&buf) == CarriageReturn) {
                lineSeparatorLength = 2;
                endOfContents--;
            }
        } else {
            while (1) {
                if (isALineSeparatorTypeCharacter(ch)) {
                    endOfContents = CFStringBufferCurrentLocation(&buf);	/* This is actually end of contentsRange */
                    CFStringBufferIncrementLocation(&buf);	/* OK for this to go past the end */
                    if ((ch == CarriageReturn) && (CFStringBufferCurrentCharacter(&buf) == NewLine)) {
                        lineSeparatorLength = 2;
                    }
                    break;
                } else if (CFStringBufferCurrentLocation(&buf) == len) {
                    endOfContents = len;
                    lineSeparatorLength = 0;
                    break;
                } else {
                    CFStringBufferIncrementLocation(&buf);
                    ch = CFStringBufferCurrentCharacter(&buf);
                }
            }
        }
        if (contentsEndIndex) *contentsEndIndex = endOfContents;
        if (lineEndIndex) *lineEndIndex = endOfContents + lineSeparatorLength;
    }
}


CFStringRef CFStringCreateByCombiningStrings(CFAllocatorRef alloc, CFArrayRef array, CFStringRef separatorString) {
    CFIndex numChars;
    CFIndex separatorNumByte;
    CFIndex stringCount = CFArrayGetCount(array);
    Boolean isSepCFString = !CF_IS_OBJC(String, separatorString); 
    Boolean canBeEightbit = isSepCFString && __CFStrIsEightBit(separatorString);
    UInt32 index;
    CFStringRef otherString;
    void *buffer;
    UInt8 *bufPtr;
    const void *separatorContents = NULL;

    if (stringCount == 0) {
        return CFStringCreateWithCharacters(alloc, NULL, 0);
    } else if (stringCount == 1) {
        return CFStringCreateCopy(alloc, CFArrayGetValueAtIndex(array, 0));
    }

    if (alloc == NULL) alloc = CFAllocatorGetDefault();

    numChars = CFStringGetLength(separatorString) * (stringCount - 1);
    for (index = 0; index < stringCount; index++) {
        otherString = (CFStringRef)CFArrayGetValueAtIndex(array, index);
        numChars += CFStringGetLength(otherString);
	// canBeEightbit is already false if the separator is an NSString...
        if (!CF_IS_OBJC(String, otherString) && __CFStrIsUnicode(otherString)) canBeEightbit = FALSE;
    }

    bufPtr = buffer = CFAllocatorAllocate(alloc, canBeEightbit ? ((numChars + 1) * sizeof(UInt8)) : (numChars * sizeof(UniChar)), 0);
    __CFSetLastAllocationEventName(buffer, "CFString (store)");
    separatorNumByte = CFStringGetLength(separatorString) * (canBeEightbit ? sizeof(UInt8) : sizeof(UniChar));

    for (index = 0; index < stringCount; index++) {
        if (index) { // add separator here unless first string
            if (separatorContents) {
                CFMoveMemory(bufPtr, separatorContents, separatorNumByte);
            } else {
                if (!isSepCFString) { // NSString
                    CFStringGetCharacters(separatorString, CFRangeMake(0, CFStringGetLength(separatorString)), (UniChar*)bufPtr);
                } else if (canBeEightbit || __CFStrIsUnicode(separatorString)) {
                    CFMoveMemory(bufPtr, (const UInt8 *)__CFStrContents(separatorString) + __CFStrSkipAnyLengthByte(separatorString), separatorNumByte);
                } else {	
                    __CFStrConvertBytesToUnicode((UInt8*)__CFStrContents(separatorString) + __CFStrSkipAnyLengthByte(separatorString), (UniChar*)bufPtr, __CFStrLength(separatorString));
                }
                separatorContents = bufPtr;
            }
            bufPtr += separatorNumByte;
        }

        otherString = (CFStringRef )CFArrayGetValueAtIndex(array, index);
        if (CF_IS_OBJC(String, otherString)) {
            CFIndex otherLength = CFStringGetLength(otherString);
            CFStringGetCharacters(otherString, CFRangeMake(0, otherLength), (UniChar*)bufPtr);
            bufPtr += otherLength * sizeof(UniChar);
        } else {
            const UInt8* otherContents = __CFStrContents(otherString);
            CFIndex otherNumByte = __CFStrLength2(otherString, otherContents) * (canBeEightbit ? sizeof(UInt8) : sizeof(UniChar));

            if (canBeEightbit || __CFStrIsUnicode(otherString)) {
                CFMoveMemory(bufPtr, otherContents + __CFStrSkipAnyLengthByte(otherString), otherNumByte);
            } else {
                __CFStrConvertBytesToUnicode(otherContents + __CFStrSkipAnyLengthByte(otherString), (UniChar*)bufPtr, __CFStrLength2(otherString, otherContents));
            }
            bufPtr += otherNumByte;
        }
    }
    if (canBeEightbit) *bufPtr = 0; // NULL byte;

    return canBeEightbit ? 
		CFStringCreateWithCStringNoCopy(alloc, buffer, __CFStringGetEightBitStringEncoding(), alloc) : 
		CFStringCreateWithCharactersNoCopy(alloc, buffer, numChars, alloc);
}


CFArrayRef CFStringCreateArrayBySeparatingStrings(CFAllocatorRef alloc, CFStringRef string, CFStringRef separatorString) {
    CFArrayRef separatorRanges;
    CFIndex length = CFStringGetLength(string);
    /* No objc dispatch needed here since CFStringCreateArrayWithFindResults() works with both CFString and NSString */
    if (!(separatorRanges = CFStringCreateArrayWithFindResults(alloc, string, separatorString, CFRangeMake(0, length), 0))) {
        return CFArrayCreate(alloc, (const void**)&string, 1, & kCFTypeArrayCallBacks);
    } else {
        UInt32 index;
        CFIndex count = CFArrayGetCount(separatorRanges);
        UInt32 startIndex = 0;
        CFIndex numChars;
        CFMutableArrayRef array = CFArrayCreateMutable(alloc, count + 2, & kCFTypeArrayCallBacks);
        const CFRange *currentRange;
        CFStringRef substring;

        for (index = 0;index < count;index++) {
            currentRange = CFArrayGetValueAtIndex(separatorRanges, index);
            numChars = currentRange->location - startIndex;
            substring = CFStringCreateWithSubstring(alloc, string, CFRangeMake(startIndex, numChars));
            CFArrayAppendValue(array, substring);
            CFRelease(substring);
            startIndex = currentRange->location + currentRange->length;
        }
        substring = CFStringCreateWithSubstring(alloc, string, CFRangeMake(startIndex, length - startIndex));
        CFArrayAppendValue(array, substring);
        CFRelease(substring);

	CFRelease(separatorRanges);
        
        return array;
    }
}

CFStringRef CFStringCreateFromExternalRepresentation(CFAllocatorRef alloc, CFDataRef data, CFStringEncoding encoding) {
    return CFStringCreateWithBytes(alloc, CFDataGetBytePtr(data), CFDataGetLength(data), encoding, TRUE);
}


CFDataRef CFStringCreateExternalRepresentation(CFAllocatorRef alloc, CFStringRef string, CFStringEncoding encoding, UInt8 lossByte) {
    CFIndex length;
    CFIndex guessedByteLength;
    UInt8 *bytes;
    CFIndex usedLength;
    SInt32 result;

    if (CF_IS_OBJC(String, string)) {	/* ??? Hope the compiler optimizes this away if OBJC_MAPPINGS is not on */
	length = CFStringGetLength(string);
    } else {
        __CFAssertIsString(string);
        length = __CFStrLength(string);
        if (__CFStrIsEightBit(string) && ((__CFStringGetEightBitStringEncoding() == encoding) || (__CFStringGetEightBitStringEncoding() == kCFStringEncodingASCII && __CFStringEncodingIsSupersetOfASCII(encoding)))) {	// Requested encoding is equal to the encoding in string
            return CFDataCreate(alloc, ((char *)__CFStrContents(string) + __CFStrSkipAnyLengthByte(string)), __CFStrLength(string));
        }
    }

    if (alloc == NULL) alloc = CFAllocatorGetDefault();

    if (encoding == kCFStringEncodingUnicode) {
        guessedByteLength = (length + 1) * sizeof(UniChar);
    } else if (((guessedByteLength = CFStringGetMaximumSizeForEncoding(length, encoding)) > length) && !CF_IS_OBJC(String, string)) { // Multi byte encoding
#if defined(__MACH__)
        if (__CFStrIsUnicode(string)) {
            guessedByteLength = CFStringEncodingByteLengthForCharacters(encoding, kCFStringEncodingPrependBOM, __CFStrContents(string), __CFStrLength(string));
        } else {
#endif
        result = __CFStringEncodeByteStream(string, 0, length, TRUE, encoding, lossByte, NULL, 0x7FFFFFFF, &guessedByteLength);
        if (!result || (!lossByte && result != length)) return NULL;
        if (guessedByteLength == length && __CFStrIsEightBit(string) && __CFStringEncodingIsSupersetOfASCII(encoding)) { // It's all ASCII !!
            return CFDataCreate(alloc, ((char *)__CFStrContents(string) + __CFStrSkipAnyLengthByte(string)), __CFStrLength(string));
        }
#if defined(__MACH__)
        }
#endif
    }
    bytes = CFAllocatorAllocate(alloc, guessedByteLength, 0);
    __CFSetLastAllocationEventName(bytes, "CFData (store)");

    result = __CFStringEncodeByteStream(string, 0, length, TRUE, encoding, lossByte, bytes, guessedByteLength, &usedLength);

    if (!result || (!lossByte && result != length)) {
        CFAllocatorDeallocate(alloc, bytes);
        return NULL;
    }

    return CFDataCreateWithBytesNoCopy(alloc, (char const *)bytes, usedLength, alloc);
}


CFStringEncoding CFStringGetSmallestEncoding(CFStringRef str) {
    CFIndex len;

    CF_OBJC_FUNCDISPATCH0(String, CFStringEncoding, str, "smallestEncoding");
    __CFAssertIsString(str);

    if (__CFStrIsEightBit(str)) return __CFStringGetEightBitStringEncoding();
    len = __CFStrLength(str);
    if (__CFStringEncodeByteStream(str, 0, len, FALSE, __CFStringGetEightBitStringEncoding(), 0, NULL, 0x7fffffff, NULL) == len) return __CFStringGetEightBitStringEncoding();
    if ((__CFStringGetEightBitStringEncoding() != __CFStringGetSystemEncoding()) && (__CFStringEncodeByteStream(str, 0, len, FALSE, __CFStringGetSystemEncoding(), 0, NULL, 0x7fffffff, NULL) == len)) return __CFStringGetSystemEncoding();
    return kCFStringEncodingUnicode;	/* ??? */
}


CFStringEncoding CFStringGetFastestEncoding(CFStringRef str) {
    CF_OBJC_FUNCDISPATCH0(String, CFStringEncoding, str, "fastestEncoding");
    __CFAssertIsString(str);
    return __CFStrIsEightBit(str) ? __CFStringGetEightBitStringEncoding() : kCFStringEncodingUnicode;	/* ??? */
}


SInt32 CFStringGetIntValue(CFStringRef str) {
    Boolean success;
    SInt32 result;
    SInt32 index = 0;
    CFStringInlineBuffer buf;
    CFStringInitInlineBuffer(str, &buf, CFRangeMake(0, CFStringGetLength(str)));
    success = __CFStringScanInteger(&buf, NULL, &index, FALSE, &result);
    return success ? result : 0;
}


double CFStringGetDoubleValue(CFStringRef str) {
    Boolean success;
    double result;
    SInt32 index = 0;
    CFStringInlineBuffer buf;
    CFStringInitInlineBuffer(str, &buf, CFRangeMake(0, CFStringGetLength(str)));
    success = __CFStringScanDouble(&buf, NULL, &index, &result);
    return success ? result : 0.0;
}


/*** Mutable functions... ***/

void CFStringSetExternalCharactersNoCopy(CFMutableStringRef string, UniChar *chars, CFIndex length, CFIndex capacity) {
    __CFAssertIsStringAndExternalMutable(string);
    CFAssert4((length <= capacity) && ((chars && capacity > 0) || (!chars && capacity == 0)), __kCFLogAssertion, "%s(): Invalid args: characters %p length %d capacity %d", __PRETTY_FUNCTION__, chars, length, capacity);
    __CFStrSetContentPtr(string, chars);
    __CFStrSetExplicitLength(string, length);
    __CFStrSetCapacity(string, capacity * sizeof(UniChar));
    __CFStrSetCapacityProvidedExternally(string);
}



void CFStringInsert(CFMutableStringRef str, CFIndex index, CFStringRef insertedStr) {
    CF_OBJC_FUNCDISPATCH2(String, void, str, "insertString:atIndex:", insertedStr, index);
    __CFAssertIsStringAndMutable(str);
    CFAssert3(index >= 0 && index <= __CFStrLength(str), __kCFLogAssertion, "%s(): string index %d out of bounds (length %d)", __PRETTY_FUNCTION__, index, __CFStrLength(str));
    __CFStringReplace(str, CFRangeMake(index, 0), insertedStr);
}


void CFStringDelete(CFMutableStringRef str, CFRange range) {
    CF_OBJC_FUNCDISPATCH1(String, void, str, "deleteCharactersInRange:", range);
    __CFAssertIsStringAndMutable(str);
    __CFAssertRangeIsInStringBounds(str, range.location, range.length);
    __CFStringChangeSize(str, range, 0, FALSE);
}


void CFStringReplace(CFMutableStringRef str, CFRange range, CFStringRef replacement) {
    CF_OBJC_FUNCDISPATCH2(String, void, str, "replaceCharactersInRange:withString:", range, replacement);
    __CFAssertIsStringAndMutable(str);
    __CFAssertRangeIsInStringBounds(str, range.location, range.length);
    __CFStringReplace(str, range, replacement);
}


void CFStringReplaceAll(CFMutableStringRef str, CFStringRef replacement) {
    CF_OBJC_FUNCDISPATCH1(String, void, str, "setString:", replacement);
    __CFAssertIsStringAndMutable(str);
    __CFStringReplace(str, CFRangeMake(0, __CFStrLength(str)), replacement);
}


void CFStringAppend(CFMutableStringRef str, CFStringRef appended) {
    CF_OBJC_FUNCDISPATCH1(String, void, str, "appendString:", appended);
    __CFAssertIsStringAndMutable(str);
    __CFStringReplace(str, CFRangeMake(__CFStrLength(str), 0), appended);
}


void CFStringAppendCharacters(CFMutableStringRef str, const UniChar *chars, CFIndex appendedLength) {
    CFIndex strLength;

    CF_OBJC_FUNCDISPATCH2(String, void, str, "appendCharacters:length:", chars, appendedLength);

    __CFAssertIsStringAndMutable(str);

    strLength = __CFStrLength(str);
    __CFStringChangeSize(str, CFRangeMake(strLength, 0), appendedLength, TRUE);
    CFMoveMemory(((UniChar *)__CFStrContents(str)) + strLength, chars, appendedLength * sizeof(UniChar));
}


// ??? Appending of bytes not in __CFStringGetEightBitStringEncoding() incorrect
// Use __CFCanUseEightBitCFStringForBytes(cStr, appendedLength, encoding)
// ??? Perhaps this should be public and more parallel to CFStringCreateWithBytes?

void __CFStringAppendBytes(CFMutableStringRef str, const char *cStr, CFIndex appendedLength, CFStringEncoding encoding) {
    CFIndex strLength;

    CF_OBJC_FUNCDISPATCH2(String, void, str, "appendCString:length:", cStr, appendedLength);

    __CFAssertIsStringAndMutable(str);
    
    strLength = __CFStrLength(str);

    __CFStringChangeSize(str, CFRangeMake(strLength, 0), appendedLength, !(__CFStrIsEightBit(str) && (encoding == kCFStringEncodingASCII || encoding == __CFStringGetEightBitStringEncoding())));

    if (__CFStrIsUnicode(str)) {
        UniChar *contents = (UniChar *)__CFStrContents(str);
        __CFStrConvertBytesToUnicode((const UInt8 *)cStr, contents + strLength, appendedLength);
    } else {
	UInt8 *contents = (UInt8 *)__CFStrContents(str);
	CFMoveMemory(contents + strLength + __CFStrSkipAnyLengthByte(str), cStr, appendedLength);
    }
}

void CFStringAppendPascalString(CFMutableStringRef str, ConstStringPtr pStr, CFStringEncoding encoding) {
    __CFStringAppendBytes(str, pStr + 1, (UInt32)*pStr, encoding);
}

void CFStringAppendCString(CFMutableStringRef str, const char *cStr, CFStringEncoding encoding) {
    __CFStringAppendBytes(str, cStr, strlen(cStr), encoding);
}


void CFStringAppendFormat(CFMutableStringRef str, CFDictionaryRef formatOptions, CFStringRef format, ...) {
    va_list argList;

    va_start(argList, format);
    CFStringAppendFormatAndArguments(str, formatOptions, format, argList);
    va_end(argList);
}


void CFStringPad(CFMutableStringRef string, CFStringRef padString, CFIndex length, CFIndex indexIntoPad) {
    CFIndex originalLength;

    CF_OBJC_FUNCDISPATCH3(String, void, string, "padWithCFString:length:padIndex:", padString, length, indexIntoPad);

    __CFAssertIsStringAndMutable(string);
    __CFAssertIsString(padString);

    originalLength = __CFStrLength(string);
    if (length < originalLength) {
        __CFStringChangeSize(string, CFRangeMake(length, originalLength - length), 0, FALSE);
    } else if (originalLength < length) {
        Boolean isUnicode = __CFStrIsUnicode(string) || __CFStrIsUnicode(padString);
        UInt8 *contents;
        CFIndex charSize = isUnicode ? sizeof(UniChar) : sizeof(UInt8);
        CFIndex padLength;
        CFIndex padRemaining = length - originalLength;

        __CFStringChangeSize(string, CFRangeMake(originalLength, 0), padRemaining, isUnicode);

        contents = (UInt8*)__CFStrContents(string) + charSize * originalLength + __CFStrSkipAnyLengthByte(string);
        padLength = __CFStrLength(padString) - indexIntoPad;
        padLength = padRemaining < padLength ? padRemaining : padLength;

        while (padRemaining > 0) {
            if (isUnicode) {
                CFStringGetCharacters(padString, CFRangeMake(indexIntoPad, padLength), (UniChar*)contents);
            } else {
                CFStringGetBytes(padString, CFRangeMake(indexIntoPad, padLength), __CFStringGetEightBitStringEncoding(), 0, FALSE, contents, padRemaining * charSize, NULL);
            }
            contents += padLength * charSize;
            padRemaining -= padLength;
            indexIntoPad = 0;
            padLength = padRemaining < padLength ? padRemaining : __CFStrLength(padString);
        }
    }
}

void CFStringTrim(CFMutableStringRef string, CFStringRef trimString) {
    CFRange range;
    UInt32 newStartIndex;
    CFIndex length;

    CF_OBJC_FUNCDISPATCH1(String, void, string, "trimWithCharactersInCFString:", trimString);

    __CFAssertIsStringAndMutable(string);
    __CFAssertIsString(trimString);

    newStartIndex = 0;
    length = __CFStrLength(string);

    while (CFStringFindWithOptions(string, trimString, CFRangeMake(newStartIndex, length - newStartIndex), kCFCompareAnchored, &range)) {
        newStartIndex = range.location + range.length;
    }

    if (newStartIndex < length) {
        CFIndex charSize = __CFStrIsUnicode(string) ? sizeof(UniChar) : sizeof(UInt8);
        UInt8 *contents = (UInt8*)__CFStrContents(string) + __CFStrSkipAnyLengthByte(string);

        length -= newStartIndex;
        if (__CFStrLength(trimString) < length) {
            while (CFStringFindWithOptions(string, trimString, CFRangeMake(newStartIndex, length), kCFCompareAnchored|kCFCompareBackwards, &range)) {
                length = range.location - newStartIndex;
            }
        }
        CFMoveMemory(contents, contents + newStartIndex * charSize, length * charSize);
        __CFStringChangeSize(string, CFRangeMake(length, __CFStrLength(string) - length), 0, FALSE);
    } else { // Only trimString in string, trim all
        __CFStringChangeSize(string, CFRangeMake(0, length), 0, FALSE);
    }
}

void CFStringTrimWhitespace(CFMutableStringRef string) {
    UInt32 newStartIndex;
    CFIndex length;
    CFStringBuffer buffer;

    CF_OBJC_FUNCDISPATCH0(String, void, string, "trimWhitespace");

    __CFAssertIsStringAndMutable(string);

    newStartIndex = 0;
    length = __CFStrLength(string);

    CFStringBufferInit(&buffer, string, 0, length);

    while (CFUniCharIsMemberOf(CFStringBufferCurrentCharacter(&buffer), kCFUniCharWhitespaceAndNewlineCharacterSet))
        CFStringBufferIncrementLocation(&buffer);
    newStartIndex = CFStringBufferCurrentLocation(&buffer);

    if (newStartIndex < length) {
        UInt8 *contents = (UInt8*)__CFStrContents(string) + __CFStrSkipAnyLengthByte(string);
        CFIndex charSize = (__CFStrIsUnicode(string) ? sizeof(UniChar) : sizeof(UInt8));

        CFStringBufferSetLocation(&buffer, length - 1);
        while (CFUniCharIsMemberOf(CFStringBufferCurrentCharacter(&buffer), kCFUniCharWhitespaceAndNewlineCharacterSet))
            CFStringBufferDecrementLocation(&buffer);
        length = CFStringBufferCurrentLocation(&buffer) - newStartIndex + 1;

        CFMoveMemory(contents, contents + newStartIndex * charSize, length * charSize);
        __CFStringChangeSize(string, CFRangeMake(length, __CFStrLength(string) - length), 0, FALSE);
    } else { // Whitespace only string
        __CFStringChangeSize(string, CFRangeMake(0, length), 0, FALSE);
    }
}

void CFStringLowercase(CFMutableStringRef string, const void *locale) {
    UInt32 currentIndex = 0;
    CFIndex length;

    CF_OBJC_FUNCDISPATCH1(String, void, string, "lowercaseSelfWithLocale:", locale);

    __CFAssertIsStringAndMutable(string);

    length = __CFStrLength(string);

    if (__CFStrIsEightBit(string)) {
        UInt8 *contents = (UInt8*)__CFStrContents(string) + __CFStrSkipAnyLengthByte(string);
        for (;currentIndex < length;currentIndex++) {
            if (contents[currentIndex] >= 'A' && contents[currentIndex] <= 'Z') {
                contents[currentIndex] += 'a' - 'A';
            } else if (contents[currentIndex] > 127) {
                __CFStringChangeSize(string, CFRangeMake(0, 0), 0, TRUE); // need to do harm way
                break;
            }
        }
    }

    if (currentIndex < length) {
        UniChar *contents = (UniChar*)__CFStrContents(string);

        for (;currentIndex < length;currentIndex++) {
            CFUniCharMapTo(contents[currentIndex], &(contents[currentIndex]), length, kCFUniCharToLowercase, 0); // Check locale for Turkish Mapping
        }
    }
}

void CFStringUppercase(CFMutableStringRef string, const void *locale) {
    UInt32 currentIndex = 0;
    CFIndex length;

    CF_OBJC_FUNCDISPATCH1(String, void, string, "uppercaseSelfWithLocale:", locale);

    __CFAssertIsStringAndMutable(string);

    length = __CFStrLength(string);

    if (__CFStrIsEightBit(string)) {
        UInt8 *contents = (UInt8*)__CFStrContents(string) + __CFStrSkipAnyLengthByte(string);
        for (;currentIndex < length;currentIndex++) {
            if (contents[currentIndex] >= 'a' && contents[currentIndex] <= 'z') {
                contents[currentIndex] -= 'a' - 'A';
            } else if (contents[currentIndex] > 127) {
                __CFStringChangeSize(string, CFRangeMake(0, 0), 0, TRUE); // need to do harm way
                break;
            }
        }
    }

    if (currentIndex < length) {
        UniChar *contents = (UniChar*)__CFStrContents(string);
        for (;currentIndex < length;currentIndex++) {
            CFUniCharMapTo(contents[currentIndex], &(contents[currentIndex]), length, kCFUniCharToUppercase, 0); // Check locale for Turkish Mapping
        }
    }
}


#define isapostrophe(X) (X == 0x0027 || X == 0x02BC)

void CFStringCapitalize(CFMutableStringRef string, const void *locale) {
    UInt32 currentIndex = 0;
    CFIndex length;
    Boolean isLastAlpha = FALSE;

    CF_OBJC_FUNCDISPATCH1(String, void, string, "capitalizeSelfWithLocale:", locale);

    __CFAssertIsStringAndMutable(string);

    length = __CFStrLength(string);

    if (__CFStrIsEightBit(string)) {
        UInt8 *contents = (UInt8*)__CFStrContents(string) + __CFStrSkipAnyLengthByte(string);
        for (;currentIndex < length;currentIndex++) {
            if (contents[currentIndex] > 127) {
                __CFStringChangeSize(string, CFRangeMake(0, 0), 0, TRUE); // need to do harm way
                break;
            }
            if (contents[currentIndex] >= 'A' && contents[currentIndex] <= 'Z') {
                contents[currentIndex] += (isLastAlpha ? 'a' - 'A' : 0);
                isLastAlpha = TRUE;
            } else if (contents[currentIndex] >= 'a' && contents[currentIndex] <= 'z') {
                contents[currentIndex] -= (!isLastAlpha ? 'a' - 'A' : 0);
                isLastAlpha = TRUE;
            } else if (isapostrophe(contents[currentIndex])) {
                isLastAlpha = TRUE;
            } else {
                isLastAlpha = FALSE;
            }
        }
    }

    if (currentIndex < length) {
        UniChar *contents = (UniChar*)__CFStrContents(string);
        for (;currentIndex < length;currentIndex++) {
            CFUniCharMapTo(contents[currentIndex], &(contents[currentIndex]), length, (isLastAlpha ? kCFUniCharToLowercase : kCFUniCharToTitlecase), 0); // ??? Check locale for Turkish Mapping
            isLastAlpha = CFUniCharIsMemberOf(contents[currentIndex], kCFUniCharLetterCharacterSet) || isapostrophe(contents[currentIndex]);
        }
    }
}

#define POSIX_SEPARATOR "/"

CF_INLINE void __CFStringReplacePathSeparator(CFMutableStringRef string, const char from, const char to) {
    UInt8 *contents = (UInt8*)__CFStrContents(string) + __CFStrSkipAnyLengthByte(string);
    CFIndex length = __CFStrLength2(string, contents);
    UInt32 isUnicode = __CFStrIsUnicode(string);
    UInt32 index;

    for (index = 0;index < length;index++) {
        if ((isUnicode ? ((UniChar*)contents)[index] : ((UInt8*)contents)[index]) == from)
		    if (isUnicode) ((UniChar*)contents)[index] = to;
			else ((UInt8*)contents)[index] = to;
    }
}


typedef struct {
    SInt16 size;
    SInt16 type;
    SInt32 loc;
    SInt32 len;
    SInt8 mainArgNum;
    SInt8 precArgNum;
    SInt8 widthArgNum;
    SInt8 unused1;
} CFFormatSpec;

typedef struct {
    SInt16 type;
    SInt16 size;
    union {
	SInt64 longlongValue;
	double doubleValue;
	void *pointerValue;
    } value;
} CFPrintValue;

enum {
    CFFormatDefaultSize = 0,
    CFFormatSize1 = 1,
    CFFormatSize2 = 2,
    CFFormatSize4 = 3,
    CFFormatSize8 = 4,
    CFFormatSize16 = 5,		/* unused */
};

enum {
    CFFormatLiteralType = 32,
    CFFormatLongType = 33,
    CFFormatDoubleType = 34,
    CFFormatPointerType = 35,
    CFFormatObjectType = 36,		/* handled specially */	/* ??? not used anymore, can be removed? */
    CFFormatCFType = 37,		/* handled specially */
    CFFormatUnicharsType = 38,		/* handled specially */
    CFFormatCharsType = 39,		/* handled specially */
    CFFormatPascalCharsType = 40,	/* handled specially */
    CFFormatSingleUnicharType = 41	/* handled specially */
};

CF_INLINE void __CFParseFormatSpec(const UniChar *uformat, const UInt8 *cformat, SInt32 *fmtIdx, SInt32 fmtLen, CFFormatSpec *spec) {
    for (;;) {
	UniChar ch;
	if (fmtLen <= *fmtIdx) return;	/* no type */
        if (cformat) ch = (UniChar)cformat[(*fmtIdx)++]; else ch = uformat[(*fmtIdx)++];
reswtch:switch (ch) {
	case 0x20: case '#': case '-': case '+': case '0':
	    break;
	case 'h':
	    spec->size = CFFormatSize2;
	    break;
	case 'l':
	    spec->size = CFFormatSize4;
	    break;
	case 'q':
	    spec->size = CFFormatSize8;
	    break;
	case 'c':
	    spec->type = CFFormatLongType;
	    spec->size = CFFormatSize1;
	    return;
	case 'O': case 'o': case 'D': case 'd': case 'i': case 'U': case 'u': case 'x': case 'X':
	    spec->type = CFFormatLongType;
	    return;
	case 'e': case 'E': case 'f': case 'g': case 'G':
	    spec->type = CFFormatDoubleType;
	    spec->size = CFFormatSize8;
	    return;
	case 'n': case 'p':		/* %n is not handled correctly currently */
	    spec->type = CFFormatPointerType;
	    spec->size = CFFormatSize4;
	    return;
	case 's':
	    spec->type = CFFormatCharsType;
	    spec->size = CFFormatSize4;
	    return;
	case 'S':
	    spec->type = CFFormatUnicharsType;
	    spec->size = CFFormatSize4;
	    return;
        case 'C':
            spec->type = CFFormatSingleUnicharType;
            spec->size = CFFormatSize2;
            return;
	case 'P':
	    spec->type = CFFormatPascalCharsType;
	    spec->size = CFFormatSize4;
	    return;
	case '@':
	    spec->type = CFFormatCFType;
	    spec->size = CFFormatSize4;
	    return;
	case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9': {
	    long long number = 0;
	    do {
		number = 10 * number + (ch - '0');
                if (cformat) ch = (UniChar)cformat[(*fmtIdx)++]; else ch = uformat[(*fmtIdx)++];
	    } while ((UInt32)(ch - '0') <= 9);
	    if ('$' == ch) {
		if (-2 == spec->precArgNum) {
		    spec->precArgNum = number - 1;	// Arg numbers start from 1
		} else if (-2 == spec->widthArgNum) {
		    spec->widthArgNum = number - 1;	// Arg numbers start from 1
		} else {
		    spec->mainArgNum = number - 1;	// Arg numbers start from 1
		}
		break;
	    }	/* else it's either precision or width, toss */
	    goto reswtch;
	}
	case '*':
	    spec->widthArgNum = -2;
	    break;
	case '.':
            if (cformat) ch = (UniChar)cformat[(*fmtIdx)++]; else ch = uformat[(*fmtIdx)++];
	    if ('*' == ch) {
		spec->precArgNum = -2;
		break;
	    }
	    goto reswtch;
	default:
	    spec->type = CFFormatLiteralType;
	    return;
	}
    }
}

#if defined(__WIN32__) || defined(__MACOS8__)
static int snprintf (char *b, size_t n, const char * f, ...) {
    int retval;
    va_list args;
    va_start (args, f);
    retval = vsprintf(b, f, args);
    va_end(args);
    return retval;
}
#endif

/* ??? It ignores the formatOptions argument.
   ??? %s depends on handling of encodings by __CFStringAppendBytes
*/
void CFStringAppendFormatAndArguments(CFMutableStringRef outputString, CFDictionaryRef formatOptions, CFStringRef formatString, va_list args) {
    _CFStringAppendFormatAndArgumentsAux(outputString, NULL, formatOptions, formatString, args);
}

#define SNPRINTF(TYPE, WHAT) {				\
    TYPE value = (TYPE) WHAT;				\
    if (hasWidth) {					\
	if (hasPrecision) {				\
	    snprintf(buffer, 255, formatBuffer, width, precision, value); \
	} else {					\
	    snprintf(buffer, 255, formatBuffer, width, value); \
	}						\
    } else {						\
	if (hasPrecision) {				\
	    snprintf(buffer, 255, formatBuffer, precision, value); \
        } else {					\
	    snprintf(buffer, 255, formatBuffer, value);	\
	}						\
    }}

void _CFStringAppendFormatAndArgumentsAux(CFMutableStringRef outputString, CFStringRef (*copyDescFunc)(void *, CFDictionaryRef), CFDictionaryRef formatOptions, CFStringRef formatString, va_list args) {
    SInt32 numSpecs, sizeSpecs, sizeArgNum, formatIdx, curSpec, argNum;
    CFIndex formatLen;
#define FORMAT_BUFFER_LEN 400
    const UInt8 *cformat = NULL;
    const UniChar *uformat = NULL;
    UniChar *formatChars = NULL;
    UniChar localFormatBuffer[FORMAT_BUFFER_LEN];

    #define VPRINTF_BUFFER_LEN 20
    CFFormatSpec localSpecsBuffer[VPRINTF_BUFFER_LEN];
    CFFormatSpec *specs;
    CFPrintValue localValuesBuffer[VPRINTF_BUFFER_LEN];
    CFPrintValue *values;
    CFAllocatorRef tmpAlloc = NULL;

    numSpecs = 0;
    sizeSpecs = 0;
    sizeArgNum = 0;
    specs = NULL;
    values = NULL;
    
    formatLen = CFStringGetLength(formatString);
    if (!CF_IS_OBJC(String, formatString)) {
        __CFAssertIsString(formatString);
        if (!__CFStrIsUnicode(formatString)) {
            cformat = __CFStrContents(formatString);
            if (cformat) cformat += __CFStrSkipAnyLengthByte(formatString);
        } else {
            uformat = __CFStrContents(formatString);
        }
    }
    if (!cformat && !uformat) {
        formatChars = (formatLen > FORMAT_BUFFER_LEN) ? CFAllocatorAllocate(tmpAlloc = CFAllocatorGetDefault(), formatLen * sizeof(UniChar), 0) : localFormatBuffer; 
	if (formatChars != localFormatBuffer) __CFSetLastAllocationEventName(formatChars, "CFString (temp)");
        CFStringGetCharacters(formatString, CFRangeMake(0, formatLen), formatChars);
        uformat = formatChars;
    }

    /* Compute an upper bound for the number of format specifications */
    if (cformat) {
        for (formatIdx = 0; formatIdx < formatLen; formatIdx++) if ('%' == cformat[formatIdx]) sizeSpecs++;
    } else {
        for (formatIdx = 0; formatIdx < formatLen; formatIdx++) if ('%' == uformat[formatIdx]) sizeSpecs++;
    } 
    sizeSpecs = 2 * sizeSpecs + 1;
    specs = (sizeSpecs > VPRINTF_BUFFER_LEN) ? CFAllocatorAllocate(tmpAlloc = CFAllocatorGetDefault(), sizeSpecs * sizeof(CFFormatSpec), 0) : localSpecsBuffer;
    if (specs != localSpecsBuffer) __CFSetLastAllocationEventName(specs, "CFString (temp)");

    /* Collect format specification information from the format string */
    for (curSpec = 0, formatIdx = 0; formatIdx < formatLen; curSpec++) {
	SInt32 newFmtIdx;
	specs[curSpec].loc = formatIdx;
	specs[curSpec].len = 0;
	specs[curSpec].size = 0;
	specs[curSpec].type = 0;
	specs[curSpec].mainArgNum = -1;
	specs[curSpec].precArgNum = -1;
	specs[curSpec].widthArgNum = -1;
        if (cformat) {
            for (newFmtIdx = formatIdx; newFmtIdx < formatLen && '%' != cformat[newFmtIdx]; newFmtIdx++);
        } else {
            for (newFmtIdx = formatIdx; newFmtIdx < formatLen && '%' != uformat[newFmtIdx]; newFmtIdx++);
        }
	if (newFmtIdx != formatIdx) {	/* Literal chunk */
	    specs[curSpec].type = CFFormatLiteralType;
	    specs[curSpec].len = newFmtIdx - formatIdx;
	} else {
	    newFmtIdx++;	/* Skip % */
	    __CFParseFormatSpec(uformat, cformat, &newFmtIdx, formatLen, &(specs[curSpec]));
            if (CFFormatLiteralType == specs[curSpec].type) {
		specs[curSpec].loc = formatIdx + 1;
		specs[curSpec].len = 1;
	    } else {
		specs[curSpec].len = newFmtIdx - formatIdx;
	    }
	}
	formatIdx = newFmtIdx;

// printf("specs[%d] = {\n  size = %d,\n  type = %d,\n  loc = %d,\n  len = %d,\n  mainArgNum = %d,\n  precArgNum = %d,\n  widthArgNum = %d\n}\n", curSpec, specs[curSpec].size, specs[curSpec].type, specs[curSpec].loc, specs[curSpec].len, specs[curSpec].mainArgNum, specs[curSpec].precArgNum, specs[curSpec].widthArgNum);

    }
    numSpecs = curSpec;
    values = localValuesBuffer;
    sizeArgNum = VPRINTF_BUFFER_LEN;

    /* Compute values array */
    argNum = 0;
    for (curSpec = 0; curSpec < numSpecs; curSpec++) {
	SInt32 newMaxArgNum;
	if (0 == specs[curSpec].type) continue;
	if (CFFormatLiteralType == specs[curSpec].type) continue;
	newMaxArgNum = sizeArgNum;
	if (newMaxArgNum < specs[curSpec].mainArgNum) {
	    newMaxArgNum = specs[curSpec].mainArgNum;
	}
	if (newMaxArgNum < specs[curSpec].precArgNum) {
	    newMaxArgNum = specs[curSpec].precArgNum;
	}
	if (newMaxArgNum < specs[curSpec].widthArgNum) {
	    newMaxArgNum = specs[curSpec].widthArgNum;
	}
	if (sizeArgNum < newMaxArgNum) {	// Need to grow the buffer...
	    if (tmpAlloc == NULL) tmpAlloc = CFAllocatorGetDefault();
	    if (values == localValuesBuffer) {
		values = CFAllocatorAllocate(tmpAlloc, newMaxArgNum * sizeof(CFPrintValue), 0);
		__CFSetLastAllocationEventName(values, "CFString (temp)");
		CFMoveMemory(values, localValuesBuffer, sizeArgNum * sizeof(CFPrintValue));		
	    } else {
		values = CFAllocatorReallocate(tmpAlloc, values, newMaxArgNum * sizeof(CFPrintValue), 0);
	    }
	    sizeArgNum = newMaxArgNum;
	}
	/* It is actually incorrect to reorder some specs and not all; we just do some random garbage here */
	if (-2 == specs[curSpec].widthArgNum) {
	    specs[curSpec].widthArgNum = argNum++;
	}
	if (-2 == specs[curSpec].precArgNum) {
	    specs[curSpec].precArgNum = argNum++;
	}
	if (-1 == specs[curSpec].mainArgNum) {
	    specs[curSpec].mainArgNum = argNum++;
	}
	values[specs[curSpec].mainArgNum].size = specs[curSpec].size;
	values[specs[curSpec].mainArgNum].type = specs[curSpec].type;
	if (-1 != specs[curSpec].widthArgNum) {
	    values[specs[curSpec].widthArgNum].size = 0;
	    values[specs[curSpec].widthArgNum].type = CFFormatLongType;
	}
	if (-1 != specs[curSpec].precArgNum) {
	    values[specs[curSpec].precArgNum].size = 0;
	    values[specs[curSpec].precArgNum].type = CFFormatLongType;
	}
    }

    /* Collect the arguments in correct type from vararg list */
    for (argNum = 0; argNum < sizeArgNum; argNum++) {
	switch (values[argNum].type) {
	case CFFormatLiteralType:
	    break;
	case CFFormatLongType:
        case CFFormatSingleUnicharType:
	    if (CFFormatSize1 == values[argNum].size) {
		values[argNum].value.longlongValue = (SInt64)(char)va_arg(args, int);
	    } else if (CFFormatSize2 == values[argNum].size) {
		values[argNum].value.longlongValue = (SInt64)(short)va_arg(args, int);
	    } else if (CFFormatSize4 == values[argNum].size) {
		values[argNum].value.longlongValue = (SInt64)va_arg(args, long);
	    } else if (CFFormatSize8 == values[argNum].size) {
		values[argNum].value.longlongValue = (SInt64)va_arg(args, long long);
	    } else {
		values[argNum].value.longlongValue = (SInt64)va_arg(args, int);
	    }
	    break;
	case CFFormatDoubleType:
	    values[argNum].value.doubleValue = va_arg(args, double);
	    break;
	case CFFormatPointerType:
	case CFFormatObjectType:
	case CFFormatCFType:
	case CFFormatUnicharsType:
	case CFFormatCharsType:
	case CFFormatPascalCharsType:
	    values[argNum].value.pointerValue = va_arg(args, void *);
	    break;
	}
    }
    va_end(args);

    /* Format the pieces together */
    for (curSpec = 0; curSpec < numSpecs; curSpec++) {
	SInt32 width = 0, precision = 0, size = 0;
	UniChar *up, ch;
	Boolean hasWidth = FALSE, hasPrecision = FALSE;

	if (-1 != specs[curSpec].widthArgNum) {
	    width = (SInt32)values[specs[curSpec].widthArgNum].value.longlongValue;
	    hasWidth = TRUE;
	}
	if (-1 != specs[curSpec].precArgNum) {
	    precision = (SInt32)values[specs[curSpec].precArgNum].value.longlongValue;
	    hasPrecision = TRUE;
	}

	switch (specs[curSpec].type) {
	case CFFormatLongType:
	case CFFormatDoubleType:
	case CFFormatPointerType: {
                SInt8 formatBuffer[128];
                SInt8 buffer[256];
                SInt32 cidx, idx, loc;
		Boolean appended = FALSE;
                loc = specs[curSpec].loc;
                // In preparation to call snprintf(), copy the format string out
                if (cformat) {
                    for (idx = 0, cidx = 0; cidx < specs[curSpec].len; idx++, cidx++) {
                        if ('$' == cformat[loc + cidx]) {
                            for (idx--; '0' <= formatBuffer[idx] && formatBuffer[idx] <= '9'; idx--);
                        } else {
                            formatBuffer[idx] = cformat[loc + cidx];
                        }
                    }
                } else {
                    for (idx = 0, cidx = 0; cidx < specs[curSpec].len; idx++, cidx++) {
                        if ('$' == uformat[loc + cidx]) {
                            for (idx--; '0' <= formatBuffer[idx] && formatBuffer[idx] <= '9'; idx--);
                        } else {
                            formatBuffer[idx] = (SInt8)uformat[loc + cidx];
                        }
                    }
                }
                formatBuffer[idx] = '\0';
                buffer[255] = '\0';
                switch (specs[curSpec].type) {
                    case CFFormatLongType:
                        if (CFFormatSize8 == specs[curSpec].size) {
                            SNPRINTF(SInt64, values[specs[curSpec].mainArgNum].value.longlongValue)
                        } else {
                            SNPRINTF(SInt32, values[specs[curSpec].mainArgNum].value.longlongValue)
                        }
                        break;
                    case CFFormatPointerType:
                        SNPRINTF(void *, values[specs[curSpec].mainArgNum].value.pointerValue)
                        break;

                    case CFFormatDoubleType:
                        SNPRINTF(double, values[specs[curSpec].mainArgNum].value.doubleValue)
			// See if we need to localize the decimal point
                        if (formatOptions) {	// We have a localization dictionary
                            CFStringRef decimalSeparator = CFDictionaryGetValue(formatOptions, kCFNSDecimalSeparatorKey);
                            if (decimalSeparator != NULL) {	// We have a decimal separator in there
                                CFIndex decimalPointLoc = 0;
                                while (buffer[decimalPointLoc] != 0 && buffer[decimalPointLoc] != '.') decimalPointLoc++;
                                if (buffer[decimalPointLoc] == '.') {	// And we have a decimal point in the formatted string
                                    buffer[decimalPointLoc] = 0;
                                    CFStringAppendCString(outputString, buffer, __CFStringGetEightBitStringEncoding());
                                    CFStringAppend(outputString, decimalSeparator);
                                    CFStringAppendCString(outputString, buffer + decimalPointLoc + 1, __CFStringGetEightBitStringEncoding());
                                    appended = TRUE;
                                }
                            }
                        }
                        break;
                }
                if (!appended) CFStringAppendCString(outputString, buffer, __CFStringGetEightBitStringEncoding());
            }
	    break;
	case CFFormatLiteralType:
            if (cformat) {
                __CFStringAppendBytes(outputString, cformat+specs[curSpec].loc, specs[curSpec].len, __CFStringGetEightBitStringEncoding());
            } else {
                CFStringAppendCharacters(outputString, uformat+specs[curSpec].loc, specs[curSpec].len);
            }
	    break;
	case CFFormatPascalCharsType:
        case CFFormatCharsType:
	    if (values[specs[curSpec].mainArgNum].value.pointerValue == NULL) {
		CFStringAppendCString(outputString, "(null)", kCFStringEncodingASCII);
	    } else if (specs[curSpec].type == CFFormatCharsType) {
		CFStringAppendCString(outputString, values[specs[curSpec].mainArgNum].value.pointerValue, __CFStringGetSystemEncoding());
	    } else {
		CFStringAppendPascalString(outputString, values[specs[curSpec].mainArgNum].value.pointerValue, __CFStringGetSystemEncoding());
	    }
            break;
        case CFFormatSingleUnicharType:
            ch = values[specs[curSpec].mainArgNum].value.longlongValue;
            CFStringAppendCharacters(outputString, &ch, 1);
            break;
        case CFFormatUnicharsType:
            //??? need to handle width, precision, and padding arguments
            up = values[specs[curSpec].mainArgNum].value.pointerValue;
            if (up) {
                for (; 0 != up[size]; size++);
                CFStringAppendCharacters(outputString, up, size);
            } else {
                CFStringAppendCString(outputString, "(null)", kCFStringEncodingASCII);
            }
            break;
	case CFFormatCFType:
	case CFFormatObjectType:
            if (values[specs[curSpec].mainArgNum].value.pointerValue) {
                CFStringRef str;
		if (copyDescFunc) {
		    str = copyDescFunc(values[specs[curSpec].mainArgNum].value.pointerValue, formatOptions);
		} else {
		    str = CFCopyDescription(values[specs[curSpec].mainArgNum].value.pointerValue);
		}
                if (str) {
                    CFStringAppend(outputString, str);
                    CFRelease(str);
                } else {
                    CFStringAppendCString(outputString, "(null description)", kCFStringEncodingASCII);
                }
            } else {
		CFStringAppendCString(outputString, "(null)", kCFStringEncodingASCII);
            }
            break;
        }
    }

    if (specs != localSpecsBuffer) CFAllocatorDeallocate(tmpAlloc, specs);
    if (values != localValuesBuffer) CFAllocatorDeallocate(tmpAlloc, values);
    if (formatChars && (formatChars != localFormatBuffer)) CFAllocatorDeallocate(tmpAlloc, formatChars);

}

#undef SNPRINTF


#if 1

/* temporary!!!! */
// ??? Bunch of CF functions defined here temporarily

void CFShow(const void *obj) { // ??? supposed to use stderr for Logging ?
     CFStringRef str = (obj ? CFCopyDescription(obj) : CFRetain(CFSTR("(null)")));
     CFIndex len = CFStringGetLength(str);
     CFStringBuffer buffer;

     CFStringBufferInit(&buffer, str, 0, len);
     while (!CFStringBufferIsAtEnd(&buffer)) {
         UniChar ch = CFStringBufferCurrentCharacter(&buffer);
         if (ch < 128) {
             printf ("%c", ch);
         } else {
             printf ("\\U%04x", ch);
         }
         CFStringBufferIncrementLocation(&buffer);
     }
     printf("\n");

     if (str) CFRelease(str);
}

void CFShowStr(CFStringRef str) {
    CFAllocatorRef alloc;

    if (!str) {
	printf ("(null)\n");
	return;
    }

    if (CF_IS_OBJC(String, str)) {
        printf ("This is an NSString, not CFString\n");
        return;
    }

    alloc = __CFStrHasCustomAllocator(str) ? __CFStrCustomAllocator(str) : NULL;

    printf ("\nLength %d\nIsEightBit %d\n", (int)__CFStrLength(str), __CFStrIsEightBit(str));
    printf ("HasLengthByte %d\nHasNullByte %d\nInlineContents %d\n",
            __CFStrHasLengthByte(str), __CFStrHasNullByte(str), __CFStrIsInline(str));

    printf ("Allocator ");
    if (alloc) {
        printf ("%p\n", (void *)alloc);
    } else {
        printf ("SystemDefault\n");
    }
    printf ("Mutable %d\n", __CFStrIsMutable(str));
    if (!__CFStrIsMutable(str) && __CFStrHasContentsDeallocator(str)) {
        if (__CFStrContentsDeallocator(str)) printf ("ContentsDeallocatorFunc %p\n", (void *)__CFStrContentsDeallocator(str));
        else printf ("ContentsDeallocatorFunc None\n");
    } else if (__CFStrIsMutable(str) && __CFStrHasContentsAllocator(str)) {
        printf ("ExternalContentsAllocator %p\n", (void *)__CFStrContentsAllocator((CFMutableStringRef)str));
    }

    if (__CFStrIsMutable(str)) {
        printf ("CurrentCapacity %d\n%sCapacity %d\n", (int)__CFStrCapacity(str), __CFStrIsFixed(str) ? "Fixed" : "Desired", (int)__CFStrDesiredCapacity(str));
    }
    printf ("Contents %p\n", (void *)__CFStrContents(str));
}

void CFLog(int p, CFStringRef format, ...) {
    CFStringRef result;
    va_list argList;

#if defined (__MACOS8__)

#define	kCFLogMaxProcessNameLength	32

    // get process name
    //
    static Boolean             gReadProcessName = false;
    static ProcessSerialNumber gProcessID;
    static char                gProcessName[kCFLogMaxProcessNameLength];

    if (!gReadProcessName)
	{
		ProcessInfoRec processInfo;
        Str255         processName;
        FSSpec         processAppSpec;
        processInfo.processInfoLength = sizeof(ProcessInfoRec);	// we need to set these
        processInfo.processName       = processName;
        processInfo.processAppSpec    = &processAppSpec;

        GetCurrentProcess(&gProcessID);

        if (GetCurrentProcess(&gProcessID) == noErr &&
			 GetProcessInformation(&gProcessID, &processInfo) == noErr)
	    {
			// convert to C string of max. length
			int len = StrLength(processName);
            if (len >= kCFLogMaxProcessNameLength) len = kCFLogMaxProcessNameLength - 1;
 			strncpy (gProcessName, processName + 1, len);
			gProcessName[len] = '\0';	// terminate string
	    }
        else
	    {
			gProcessName[0] = '\0';	// empty name
	    }

		gReadProcessName = true;
    }
#endif

    va_start(argList, format);
    result = CFStringCreateWithFormatAndArguments(NULL, NULL, format, argList);
    va_end(argList);

#if defined (__MACOS8__)
    // we only bother with low order long of PSN (this will come back to haunt me, i'm sure)
    printf("*** CFLog (%d): %s[%ld] ", p, gProcessName, gProcessID.lowLongOfPSN);
#else
#if defined(__WIN32__)
    printf("*** CFLog (%d): %s[%d] ", p, _CFArgv()[0], GetCurrentProcessId());
#else
    printf("*** CFLog (%d): [%d] ", p, getpid());
#endif
#endif

    CFShow(result);
    CFRelease(result);
}

#endif

#if defined (__MACOS8__)

// these functions are required by the Metrowerks Standard Library
// in order to handle the stdio functionality.

#define	kCFLogFileName	"CoreFoundationLib Log"
static FILE* gCFLogFile = NULL;

short InstallConsole(short fd);
void  RemoveConsole(void);
long  WriteCharsToConsole(char *buffer, long n);
long  ReadCharsFromConsole(char *buffer, long n);

short InstallConsole(short fd)
{
    // open log file in current directory which should be application
    // directory. if this fails, just return 0. This function should
    // only be called in the debug case anyways.
    //
    if (gCFLogFile == NULL)
    {
        gCFLogFile = fopen (kCFLogFileName, "w");
        return errno;
    }
    else
        return ENOERR;
}

void RemoveConsole(void)
{
    // close open file
    //
    if (gCFLogFile != NULL)
    {
        fflush (gCFLogFile);
    	fclose (gCFLogFile);
    	gCFLogFile = NULL;
    }
}

long WriteCharsToConsole(char *buffer, long n)
{
    // write characters to file. return number
    // of characters written
    //
    if (gCFLogFile != NULL)
    {
        n = fwrite (buffer, sizeof(char), n, gCFLogFile);
        fflush (gCFLogFile);
		return n;
    }
    else
    {
         return 0;	// return zero characters written
    }
}

long ReadCharsFromConsole(char *buffer, long n)
{
#pragma unused (buffer, n)
    return 0;		// always return zero characters read
}

#endif


