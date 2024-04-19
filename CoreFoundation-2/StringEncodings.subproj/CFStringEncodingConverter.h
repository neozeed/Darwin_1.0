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
/*	CFStringEncodingConverter.h
	Copyright 1998-2000, Apple, Inc. All rights reserved.
*/

#ifndef __CFSTRINGENCODINGCONVERTER__
#define __CFSTRINGENCODINGCONVERTER__ 1

#include <CoreFoundation/CFString.h>


#if defined(__cplusplus)
extern "C" {
#endif

/* Values for flags argument for the conversion functions below.  These can be combined, but the three NonSpacing behavior flags are exclusive.
*/
enum {
    kCFStringEncodingAllowLossyConversion = 1, // Uses fallback functions to substitutes non mappable chars
    kCFStringEncodingBasicDirectionLeftToRight = (1 << 1), // Converted with original direction left-to-right.
    kCFStringEncodingBasicDirectionRightToLeft = (1 << 2), // Converted with original direction right-to-left.
    kCFStringEncodingSubstituteCombinings = (1 << 3), // Uses fallback function to combining chars.
    kCFStringEncodingComposeCombinings = (1 << 4), // Checks mappable precomposed equivalents for decomposed sequences.  This is the default behavior.
    kCFStringEncodingIgnoreCombinings = (1 << 5), // Ignores combining chars.
    kCFStringEncodingUseCanonical = (1 << 6), // Always use canonical form
    kCFStringEncodingUseHFSPlusCanonical = (1 << 7), // Always use canonical form but leaves 0x2000 ranges
    kCFStringEncodingPrependBOM = (1 << 8), // Prepend BOM sequence (i.e. ISO2022KR)
    kCFStringEncodingDisableCorporateArea = (1 << 9), // Disable the usage of 0xF8xx area for Apple proprietary chars in converting to UniChar, resulting loosely mapping.
};

enum {
    kCFStringEncodingConversionSuccess = 0,
    kCFStringEncodingInvalidInputStream = 1,
    kCFStringEncodingInsufficientOutputBufferLength = 2,
    kCFStringEncodingConverterUnavailable = 3,
};

/* Converts characters into the specified encoding.  Returns number of characters processed.
If maxByteLen is 0, bytes is ignored.
*/
extern UInt32 CFStringEncodingUnicodeToBytes(UInt32 encoding, UInt32 flags, const UniChar *characters, UInt32 numChars, UInt32 *usedCharLen, UInt8 *bytes, UInt32 maxByteLen, UInt32 *usedByteLen);

/* Converts bytes in the specified encoding into unicode.  Returns number of bytes processed.
maxCharLen & usdCharLen are in UniChar length, not byte length.
If maxCharLen is 0, characters is ignored.
*/
extern UInt32 CFStringEncodingBytesToUnicode(UInt32 encoding, UInt32 flags, const UInt8 *bytes, UInt32 numBytes, UInt32 *usedByteLen, UniChar *characters, UInt32 maxCharLen, UInt32 *usedCharLen);

/* Fallback functions used when allowLossy
*/
typedef UInt32 (*CFStringEncodingToBytesFallbackProc)(const UniChar *characters, UInt32 numChars, UInt8 *bytes, UInt32 maxByteLen, UInt32 *usedByteLen);
typedef UInt32 (*CFStringEncodingToUnicodeFallbackProc)(const UInt8 *bytes, UInt32 numBytes, UniChar *characters, UInt32 maxCharLen, UInt32 *usedCharLen);

extern Boolean CFStringEncodingIsValidEncoding(UInt32 encoding);

/* Returns kCFStringEncodingInvalidId terminated encoding list
*/
extern const UInt32 *CFStringEncodingListOfAvailableEncodings(void);

extern const char *CFStringEncodingName(UInt32 encoding);

/* Returns NULL-terminated list of IANA registered canonical names
*/
extern const char **CFStringEncodingCanonicalCharsetNames(UInt32 encoding);

/* Returns required length of destination buffer for conversion.  These functions are faster than specifying 0 to maxByteLen (maxCharLen), but unnecessarily optimal length
*/
extern UInt32 CFStringEncodingCharLengthForBytes(UInt32 encoding, UInt32 flags, const UInt8 *bytes, UInt32 numBytes);
extern UInt32 CFStringEncodingByteLengthForCharacters(UInt32 encoding, UInt32 flags, const UniChar *characters, UInt32 numChars);

/* Can register functions used for lossy conversion.  Reregisters default procs if NULL
*/
extern void CFStringEncodingRegisterFallbackProcedures(UInt32 encoding, CFStringEncodingToBytesFallbackProc toBytes, CFStringEncodingToUnicodeFallbackProc toUnicode);

#if defined(__cplusplus)
}
#endif

#endif __CFSTRINGENCODINGCONVERTER__

