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
 * MultiplexedQueue.m
 * - a queue of queues
 * - used by Receiver object to propogate a packet to multiple receivers
 */

/*
 * Modification History:
 * 
 * April 29, 1999	Dieter Siegmund (dieter@apple.com)
 * - initial revision
 */

#import "MultiplexedQueue.h"
#import "Packet.h"
#import <objc/List.h>
#import <unistd.h>
#import <stdlib.h>
#import <stdio.h>

#import "dprintf.h"

@implementation MultiplexedQueue
- init
{
    [super init];
    mutex_init(&lock);
    condition_init(&wakeup);
    list = [[List alloc] initCount:12];
    return self;
}

- (unsigned)count
{
    int c;

    mutex_lock(&lock);
    c = [list count];
    mutex_unlock(&lock);
    return (c);
}

- attach:q
{
    mutex_lock(&lock);
    [list addObject:q];
    mutex_unlock(&lock);
    return self;
}

- detach:q
{
    mutex_lock(&lock);
    [list removeObject:q];
    condition_signal(&wakeup);
    mutex_unlock(&lock);
    return self;
}

- free
{
    mutex_lock(&lock);
    while ([list count] > 0) {
	dprintf(("MultiplexedQueue: waiting for queue to drain\n"));
	condition_wait(&wakeup, &lock);
    }
    mutex_unlock(&lock);
    dprintf(("MultiplexedQueue: drained\n"));
    condition_clear(&wakeup);
    mutex_clear(&lock);
    [list free];
    return [super free];
}

- deliverData:(void *) data Size:(int)size From:(struct in_addr)from
{
    unsigned	count;
    unsigned 	i;
    
    mutex_lock(&lock);
    count = [list count];
    if (count > 0) {
	id		pkt;
	pkt = [[Packet alloc] initData:data Size:size From:from
			      RefCount:count];
	if (pkt != nil) {
	    for (i = 0; i < count; i++) {
		id q = [list objectAt:i];
		[q append:pkt];
	    }
	}
    }
    mutex_unlock(&lock);
    return self;
}
@end

