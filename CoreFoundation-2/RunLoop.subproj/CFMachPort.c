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
/*	CFMachPort.c
	Copyright 1998-2000, Apple, Inc. All rights reserved.
	Responsibility: Christopher Kane
*/

#if 0

#if defined(__MACH__)

#define CFRUNLOOP_NEW_API
#include <CoreFoundation/CFMachPort.h>
#include <CoreFoundation/CFRunLoop.h>
#include <CoreFoundation/CFDictionary.h>
#include <mach/mach.h>
#include "CFInternal.h"
#undef CFMachPortCreate
#undef CFMachPortCreateWithPort

static UInt32 __CFAllMachPortsLock = 0;
static CFMutableDictionaryRef __CFAllMachPorts = NULL;

struct __CFMachPort {
    CFRuntimeBase _base;
    CFAllocatorRef _allocator;
    UInt32 _lock;
    mach_port_t _port;			/* immutable; invalidated */
    CFRunLoopSourceRef _source;		/* immutable, once created; invalidated */
    CFMachPortCallBack _callout;	/* immutable */
    CFMachPortContext _context;		/* immutable; invalidated */
};

/* Bit 0 in the base reserved bits is used for invalid state */
/* Bit 1 in the base reserved bits is used for has-receive-ref state */
/* Bit 2 in the base reserved bits is used for has-send-ref state */

CF_INLINE Boolean __CFMachPortIsValid(CFMachPortRef mp) {
    return (Boolean)__CFBitfieldValue(((CFRuntimeBase *)mp)->_info, 0, 0);
}

CF_INLINE void __CFMachPortSetValid(CFMachPortRef mp) {
    __CFBitfieldSetValue(((CFRuntimeBase *)mp)->_info, 0, 0, 1);
}

CF_INLINE void __CFMachPortUnsetValid(CFMachPortRef mp) {
    __CFBitfieldSetValue(((CFRuntimeBase *)mp)->_info, 0, 0, 0);
}

CF_INLINE Boolean __CFMachPortHasReceive(CFMachPortRef mp) {
    return (Boolean)__CFBitfieldValue(((CFRuntimeBase *)mp)->_info, 1, 1);
}

CF_INLINE void __CFMachPortSetHasReceive(CFMachPortRef mp) {
    __CFBitfieldSetValue(((CFRuntimeBase *)mp)->_info, 1, 1, 1);
}

CF_INLINE void __CFMachPortUnsetHasReceive(CFMachPortRef mp) {
    __CFBitfieldSetValue(((CFRuntimeBase *)mp)->_info, 1, 1, 0);
}

CF_INLINE Boolean __CFMachPortHasSend(CFMachPortRef mp) {
    return (Boolean)__CFBitfieldValue(((CFRuntimeBase *)mp)->_info, 2, 2);
}

CF_INLINE void __CFMachPortSetHasSend(CFMachPortRef mp) {
    __CFBitfieldSetValue(((CFRuntimeBase *)mp)->_info, 2, 2, 1);
}

CF_INLINE void __CFMachPortUnsetHasSend(CFMachPortRef mp) {
    __CFBitfieldSetValue(((CFRuntimeBase *)mp)->_info, 2, 2, 0);
}

CF_INLINE Boolean __CFMachPortNewStyle(CFMachPortRef mp) {
    return (Boolean)__CFBitfieldValue(((CFRuntimeBase *)mp)->_info, 3, 3);
}

CF_INLINE void __CFMachPortSetNewStyle(CFMachPortRef mp) {
    __CFBitfieldSetValue(((CFRuntimeBase *)mp)->_info, 3, 3, 1);
}

CF_INLINE void __CFMachPortLock(CFMachPortRef mp) {
    __CFSpinLock((UInt32 *)&(mp->_lock));
}

CF_INLINE void __CFMachPortUnlock(CFMachPortRef mp) {
    __CFSpinUnlock((UInt32 *)&(mp->_lock));
}

CFTypeID CFMachPortGetTypeID(void) {
    return __kCFMachPortTypeID;
}

Boolean __CFMachPortEqual(CFTypeRef cf1, CFTypeRef cf2) {
    CFMachPortRef mp1 = (CFMachPortRef)cf1;
    CFMachPortRef mp2 = (CFMachPortRef)cf2;
    return (mp1->_port == mp2->_port);
}

CFHashCode __CFMachPortHash(CFTypeRef cf) {
    CFMachPortRef mp = (CFMachPortRef)cf;
    return (CFHashCode)mp->_port;
}

CFStringRef __CFMachPortCopyDescription(CFTypeRef cf) {
    CFMachPortRef mp = (CFMachPortRef)cf;
    CFStringRef result;
    const char *locked;
    CFStringRef contextDesc = NULL;
    locked = mp->_lock ? "Yes" : "No";
    __CFMachPortLock(mp);
    if (NULL != mp->_context.copyDescription) {
	contextDesc = mp->_context.copyDescription(mp->_context.info);
    }
    if (NULL == contextDesc) {
	contextDesc = CFStringCreateWithFormat(CFGetAllocator(mp), NULL, CFSTR("<CFMachPort context %p>"), mp->_context.info);
    }
    result = CFStringCreateWithFormat(CFGetAllocator(mp), NULL, CFSTR("<CFMachPort %p [%p]>{locked = %s, valid = %s, port = %p, source = %p, callout = %p, context = %@}"), cf, CFGetAllocator(mp), locked, (__CFMachPortIsValid(mp) ? "Yes" : "No"), mp->_port, mp->_source, mp->_callout, (NULL != contextDesc ? contextDesc : CFSTR("<no description>")));
    if (NULL != contextDesc) {
	CFRelease(contextDesc);
    }
    __CFMachPortUnlock(mp);
    return result;
}

CFAllocatorRef __CFMachPortGetAllocator(CFTypeRef cf) {
    CFMachPortRef mp = (CFMachPortRef)cf;
    return mp->_allocator;
}

void __CFMachPortDeallocate(CFTypeRef cf) {
    /* Since CFMachPorts are cached, we can only get here sometime after being invalidated */
    CFMachPortRef mp = (CFMachPortRef)cf;
    CFAllocatorRef allocator = CFGetAllocator(mp);
    CFAllocatorDeallocate(allocator, mp);
    CFRelease(allocator);
}

CFMachPortRef CFMachPortCreate(CFAllocatorRef allocator, CFMachPortCallBack callout, CFMachPortContext *context) {
    CFMachPortRef result;
    mach_port_t port;
    kern_return_t ret;
    ret = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &port);
    if (KERN_SUCCESS != ret) {
	return NULL;
    }
    ret = mach_port_insert_right(mach_task_self(), port, port, MACH_MSG_TYPE_MAKE_SEND);
    if (KERN_SUCCESS != ret) {
	mach_port_destroy(mach_task_self(), port);
	return NULL;
    }
    result = CFMachPortCreateWithPort(allocator, port, callout, context);
    if (NULL != result) {
	__CFMachPortSetHasReceive(result);
	__CFMachPortSetHasSend(result);
    }
    return result;
}

CFMachPortRef CFMachPortCreate_new(CFAllocatorRef allocator, CFMachPortCallBack callout, CFMachPortContext *context, Boolean *shouldFreeInfo) {
    CFMachPortRef result;
    mach_port_t port;
    kern_return_t ret;
    if (shouldFreeInfo) *shouldFreeInfo = TRUE;
    ret = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &port);
    if (KERN_SUCCESS != ret) {
	return NULL;
    }
    ret = mach_port_insert_right(mach_task_self(), port, port, MACH_MSG_TYPE_MAKE_SEND);
    if (KERN_SUCCESS != ret) {
	mach_port_destroy(mach_task_self(), port);
	return NULL;
    }
    result = CFMachPortCreateWithPort_new(allocator, port, callout, context, shouldFreeInfo);
    if (NULL != result) {
	__CFMachPortSetHasReceive(result);
	__CFMachPortSetHasSend(result);
    }
    return result;
}

CFMachPortRef CFMachPortCreateWithPort(CFAllocatorRef allocator, mach_port_t port, CFMachPortCallBack callout, CFMachPortContext *context) {
    CFMachPortRef memory;
    SInt32 size;
    __CFSpinLock(&__CFAllMachPortsLock);
    if (NULL == __CFAllMachPorts) {
	__CFAllMachPorts = CFDictionaryCreateMutable(kCFAllocatorSystemDefault, 0, NULL, &kCFTypeDictionaryValueCallBacks);
    }
    if (CFDictionaryGetValueIfPresent(__CFAllMachPorts, (void *)port, (const void **)&memory)) {
	__CFSpinUnlock(&__CFAllMachPortsLock);
	return (CFMachPortRef)CFRetain(memory);
    }
    size = sizeof(struct __CFMachPort);
    allocator = (NULL == allocator) ? CFRetain(CFAllocatorGetDefault()) : CFRetain(allocator);
    memory = CFAllocatorAllocate(allocator, size, 0);
    if (NULL == memory) {
	__CFSpinUnlock(&__CFAllMachPortsLock);
	CFRelease(allocator);
	return NULL;
    }
    __CFGenericInitBase(memory, __CFMachPortIsa(), __kCFMachPortTypeID);
    memory->_allocator = allocator;
    __CFMachPortSetValid(memory);
    memory->_lock = 0;
    memory->_port = port;
    memory->_source = NULL;
    memory->_callout = callout;
    if (NULL != context) {
	memmove(&memory->_context, context, sizeof(CFMachPortContext));
	memory->_context.info = context->retain ? (void *)context->retain(context->info) : context->info;
    } else {
	memory->_context.info = NULL;
	memory->_context.retain = NULL;
	memory->_context.release = NULL;
	memory->_context.copyDescription = NULL;
    }
    CFDictionaryAddValue(__CFAllMachPorts, (void *)port, memory);
    __CFSpinUnlock(&__CFAllMachPortsLock);
    return memory;
}

CFMachPortRef CFMachPortCreateWithPort_new(CFAllocatorRef allocator, mach_port_t port, CFMachPortCallBack callout, CFMachPortContext *context, Boolean *shouldFreeInfo) {
    CFMachPortRef memory;
    SInt32 size;
    if (shouldFreeInfo) *shouldFreeInfo = TRUE;
    __CFSpinLock(&__CFAllMachPortsLock);
    if (NULL == __CFAllMachPorts) {
	__CFAllMachPorts = CFDictionaryCreateMutable(kCFAllocatorSystemDefault, 0, NULL, &kCFTypeDictionaryValueCallBacks);
    }
    if (CFDictionaryGetValueIfPresent(__CFAllMachPorts, (void *)port, (const void **)&memory)) {
	__CFSpinUnlock(&__CFAllMachPortsLock);
	return (CFMachPortRef)CFRetain(memory);
    }
    size = sizeof(struct __CFMachPort);
    allocator = (NULL == allocator) ? CFRetain(CFAllocatorGetDefault()) : CFRetain(allocator);
    memory = CFAllocatorAllocate(allocator, size, 0);
    if (NULL == memory) {
	__CFSpinUnlock(&__CFAllMachPortsLock);
	CFRelease(allocator);
	return NULL;
    }
    __CFGenericInitBase(memory, __CFMachPortIsa(), __kCFMachPortTypeID);
    memory->_allocator = allocator;
    __CFMachPortSetValid(memory);
    memory->_lock = 0;
    memory->_port = port;
    memory->_source = NULL;
    memory->_callout = callout;
    if (NULL != context) {
	memmove(&memory->_context, context, sizeof(CFMachPortContext));
	memory->_context.info = context->retain ? (void *)context->retain(context->info) : context->info;
    } else {
	memory->_context.info = NULL;
	memory->_context.retain = NULL;
	memory->_context.release = NULL;
	memory->_context.copyDescription = NULL;
    }
    __CFMachPortSetNewStyle(memory);
    CFDictionaryAddValue(__CFAllMachPorts, (void *)port, memory);
    __CFSpinUnlock(&__CFAllMachPortsLock);
    if (shouldFreeInfo) *shouldFreeInfo = FALSE;
    return memory;
}

void CFMachPortInvalidate(CFMachPortRef mp) {
    CF_OBJC_FUNCDISPATCH0(MachPort, void, mp, "invalidate");
    __CFGenericValidateType(mp, __kCFMachPortTypeID);
    CFRetain(mp);
    __CFMachPortLock(mp);
    __CFSpinLock(&__CFAllMachPortsLock);
    if (__CFMachPortIsValid(mp)) {
	CFRunLoopSourceRef source;
	__CFMachPortUnsetValid(mp);
	CFDictionaryRemoveValue(__CFAllMachPorts, (void *)(mp->_port));
	__CFSpinUnlock(&__CFAllMachPortsLock);
	if (__CFMachPortHasReceive(mp)) {
	    mach_port_mod_refs(mach_task_self(), mp->_port, MACH_PORT_RIGHT_RECEIVE, -1);
	}
	if (__CFMachPortHasSend(mp)) {
	    mach_port_mod_refs(mach_task_self(), mp->_port, MACH_PORT_RIGHT_SEND, -1);
	}
	mp->_port = MACH_PORT_NULL;
	source = mp->_source;
	mp->_source = NULL;
	if (mp->_context.release) {
	    mp->_context.release(mp->_context.info);
	}
	mp->_context.info = NULL;
	__CFMachPortUnlock(mp);
	if (NULL != source) {
	    CFRunLoopSourceInvalidate(source);
	    CFRelease(source);
	}
    } else {
	__CFSpinUnlock(&__CFAllMachPortsLock);
	__CFMachPortUnlock(mp);
    }
    CFRelease(mp);
}

Boolean CFMachPortIsValid(CFMachPortRef mp) {
    CF_OBJC_FUNCDISPATCH0(MachPort, Boolean, mp, "isValid");
    __CFGenericValidateType(mp, __kCFMachPortTypeID);
    return __CFMachPortIsValid(mp);
}

mach_port_t CFMachPortGetPort(CFMachPortRef mp) {
    CF_OBJC_FUNCDISPATCH0(MachPort, mach_port_t, mp, "machPort");
    __CFGenericValidateType(mp, __kCFMachPortTypeID);
    return mp->_port;
}

void CFMachPortGetContext(CFMachPortRef mp, CFMachPortContext *context) {
    __CFGenericValidateType(mp, __kCFMachPortTypeID);
    CFAssert1(0 == context->version, __kCFLogAssertion, "%s(): context version not initialized to 0", __PRETTY_FUNCTION__);
    memmove(context, &mp->_context, sizeof(CFMachPortContext));
}

void CFMachPortInvalidateAll(void) {
#warning CF: CFMachPortInvalidateAll unimplemented
}

static mach_port_t __CFMachPortGetPort(void *info) {
    CFMachPortRef mp = info;
    return mp->_port;
}

static void *__CFMachPortPerform(void *msg, CFAllocatorRef allocator, void *info) {
    CFMachPortRef mp = info;
    void *context_info;
    void (*context_release)(const void *);
    __CFMachPortLock(mp);
    if (!__CFMachPortIsValid(mp)) {
	__CFMachPortUnlock(mp);
	return NULL;
    }
    if (NULL != mp->_context.retain) {
	context_info = (void *)mp->_context.retain(mp->_context.info);
	context_release = mp->_context.release;
    } else {
	context_info = mp->_context.info;
	context_release = NULL;
    }
    __CFMachPortUnlock(mp);
    if (__CFMachPortNewStyle(mp)) {
        mp->_callout(mp, msg, ((mach_msg_header_t *)msg)->msgh_size, context_info);
    } else {
        mp->_callout(mp, msg, context_info);
    }
    if (context_release) {
	context_release(context_info);
    }
    return NULL;
}

CFRunLoopSourceRef CFMachPortCreateRunLoopSource(CFAllocatorRef allocator, CFMachPortRef mp, CFIndex order) {
    CFRunLoopSourceRef result = NULL;
    __CFGenericValidateType(mp, __kCFMachPortTypeID);
    __CFMachPortLock(mp);
#warning CF: adding ref to receive right is disabled for now -- doesnt work in 1F
#if 0
    if (!__CFMachPortHasReceive(mp)) {
	kern_return_t ret;

// this fails in 1F with KERN_INVALID_VALUE -- only 0 and -1 are valid for delta
	ret = mach_port_mod_refs(mach_task_self(), mp->_port, MACH_PORT_RIGHT_RECEIVE, +1);
	if (KERN_SUCCESS != ret) {
	    __CFMachPortUnlock(mp);
	    return NULL;
	}
	__CFMachPortSetHasReceive(mp);
    }
#endif
    if (NULL == mp->_source) {
	CFRunLoopSourceContext1 context;
	context.version = 1;
	context.info = (void *)mp;
	context.retain = (const void *(*)(const void *))CFRetain;
	context.release = (void (*)(const void *))CFRelease;
	context.copyDescription = (CFStringRef (*)(const void *))__CFMachPortCopyDescription;
	context.equal = (Boolean (*)(const void *, const void *))__CFMachPortEqual;
	context.hash = (CFHashCode (*)(const void *))__CFMachPortHash;
	context.getPort = __CFMachPortGetPort;
	context.perform = __CFMachPortPerform;
	mp->_source = CFRunLoopSourceCreate(allocator, order, (CFRunLoopSourceContext *)&context);
    }
    if (NULL != mp->_source) {
	result = (CFRunLoopSourceRef)CFRetain(mp->_source);
    }
    __CFMachPortUnlock(mp);
    return result;
}

#endif /* __MACH__ */

#else 0

#if defined(__MACH__)

#define CFRUNLOOP_NEW_API
#include "CFInternal.h"
#include <CoreFoundation/CFMachPort.h>
#include <CoreFoundation/CFRunLoop.h>
#include <CoreFoundation/CFDictionary.h>
#include <CoreFoundation/CFArray.h>
#include <mach/mach.h>
#include <mach/message.h>
#undef CFMachPortCreate
#undef CFMachPortCreateWithPort

#if !defined(MACH_RCV_LARGE)
#define MACH_RCV_LARGE 0x00000004
#endif

static SInt32 __CFAllMachPortsLock = 0;
static CFMutableDictionaryRef __CFAllMachPorts = NULL;
static SInt32 __CFAllListenMachPortsLock = 0;
static CFMutableArrayRef __CFAllListenMachPorts = NULL;
static void *__CFMachPortManagerThread = NULL;
static mach_port_t __CFAllListenMachPortsSet = MACH_PORT_NULL;

struct __CFMachPortMsg {
};

CF_INLINE void __CFMachPortMsgDestroy(struct __CFMachPortMsg *msg) {
    CFAllocatorDeallocate(kCFAllocatorSystemDefault, msg);
}

struct __CFMachPort {
    CFRuntimeBase _base;
    CFAllocatorRef _allocator;
    UInt32 _lock;
    mach_port_t _port;			/* immutable */
    SInt32 _portSetCount;
    CFRunLoopSourceRef _source;
    CFMutableArrayRef _runLoops;
    CFMutableArrayRef _msgQ;
    CFMachPortCallBack _callout;	/* immutable */
    CFMachPortContext _context;		/* immutable */
};

/* Bit 3 in the base reserved bits is used for invalid state */

CF_INLINE Boolean __CFMachPortNewStyle(CFMachPortRef mp) {
    return (Boolean)__CFBitfieldValue(((CFRuntimeBase *)mp)->_info, 0, 0);
}

CF_INLINE void __CFMachPortSetNewStyle(CFMachPortRef mp) {
    __CFBitfieldSetValue(((CFRuntimeBase *)mp)->_info, 0, 0, 1);
}

CF_INLINE Boolean __CFMachPortIsValid(CFMachPortRef mp) {
    return (Boolean)__CFBitfieldValue(((CFRuntimeBase *)mp)->_info, 3, 3);
}

CF_INLINE void __CFMachPortSetValid(CFMachPortRef mp) {
    __CFBitfieldSetValue(((CFRuntimeBase *)mp)->_info, 3, 3, 1);
}

CF_INLINE void __CFMachPortUnsetValid(CFMachPortRef mp) {
    __CFBitfieldSetValue(((CFRuntimeBase *)mp)->_info, 3, 3, 0);
}

CF_INLINE void __CFMachPortLock(CFMachPortRef mp) {
    __CFSpinLock((UInt32 *)&(mp->_lock));
}

CF_INLINE void __CFMachPortUnlock(CFMachPortRef mp) {
    __CFSpinUnlock((UInt32 *)&(mp->_lock));
}

static void __CFMachPortWakeUpWaitingRunLoop(CFMachPortRef mp) {
    CFRunLoopRef rl = NULL;
    SInt32 idx, cnt;
    for (idx = 0, cnt = CFArrayGetCount(mp->_runLoops); idx < cnt; idx++) {
	CFRunLoopRef value = (CFRunLoopRef)CFArrayGetValueAtIndex(mp->_runLoops, idx);
	CFStringRef currentMode = CFRunLoopCopyCurrentMode(value);
	if (NULL != currentMode && CFRunLoopIsWaiting(value) && CFRunLoopContainsSource(value, mp->_source, currentMode)) {
	    CFRelease(currentMode);
	    /* ideally, this would be a run loop which isn't also in a
	    * signaled state for this or another source, but that's tricky;
	    * we move this run loop to the end of the list to scramble them
	    * a bit, and always search from the front */
	    CFArrayRemoveValueAtIndex(mp->_runLoops, idx);
	    rl = value;
	    break;
	}
	if (NULL != currentMode) CFRelease(currentMode);
    }
    if (NULL == rl) {	/* didn't choose one above, so choose first */
	rl = (CFRunLoopRef)CFArrayGetValueAtIndex(mp->_runLoops, 0);
	CFArrayRemoveValueAtIndex(mp->_runLoops, 0);
    }
    CFArrayAppendValue(mp->_runLoops, rl);
    CFRunLoopWakeUp(rl);
}

static CFMachPortRef __CFMachPortFindForMachPort(mach_port_t port) {
    SInt32 idx, cnt;
    __CFSpinLock(&__CFAllListenMachPortsLock);
    for (idx = 0, cnt = CFArrayGetCount(__CFAllListenMachPorts); idx < cnt; idx++) {
	CFMachPortRef mp = (CFMachPortRef)CFArrayGetValueAtIndex(__CFAllListenMachPorts, idx);
	if (mp->_port == port) {
	    __CFSpinUnlock(&__CFAllListenMachPortsLock);
	    return mp;
	}
    }
    __CFSpinUnlock(&__CFAllListenMachPortsLock);
    return NULL;
}

static int queue_count = 0;

static mach_msg_header_t *__CFMachPortHandleMessage(CFMachPortRef mp, mach_msg_header_t *msg) {
    __CFMachPortLock(mp);
    if (__CFMachPortIsValid(mp) && 0 < CFArrayGetCount(mp->_runLoops)) {
	CFArrayAppendValue(mp->_msgQ, msg);
	queue_count++; // dont really care if this is thread-unsafe
	msg = NULL;
	CFRunLoopSourceSignal(mp->_source);
	__CFMachPortWakeUpWaitingRunLoop(mp);
    }
    __CFMachPortUnlock(mp);
    return msg;
}

#include <sys/time.h>

static void * __CFMachPortManager(void * arg) {
    mach_msg_header_t *msg = NULL;
    for (;;) {
	kern_return_t ret;
	CFMachPortRef mp;
	if (NULL == msg) {
	    msg = CFAllocatorAllocate(kCFAllocatorSystemDefault, 512, 0);
	    msg->msgh_size = 512;
	}
	msg->msgh_local_port = __CFAllListenMachPortsSet;
	msg->msgh_bits = 0;
	{ // Throttle to no more than 200 incoming msgs/sec, and less
	  // if there are lots enqueue internally and outstanding.
	  // (US$) 307.95 is a market quote on 1 oz year 2000 gold US
	  // Eagle (non-proof), value last updated: 9 March 2000.
	  // XXX need to update this value from time to time
	  // This coin's price has been empirically found to best model
	  // the desired throttling behavior.
	  extern int nanosleep(const struct timespec *requested_time, 
				struct timespec *remaining_time);
	  struct timespec input = {0, 5000000 + (long)(queue_count * 100 * 307.95)};
	  nanosleep(&input, NULL);
	}
	ret = mach_msg(msg, MACH_RCV_MSG|MACH_RCV_LARGE, 0, msg->msgh_size, __CFAllListenMachPortsSet, 0, MACH_PORT_NULL);
	if (MACH_RCV_TOO_LARGE == ret) {
            msg->msgh_size += sizeof(mach_msg_trailer_t);
	    msg = CFAllocatorReallocate(kCFAllocatorSystemDefault, msg, msg->msgh_size, 0);
	    continue;
	} else if (MACH_MSG_SUCCESS != ret) {
	    __CFAbort();
	}
	mp = __CFMachPortFindForMachPort(msg->msgh_local_port);
	if (NULL != mp) {
	    msg = __CFMachPortHandleMessage(mp, msg);
	}
	if (NULL != msg) {
	    // This can happen, as there is a race after the MACH_RCV_MSG
	    // and taking the lock for enqueuing
	    __CFMachPortMsgDestroy((struct __CFMachPortMsg *)msg);
	}
    }
    return (void *)0;
}

static void *__CFMessagePortManager(void *arg) {
    extern int nanosleep(const struct timespec *requested_time, 
				struct timespec *remaining_time);
    const char delays[] = {3, 1, 4, 1, 5, 9, 2, 6, 5, 3, 5, 8, 9, 7, 9, 3, 
			2, 3, 8, 4, 6, 2, 6, 4, 3, 3, 8, 3, 2, 7, 9, 5, 0,
			2, 8, 8, 4, 1, 9, 7, 1, 6, 9, 3, 9, 9, 3, 7, 5, 0, 8};
    int cur = 0;
    for (;;) {
	struct timespec input = {delays[cur] * 13, 0};
	nanosleep(&input, NULL);
        cur = (cur + 1) % (sizeof(delays)/sizeof(delays[0]));
    }
    return (void *)0;
}

CFTypeID CFMachPortGetTypeID(void) {
    return __kCFMachPortTypeID;
}

Boolean __CFMachPortEqual(CFTypeRef cf1, CFTypeRef cf2) {
    CFMachPortRef mp1 = (CFMachPortRef)cf1;
    CFMachPortRef mp2 = (CFMachPortRef)cf2;
    return (mp1->_port == mp2->_port);
}

CFHashCode __CFMachPortHash(CFTypeRef cf) {
    CFMachPortRef mp = (CFMachPortRef)cf;
    return (CFHashCode)mp->_port;
}

CFStringRef __CFMachPortCopyDescription(CFTypeRef cf) {
    CFMachPortRef mp = (CFMachPortRef)cf;
    CFMutableStringRef result;
    CFStringRef contextDesc = NULL;
    result = CFStringCreateMutable(CFGetAllocator(mp), 0);
    __CFMachPortLock(mp);
    CFStringAppendFormat(result, NULL, CFSTR("<CFMachPort 0x%x [0x%x]>{locked = %s, valid = %s, port = 0x%x, port set count = %d,\n    callout = 0x%x, context = "), (UInt32)cf, (UInt32)CFGetAllocator(mp), (mp->_lock ? "Yes" : "No"), (__CFMachPortIsValid(mp) ? "Yes" : "No"), (UInt32)mp->_port, mp->_portSetCount, mp->_callout);
    if (NULL != mp->_context.copyDescription) {
	contextDesc = mp->_context.copyDescription(mp->_context.info);
    }
    if (NULL != contextDesc) {
	CFStringAppend(result, contextDesc);
	CFRelease(contextDesc);
    } else {
	contextDesc = CFStringCreateWithFormat(CFGetAllocator(mp), NULL, CFSTR("<CFMachPort context 0x%x>"), (UInt32)mp->_context.info);
	CFStringAppend(result, contextDesc);
	CFRelease(contextDesc);
    }
    CFStringAppendFormat(result, NULL, CFSTR(",\n    source = 0x%x,\n    run loops = %@,\n    message queue = %@\n}"), (UInt32)mp->_source, mp->_runLoops, mp->_msgQ);
    __CFMachPortUnlock(mp);
    return result;
}

CFAllocatorRef __CFMachPortGetAllocator(CFTypeRef cf) {
    CFMachPortRef mp = (CFMachPortRef)cf;
    return mp->_allocator;
}

void __CFMachPortDeallocate(CFTypeRef cf) {
    /* Since CFMachPorts are cached, we can only get here sometime after being invalidated */
    CFMachPortRef mp = (CFMachPortRef)cf;
    CFAllocatorRef allocator = CFGetAllocator(mp);
    CFAllocatorDeallocate(allocator, mp);
    CFRelease(allocator);
}

CFMachPortRef CFMachPortCreate(CFAllocatorRef allocator, CFMachPortCallBack callout, CFMachPortContext *context) {
    mach_port_t port;
    kern_return_t ret;
    ret = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &port);
    if (0 == ret) {
	ret = mach_port_insert_right(mach_task_self(), port, port, MACH_MSG_TYPE_MAKE_SEND);
    }
    if (KERN_SUCCESS != ret) __CFAbort();
    return CFMachPortCreateWithPort(allocator, port, callout, context);
}

CFMachPortRef CFMachPortCreate_new(CFAllocatorRef allocator, CFMachPortCallBack callout, CFMachPortContext *context, Boolean *shouldFreeInfo) {
    mach_port_t port;
    kern_return_t ret;
    if (shouldFreeInfo) *shouldFreeInfo = TRUE;
    ret = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &port);
    if (0 == ret) {
	ret = mach_port_insert_right(mach_task_self(), port, port, MACH_MSG_TYPE_MAKE_SEND);
    }
    if (KERN_SUCCESS != ret) __CFAbort();
    return CFMachPortCreateWithPort_new(allocator, port, callout, context, shouldFreeInfo);
}

CFMachPortRef CFMachPortCreateWithPort(CFAllocatorRef allocator, mach_port_t port, CFMachPortCallBack callout, CFMachPortContext *context) {
    CFMachPortRef memory;
    UInt32 size;
    __CFSpinLock(&__CFAllMachPortsLock);
    if (NULL == __CFAllMachPorts) {
	__CFAllMachPorts = CFDictionaryCreateMutable(kCFAllocatorSystemDefault, 0, NULL, &kCFTypeDictionaryValueCallBacks);
    }
    if (CFDictionaryGetValueIfPresent(__CFAllMachPorts, (void *)port, (const void **)&memory)) {
	__CFSpinUnlock(&__CFAllMachPortsLock);
        // get rid of any allocated memory from the context since we don't need it here
        if (context != NULL && context->release != NULL) context->release(context->info);
	CFRetain(memory);
	return memory;
    }
    size = sizeof(struct __CFMachPort);
    allocator = (NULL == allocator) ? CFRetain(CFAllocatorGetDefault()) : CFRetain(allocator);
    memory = CFAllocatorAllocate(allocator, size, 0);
    if (NULL == memory) {
	CFRelease(allocator);
	__CFSpinUnlock(&__CFAllMachPortsLock);
        if (context != NULL && context->release != NULL) context->release(context->info);
	return NULL;
    }
    __CFGenericInitBase(memory, __CFMachPortIsa(), __kCFMachPortTypeID);
    memory->_allocator = allocator;
    __CFMachPortSetValid(memory);
    memory->_lock = 0;
    memory->_port = port;
    memory->_portSetCount = 0;
    memory->_source = NULL;
    memory->_runLoops = CFArrayCreateMutable(allocator, 0, NULL);
    memory->_msgQ = CFArrayCreateMutable(allocator, 0, NULL);
    memory->_callout = callout;
    if (context) {
	memory->_context.info = context->retain ? (void *)context->retain(context->info) : context->info;
	memory->_context.retain = context->retain;
	memory->_context.release = context->release;
	memory->_context.copyDescription = context->copyDescription;
    } else {
	memory->_context.info = 0;
	memory->_context.retain = 0;
	memory->_context.release = 0;
	memory->_context.copyDescription = 0;
    }
    CFDictionaryAddValue(__CFAllMachPorts, (void *)port, memory);
    __CFSpinUnlock(&__CFAllMachPortsLock);
    return memory;
}

CFMachPortRef CFMachPortCreateWithPort_new(CFAllocatorRef allocator, mach_port_t port, CFMachPortCallBack callout, CFMachPortContext *context, Boolean *shouldFreeInfo) {
    CFMachPortRef memory;
    UInt32 size;
    if (shouldFreeInfo) *shouldFreeInfo = TRUE;
    __CFSpinLock(&__CFAllMachPortsLock);
    if (NULL == __CFAllMachPorts) {
	__CFAllMachPorts = CFDictionaryCreateMutable(kCFAllocatorSystemDefault, 0, NULL, &kCFTypeDictionaryValueCallBacks);
    }
    if (CFDictionaryGetValueIfPresent(__CFAllMachPorts, (void *)port, (const void **)&memory)) {
	__CFSpinUnlock(&__CFAllMachPortsLock);
	return (CFMachPortRef)CFRetain(memory);
    }
    size = sizeof(struct __CFMachPort);
    allocator = (NULL == allocator) ? CFRetain(CFAllocatorGetDefault()) : CFRetain(allocator);
    memory = CFAllocatorAllocate(allocator, size, 0);
    if (NULL == memory) {
	CFRelease(allocator);
	__CFSpinUnlock(&__CFAllMachPortsLock);
        if (context != NULL && context->release != NULL) context->release(context->info);
	return NULL;
    }
    __CFGenericInitBase(memory, __CFMachPortIsa(), __kCFMachPortTypeID);
    memory->_allocator = allocator;
    __CFMachPortSetValid(memory);
    memory->_lock = 0;
    memory->_port = port;
    memory->_portSetCount = 0;
    memory->_source = NULL;
    memory->_runLoops = CFArrayCreateMutable(allocator, 0, NULL);
    memory->_msgQ = CFArrayCreateMutable(allocator, 0, NULL);
    memory->_callout = callout;
    if (context) {
	memory->_context.info = context->retain ? (void *)context->retain(context->info) : context->info;
	memory->_context.retain = context->retain;
	memory->_context.release = context->release;
	memory->_context.copyDescription = context->copyDescription;
    } else {
	memory->_context.info = 0;
	memory->_context.retain = 0;
	memory->_context.release = 0;
	memory->_context.copyDescription = 0;
    }
    __CFMachPortSetNewStyle(memory);
    CFDictionaryAddValue(__CFAllMachPorts, (void *)port, memory);
    __CFSpinUnlock(&__CFAllMachPortsLock);
    if (shouldFreeInfo) *shouldFreeInfo = FALSE;
    return memory;
}

void CFMachPortInvalidate(CFMachPortRef mp) {
    CF_OBJC_FUNCDISPATCH0(MachPort, void, mp, "invalidate");
    __CFGenericValidateType(mp, __kCFMachPortTypeID);
    CFRetain(mp);
    __CFSpinLock(&__CFAllMachPortsLock);
    __CFMachPortLock(mp);
    if (__CFMachPortIsValid(mp)) {
	SInt32 idx;
	__CFMachPortUnsetValid(mp);
	CFDictionaryRemoveValue(__CFAllMachPorts, (void *)(mp->_port));
	__CFSpinLock(&__CFAllListenMachPortsLock);
	mach_port_move_member(mach_task_self(), mp->_port, MACH_PORT_NULL);
	mach_port_destroy(mach_task_self(), mp->_port);
	idx = CFArrayGetFirstIndexOfValue(__CFAllListenMachPorts, CFRangeMake(0, CFArrayGetCount(__CFAllListenMachPorts)), mp);
	if (0 <= idx) CFArrayRemoveValueAtIndex(__CFAllListenMachPorts, idx);
	__CFSpinUnlock(&__CFAllListenMachPortsLock);
	mp->_port = MACH_PORT_NULL;
	mp->_portSetCount = 0;
	/* Toss all messages in local queue */
	// ??? is tossing all of the messages really the right thing? (It's what mach does.)
	for (idx = CFArrayGetCount(mp->_msgQ); idx--;) {
	    __CFMachPortMsgDestroy((struct __CFMachPortMsg *)CFArrayGetValueAtIndex(mp->_msgQ, idx));
	    queue_count--;
	}
	CFRelease(mp->_msgQ);
	mp->_msgQ = NULL;
	for (idx = CFArrayGetCount(mp->_runLoops); idx--;) {
	    CFRunLoopWakeUp((CFRunLoopRef)CFArrayGetValueAtIndex(mp->_runLoops, idx));
	}
	CFRelease(mp->_runLoops);
	mp->_runLoops = NULL;
	/* Some folks might want this callout to occur before all the other stuff is torn down */
	if (mp->_context.release) {
	    mp->_context.release(mp->_context.info);
	}
	mp->_context.info = NULL;
	if (NULL != mp->_source) {
	    CFRunLoopSourceInvalidate(mp->_source);
	    CFRelease(mp->_source);
	    mp->_source = NULL;
	}
    }
    __CFMachPortUnlock(mp);
    __CFSpinUnlock(&__CFAllMachPortsLock);
    CFRelease(mp);
}

Boolean CFMachPortIsValid(CFMachPortRef mp) {
    CF_OBJC_FUNCDISPATCH0(MachPort, Boolean, mp, "isValid");
    __CFGenericValidateType(mp, __kCFMachPortTypeID);
    return __CFMachPortIsValid(mp);
}

mach_port_t CFMachPortGetPort(CFMachPortRef mp) {
    CF_OBJC_FUNCDISPATCH0(MachPort, mach_port_t, mp, "machPort");
    __CFGenericValidateType(mp, __kCFMachPortTypeID);
    return mp->_port;
}

void CFMachPortGetContext(CFMachPortRef mp, CFMachPortContext *context) {
    __CFGenericValidateType(mp, __kCFMachPortTypeID);
    CFAssert1(0 == context->version, __kCFLogAssertion, "%s(): context version not initialized to 0", __PRETTY_FUNCTION__);
    *context = mp->_context;
}

void CFMachPortInvalidateAll(void) {
#warning CF: unimplemented
}

static void __CFMachPortSchedule(void *info, CFRunLoopRef rl, CFStringRef mode) {
    CFMachPortRef mp = info;
    __CFMachPortLock(mp);
    if (__CFMachPortIsValid(mp)) {
	CFArrayAppendValue(mp->_runLoops, rl);
	mp->_portSetCount++;
	if (1 == mp->_portSetCount) {
	    kern_return_t ret;
	    __CFSpinLock(&__CFAllListenMachPortsLock);
	    if (NULL == __CFAllListenMachPorts) {
		__CFAllListenMachPorts = CFArrayCreateMutable(kCFAllocatorSystemDefault, 0, NULL);
		ret = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_PORT_SET, &__CFAllListenMachPortsSet);
		if (KERN_SUCCESS != ret)
		    __CFAbort();
	    }
	    ret = mach_port_move_member(mach_task_self(), mp->_port, __CFAllListenMachPortsSet);
	    if (KERN_SUCCESS != ret)
		__CFAbort();
	    CFArrayAppendValue(__CFAllListenMachPorts, mp);
	    __CFSpinUnlock(&__CFAllListenMachPortsLock);
	}
	if (NULL == __CFMachPortManagerThread) {
	    __CFMachPortManagerThread = __CFStartSimpleThread(__CFMachPortManager, 0);
	    __CFStartSimpleThread(__CFMessagePortManager, 0);
	}
	if (0 < CFArrayGetCount(mp->_msgQ) && 0 < CFArrayGetCount(mp->_runLoops)) {
	    CFRunLoopSourceSignal(mp->_source);
	    __CFMachPortWakeUpWaitingRunLoop(mp);
	}
    }
    __CFMachPortUnlock(mp);
}

static void __CFMachPortCancel(void *info, CFRunLoopRef rl, CFStringRef mode) {
    CFMachPortRef mp = info;
    SInt32 idx;
    __CFMachPortLock(mp);
    mp->_portSetCount--;
    if (0 == mp->_portSetCount) {
	__CFSpinLock(&__CFAllListenMachPortsLock);
	mach_port_move_member(mach_task_self(), mp->_port, MACH_PORT_NULL);
	idx = CFArrayGetFirstIndexOfValue(__CFAllListenMachPorts, CFRangeMake(0, CFArrayGetCount(__CFAllListenMachPorts)), mp);
	if (0 <= idx) CFArrayRemoveValueAtIndex(__CFAllListenMachPorts, idx);
	__CFSpinUnlock(&__CFAllListenMachPortsLock);
    }
    if (NULL != mp->_runLoops) {
	idx = CFArrayGetFirstIndexOfValue(mp->_runLoops, CFRangeMake(0, CFArrayGetCount(mp->_runLoops)), rl);
	if (0 <= idx) CFArrayRemoveValueAtIndex(mp->_runLoops, idx);
    }
    __CFMachPortUnlock(mp);
}

static void __CFMachPortPerform(void *info) {
    CFMachPortRef mp = info;
    struct __CFMachPortMsg *msg = NULL;
    __CFMachPortLock(mp);
    if (!__CFMachPortIsValid(mp)) {
	__CFMachPortUnlock(mp);
	return;
    }
    if (0 < CFArrayGetCount(mp->_msgQ)) {
	msg = (struct __CFMachPortMsg *)CFArrayGetValueAtIndex(mp->_msgQ, 0);
	CFArrayRemoveValueAtIndex(mp->_msgQ, 0);
	queue_count--;
    }
    __CFMachPortUnlock(mp);
    if (NULL != msg) {
	if (__CFMachPortNewStyle(mp)) {
	    ((void (*)(void *, void *, int, void *))mp->_callout)(mp, msg, ((mach_msg_header_t *)msg)->msgh_size, mp->_context.info);
	} else {
	    ((void (*)(void *, void *, void *))mp->_callout)(mp, msg, mp->_context.info);
	}
	__CFMachPortMsgDestroy(msg);
    }
    __CFMachPortLock(mp);
    if (__CFMachPortIsValid(mp) && 0 < CFArrayGetCount(mp->_msgQ) && 0 < CFArrayGetCount(mp->_runLoops)) {
	CFRunLoopSourceSignal(mp->_source);
	__CFMachPortWakeUpWaitingRunLoop(mp);
    }
    __CFMachPortUnlock(mp);
}

CFRunLoopSourceRef CFMachPortCreateRunLoopSource(CFAllocatorRef allocator, CFMachPortRef mp, CFIndex order) {
   CFRunLoopSourceRef result = NULL;
    __CFMachPortLock(mp);
    if (NULL == mp->_source) {
	CFRunLoopSourceContext context;
	context.version = 0;
	context.info = (void *)mp;
	context.retain = (const void *(*)(const void *))CFRetain;
	context.release = (void (*)(const void *))CFRelease;
	context.copyDescription = (CFStringRef (*)(const void *))__CFMachPortCopyDescription;
	context.equal = (Boolean (*)(const void *, const void *))__CFMachPortEqual;
	context.hash = (CFHashCode (*)(const void *))__CFMachPortHash;
	context.schedule = __CFMachPortSchedule;
	context.cancel = __CFMachPortCancel;
	context.perform = __CFMachPortPerform;
	mp->_source = CFRunLoopSourceCreate(allocator, order, &context);
    }
    CFRetain(mp->_source);	/* This retain is for the receiver */
    result = mp->_source;
    __CFMachPortUnlock(mp);
    return result;
}

#endif /* __MACH__ */

#endif
