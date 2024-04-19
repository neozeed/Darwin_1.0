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
/*	CFRunLoop.c
	Copyright 1998-2000, Apple, Inc. All rights reserved.
	Responsibility: Christopher Kane
*/

#define CFRUNLOOP_NEW_API

#include "CFInternal.h"
#include <CoreFoundation/CFRunLoop.h>
#include <CoreFoundation/CFSet.h>
#include <limits.h>
#if defined(__MACH__)
#include <mach/mach.h>
#include <mach/clock_types.h>
#include <mach/clock.h>
#else
#include <windows.h>
#endif
#include <math.h>

#undef CFRunLoopRunInMode
#undef CFRunLoopTimerCreate
#undef CFRunLoopTimerSetNextFireDate

static UInt32 __CFSendTrivialMachMessage(UInt32 port, UInt32 msg_id, CFOptionFlags options, UInt32 timeout) {
    kern_return_t result;
    mach_msg_header_t header;
    header.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0);
    header.msgh_size = sizeof(mach_msg_header_t);
    header.msgh_remote_port = port;
    header.msgh_local_port = MACH_PORT_NULL;
    header.msgh_id = msg_id;
    result = mach_msg(&header, MACH_SEND_MSG|options, header.msgh_size, 0, MACH_PORT_NULL, timeout, MACH_PORT_NULL);
    return result;
}

/* unlock a run loop and modes before doing callouts/sleeping */
/* never try to take the run loop lock with a mode locked */
/* be very careful of common subexpression elimination and compacting code, particular across locks and unlocks! */
/* run loop mode structures should never be deallocated, even if they become empty */

struct __CFRunLoopMode {
    UInt8 _GUARD1[1040];
    UInt32 _lock;		/* must have the run loop locked before locking this */
    CFStringRef _name;
    struct __CFRunLoopMode *_next;
    CFMutableSetRef _sources;
    CFMutableSetRef _observers;
    CFMutableSetRef _timers;
#if defined(__MACH__)
    mach_port_t _portSet;
#endif
#if defined(__WIN32__)
    DWORD _msgQMask;
#endif
    UInt8 _GUARD2[1040];
};

CF_INLINE void __CFRunLoopModeLock(struct __CFRunLoopMode *rlm) {
    __CFSpinLock(&(rlm->_lock));
}

CF_INLINE void __CFRunLoopModeUnlock(struct __CFRunLoopMode *rlm) {
    __CFSpinUnlock(&(rlm->_lock));
}

static mach_port_t clock_port = MACH_PORT_NULL;

struct __CFRunLoop {
    CFRuntimeBase _base;
    CFAllocatorRef _allocator;
    UInt32 _lock;			/* locked for traversing modes */
#if defined(__MACH__)
    mach_port_t _waitPort;
#endif
#if defined(__WIN32__)
    HANDLE _waitPort;
#endif
    struct __CFRunLoopMode *_currentMode;
    struct __CFRunLoopMode *_modes;	/* linked list of all modes */
};

/* Bit 0 of the base reserved bits is used for abort state */
/* Bit 1 of the base reserved bits is used for sleeping state */

CF_INLINE Boolean __CFRunLoopIsStopped(CFRunLoopRef rl) {
    return (Boolean)__CFBitfieldValue(((CFRuntimeBase *)rl)->_info, 0, 0);
}

CF_INLINE void __CFRunLoopSetStopped(CFRunLoopRef rl) {
    __CFBitfieldSetValue(((CFRuntimeBase *)rl)->_info, 0, 0, 1);
}

CF_INLINE void __CFRunLoopUnsetStopped(CFRunLoopRef rl) {
    __CFBitfieldSetValue(((CFRuntimeBase *)rl)->_info, 0, 0, 0);
}

CF_INLINE Boolean __CFRunLoopIsSleeping(CFRunLoopRef rl) {
    return (Boolean)__CFBitfieldValue(((CFRuntimeBase *)rl)->_info, 1, 1);
}

CF_INLINE void __CFRunLoopSetSleeping(CFRunLoopRef rl) {
    __CFBitfieldSetValue(((CFRuntimeBase *)rl)->_info, 1, 1, 1);
}

CF_INLINE void __CFRunLoopUnsetSleeping(CFRunLoopRef rl) {
    __CFBitfieldSetValue(((CFRuntimeBase *)rl)->_info, 1, 1, 0);
}

CF_INLINE void __CFRunLoopLock(CFRunLoopRef rl) {
    __CFSpinLock(&(((CFRunLoopRef)rl)->_lock));
}

CF_INLINE void __CFRunLoopUnlock(CFRunLoopRef rl) {
    __CFSpinUnlock(&(((CFRunLoopRef)rl)->_lock));
}

/* call with rl locked */
static struct __CFRunLoopMode *__CFRunLoopFindMode(CFRunLoopRef rl, CFStringRef mode, Boolean create) {
    struct __CFRunLoopMode *curMode, *lastMode;
    for (curMode = rl->_modes; curMode; curMode = curMode->_next)
	if (curMode->_name == mode || CFEqual(mode, curMode->_name)) {
	    __CFRunLoopModeLock(curMode);
	    return curMode;
	}
    if (!create) {
	return NULL;
    }
    curMode = CFAllocatorAllocate(CFGetAllocator(rl), sizeof(struct __CFRunLoopMode), 0);
{ int idx; for (idx = 0; idx < 1040; idx++) curMode->_GUARD1[idx] = curMode->_GUARD2[idx] = 0xCF; }
    curMode->_lock = 0;
    curMode->_next = NULL;
    curMode->_name = CFStringCreateCopy(CFGetAllocator(rl), mode);
    curMode->_sources = NULL;
    curMode->_observers = NULL;
    curMode->_timers = NULL;
#if defined(__MACH__)
    {
	kern_return_t ret;
	ret = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_PORT_SET, &(curMode->_portSet));
	if (KERN_SUCCESS == ret) {
	    ret = mach_port_insert_member(mach_task_self(), rl->_waitPort, curMode->_portSet);
	}
	if (KERN_SUCCESS != ret) __CFAbort();
    }
#endif
#if defined(__WIN32__)
    curMode->_msgQMask = 0;
#endif
    for (lastMode = rl->_modes; lastMode && lastMode->_next; lastMode = lastMode->_next);
    if (!lastMode) {
	((CFRunLoopRef)rl)->_modes = curMode;
    } else {
	lastMode->_next = curMode;
    }
    __CFRunLoopModeLock(curMode);	/* return mode locked */
    return curMode;
}

#if defined(__WIN32__)

CF_INLINE Boolean __CFRunLoopModeIsEmpty(CFRunLoopRef rl, struct __CFRunLoopMode *rlm) {
    return (0 == (rlm->_sources ? CFSetGetCount(rlm->_sources) : 0) && 0 == (rlm->_timers ? CFSetGetCount(rlm->_timers) : 0) && 0 == rlm->_msgQMask);
}

DWORD __CFRunLoopGetWindowsMessageQueueMask(CFRunLoopRef rl, CFStringRef mode) {
    struct __CFRunLoopMode *rlm;
    DWORD result = 0;
    __CFRunLoopLock(rl);
    rlm = __CFRunLoopFindMode(rl, mode, FALSE);
    if (rlm) {
	result = rlm->_msgQMask;
    }
    __CFRunLoopUnlock(rl);
    __CFRunLoopModeUnlock(rlm);
    return result;
}

void __CFRunLoopSetWindowsMessageQueueMask(CFRunLoopRef rl, DWORD mask, CFStringRef mode) {
    struct __CFRunLoopMode *rlm;
    __CFRunLoopLock(rl);
    rlm = __CFRunLoopFindMode(rl, mode, TRUE);
    rlm->_msgQMask = mask;
    __CFRunLoopUnlock(rl);
    __CFRunLoopModeUnlock(rlm);
}

#else

CF_INLINE Boolean __CFRunLoopModeIsEmpty(CFRunLoopRef rl, struct __CFRunLoopMode *rlm) {
    return (0 == (rlm->_sources ? CFSetGetCount(rlm->_sources) : 0) && 0 == (rlm->_timers ? CFSetGetCount(rlm->_timers) : 0));
}

#endif

/* Bit 3 in the base reserved bits is used for invalid state in run loop objects */

CF_INLINE Boolean __CFIsValid(const void *cf) {
    return (Boolean)__CFBitfieldValue(((CFRuntimeBase *)cf)->_info, 3, 3);
}

CF_INLINE void __CFSetValid(void *cf) {
    __CFBitfieldSetValue(((CFRuntimeBase *)cf)->_info, 3, 3, 1);
}

CF_INLINE void __CFUnsetValid(void *cf) {
    __CFBitfieldSetValue(((CFRuntimeBase *)cf)->_info, 3, 3, 0);
}

struct __CFRunLoopSource {
    CFRuntimeBase _base;
    CFAllocatorRef _allocator;
    UInt32 _lock;
    CFIndex _order;			/* immutable */
    union {
	CFRunLoopSourceContext version0;	/* immutable, except invalidation */
	CFRunLoopSourceContext1 version1;	/* immutable, except invalidation */
    } _context;
};

/* Bit 1 of the base reserved bits is used for signaled state */

CF_INLINE Boolean __CFRunLoopSourceIsSignaled(CFRunLoopSourceRef rls) {
    return (Boolean)__CFBitfieldValue(((CFRuntimeBase *)rls)->_info, 1, 1);
}

CF_INLINE void __CFRunLoopSourceSetSignaled(CFRunLoopSourceRef rls) {
    __CFBitfieldSetValue(((CFRuntimeBase *)rls)->_info, 1, 1, 1);
}

CF_INLINE void __CFRunLoopSourceUnsetSignaled(CFRunLoopSourceRef rls) {
    __CFBitfieldSetValue(((CFRuntimeBase *)rls)->_info, 1, 1, 0);
}

CF_INLINE void __CFRunLoopSourceLock(CFRunLoopSourceRef rls) {
    __CFSpinLock((UInt32 *)&(rls->_lock));
}

CF_INLINE void __CFRunLoopSourceUnlock(CFRunLoopSourceRef rls) {
    __CFSpinUnlock((UInt32 *)&(rls->_lock));
}

/* rlm is not locked */
CF_INLINE void __CFRunLoopSourceSchedule(CFRunLoopSourceRef rls, CFRunLoopRef rl, struct __CFRunLoopMode *rlm) {	/* DOES CALLOUT */
    if (0 == rls->_context.version0.version) {
	if (NULL != rls->_context.version0.schedule) {
	    rls->_context.version0.schedule(rls->_context.version0.info, rl, rlm->_name);
	}
#if defined(__MACH__)
    } else if (1 == rls->_context.version0.version) {
	kern_return_t ret = 0;
	__CFRunLoopSourceLock(rls);
	ret = mach_port_insert_member(mach_task_self(), rls->_context.version1.getPort(rls->_context.version1.info), rlm->_portSet);
	if (KERN_SUCCESS != ret) __CFAbort();
	__CFRunLoopSourceUnlock(rls);
#endif
    }
}

/* rlm is not locked */
CF_INLINE void __CFRunLoopSourceCancel(CFRunLoopSourceRef rls, CFRunLoopRef rl, struct __CFRunLoopMode *rlm) {	/* DOES CALLOUT */
    if (0 == rls->_context.version0.version) {
	if (NULL != rls->_context.version0.cancel) {
	    rls->_context.version0.cancel(rls->_context.version0.info, rl, rlm->_name);
	}
#if defined(__MACH__)
    } else if (1 == rls->_context.version0.version) {
	kern_return_t ret = 0;
	__CFRunLoopSourceLock(rls);
	ret = mach_port_extract_member(mach_task_self(), rls->_context.version1.getPort(rls->_context.version1.info), rlm->_portSet);
	if (KERN_SUCCESS != ret) __CFAbort();
	__CFRunLoopSourceUnlock(rls);
#endif
    }
}

struct __CFRunLoopObserver {
    CFRuntimeBase _base;
    CFAllocatorRef _allocator;
    UInt32 _lock;
    CFRunLoopRef _runLoop;
    CFIndex _rlCount;
    CFOptionFlags _activities;		/* immutable */
    CFIndex _order;			/* immutable */
    CFRunLoopObserverCallBack _callout;	/* immutable */
    CFRunLoopObserverContext _context;	/* immutable, except invalidation */
};

/* Bit 0 of the base reserved bits is used for firing state */
/* Bit 1 of the base reserved bits is used for repeats state */

CF_INLINE Boolean __CFRunLoopObserverIsFiring(CFRunLoopObserverRef rlo) {
    return (Boolean)__CFBitfieldValue(((CFRuntimeBase *)rlo)->_info, 0, 0);
}

CF_INLINE void __CFRunLoopObserverSetFiring(CFRunLoopObserverRef rlo) {
    __CFBitfieldSetValue(((CFRuntimeBase *)rlo)->_info, 0, 0, 1);
}

CF_INLINE void __CFRunLoopObserverUnsetFiring(CFRunLoopObserverRef rlo) {
    __CFBitfieldSetValue(((CFRuntimeBase *)rlo)->_info, 0, 0, 0);
}

CF_INLINE Boolean __CFRunLoopObserverRepeats(CFRunLoopObserverRef rlo) {
    return (Boolean)__CFBitfieldValue(((CFRuntimeBase *)rlo)->_info, 1, 1);
}

CF_INLINE void __CFRunLoopObserverSetRepeats(CFRunLoopObserverRef rlo) {
    __CFBitfieldSetValue(((CFRuntimeBase *)rlo)->_info, 1, 1, 1);
}

CF_INLINE void __CFRunLoopObserverUnsetRepeats(CFRunLoopObserverRef rlo) {
    __CFBitfieldSetValue(((CFRuntimeBase *)rlo)->_info, 1, 1, 0);
}

CF_INLINE void __CFRunLoopObserverLock(CFRunLoopObserverRef rlo) {
    __CFSpinLock((UInt32 *)&(rlo->_lock));
}

CF_INLINE void __CFRunLoopObserverUnlock(CFRunLoopObserverRef rlo) {
    __CFSpinUnlock((UInt32 *)&(rlo->_lock));
}

CF_INLINE void __CFRunLoopObserverSchedule(CFRunLoopObserverRef rlo, CFRunLoopRef rl, struct __CFRunLoopMode *rlm) {
    if (0 == rlo->_rlCount) {
	rlo->_runLoop = rl;
    }
    rlo->_rlCount++;
}

CF_INLINE void __CFRunLoopObserverCancel(CFRunLoopObserverRef rlo, CFRunLoopRef rl, struct __CFRunLoopMode *rlm) {
    rlo->_rlCount--;
    if (0 == rlo->_rlCount) {
	rlo->_runLoop = NULL;
    }
}

struct __CFRunLoopTimer {
    CFRuntimeBase _base;
    CFAllocatorRef _allocator;
    UInt32 _lock;
    CFRunLoopRef _runLoop;
    CFIndex _rlCount;
    mach_port_t _port;
    CFIndex _order;			/* immutable */
    CFAbsoluteTime _fireTime;
    CFTimeInterval _interval;		/* immutable; 0.0 means non-repeating */
    CFRunLoopTimerCallBack _callout;	/* immutable */
    CFRunLoopTimerContext _context;	/* immutable, except invalidation */
};

/* Bit 0 of the base reserved bits is used for firing state */
/* Bit 1 of the base reserved bits is used for has-fired state */

CF_INLINE Boolean __CFRunLoopTimerIsFiring(CFRunLoopTimerRef rlt) {
    return (Boolean)__CFBitfieldValue(((CFRuntimeBase *)rlt)->_info, 0, 0);
}

CF_INLINE void __CFRunLoopTimerSetFiring(CFRunLoopTimerRef rlt) {
    __CFBitfieldSetValue(((CFRuntimeBase *)rlt)->_info, 0, 0, 1);
}

CF_INLINE void __CFRunLoopTimerUnsetFiring(CFRunLoopTimerRef rlt) {
    __CFBitfieldSetValue(((CFRuntimeBase *)rlt)->_info, 0, 0, 0);
}

CF_INLINE void __CFRunLoopTimerLock(CFRunLoopTimerRef rlt) {
    __CFSpinLock((UInt32 *)&(rlt->_lock));
}

CF_INLINE void __CFRunLoopTimerUnlock(CFRunLoopTimerRef rlt) {
    __CFSpinUnlock((UInt32 *)&(rlt->_lock));
}

static UInt32 __CFRLTFTimeLock = 0;

CF_INLINE void __CFRunLoopTimerFireTimeLock(void) {
    __CFSpinLock(&__CFRLTFTimeLock);
}

CF_INLINE void __CFRunLoopTimerFireTimeUnlock(void) {
    __CFSpinUnlock(&__CFRLTFTimeLock);
}

CF_INLINE void __CFRunLoopTimerSchedule(CFRunLoopTimerRef rlt, CFRunLoopRef rl, struct __CFRunLoopMode *rlm) {
    if (0 == rlt->_rlCount) {
	rlt->_runLoop = rl;
    }
    rlt->_rlCount++;
    mach_port_insert_member(mach_task_self(), rlt->_port, rlm->_portSet);
    {
	    mach_timespec_t timeout;
	    CFTimeInterval delta;
	    delta = rlt->_fireTime - CFAbsoluteTimeGetCurrent();
	    if (delta < 0) delta = 0.0;
	    delta += 0.001;
	    if (7776000.0 < delta) delta = 7776000.0;
	    timeout.tv_sec = floor(delta);
	    timeout.tv_nsec = (delta - timeout.tv_sec) * 1000000000;
	    clock_alarm(clock_port, TIME_RELATIVE, timeout, rlt->_port);
    }
}

CF_INLINE void __CFRunLoopTimerCancel(CFRunLoopTimerRef rlt, CFRunLoopRef rl, struct __CFRunLoopMode *rlm) {
    rlt->_rlCount--;
    if (0 == rlt->_rlCount) {
	rlt->_runLoop = NULL;
    }
    if (__CFIsValid(rlt)) {
	mach_port_extract_member(mach_task_self(), rlt->_port, rlm->_portSet);
    }
}


/* CFRunLoop */

CF_DEFINECONSTANTSTRING(kCFDefaultRunLoopMode, "kCFRunLoopDefaultMode") // obsolete
CF_DEFINECONSTANTSTRING(kCFRunLoopDefaultMode, "kCFRunLoopDefaultMode")
CF_DEFINECONSTANTSTRING(kCFRunLoopAnyMode, "kCFRunLoopAnyMode")

#if defined(__MACH__)

struct _findsource {
    mach_port_t port;
    CFRunLoopSourceRef result;
};

static void ___CFRunLoopFindSource(const void *value, void *ctx) {
    CFRunLoopSourceRef rls = (CFRunLoopSourceRef)value;
    struct _findsource *context = (struct _findsource *)ctx;
    if (NULL != context->result) return;
    if (1 != rls->_context.version0.version) return;
    if (rls->_context.version1.getPort(rls->_context.version1.info) == context->port) {
	context->result = rls;
    }
}

// call with mode locked
CF_INLINE CFRunLoopSourceRef __CFRunLoopModeFindSourceForMachPort(struct __CFRunLoopMode *rlm, mach_port_t port) {	/* DOES CALLOUT */
    struct _findsource context = {port, NULL};
    if (NULL != rlm->_sources) {
	CFSetApplyFunction(rlm->_sources, ___CFRunLoopFindSource, &context);
    }
    return context.result;
}

struct _findtimer {
    mach_port_t port;
    CFRunLoopTimerRef result;
};

static void ___CFRunLoopFindTimer(const void *value, void *ctx) {
    CFRunLoopTimerRef rlt = (CFRunLoopTimerRef)value;
    struct _findtimer *context = (struct _findtimer *)ctx;
    if (NULL != context->result) return;
    if (rlt->_port == context->port) {
	context->result = rlt;
    }
}

// call with mode locked
CF_INLINE CFRunLoopTimerRef __CFRunLoopModeFindTimerForMachPort(struct __CFRunLoopMode *rlm, mach_port_t port) {
    struct _findtimer context = {port, NULL};
    if (NULL != rlm->_timers) {
	CFSetApplyFunction(rlm->_timers, ___CFRunLoopFindTimer, &context);
#warning CF: should eliminate this check after we get off clock_alarm and we can cancel outstanding timer requests
	if (NULL != context.result && CFAbsoluteTimeGetCurrent() - 0.001 < context.result->_fireTime) {
	    return NULL;
	}
    }
    return context.result;
}
#endif

CFTypeID CFRunLoopGetTypeID(void) {
    return __kCFRunLoopTypeID;
}

Boolean __CFRunLoopEqual(CFTypeRef cf1, CFTypeRef cf2) {
    CFRunLoopRef rl1 = (CFRunLoopRef)cf1;
    CFRunLoopRef rl2 = (CFRunLoopRef)cf2;
    return (rl1 == rl2);
}

CFHashCode __CFRunLoopHash(CFTypeRef cf) {
    CFRunLoopRef rl = (CFRunLoopRef)cf;
    return (CFHashCode)rl;
}

CFStringRef __CFRunLoopCopyDescription(CFTypeRef cf) {
#warning CF: unimplemented
    return NULL;
}

CFAllocatorRef __CFRunLoopGetAllocator(CFTypeRef cf) {
    CFRunLoopRef rl = (CFRunLoopRef)cf;
    return rl->_allocator;
}

void __CFRunLoopDeallocate(CFTypeRef cf) {
    CFRunLoopRef rl = (CFRunLoopRef)cf;
    CFAllocatorRef allocator = CFGetAllocator(rl);
    struct __CFRunLoopMode *mode;
    CFIndex idx, cnt;
    void **list, *buffer[256];

    /* We try to keep the run loop in a valid state as long as possible,
       since sources may have non-retained references to the run loop.
       Another reason is that we don't want to lock the run loop for
       callback reasons, if we can get away without that.  We start by
       eliminating the sources, since they are the most likely to call
       back into the run loop during their "cancellation". */
    for (mode = rl->_modes; NULL != mode; mode = mode->_next) {
	if (NULL == mode->_sources) continue;
	cnt = CFSetGetCount(mode->_sources);
	list = (cnt <= 256) ? buffer : CFAllocatorAllocate(allocator, cnt * sizeof(void *), 0);
	CFSetGetValues(mode->_sources, list);
	for (idx = 0; idx < cnt; idx++) {
	    CFRetain(list[idx]);
	}
	CFSetRemoveAllValues(mode->_sources);
	for (idx = 0; idx < cnt; idx++) {
	    __CFRunLoopSourceCancel((CFRunLoopSourceRef)list[idx], rl, mode);
	    CFRelease(list[idx]);
	}
	if (list != buffer) CFAllocatorDeallocate(allocator, list);
    }
    for (mode = rl->_modes; NULL != mode; mode = mode->_next) {
	if (NULL == mode->_observers) continue;
	cnt = CFSetGetCount(mode->_observers);
	list = (cnt <= 256) ? buffer : CFAllocatorAllocate(allocator, cnt * sizeof(void *), 0);
	CFSetGetValues(mode->_observers, list);
	for (idx = 0; idx < cnt; idx++) {
	    CFRetain(list[idx]);
	}
	CFSetRemoveAllValues(mode->_observers);
	for (idx = 0; idx < cnt; idx++) {
	    __CFRunLoopObserverCancel((CFRunLoopObserverRef)list[idx], rl, mode);
	    CFRelease(list[idx]);
	}
	if (list != buffer) CFAllocatorDeallocate(allocator, list);
    }
    for (mode = rl->_modes; NULL != mode; mode = mode->_next) {
	if (NULL == mode->_timers) continue;
	cnt = CFSetGetCount(mode->_timers);
	list = (cnt <= 256) ? buffer : CFAllocatorAllocate(allocator, cnt * sizeof(void *), 0);
	CFSetGetValues(mode->_timers, list);
	for (idx = 0; idx < cnt; idx++) {
	    CFRetain(list[idx]);
	}
	CFSetRemoveAllValues(mode->_timers);
	for (idx = 0; idx < cnt; idx++) {
	    __CFRunLoopTimerCancel((CFRunLoopTimerRef)list[idx], rl, mode);
	    CFRelease(list[idx]);
	}
	if (list != buffer) CFAllocatorDeallocate(allocator, list);
    }
    __CFRunLoopLock(rl);
    for (mode = rl->_modes; NULL != mode; mode = rl->_modes) {
	rl->_modes = mode->_next;
	if (NULL != mode->_sources) CFRelease(mode->_sources);	
	if (NULL != mode->_observers) CFRelease(mode->_observers);	
	if (NULL != mode->_timers) CFRelease(mode->_timers);	
	CFRelease(mode->_name);
#if defined(__MACH__)
	mach_port_destroy(mach_task_self(), mode->_portSet);
#endif
	CFAllocatorDeallocate(allocator, mode);
    }
#if defined(__MACH__)
    mach_port_destroy(mach_task_self(), rl->_waitPort);
    rl->_waitPort = 0;
#endif
#if defined(__WIN32__)
    CloseHandle(rl->_waitPort);
    rl->_waitPort = 0;
#endif
    __CFRunLoopUnlock(rl);
    CFAllocatorDeallocate(allocator, rl);
    CFRelease(allocator);
}

CFRunLoopRef CFRunLoopGetCurrent(void) {
    CFRunLoopRef currentLoop = __CFGetThreadSpecificData()->_runLoop;
    if (!currentLoop) {
	struct __CFRunLoopMode *rlm;
	CFAllocatorRef allocator = CFRetain(CFAllocatorGetDefault());
	currentLoop = CFAllocatorAllocate(allocator, sizeof(struct __CFRunLoop), 0);
	if (NULL == currentLoop) __CFAbort();
	__CFGenericInitBase(currentLoop, NULL, __kCFRunLoopTypeID);
	currentLoop->_allocator = allocator;
	__CFRunLoopUnsetStopped(currentLoop);
	currentLoop->_lock = 0;
#if defined(__MACH__)
	if (MACH_PORT_NULL == clock_port) {
	    host_get_clock_service(mach_host_self(), SYSTEM_CLOCK, &clock_port);
	}
	{
	    kern_return_t ret;
	    ret = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &(currentLoop->_waitPort));
	    if (KERN_SUCCESS == ret) {
		ret = mach_port_insert_right(mach_task_self(), currentLoop->_waitPort, currentLoop->_waitPort, MACH_MSG_TYPE_MAKE_SEND);
	    }
	    if (KERN_SUCCESS == ret) {
		mach_port_limits_t limits;
		limits.mpl_qlimit = 1;
		ret = mach_port_set_attributes(mach_task_self(), currentLoop->_waitPort, MACH_PORT_LIMITS_INFO, (mach_port_info_t)&limits, MACH_PORT_LIMITS_INFO_COUNT);
	    }
	    if (KERN_SUCCESS != ret) __CFAbort();
	}
#else
	currentLoop->_waitPort = CreateEvent(NULL, TRUE, FALSE, NULL);
#endif
	currentLoop->_currentMode = NULL;
	currentLoop->_modes = NULL;
	rlm = __CFRunLoopFindMode(currentLoop, kCFRunLoopDefaultMode, TRUE);
	__CFRunLoopModeUnlock(rlm);
	__CFGetThreadSpecificData()->_runLoop = currentLoop;
    }
    return currentLoop;
}

void _CFRunLoopSetCurrent(CFRunLoopRef rl) {
    CFRunLoopRef currentLoop = __CFGetThreadSpecificData()->_runLoop;
    if (rl != currentLoop) {
        if (rl) CFRetain(rl);
        if (currentLoop) CFRelease(currentLoop);
        __CFGetThreadSpecificData()->_runLoop = rl;
    }
}

CFStringRef CFRunLoopCopyCurrentMode(CFRunLoopRef rl) {
    CFStringRef result = NULL;
    __CFRunLoopLock(rl);
    if (NULL != rl->_currentMode) {
	result = CFRetain(rl->_currentMode->_name);
    }
    __CFRunLoopUnlock(rl);
    return result;
}

CFArrayRef CFRunLoopCopyAllModes(CFRunLoopRef rl) {
    struct __CFRunLoopMode *curMode;
    CFIndex cnt;
    CFMutableArrayRef array;
    __CFRunLoopLock(rl);
    for (curMode = rl->_modes, cnt = 0; curMode; curMode = curMode->_next) {
	cnt++;
    }
    array = CFArrayCreateMutable(CFGetAllocator(rl), cnt, &kCFTypeArrayCallBacks);
    for (curMode = rl->_modes; curMode; curMode = curMode->_next) {
	CFArrayAppendValue(array, curMode->_name);
    }
    __CFRunLoopUnlock(rl);
    return array;
}

enum {
    __kCFRunLoopRunHandled = 8
};

static CFComparisonResult __CFRunLoopObserverComparator(const void *val1, const void *val2, void *context) {
    CFRunLoopObserverRef o1 = (CFRunLoopObserverRef)val1;
    CFRunLoopObserverRef o2 = (CFRunLoopObserverRef)val2;
    if (o1->_order < o2->_order) return kCFCompareLessThan;
    if (o2->_order < o1->_order) return kCFCompareGreaterThan;
    return kCFCompareEqualTo;
}

/* rl is unlocked, theMode is locked on entrance and exit */
static void __CFRunLoopDoObservers(CFRunLoopRef rl, struct __CFRunLoopMode *theMode, CFRunLoopActivity activity) {	/* DOES CALLOUT */
    CFIndex idx, cnt;
    void **list, *buffer[256];

    /* Fire the observers */
    if (NULL == theMode->_observers) {
	return;
    }
    cnt = CFSetGetCount(theMode->_observers);
    list = (cnt <= 256) ? buffer : CFAllocatorAllocate(CFGetAllocator(rl), cnt * sizeof(void *), 0);
    CFSetGetValues(theMode->_observers, list);
    for (idx = 0; idx < cnt; idx++) {
	CFRetain(list[idx]);
    }
    __CFRunLoopModeUnlock(theMode);
    CFQSortArray(list, cnt, sizeof(CFRunLoopObserverRef), __CFRunLoopObserverComparator, NULL);
    for (idx = 0; idx < cnt; idx++) {
	CFRunLoopObserverRef rlo = list[idx];
	if (0 != (rlo->_activities & activity)) {
	    __CFRunLoopObserverLock(rlo);
	    if (__CFIsValid(rlo) && !__CFRunLoopObserverIsFiring(rlo)) {
		__CFRunLoopObserverUnlock(rlo);
		__CFRunLoopObserverSetFiring(rlo);
		rlo->_callout(rlo, activity, rlo->_context.info);	/* CALLOUT */
		__CFRunLoopObserverUnsetFiring(rlo);
		if (!__CFRunLoopObserverRepeats(rlo))
		    CFRunLoopObserverInvalidate(rlo);
	    } else {
		__CFRunLoopObserverUnlock(rlo);
	    }
	}
	if (!__CFIsValid(rlo)) {
	    __CFRunLoopModeLock(theMode);
	    if (CFSetContainsValue(theMode->_observers, rlo)) {
		CFSetRemoveValue(theMode->_observers, rlo);
		__CFRunLoopModeUnlock(theMode);
		__CFRunLoopObserverCancel(rlo, rl, theMode);	/* DOES CALLOUT */
	    } else if (NULL != theMode) {
		__CFRunLoopModeUnlock(theMode);
	    }
	}
    }
    for (idx = 0; idx < cnt; idx++) {
	CFRelease(list[idx]);
    }
    if (list != buffer) CFAllocatorDeallocate(CFGetAllocator(rl), list);
    __CFRunLoopModeLock(theMode);
}

static CFComparisonResult __CFRunLoopSourceComparator(const void *val1, const void *val2, void *context) {
    CFRunLoopSourceRef o1 = (CFRunLoopSourceRef)val1;
    CFRunLoopSourceRef o2 = (CFRunLoopSourceRef)val2;
    if (o1->_order < o2->_order) return kCFCompareLessThan;
    if (o2->_order < o1->_order) return kCFCompareGreaterThan;
    return kCFCompareEqualTo;
}

/* rl is unlocked, theMode is locked on entrance and exit */
static Boolean __CFRunLoopDoSources0(CFRunLoopRef rl, struct __CFRunLoopMode *theMode, Boolean stopAfterHandle) {	/* DOES CALLOUT */
    CFIndex idx, cnt;
    void **list, *buffer[256];
    Boolean sourceHandled = FALSE;

    /* Fire the version 0 sources */
    if (NULL == theMode->_sources) {
	return FALSE;
    }
    cnt = CFSetGetCount(theMode->_sources);
    list = (cnt <= 256) ? buffer : CFAllocatorAllocate(CFGetAllocator(rl), cnt * sizeof(void *), 0);
    CFSetGetValues(theMode->_sources, list);
    for (idx = 0; idx < cnt; idx++) {
	CFRetain(list[idx]);
    }
    __CFRunLoopModeUnlock(theMode);
    CFQSortArray(list, cnt, sizeof(CFRunLoopSourceRef), __CFRunLoopSourceComparator, NULL);
    for (idx = 0; idx < cnt; idx++) {
	CFRunLoopSourceRef rls = list[idx];
	if (!stopAfterHandle || (stopAfterHandle && !sourceHandled)) {
	    __CFRunLoopSourceLock(rls);
	    if (__CFIsValid(rls) && __CFRunLoopSourceIsSignaled(rls) && 0 == rls->_context.version0.version) {
		__CFRunLoopSourceUnsetSignaled(rls);
		__CFRunLoopSourceUnlock(rls);
		if (NULL != rls->_context.version0.perform) {
		    rls->_context.version0.perform(rls->_context.version0.info); /* CALLOUT */
		}
		sourceHandled = TRUE;
#warning CF: should round-robin amonst equal-order input sources
	    } else {
		__CFRunLoopSourceUnlock(rls);
	    }
	}
	if (!__CFIsValid(rls)) {
	    __CFRunLoopModeLock(theMode);
	    if (CFSetContainsValue(theMode->_sources, rls)) {
		CFSetRemoveValue(theMode->_sources, rls);
		__CFRunLoopModeUnlock(theMode);
		__CFRunLoopSourceCancel(rls, rl, theMode);	/* DOES CALLOUT */
	    } else if (NULL != theMode) {
		__CFRunLoopModeUnlock(theMode);
	    }
	}
    }
    for (idx = 0; idx < cnt; idx++) {
	CFRelease(list[idx]);
    }
    if (list != buffer) CFAllocatorDeallocate(CFGetAllocator(rl), list);
    __CFRunLoopModeLock(theMode);
    return sourceHandled;
}

static Boolean __CFRunLoopDoSource1(CFRunLoopRef rl, struct __CFRunLoopMode *theMode, CFRunLoopSourceRef rls, mach_msg_header_t *msg, CFIndex size, mach_msg_header_t **reply) {	/* DOES CALLOUT */
    Boolean sourceHandled = FALSE;

    /* Fire a version 1 source */
    CFRetain(rls);
    __CFRunLoopModeUnlock(theMode);
    __CFRunLoopSourceLock(rls);
    if (__CFIsValid(rls)) {
	__CFRunLoopSourceUnsetSignaled(rls);
	__CFRunLoopSourceUnlock(rls);
	if (NULL != rls->_context.version1.perform) {
	    *reply = rls->_context.version1.perform(msg, size, kCFAllocatorSystemDefault, rls->_context.version1.info); /* CALLOUT */
	}
	sourceHandled = TRUE;
    } else {
	__CFRunLoopSourceUnlock(rls);
    }
    if (!__CFIsValid(rls)) {
	__CFRunLoopModeLock(theMode);
	if (NULL != theMode->_sources && CFSetContainsValue(theMode->_sources, rls)) {
	    CFSetRemoveValue(theMode->_sources, rls);
	    __CFRunLoopModeUnlock(theMode);
	    __CFRunLoopSourceCancel(rls, rl, theMode);	/* DOES CALLOUT */
	} else if (NULL != theMode) {
	    __CFRunLoopModeUnlock(theMode);
	}
    }
    CFRelease(rls);
    __CFRunLoopModeLock(theMode);
    return sourceHandled;
}

static Boolean __CFRunLoopDoTimer(CFRunLoopRef rl, struct __CFRunLoopMode *theMode, CFRunLoopTimerRef rlt) {	/* DOES CALLOUT */
    Boolean timerHandled = FALSE;

    /* Fire a timer */
    CFRetain(rlt);
    __CFRunLoopModeUnlock(theMode);
    __CFRunLoopTimerLock(rlt);
    if (__CFIsValid(rlt) && !__CFRunLoopTimerIsFiring(rlt)) {
	__CFRunLoopTimerSetFiring(rlt);
	__CFRunLoopTimerUnlock(rlt);
	rlt->_callout(rlt, rlt->_context.info);
	__CFRunLoopTimerUnsetFiring(rlt);
	timerHandled = TRUE;
    } else {
	__CFRunLoopTimerUnlock(rlt);
    }
    if (__CFIsValid(rlt)) {
	if (0.0 == rlt->_interval) {
	    CFRunLoopTimerInvalidate(rlt);	/* DOES CALLOUT */
	} else {
	    mach_timespec_t timeout;
	    CFTimeInterval delta;
	    CFAbsoluteTime currentTime = CFAbsoluteTimeGetCurrent();
	    __CFRunLoopTimerFireTimeLock();
	    while (rlt->_fireTime <= currentTime) {
		rlt->_fireTime += rlt->_interval;
	    }
	    delta = rlt->_fireTime - currentTime;
	    __CFRunLoopTimerFireTimeUnlock();
	    if (delta < 0) delta = 0.0;
	    delta += 0.001;
	    if (7776000.0 < delta) delta = 7776000.0;
	    timeout.tv_sec = floor(delta);
	    timeout.tv_nsec = (delta - timeout.tv_sec) * 1000000000;
	    clock_alarm(clock_port, TIME_RELATIVE, timeout, rlt->_port);
	}
    }
    if (!__CFIsValid(rlt)) {
	__CFRunLoopModeLock(theMode);
	if (NULL != theMode->_timers && CFSetContainsValue(theMode->_timers, rlt)) {
	    CFSetRemoveValue(theMode->_timers, rlt);
	    __CFRunLoopModeUnlock(theMode);
	    __CFRunLoopTimerCancel(rlt, rl, theMode);	/* DOES CALLOUT */
	} else if (NULL != theMode) {
	    __CFRunLoopModeUnlock(theMode);
	}
    }
    CFRelease(rlt);
    __CFRunLoopModeLock(theMode);
    return timerHandled;
}

CF_EXPORT Boolean _CFRunLoopFinished(CFRunLoopRef rl, CFStringRef mode) {
    struct __CFRunLoopMode *currentMode;
    Boolean result = FALSE;
    __CFRunLoopLock(rl);
    currentMode = __CFRunLoopFindMode(rl, mode, FALSE);
    if (!currentMode || __CFRunLoopModeIsEmpty(rl, currentMode)) {
	result = TRUE;
    }
    __CFRunLoopUnlock(rl);
    __CFRunLoopModeUnlock(currentMode);
    return result;
}

/* rl is unlocked, currentMode locked on entrance and exit */
static SInt32 __CFRunLoopRun(CFRunLoopRef rl, struct __CFRunLoopMode *currentMode, CFTimeInterval seconds, Boolean stopAfterHandle) {  /* DOES CALLOUT */
    CFAbsoluteTime termTime = CFAbsoluteTimeGetCurrent() + seconds;
    if (0.0 < seconds) {
	mach_timespec_t timeout;
	seconds += 0.001;
	if (7776000.0 < seconds) seconds = 7776000.0;
	timeout.tv_sec = floor(seconds);
	timeout.tv_nsec = (seconds - timeout.tv_sec) * 1000000000;
	clock_alarm(clock_port, TIME_RELATIVE, timeout, rl->_waitPort);
    }
    for (;;) {
	mach_msg_header_t *msg, *reply;
	kern_return_t ret;
	Boolean sourceHandled;
	CFRunLoopSourceRef rls;
	CFRunLoopTimerRef rlt;

	if (__CFRunLoopModeIsEmpty(rl, currentMode)) {
	    return kCFRunLoopRunFinished;
	}

	__CFRunLoopDoObservers(rl, currentMode, kCFRunLoopBeforeTimers);

    	__CFRunLoopDoObservers(rl, currentMode, kCFRunLoopBeforeSources);

    	sourceHandled = __CFRunLoopDoSources0(rl, currentMode, stopAfterHandle);

	/* Check ending conditions */
	if (sourceHandled && stopAfterHandle) {
	    return __kCFRunLoopRunHandled;
	}
	if (termTime <= CFAbsoluteTimeGetCurrent()) {
	    return kCFRunLoopRunTimedOut;
	}
	if (__CFRunLoopIsStopped(rl)) {
	    return kCFRunLoopRunStopped;
	}
	if ( __CFRunLoopModeIsEmpty(rl, currentMode)) {
	    return kCFRunLoopRunFinished;
	}

	/* In that sleep of death what nightmares may come ... */
	reply = NULL;
	for (;;) {
	    if (NULL != reply) {
		ret = mach_msg(reply, MACH_SEND_MSG, reply->msgh_size, 0, MACH_PORT_NULL, 0, MACH_PORT_NULL);
#warning CF: what should be done with the return value?
		CFAllocatorDeallocate(kCFAllocatorSystemDefault, reply);
		reply = NULL;
	    }
	    if (__CFRunLoopIsStopped(rl)) {	/* Not thread-safe */
		return kCFRunLoopRunStopped;
	    }
	    if ( __CFRunLoopModeIsEmpty(rl, currentMode)) {
		return kCFRunLoopRunFinished;
	    }
	    __CFRunLoopDoObservers(rl, currentMode, kCFRunLoopBeforeWaiting);
	    __CFRunLoopModeUnlock(currentMode);
	    __CFRunLoopSetSleeping(rl);
	    msg = CFAllocatorAllocate(kCFAllocatorSystemDefault, 1024, 0);
	    msg->msgh_size = 1024;

	    try_receive:
	    msg->msgh_bits = 0;
	    msg->msgh_local_port = currentMode->_portSet;
	    msg->msgh_remote_port = MACH_PORT_NULL;
	    msg->msgh_id = 0;

	    ret = mach_msg(msg, MACH_RCV_MSG|MACH_RCV_LARGE, 0, msg->msgh_size, currentMode->_portSet, 0, MACH_PORT_NULL);

	    if (MACH_RCV_TOO_LARGE == ret) {
		msg->msgh_size += sizeof(mach_msg_trailer_t);
		msg = CFAllocatorReallocate(kCFAllocatorSystemDefault, msg, msg->msgh_size, 0);
		goto try_receive;
	    } else if (MACH_MSG_SUCCESS != ret) {
		__CFAbort();
	    }

	    __CFRunLoopUnsetSleeping(rl);
	    __CFRunLoopModeLock(currentMode);
	    __CFRunLoopDoObservers(rl, currentMode, kCFRunLoopAfterWaiting);
	    rls = __CFRunLoopModeFindSourceForMachPort(currentMode, msg->msgh_local_port);
	    if (NULL != rls) {
		__CFRunLoopDoSource1(rl, currentMode, rls, msg, msg->msgh_size, &reply);
	    } else {
		rlt = __CFRunLoopModeFindTimerForMachPort(currentMode, msg->msgh_local_port);
		if (NULL != rlt) {
		    __CFRunLoopDoTimer(rl, currentMode, rlt);
		} else {
		    CFAllocatorDeallocate(kCFAllocatorSystemDefault, msg);
		    break;
		}
	    }
	    CFAllocatorDeallocate(kCFAllocatorSystemDefault, msg);

	    __CFRunLoopDoObservers(rl, currentMode, kCFRunLoopBeforeTimers);
	    __CFRunLoopDoObservers(rl, currentMode, kCFRunLoopBeforeSources);
	}
    }
}

void CFRunLoopRun(void) {	/* DOES CALLOUT */
    struct __CFRunLoopMode *currentMode, *previousMode;
    CFRunLoopRef rl = CFRunLoopGetCurrent();
    SInt32 result;

    __CFRunLoopLock(rl);
    currentMode = __CFRunLoopFindMode(rl, kCFRunLoopDefaultMode, FALSE);
    if (!currentMode) {
	__CFRunLoopUnlock(rl);
	return;
    }
    previousMode = rl->_currentMode;
    rl->_currentMode = currentMode;
    __CFRunLoopUnsetStopped(rl);
    __CFRunLoopUnlock(rl);
    do {
	__CFRunLoopDoObservers(rl, currentMode, kCFRunLoopEntry);
	result = __CFRunLoopRun(rl, currentMode, 1.0e8, FALSE);
	__CFRunLoopDoObservers(rl, currentMode, kCFRunLoopExit);
    } while (kCFRunLoopRunStopped != result && kCFRunLoopRunFinished != result);
    __CFRunLoopModeUnlock(currentMode);
    __CFRunLoopLock(rl);
    rl->_currentMode = previousMode;
    __CFRunLoopUnlock(rl);
}

Boolean CFRunLoopRunInMode(CFStringRef mode, CFTimeInterval seconds) {	/* DOES CALLOUT */
    struct __CFRunLoopMode *currentMode, *previousMode;
    CFRunLoopRef rl = CFRunLoopGetCurrent();
    SInt32 result;

    __CFRunLoopLock(rl);
    currentMode = __CFRunLoopFindMode(rl, mode, FALSE);
    if (!currentMode) {
	__CFRunLoopUnlock(rl);
	return FALSE;
    }
    previousMode = rl->_currentMode;
    rl->_currentMode = currentMode;
    __CFRunLoopUnsetStopped(rl);
    __CFRunLoopUnlock(rl);
    __CFRunLoopDoObservers(rl, currentMode, kCFRunLoopEntry);
    result = __CFRunLoopRun(rl, currentMode, seconds, TRUE);
    __CFRunLoopDoObservers(rl, currentMode, kCFRunLoopExit);
    __CFRunLoopModeUnlock(currentMode);
    __CFRunLoopLock(rl);
    rl->_currentMode = previousMode;
    __CFRunLoopUnlock(rl);
    return (__kCFRunLoopRunHandled == result) ? TRUE : FALSE;
}

SInt32 CFRunLoopRunInMode_new(CFStringRef mode, CFTimeInterval seconds) {     /* DOES CALLOUT */
    struct __CFRunLoopMode *currentMode, *previousMode;
    CFRunLoopRef rl = CFRunLoopGetCurrent();
    SInt32 result;

    __CFRunLoopLock(rl);
    currentMode = __CFRunLoopFindMode(rl, mode, FALSE);
    if (!currentMode) {
	__CFRunLoopUnlock(rl);
	return kCFRunLoopRunFinished;
    }
    previousMode = rl->_currentMode;
    rl->_currentMode = currentMode;
    __CFRunLoopUnsetStopped(rl);
    __CFRunLoopUnlock(rl);
    __CFRunLoopDoObservers(rl, currentMode, kCFRunLoopEntry);
    result = __CFRunLoopRun(rl, currentMode, seconds, FALSE);
    __CFRunLoopDoObservers(rl, currentMode, kCFRunLoopExit);
    __CFRunLoopModeUnlock(currentMode);
    __CFRunLoopLock(rl);
    rl->_currentMode = previousMode;
    __CFRunLoopUnlock(rl);
    if (result == __kCFRunLoopRunHandled) return kCFRunLoopRunFinished;
    return result;
}

CFDateRef CFRunLoopCopyNextTimerFireDate(CFRunLoopRef rl, CFStringRef mode) {
    struct __CFRunLoopMode *theMode;
    CFDateRef result = NULL;
    __CFRunLoopLock(rl);
    theMode = __CFRunLoopFindMode(rl, mode, FALSE);
    __CFRunLoopUnlock(rl);
    if (theMode) {
#warning CF: unimplemented
	__CFRunLoopModeUnlock(theMode);
    }
    return result;
}

CFAbsoluteTime CFRunLoopGetNextTimerFireDate(CFRunLoopRef rl, CFStringRef mode) {
    struct __CFRunLoopMode *theMode;
    CFAbsoluteTime at = 0.0;
    __CFRunLoopLock(rl);
    theMode = __CFRunLoopFindMode(rl, mode, FALSE);
    __CFRunLoopUnlock(rl);
    if (theMode) {
#warning CF: unimplemented
	__CFRunLoopModeUnlock(theMode);
    }
    return at;
}

Boolean CFRunLoopIsWaiting(CFRunLoopRef rl) {
    return __CFRunLoopIsSleeping(rl);
}

void CFRunLoopWakeUp(CFRunLoopRef rl) {
#if defined(__MACH__)
    kern_return_t ret;
    /* We unconditionally try to send the message, since we don't want
     * to lose a wakeup, but the send may fail if there is already a
     * wakeup pending, since the queue length is 1. */
    ret = __CFSendTrivialMachMessage(rl->_waitPort, 0, MACH_SEND_TIMEOUT, 0);
    if (ret != MACH_MSG_SUCCESS && ret != MACH_SEND_TIMED_OUT) {
	__CFAbort();
    }
#else
    SetEvent(rl->_waitPort);
#endif
}

void CFRunLoopAbort(CFRunLoopRef rl) {
    CFRunLoopStop(rl);
}

void CFRunLoopStop(CFRunLoopRef rl) {
    __CFRunLoopLock(rl);
    __CFRunLoopSetStopped(rl);
    __CFRunLoopUnlock(rl);
    CFRunLoopWakeUp(rl);
}

Boolean CFRunLoopContainsSource(CFRunLoopRef rl, CFRunLoopSourceRef rls, CFStringRef mode) {
    struct __CFRunLoopMode *theMode;
    Boolean hasValue = FALSE;
    __CFRunLoopLock(rl);
    theMode = __CFRunLoopFindMode(rl, mode, FALSE);
    __CFRunLoopUnlock(rl);
    if (NULL != theMode && NULL != theMode->_sources) {
	hasValue = CFSetContainsValue(theMode->_sources, rls);
	__CFRunLoopModeUnlock(theMode);
    }
    return hasValue;
}

void CFRunLoopAddSource(CFRunLoopRef rl, CFRunLoopSourceRef rls, CFStringRef mode) {	/* DOES CALLOUT */
    struct __CFRunLoopMode *theMode;
    if (!__CFIsValid(rls)) return;
    __CFRunLoopLock(rl);
    theMode = __CFRunLoopFindMode(rl, mode, TRUE);
    __CFRunLoopUnlock(rl);
    if (NULL != theMode && NULL == theMode->_sources) {
	theMode->_sources = CFSetCreateMutable(CFGetAllocator(rl), 0, &kCFTypeSetCallBacks);
    }
    if (NULL != theMode && !CFSetContainsValue(theMode->_sources, rls)) {
	CFSetAddValue(theMode->_sources, rls);
	__CFRunLoopModeUnlock(theMode);
	__CFRunLoopSourceSchedule(rls, rl, theMode);	/* DOES CALLOUT */
    } else if (NULL != theMode) {
	__CFRunLoopModeUnlock(theMode);
    }
}

void CFRunLoopRemoveSource(CFRunLoopRef rl, CFRunLoopSourceRef rls, CFStringRef mode) {	/* DOES CALLOUT */
    struct __CFRunLoopMode *theMode;
    __CFRunLoopLock(rl);
    theMode = __CFRunLoopFindMode(rl, mode, FALSE);
    __CFRunLoopUnlock(rl);
    if (NULL != theMode && NULL != theMode->_sources && CFSetContainsValue(theMode->_sources, rls)) {
	CFRetain(rls);
	CFSetRemoveValue(theMode->_sources, rls);
	__CFRunLoopModeUnlock(theMode);
	__CFRunLoopSourceCancel(rls, rl, theMode);	/* DOES CALLOUT */
	CFRelease(rls);
    } else if (NULL != theMode) {
	__CFRunLoopModeUnlock(theMode);
    }
}

Boolean CFRunLoopContainsObserver(CFRunLoopRef rl, CFRunLoopObserverRef rlo, CFStringRef mode) {
    struct __CFRunLoopMode *theMode;
    Boolean hasValue = FALSE;
    __CFRunLoopLock(rl);
    theMode = __CFRunLoopFindMode(rl, mode, FALSE);
    __CFRunLoopUnlock(rl);
    if (NULL != theMode && NULL != theMode->_observers) {
	hasValue = CFSetContainsValue(theMode->_observers, rlo);
	__CFRunLoopModeUnlock(theMode);
    }
    return hasValue;
}

void CFRunLoopAddObserver(CFRunLoopRef rl, CFRunLoopObserverRef rlo, CFStringRef mode) {
    struct __CFRunLoopMode *theMode;
    if (!__CFIsValid(rlo) || (NULL != rlo->_runLoop && rlo->_runLoop != rl)) return;
    __CFRunLoopLock(rl);
    theMode = __CFRunLoopFindMode(rl, mode, TRUE);
    __CFRunLoopUnlock(rl);
    if (NULL != theMode && NULL == theMode->_observers) {
	theMode->_observers = CFSetCreateMutable(CFGetAllocator(rl), 0, &kCFTypeSetCallBacks);
    }
    if (NULL != theMode && !CFSetContainsValue(theMode->_observers, rlo)) {
	CFSetAddValue(theMode->_observers, rlo);
	__CFRunLoopModeUnlock(theMode);
	__CFRunLoopObserverSchedule(rlo, rl, theMode);
    } else if (NULL != theMode) {
	__CFRunLoopModeUnlock(theMode);
    }
}

void CFRunLoopRemoveObserver(CFRunLoopRef rl, CFRunLoopObserverRef rlo, CFStringRef mode) {
    struct __CFRunLoopMode *theMode;
    __CFRunLoopLock(rl);
    theMode = __CFRunLoopFindMode(rl, mode, FALSE);
    __CFRunLoopUnlock(rl);
    if (NULL != theMode && NULL != theMode->_observers && CFSetContainsValue(theMode->_observers, rlo)) {
	CFRetain(rlo);
	CFSetRemoveValue(theMode->_observers, rlo);
	__CFRunLoopModeUnlock(theMode);
	__CFRunLoopObserverCancel(rlo, rl, theMode);
	CFRelease(rlo);
    } else if (NULL != theMode) {
	__CFRunLoopModeUnlock(theMode);
    }
}

Boolean CFRunLoopContainsTimer(CFRunLoopRef rl, CFRunLoopTimerRef rlt, CFStringRef mode) {
    struct __CFRunLoopMode *theMode;
    Boolean hasValue = FALSE;
    __CFRunLoopLock(rl);
    theMode = __CFRunLoopFindMode(rl, mode, FALSE);
    __CFRunLoopUnlock(rl);
    if (NULL != theMode && NULL != theMode->_timers) {
	hasValue = CFSetContainsValue(theMode->_timers, rlt);
	__CFRunLoopModeUnlock(theMode);
    }
    return hasValue;
}

void CFRunLoopAddTimer(CFRunLoopRef rl, CFRunLoopTimerRef rlt, CFStringRef mode) {
    struct __CFRunLoopMode *theMode;
    if (!__CFIsValid(rlt) || (NULL != rlt->_runLoop && rlt->_runLoop != rl)) return;
    __CFRunLoopLock(rl);
    theMode = __CFRunLoopFindMode(rl, mode, TRUE);
    __CFRunLoopUnlock(rl);
    if (NULL != theMode && NULL == theMode->_timers) {
	theMode->_timers = CFSetCreateMutable(CFGetAllocator(rl), 0, &kCFTypeSetCallBacks);
    }
    if (NULL != theMode && !CFSetContainsValue(theMode->_timers, rlt)) {
	CFSetAddValue(theMode->_timers, rlt);
	__CFRunLoopModeUnlock(theMode);
	__CFRunLoopTimerSchedule(rlt, rl, theMode);
    } else if (NULL != theMode) {
	__CFRunLoopModeUnlock(theMode);
    }
}

void CFRunLoopRemoveTimer(CFRunLoopRef rl, CFRunLoopTimerRef rlt, CFStringRef mode) {
    struct __CFRunLoopMode *theMode;
    __CFRunLoopLock(rl);
    theMode = __CFRunLoopFindMode(rl, mode, FALSE);
    __CFRunLoopUnlock(rl);
    if (NULL != theMode && NULL != theMode->_timers && CFSetContainsValue(theMode->_timers, rlt)) {
	CFRetain(rlt);
	CFSetRemoveValue(theMode->_timers, rlt);
	__CFRunLoopModeUnlock(theMode);
	__CFRunLoopTimerCancel(rlt, rl, theMode);
	CFRelease(rlt);
    } else if (NULL != theMode) {
	__CFRunLoopModeUnlock(theMode);
    }
}


/* CFRunLoopSource */

CFTypeID CFRunLoopSourceGetTypeID(void) {
    return __kCFRunLoopSourceTypeID;
}

Boolean __CFRunLoopSourceEqual(CFTypeRef cf1, CFTypeRef cf2) {	/* DOES CALLOUT */
    CFRunLoopSourceRef rls1 = (CFRunLoopSourceRef)cf1;
    CFRunLoopSourceRef rls2 = (CFRunLoopSourceRef)cf2;
    if (rls1 == rls2) return TRUE;
    if (rls1->_order != rls2->_order) return FALSE;
    if (rls1->_context.version0.version != rls2->_context.version0.version) return FALSE;
    if (rls1->_context.version0.hash != rls2->_context.version0.hash) return FALSE;
    if (rls1->_context.version0.equal != rls2->_context.version0.equal) return FALSE;
    if (rls1->_context.version0.equal)
	return rls1->_context.version0.equal(rls1->_context.version0.info, rls2->_context.version0.info);
    return TRUE;
}

CFHashCode __CFRunLoopSourceHash(CFTypeRef cf) {	/* DOES CALLOUT */
    CFRunLoopSourceRef rls = (CFRunLoopSourceRef)cf;
    if (rls->_context.version0.hash)
	return rls->_context.version0.hash(rls->_context.version0.info);
    return (CFHashCode)rls;
}

CFStringRef __CFRunLoopSourceCopyDescription(CFTypeRef cf) {	/* DOES CALLOUT */
    CFRunLoopSourceRef rls = (CFRunLoopSourceRef)cf;
    CFStringRef result;
    CFStringRef contextDesc = NULL;
    if (NULL != rls->_context.version0.copyDescription) {
	contextDesc = rls->_context.version0.copyDescription(rls->_context.version0.info);
    }
    if (NULL == contextDesc) {
	contextDesc = CFStringCreateWithFormat(CFGetAllocator(rls), NULL, CFSTR("<CFRunLoopSource context 0x%x>"), rls->_context.version0.info);
    }
    result = CFStringCreateWithFormat(CFGetAllocator(rls), NULL, CFSTR("<CFRunLoopSource 0x%x [0x%x]>{locked = %s, valid = %s, order = %d, context = %@}"), (UInt32)cf, (UInt32)CFGetAllocator(rls), rls->_lock ? "Yes" : "No", __CFIsValid(rls) ? "Yes" : "No", rls->_order, contextDesc);
    CFRelease(contextDesc);
    return result;
}

CFAllocatorRef __CFRunLoopSourceGetAllocator(CFTypeRef cf) {
    CFRunLoopSourceRef rls = (CFRunLoopSourceRef)cf;
    return rls->_allocator;
}

void __CFRunLoopSourceDeallocate(CFTypeRef cf) {	/* DOES CALLOUT */
    CFRunLoopSourceRef rls = (CFRunLoopSourceRef)cf;
    CFAllocatorRef allocator = CFGetAllocator(rls);
    CFRunLoopSourceInvalidate(rls);
    if (rls->_context.version0.release) {
	rls->_context.version0.release(rls->_context.version0.info);
    }
    CFAllocatorDeallocate(allocator, rls);
    CFRelease(allocator);
}

CFRunLoopSourceRef CFRunLoopSourceCreate(CFAllocatorRef allocator, CFIndex order, CFRunLoopSourceContext *context) {
    CFRunLoopSourceRef memory;
    UInt32 size;
    if (NULL == context) __CFAbort();
    size = sizeof(struct __CFRunLoopSource);
    allocator = (NULL == allocator) ? CFRetain(CFAllocatorGetDefault()) : CFRetain(allocator);
    memory = CFAllocatorAllocate(allocator, size, 0);
    if (NULL == memory) {
	CFRelease(allocator);
	return NULL;
    }
    __CFGenericInitBase(memory, NULL, __kCFRunLoopSourceTypeID);
    memory->_allocator = allocator;
    __CFSetValid(memory);
    __CFRunLoopSourceUnsetSignaled(memory);
    memory->_lock = 0;
    memory->_order = order;
    memmove(&memory->_context, context, (0 == context->version) ? sizeof(CFRunLoopSourceContext) : sizeof(CFRunLoopSourceContext1));
    if (context->retain) {
	memory->_context.version0.info = (void *)context->retain(context->info);
    }
    return memory;
}

CFIndex CFRunLoopSourceGetOrder(CFRunLoopSourceRef rls) {
    __CFGenericValidateType(rls, __kCFRunLoopSourceTypeID);
    return rls->_order;
}

void CFRunLoopSourceInvalidate(CFRunLoopSourceRef rls) {
    __CFGenericValidateType(rls, __kCFRunLoopSourceTypeID);
    __CFRunLoopSourceLock(rls);
    if (__CFIsValid(rls)) {
	__CFUnsetValid(rls);
	/* for hashing- and equality-use purposes, can't actually release the context here */
    }
    __CFRunLoopSourceUnlock(rls);
}

Boolean CFRunLoopSourceIsValid(CFRunLoopSourceRef rls) {
    __CFGenericValidateType(rls, __kCFRunLoopSourceTypeID);
    return __CFIsValid(rls);
}

void CFRunLoopSourceGetContext(CFRunLoopSourceRef rls, CFRunLoopSourceContext *context) {
    __CFGenericValidateType(rls, __kCFRunLoopSourceTypeID);
#if defined(__MACH__)
    CFAssert1(0 == context->version || 1 == context->version, __kCFLogAssertion, "%s(): context version not initialized to 0 or 1", __PRETTY_FUNCTION__);
#else
    CFAssert1(0 == context->version, __kCFLogAssertion, "%s(): context version not initialized to 0", __PRETTY_FUNCTION__);
#endif
    memmove(context, &rls->_context, (0 == context->version) ? sizeof(CFRunLoopSourceContext) : sizeof(CFRunLoopSourceContext1));
}

void CFRunLoopSourceSignal(CFRunLoopSourceRef rls) {
    __CFRunLoopSourceLock(rls);
    if (__CFIsValid(rls)) {
	__CFRunLoopSourceSetSignaled(rls);
    }
    __CFRunLoopSourceUnlock(rls);
}


/* CFRunLoopObserver */

CFTypeID CFRunLoopObserverGetTypeID(void) {
    return __kCFRunLoopObserverTypeID;
}

Boolean __CFRunLoopObserverEqual(CFTypeRef cf1, CFTypeRef cf2) {
    CFRunLoopObserverRef rlo1 = (CFRunLoopObserverRef)cf1;
    CFRunLoopObserverRef rlo2 = (CFRunLoopObserverRef)cf2;
    if (rlo1 == rlo2) return TRUE;
    if (rlo1->_activities != rlo2->_activities) return FALSE;
    if (rlo1->_order != rlo2->_order) return FALSE;
    if (rlo1->_callout != rlo2->_callout) return FALSE;
    return TRUE;
}

CFHashCode __CFRunLoopObserverHash(CFTypeRef cf) {
    CFRunLoopObserverRef rlo = (CFRunLoopObserverRef)cf;
    return (CFHashCode)rlo;
}

CFStringRef __CFRunLoopObserverCopyDescription(CFTypeRef cf) {	/* DOES CALLOUT */
    CFRunLoopObserverRef rlo = (CFRunLoopObserverRef)cf;
    CFStringRef result;
    CFStringRef contextDesc = NULL;
    __CFRunLoopObserverLock(rlo);
    if (NULL != rlo->_context.copyDescription) {
	contextDesc = rlo->_context.copyDescription(rlo->_context.info);
    }
    if (!contextDesc) {
	contextDesc = CFStringCreateWithFormat(CFGetAllocator(rlo), NULL, CFSTR("<CFRunLoopObserver context 0x%x>"), rlo->_context.info);
    }
    result = CFStringCreateWithFormat(CFGetAllocator(rlo), NULL, CFSTR("<CFRunLoopObserver 0x%x [0x%x]>{locked = %s, valid = %s, activities = 0x%x, repeats = %s, order = %d, callout = 0x%x, context = %@}"), (UInt32)cf, (UInt32)CFGetAllocator(rlo), rlo->_lock ? "Yes" : "No", __CFIsValid(rlo) ? "Yes" : "No", rlo->_activities, __CFRunLoopObserverRepeats(rlo) ? "Yes" : "No", rlo->_order, rlo->_callout, contextDesc);
    __CFRunLoopObserverUnlock(rlo);
    CFRelease(contextDesc);
    return result;
}

CFAllocatorRef __CFRunLoopObserverGetAllocator(CFTypeRef cf) {
    CFRunLoopObserverRef rlo = (CFRunLoopObserverRef)cf;
    return rlo->_allocator;
}

void __CFRunLoopObserverDeallocate(CFTypeRef cf) {	/* DOES CALLOUT */
    CFRunLoopObserverRef rlo = (CFRunLoopObserverRef)cf;
    CFAllocatorRef allocator = CFGetAllocator(rlo);
    CFRunLoopObserverInvalidate(rlo);
    CFAllocatorDeallocate(allocator, rlo);
    CFRelease(allocator);
}

CFRunLoopObserverRef CFRunLoopObserverCreate(CFAllocatorRef allocator, CFOptionFlags activities, Boolean repeats, CFIndex order, CFRunLoopObserverCallBack callout, CFRunLoopObserverContext *context) {
    CFRunLoopObserverRef memory;
    UInt32 size;
    size = sizeof(struct __CFRunLoopObserver);
    allocator = (NULL == allocator) ? CFRetain(CFAllocatorGetDefault()) : CFRetain(allocator);
    memory = CFAllocatorAllocate(allocator, size, 0);
    if (NULL == memory) {
	CFRelease(allocator);
	return NULL;
    }
    __CFGenericInitBase(memory, NULL, __kCFRunLoopObserverTypeID);
    memory->_allocator = allocator;
    __CFSetValid(memory);
    __CFRunLoopObserverUnsetFiring(memory);
    if (repeats) {
	__CFRunLoopObserverSetRepeats(memory);
    } else {
	__CFRunLoopObserverUnsetRepeats(memory);
    }
    memory->_lock = 0;
    memory->_runLoop = NULL;
    memory->_rlCount = 0;
    memory->_activities = activities;
    memory->_order = order;
    memory->_callout = callout;
    if (context) {
	if (context->retain) {
	    memory->_context.info = (void *)context->retain(context->info);
	} else {
	    memory->_context.info = context->info;
	}
	memory->_context.retain = context->retain;
	memory->_context.release = context->release;
	memory->_context.copyDescription = context->copyDescription;
    } else {
	memory->_context.info = 0;
	memory->_context.retain = 0;
	memory->_context.release = 0;
	memory->_context.copyDescription = 0;
    }
    return memory;
}

CFOptionFlags CFRunLoopObserverGetActivities(CFRunLoopObserverRef rlo) {
    __CFGenericValidateType(rlo, __kCFRunLoopObserverTypeID);
    return rlo->_activities;
}

CFIndex CFRunLoopObserverGetOrder(CFRunLoopObserverRef rlo) {
    __CFGenericValidateType(rlo, __kCFRunLoopObserverTypeID);
    return rlo->_order;
}

Boolean CFRunLoopObserverDoesRepeat(CFRunLoopObserverRef rlo) {
    __CFGenericValidateType(rlo, __kCFRunLoopObserverTypeID);
    return __CFRunLoopObserverRepeats(rlo);
}

void CFRunLoopObserverInvalidate(CFRunLoopObserverRef rlo) {	/* DOES CALLOUT */
    __CFGenericValidateType(rlo, __kCFRunLoopObserverTypeID);
    __CFRunLoopObserverLock(rlo);
    if (__CFIsValid(rlo)) {
	__CFUnsetValid(rlo);
	if (rlo->_context.release)
	    rlo->_context.release(rlo->_context.info);
	rlo->_context.info = NULL;
    }
    __CFRunLoopObserverUnlock(rlo);
}

Boolean CFRunLoopObserverIsValid(CFRunLoopObserverRef rlo) {
    return __CFIsValid(rlo);
}

void CFRunLoopObserverGetContext(CFRunLoopObserverRef rlo, CFRunLoopObserverContext *context) {
    __CFGenericValidateType(rlo, __kCFRunLoopObserverTypeID);
    CFAssert1(0 == context->version, __kCFLogAssertion, "%s(): context version not initialized to 0", __PRETTY_FUNCTION__);
    *context = rlo->_context;
}

/* CFRunLoopTimer */

CFTypeID CFRunLoopTimerGetTypeID(void) {
    return __kCFRunLoopTimerTypeID;
}

Boolean __CFRunLoopTimerEqual(CFTypeRef cf1, CFTypeRef cf2) {
    CFRunLoopTimerRef rlt1 = (CFRunLoopTimerRef)cf1;
    CFRunLoopTimerRef rlt2 = (CFRunLoopTimerRef)cf2;
    if (rlt1 == rlt2) return TRUE;
    if (rlt1->_interval != rlt2->_interval) return FALSE;
    if (rlt1->_order != rlt2->_order) return FALSE;
    if (rlt1->_callout != rlt2->_callout) return FALSE;
    return TRUE;
}

CFHashCode __CFRunLoopTimerHash(CFTypeRef cf) {
    CFRunLoopTimerRef rlt = (CFRunLoopTimerRef)cf;
    return (CFHashCode)rlt;
}

CFStringRef __CFRunLoopTimerCopyDescription(CFTypeRef cf) {	/* DOES CALLOUT */
    CFRunLoopTimerRef rlt = (CFRunLoopTimerRef)cf;
    CFStringRef result;
    CFStringRef contextDesc = NULL;
    CFAbsoluteTime fireTime;
    __CFRunLoopTimerFireTimeLock();
    fireTime = rlt->_fireTime;
    __CFRunLoopTimerFireTimeUnlock();
    __CFRunLoopTimerLock(rlt);
    if (NULL != rlt->_context.copyDescription) {
	contextDesc = rlt->_context.copyDescription(rlt->_context.info);
    }
    if (NULL == contextDesc) {
	contextDesc = CFStringCreateWithFormat(CFGetAllocator(rlt), NULL, CFSTR("<CFRunLoopTimer context 0x%x>"), rlt->_context.info);
    }
    result = CFStringCreateWithFormat(CFGetAllocator(rlt), NULL, CFSTR("<CFRunLoopTimer 0x%x [0x%x]>{locked = %s, valid = %s, interval = %0.09g, next fire date = %0.09g, order = %d, callout = 0x%x, context = %@}"), (UInt32)cf, (UInt32)CFGetAllocator(rlt), rlt->_lock ? "Yes" : "No", __CFIsValid(rlt) ? "Yes" : "No", rlt->_interval, fireTime, rlt->_order, rlt->_callout, contextDesc);
    __CFRunLoopTimerUnlock(rlt);
    CFRelease(contextDesc);
    return result;
}

CFAllocatorRef __CFRunLoopTimerGetAllocator(CFTypeRef cf) {
    CFRunLoopTimerRef rlt = (CFRunLoopTimerRef)cf;
    return rlt->_allocator;
}

void __CFRunLoopTimerDeallocate(CFTypeRef cf) {	/* DOES CALLOUT */
    CFRunLoopTimerRef rlt = (CFRunLoopTimerRef)cf;
    CFAllocatorRef allocator = CFGetAllocator(rlt);
    CFRunLoopTimerInvalidate(rlt);	/* DOES CALLOUT */
    CFAllocatorDeallocate(allocator, rlt);
    CFRelease(allocator);
}

CFRunLoopTimerRef CFRunLoopTimerCreate(CFAllocatorRef allocator, CFDateRef fireDate, CFTimeInterval interval, CFIndex order, CFRunLoopTimerCallBack callout, CFRunLoopTimerContext *context) {
    CFRunLoopTimerRef memory;
    UInt32 size;
    size = sizeof(struct __CFRunLoopTimer);
    allocator = (NULL == allocator) ? CFRetain(CFAllocatorGetDefault()) : CFRetain(allocator);
    memory = CFAllocatorAllocate(allocator, size, 0);
    if (NULL == memory) {
	CFRelease(allocator);
	return NULL;
    }
    __CFGenericInitBase(memory, __CFRunLoopTimerIsa(), __kCFRunLoopTimerTypeID);
    memory->_allocator = allocator;
    __CFSetValid(memory);
    __CFRunLoopTimerUnsetFiring(memory);
    memory->_lock = 0;
    memory->_runLoop = NULL;
    memory->_rlCount = 0;
    mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &(memory->_port));
    memory->_order = order;
    memory->_fireTime = CFDateGetAbsoluteTime(fireDate);
    memory->_interval = interval;
    memory->_callout = callout;
    if (NULL != context) {
	if (context->retain) {
	    memory->_context.info = (void *)context->retain(context->info);
	} else {
	    memory->_context.info = context->info;
	}
	memory->_context.retain = context->retain;
	memory->_context.release = context->release;
	memory->_context.copyDescription = context->copyDescription;
    } else {
	memory->_context.info = 0;
	memory->_context.retain = 0;
	memory->_context.release = 0;
	memory->_context.copyDescription = 0;
    }
    return memory;
}

CFRunLoopTimerRef CFRunLoopTimerCreate_new(CFAllocatorRef allocator, CFAbsoluteTime fireDate, CFTimeInterval interval, CFOptionFlags flags, CFIndex order, CFRunLoopTimerCallBack callout, CFRunLoopTimerContext *context) {
    CFRunLoopTimerRef memory;
    UInt32 size;
    size = sizeof(struct __CFRunLoopTimer);
    allocator = (NULL == allocator) ? CFRetain(CFAllocatorGetDefault()) : CFRetain(allocator);
    memory = CFAllocatorAllocate(allocator, size, 0);
    if (NULL == memory) {
	CFRelease(allocator);
	return NULL;
    }
    __CFGenericInitBase(memory, __CFRunLoopTimerIsa(), __kCFRunLoopTimerTypeID);
    memory->_allocator = allocator;
    __CFSetValid(memory);
    __CFRunLoopTimerUnsetFiring(memory);
    memory->_lock = 0;
    memory->_runLoop = NULL;
    memory->_rlCount = 0;
    mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &(memory->_port));
    memory->_order = order;
    memory->_fireTime = fireDate;
    memory->_interval = interval;
    memory->_callout = callout;
    if (NULL != context) {
	if (context->retain) {
	    memory->_context.info = (void *)context->retain(context->info);
	} else {
	    memory->_context.info = context->info;
	}
	memory->_context.retain = context->retain;
	memory->_context.release = context->release;
	memory->_context.copyDescription = context->copyDescription;
    } else {
	memory->_context.info = 0;
	memory->_context.retain = 0;
	memory->_context.release = 0;
	memory->_context.copyDescription = 0;
    }
    return memory;
}

CFDateRef CFRunLoopTimerCopyNextFireDate(CFRunLoopTimerRef rlt) {
    CFAbsoluteTime fireTime;
    CFDateRef result = NULL;
    CF_OBJC_FUNCDISPATCH0(RunLoopTimer, CFDateRef , rlt, "_cffireDate");
    __CFGenericValidateType(rlt, __kCFRunLoopTimerTypeID);
    __CFRunLoopTimerFireTimeLock();
    fireTime = rlt->_fireTime;
    __CFRunLoopTimerFireTimeUnlock();
    __CFRunLoopTimerLock(rlt);
    if (__CFIsValid(rlt)) {
	result = CFDateCreate(CFGetAllocator(rlt), fireTime);
    }
    __CFRunLoopTimerUnlock(rlt);
    return result;
}

CFAbsoluteTime CFRunLoopTimerGetNextFireDate(CFRunLoopTimerRef rlt) {
    CFAbsoluteTime fireTime;
    CFAbsoluteTime result = 0.0;
    CF_OBJC_FUNCDISPATCH0(RunLoopTimer, CFAbsoluteTime, rlt, "_cffireTime");
    __CFGenericValidateType(rlt, __kCFRunLoopTimerTypeID);
    __CFRunLoopTimerFireTimeLock();
    fireTime = rlt->_fireTime;
    __CFRunLoopTimerFireTimeUnlock();
    __CFRunLoopTimerLock(rlt);
    if (__CFIsValid(rlt)) {
	result = fireTime;
    }
    __CFRunLoopTimerUnlock(rlt);
    return result;
}

void CFRunLoopTimerSetNextFireDate(CFRunLoopTimerRef rlt, CFDateRef fireDate) {
    __CFRunLoopTimerFireTimeLock();
    rlt->_fireTime = CFDateGetAbsoluteTime(fireDate);
    __CFRunLoopTimerFireTimeUnlock();
    if (rlt->_runLoop != NULL) {
	    mach_timespec_t timeout;
	    CFTimeInterval delta;
	    delta = rlt->_fireTime - CFAbsoluteTimeGetCurrent();
	    if (delta < 0) delta = 0.0;
	    delta += 0.001;
	    if (7776000.0 < delta) delta = 7776000.0;
	    timeout.tv_sec = floor(delta);
	    timeout.tv_nsec = (delta - timeout.tv_sec) * 1000000000;
	    clock_alarm(clock_port, TIME_RELATIVE, timeout, rlt->_port);
    }
}

void CFRunLoopTimerSetNextFireDate_new(CFRunLoopTimerRef rlt, CFAbsoluteTime fireDate) {
    __CFRunLoopTimerFireTimeLock();
    rlt->_fireTime = fireDate;
    __CFRunLoopTimerFireTimeUnlock();
    if (rlt->_runLoop != NULL) {
	    mach_timespec_t timeout;
	    CFTimeInterval delta;
	    delta = rlt->_fireTime - CFAbsoluteTimeGetCurrent();
	    if (delta < 0) delta = 0.0;
	    delta += 0.001;
	    if (7776000.0 < delta) delta = 7776000.0;
	    timeout.tv_sec = floor(delta);
	    timeout.tv_nsec = (delta - timeout.tv_sec) * 1000000000;
	    clock_alarm(clock_port, TIME_RELATIVE, timeout, rlt->_port);
    }
}

CFTimeInterval CFRunLoopTimerGetInterval(CFRunLoopTimerRef rlt) {
    CF_OBJC_FUNCDISPATCH0(RunLoopTimer, CFTimeInterval, rlt, "timeInterval");
    __CFGenericValidateType(rlt, __kCFRunLoopTimerTypeID);
    return rlt->_interval;
}

Boolean CFRunLoopTimerDoesRepeat(CFRunLoopTimerRef rlt) {
    __CFGenericValidateType(rlt, __kCFRunLoopTimerTypeID);
    return (0.0 != rlt->_interval);
}

CFIndex CFRunLoopTimerGetOrder(CFRunLoopTimerRef rlt) {
    CF_OBJC_FUNCDISPATCH0(RunLoopTimer, CFIndex, rlt, "order");
    __CFGenericValidateType(rlt, __kCFRunLoopTimerTypeID);
    return rlt->_order;
}

void CFRunLoopTimerInvalidate(CFRunLoopTimerRef rlt) {	/* DOES CALLOUT */
    CF_OBJC_FUNCDISPATCH0(RunLoopTimer, void, rlt, "invalidate");
    __CFGenericValidateType(rlt, __kCFRunLoopTimerTypeID);
    __CFRunLoopTimerLock(rlt);
    if (__CFIsValid(rlt)) {
	__CFUnsetValid(rlt);
	mach_port_destroy(mach_task_self(), rlt->_port);
	if (rlt->_context.release)
	    rlt->_context.release(rlt->_context.info);	/* CALLOUT */
	rlt->_context.info = NULL;
    }
    __CFRunLoopTimerUnlock(rlt);
}

Boolean CFRunLoopTimerIsValid(CFRunLoopTimerRef rlt) {
    CF_OBJC_FUNCDISPATCH0(RunLoopTimer, Boolean, rlt, "isValid");
    __CFGenericValidateType(rlt, __kCFRunLoopTimerTypeID);
    return __CFIsValid(rlt);
}

void CFRunLoopTimerGetContext(CFRunLoopTimerRef rlt, CFRunLoopTimerContext *context) {
    __CFGenericValidateType(rlt, __kCFRunLoopTimerTypeID);
    CFAssert1(0 == context->version, __kCFLogAssertion, "%s(): context version not initialized to 0", __PRETTY_FUNCTION__);
    *context = rlt->_context;
}

