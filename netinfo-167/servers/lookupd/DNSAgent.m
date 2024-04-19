/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 * DNSAgent.m
 *
 * DNS lookup agent for lookupd
 *
 * Copyright (c) 1995, NeXT Computer Inc.
 * All rights reserved.
 * Written by Marc Majka
 */

#import <config.h>
#import "DNSAgent.h"
#import "LUPrivate.h"
#import "Controller.h"
#import "Syslog.h"
#import <sys/types.h>
#import <netinet/in.h>
#import <netdb.h>
#import <sys/socket.h>
#import <arpa/inet.h>
#import <string.h>
#import <arpa/nameser.h>
#import <sys/param.h>
#import "stringops.h"
#import <stdio.h>
#import <string.h>
#import <libc.h>

#define DefaultServerTimeout 2

extern char *nettoa(unsigned long);

@implementation DNSAgent

int
msg_callback(int priority, char *msg)
{
	[lookupLog log:msg priority:priority];
	return 0;
}

+ (DNSAgent *)alloc
{
	DNSAgent *agent;
	char str[128];

	agent = [super alloc];

	sprintf(str, "Allocated DNSAgent 0x%x\n", (int)agent);
	[lookupLog syslogDebug:str];

	return agent;
}

- (DNSAgent *)init
{
	LUDictionary *config;
	char *dn, *x;
	struct timeval t;
	unsigned long r;
	BOOL didSetTimeout;

	[super init];

	dn = NULL;
	r = 2;
	t.tv_sec = 0; 
	t.tv_usec = 0;
	allHostsEnabled = YES;
	didSetTimeout = NO;

	config = [controller configurationForAgent:"DNSAgent"];
	if (config != nil)
	{
		if ([config hasValue:"NO" forKey:"AllHostsEnabled"])
			allHostsEnabled = NO;
		dn = [config valueForKey:"Domain"];
		x = [config valueForKey:"Timeout"];
		if (x != NULL)
		{
			didSetTimeout = YES;
			t.tv_sec = [config unsignedLongForKey:"Timeout"];
		}
		x = [config valueForKey:"Retries"];
		if (x != NULL) r = [config unsignedLongForKey:"Retries"];
	}

	[controller rpcLock];
	dns = dns_open(dn);
	dns_open_log(dns, "DNSAgent", DNS_LOG_CALLBACK, NULL, 0, 0, msg_callback);
	[controller rpcUnlock];
	if (config != nil) [config release];
	if (dns == NULL)
	{
		[self dealloc];
		return nil;
	}

	dns_set_server_retries(dns, r);

	if (didSetTimeout == NO)
	{
		t.tv_sec = DefaultServerTimeout;
		t.tv_usec = 0;
	}

	if (t.tv_sec == 0) t.tv_sec = 1;
	dns_set_server_timeout(dns, &t);

	stats = nil;
	[self resetStatistics];

	return self;
}

- (const char *)name
{
	return "Domain_Name_System";
}

- (const char *)shortName
{
	return "DNSAgent";
}

- (void)dealloc
{
	char str[128];

	if (stats != nil) [stats release];
	dns_free(dns);

	sprintf(str, "Deallocated DNSAgent 0x%x\n", (int)self);
	[lookupLog syslogDebug:str];

	[super dealloc];
}

- (LUDictionary *)statistics
{
	return stats;
}

- (void)resetStatistics
{
	int i;
	char str[32];

	if (stats != nil) [stats release];
	stats = [[LUDictionary alloc] init];
	[stats setBanner:"DNSAgent statistics"];
	[stats setValue:"Domain_Name_System" forKey:"information_system"];
	[stats setValue:dns->domain forKey:"domain_name"];
	for (i = 0; i < dns->server_count; i++)
		[stats mergeValue:inet_ntoa(dns->server[i].sin_addr) forKey:"nameserver"];

#ifdef _UNIX_BSD_43_
	sprintf(str, "%lu+%lu", dns->timeout.tv_sec, dns->timeout.tv_usec);
#else
	sprintf(str, "%u+%u", dns->timeout.tv_sec, dns->timeout.tv_usec);
#endif
	[stats setValue:str forKey:"timeout"];

#ifdef _UNIX_BSD_43_
	sprintf(str, "%lu+%lu", dns->server_timeout.tv_sec, dns->server_timeout.tv_usec);
#else
	sprintf(str, "%u+%u", dns->server_timeout.tv_sec, dns->server_timeout.tv_usec);
#endif

	[stats setValue:str forKey:"server_timeout"];

	sprintf(str, "%u", dns->server_retries);
	[stats setValue:str forKey:"server_retries"];

	[stats setValue:dns->domain forKey:"domain_name"];
}

- (BOOL)isValid:(LUDictionary *)item
{
	time_t now, ttl;
	time_t bestBefore;

	if (item == nil) return NO;

	bestBefore = [item unsignedLongForKey:"_lookup_DNS_timestamp"];
	ttl = [item unsignedLongForKey:"_lookup_DNS_time_to_live"];
	bestBefore += ttl;

	now = time(0);
	if (now > bestBefore) return NO;
	return YES;
}

- (LUDictionary *)stamp:(LUDictionary *)item
{
	if (item == nil) return nil;

	[item setAgent:self];
	[item setValue:"DNS" forKey:"_lookup_info_system"];
	return item;
}

- (LUDictionary *)dictForDNSReply:(dns_reply_t *)r
{
	LUDictionary *host;
	char *longName = NULL;
	char *shortName = NULL;
	char *domainName = NULL;
	int i, got_data;
	time_t now;
	char scratch[32];
	int alias;

	if (r == NULL) return nil;
	if (r->status != DNS_STATUS_OK) return nil;
	if ((r->header->flags & DNS_FLAGS_RCODE_MASK) != DNS_FLAGS_RCODE_NO_ERROR)
		return nil;

	if (r->header->ancount == 0) return nil;

 	host = [[LUDictionary alloc] init];
	sprintf(scratch, "%u", r->answer[0]->ttl);
	[host setValue:scratch forKey:"_lookup_DNS_time_to_live"];
	now = time(0);
	sprintf(scratch, "%lu", now);
	[host setValue:scratch forKey:"_lookup_DNS_timestamp"];

	[host setValue:inet_ntoa(r->server) forKey:"_lookup_DNS_server"];

	alias = 0;
	got_data = 0;
	longName = lowerCase(r->answer[0]->name);
	
	for (i = 0; i < r->header->ancount; i++)
	{
		if (r->answer[i]->type == DNS_TYPE_A)
		{
			got_data++;
			[host mergeValue:inet_ntoa(r->answer[i]->data.A->addr)
				forKey:"ip_address"];
		}

		else if (r->answer[i]->type == DNS_TYPE_CNAME)
		{
			got_data++;
			alias++;
			freeString(longName);
			longName = lowerCase(r->answer[i]->name);
			[host mergeValue:longName forKey:"alias"];

			/* Real name of the host is value of CNAME */
			freeString(longName);
			longName = lowerCase(r->answer[i]->data.CNAME->name);
		}

		else if (r->answer[i]->type == DNS_TYPE_PTR)
		{
			got_data++;
			
			/* Save referring name in case someone wants it later */
			[host mergeValue:r->answer[i]->name forKey:"ptr_name"];

			/* Real name of the host is value of PTR */
			freeString(longName);
			longName = lowerCase(r->answer[i]->data.PTR->name);
		}
	}

	if (got_data == 0)
	{
		[host release];
		return nil;
	}

	[host mergeValue:longName forKey:"name"];

	shortName = prefix(longName, '.');
	domainName = postfix(longName, '.');

	if (domainName != NULL)
	{
		[host setValue:domainName forKey:"_lookup_DNS_domain"];
		if (strcmp(domainName, dns->domain) == 0)
			[host mergeValue:shortName forKey:"name"];
	}
	
	freeString(shortName);
	freeString(domainName);

	if (alias > 0)
	{
		[host mergeValues:[host valuesForKey:"alias"] forKey:"name"];
		[host removeKey:"alias"];
	}

	return host;
}

- (LUDictionary *)hostWithName:(char *)name
{
	LUDictionary *host;
	dns_reply_t *r;
	dns_question_t q;

	q.class = DNS_CLASS_IN;	
	q.type = DNS_TYPE_A;
	q.name = name;

	r = dns_query(dns, &q);
	host = [self dictForDNSReply:r];
	dns_free_reply(r);

	return [self stamp:host];
}

- (LUDictionary *)hostWithInternetAddress:(struct in_addr *)addr
{
	LUDictionary *host;
	dns_reply_t *r;
	dns_question_t q;
	char name[32];
	union
	{
		unsigned long a;
		unsigned char b[4];
	} ab;

	ab.a = addr->s_addr;

	sprintf(name, "%u.%u.%u.%u.in-addr.arpa",
		ab.b[3], ab.b[2], ab.b[1], ab.b[0]);

	q.class = DNS_CLASS_IN;	
	q.type = DNS_TYPE_PTR;
	q.name = name;

	r = dns_query(dns, &q);
	host = [self dictForDNSReply:r];
	dns_free_reply(r);

	if (host != nil) [host mergeValue:inet_ntoa(*addr) forKey:"ip_address"];

	return [self stamp:host];
}

/* All hosts in my domain */
- (LUArray *)allHosts
{
	LUArray *all;
	LUDictionary *host;
	dns_reply_list_t *l;
	int i;
	char scratch[1024];

	if (!allHostsEnabled) return nil;

	l = dns_zone_transfer(dns, DNS_CLASS_IN);
	if (l == NULL) return nil;

	all = [[LUArray alloc] init];
	sprintf(scratch, "DNSAgent: all %s", [self categoryName:LUCategoryHost]);
	[all setBanner:scratch];

	for (i = 0; i < l->count; i++)
	{
		host = [self dictForDNSReply:l->reply[i]];
		if (host != nil) [all addObject:host];
		[host release];
	}

	dns_free_reply_list(l);

	return all;
}

- (LUDictionary *)networkWithName:(char *)name
{
	LUDictionary *net;
	dns_reply_t *r;
	dns_question_t q;
	char **l;
	char *longName, *shortName, *domainName;
	char str[32];

	q.class = DNS_CLASS_IN;	
	q.type = DNS_TYPE_PTR;
	q.name = name;

	r = dns_query(dns, &q);
	net = [self dictForDNSReply:r];
	dns_free_reply(r);

	if (net == nil) return nil;

	l = explode([net valueForKey:"name"], '.');
	if (l == NULL)
	{
		[net release];
		return nil;
	}

	if (!((listLength(l) == 6) && streq(l[4], "in-addr") && streq(l[5], "arpa")))
	{
		freeList(l);
		[net release];
		return nil;
	}

	sprintf(str, "%s.%s.%s.%s", l[3], l[2], l[1], l[0]);
	freeList(l);
	
	longName = [net valueForKey:"ptr_name"];
	shortName = prefix(longName, '.');
	domainName = postfix(longName, '.');

	if (domainName != NULL)
	{
		[net setValue:domainName forKey:"_lookup_DNS_domain"];
		freeString(domainName);
	}

	[net setValue:longName forKey:"name"];
	if (shortName != NULL) [net addValue:shortName forKey:"name"];
	[net setValue:str forKey:"ip_address"];
	[net removeKey:"ptr_name"];

	return [self stamp:net];
}

- (LUDictionary *)networkWithInternetAddress:(struct in_addr *)addr
{
	LUDictionary *net;
	dns_reply_t *r;
	dns_question_t q;
	char name[32];
	union
	{
		unsigned long a;
		unsigned char b[4];
	} ab;

	ab.a = addr->s_addr;

	sprintf(name, "%u.%u.%u.%u.in-addr.arpa",
		ab.b[3], ab.b[2], ab.b[1], ab.b[0]);

	q.class = DNS_CLASS_IN;	
	q.type = DNS_TYPE_PTR;
	q.name = name;

	r = dns_query(dns, &q);
	net = [self dictForDNSReply:r];
	dns_free_reply(r);

	if (net == nil) return nil;

	[net mergeValue:inet_ntoa(*addr) forKey:"ip_address"];
	[net removeKey:"ptr_name"];

	return [self stamp:net];
}

- (char **)searchList
{
	char **l, *dn, *s, *p;
	int i;

	if (dns == NULL) return NULL;

	l = NULL;

	if (dns->search_count > 0)
	{
		for (i = 0; i < dns->search_count; i++)
			l = appendString(dns->search[i], l);
		return l;
	}

	dn = copyString(dns->domain);
	l = appendString(dns->domain, l);

	s = strchr(dn, '.');
	if (s != NULL)
	{
		s++;
		p = strchr(s, '.');
		while (p != NULL)
		{
			l = appendString(s, l);
			s = p + 1;
			p = strchr(s, '.');
		}
	}

	return l;
}

@end
