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
 * Packet.h
 * - store a read-only packet with a reference count
 * - used to allow multiple threads to share a copy of the
 *   same packet
 * - packet is discarded when the reference count drops to 0
 */

/*
 * Modification History:
 * 
 * April 29, 1999	Dieter Siegmund (dieter@apple.com)
 * - initial revision
 */

#import <objc/Object.h>
#import <sys/types.h>
#import <netinet/in.h>
#import "threadcompat.h"

@interface Packet:Object
{
    void *		data;
    int			size;
    struct in_addr	from;
    int			ref_count;
    MUTEX_STRUCT	lock;
    int			number;
}

- initData:(void *)data Size:(int) size From:(struct in_addr)from
  RefCount:(int)ref_count;
- free;
- (int) size;
- (void *) data;
- (struct in_addr)from;
- (int) number;
@end
