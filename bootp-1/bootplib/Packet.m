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
 * Packet.m
 * - store a read-only packet with a reference count
 * - used to allow multiple threads to share a copy of the
 *   same packet
 * - packet is discarded when the reference count drops to 0
 */

#import "Packet.h"
#import <unistd.h>
#import <stdlib.h>
#import <stdio.h>
#import <string.h>

#import "dprintf.h"

static int count = 0;

/* Assumption: multiple free'ers, but single init'er,
 * hence no lock on count.
 */

@implementation Packet
- initData:(void *)d Size:(int)s From:(struct in_addr)ip RefCount:(int)r
{
    [super init];

    data = malloc(s);
    if (data == NULL)
	return [super free];
    memcpy(data, d, s);
    size = s;
    from = ip;
    ref_count = r;
    number = ++count;
    mutex_init(&lock);
    return self;
}

- free
{
    mutex_lock(&lock);
    if (ref_count && --ref_count > 0) {
	mutex_unlock(&lock);
	return nil;
    }
    mutex_unlock(&lock);
    dprintf(("[Packet free]: object %x number %d\n", self, number));
    if (data)
	free(data);
    data = NULL;
    mutex_clear(&lock);
    return [super free];
}

- (int) number
{
    return number;
}

- (int) size
{
    return size;
}

- (void *) data
{
    return data;
}

- (struct in_addr) from
{
    return from;
}
@end

