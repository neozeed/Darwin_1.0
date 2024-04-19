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
/*
 * Receiver.h
 * - receive packets from a socket and propogate them to
 *   each of the registered queues
 */

/*
 * Modification History:
 * 
 * April 29, 1999	Dieter Siegmund (dieter@apple.com)
 * - initial revision
 */

#import <objc/Object.h>
#import "threadcompat.h"
#import <mach/boolean.h>
#import "Queue.h"
#import "MultiplexedQueue.h"

@interface Receiver:Object
{
    MUTEX_STRUCT	lock;
    CONDITION_STRUCT	wakeup;
    boolean_t		thread_running;
    boolean_t		receive;
    int			fd;
    id			rxQ;
    UTHREAD_T		receiver;
    char 		rxpkt[2048];
}

- initWithSocket:(int)sockfd;
#if 0
- initWithFileDescriptor:(int)fd;
#endif 0
- (void) waitForReceivers;
- free;
- start;
- stop;
- attach:(id<QueueAppend>) q;
- detach:(id<QueueAppend>) q;
- setRunning:(boolean_t)v;
- (boolean_t)isRunning;
- (boolean_t)packet:(void *)data Size:(unsigned)size From:(struct in_addr)ip;
@end
