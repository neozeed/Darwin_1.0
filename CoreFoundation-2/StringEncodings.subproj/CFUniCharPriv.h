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
/*	CFUniCharPriv.h
	Copyright 1998-2000, Apple, Inc. All rights reserved.
*/

#if !defined __UCHAR_PRIVATE__
#define __UCHAR_PRIVATE__ 1

#include <CoreFoundation/CFBase.h>
#include "CFUniChar.h"

#define PRIVATE_SYSTEM_BASE (kCFUniCharIllegalCharacterSet + 1)
#define EXTENDED_BITMAP_BASE (100)

#define kCFUniCharDottedICaseFlag (1) // This is currently private

enum {
    kCFUniCharHasNonSelfLowercaseMapping = PRIVATE_SYSTEM_BASE,
    kCFUniCharHasNonSelfUppercaseMapping,
    kCFUniCharHasNonSelfTitlecaseMapping,
};

extern Boolean _UniCharLoadCharacterSetBitmapWithName(const char *bitmapName, void **bytes);
extern void _UniCharGetBitmap(char *bitmap, UInt32 charset, Boolean isInverted);
#endif
