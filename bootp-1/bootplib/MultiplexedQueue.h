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
 * MultiplexedQueue.h
 * - a queue of queues
 * - used by Receiver object to propogate a packet to multiple receivers
 */
/*
 * Modification History:
 * 
 * April 29, 1999	Dieter Siegmund (dieter@apple.com)
 * - initial revision
 */

#import "Queue.h"
#import "PacketQueueExported.h"
#import "threadcompat.h"

@interface MultiplexedQueue:Object
{
    MUTEX_STRUCT	lock;
    CONDITION_STRUCT	wakeup;
    id			list;
}
- init;
- attach:(id<QueueAppend>) q;
- detach:(id<QueueAppend>) q;
- free;
- deliverData:(void *) data Size:(int)size From:(struct in_addr)from;
- (unsigned)count;
@end
