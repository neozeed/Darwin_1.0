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

#import <mach/boolean.h>
#import <sys/time.h>
#import "dhcpOptions.h"
#import "gen_dhcp_tags.h"

/*
 * Function: is_dhcp_packet
 *
 * Purpose:
 *   Return whether packet is a DHCP packet.
 *   If the packet contains DHCP message ids, then its a DHCP packet.
 */
static __inline__ boolean_t
is_dhcp_packet(id options, dhcp_msgtype_t * msgtype)
{
    u_char * opt;
    int opt_len;

    if (options != nil) {
	opt = [options findOptionWithTag:dhcptag_dhcp_message_type_e
	       Length:&opt_len];
	if (opt != NULL) {
	    if (msgtype)
		*msgtype = *opt;
	    return (TRUE);
	}
    }
    return (FALSE);
}

static __inline__ struct timeval
timeval_from_secs(dhcp_time_secs_t secs)
{
    struct timeval tv;
    tv.tv_sec = secs;
    tv.tv_usec = 0;
    return (tv);
}

static __inline__ dhcp_time_secs_t
current_secs()
{
    struct timeval tv;

    gettimeofday(&tv, 0);
    return (tv.tv_sec);
}

