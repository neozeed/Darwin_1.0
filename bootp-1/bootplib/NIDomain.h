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
 * NIDomain.h
 * - simple object representing a netinfo domain
 */

/*
 * Modification History:
 * 
 * May 20, 1998	Dieter Siegmund (dieter@apple.com)
 * - initial revision
 */

#import <objc/Object.h>
#import	<mach/boolean.h>
#import <netinet/if_ether.h>
#import <netinet/in.h>
#import <netinfo/ni.h>
#import <netinfo/ni_util.h>
#import "interfaces.h"

#define NIPROP_MASTER		"master"
#define NI_DOMAIN_LOCAL		"."
#define NI_DOMAIN_PARENT	".."

@interface NIDomain : Object
{
    void *		handle;
    char *		domainname; 		/* path or host/tag */
    struct sockaddr_in	sockaddr;
    ni_name		tag;
    ni_name		master;
    boolean_t		is_master;
}

- initWithDomain:(ni_name) domain_name;
- initWithDomain:(ni_name) domain_name 
      Interfaces:(interface_list_t *)l; /* path or host/tag */
- initParentDomain:domain
    Interfaces:(interface_list_t *)l;	/* open parent of given domain */
- (boolean_t) openPath:(ni_name) domain_name; /* path */
- (boolean_t) openHost:(ni_name)host Tag:(ni_name)tag; /* host/tag */
- (void *) handle;
- (ni_name) domain_name;
- (ni_name) master;
- (struct in_addr) ip;
- (ni_name) tag;
- (boolean_t) isMaster;
- free;
@end

