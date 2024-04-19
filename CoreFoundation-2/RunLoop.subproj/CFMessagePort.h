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
/*	CFMessagePort.h
	Copyright 1998-2000, Apple, Inc. All rights reserved.
*/

#if !defined(__COREFOUNDATION_CFMESSAGEPORT__)
#define __COREFOUNDATION_CFMESSAGEPORT__ 1

#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFRunLoop.h>
#include <CoreFoundation/CFData.h>

#if defined(__cplusplus)
extern "C" {
#endif

    /* *** Some notes on the typical usage of CFMessagePort between a client library and a server.  Similar code can be used for peer-to-peer messages.  *** */

    /* The server registers with some unique well-known name.  The visibility of the registration is between the server and other tasks that have the same bootstrap port.  Below is the essence of client library code that messages the server ():
    
    CFStringRef ServerPortName;
    CFMessagePortRef ServerPort;
    CFAllocatorRef allocator = NULL;
    SInt32 messageId = 0; // some client-defined number
    CFDataRef data = NULL; // some client-specified data
    CFDataRef returnData = NULL;
    CFTimeInterval sendTimeout = (CFTimeInterval)10; // server gets 10 seconds to accept message
    CFTimeInterval receiveTimeout = (CFTimeInterval)10; // client gets 10 seconds to accept reply
    CFStringRef runLoopMode = kCFRunLoopDefaultMode; // client will wait for reply in default run loop mode (and hence be responsive to other sources in this mode, but unresponsive to sources associated with other modes --- a run loop source can be added in multiple modes).
    Boolean expectReply = TRUE;
    SInt32 result;

    ServerPortName = CFSTR("Apple Baz Server");
    ServerPort = CFMessagePortCreateRemote(allocator, ServerPortName);

    result = CFMessagePortSendRequest2(ServerPort, messageId, data, sendTimeout, receiveTimeout, expectReply, &returnData, runLoopMode);

    if (result == kCFMessagePortSuccess && expectReply) {
        // chew on the returnData and then release it
        CFRelease(returnData);
    }

    
    The server must create a message port, create a run loop source from the message port, add the run loop source to some run loop, and then run that run loop.  This may seem a bit convoluted, but run loops can wait on lots of things besides message ports.

    The server will dispatch when it receives a message on the message port.  Below is the essence of the server code:
    
    CFMessagePortContext context;
    CFMessagePortRef port;
    CFRunLoopRef runLoop;
    CFRunLoopSourceRef source;
    CFStringRef name = CFSTR("Apple Baz Server");

    context.version = 0;
    context.info = NULL;
    context.retain = NULL;
    context.release = NULL;
    context.copyDescription = NULL;

    port = CFMessagePortCreateLocal(NULL, name, SomeMessagePortCallBack, &context, NULL);
    if (!port) return;
    runLoop = CFRunLoopGetCurrent();
    source = CFMessagePortCreateRunLoopSource(NULL, port, 0);
    CFRunLoopAddSource(runLoop, source, kCFRunLoopDefaultMode);
    CFRelease(source);
    CFRelease(port);
    CFRunLoopRun(); // does not return

    The declaration for the dispatcher, SomeMessagePortCallBack(), (supplied as an argument in CFMessagePortCreateLocal()) would look something like:

    CFDataRef SomeMessagePortCallBack(CFMessagePortRef local, SInt32 messageId, CFDataRef data, void *contextInfo);

    The server is allowed to do anything it likes in this dispatcher, including sending and receiving messages on any CFMessagePort.  Typically, a case statement is run with messsageId, and the data is parsed for various arguments.  The server is not responsible for releasing data.  It will be released after the dispatcher has returned.

*/

typedef struct __CFMessagePort * CFMessagePortRef;

enum {
    kCFMessagePortSuccess = 0,
    kCFMessagePortSendTimeout = -1,
    kCFMessagePortReceiveTimeout = -2
};

typedef struct {
    CFIndex	version;
    void *	info;
    const void *(*retain)(const void *info);
    void	(*release)(const void *info);
    CFStringRef	(*copyDescription)(const void *info);
} CFMessagePortContext;

typedef CFDataRef (*CFMessagePortCallBack)(CFMessagePortRef local, SInt32 msgid, CFDataRef data, void *info);
/* If callout wants to keep a hold of the data past the return of the callout, it must COPY the data. This includes the case where the data is given to some routine which _might_ keep a hold of it; System will release returned CFData. */

CF_EXPORT CFTypeID	CFMessagePortGetTypeID(void);

#if !defined(CFRUNLOOP_NEW_API)
CF_EXPORT CFMessagePortRef	CFMessagePortCreateLocal(CFAllocatorRef allocator, CFStringRef name, CFMessagePortCallBack callout, CFMessagePortContext *context);
#else
#define CFMessagePortCreateLocal(A, N, C, X, B) CFMessagePortCreateLocal_new(A, N, C, X, B)
CF_EXPORT CFMessagePortRef	CFMessagePortCreateLocal_new(CFAllocatorRef allocator, CFStringRef name, CFMessagePortCallBack callout, CFMessagePortContext *context, Boolean *shouldFreeInfo);
#endif
CF_EXPORT CFMessagePortRef	CFMessagePortCreateRemote(CFAllocatorRef allocator, CFStringRef name);

CF_EXPORT Boolean	CFMessagePortIsRemote(CFMessagePortRef ms);
CF_EXPORT CFStringRef	CFMessagePortGetName(CFMessagePortRef ms);
CF_EXPORT Boolean	CFMessagePortSetName(CFMessagePortRef ms, CFStringRef newName);
CF_EXPORT void		CFMessagePortGetContext(CFMessagePortRef ms, CFMessagePortContext *context);
CF_EXPORT void		CFMessagePortInvalidate(CFMessagePortRef ms);
CF_EXPORT Boolean	CFMessagePortIsValid(CFMessagePortRef ms);

#if !defined(CFRUNLOOP_NEW_API)
CF_EXPORT SInt32	CFMessagePortSendRequest(CFMessagePortRef remote, SInt32 msgid, CFDataRef data, CFTimeInterval sendTimeout, CFTimeInterval rcvTimeout, Boolean expectReply, CFDataRef *returnData);
CF_EXPORT SInt32	CFMessagePortSendRequest2(CFMessagePortRef remote, SInt32 msgid, CFDataRef data, CFTimeInterval sendTimeout, CFTimeInterval rcvTimeout, Boolean expectReply, CFDataRef *returnData, CFStringRef replyMode);
#else
/* NULL replyMode argument means no return value expected, dont wait for it */
#define CFMessagePortSendRequest(P, I, D, ST, RT, M, RD) CFMessagePortSendRequest_new(P, I, D, ST, RT, M, RD)
CF_EXPORT SInt32	CFMessagePortSendRequest_new(CFMessagePortRef remote, SInt32 msgid, CFDataRef data, CFTimeInterval sendTimeout, CFTimeInterval rcvTimeout, CFStringRef replyMode, CFDataRef *returnData);
#endif

CF_EXPORT CFRunLoopSourceRef	CFMessagePortCreateRunLoopSource(CFAllocatorRef allocator, CFMessagePortRef ms, CFIndex order);

#if defined(__cplusplus)
}
#endif

#endif /* ! __COREFOUNDATION_CFMESSAGEPORT__ */

