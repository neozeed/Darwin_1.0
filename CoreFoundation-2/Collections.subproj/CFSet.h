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
/*	CFSet.h
	Copyright 1998-2000, Apple, Inc. All rights reserved.
*/

#if !defined(__COREFOUNDATION_CFSET__)
#define __COREFOUNDATION_CFSET__ 1

#include <CoreFoundation/CFBase.h>

#if defined(__cplusplus)
extern "C" {
#endif

typedef const void *	(*CFSetRetainCallBack)(CFAllocatorRef allocator, const void *value);
typedef void		(*CFSetReleaseCallBack)(CFAllocatorRef allocator, const void *value);
typedef CFStringRef	(*CFSetCopyDescriptionCallBack)(const void *value);
typedef Boolean		(*CFSetEqualCallBack)(const void *value1, const void *value2);
typedef CFHashCode	(*CFSetHashCallBack)(const void *value);
typedef struct {
    CFIndex				version;
    CFSetRetainCallBack			retain;
    CFSetReleaseCallBack		release;
    CFSetCopyDescriptionCallBack	copyDescription;
    CFSetEqualCallBack			equal;
    CFSetHashCallBack			hash;
} CFSetCallBacks;

CF_EXPORT
const CFSetCallBacks kCFTypeSetCallBacks;

CF_EXPORT
const CFSetCallBacks kCFCopyStringSetCallBacks;

typedef void (*CFSetApplierFunction)(const void *value, void *context);

typedef const struct __CFSet * CFSetRef;
typedef struct __CFSet * CFMutableSetRef;

CF_EXPORT
CFTypeID CFSetGetTypeID(void);

CF_EXPORT
CFSetRef CFSetCreate(CFAllocatorRef allocator, const void **values, CFIndex numValues, const CFSetCallBacks *callBacks);

CF_EXPORT
CFSetRef CFSetCreateCopy(CFAllocatorRef allocator, CFSetRef theSet);

CF_EXPORT
CFMutableSetRef CFSetCreateMutable(CFAllocatorRef allocator, CFIndex capacity, const CFSetCallBacks *callBacks);

CF_EXPORT
CFMutableSetRef CFSetCreateMutableCopy(CFAllocatorRef allocator, CFIndex capacity, CFSetRef theSet);

CF_EXPORT
CFIndex CFSetGetCount(CFSetRef theSet);

CF_EXPORT
CFIndex CFSetGetCountOfValue(CFSetRef theSet, const void *value);

CF_EXPORT
Boolean CFSetContainsValue(CFSetRef theSet, const void *value);

CF_EXPORT
const void *CFSetGetValue(CFSetRef theSet, const void *value);

CF_EXPORT
Boolean CFSetGetValueIfPresent(CFSetRef theSet, const void *candidate, const void **value);

CF_EXPORT
void CFSetGetValues(CFSetRef theSet, const void **values);

CF_EXPORT
void CFSetApplyFunction(CFSetRef theSet, CFSetApplierFunction applier, void *context);

CF_EXPORT
void CFSetAddValue(CFMutableSetRef theSet, const void *value);

CF_EXPORT
void CFSetReplaceValue(CFMutableSetRef theSet, const void *value);

CF_EXPORT
void CFSetSetValue(CFMutableSetRef theSet, const void *value);

CF_EXPORT
void CFSetRemoveValue(CFMutableSetRef theSet, const void *value);

CF_EXPORT
void CFSetRemoveAllValues(CFMutableSetRef theSet);

#if defined(__cplusplus)
}
#endif

#endif /* ! __COREFOUNDATION_CFSET__ */

