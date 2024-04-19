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
 * Queue.m
 * - yet another queue implementation
 */
/*
 * Modification History:
 * 
 * April 29, 1999	Dieter Siegmund (dieter@apple.com)
 * - initial revision
 */

#import "Queue.h"
#import <unistd.h>
#import <stdlib.h>
#import <stdio.h>

@implementation QueueEl
- initWithObject:obj
{
    [super init];
    value = obj;
    attr = NULL;
    return self;
}

- setNext:obj
{
    next = obj;
    return self;
}

- setValue:obj
{
    value = obj;
    return self;
}

- setAttr:(void *)a
{
    attr = a;
    return self;
}

- (void *)attr
{
    return attr;
}

- next
{
    return next;
}

- value
{
    return value;
}

@end

@implementation Queue
- head
{
    return head;
}

- append:obj
{
    id new = [[QueueEl alloc] initWithObject:obj];

    if (head == nil) {
	head = tail = new;
    }
    else {
	[tail setNext:new];
	tail = new;
    }
    count++;
    return self;
}

- remove
{
    id v;
    id old_head = head;

    if (head == nil)
	return nil;
    head = [head next];
    v = [old_head value];
    [old_head free];
    count--;
    return v;
}

- drain
{
    while ([self remove] != nil)
	;
    return self;
}

- (unsigned)count
{
    return (count);
}

- free
{
    id scan;

    for (scan = head; scan != nil; ) {
	id next = [scan next];
	[scan free];
	scan = next;
    }
    head = tail = nil;
    return (self);
}

- freeValues
{
    id scan;

    for (scan = head; scan != nil; scan = [scan next])
	[[scan value] free];
    return (self);
}

@end

