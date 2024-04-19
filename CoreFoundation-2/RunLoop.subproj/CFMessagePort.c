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
/*	CFMessagePort.c
	Copyright 1998-2000, Apple, Inc. All rights reserved.
	Responsibility: Christopher Kane
*/

#define CFRUNLOOP_NEW_API
#include "CFMessagePort.h"
#include <CoreFoundation/CFRunLoop.h>
#include <CoreFoundation/CFMachPort.h>
#include <CoreFoundation/CFDictionary.h>
#include "CFVeryPrivate.h"
#include <limits.h>
#include "CFInternal.h"
#include <mach/mach.h>
#include <mach/message.h>
#include <mach/mach_error.h>
#include <servers/bootstrap.h>
#undef CFMessagePortCreateLocal
#undef CFMessagePortSendRequest

#define __kCFMessagePortMaxNameLengthMax 255

#if defined(BOOTSTRAP_MAX_NAME_LEN)
    #define __kCFMessagePortMaxNameLength BOOTSTRAP_MAX_NAME_LEN
#else
    #define __kCFMessagePortMaxNameLength 128
#endif

#if __kCFMessagePortMaxNameLengthMax < __kCFMessagePortMaxNameLength
    #undef __kCFMessagePortMaxNameLength
    #define __kCFMessagePortMaxNameLength __kCFMessagePortMaxNameLengthMax
#endif

static UInt32 __CFAllMessagePortsLock = 0;
static CFMutableDictionaryRef __CFAllLocalMessagePorts = NULL;
static CFMutableDictionaryRef __CFAllRemoteMessagePorts = NULL;

CF_INLINE CFMessagePortRef __CFMessagePortLookUpInCache(CFMutableDictionaryRef *cache, CFStringRef name) {
    CFMessagePortRef existing;
    if (NULL == *cache) {
	*cache = CFDictionaryCreateMutable(kCFAllocatorSystemDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    }
    if (CFDictionaryGetValueIfPresent(*cache, name, (const void **)&existing)) {
	return existing;
    }
    return NULL;
}

struct __CFMessagePort {
    CFRuntimeBase _base;
    CFAllocatorRef _allocator;
    UInt32 _lock;
    CFStringRef _name;
    CFMachPortRef _port;		/* immutable; invalidated */
    void *_reply;
    SInt32 _desiredReply;
    CFMachPortRef _replyPort;		/* only used by remote port; immutable once created; invalidated */
    CFRunLoopSourceRef _source;		/* only used by local port; immutable once created; invalidated */
    CFMessagePortCallBack _callout;	/* only used by local port; immutable */
    CFMessagePortContext _context;	/* not part of remote port; immutable; invalidated */
};

/* Bit 0 of the base reserved bits is used for wants reply state */
/* Bit 1 of the base reserved bits is used for has reply state */
/* Bit 2 of the base reserved bits is used for remote state */
/* Bit 3 in the base reserved bits is used for invalid state */

CF_INLINE Boolean __CFMessagePortWantsReply(CFMessagePortRef ms) {
    return (Boolean)__CFBitfieldValue(((CFRuntimeBase *)ms)->_info, 0, 0);
}

CF_INLINE void __CFMessagePortSetWantsReply(CFMessagePortRef ms) {
    __CFBitfieldSetValue(((CFRuntimeBase *)ms)->_info, 0, 0, 1);
}

CF_INLINE void __CFMessagePortUnsetWantsReply(CFMessagePortRef ms) {
    __CFBitfieldSetValue(((CFRuntimeBase *)ms)->_info, 0, 0, 0);
}

CF_INLINE Boolean __CFMessagePortHasReply(CFMessagePortRef ms) {
    return (Boolean)__CFBitfieldValue(((CFRuntimeBase *)ms)->_info, 1, 1);
}

CF_INLINE void __CFMessagePortSetHasReply(CFMessagePortRef ms) {
    __CFBitfieldSetValue(((CFRuntimeBase *)ms)->_info, 1, 1, 1);
}

CF_INLINE void __CFMessagePortUnsetHasReply(CFMessagePortRef ms) {
    __CFBitfieldSetValue(((CFRuntimeBase *)ms)->_info, 1, 1, 0);
}

CF_INLINE Boolean __CFMessagePortIsRemote(CFMessagePortRef ms) {
    return (Boolean)__CFBitfieldValue(((CFRuntimeBase *)ms)->_info, 2, 2);
}

CF_INLINE void __CFMessagePortSetRemote(CFMessagePortRef ms) {
    __CFBitfieldSetValue(((CFRuntimeBase *)ms)->_info, 2, 2, 1);
}

CF_INLINE void __CFMessagePortUnsetRemote(CFMessagePortRef ms) {
    __CFBitfieldSetValue(((CFRuntimeBase *)ms)->_info, 2, 2, 0);
}

CF_INLINE Boolean __CFMessagePortIsValid(CFMessagePortRef ms) {
    return (Boolean)__CFBitfieldValue(((CFRuntimeBase *)ms)->_info, 3, 3);
}

CF_INLINE void __CFMessagePortSetValid(CFMessagePortRef ms) {
    __CFBitfieldSetValue(((CFRuntimeBase *)ms)->_info, 3, 3, 1);
}

CF_INLINE void __CFMessagePortUnsetValid(CFMessagePortRef ms) {
    __CFBitfieldSetValue(((CFRuntimeBase *)ms)->_info, 3, 3, 0);
}

CF_INLINE void __CFMessagePortLock(CFMessagePortRef ms) {
    __CFSpinLock((UInt32 *)&(ms->_lock));
}

CF_INLINE void __CFMessagePortUnlock(CFMessagePortRef ms) {
    __CFSpinUnlock((UInt32 *)&(ms->_lock));
}

// Just a heuristic
#define __CFMessagePortMaxInlineBytes 4096*10

struct __CFMessagePortMachMsg0 {
    SInt32 msgid;
    SInt32 byteslen;
    UInt8 bytes[__CFMessagePortMaxInlineBytes];
};

struct __CFMessagePortMachMsg1 {
    mach_msg_descriptor_t desc;
    SInt32 msgid;
};

struct __CFMessagePortMachMessage {
    mach_msg_header_t head;
    mach_msg_body_t body;
    union {
	struct __CFMessagePortMachMsg0 msg0;
	struct __CFMessagePortMachMsg1 msg1;
    } contents;
};

static struct __CFMessagePortMachMessage *__CFMessagePortCreateMessage(CFAllocatorRef allocator, mach_port_t port, mach_port_t replyPort, SInt32 convid, SInt32 msgid, const UInt8 *bytes, SInt32 byteslen) {
    struct __CFMessagePortMachMessage *msg;
    SInt32 size = sizeof(mach_msg_header_t) + sizeof(mach_msg_body_t);
    if (byteslen < __CFMessagePortMaxInlineBytes) {
	size += 2 * sizeof(SInt32) + (byteslen + 3) & ~0x3;
    } else {
	size += sizeof(struct __CFMessagePortMachMsg1);
    }
    msg = CFAllocatorAllocate(allocator, size, 0);
    msg->head.msgh_id = convid;
    msg->head.msgh_size = size;
    msg->head.msgh_remote_port = port;
    msg->head.msgh_local_port = replyPort;
    msg->head.msgh_reserved = 0;
    msg->head.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, (replyPort ? MACH_MSG_TYPE_MAKE_SEND : 0));
    if (byteslen < __CFMessagePortMaxInlineBytes) {
	msg->body.msgh_descriptor_count = 0;
	msg->contents.msg0.msgid = msgid;
	msg->contents.msg0.byteslen = byteslen;
	if (NULL != bytes && 0 < byteslen) {
	    memmove(msg->contents.msg0.bytes, bytes, byteslen);
	}
	memset(msg->contents.msg0.bytes + byteslen, 0, ((byteslen + 3) & ~0x3) - byteslen);
    } else {
	msg->head.msgh_bits |= MACH_MSGH_BITS_COMPLEX;
	msg->body.msgh_descriptor_count = 1;
	msg->contents.msg1.desc.out_of_line.deallocate = FALSE;
	msg->contents.msg1.desc.out_of_line.copy = MACH_MSG_VIRTUAL_COPY;
	msg->contents.msg1.desc.out_of_line.address = (void *)bytes;
	msg->contents.msg1.desc.out_of_line.size = byteslen;
	msg->contents.msg1.desc.out_of_line.type = MACH_MSG_OOL_DESCRIPTOR;
	msg->contents.msg1.msgid = msgid;
    }
    return msg;
}

static Boolean __CFMessagePortNativeSetNameLocal(CFMachPortRef port, UInt8 *portname) {
    mach_port_t bp;
    kern_return_t ret;
    task_get_bootstrap_port(mach_task_self(), &bp);
    ret = bootstrap_register(bp, portname, CFMachPortGetPort(port));
    return (ret == KERN_SUCCESS) ? TRUE : FALSE;
}

CFTypeID CFMessagePortGetTypeID(void) {
    return __kCFMessagePortTypeID;
}

Boolean __CFMessagePortEqual(CFTypeRef cf1, CFTypeRef cf2) {
    CFMessagePortRef ms1 = (CFMessagePortRef)cf1;
    CFMessagePortRef ms2 = (CFMessagePortRef)cf2;
    return (ms1->_port == ms2->_port);
}

CFHashCode __CFMessagePortHash(CFTypeRef cf) {
    CFMessagePortRef ms = (CFMessagePortRef)cf;
    return (CFHashCode)ms->_port;
}

CFStringRef __CFMessagePortCopyDescription(CFTypeRef cf) {
    CFMessagePortRef ms = (CFMessagePortRef)cf;
    CFStringRef result;
    const char *locked;
    CFStringRef contextDesc = NULL;
    locked = ms->_lock ? "Yes" : "No";
    __CFMessagePortLock(ms);
    if (!__CFMessagePortIsRemote(ms)) {
	if (NULL != ms->_context.copyDescription) {
	    contextDesc = ms->_context.copyDescription(ms->_context.info);
	}
	if (NULL == contextDesc) {
	    contextDesc = CFStringCreateWithFormat(CFGetAllocator(ms), NULL, CFSTR("<CFMessagePort context %p>"), ms->_context.info);
	}
	result = CFStringCreateWithFormat(CFGetAllocator(ms), NULL, CFSTR("<CFMessagePort %p [%p]>{locked = %s, valid = %s, remote = %s, name = %@}"), cf, CFGetAllocator(ms), locked, (__CFMessagePortIsValid(ms) ? "Yes" : "No"), (__CFMessagePortIsRemote(ms) ? "Yes" : "No"), ms->_name);
    } else {
	result = CFStringCreateWithFormat(CFGetAllocator(ms), NULL, CFSTR("<CFMessagePort %p [%p]>{locked = %s, valid = %s, remote = %s, name = %@, source = %p, callout = %p, context = %@}"), cf, CFGetAllocator(ms), locked, (__CFMessagePortIsValid(ms) ? "Yes" : "No"), (__CFMessagePortIsRemote(ms) ? "Yes" : "No"), ms->_name, ms->_source, ms->_callout, (NULL != contextDesc ? contextDesc : CFSTR("<no description>")));
    }
    if (NULL != contextDesc) {
	CFRelease(contextDesc);
    }
    __CFMessagePortUnlock(ms);
    return result;
}

CFAllocatorRef __CFMessagePortGetAllocator(CFTypeRef cf) {
    CFMessagePortRef ms = (CFMessagePortRef)cf;
    return ms->_allocator;
}

void __CFMessagePortDeallocate(CFTypeRef cf) {
    /* Since CFMessagePorts are cached, we can only get here sometime after being invalidated */
    CFMessagePortRef ms = (CFMessagePortRef)cf;
    CFAllocatorRef allocator = CFGetAllocator(ms);
    CFRelease(ms->_name);
    CFAllocatorDeallocate(allocator, ms);
    CFRelease(allocator);
}

static CFStringRef __CFMessagePortSanitizeStringName(CFAllocatorRef allocator, CFStringRef name, UInt8 **utfnamep, CFIndex *utfnamelenp) {
    UInt8 *utfname;
    CFIndex utflen;
    CFStringRef result;
    utfname = CFAllocatorAllocate(allocator, __kCFMessagePortMaxNameLength, 0);
    CFStringGetBytes(name, CFRangeMake(0, CFStringGetLength(name)), kCFStringEncodingUTF8, 0, FALSE, utfname, __kCFMessagePortMaxNameLength, &utflen);
    utfname[utflen] = '\0';
    /* A new string is created, because the original string may have been
       truncated to the max length, and we want the string name to definately
       match the raw UTF-8 chunk that has been created. Also, this is useful
       to get a constant string in case the original name string was mutable. */
    result = CFStringCreateWithBytes(allocator, utfname, utflen, kCFStringEncodingUTF8, FALSE);
    if (NULL != utfnamep) {
	*utfnamep = utfname;
    } else {
	CFAllocatorDeallocate(allocator, utfname);
    }
    if (NULL != utfnamelenp) {
	*utfnamelenp = utflen;
    }
    return result;
}

static void __CFMessagePortDummyCallback(CFMachPortRef port, void *msg, CFIndex size, void *info) {
// not supposed to be implemented
}

CFMessagePortRef CFMessagePortCreateLocal(CFAllocatorRef allocator, CFStringRef name, CFMessagePortCallBack callout, CFMessagePortContext *context) {
    CFMessagePortRef memory;
    CFMachPortRef native;
    UInt8 *utfname = NULL;
    CFIndex utflen = 0, size;

    if (NULL != name) {
	name = __CFMessagePortSanitizeStringName(allocator, name, &utfname, &utflen);
    }
    __CFSpinLock(&__CFAllMessagePortsLock);
    if (NULL != name) {
	memory = __CFMessagePortLookUpInCache(&__CFAllLocalMessagePorts, name);
	if (NULL != memory) {
	    __CFSpinUnlock(&__CFAllMessagePortsLock);
	    CFRelease(name);
	    CFAllocatorDeallocate(allocator, utfname);
	    return (CFMessagePortRef)CFRetain(memory);
	}
    }
    native = CFMachPortCreate(allocator, __CFMessagePortDummyCallback, NULL, NULL);
    if (NULL != native && NULL != name && !__CFMessagePortNativeSetNameLocal(native, utfname)) {
	CFMachPortInvalidate(native);
	CFRelease(native);
	native = NULL;
    }
    CFAllocatorDeallocate(allocator, utfname);
    if (NULL == native) {
	if (NULL != name) {
	    CFRelease(name);
	}
	__CFSpinUnlock(&__CFAllMessagePortsLock);
	return NULL;
    }
    size = sizeof(struct __CFMessagePort);
    allocator = (NULL == allocator) ? CFRetain(CFAllocatorGetDefault()) : CFRetain(allocator);
    memory = CFAllocatorAllocate(allocator, size, 0);
    if (NULL == memory) {
	CFRelease(allocator);
	CFMachPortInvalidate(native);
	CFRelease(native);
	if (NULL != name) {
	    CFRelease(name);
	}
	__CFSpinUnlock(&__CFAllMessagePortsLock);
	return NULL;
    }
    __CFGenericInitBase(memory, NULL, __kCFMessagePortTypeID);
    memory->_allocator = allocator;
    __CFMessagePortSetValid(memory);
    __CFMessagePortUnsetRemote(memory);
    memory->_lock = 0;
    memory->_name = name;
    memory->_port = native;
    memory->_reply = NULL;
    memory->_desiredReply = 0;
    memory->_replyPort = NULL;
    memory->_source = NULL;
    memory->_callout = callout;
    if (NULL != context) {
	memmove(&memory->_context, context, sizeof(CFMessagePortContext));
	memory->_context.info = context->retain ? (void *)context->retain(context->info) : context->info;
    } else {
	memory->_context.info = NULL;
	memory->_context.retain = NULL;
	memory->_context.release = NULL;
	memory->_context.copyDescription = NULL;
    }
    if (NULL != name) {
	CFDictionaryAddValue(__CFAllLocalMessagePorts, name, memory);
    }
    __CFSpinUnlock(&__CFAllMessagePortsLock);
    return memory;
}

CFMessagePortRef CFMessagePortCreateLocal_new(CFAllocatorRef allocator, CFStringRef name, CFMessagePortCallBack callout, CFMessagePortContext *context, Boolean *shouldFreeInfo) {
    CFMessagePortRef memory;
    CFMachPortRef native;
    UInt8 *utfname = NULL;
    CFIndex utflen = 0, size;

    if (shouldFreeInfo) *shouldFreeInfo = TRUE;
    if (NULL != name) {
	name = __CFMessagePortSanitizeStringName(allocator, name, &utfname, &utflen);
    }
    __CFSpinLock(&__CFAllMessagePortsLock);
    if (NULL != name) {
	memory = __CFMessagePortLookUpInCache(&__CFAllLocalMessagePorts, name);
	if (NULL != memory) {
	    __CFSpinUnlock(&__CFAllMessagePortsLock);
	    CFRelease(name);
	    CFAllocatorDeallocate(allocator, utfname);
	    return (CFMessagePortRef)CFRetain(memory);
	}
    }
    native = CFMachPortCreate(allocator, __CFMessagePortDummyCallback, NULL, NULL);
    if (NULL != native && NULL != name && !__CFMessagePortNativeSetNameLocal(native, utfname)) {
	CFMachPortInvalidate(native);
	CFRelease(native);
	native = NULL;
    }
    CFAllocatorDeallocate(allocator, utfname);
    if (NULL == native) {
	if (NULL != name) {
	    CFRelease(name);
	}
	__CFSpinUnlock(&__CFAllMessagePortsLock);
	return NULL;
    }
    size = sizeof(struct __CFMessagePort);
    allocator = (NULL == allocator) ? CFRetain(CFAllocatorGetDefault()) : CFRetain(allocator);
    memory = CFAllocatorAllocate(allocator, size, 0);
    if (NULL == memory) {
	CFRelease(allocator);
	CFMachPortInvalidate(native);
	CFRelease(native);
	if (NULL != name) {
	    CFRelease(name);
	}
	__CFSpinUnlock(&__CFAllMessagePortsLock);
	return NULL;
    }
    __CFGenericInitBase(memory, NULL, __kCFMessagePortTypeID);
    memory->_allocator = allocator;
    __CFMessagePortSetValid(memory);
    __CFMessagePortUnsetRemote(memory);
    memory->_lock = 0;
    memory->_name = name;
    memory->_port = native;
    memory->_reply = NULL;
    memory->_desiredReply = 0;
    memory->_replyPort = NULL;
    memory->_source = NULL;
    memory->_callout = callout;
    if (NULL != context) {
	memmove(&memory->_context, context, sizeof(CFMessagePortContext));
	memory->_context.info = context->retain ? (void *)context->retain(context->info) : context->info;
    } else {
	memory->_context.info = NULL;
	memory->_context.retain = NULL;
	memory->_context.release = NULL;
	memory->_context.copyDescription = NULL;
    }
    if (NULL != name) {
	CFDictionaryAddValue(__CFAllLocalMessagePorts, name, memory);
    }
    __CFSpinUnlock(&__CFAllMessagePortsLock);
    if (shouldFreeInfo) *shouldFreeInfo = FALSE;
    return memory;
}

CFMessagePortRef CFMessagePortCreateRemote(CFAllocatorRef allocator, CFStringRef name) {
    CFMessagePortRef memory;
    CFMachPortRef native;
    UInt8 *utfname = NULL;
    CFIndex utflen = 0, size;
    mach_port_t bp, port;
    kern_return_t ret;

    name = __CFMessagePortSanitizeStringName(allocator, name, &utfname, &utflen);
    if (NULL == name) {
	return NULL;
    }
    __CFSpinLock(&__CFAllMessagePortsLock);
    if (NULL != name) {
	memory = __CFMessagePortLookUpInCache(&__CFAllRemoteMessagePorts, name);
	if (NULL != memory) {
	    __CFSpinUnlock(&__CFAllMessagePortsLock);
	    CFRelease(name);
	    CFAllocatorDeallocate(allocator, utfname);
	    return (CFMessagePortRef)CFRetain(memory);
	}
    }
    task_get_bootstrap_port(mach_task_self(), &bp);
#warning CF: what sort of references on the port does this give us?
    ret = bootstrap_look_up(bp, utfname, &port);
    native = (KERN_SUCCESS == ret) ? CFMachPortCreateWithPort(allocator, port, __CFMessagePortDummyCallback, NULL, NULL) : NULL;
    CFAllocatorDeallocate(allocator, utfname);
    if (NULL == native) {
	if (NULL != name) {
	    CFRelease(name);
	}
	__CFSpinUnlock(&__CFAllMessagePortsLock);
	return NULL;
    }
    size = sizeof(struct __CFMessagePort) - sizeof(CFMessagePortContext);
    allocator = (NULL == allocator) ? CFRetain(CFAllocatorGetDefault()) : CFRetain(allocator);
    memory = CFAllocatorAllocate(allocator, size, 0);
    if (NULL == memory) {
	CFRelease(allocator);
	CFMachPortInvalidate(native);
	CFRelease(native);
	if (NULL != name) {
	    CFRelease(name);
	}
	__CFSpinUnlock(&__CFAllMessagePortsLock);
	return NULL;
    }
    __CFGenericInitBase(memory, NULL, __kCFMessagePortTypeID);
    memory->_allocator = allocator;
    __CFMessagePortSetValid(memory);
    __CFMessagePortSetRemote(memory);
    memory->_lock = 0;
    memory->_name = name;
    memory->_port = native;
    memory->_reply = NULL;
    memory->_desiredReply = 0;
    memory->_replyPort = NULL;
    memory->_source = NULL;
    memory->_callout = NULL;
    if (NULL != name) {
	CFDictionaryAddValue(__CFAllRemoteMessagePorts, name, memory);
    }
    __CFSpinUnlock(&__CFAllMessagePortsLock);
    return (CFMessagePortRef)memory;
}

void CFMessagePortInvalidate(CFMessagePortRef ms) {
    __CFGenericValidateType(ms, __kCFMessagePortTypeID);
    CFRetain(ms);
    __CFMessagePortLock(ms);
    if (__CFMessagePortIsValid(ms)) {
	__CFMessagePortUnsetValid(ms);
	if (__CFMessagePortIsRemote(ms)) {
	    __CFSpinLock(&__CFAllMessagePortsLock);
	    CFDictionaryRemoveValue(__CFAllRemoteMessagePorts, ms->_name);
	    __CFSpinUnlock(&__CFAllMessagePortsLock);
	    CFMachPortInvalidate(ms->_port);
#warning remote Mach port probably has a send ref that we need to dealloc here, gotten from bs server
	    CFRelease(ms->_port);
	    ms->_port = NULL;
	    if (NULL != ms->_replyPort) {
		CFMachPortInvalidate(ms->_replyPort);
		CFRelease(ms->_replyPort);
		ms->_replyPort = NULL;
	    }
	} else {
	    __CFSpinLock(&__CFAllMessagePortsLock);
	    CFDictionaryRemoveValue(__CFAllLocalMessagePorts, ms->_name);
	    __CFSpinUnlock(&__CFAllMessagePortsLock);
	    CFMachPortInvalidate(ms->_port);
	    CFRelease(ms->_port);
	    ms->_port = NULL;
            if (ms->_context.release) {
                ms->_context.release(ms->_context.info);
            }
            ms->_context.info = NULL;
	    if (NULL != ms->_source) {
		CFRunLoopSourceInvalidate(ms->_source);
		CFRelease(ms->_source);
		ms->_source = NULL;
	    }
	}
    }
    __CFMessagePortUnlock(ms);
    CFRelease(ms);
}

Boolean CFMessagePortIsValid(CFMessagePortRef ms) {
    __CFGenericValidateType(ms, __kCFMessagePortTypeID);
    return __CFMessagePortIsValid(ms);
}

Boolean CFMessagePortIsRemote(CFMessagePortRef ms) {
    __CFGenericValidateType(ms, __kCFMessagePortTypeID);
    return __CFMessagePortIsRemote(ms);
}

CFStringRef CFMessagePortGetName(CFMessagePortRef ms) {
    __CFGenericValidateType(ms, __kCFMessagePortTypeID);
    return ms->_name;
}

Boolean CFMessagePortSetName(CFMessagePortRef ms, CFStringRef name) {
    CFMessagePortRef existing;
    UInt8 *utfname = NULL;
    CFIndex utflen = 0;

    __CFGenericValidateType(ms, __kCFMessagePortTypeID);
//    if (__CFMessagePortIsRemote(ms)) return FALSE;
#warning make this an assertion
// and assert than newName is non-NULL

    name = __CFMessagePortSanitizeStringName(CFGetAllocator(ms), name, &utfname, &utflen);
    if (NULL == name) {
	return FALSE;
    }
    __CFSpinLock(&__CFAllMessagePortsLock);
    if (NULL != name) {
	existing = __CFMessagePortLookUpInCache(&__CFAllLocalMessagePorts, name);
	if (NULL != existing) {
	    __CFSpinUnlock(&__CFAllMessagePortsLock);
	    CFRelease(name);
	    CFAllocatorDeallocate(CFGetAllocator(ms), utfname);
	    return FALSE;
	}
    }
    if (NULL != name && !CFEqual(ms->_name, name)) {
	if (!__CFMessagePortNativeSetNameLocal(ms->_port, utfname)) {
	    __CFSpinUnlock(&__CFAllMessagePortsLock);
	    CFRelease(name);
	    CFAllocatorDeallocate(CFGetAllocator(ms), utfname);
	    return FALSE;
	}
	CFDictionaryRemoveValue(__CFAllLocalMessagePorts, ms->_name);
	CFRelease(ms->_name);
	ms->_name = name;
	CFDictionaryAddValue(__CFAllLocalMessagePorts, name, ms);
    }
    __CFSpinUnlock(&__CFAllMessagePortsLock);
    CFAllocatorDeallocate(CFGetAllocator(ms), utfname);
    return TRUE;
}

void CFMessagePortGetContext(CFMessagePortRef ms, CFMessagePortContext *context) {
    __CFGenericValidateType(ms, __kCFMessagePortTypeID);
#warning CF: assert that this is a local port
    CFAssert1(0 == context->version, __kCFLogAssertion, "%s(): context version not initialized to 0", __PRETTY_FUNCTION__);
    memmove(context, &ms->_context, sizeof(CFMessagePortContext));
}

SInt32 CFMessagePortSendRequest(CFMessagePortRef remote, SInt32 msgid, CFDataRef data, CFTimeInterval sendTimeout, CFTimeInterval rcvTimeout, Boolean expectReply, CFDataRef *returnData) {
    return CFMessagePortSendRequest2(remote, msgid, data, sendTimeout, rcvTimeout, expectReply, returnData, kCFRunLoopDefaultMode);
}
    
SInt32 CFMessagePortSendRequest2(CFMessagePortRef remote, SInt32 msgid, CFDataRef data, CFTimeInterval sendTimeout, CFTimeInterval rcvTimeout, Boolean expectReply, CFDataRef *returnDatap, CFStringRef replyMode) {
    struct __CFMessagePortMachMessage *sendmsg;
    CFAbsoluteTime finishDate;
    CFRunLoopRef currentRL = CFRunLoopGetCurrent();
    CFRunLoopSourceRef source = NULL;
    Boolean didRegister = FALSE;
    kern_return_t ret;

#warning CF: This should be an assert
    // if (!__CFMessagePortIsRemote(remote)) return -999;
    __CFMessagePortLock(remote);
    __CFMessagePortUnsetWantsReply(remote);
    if (NULL == remote->_replyPort) {
	remote->_replyPort = CFMachPortCreate(CFGetAllocator(remote), __CFMessagePortDummyCallback, NULL, NULL);
    }
    remote->_desiredReply = 1;
    sendmsg = __CFMessagePortCreateMessage(CFGetAllocator(remote), CFMachPortGetPort(remote->_port), (expectReply ? CFMachPortGetPort(remote->_replyPort) : MACH_PORT_NULL), remote->_desiredReply, msgid, (data ? CFDataGetBytePtr(data) : NULL), (data ? CFDataGetLength(data) : -1));
    __CFMessagePortUnlock(remote);
    if (expectReply) {
        source = CFMessagePortCreateRunLoopSource(CFGetAllocator(remote), remote, -100);
        didRegister = !CFRunLoopContainsSource(currentRL, source, replyMode);
	if (didRegister) {
            CFRunLoopAddSource(currentRL, source, replyMode);
	}
	CFRelease(source);
    }
#warning CF: send timeout not used
    remote->_reply = NULL;
    ret = mach_msg((mach_msg_header_t *)sendmsg, MACH_SEND_MSG, sendmsg->head.msgh_size, 0, MACH_PORT_NULL, 0, MACH_PORT_NULL);
    CFAllocatorDeallocate(CFGetAllocator(remote), sendmsg);
    if (KERN_SUCCESS != ret) {
	if (didRegister) {
	    CFRunLoopRemoveSource(currentRL, source, replyMode);
	}
	if (MACH_SEND_TIMED_OUT == ret) return kCFMessagePortSendTimeout;
#warning CF: this needs to be a real constant
	return -3;
    }
    if (!expectReply) {
	return kCFMessagePortSuccess;
    }
    if (NULL != returnDatap) {
	__CFMessagePortSetWantsReply(remote);
    }
    finishDate = rcvTimeout + CFAbsoluteTimeGetCurrent();
    for (;;) {
	CFRunLoopRunInMode(replyMode, rcvTimeout);
	rcvTimeout = finishDate - CFAbsoluteTimeGetCurrent();
	if (__CFMessagePortHasReply(remote) || rcvTimeout < 0.0) {
	    break;
	}
    }
    __CFMessagePortUnsetWantsReply(remote);
    if (didRegister) {
        CFRunLoopRemoveSource(currentRL, source, replyMode);
    }
    if (!__CFMessagePortHasReply(remote)) {
	return kCFMessagePortReceiveTimeout;
    }
    if (NULL != returnDatap) {
	*returnDatap = remote->_reply;
    }
    __CFMessagePortUnsetHasReply(remote);
    return kCFMessagePortSuccess;
}

SInt32 CFMessagePortSendRequest_new(CFMessagePortRef remote, SInt32 msgid, CFDataRef data, CFTimeInterval sendTimeout, CFTimeInterval rcvTimeout, CFStringRef replyMode, CFDataRef *returnDatap) {
    struct __CFMessagePortMachMessage *sendmsg;
    CFAbsoluteTime finishDate;
    CFRunLoopRef currentRL = CFRunLoopGetCurrent();
    CFRunLoopSourceRef source = NULL;
    Boolean didRegister = FALSE;
    kern_return_t ret;

#warning CF: This should be an assert
    // if (!__CFMessagePortIsRemote(remote)) return -999;
    __CFMessagePortLock(remote);
    __CFMessagePortUnsetWantsReply(remote);
    if (NULL == remote->_replyPort) {
	remote->_replyPort = CFMachPortCreate(CFGetAllocator(remote), __CFMessagePortDummyCallback, NULL, NULL);
    }
    remote->_desiredReply = 1;
    sendmsg = __CFMessagePortCreateMessage(CFGetAllocator(remote), CFMachPortGetPort(remote->_port), (replyMode != NULL ? CFMachPortGetPort(remote->_replyPort) : MACH_PORT_NULL), remote->_desiredReply, msgid, (data ? CFDataGetBytePtr(data) : NULL), (data ? CFDataGetLength(data) : -1));
    __CFMessagePortUnlock(remote);
    if (replyMode != NULL) {
        source = CFMessagePortCreateRunLoopSource(CFGetAllocator(remote), remote, -100);
        didRegister = !CFRunLoopContainsSource(currentRL, source, replyMode);
	if (didRegister) {
            CFRunLoopAddSource(currentRL, source, replyMode);
	}
	CFRelease(source);
    }
#warning CF: send timeout not used
    remote->_reply = NULL;
    ret = mach_msg((mach_msg_header_t *)sendmsg, MACH_SEND_MSG, sendmsg->head.msgh_size, 0, MACH_PORT_NULL, 0, MACH_PORT_NULL);
    CFAllocatorDeallocate(CFGetAllocator(remote), sendmsg);
    if (KERN_SUCCESS != ret) {
	if (didRegister) {
	    CFRunLoopRemoveSource(currentRL, source, replyMode);
	}
	if (MACH_SEND_TIMED_OUT == ret) return kCFMessagePortSendTimeout;
#warning CF: this needs to be a real constant
	return -3;
    }
    if (replyMode == NULL) {
	return kCFMessagePortSuccess;
    }
    if (NULL != returnDatap) {
	__CFMessagePortSetWantsReply(remote);
    }
    finishDate = rcvTimeout + CFAbsoluteTimeGetCurrent();
    for (;;) {
	CFRunLoopRunInMode(replyMode, rcvTimeout);
	rcvTimeout = finishDate - CFAbsoluteTimeGetCurrent();
	if (__CFMessagePortHasReply(remote) || rcvTimeout < 0.0) {
	    break;
	}
    }
    __CFMessagePortUnsetWantsReply(remote);
    if (didRegister) {
        CFRunLoopRemoveSource(currentRL, source, replyMode);
    }
    if (!__CFMessagePortHasReply(remote)) {
	return kCFMessagePortReceiveTimeout;
    }
    if (NULL != returnDatap) {
	*returnDatap = remote->_reply;
    }
    __CFMessagePortUnsetHasReply(remote);
    return kCFMessagePortSuccess;
}

static mach_port_t __CFMessagePortGetPort(void *info) {
    CFMessagePortRef ms = info;
    return __CFMessagePortIsRemote(ms) ? CFMachPortGetPort(ms->_replyPort) : CFMachPortGetPort(ms->_port);
}

static void *__CFMessagePortPerform(void *msg, CFIndex size, CFAllocatorRef allocator, void *info) {
    CFMessagePortRef ms = info;
    struct __CFMessagePortMachMessage *msgp = msg;
    struct __CFMessagePortMachMessage *replymsg;
    __CFMessagePortLock(ms);
    if (!__CFMessagePortIsValid(ms)) {
	__CFMessagePortUnlock(ms);
	return NULL;
    }
    if (0 < (SInt32)msgp->head.msgh_id) {				/* request */
	void *context_info;
	void (*context_release)(const void *);
	CFDataRef returnData, data = NULL;
	void *return_bytes = NULL;
	CFIndex return_len = 0;
	SInt32 msgid;
	if (NULL != ms->_context.retain) {
	    context_info = (void *)ms->_context.retain(ms->_context.info);
	    context_release = ms->_context.release;
	} else {
	    context_info = ms->_context.info;
	    context_release = NULL;
	}
	__CFMessagePortUnlock(ms);
	/* Create no-copy, no-free-bytes wrapper CFData */
	if (0 == msgp->body.msgh_descriptor_count) {
	    msgid = msgp->contents.msg0.msgid;
	    if (0 <= msgp->contents.msg0.byteslen) {
		data = CFDataCreateWithBytesNoCopy(kCFAllocatorSystemDefault, msgp->contents.msg0.bytes, msgp->contents.msg0.byteslen, kCFAllocatorNull);
	    }
	} else {
	    msgid = msgp->contents.msg1.msgid;
	    data = CFDataCreateWithBytesNoCopy(kCFAllocatorSystemDefault, msgp->contents.msg1.desc.out_of_line.address, msgp->contents.msg1.desc.out_of_line.size, kCFAllocatorNull);
	}
	returnData = ms->_callout(ms, msgid, data, context_info);
	/* Now, returnData could be (1) NULL, (2) an ordinary data < MAX_INLINE,
	(3) ordinary data >= MAX_INLINE, (4) a no-copy data < MAX_INLINE,
	(5) a no-copy data >= MAX_INLINE. In cases (2) and (4), we send the return
	bytes inline in the Mach message, so can release the returnData object
	here. In cases (3) and (5), we'll send the data out-of-line, we need to
	create a copy of the memory, which we'll have the kernel autodeallocate
	for us on send. In case (4) also, the bytes in the return data may be part
	of the bytes in "data" that we sent into the callout, so if the incoming
	data was received out of line, we wouldn't be able to clean up the out-of-line
	wad until the message was sent either, if we didn't make the copy. */
	if (NULL != returnData) {
	    return_len = CFDataGetLength(returnData);
	    if (return_len < __CFMessagePortMaxInlineBytes) {
		return_bytes = CFDataGetBytePtr(returnData);
	    } else {
		return_bytes = NULL;
		vm_allocate(mach_task_self(), (vm_address_t *)&return_bytes, return_len, TRUE);
		/* vm_copy would only be a win here if the source address
		   is page aligned; it is a lose in all other cases, since
		   the kernel will just do the memmove for us (but not in
		   as simple a way). */
		memmove(return_bytes, CFDataGetBytePtr(returnData), return_len);
	    }
	}
	replymsg = __CFMessagePortCreateMessage(allocator, msgp->head.msgh_remote_port, MACH_PORT_NULL, -1 * (SInt32)msgp->head.msgh_id, msgid, return_bytes, return_len);
	if (1 == replymsg->body.msgh_descriptor_count) {
	    replymsg->contents.msg1.desc.out_of_line.deallocate = TRUE;
	}
	if (data) CFRelease(data);
	if (1 == msgp->body.msgh_descriptor_count) {
	    __CFRecordAllocationEvent(__kCFVMDeallocateEvent, msgp->contents.msg1.desc.out_of_line.address, msgp->contents.msg1.desc.out_of_line.size, 0, NULL);  
	    vm_deallocate(mach_task_self(), (vm_address_t)msgp->contents.msg1.desc.out_of_line.address, msgp->contents.msg1.desc.out_of_line.size);
	}
	if (returnData) CFRelease(returnData);
	if (context_release) {
	    context_release(context_info);
	}
	return replymsg;
    } else if (ms->_desiredReply == -1 * (SInt32)msgp->head.msgh_id) {	/* possible reply */
	replymsg = (struct __CFMessagePortMachMessage *)msg;
	if (__CFMessagePortWantsReply(ms)) {
	    if (0 == replymsg->body.msgh_descriptor_count) {
		if (0 <= replymsg->contents.msg0.byteslen) {
		    ms->_reply = CFDataCreate(kCFAllocatorSystemDefault, replymsg->contents.msg0.bytes, replymsg->contents.msg0.byteslen);
		} else {
		    ms->_reply = NULL;
		}
	    } else {
#warning CF: should create a no-copy data here that has a custom VM-freeing allocator, and not vm_dealloc here
		ms->_reply = CFDataCreate(kCFAllocatorSystemDefault, replymsg->contents.msg1.desc.out_of_line.address, replymsg->contents.msg1.desc.out_of_line.size);
		__CFRecordAllocationEvent(__kCFVMDeallocateEvent, replymsg->contents.msg1.desc.out_of_line.address, replymsg->contents.msg1.desc.out_of_line.size, 0, NULL);  
		vm_deallocate(mach_task_self(), (vm_address_t)replymsg->contents.msg1.desc.out_of_line.address, replymsg->contents.msg1.desc.out_of_line.size);
	    }
	    __CFMessagePortSetHasReply(ms);
	} else {
	    if (1 == replymsg->body.msgh_descriptor_count) {
		__CFRecordAllocationEvent(__kCFVMDeallocateEvent, replymsg->contents.msg1.desc.out_of_line.address, replymsg->contents.msg1.desc.out_of_line.size, 0, NULL);  
		vm_deallocate(mach_task_self(), (vm_address_t)replymsg->contents.msg1.desc.out_of_line.address, replymsg->contents.msg1.desc.out_of_line.size);
	    }
	}
	__CFMessagePortUnlock(ms);
	CFRunLoopStop(CFRunLoopGetCurrent());
    } else {	/* discard message */
	if (1 == msgp->body.msgh_descriptor_count) {
	    __CFRecordAllocationEvent(__kCFVMDeallocateEvent, msgp->contents.msg1.desc.out_of_line.address, msgp->contents.msg1.desc.out_of_line.size, 0, NULL);  
	    vm_deallocate(mach_task_self(), (vm_address_t)msgp->contents.msg1.desc.out_of_line.address, msgp->contents.msg1.desc.out_of_line.size);
	}
	__CFMessagePortUnlock(ms);
    }
    return NULL;
}

CFRunLoopSourceRef CFMessagePortCreateRunLoopSource(CFAllocatorRef allocator, CFMessagePortRef ms, CFIndex order) {
    CFRunLoopSourceRef result = NULL;
    __CFGenericValidateType(ms, __kCFMessagePortTypeID);
#warning CF: This should be an assert
   // if (__CFMessagePortIsRemote(ms)) return NULL;
    __CFMessagePortLock(ms);
    if (NULL == ms->_source) {
	CFRunLoopSourceContext1 context;
	context.version = 1;
	context.info = (void *)ms;
	context.retain = (const void *(*)(const void *))CFRetain;
	context.release = (void (*)(const void *))CFRelease;
	context.copyDescription = (CFStringRef (*)(const void *))__CFMessagePortCopyDescription;
	context.equal = (Boolean (*)(const void *, const void *))__CFMessagePortEqual;
	context.hash = (CFHashCode (*)(const void *))__CFMessagePortHash;
	context.getPort = __CFMessagePortGetPort;
	context.perform = __CFMessagePortPerform;
	ms->_source = CFRunLoopSourceCreate(allocator, order, (CFRunLoopSourceContext *)&context);
    }
    if (NULL != ms->_source) {
	result = (CFRunLoopSourceRef)CFRetain(ms->_source);
    }
    __CFMessagePortUnlock(ms);
    return result;
}

