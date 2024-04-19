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
 * Transmitter.h
 * - uses BPF to send a broadcast packet (IP, ARP)
 *   and the socket to send a non-broadcast UDP packet
 * - Note: BPF is limited to ethernet only
 */

/*
 * Modification History:
 * 
 * April 29, 1999	Dieter Siegmund (dieter@apple.com)
 * - initial revision
 */

#import <objc/Object.h>
#import "threadcompat.h"

@interface Transmitter:Object
{
    boolean_t		busy;
    MUTEX_STRUCT	lock;
    CONDITION_STRUCT	wakeup;

    /* used for broadcast/subnet local unicast */
    char 		sendbuf[2048];
    int 		ip_id;

    /* used for unicast */
    int			sockfd;
}

- init;
- setSocket:(int)fd;
- free;

- (int) transmit:(char *) if_name 
      DestHWType:(int)hwtype
      DestHWAddr:(void *)hwaddr
       DestHWLen:(int)hwlen
	  DestIP:(struct in_addr) dest_ip
	SourceIP:(struct in_addr) src_ip
	DestPort:(u_short) dest_port 
      SourcePort:(u_short) src_port
	    Data:(void *) data 
	  Length:(int) len;

- (int) transmit:(char *) if_name
	    Type:(short) type
	    Data:(void *) data 
	  Length:(int) len;
@end

