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
/*	CFUniChar.h
	Copyright 1998-2000, Apple, Inc. All rights reserved.
*/

#if !defined(__COREFOUNDATION_CFUNICHAR__)
#define __COREFOUNDATION_CFUNICHAR__ 1

#include <CoreFoundation/CFBase.h>

#if defined(__cplusplus)
extern "C" {
#endif __cplusplus

enum {
    kCFUniCharControlCharacterSet = 1,
    kCFUniCharWhitespaceCharacterSet,
    kCFUniCharWhitespaceAndNewlineCharacterSet,
    kCFUniCharDecimalDigitCharacterSet,
    kCFUniCharLetterCharacterSet,
    kCFUniCharLowercaseLetterCharacterSet,
    kCFUniCharUppercaseLetterCharacterSet,
    kCFUniCharNonBaseCharacterSet,
    kCFUniCharDecomposableCharacterSet,
    kCFUniCharAlphaNumericCharacterSet,
    kCFUniCharPunctuationCharacterSet,
    kCFUniCharIllegalCharacterSet,
};

CF_EXPORT Boolean CFUniCharIsMemberOf(UniChar theChar, UInt32 charset);

enum {
    kCFUniCharToLowercase,
    kCFUniCharToUppercase,
    kCFUniCharToTitlecase,
    kCFUniCharDecompose,
};

CF_EXPORT UInt32 CFUniCharMapTo(UniChar theChar, UniChar *convertedChar, UInt32 maxLength, UInt16 ctype, UInt32 flags);

extern UniChar toulower(UniChar);
extern UniChar touupper(UniChar);
extern UniChar toutitle(UniChar);

#if defined(__cplusplus)
}
#endif

#endif /* ! __COREFOUNDATION_CFUNICHAR__ */

