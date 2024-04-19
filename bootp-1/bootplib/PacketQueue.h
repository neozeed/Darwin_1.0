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
 * PacketQueue.h
 * - a queue of packets with a timeout
 */
/*
 * Modification History:
 * 
 * April 29, 1999	Dieter Siegmund (dieter@apple.com)
 * - initial revision
 */

#import "Queue.h"
#import "PacketQueueExported.h"
#import "Timeout.h"
#import "threadcompat.h"

@interface PacketQueue:Queue<Timeout, QueueAppend>
{
    MUTEX_STRUCT	lock;
    CONDITION_STRUCT	wakeup;
    volatile boolean_t	timed_out;
}

- init;
- drain;
- init;
- free;
- (boolean_t) timedOut;
- (void)clearTimeout;
- (void)wait;
- (void)lock;
- (void)unlock;

/* Timeout method: */
- (void)timeout;

/* PacketQueueExported method: */
- append:v;

@end
