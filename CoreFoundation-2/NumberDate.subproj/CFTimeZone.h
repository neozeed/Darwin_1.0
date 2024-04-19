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
/*	CFTimeZone.h
	Copyright 1998-2000, Apple, Inc. All rights reserved.
*/

#if !defined(__COREFOUNDATION_CFTIMEZONE__)
#define __COREFOUNDATION_CFTIMEZONE__ 1

#include <CoreFoundation/CFBase.h>
#include <CoreFoundation/CFArray.h>
#include <CoreFoundation/CFData.h>
#include <CoreFoundation/CFDate.h>
#include <CoreFoundation/CFDictionary.h>
#include <CoreFoundation/CFString.h>

#if defined(__cplusplus)
extern "C" {
#endif

CF_EXPORT
CFTypeID CFTimeZoneGetTypeID(void);

CF_EXPORT
CFTimeZoneRef CFTimeZoneCopySystem(void);

CF_EXPORT
void CFTimeZoneResetSystem(void);

CF_EXPORT
CFTimeZoneRef CFTimeZoneCopyDefault(void);

CF_EXPORT
void CFTimeZoneSetDefault(CFTimeZoneRef tz);

CF_EXPORT
CFArrayRef CFTimeZoneCopyKnownNames(void);

CF_EXPORT
CFDictionaryRef CFTimeZoneCopyAbbreviationDictionary(void);

CF_EXPORT
void CFTimeZoneSetAbbreviationDictionary(CFDictionaryRef dict);

CF_EXPORT
CFTimeZoneRef CFTimeZoneCreate(CFAllocatorRef allocator, CFStringRef name, CFDataRef data);

CF_EXPORT
CFTimeZoneRef CFTimeZoneCreateWithTimeIntervalFromGMT(CFAllocatorRef allocator, CFTimeInterval ti);

CF_EXPORT
CFTimeZoneRef CFTimeZoneCreateWithName(CFAllocatorRef allocator, CFStringRef name, Boolean tryAbbrev);

CF_EXPORT
CFStringRef CFTimeZoneGetName(CFTimeZoneRef tz);

CF_EXPORT
CFDataRef CFTimeZoneGetData(CFTimeZoneRef tz);

CF_EXPORT
CFTimeInterval CFTimeZoneGetSecondsFromGMT(CFTimeZoneRef tz, CFAbsoluteTime at);

CF_EXPORT
CFStringRef CFTimeZoneCopyAbbreviation(CFTimeZoneRef tz, CFAbsoluteTime at);

CF_EXPORT
Boolean CFTimeZoneIsDaylightSavingTime(CFTimeZoneRef tz, CFAbsoluteTime at);

#if defined(__cplusplus)
}
#endif

#endif /* ! __COREFOUNDATION_CFTIMEZONE__ */

