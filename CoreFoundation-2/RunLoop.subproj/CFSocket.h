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
/*	CFSocket.h
	Copyright 1999-2000, Apple, Inc. All rights reserved.
*/

#if !defined(__COREFOUNDATION_CFSOCKET__)
#define __COREFOUNDATION_CFSOCKET__ 1

#if !defined(__MACOS8__)

#include <CoreFoundation/CFBase.h>
#include <CoreFoundation/CFData.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFRunLoop.h>

#if defined(__WIN32__)
    #include <winsock.h>
#endif /* __WIN32__ */

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

#if defined(__WIN32__)
    typedef SOCKET CFSocketNativeHandle;
#else
    typedef int CFSocketNativeHandle;
#if !defined(INVALID_SOCKET)
    #define INVALID_SOCKET (CFSocketNativeHandle)(-1)
#endif /* !INVALID_SOCKET */
#endif /* __WIN32__ */

typedef struct __CFSocket * CFSocketRef;

/* A CFSocket contains a native socket within a structure that can 
be used to read from the socket in the background and make the data
thus read available using a runloop source.  The callback used for
this may be of three types, as specified by the callBackTypes
argument when creating the CFSocket.

If kCFSocketReadCallBack is used, then data will not be
automatically read, but the callback will be called when data
is available to be read, or a new child socket is waiting to be
accepted.
    
If kCFSocketAcceptCallBack is used, then new child sockets will be
accepted and passed to the callback, with the data argument being
a pointer to a CFSocketNativeHandle.  This is usable only with
connection rendezvous sockets.

If kCFSocketDataCallBack is used, then data will be read in chunks
in the background and passed to the callback, with the data argument
being a CFDataRef.

Addresses are stored as CFDatas containing a struct sockaddr
appropriate for the protocol family; make sure that all fields are
filled in properly when passing in an address.  Also, do not close
the underlying native socket directly; instead, invalidate the CFSocket.
*/

typedef enum {
    kCFSocketSuccess = 0,
    kCFSocketError = -1,
    kCFSocketTimeout = -2
} CFSocketError;

typedef enum {
    kCFSocketNoCallBack = 0,
    kCFSocketReadCallBack = 1,
    kCFSocketAcceptCallBack = 2,
    kCFSocketDataCallBack = 3
} CFSocketCallBackType;

typedef void (*CFSocketCallBack)(CFSocketRef s, CFSocketCallBackType type, CFDataRef address, const void *data, void *info);
/* If the callback wishes to keep hold of address or data after the point that it returns, then it must copy them. */

typedef struct {
    CFIndex	version;
    void *	info;
    const void *(*retain)(const void *info);
    void	(*release)(const void *info);
    CFStringRef	(*copyDescription)(const void *info);
} CFSocketContext;

CF_EXPORT CFTypeID	CFSocketGetTypeID(void);

CF_EXPORT CFSocketRef	CFSocketCreate(CFAllocatorRef allocator, SInt32 protocolFamily, SInt32 socketType, SInt32 protocol, CFOptionFlags callBackTypes, CFSocketCallBack callout, const CFSocketContext *context);
CF_EXPORT CFSocketRef	CFSocketCreateWithNative(CFAllocatorRef allocator, CFSocketNativeHandle sock, CFOptionFlags callBackTypes, CFSocketCallBack callout, const CFSocketContext *context);

CF_EXPORT CFSocketError	CFSocketSetAddress(CFSocketRef s, CFDataRef address);
CF_EXPORT CFSocketError	CFSocketConnectToAddress(CFSocketRef s, CFDataRef address, CFTimeInterval timeout);
CF_EXPORT void		CFSocketInvalidate(CFSocketRef s);

CF_EXPORT Boolean	CFSocketIsValid(CFSocketRef s);
CF_EXPORT CFDataRef	CFSocketCopyAddress(CFSocketRef s);
CF_EXPORT CFDataRef	CFSocketCopyPeerAddress(CFSocketRef s);
CF_EXPORT void		CFSocketGetContext(CFSocketRef s, CFSocketContext *context);
CF_EXPORT CFSocketNativeHandle	CFSocketGetNative(CFSocketRef s);

CF_EXPORT CFRunLoopSourceRef	CFSocketCreateRunLoopSource(CFAllocatorRef allocator, CFSocketRef s, CFIndex order);

/* For convenience, a function is provided to send data using the socket with a timeout.  The timeout will be used only if the specified value is positive.  The address need not be specified if the socket is already connected. */
CF_EXPORT CFSocketError	CFSocketSendData(CFSocketRef s, CFDataRef address, CFDataRef data, CFTimeInterval timeout);

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* ! __MACOS8__ */

#endif /* ! __COREFOUNDATION_CFSOCKET__ */

