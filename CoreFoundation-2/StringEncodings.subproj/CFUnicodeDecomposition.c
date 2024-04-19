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
/*	CFUnicodeDecomposition.c
	Copyright 1999-2000, Apple, Inc. All rights reserved.
	Responsibility: Aki Inoue
*/

#include <CoreFoundation/CFString.h>
#include "CFStringEncodingConverterExt.h"
#include "CFUniChar.h"

Boolean CFStringEncodingIsDecomposableCharacter(UniChar character, Boolean isHFSPlusCanonical) {
    if (isHFSPlusCanonical && (character >= 0x2000) && (character < 0x3000)) return FALSE;
    return CFUniCharIsMemberOf(character, kCFUniCharDecomposableCharacterSet);
}

UInt32 CFStringEncodingDecomposeCharacter(UInt32 flags, UniChar character, UniChar *characters) {

    if (!CFStringEncodingIsDecomposableCharacter(character, (flags & kCFStringEncodingUseHFSPlusCanonical) != FALSE)) return 0;
    return CFUniCharMapTo(character, characters, MAX_DECOMPOSED_LENGTH, kCFUniCharDecompose, 0);
}
