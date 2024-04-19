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
/*	CFRunLoop.h
	Copyright 1998-2000, Apple, Inc. All rights reserved.
*/

#if !defined(__COREFOUNDATION_CFRUNLOOP__)
#define __COREFOUNDATION_CFRUNLOOP__ 1

#include <CoreFoundation/CFBase.h>
#include <CoreFoundation/CFArray.h>
#include <CoreFoundation/CFDate.h>
#include <CoreFoundation/CFString.h>
#if defined(__MACH__)
    #include <mach/port.h>
#endif

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct __CFRunLoop * CFRunLoopRef;
typedef struct __CFRunLoopSource * CFRunLoopSourceRef;
typedef struct __CFRunLoopObserver * CFRunLoopObserverRef;
typedef struct __CFRunLoopTimer * CFRunLoopTimerRef;

#if defined(CFRUNLOOP_NEW_API)
/* Reasons for CFRunLoopRunInMode() to Return */
enum {
    kCFRunLoopRunFinished = 1,
    kCFRunLoopRunStopped = 2,
    kCFRunLoopRunTimedOut = 3
};
#endif

/* Run Loop Observer Activities */
typedef enum {
    kCFRunLoopEntry = (1 << 0),
    kCFRunLoopBeforeTimers = (1 << 1),
    kCFRunLoopBeforeSources = (1 << 2),
    kCFRunLoopBeforeWaiting = (1 << 5),
    kCFRunLoopAfterWaiting = (1 << 6),
    kCFRunLoopExit = (1 << 7),
    kCFRunLoopAllActivities = 0x0FFFFFFFU
} CFRunLoopActivity;

CF_EXPORT const CFStringRef kCFRunLoopDefaultMode;
#if defined(CFRUNLOOP_NEW_API)
CF_EXPORT const CFStringRef kCFRunLoopAnyMode;
#endif
#if !defined(CFRUNLOOP_NEW_API)
CF_EXPORT const CFStringRef kCFDefaultRunLoopMode;	// obsolete; will be removed
#endif

CF_EXPORT CFTypeID	CFRunLoopGetTypeID(void);

CF_EXPORT CFRunLoopRef	CFRunLoopGetCurrent(void);

CF_EXPORT CFStringRef	CFRunLoopCopyCurrentMode(CFRunLoopRef rl);
CF_EXPORT CFArrayRef	CFRunLoopCopyAllModes(CFRunLoopRef rl);
CF_EXPORT void		CFRunLoopRun(void);
#if !defined(CFRUNLOOP_NEW_API)
CF_EXPORT Boolean	CFRunLoopRunInMode(CFStringRef mode, CFTimeInterval seconds);
#else
#define CFRunLoopRunInMode(M, S)	CFRunLoopRunInMode_new(M, S)
CF_EXPORT SInt32	CFRunLoopRunInMode_new(CFStringRef mode, CFTimeInterval seconds);
#endif
#if !defined(CFRUNLOOP_NEW_API)
CF_EXPORT CFDateRef	CFRunLoopCopyNextTimerFireDate(CFRunLoopRef rl, CFStringRef mode);
#else
CF_EXPORT CFAbsoluteTime CFRunLoopGetNextTimerFireDate(CFRunLoopRef rl, CFStringRef mode);
#endif
CF_EXPORT Boolean	CFRunLoopIsWaiting(CFRunLoopRef rl);
CF_EXPORT void		CFRunLoopWakeUp(CFRunLoopRef rl);
CF_EXPORT void		CFRunLoopStop(CFRunLoopRef rl);
#if !defined(CFRUNLOOP_NEW_API)
CF_EXPORT void		CFRunLoopAbort(CFRunLoopRef rl);
#endif

CF_EXPORT Boolean	CFRunLoopContainsSource(CFRunLoopRef rl, CFRunLoopSourceRef source, CFStringRef mode);
CF_EXPORT void		CFRunLoopAddSource(CFRunLoopRef rl, CFRunLoopSourceRef source, CFStringRef mode);
CF_EXPORT void		CFRunLoopRemoveSource(CFRunLoopRef rl, CFRunLoopSourceRef source, CFStringRef mode);

CF_EXPORT Boolean	CFRunLoopContainsObserver(CFRunLoopRef rl, CFRunLoopObserverRef observer, CFStringRef mode);
CF_EXPORT void		CFRunLoopAddObserver(CFRunLoopRef rl, CFRunLoopObserverRef observer, CFStringRef mode);
CF_EXPORT void		CFRunLoopRemoveObserver(CFRunLoopRef rl, CFRunLoopObserverRef observer, CFStringRef mode);

CF_EXPORT Boolean	CFRunLoopContainsTimer(CFRunLoopRef rl, CFRunLoopTimerRef timer, CFStringRef mode);
CF_EXPORT void		CFRunLoopAddTimer(CFRunLoopRef rl, CFRunLoopTimerRef timer, CFStringRef mode);
CF_EXPORT void		CFRunLoopRemoveTimer(CFRunLoopRef rl, CFRunLoopTimerRef timer, CFStringRef mode);

typedef struct {
    CFIndex	version;
    void *	info;
    const void *(*retain)(const void *info);
    void	(*release)(const void *info);
    CFStringRef	(*copyDescription)(const void *info);
    Boolean	(*equal)(const void *info1, const void *info2);
    CFHashCode	(*hash)(const void *info);
    void	(*schedule)(void *info, CFRunLoopRef rl, CFStringRef mode);
    void	(*cancel)(void *info, CFRunLoopRef rl, CFStringRef mode);
    void	(*perform)(void *info);
} CFRunLoopSourceContext;

#if defined(__MACH__)
typedef struct {
    CFIndex	version;
    void *	info;
    const void *(*retain)(const void *info);
    void	(*release)(const void *info);
    CFStringRef	(*copyDescription)(const void *info);
    Boolean	(*equal)(const void *info1, const void *info2);
    CFHashCode	(*hash)(const void *info);
    mach_port_t	(*getPort)(void *info);
    void *	(*perform)(void *msg, CFIndex size, CFAllocatorRef allocator, void *info);
} CFRunLoopSourceContext1;
#endif

CF_EXPORT CFTypeID	CFRunLoopSourceGetTypeID(void);

CF_EXPORT CFRunLoopSourceRef	CFRunLoopSourceCreate(CFAllocatorRef allocator, CFIndex order, CFRunLoopSourceContext *context);

CF_EXPORT CFIndex	CFRunLoopSourceGetOrder(CFRunLoopSourceRef source);
CF_EXPORT void		CFRunLoopSourceInvalidate(CFRunLoopSourceRef source);
CF_EXPORT Boolean	CFRunLoopSourceIsValid(CFRunLoopSourceRef source);
CF_EXPORT void		CFRunLoopSourceGetContext(CFRunLoopSourceRef source, CFRunLoopSourceContext *context);
CF_EXPORT void		CFRunLoopSourceSignal(CFRunLoopSourceRef source);

typedef struct {
    CFIndex	version;
    void *	info;
    const void *(*retain)(const void *info);
    void	(*release)(const void *info);
    CFStringRef	(*copyDescription)(const void *info);
} CFRunLoopObserverContext;

typedef void (*CFRunLoopObserverCallBack)(CFRunLoopObserverRef observer, CFRunLoopActivity activity, void *info);

CF_EXPORT CFTypeID	CFRunLoopObserverGetTypeID(void);

CF_EXPORT CFRunLoopObserverRef	CFRunLoopObserverCreate(CFAllocatorRef allocator, CFOptionFlags activities, Boolean repeats, CFIndex order, CFRunLoopObserverCallBack callout, CFRunLoopObserverContext *context);

CF_EXPORT CFOptionFlags	CFRunLoopObserverGetActivities(CFRunLoopObserverRef observer);
CF_EXPORT Boolean	CFRunLoopObserverDoesRepeat(CFRunLoopObserverRef observer);
CF_EXPORT CFIndex	CFRunLoopObserverGetOrder(CFRunLoopObserverRef observer);
CF_EXPORT void		CFRunLoopObserverInvalidate(CFRunLoopObserverRef observer);
CF_EXPORT Boolean	CFRunLoopObserverIsValid(CFRunLoopObserverRef observer);
CF_EXPORT void		CFRunLoopObserverGetContext(CFRunLoopObserverRef observer, CFRunLoopObserverContext *context);

typedef struct {
    CFIndex	version;
    void *	info;
    const void *(*retain)(const void *info);
    void	(*release)(const void *info);
    CFStringRef	(*copyDescription)(const void *info);
} CFRunLoopTimerContext;

typedef void (*CFRunLoopTimerCallBack)(CFRunLoopTimerRef timer, void *info);

CF_EXPORT CFTypeID	CFRunLoopTimerGetTypeID(void);

#if !defined(CFRUNLOOP_NEW_API)
CF_EXPORT CFRunLoopTimerRef	CFRunLoopTimerCreate(CFAllocatorRef allocator, CFDateRef fireDate, CFTimeInterval interval, CFIndex order, CFRunLoopTimerCallBack callout, CFRunLoopTimerContext *context);
CF_EXPORT CFDateRef	CFRunLoopTimerCopyNextFireDate(CFRunLoopTimerRef timer);
CF_EXPORT void		CFRunLoopTimerSetNextFireDate(CFRunLoopTimerRef timer, CFDateRef fireDate);
#else
#define CFRunLoopTimerCreate(A, D, I, F, O, C, X) CFRunLoopTimerCreate_new(A, D, I, F, O, C, X)
#define CFRunLoopTimerSetNextFireDate(T, D) CFRunLoopTimerSetNextFireDate_new(T, D)
CF_EXPORT CFRunLoopTimerRef	CFRunLoopTimerCreate_new(CFAllocatorRef allocator, CFAbsoluteTime fireDate, CFTimeInterval interval, CFOptionFlags flags, CFIndex order, CFRunLoopTimerCallBack callout, CFRunLoopTimerContext *context);
CF_EXPORT CFAbsoluteTime CFRunLoopTimerGetNextFireDate(CFRunLoopTimerRef timer);
CF_EXPORT void		CFRunLoopTimerSetNextFireDate_new(CFRunLoopTimerRef timer, CFAbsoluteTime fireDate);
#endif
CF_EXPORT CFTimeInterval CFRunLoopTimerGetInterval(CFRunLoopTimerRef timer);
CF_EXPORT Boolean	CFRunLoopTimerDoesRepeat(CFRunLoopTimerRef timer);
CF_EXPORT CFIndex	CFRunLoopTimerGetOrder(CFRunLoopTimerRef timer);
CF_EXPORT void		CFRunLoopTimerInvalidate(CFRunLoopTimerRef timer);
CF_EXPORT Boolean	CFRunLoopTimerIsValid(CFRunLoopTimerRef timer);
CF_EXPORT void		CFRunLoopTimerGetContext(CFRunLoopTimerRef timer, CFRunLoopTimerContext *context);

#if defined(__cplusplus)
}
#endif

#endif /* ! __COREFOUNDATION_CFRUNLOOP__ */

