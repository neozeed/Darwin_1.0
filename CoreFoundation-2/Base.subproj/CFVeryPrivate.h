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
/*	CFVeryPrivate.h
	Copyright 1998-2000, Apple, Inc. All rights reserved.
*/

/*
        NOT TO BE USED OUTSIDE CF WITHOUT FIRST CONTACTING ckane!
*/

#if !defined(__COREFOUNDATION_CFVERYPRIVATE__)
#define __COREFOUNDATION_CFVERYPRIVATE__ 1

#if !defined(CF_BUILDING_CF) && !defined(BUILDING_PBS) && !defined(NSBUILDINGFOUNDATION) && !defined(_NSBUILDING_APPKIT_DLL)
#error The header file CFVeryPrivate.h is for the exclusive use of pbs, Foundation, and the AppKit.  No other project should import it.
#endif

#include "CFPriv.h"
#include <CoreFoundation/CFURL.h>
#include <CoreFoundation/CFCharacterSet.h>


#if defined(__cplusplus)
extern "C" {
#endif

#if defined(CF_BUILDING_CF) || defined(NSBUILDINGFOUNDATION)



CF_EXPORT CFStringRef __CFStringCreateImmutableFunnel2(CFAllocatorRef alloc, const void *bytes, CFIndex numBytes, CFStringEncoding encoding, Boolean possiblyExternalFormat, Boolean tryToReduceUnicode, Boolean hasLengthByte, Boolean hasNullByte, Boolean noCopy, CFAllocatorRef contentsDeallocator);
CF_EXPORT CFComparisonResult _CFCompareStringsWithLocale(CFStringRef str1, CFStringRef str2, CFRange compareRange, CFOptionFlags options, CFDictionaryRef compareLocale);
#endif


CF_EXPORT CFStringRef _CFCreateLimitedUniqueString();


#if !defined(__MACOS8__)
/* Enumeration
 Call CFStartSearchPathEnumeration() once, then call CFGetNextSearchPathEnumeration() one or more times with the returned state.
 The return value of CFGetNextSearchPathEnumeration() should be used as the state next time around.
 When CFGetNextSearchPathEnumeration() returns 0, you're done.
*/
typedef CFIndex CFSearchPathEnumerationState;
CF_EXPORT CFSearchPathEnumerationState __CFStartSearchPathEnumeration(CFSearchPathDirectory dir, CFSearchPathDomainMask domainMask);
CF_EXPORT CFSearchPathEnumerationState __CFGetNextSearchPathEnumeration(CFSearchPathEnumerationState state, UInt8 *path);
#endif


CF_EXPORT void CFLog(int p, CFStringRef str, ...);

CF_INLINE Boolean __CFStringEncodingIsSupersetOfASCII(CFStringEncoding encoding) {
    switch (encoding & 0x0000FF00) {
        case 0x0: // MacOS Script range
            return TRUE;

        case 0x100: // Unicode range
            if (encoding == kCFStringEncodingUnicode) return FALSE;
            return TRUE;

        case 0x200: // ISO range
            return TRUE;
            
        case 0x600: // National standards range
            if (encoding != kCFStringEncodingASCII) return FALSE;
            return TRUE;

        case 0x800: // ISO 2022 range
            return FALSE; // It's modal encoding

        case 0xA00: // Misc standard range
            return TRUE;

        case 0xB00:
            if (encoding == kCFStringEncodingNonLossyASCII) return FALSE;
            return TRUE;

        case 0xC00: // EBCDIC
            return FALSE;

        default:
            return ((encoding & 0x0000FF00) > 0x0C00 ? FALSE : TRUE);
    }
}

/* Desperately using extern here */
CF_EXPORT CFStringEncoding __CFDefaultEightBitStringEncoding;
CF_EXPORT CFStringEncoding __CFStringComputeEightBitStringEncoding(void);

CF_INLINE CFStringEncoding __CFStringGetEightBitStringEncoding(void) {
    if (__CFDefaultEightBitStringEncoding == kCFStringEncodingInvalidId) __CFStringComputeEightBitStringEncoding();
    return __CFDefaultEightBitStringEncoding;
}

typedef struct {      /* A simple struct to maintain ASCII/Unicode versions of the same buffer. */
     union {
        UInt8 *ascii;
	UniChar *unicode;
    } chars;
    Boolean isASCII;	/* This really does mean 7-bit ASCII, not _NSDefaultCStringEncoding() */
    Boolean shouldFreeChars;	/* If the number of bytes exceeds __kCFVarWidthLocalBufferSize, bytes are allocated */
    CFAllocatorRef allocator;	/* Use this allocator to allocate, reallocate, and deallocate the bytes */
    UInt32 numChars;	/* This is in terms of ascii or unicode; that is, if isASCII, it is number of 7-bit chars; otherwise it is number of UniChars; note that the actual allocated space might be larger */
#define __kCFVarWidthLocalBufferSize 1008
    UInt8 localBuffer[__kCFVarWidthLocalBufferSize];	/* private; 168 ISO2022JP chars, 504 Unicode chars, 1008 ASCII chars */
} CFVarWidthCharBuffer;


/* Convert a byte stream to ASCII (7-bit!) or Unicode, with a CFVarWidthCharBuffer struct on the stack. FALSE return indicates an error occured during the conversion. Depending on .isASCII, follow .chars.ascii or .chars.unicode.  If .shouldFreeChars is returned as true, free the returned buffer when done with it.  If useClientsMemoryPtr is provided as non-NULL, and the provided memory can be used as is, this is set to TRUE, and the .ascii or .unicode buffer in CFVarWidthCharBuffer is set to bytes.
!!! If the stream is Unicode and has no BOM, the data is assumed to be big endian! Could be trouble on Intel if someone didn't follow that assumption.
*/
CF_EXPORT Boolean __CFStringDecodeByteStream2(const UInt8 *bytes, UInt32 len, CFStringEncoding encoding, Boolean alwaysUnicode, CFVarWidthCharBuffer *buffer, Boolean *useClientsMemoryPtr);



/* Create a byte stream from a CFString backing. Can convert a string piece at a time
   into a fixed size buffer. Returns number of characters converted.
   Characters that cannot be converted to the specified encoding are represented
   with the char specified by lossByte; if 0, then lossy conversion is not allowed
   and conversion stops, returning partial results.
   generatingExternalFile indicates that any extra stuff to allow this data to be
   persistent (for instance, BOM) should be included. 
   Pass buffer==NULL if you don't care about the converted string (but just the convertability,
   or number of bytes required, indicated by usedBufLen).
   Does not zero-terminate. If you want to create Pascal or C string, allow one extra byte at start or end.
*/
CF_EXPORT CFIndex __CFStringEncodeByteStream(CFStringRef string, CFIndex rangeLoc, CFIndex rangeLen, Boolean generatingExternalFile, CFStringEncoding encoding, char lossByte, UInt8 *buffer, CFIndex max, CFIndex *usedBufLen);

/* Convert single byte to Unicode; assumes one-to-one correspondence (that is, can only be used with 1-byte encodings). You can use the function if it's not NULL.
*/
CF_EXPORT Boolean (*__CFCharToUniCharFunc)(UInt32 flags, UInt8 ch, UniChar *unicodeChar);

CF_EXPORT void __CFStrConvertBytesToUnicode(const UInt8 *bytes, UniChar *buffer, CFIndex numChars);

/* result is long long or int, depending on doLonglong
*/
CF_EXPORT Boolean __CFStringScanInteger(CFStringInlineBuffer *buf, CFDictionaryRef locale, SInt32 *indexPtr, Boolean doLonglong, void *result);
CF_EXPORT Boolean __CFStringScanDouble(CFStringInlineBuffer *buf, CFDictionaryRef locale, SInt32 *indexPtr, double *resultPtr); 
CF_EXPORT Boolean __CFStringScanHex(CFStringInlineBuffer *buf, SInt32 *indexPtr, unsigned *result);

/* Character class functions UnicodeData-2_1_5.txt
*/
CF_INLINE Boolean __CFIsWhitespace(UniChar theChar) {
    return ((theChar < 0x21) || (theChar > 0x7E && theChar < 0xA1) || (theChar >= 0x2000 && theChar <= 0x200B) || (theChar == 0x3000)) ? TRUE : FALSE;
}

/* Same as CFStringGetCharacterFromInlineBuffer() but returns 0xFFFF on EOF
*/
CF_INLINE UniChar __CFStringGetCharacterFromInlineBufferAux(CFStringInlineBuffer *buf, SInt32 idx) {
    return (idx >= buf->rangeToBuffer.length) ? 0xFFFF : CFStringGetCharacterFromInlineBuffer(buf, idx);
}

/* These two allow specifying an alternate description function (instead of CFCopyDescription); used by NSString
*/
CF_EXPORT void _CFStringAppendFormatAndArgumentsAux(CFMutableStringRef outputString, CFStringRef (*copyDescFunc)(void *, CFDictionaryRef), CFDictionaryRef formatOptions, CFStringRef formatString, va_list args);
CF_EXPORT CFStringRef  _CFStringCreateWithFormatAndArgumentsAux(CFAllocatorRef alloc, CFStringRef (*copyDescFunc)(void *, CFDictionaryRef), CFDictionaryRef formatOptions, CFStringRef format, va_list arguments);

CF_EXPORT void CFCharacterSetCompact(CFMutableCharacterSetRef theSet);
CF_EXPORT void CFCharacterSetFast(CFMutableCharacterSetRef theSet);

CF_EXPORT CFURLRef _CFURLAlloc(CFAllocatorRef allocator);
CF_EXPORT void _CFURLInitWithString(CFURLRef url, CFStringRef string, CFURLRef baseURL);

CF_INLINE UInt64 __CFReadTSR() {
    union {
	UInt64 time64;
	UInt32 word[2];
    } now;
#if defined(__i386__)
    /* Read from Pentium and Pentium Pro 64-bit timestamp counter. */
    /* The counter is set to 0 at processor reset and increments on */
    /* every clock cycle. */
    __asm__ volatile("rdtsc" : : : "eax", "edx");
    __asm__ volatile("movl %%eax,%0" : "=m" (now.word[0]) : : "eax");
    __asm__ volatile("movl %%edx,%0" : "=m" (now.word[1]) : : "edx");
#elif defined(__ppc__) && defined(__MACH__)
    /* Read from PowerPC 64-bit time base register. The increment */
    /* rate of the time base is implementation-dependent, but is */
    /* 1/4th the bus clock cycle on 603/604/750 processors. */
    UInt32 t3;
    do {
	__asm__ volatile("mftbu %0" : "=r" (now.word[0]));
	__asm__ volatile("mftb %0" : "=r" (now.word[1]));
	__asm__ volatile("mftbu %0" : "=r" (t3));
    } while (now.word[0] != t3);
#elif defined(__MACOS8__)
    now.time64 = 0ULL;
#else
// ??? Do not know how to read a time stamp register on this architecture
    now.time64 = 0ULL;
#endif
    return now.time64;
}

#if defined(__cplusplus)
}
#endif

#endif /* ! __COREFOUNDATION_CFVERYPRIVATE__ */

