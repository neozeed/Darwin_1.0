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
/*	CFMachPort.h
	Copyright 1998-2000, Apple, Inc. All rights reserved.
*/

#if !defined(__COREFOUNDATION_CFMACHPORT__)
#define __COREFOUNDATION_CFMACHPORT__ 1

#if defined(__MACH__)

#include <CoreFoundation/CFRunLoop.h>
#include <mach/port.h>

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct __CFMachPort * CFMachPortRef;

typedef struct {
    CFIndex	version;
    void *	info;
    const void *(*retain)(const void *info);
    void	(*release)(const void *info);
    CFStringRef	(*copyDescription)(const void *info);
} CFMachPortContext;

#if !defined(CFRUNLOOP_NEW_API)
typedef void (*CFMachPortCallBack)(CFMachPortRef port, void *msg, void *info);
#else
typedef void (*CFMachPortCallBack)(CFMachPortRef port, void *msg, CFIndex size, void *info);
#endif

CF_EXPORT void		CFMachPortInvalidateAll(void);

CF_EXPORT CFTypeID	CFMachPortGetTypeID(void);

#if !defined(CFRUNLOOP_NEW_API)
CF_EXPORT CFMachPortRef	CFMachPortCreate(CFAllocatorRef allocator, CFMachPortCallBack callout, CFMachPortContext *context);
CF_EXPORT CFMachPortRef	CFMachPortCreateWithPort(CFAllocatorRef allocator, mach_port_t portNum, CFMachPortCallBack callout, CFMachPortContext *context);
#else
#define CFMachPortCreate(A, C, X, B) CFMachPortCreate_new(A, C, X, B)
#define CFMachPortCreateWithPort(A, P, C, X, B) CFMachPortCreateWithPort_new(A, P, C, X, B)
CF_EXPORT CFMachPortRef	CFMachPortCreate_new(CFAllocatorRef allocator, CFMachPortCallBack callout, CFMachPortContext *context, Boolean *shouldFreeInfo);
CF_EXPORT CFMachPortRef	CFMachPortCreateWithPort_new(CFAllocatorRef allocator, mach_port_t portNum, CFMachPortCallBack callout, CFMachPortContext *context, Boolean *shouldFreeInfo);
#endif

CF_EXPORT mach_port_t	CFMachPortGetPort(CFMachPortRef port);
CF_EXPORT void		CFMachPortGetContext(CFMachPortRef port, CFMachPortContext *context);
CF_EXPORT void		CFMachPortInvalidate(CFMachPortRef port);
CF_EXPORT Boolean	CFMachPortIsValid(CFMachPortRef port);

CF_EXPORT CFRunLoopSourceRef	CFMachPortCreateRunLoopSource(CFAllocatorRef allocator, CFMachPortRef port, CFIndex order);

#if defined(__cplusplus)
}
#endif

#endif /* __MACH__ */

#endif /* ! __COREFOUNDATION_CFMACHPORT__ */

