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
#import <stdio.h>
#import <unistd.h>
#import <stdlib.h>
#import <mach/mach.h>
#import <mach/mach_error.h>
#import <servers/bootstrap.h>
#import <ctype.h>
#import <string.h>

#import "machcompat.h"
#import "ipconfig_ext.h"
#import "../bootplib/ipconfig.h"
#import "dhcpOptionsPrivate.h"
#import <netinet/in.h>
#import <netinet/udp.h>
#import <netinet/in_systm.h>
#import <netinet/ip.h>
#import <netinet/bootp.h>
#import <arpa/inet.h>

typedef int func_t(port_t server, int argc, char * argv[]);
typedef func_t * funcptr_t;
char * progname = NULL;

#import "rfc_options.h"
static unsigned char	rfc_magic[4] = RFC_OPTIONS_MAGIC;

static id
S_packet_options(struct dhcp * dhcp, int pkt_len)
{
    id options;

    if (bcmp(dhcp->dp_options, rfc_magic, sizeof(rfc_magic)))
	return (nil);
    
    options = [[dhcpOptions alloc] 
	       initWithBuffer:dhcp->dp_options + RFC_MAGIC_SIZE
	       Size:pkt_len - sizeof(struct dhcp) - RFC_MAGIC_SIZE];
    if (options == nil) {
	fprintf(stderr, "S_packet_options: options nil");
	return nil;
    }
    if ([options parse] == FALSE) {
	fprintf(stderr, "S_packet_options: options parse failed");
	[options free];
	return nil;
    }
    return (options);
}

static void
print_packet(struct dhcp *dp, int bp_len)
{
	int i, j, len;

	if (bp_len == 0)
	    return;
	printf("bp_op = ");
	if (dp->dp_op == BOOTREQUEST) printf("BOOTREQUEST\n");
	else if (dp->dp_op == BOOTREPLY) printf("BOOTREPLY\n");
	else
	{
		i = dp->dp_op;
		printf("%d\n", i);
	}

	i = dp->dp_htype;
	printf("bp_htype = %d\n", i);

	printf("dp_flags = %x\n", dp->dp_flags);
	len = dp->dp_hlen;
	printf("bp_hlen = %d\n", len);

	i = dp->dp_hops;
	printf("bp_hops = %d\n", i);

	printf("bp_xid = %lu\n", (u_long)ntohl(dp->dp_xid));

	printf("bp_secs = %hu\n", dp->dp_secs);

	printf("bp_ciaddr = %s\n", inet_ntoa(dp->dp_ciaddr));
	printf("bp_yiaddr = %s\n", inet_ntoa(dp->dp_yiaddr));
	printf("bp_siaddr = %s\n", inet_ntoa(dp->dp_siaddr));
	printf("bp_giaddr = %s\n", inet_ntoa(dp->dp_giaddr));

	printf("bp_chaddr = ");
	for (j = 0; j < len; j++)
	{
		i = dp->dp_chaddr[j];
		printf("%0x", i);
		if (j < (len - 1)) printf(":");
	}
	printf("\n");

	printf("bp_sname = %s\n", dp->dp_sname);
	printf("bp_file = %s\n", dp->dp_file);

	{
	    id			options;

	    options = S_packet_options(dp, bp_len);
	    if (options != nil) {
		printf("options: packet size %d\n", bp_len);
		[options print];
		[options free];
	    }
	}
}

static int
S_wait_all(port_t server, int argc, char * argv[])
{
    kern_return_t	status;
    
    status = ipconfig_wait_all(server);
    if (status == KERN_SUCCESS)
	return (0);
    fprintf(stderr, "wait all failed, %s\n", mach_error_string(status));
    return (1);
}

static int
S_wait_if(port_t server, int argc, char * argv[])
{
    if_name		name;
    kern_return_t	status;

    strcpy(name, argv[0]);
    status = ipconfig_wait_if(server, name);
    if (status == KERN_SUCCESS) {
	return (0);
    }
    fprintf(stderr, "wait if %s failed, %s\n", name,
	    mach_error_string(status));
    return (1);
}

static int
S_if_addr(port_t server, int argc, char * argv[])
{
    struct in_addr	ip;
    if_name		name;
    kern_return_t	status;

    strcpy(name, argv[0]);
    status = ipconfig_if_addr(server, name, (ip_address_t *)&ip);
    if (status == KERN_SUCCESS) {
	printf("%s\n", inet_ntoa(ip));
	return (0);
    }
    fprintf(stderr, "get if addr %s failed, %s\n", name,
	    mach_error_string(status));
    return (1);
}

static int
S_if_count(port_t server, int argc, char * argv[])
{
    int 		count = 0;
    kern_return_t	status;

    status = ipconfig_if_count(server, &count);
    if (status == KERN_SUCCESS) {
	printf("%d\n", count);
	return (0);
    }
    fprintf(stderr, "get if count failed, %s\n", mach_error_string(status));
    return (1);
}

static int
S_get_option(port_t server, int argc, char * argv[])
{
    char		buf[1024];
    inline_data 	data;
    unsigned int 	data_len = sizeof(data);
    if_name		name;
    char		err[1024];
    int			tag;
    kern_return_t	status;

    strcpy(name, argv[0]);

    tag = [dhcpOptions tagWithName:argv[1]];
    if (tag == -1)
	tag = atoi(argv[1]);
    status = ipconfig_get_option(server, name, tag, data, &data_len);
    if (status == KERN_SUCCESS) {
	if ([dhcpOptions str:buf FromTag:tag Option:data Length:data_len
			 ErrorString:err]) {
	    printf("%s\n", buf);
	    return (0);
	}
	fprintf(stderr, "couldn't convert the option, %s\n",
		err);
    }
    else {
	fprintf(stderr, "ipconfig_get_option failed, %s\n", 
		mach_error_string(status));
    }
    return (1);
}

static int
S_get_packet(port_t server, int argc, char * argv[])
{
    inline_data 	data;
    unsigned int 	data_len = sizeof(data);
    if_name		name;
    kern_return_t	status;

    strcpy(name, argv[0]);
    status = ipconfig_get_packet(server, name, data, &data_len);
    if (status == KERN_SUCCESS) {
	print_packet((struct dhcp *)data, data_len);
    }
    return (1);
}

static struct {
    char *	command;
    funcptr_t	func;
    int		argc;
    char *	usage;
} commands[] = {
    { "waitall", S_wait_all, 0, "" },
    { "getifaddr", S_if_addr, 1, "<interface name>" },
    { "waitif", S_wait_if, 1, " <interface name>" },
    { "ifcount", S_if_count, 0, "" },
    { "getoption", S_get_option, 2, 
      " <interface name | \"\" (=any)> <option name> | <option code>" },
    { "getpacket", S_get_packet, 1, " <interface name>" },
    { NULL, NULL, NULL },
};

void
usage()
{
    int i;
    fprintf(stderr, "usage: %s <command> <args>\n", progname);
    fprintf(stderr, "where <command> is one of ");
    for (i = 0; commands[i].command; i++) {
	fprintf(stderr, "%s%s",  i == 0 ? "" : ", ",
		commands[i].command);
    }
    fprintf(stderr, "\n");
    exit(1);
}

static funcptr_t
S_lookup_func(char * cmd, int argc)
{
    int i;

    for (i = 0; commands[i].command; i++) {
	if (strcmp(cmd, commands[i].command) == 0) {
	    if (argc != commands[i].argc) {
		fprintf(stderr, "usage: %s %s\n", commands[i].command,
			commands[i].usage ? commands[i].usage : "");
		exit(1);
	    }
	    return commands[i].func;
	}
    }
    return (NULL);
}

int
main(int argc, char * argv[])
{
    boolean_t		active;
    funcptr_t		func;
    port_t		server;
    kern_return_t	status;

    progname = argv[0];
    if (argc < 2)
	usage();

    status = ipconfig_server_port(&server, &active);
    if (status != KERN_SUCCESS) {
	mach_error("ipconfig_server_port failed", status);
	exit(1);
    }

    if (active == FALSE) {
	fprintf(stderr, "ipconfig server not active\n");
	/* start it maybe??? */
	exit(1);
    }

    argv++; argc--;

#if 0
    {
	struct in_addr	dhcp_ni_addr;
	char		dhcp_ni_tag[128];
	int		len;
	kern_return_t	status;

	len = sizeof(dhcp_ni_addr);
	status = ipconfig_get_option(server, IPCONFIG_IF_ANY,
				     112, (void *)&dhcp_ni_addr, &len);
	if (status != KERN_SUCCESS) {
	    fprintf(stderr, "get 112 failed, %s\n", mach_error_string(status));
	}
	else {
	    fprintf(stderr, "%d bytes:%s\n", len, inet_ntoa(dhcp_ni_addr));
	}
	len = sizeof(dhcp_ni_tag) - 1;
	status = ipconfig_get_option(server, IPCONFIG_IF_ANY,
				     113, (void *)&dhcp_ni_tag, &len);
	if (status != KERN_SUCCESS) {
	    fprintf(stderr, "get 113 failed, %s\n", mach_error_string(status));
	}
	else {
	    dhcp_ni_tag[len] = '\0';
	    fprintf(stderr, "%d bytes:%s\n", len, dhcp_ni_tag);
	}
	
    }
#endif 0
    func = S_lookup_func(argv[0], argc - 1);
    if (func == NULL)
	usage();
    argv++; argc--;
    exit ((*func)(server, argc, argv));
}
