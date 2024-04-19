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
 * PacketQueue.m
 * - a queue of packets with a timeout
 */
/*
 * Modification History:
 * 
 * April 29, 1999	Dieter Siegmund (dieter@apple.com)
 * - initial revision
 */

#import "PacketQueue.h"

#import <unistd.h>
#import <stdlib.h>
#import <stdio.h>

#import "dprintf.h"

@implementation PacketQueue
- drain
{
    mutex_lock(&lock);
    [super drain];
    mutex_unlock(&lock);
    return self;
}

- append:v
{
    id obj;

    mutex_lock(&lock);
    obj = [super append:v];
    condition_signal(&wakeup);
    mutex_unlock(&lock);
    return obj;
}

- (void)timeout
{
    dprintf(("PacketQueue: %x timeout\n", (unsigned)self));
    mutex_lock(&lock);
    timed_out = TRUE;
    condition_signal(&wakeup);
    mutex_unlock(&lock);
    dprintf(("PacketQueue: timeout completed\n"));
    return;
}

- init
{
    [super init];
    mutex_init(&lock);
    condition_init(&wakeup);
    return self;
}

- free
{
    mutex_clear(&lock);
    condition_clear(&wakeup);
    return [super free];
}

- (boolean_t) timedOut
{
    return (timed_out);
}

- (void) clearTimeout
{
    timed_out = FALSE;
    return;
}

- (void)lock
{
    mutex_lock(&lock);
}

- (void) wait
{
    condition_wait(&wakeup, &lock);
}

- (void)unlock
{
    mutex_unlock(&lock);
}

@end
