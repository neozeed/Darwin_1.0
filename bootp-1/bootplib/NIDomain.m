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
 * NIDomain.m
 * - simple object interface to a netinfo domain
 */

/*
 * Modification History:
 * 
 * May 20, 1998	Dieter Siegmund (dieter@apple.com)
 * - initial revision
 */

#import	<netdb.h>
#import <string.h>
#import <sys/types.h>
#import <sys/socket.h>
#import <net/if.h>
#import <netinet/in.h>
#import <netinet/if_ether.h>
#import <arpa/inet.h>
#import <string.h>
#import <unistd.h>
#import <stdlib.h>
#import <stdio.h>
#import <objc/List.h>
#import "dprintf.h"
#import <netinfo/ni_util.h>
#import "NIDomain.h"

static __inline__ boolean_t
S_has_path_component(u_char * path, u_char * comp, u_char sep)
{
    u_char * path_comp;
    u_char * sep_ptr;

    if (strcmp(path, comp) == 0)
	return (TRUE);

    for (path_comp = path, sep_ptr = strchr(path, sep); sep_ptr; 
	 sep_ptr = strchr(path_comp = (sep_ptr + 1), sep)) {
	if (strncmp(path_comp, comp, sep_ptr - path_comp) == 0) {
	    return (TRUE);
	}
    }
    return (FALSE);
}

static __inline__ boolean_t
is_this_our_name(interface_list_t * list, char * name)
{
    struct hostent * h;

    h = gethostbyname(name);
    if (h && h->h_addr_list && *h->h_addr_list) {
	struct in_addr * * a = (struct in_addr * *)h->h_addr_list;
	while (*a) {
	    dprintf(("address '%s'\n", inet_ntoa(**a)));
	    if (if_lookupbyip(list, **a)) {
		dprintf(("We are master of domain (name = %s)\n", name));
		return (TRUE);
	    }
	    a++;
	}
    }
    return (FALSE);
}

@implementation NIDomain
- (void)setMaster:(interface_list_t *)list
{
    ni_id		dir;
    ni_namelist		nl;
    u_char *		slash;
    ni_status		status;

    if (list == NULL)
	return;

    status = ni_pathsearch(handle, &dir, "/");
    if (status != NI_OK)
	return;
    
    NI_INIT(&nl);
    status = ni_lookupprop(handle, &dir, NIPROP_MASTER, &nl);
    if (status != NI_OK)
	return;

    if (nl.ninl_len && (slash = strchr(nl.ninl_val[0], '/'))) {
	int		len;
	u_char		tmp[256];

	len = slash - (u_char *) nl.ninl_val[0];
	strncpy(tmp, nl.ninl_val[0], len);
	tmp[len] = '\0';
	master = ni_name_dup(tmp);
	is_master = is_this_our_name(list, master);
    }
    ni_namelist_free(&nl);
    return;
}

- initParentDomain:domain Interfaces:(interface_list_t *)iflist
{
    ni_status 	status;
    u_char	tmp[256];

    [super init];
    
    sprintf(tmp, "%s/%s", [domain domain_name], NI_DOMAIN_PARENT);
    domainname = ni_name_dup(tmp);
    status = ni_open([domain handle], NI_DOMAIN_PARENT, &handle);
    if (status != NI_OK)
	return [self free];
    status = ni_addrtag(handle, &sockaddr, &tag);
    if (status != NI_OK)
	return [self free];
    [self setMaster:iflist];
    return (self);
	
}

- initWithDomain:(ni_name) domain_name
{
    return [self initWithDomain:domain_name Interfaces:NULL];
}

- initWithDomain:(ni_name) domain_name Interfaces:(interface_list_t *)iflist
{
    [super init];

    if (domain_name[0] == '/' 
	|| S_has_path_component(domain_name, "..", '/')
	|| S_has_path_component(domain_name, ".", '/')) { /* path */
	/* domain_name is an absolute/relative path */
	if ([self openPath:domain_name]) {
	    [self setMaster:iflist];
	    return self;
	}
    }
    else { /* not a path */
	char * slash;
	slash = strchr(domain_name, '/');
	if (slash && slash == strrchr(domain_name, '/')) {
	    char hostname[128];
	    
	    /* connect to hostname/tag */
	    strncpy(hostname, domain_name, slash - domain_name);
	    hostname[slash - domain_name] = '\0';
	    if ([self openHost:hostname Tag:slash + 1]) {
		[self setMaster:iflist];
		return self;
	    }
	}
    }
    return [self free];
}

- (boolean_t) openPath:(ni_name) domain_name
{
    ni_status 	status;

    domainname = ni_name_dup(domain_name);

    status = ni_open(NULL, domainname, &handle);
    if (status != NI_OK)
	return (FALSE);
    status = ni_addrtag(handle, &sockaddr, &tag);
    if (status != NI_OK)
	return (FALSE);
    return (TRUE);
}

- (boolean_t) openHost:(ni_name)host Tag:(ni_name)t
{
    struct hostent * 	h;
    char 		host_tag[128];

    tag = ni_name_dup(t);
    sprintf(host_tag, "%s/%s", host, tag);
    domainname = ni_name_dup(host_tag);

    h = gethostbyname(host);
    if (h != NULL && h->h_addrtype == AF_INET) {
	struct in_addr * * s = (struct in_addr * *)h->h_addr_list;
	while (*s) {
	    sockaddr.sin_len = sizeof(struct sockaddr_in);
	    sockaddr.sin_family = AF_INET;
	    sockaddr.sin_addr = **s;
	    handle = ni_connect(&sockaddr, tag);
	    if (handle != NULL) {
		break;
	    }
	    s++;
	}
    }
    if (handle == NULL)
	return (FALSE);
    return (TRUE);
}

- (void *) handle
{
    return handle;
}

- (ni_name)domain_name
{
    return (domainname);
}

- (ni_name)tag
{
    return (tag);
}

- (struct in_addr)ip
{
    return (sockaddr.sin_addr);
}

- (ni_name) master
{
    return (master);
}

- free
{
    if (handle != NULL)
	ni_free(handle);
    if (domainname != NULL)
	ni_name_free(&domainname);
    if (tag != NULL)
	ni_name_free(&tag);
    if (master != NULL)
	ni_name_free(&master);
    handle = NULL;
    return [super free];
}

- (boolean_t) isMaster
{
    return (is_master);
}
@end

