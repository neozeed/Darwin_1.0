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
/*	CFStringEncodingConverterPriv.h
	Copyright 1998-2000, Apple, Inc. All rights reserved.
*/

#include <CoreFoundation/CFBase.h>
#include "CFStringEncodingConverterExt.h" 

#define MAX_IANA_ALIASES (4)

typedef UInt32 (*_CFToBytesProc)(const void *converter, UInt32 flags, const UniChar *characters, UInt32 numChars, UInt8 *bytes, UInt32 maxByteLen, UInt32 *usedByteLen);
typedef UInt32 (*_CFToUnicodeProc)(const void *converter, UInt32 flags, const UInt8 *bytes, UInt32 numBytes, UniChar *characters, UInt32 maxCharLen, UInt32 *usedCharLen);

typedef struct {
    _CFToBytesProc toBytes;
    _CFToUnicodeProc toUnicode;
    _CFToUnicodeProc toCanonicalUnicode;
    void *_toBytes; // original proc
    void *_toUnicode; // original proc
    UInt16 maxLen;
    CFStringEncodingToBytesLenProc toBytesLen;
    CFStringEncodingToUnicodeLenProc toUnicodeLen;
    CFStringEncodingToBytesFallbackProc toBytesFallback;
    CFStringEncodingToUnicodeFallbackProc toUnicodeFallback;
    CFStringEncodingToBytesPrecomposeProc toBytesPrecompose;
    CFStringEncodingIsValidCombiningCharacterProc isValidCombiningChar;
} _CFEncodingConverter;

typedef struct {
    UInt32 encoding;
    _CFEncodingConverter *converter;
    const char *encodingName;
    const char *ianaNames[MAX_IANA_ALIASES];
    const char *loadablePath;
    CFStringEncodingBootstrapProc bootstrap;
    CFStringEncodingToBytesFallbackProc toBytesFallback;
    CFStringEncodingToUnicodeFallbackProc toUnicodeFallback;
    UInt16 scriptCode;
} _CFConverterEntry;

/* Built-in encodings */
#ifdef __MACH__
#define CF_PRIVATE_EXTERN __private_extern__
#else
#define CF_PRIVATE_EXTERN extern
#endif

CF_PRIVATE_EXTERN CFStringEncodingConverter __CFConverterASCII;
CF_PRIVATE_EXTERN CFStringEncodingConverter __CFConverterISOLatin1;
CF_PRIVATE_EXTERN CFStringEncodingConverter __CFConverterMacRoman;
CF_PRIVATE_EXTERN CFStringEncodingConverter __CFConverterWinLatin1;
CF_PRIVATE_EXTERN CFStringEncodingConverter __CFConverterNextStepLatin;
CF_PRIVATE_EXTERN CFStringEncodingConverter __CFConverterUTF8;

