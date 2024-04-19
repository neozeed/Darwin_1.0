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
#import <syslog.h>

#import "dprintf.h"
#import "threadcompat.h"
#import "machcompat.h"
#import "ipconfigd.h"
#import "ipconfig_ext.h"
#import "../bootplib/ipconfig.h"

static char request_buf[1024];
static char reply_buf[1024];

kern_return_t
_ipconfig_config_if(port_t p, if_name name)
{
    dprintf(("config called with %s\n", name));
    return (KERN_SUCCESS);
}

kern_return_t
_ipconfig_config_all(port_t p)
{
    dprintf(("config all called\n"));
    return (KERN_SUCCESS);
}

kern_return_t
_ipconfig_wait_if(port_t p, if_name name)
{
    dprintf(("Waiting for %s to complete\n", name));
    if (wait_if(name) == TRUE)
	return (KERN_SUCCESS);
    return (KERN_FAILURE);
}

kern_return_t
_ipconfig_wait_all(port_t p)
{

    dprintf(("Waiting for all interfaces to complete\n"));
    wait_all();
    return (KERN_SUCCESS);
}

kern_return_t
_ipconfig_if_name(port_t p, int intface, if_name name)
{

    dprintf(("Getting interface name\n"));
    if (get_if_name(intface, name) == TRUE)
	return (KERN_SUCCESS);
    return (KERN_FAILURE);
}

kern_return_t
_ipconfig_if_addr(port_t p, if_name name, u_int32_t * addr)
{
    dprintf(("Getting interface address\n"));
    if (get_if_addr(name, addr) == TRUE)
	return (KERN_SUCCESS);
    return (KERN_FAILURE);
}

kern_return_t
_ipconfig_if_count(port_t p, int * count)
{
    dprintf(("Getting interface count\n"));
    *count = get_if_count();
    return (KERN_SUCCESS);
}

kern_return_t
_ipconfig_get_option(port_t p, if_name name, int option_code,
		     inline_data option_data,
		     unsigned int * option_dataCnt)
{
    if (get_if_option(name, option_code, option_data, option_dataCnt) 
	== TRUE) {
	return (KERN_SUCCESS);
    }
    return (KERN_FAILURE);
}

kern_return_t
_ipconfig_get_packet(port_t p, if_name name,
		     inline_data packet_data,
		     unsigned int * packet_dataCnt)
{
    if (get_if_packet(name, packet_data, packet_dataCnt) == TRUE) {
	return (KERN_SUCCESS);
    }
    return (KERN_FAILURE);
}

static port_t		service_port;

void server_loop()
{
    msg_header_t *	request = (msg_header_t *)request_buf;
    msg_header_t *	reply = (msg_header_t *)reply_buf;

    while (1) {
	msg_return_t r;
#ifdef MOSX
	request->msgh_size = sizeof(request_buf);
	request->msgh_local_port = service_port;
	r = msg_receive(request, MSG_OPTION_NONE, 0);
#else MOSX
	request->msg_size = sizeof(request_buf);
	request->msg_local_port = service_port;
	r = msg_receive(request, MSG_OPTION_NONE, 0);
#endif
	if (r == RCV_SUCCESS) {
	    extern boolean_t ipconfig_server(msg_header_t *, msg_header_t *);

	    if (ipconfig_server(request, reply)) {
		r = msg_send(reply, MSG_OPTION_NONE, 0);
		if (r != SEND_SUCCESS)
		    syslog(LOG_ERR, "msg_send: %s", mach_error_string(r));
	    }
	}
	else
	    syslog(LOG_ERR, "msg_receive: %s", mach_error_string(r));
    }
}

void
server_init()
{
#ifndef MOSX
    port_t		bootstrap_port;
#endif MOSX
    port_all_t		my_port;
    kern_return_t 	status;
    
    status = port_allocate(task_self(), &service_port);
    if (status != KERN_SUCCESS) {
	mach_error("Couldn't allocate a service port", status);
	exit(1);
    }

#ifndef MOSX 
    status = task_get_bootstrap_port(task_self(), &bootstrap_port);
    if (status != KERN_SUCCESS) {
	mach_error("task_get_bootstrap_port", status);
	exit(1);
    }
    status = bootstrap_create_service(bootstrap_port, IPCONFIG_SERVER,
				      &my_port);
    if (status == BOOTSTRAP_NAME_IN_USE)
	;
    else if (status != BOOTSTRAP_SUCCESS) {
	mach_error("bootstrap_create_service", status);
	exit(1);
    }
#endif MOSX
    status = bootstrap_register(bootstrap_port, IPCONFIG_SERVER, service_port);
    if (status != BOOTSTRAP_SUCCESS) {
	mach_error("bootstrap_register", status);
	exit(1);
    }
}

