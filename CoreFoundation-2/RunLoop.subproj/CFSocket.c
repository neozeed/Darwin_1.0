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
/*	CFSocket.c
	Copyright 1999-2000, Apple, Inc. All rights reserved.
	Responsibility: Doug Davidson
*/

#define FD_SETSIZE 256
#include <sys/types.h>
#include <math.h>
#include <limits.h>
#include <CoreFoundation/CFArray.h>
#include <CoreFoundation/CFData.h>
#include <CoreFoundation/CFDictionary.h>
#include <CoreFoundation/CFRunLoop.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFSocket.h>
#include "CFInternal.h"
#if defined(__WIN32__)
#include <winsock.h>
//#include <errno.h>
#elif defined(__MACH__)
#include <libc.h>
#else
#include <sys/filio.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#endif

#define MAX_SOCKADDR_LEN 256
#define MAX_DATA_SIZE 65536

/* locks are to be acquired in the following order:
   (1) __CFAllSocketsLock
   (2) an individual CFSocket's lock
   (3) __CFReadSocketsLock
*/
static SInt32 __CFAllSocketsLock = 0; /* controls __CFAllSockets */
static CFMutableDictionaryRef __CFAllSockets = NULL;
static SInt32 __CFReadSocketsLock = 0; /* controls __CFReadSockets and __CFReadSocketsFds */
static CFMutableArrayRef __CFReadSockets = NULL;
static fd_set __CFReadSocketsFds = {{0}};

static CFSocketNativeHandle __CFWakeupSocketPair[2] = {0, 0};
static void *__CFSocketManagerThread = NULL;

#if defined(__WIN32__)
static Boolean __CFSocketWinSockInitialized = FALSE;
#else
#define CFSOCKET_USE_SOCKETPAIR
#define closesocket(a) close((a))
#define ioctlsocket(a,b,c) ioctl((a),(b),(c))
#endif

struct __CFSocket {
    CFRuntimeBase _base;
    CFAllocatorRef _allocator;
    UInt32 _lock;
    CFSocketNativeHandle _socket;	/* immutable */
    SInt32 _socketType;
    CFDataRef _address;
    CFDataRef _peerAddress;
    SInt32 _socketSetCount;
    CFRunLoopSourceRef _source;
    CFMutableArrayRef _runLoops;
    CFSocketCallBack _callout;		/* immutable */
    CFSocketContext _context;		/* immutable */
    CFIndex _maxQueueLen;
    CFMutableArrayRef _dataQueue;
    CFMutableArrayRef _addressQueue;
};

/* Bit 3 in the base reserved bits is used for invalid state (mutable) */
/* Bits 4-7 in the base reserved bits are used for callback types (immutable) */
/* Of this, bits 4-5 are used for the read callback type. */

CF_INLINE Boolean __CFSocketIsValid(CFSocketRef s) {
    return (Boolean)__CFBitfieldValue(((CFRuntimeBase *)s)->_info, 3, 3);
}

CF_INLINE void __CFSocketSetValid(CFSocketRef s) {
    __CFBitfieldSetValue(((CFRuntimeBase *)s)->_info, 3, 3, 1);
}

CF_INLINE void __CFSocketUnsetValid(CFSocketRef s) {
    __CFBitfieldSetValue(((CFRuntimeBase *)s)->_info, 3, 3, 0);
}

CF_INLINE UInt8 __CFSocketCallBackTypes(CFSocketRef s) {
    return (UInt8)__CFBitfieldValue(((CFRuntimeBase *)s)->_info, 7, 4);
}

CF_INLINE UInt8 __CFSocketReadCallBackType(CFSocketRef s) {
    return (UInt8)__CFBitfieldValue(((CFRuntimeBase *)s)->_info, 5, 4);
}

CF_INLINE void __CFSocketSetCallBackTypes(CFSocketRef s, UInt8 types) {
    __CFBitfieldSetValue(((CFRuntimeBase *)s)->_info, 7, 4, types & 0xF);
}

CF_INLINE void __CFSocketLock(CFSocketRef s) {
    __CFSpinLock((UInt32 *)&(s->_lock));
}

CF_INLINE void __CFSocketUnlock(CFSocketRef s) {
    __CFSpinUnlock((UInt32 *)&(s->_lock));
}

CF_INLINE Boolean __CFSocketIsConnectionOriented(CFSocketRef s) {
    return (SOCK_STREAM == s->_socketType || SOCK_SEQPACKET == s->_socketType);
}

CF_INLINE void __CFSocketEstablishAddress(CFSocketRef s) {
    /* socket should already be locked */
    UInt8 name[MAX_SOCKADDR_LEN];
    int namelen = sizeof(name);
    if (__CFSocketIsValid(s) && NULL == s->_address && INVALID_SOCKET != s->_socket && 0 == getsockname(s->_socket, (struct sockaddr *)name, &namelen) && NULL != name && 0 < namelen) {
        s->_address = CFDataCreate(CFGetAllocator(s), name, namelen);
    }
}

CF_INLINE void __CFSocketEstablishPeerAddress(CFSocketRef s) {
    /* socket should already be locked */
    UInt8 name[MAX_SOCKADDR_LEN];
    int namelen = sizeof(name);
    if (__CFSocketIsValid(s) && NULL == s->_peerAddress && INVALID_SOCKET != s->_socket & 0 == getpeername(s->_socket, (struct sockaddr *)name, &namelen) && NULL != name && 0 < namelen) {
        s->_peerAddress = CFDataCreate(CFGetAllocator(s), name, namelen);
    }
}
    
static SInt32 __CFSocketCreateWakeupSocketPair(void) {
#if defined(CFSOCKET_USE_SOCKETPAIR)
    return socketpair(PF_LOCAL, SOCK_DGRAM, 0, __CFWakeupSocketPair);
#else
    //??? should really use native Win32 facilities
    UInt32 i;
    SInt32 error = 0;
    struct sockaddr_in address[2];
    int namelen = sizeof(struct sockaddr_in);
    for (i = 0; i < 2; i++) {
        __CFWakeupSocketPair[i] = socket(PF_INET, SOCK_DGRAM, 0);
        CFZeroMemory(&(address[i]), sizeof(struct sockaddr_in));
        address[i].sin_family = AF_INET;
        address[i].sin_addr.s_addr = INADDR_LOOPBACK;
        if (0 <= error) error = bind(__CFWakeupSocketPair[i], (struct sockaddr *)&(address[i]), sizeof(struct sockaddr_in));
        if (0 <= error) error = getsockname(__CFWakeupSocketPair[i], (struct sockaddr *)&(address[i]), &namelen);
        if (sizeof(struct sockaddr_in) != namelen) error = -1;
    }
    if (0 <= error) error = connect(__CFWakeupSocketPair[0], (struct sockaddr *)&(address[1]), sizeof(struct sockaddr_in));
    if (0 <= error) error = connect(__CFWakeupSocketPair[1], (struct sockaddr *)&(address[0]), sizeof(struct sockaddr_in));
    if (0 > error) {
        closesocket(__CFWakeupSocketPair[0]);
        closesocket(__CFWakeupSocketPair[1]);
    }
    return error;
#endif
}

static void __CFSocketInitialize(void) {
    UInt32 yes = 1;
#if defined(__WIN32__)
    if (!__CFSocketWinSockInitialized) {
        WORD versionRequested = MAKEWORD(1, 1);
        WSADATA wsaData;
        int errorStatus = WSAStartup(versionRequested, &wsaData);
        if (errorStatus != 0 || LOBYTE(wsaData.wVersion) != LOBYTE(versionRequested) || HIBYTE(wsaData.wVersion) != HIBYTE(versionRequested)) {
            WSACleanup();
            CFLog(0, CFSTR("*** Could not initialize WinSock subsystem!!!"));
        }
        __CFSocketWinSockInitialized = TRUE;
    }
#endif
    __CFReadSockets = CFArrayCreateMutable(kCFAllocatorSystemDefault, 0, NULL);
    FD_ZERO(&__CFReadSocketsFds);
    if (0 > __CFSocketCreateWakeupSocketPair()) {
        CFLog(0, CFSTR("*** Could not create wakeup socket pair for CFSocket!!!"));
    } else {
        /* wakeup sockets must be non-blocking */
        ioctlsocket(__CFWakeupSocketPair[0], FIONBIO, &yes);
        ioctlsocket(__CFWakeupSocketPair[1], FIONBIO, &yes);
        FD_SET(__CFWakeupSocketPair[1], &__CFReadSocketsFds);
    }
}

static void __CFSocketWakeUpWaitingRunLoop(CFSocketRef s) {
    CFRunLoopRef rl = NULL;
    SInt32 idx, cnt = CFArrayGetCount(s->_runLoops);
    if (0 == cnt) return;
    if (1 == cnt) {
	rl = (CFRunLoopRef)CFArrayGetValueAtIndex(s->_runLoops, 0);
        CFRunLoopWakeUp(rl);
        return;
    }
    for (idx = 0; idx < cnt; idx++) {
	CFRunLoopRef value = (CFRunLoopRef)CFArrayGetValueAtIndex(s->_runLoops, idx);
	CFStringRef currentMode = CFRunLoopCopyCurrentMode(value);
	if (NULL != currentMode && CFRunLoopIsWaiting(value) && CFRunLoopContainsSource(value, s->_source, currentMode)) {
	    CFRelease(currentMode);
	    /* ideally, this would be a run loop which isn't also in a
	    * signaled state for this or another source, but that's tricky;
	    * we move this run loop to the end of the list to scramble them
	    * a bit, and always search from the front */
	    CFArrayRemoveValueAtIndex(s->_runLoops, idx);
	    rl = value;
	    break;
	}
	if (NULL != currentMode) CFRelease(currentMode);
    }
    if (NULL == rl) {	/* didn't choose one above, so choose first */
	rl = (CFRunLoopRef)CFArrayGetValueAtIndex(s->_runLoops, 0);
	CFArrayRemoveValueAtIndex(s->_runLoops, 0);
    }
    CFArrayAppendValue(s->_runLoops, rl);
    CFRunLoopWakeUp(rl);
}

static void __CFSocketHandleRead(CFSocketRef s) {
    static CFDataRef zeroLengthData = NULL;
    if (NULL == zeroLengthData) zeroLengthData = CFDataCreateMutable(kCFAllocatorSystemDefault, 0);
    if (!CFSocketIsValid(s)) return;
    if (__CFSocketReadCallBackType(s) == kCFSocketDataCallBack) {
        UInt8 buffer[MAX_DATA_SIZE];
        UInt8 name[MAX_SOCKADDR_LEN];
        int namelen = sizeof(name);
        SInt32 recvlen = recvfrom(s->_socket, buffer, sizeof(buffer), 0, (struct sockaddr *)name, &namelen);
        CFDataRef data, address = NULL;
#if defined(LOG_CFSOCKET)
        printf("read %d bytes on socket %d\n", recvlen, s->_socket);
#endif
        if (0 >= recvlen) {
            //??? should return error if <0
            /* zero-length data is the signal for perform to invalidate */
            data = CFRetain(zeroLengthData);
        } else {
            data = CFDataCreate(CFGetAllocator(s), buffer, recvlen);
        }
        __CFSocketLock(s);
        if (!__CFSocketIsValid(s)) {
            CFRelease(data);
            __CFSocketUnlock(s);
            return;
        }
        if (NULL != name && 0 < namelen) {
            //??? possible optimizations:  uniquing; storing last value
            address = CFDataCreate(CFGetAllocator(s), name, namelen);
        } else if (__CFSocketIsConnectionOriented(s)) {
            if (NULL == s->_peerAddress) __CFSocketEstablishPeerAddress(s);
            if (NULL != s->_peerAddress) address = CFRetain(s->_peerAddress);
        }
        if (NULL == address) {
            address = CFRetain(zeroLengthData);
        }
        if (NULL == s->_dataQueue) {
            s->_dataQueue = CFArrayCreateMutable(CFGetAllocator(s), 0, &kCFTypeArrayCallBacks);
        }
        if (NULL == s->_addressQueue) {
            s->_addressQueue = CFArrayCreateMutable(CFGetAllocator(s), 0, &kCFTypeArrayCallBacks);
        }
        CFArrayAppendValue(s->_dataQueue, data);
        CFRelease(data);
        CFArrayAppendValue(s->_addressQueue, address);
        CFRelease(address);
        if (0 < recvlen && 0 < s->_socketSetCount && (0 == s->_maxQueueLen || CFArrayGetCount(s->_dataQueue) < s->_maxQueueLen)) {
            __CFSpinLock(&__CFReadSocketsLock);
            /* restore socket to fds */
            FD_SET(s->_socket, &__CFReadSocketsFds);
            __CFSpinUnlock(&__CFReadSocketsLock);
        }
    } else if (__CFSocketReadCallBackType(s) == kCFSocketAcceptCallBack) {
        UInt8 name[MAX_SOCKADDR_LEN];
        int namelen = sizeof(name);
        CFSocketNativeHandle sock = accept(s->_socket, (struct sockaddr *)name, &namelen);
        CFDataRef address;
        if (INVALID_SOCKET == sock) {
            //??? should return error
            return;
        }
        if (NULL != name && 0 < namelen) {
            address = CFDataCreate(CFGetAllocator(s), name, namelen);
        } else {
            address = CFRetain(zeroLengthData);
        }
        __CFSocketLock(s);
        if (!__CFSocketIsValid(s)) {
            closesocket(sock);
            CFRelease(address);
            __CFSocketUnlock(s);
            return;
        }
        if (NULL == s->_dataQueue) {
            s->_dataQueue = CFArrayCreateMutable(CFGetAllocator(s), 0, NULL);
        }
        if (NULL == s->_addressQueue) {
            s->_addressQueue = CFArrayCreateMutable(CFGetAllocator(s), 0, &kCFTypeArrayCallBacks);
        }
        CFArrayAppendValue(s->_dataQueue, (void *)sock);
        CFArrayAppendValue(s->_addressQueue, address);
        CFRelease(address);
        if (0 < s->_socketSetCount && (0 == s->_maxQueueLen || CFArrayGetCount(s->_dataQueue) < s->_maxQueueLen)) {
            __CFSpinLock(&__CFReadSocketsLock);
            /* restore socket to fds */
            FD_SET(s->_socket, &__CFReadSocketsFds);
            __CFSpinUnlock(&__CFReadSocketsLock);
        }
    } else {
        __CFSocketLock(s);
        if (!__CFSocketIsValid(s)) {
            __CFSocketUnlock(s);
            return;
        }
    }
    CFRunLoopSourceSignal(s->_source);
#if defined(LOG_CFSOCKET)
    printf("read signaling source for socket %d\n", s->_socket);
#endif
    __CFSocketWakeUpWaitingRunLoop(s);
    __CFSocketUnlock(s);
}

static void * __CFSocketManager(void * arg) {
    fd_set readfds;
    SInt32 nrfds;
    SInt32 idx, cnt;
    UInt8 buffer[256];
    CFMutableArrayRef selectedSockets = CFArrayCreateMutable(kCFAllocatorSystemDefault, 0, &kCFTypeArrayCallBacks);
    for (;;) {       
	__CFSpinLock(&__CFReadSocketsLock);
#if defined(LOG_CFSOCKET)
        printf("socket manager looking at sockets ");
        for (idx = 0, cnt = CFArrayGetCount(__CFReadSockets); idx < cnt; idx++) {
            CFSocketRef s = (CFSocketRef)CFArrayGetValueAtIndex(__CFReadSockets, idx);
            if (FD_ISSET(s->_socket, &__CFReadSocketsFds)) {
                printf("%d ", s->_socket);
            } else {
                printf("(%d) ", s->_socket);
            }
        }
        printf("\n");
#endif
        FD_COPY(&__CFReadSocketsFds, &readfds);
	__CFSpinUnlock(&__CFReadSocketsLock);

        //??? should check for errors
        nrfds = select(FD_SETSIZE, &readfds, NULL, NULL, NULL);
        if (0 >= nrfds) continue;
        if (FD_ISSET(__CFWakeupSocketPair[1], &readfds)) {
            recv(__CFWakeupSocketPair[1], buffer, sizeof(buffer), 0);
#if defined(LOG_CFSOCKET)
            printf("socket manager received %c on wakeup socket\n", buffer);
#endif
        }
        if (0 == (cnt = CFArrayGetCount(__CFReadSockets))) continue;
        __CFSpinLock(&__CFReadSocketsLock);
        for (idx = 0; idx < cnt; idx++) {
            CFSocketRef s = (CFSocketRef)CFArrayGetValueAtIndex(__CFReadSockets, idx);
            if (FD_ISSET(s->_socket, &readfds)) {
                CFArrayAppendValue(selectedSockets, s);
                /* socket is removed from fds here, will be restored in read handling or in perform function */
                FD_CLR(s->_socket, &__CFReadSocketsFds);
            }
        }
        __CFSpinUnlock(&__CFReadSocketsLock);
        for (idx = 0, cnt = CFArrayGetCount(selectedSockets); idx < cnt; idx++) {
            CFSocketRef s = (CFSocketRef)CFArrayGetValueAtIndex(selectedSockets, idx);
#if defined(LOG_CFSOCKET)
            printf("socket manager signaling socket %d\n", s->_socket);
#endif
            __CFSocketHandleRead(s);
        }
        CFArrayRemoveAllValues(selectedSockets);
    }
    return (void *)0;
}

CFTypeID CFSocketGetTypeID(void) {
    return __kCFSocketTypeID;
}

Boolean __CFSocketEqual(CFTypeRef cf1, CFTypeRef cf2) {
    return ((cf1 == cf2) ? TRUE : FALSE);
}

CFHashCode __CFSocketHash(CFTypeRef cf) {
    return (CFHashCode)cf;
}

CFStringRef __CFSocketCopyDescription(CFTypeRef cf) {
    CFSocketRef s = (CFSocketRef)cf;
    CFMutableStringRef result;
    CFStringRef contextDesc = NULL;
    result = CFStringCreateMutable(CFGetAllocator(s), 0);
    __CFSocketLock(s);
    CFStringAppendFormat(result, NULL, CFSTR("<CFSocket 0x%x [0x%x]>{locked = %s, valid = %s, type = %d, socket = %d, socket set count = %d\n    callback types = 0x%x, callout = 0x%x, context = "), (UInt32)cf, (UInt32)CFGetAllocator(s), (s->_lock ? "Yes" : "No"), (__CFSocketIsValid(s) ? "Yes" : "No"), s->_socketType, s->_socket, s->_socketSetCount, __CFSocketCallBackTypes(s), s->_callout);
    if (NULL != s->_context.copyDescription) {
	contextDesc = (CFStringRef)INVOKE_CALLBACK1(s->_context.copyDescription, s->_context.info);
    }
    if (NULL != contextDesc) {
	CFStringAppend(result, contextDesc);
	CFRelease(contextDesc);
    } else {
	contextDesc = CFStringCreateWithFormat(CFGetAllocator(s), NULL, CFSTR("<CFSocket context 0x%x>"), (UInt32)s->_context.info);
	CFStringAppend(result, contextDesc);
	CFRelease(contextDesc);
    }
    CFStringAppendFormat(result, NULL, CFSTR(",\n    source = 0x%x,\n    run loops = %@\n}"), (UInt32)s->_source, s->_runLoops);
    __CFSocketUnlock(s);
    return result;
}

CFAllocatorRef __CFSocketGetAllocator(CFTypeRef cf) {
    CFSocketRef s = (CFSocketRef)cf;
    return s->_allocator;
}

void __CFSocketDeallocate(CFTypeRef cf) {
    /* Since CFSockets are cached, we can only get here sometime after being invalidated */
    CFSocketRef s = (CFSocketRef)cf;
    CFAllocatorRef allocator = CFGetAllocator(s);
    if (NULL != s->_address) {
        CFRelease(s->_address);
        s->_address = NULL;
    }
    CFAllocatorDeallocate(allocator, s);
    CFRelease(allocator);
}

CFSocketError CFSocketSetAddress(CFSocketRef s, CFDataRef address) {
    const UInt8 *name;
    SInt32 namelen, result = 0;
    __CFGenericValidateType(s, __kCFSocketTypeID);
    if (NULL == address) return -1;
    name = CFDataGetBytePtr(address);
    namelen = CFDataGetLength(address);
    __CFSocketLock(s);
    if (__CFSocketIsValid(s) && INVALID_SOCKET != s->_socket && NULL != name && 0 < namelen) {
        result = bind(s->_socket, (struct sockaddr *)name, namelen);
        if (0 == result) {
            __CFSocketEstablishAddress(s);
            listen(s->_socket, FD_SETSIZE);
        }
    }
    if (NULL == s->_address && NULL != name && 0 < namelen && 0 == result) {
        s->_address = CFDataCreateCopy(CFGetAllocator(s), address);
    }   
    __CFSocketUnlock(s);
    //??? should return errno
    return result;
}

void CFSocketSetAcceptBacklog(CFSocketRef s, CFIndex limit) {
    __CFGenericValidateType(s, __kCFSocketTypeID);
    __CFSocketLock(s);
    if (__CFSocketIsValid(s) && INVALID_SOCKET != s->_socket) {
        listen(s->_socket, limit);
    }
    __CFSocketUnlock(s);
}

void CFSocketSetMaximumQueueLength(CFSocketRef s, CFIndex length) {
    __CFGenericValidateType(s, __kCFSocketTypeID);
    __CFSocketLock(s);
    if (__CFSocketIsValid(s)) {
        s->_maxQueueLen = length;
    }
    __CFSocketUnlock(s);
}

CFSocketError CFSocketConnectToAddress(CFSocketRef s, CFDataRef address, CFTimeInterval timeout) {
    //??? need timeout
    const UInt8 *name;
    SInt32 namelen, result = 0;
    __CFGenericValidateType(s, __kCFSocketTypeID);
    name = CFDataGetBytePtr(address);
    namelen = CFDataGetLength(address);
    __CFSocketLock(s);
    if (__CFSocketIsValid(s) && INVALID_SOCKET != s->_socket && NULL != name && 0 < namelen) {
        result = connect(s->_socket, (struct sockaddr *)name, namelen);
        if (0 == result) {
            __CFSocketEstablishPeerAddress(s);
            if (NULL == s->_peerAddress && NULL != name && 0 < namelen && __CFSocketIsConnectionOriented(s)) {
                s->_peerAddress = CFDataCreateCopy(CFGetAllocator(s), address);
            }
        }
    }
    __CFSocketUnlock(s);
    //??? should return errno
    return result;
}

CFSocketRef CFSocketCreate(CFAllocatorRef allocator, SInt32 protocolFamily, SInt32 socketType, SInt32 protocol, CFOptionFlags callBackTypes, CFSocketCallBack callout, const CFSocketContext *context) {
    CFSocketNativeHandle sock = INVALID_SOCKET;
    CFSocketRef s = NULL;
    if (0 >= protocolFamily) protocolFamily = PF_INET;
    if (PF_INET == protocolFamily) {
        if (0 >= socketType) socketType = SOCK_STREAM;
        if (0 >= protocol && SOCK_STREAM == socketType) protocol = IPPROTO_TCP;
        if (0 >= protocol && SOCK_DGRAM == socketType) protocol = IPPROTO_UDP;
    }
    if (PF_LOCAL == protocolFamily && 0 >= socketType) socketType = SOCK_STREAM;
    sock = socket(protocolFamily, socketType, protocol);
    if (INVALID_SOCKET != sock) {
        s = CFSocketCreateWithNative(allocator, sock, callBackTypes, callout, context);
    }
    return s;
}

CFSocketRef CFSocketCreateWithNative(CFAllocatorRef allocator, CFSocketNativeHandle sock, CFOptionFlags callBackTypes, CFSocketCallBack callout, const CFSocketContext *context) {
    CFSocketRef memory;
    UInt32 size;
    int typeSize = sizeof(memory->_socketType);
    __CFSpinLock(&__CFReadSocketsLock);
    if (NULL == __CFReadSockets) __CFSocketInitialize();
    __CFSpinUnlock(&__CFReadSocketsLock);
    __CFSpinLock(&__CFAllSocketsLock);
    if (NULL == __CFAllSockets) {
	__CFAllSockets = CFDictionaryCreateMutable(kCFAllocatorSystemDefault, 0, NULL, &kCFTypeDictionaryValueCallBacks);
    }
    if (INVALID_SOCKET != sock && CFDictionaryGetValueIfPresent(__CFAllSockets, (void *)sock, (const void **)&memory)) {
	__CFSpinUnlock(&__CFAllSocketsLock);
	CFRetain(memory);
	return memory;
    }
    size = sizeof(struct __CFSocket);
    allocator = (NULL == allocator) ? CFRetain(CFAllocatorGetDefault()) : CFRetain(allocator);
    memory = CFAllocatorAllocate(allocator, size, 0);
    if (NULL == memory) {
	CFRelease(allocator);
	__CFSpinUnlock(&__CFAllSocketsLock);
	return NULL;
    }
    __CFGenericInitBase(memory, NULL, __kCFSocketTypeID);
    __CFSocketSetCallBackTypes(memory, callBackTypes);
    memory->_allocator = allocator;
    if (INVALID_SOCKET != sock) __CFSocketSetValid(memory);
    memory->_lock = 0;
    memory->_socket = sock;
    if (INVALID_SOCKET == sock || 0 != getsockopt(sock, SOL_SOCKET, SO_TYPE, &(memory->_socketType), &typeSize)) memory->_socketType = 0;
    memory->_address = NULL;
    memory->_peerAddress = NULL;
    memory->_socketSetCount = 0;
    memory->_source = NULL;
    if (INVALID_SOCKET != sock) {
        memory->_runLoops = CFArrayCreateMutable(allocator, 0, NULL);
    } else {
        memory->_runLoops = NULL;
    }
    memory->_callout = callout;
    FAULT_CALLBACK((void **)&(memory->_callout));
    memory->_dataQueue = NULL;
    memory->_addressQueue = NULL;
    memory->_maxQueueLen = 0;
    if (NULL != context) {
	memory->_context.retain = context->retain;
        FAULT_CALLBACK((void **)&(memory->_context.retain));
	memory->_context.release = context->release;
        FAULT_CALLBACK((void **)&(memory->_context.release));
	memory->_context.copyDescription = context->copyDescription;
        FAULT_CALLBACK((void **)&(memory->_context.copyDescription));
	memory->_context.info = context->retain ? (void *)INVOKE_CALLBACK1(context->retain, context->info) : context->info;
    } else {
	memory->_context.info = 0;
	memory->_context.retain = 0;
	memory->_context.release = 0;
	memory->_context.copyDescription = 0;
    }
    if (INVALID_SOCKET != sock) CFDictionaryAddValue(__CFAllSockets, (void *)sock, memory);
    __CFSpinUnlock(&__CFAllSocketsLock);
    return memory;
}

void CFSocketInvalidate(CFSocketRef s) {
    __CFGenericValidateType(s, __kCFSocketTypeID);
    CFRetain(s);
    __CFSpinLock(&__CFAllSocketsLock);
    __CFSocketLock(s);
    if (__CFSocketIsValid(s)) {
	SInt32 idx;
	__CFSocketUnsetValid(s);
	__CFSpinLock(&__CFReadSocketsLock);
        if (NULL == __CFReadSockets) __CFSocketInitialize();
        idx = CFArrayGetFirstIndexOfValue(__CFReadSockets, CFRangeMake(0, CFArrayGetCount(__CFReadSockets)), s);
	if (0 <= idx) CFArrayRemoveValueAtIndex(__CFReadSockets, idx);
        FD_CLR(s->_socket, &__CFReadSocketsFds);
	__CFSpinUnlock(&__CFReadSocketsLock);
	CFDictionaryRemoveValue(__CFAllSockets, (void *)(s->_socket));
        closesocket(s->_socket);
	s->_socket = INVALID_SOCKET;
	if (NULL != s->_peerAddress) {
            CFRelease(s->_peerAddress);
            s->_peerAddress = NULL;
        }
	if (NULL != s->_dataQueue) {
            CFRelease(s->_dataQueue);
            s->_dataQueue = NULL;
        }
	if (NULL != s->_addressQueue) {
            CFRelease(s->_addressQueue);
            s->_addressQueue = NULL;
        }
        s->_socketSetCount = 0;
	for (idx = CFArrayGetCount(s->_runLoops); idx--;) {
	    CFRunLoopWakeUp((CFRunLoopRef)CFArrayGetValueAtIndex(s->_runLoops, idx));
	}
	CFRelease(s->_runLoops);
	s->_runLoops = NULL;
	/* Some folks might want this callout to occur before all the other stuff is torn down */
	if (NULL != s->_context.release) {
	    INVOKE_CALLBACK1(s->_context.release, s->_context.info);
	}
	s->_context.info = NULL;
	if (NULL != s->_source) {
	    CFRunLoopSourceInvalidate(s->_source);
	    CFRelease(s->_source);
	    s->_source = NULL;
	}
    }
    __CFSocketUnlock(s);
    __CFSpinUnlock(&__CFAllSocketsLock);
    CFRelease(s);
}

Boolean CFSocketIsValid(CFSocketRef s) {
    __CFGenericValidateType(s, __kCFSocketTypeID);
    return __CFSocketIsValid(s);
}

CFSocketNativeHandle CFSocketGetNative(CFSocketRef s) {
    __CFGenericValidateType(s, __kCFSocketTypeID);
    return s->_socket;
}

CFDataRef CFSocketCopyAddress(CFSocketRef s) {
    CFDataRef result = NULL;
    __CFGenericValidateType(s, __kCFSocketTypeID);
    __CFSocketLock(s);
    __CFSocketEstablishAddress(s);
    if (NULL != s->_address) {
	result = CFRetain(s->_address);
    }
    __CFSocketUnlock(s);
    return result;
}

CFDataRef CFSocketCopyPeerAddress(CFSocketRef s) {
    CFDataRef result = NULL;
    __CFGenericValidateType(s, __kCFSocketTypeID);
    __CFSocketLock(s);
    __CFSocketEstablishPeerAddress(s);
    if (NULL != s->_peerAddress) {
	result = CFRetain(s->_peerAddress);
    }
    __CFSocketUnlock(s);
    return result;
}

void CFSocketGetContext(CFSocketRef s, CFSocketContext *context) {
    __CFGenericValidateType(s, __kCFSocketTypeID);
    CFAssert1(0 == context->version, __kCFLogAssertion, "%s(): context version not initialized to 0", __PRETTY_FUNCTION__);
    *context = s->_context;
}

static void __CFSocketSchedule(void *info, CFRunLoopRef rl, CFStringRef mode) {
    CFSocketRef s = info;
    Boolean wakeup = FALSE;
    UInt8 c = 's';
    __CFSocketLock(s);
    //??? also need to arrange delivery of all pending data
    if (__CFSocketIsValid(s)) {
	CFArrayAppendValue(s->_runLoops, rl);
	s->_socketSetCount++;
	if (1 == s->_socketSetCount) {
#if defined(LOG_CFSOCKET)
    printf("scheduling socket %d\n", s->_socket);
#endif
            __CFSpinLock(&__CFReadSocketsLock);
	    if (NULL == __CFReadSockets) __CFSocketInitialize();
            if (__CFSocketReadCallBackType != kCFSocketNoCallBack) {
                CFArrayAppendValue(__CFReadSockets, s);
                FD_SET(s->_socket, &__CFReadSocketsFds);
                wakeup = TRUE;
            }
            __CFSpinUnlock(&__CFReadSocketsLock);
	}
	if (NULL == __CFSocketManagerThread) {
	    __CFSocketManagerThread = __CFStartSimpleThread(__CFSocketManager, 0);
	}
    }
    __CFSocketUnlock(s);
    if (wakeup) send(__CFWakeupSocketPair[0], &c, sizeof(c), 0);
}

static void __CFSocketCancel(void *info, CFRunLoopRef rl, CFStringRef mode) {
    CFSocketRef s = info;
    SInt32 idx;
    __CFSocketLock(s);
    s->_socketSetCount--;
    if (0 == s->_socketSetCount) {
	__CFSpinLock(&__CFReadSocketsLock);
        if (NULL == __CFReadSockets) __CFSocketInitialize();
	idx = CFArrayGetFirstIndexOfValue(__CFReadSockets, CFRangeMake(0, CFArrayGetCount(__CFReadSockets)), s);
	if (0 <= idx) CFArrayRemoveValueAtIndex(__CFReadSockets, idx);
        FD_CLR(s->_socket, &__CFReadSocketsFds);
	__CFSpinUnlock(&__CFReadSocketsLock);
    }
    if (NULL != s->_runLoops) {
	idx = CFArrayGetFirstIndexOfValue(s->_runLoops, CFRangeMake(0, CFArrayGetCount(s->_runLoops)), rl);
	if (0 <= idx) CFArrayRemoveValueAtIndex(s->_runLoops, idx);
    }
    __CFSocketUnlock(s);
}

static void __CFSocketPerform(void *info) {
    CFSocketRef s = info;
    CFDataRef data = NULL;
    CFDataRef address = NULL;
    CFSocketNativeHandle sock = INVALID_SOCKET;
    Boolean wakeup = FALSE;
    UInt8 c = 'p', readCallBackType;
    __CFSocketLock(s);
    if (!__CFSocketIsValid(s)) {
	__CFSocketUnlock(s);
	return;
    }
#if defined(LOG_CFSOCKET)
    printf("entering perform for socket %d\n", s->_socket);
#endif
    readCallBackType = __CFSocketReadCallBackType(s);
    if (kCFSocketDataCallBack == readCallBackType) {
        if (NULL != s->_dataQueue && 0 < CFArrayGetCount(s->_dataQueue)) {
            data = CFArrayGetValueAtIndex(s->_dataQueue, 0);
            CFRetain(data);
            CFArrayRemoveValueAtIndex(s->_dataQueue, 0);
            address = CFArrayGetValueAtIndex(s->_addressQueue, 0);
            CFRetain(address);
            CFArrayRemoveValueAtIndex(s->_addressQueue, 0);
        }
    } else if (kCFSocketAcceptCallBack == readCallBackType) {
        if (NULL != s->_dataQueue && 0 < CFArrayGetCount(s->_dataQueue)) {
            sock = (CFSocketNativeHandle)CFArrayGetValueAtIndex(s->_dataQueue, 0);
            CFArrayRemoveValueAtIndex(s->_dataQueue, 0);
            address = CFArrayGetValueAtIndex(s->_addressQueue, 0);
            CFRetain(address);
            CFArrayRemoveValueAtIndex(s->_addressQueue, 0);
        }
    }
    __CFSocketUnlock(s);
    if (kCFSocketDataCallBack == readCallBackType) {
        if (NULL != data) {
            SInt32 datalen = CFDataGetLength(data);
#if defined(LOG_CFSOCKET)
    printf("perform calling out data to socket %d\n", s->_socket);
#endif
            INVOKE_CALLBACK5(s->_callout, s, kCFSocketDataCallBack, address, data, s->_context.info);
            CFRelease(data);
            CFRelease(address);
            if (0 == datalen) CFSocketInvalidate(s);
        }
    } else if (kCFSocketAcceptCallBack == readCallBackType) {
        if (INVALID_SOCKET != sock) {
#if defined(LOG_CFSOCKET)
    printf("perform calling out accept to socket %d\n", s->_socket);
#endif
            INVOKE_CALLBACK5(s->_callout, s, kCFSocketAcceptCallBack, address, &sock, s->_context.info);
            CFRelease(address);
        }
    } else if (kCFSocketReadCallBack == readCallBackType) {
#if defined(LOG_CFSOCKET)
    printf("perform calling out read to socket %d\n", s->_socket);
#endif
        INVOKE_CALLBACK5(s->_callout, s, kCFSocketReadCallBack, NULL, NULL, s->_context.info);
    }
    __CFSocketLock(s);
    if (__CFSocketIsValid(s) && kCFSocketNoCallBack != readCallBackType) {
        if (NULL != s->_dataQueue && 0 < CFArrayGetCount(s->_dataQueue)) {
            CFRunLoopSourceSignal(s->_source);
#if defined(LOG_CFSOCKET)
            printf("perform signaling source for socket %d\n", s->_socket);
#endif
            __CFSocketWakeUpWaitingRunLoop(s);
        }
        if (0 < s->_socketSetCount) {
            __CFSpinLock(&__CFReadSocketsLock);
            /* restore socket to fds */
            FD_SET(s->_socket, &__CFReadSocketsFds);
            wakeup = TRUE;
            __CFSpinUnlock(&__CFReadSocketsLock);
        }
    }
    __CFSocketUnlock(s);
    if (wakeup) send(__CFWakeupSocketPair[0], &c, sizeof(c), 0);
}

CFRunLoopSourceRef CFSocketCreateRunLoopSource(CFAllocatorRef allocator, CFSocketRef s, CFIndex order) {
    CFRunLoopSourceRef result = NULL;
    __CFGenericValidateType(s, __kCFSocketTypeID);
    __CFSocketLock(s);
    if (__CFSocketIsValid(s)) {
        if (NULL == s->_source) {
            CFRunLoopSourceContext context;
            context.version = 0;
            context.info = (void *)s;
            context.retain = (const void *(*)(const void *))CFRetain;
            context.release = (void (*)(const void *))CFRelease;
            context.copyDescription = (CFStringRef (*)(const void *))__CFSocketCopyDescription;
            context.equal = (Boolean (*)(const void *, const void *))__CFSocketEqual;
            context.hash = (CFHashCode (*)(const void *))__CFSocketHash;
            context.schedule = __CFSocketSchedule;
            context.cancel = __CFSocketCancel;
            context.perform = __CFSocketPerform;
            s->_source = CFRunLoopSourceCreate(allocator, order, &context);
        }
        CFRetain(s->_source);	/* This retain is for the receiver */
        result = s->_source;
    }
    __CFSocketUnlock(s);
    return result;
}

//??? need timeout, error handling, retries
CFSocketError CFSocketSendData(CFSocketRef s, CFDataRef address, CFDataRef data, CFTimeInterval timeout) {
    const UInt8 *dataptr, *addrptr = NULL;
    SInt32 datalen, addrlen = 0, size = 0;
    CFSocketNativeHandle sock = INVALID_SOCKET;
    struct timeval tv;
    __CFGenericValidateType(s, __kCFSocketTypeID);
    if (address) {
        addrptr = CFDataGetBytePtr(address);
        addrlen = CFDataGetLength(address);
    }
    dataptr = CFDataGetBytePtr(data);
    datalen = CFDataGetLength(data);
    __CFSocketLock(s);
    if (__CFSocketIsValid(s)) sock = s->_socket;
    __CFSocketUnlock(s);
    if (INVALID_SOCKET != sock) {
        tv.tv_sec = (0 >= timeout || INT_MAX <= timeout) ? INT_MAX : (SInt32)floor(timeout);
        tv.tv_usec = (int32_t)((timeout - floor(timeout)) * 1.0E6);
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
        if (NULL != addrptr && 0 < addrlen) {
            size = sendto(sock, dataptr, datalen, 0, (struct sockaddr *)addrptr, addrlen);
        } else {
            size = send(sock, dataptr, datalen, 0);
        }
#if defined(LOG_CFSOCKET)
        printf("wrote %d bytes to socket %d\n", size, s->_socket);
#endif
    }
    return (size > 0) ? kCFSocketSuccess : kCFSocketError;
}

