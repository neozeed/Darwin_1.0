/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
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
 * interfaces.h
 * - get the list of inet interfaces in the system
 */

/*
 * Modification History
 * 02/23/98	Dieter Siegmund (dieter@apple.com)
 * - initial version
 */

#import <sys/socket.h>
#import <net/if.h>
#import <netinet/in.h>
#import <netinet/in_systm.h>
#import <netinet/ip.h>
#import <netinet/udp.h>
#import <netinet/bootp.h>
#import <net/if_arp.h>
#import <netinet/if_ether.h>
#import <net/if_dl.h>
#import <net/if_types.h>
#import <mach/boolean.h>
#import <sys/param.h>


/*
 * Type: interface_t
 * Purpose:
 *   Enclose IP and link-level information for a particular
 *   interface.
 * Note:
 *   This structure is self contained ie. it has no pointers 
 *   so that it may simply be copied without worrying about
 *   pointer references.
 */
typedef struct {
    char 		name[IFNAMSIZ + 1]; /* eg. en0 */
    short		flags;

    boolean_t		inet_valid;
    struct in_addr	addr;
    struct in_addr	mask;
    struct in_addr	netaddr;
    struct in_addr	broadcast;
    u_char		hostname[MAXHOSTNAMELEN + 1];

    boolean_t		link_valid;
    struct sockaddr_dl	link;
} interface_t;

typedef struct {
    interface_t *	list;
    int			count;
} interface_list_t;

interface_list_t * 	if_init();
interface_t * 		if_first_broadcast_inet(interface_list_t * intface);
interface_t *		if_lookupbyname(interface_list_t * intface, 
					const char * name);
interface_t *		if_lookupbylinkindex(interface_list_t * intface, 
					     int link_index);
interface_t *		if_lookupbyip(interface_list_t * intface,
				      struct in_addr iaddr);
interface_t *		if_lookupbysubnet(interface_list_t * intface, 
					  struct in_addr iaddr);
void			if_free(interface_list_t * * list_p);

static __inline__ char *
if_name(interface_t * if_p)
{
    return (if_p->name);
}

static __inline__ struct in_addr
if_addr(interface_t * if_p)
{
    return (if_p->addr);
}

static __inline__ struct in_addr
if_broadcast(interface_t * if_p)
{
    return (if_p->broadcast);
}

static __inline__ short
if_flags(interface_t * if_p)
{
    return (if_p->flags);
}

static __inline__ void
if_setflags(interface_t * if_p, short flags)
{
    if_p->flags = flags;
    return;
}

static __inline__ int
if_count(interface_list_t * list_p)
{
    return (list_p->count);
}

static __inline__ interface_t *		
if_at_index(interface_list_t * list_p, int i)
{
    if (i >= list_p->count || i < 0)
	return (NULL);
    return (list_p->list + i);
}

static __inline__ int
if_index(interface_list_t * list_p, interface_t * if_p)
{
    return (if_p - list_p->list);
}


static __inline__ int
dl_to_arp_hwtype(int dltype)
{
    int type;

    switch (dltype) {
    case IFT_ETHER:
	type = ARPHRD_ETHER;
	break;
    default:
	type = -1;
	break;
    }
    return (type);
}

static __inline__ int
if_link_arptype(interface_t * if_p)
{
    return (dl_to_arp_hwtype(if_p->link.sdl_type));
}


static __inline__ void *
if_link_address(interface_t * if_p)
{
    return (if_p->link.sdl_data + if_p->link.sdl_nlen);
}

static __inline__ int
if_link_length(interface_t * if_p)
{
    return (if_p->link.sdl_alen);
}
