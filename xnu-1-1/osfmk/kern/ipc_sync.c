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
 * @OSF_COPYRIGHT@
 * 
 */

#include <kern/ipc_kobject.h>
#include <kern/ipc_sync.h>
#include <ipc/port.h>
#include <ipc/ipc_space.h>
#include <ipc/ipc_port.h>
#include <mach/semaphore_server.h>
#include <mach/lock_set_server.h>
#include <mach/mach_port_server.h>
#include <mach/port.h>


semaphore_t
convert_port_to_semaphore (ipc_port_t port)
{
	semaphore_t semaphore = SEMAPHORE_NULL;

	if (IP_VALID (port)) {
		ip_lock(port);
		if (ip_active(port) && (ip_kotype(port) == IKOT_SEMAPHORE)) {
			semaphore = (semaphore_t) port->ip_kobject;
			semaphore_reference(semaphore);
		}
		ip_unlock(port);
	}

	return (semaphore);
}

ipc_port_t
convert_semaphore_to_port (semaphore_t semaphore)
{
	ipc_port_t port;

	if (semaphore != SEMAPHORE_NULL)
		port = ipc_port_make_send(semaphore->port);
	else
		port = IP_NULL;

	return (port);
}

lock_set_t
convert_port_to_lock_set (ipc_port_t port)
{
	lock_set_t lock_set = LOCK_SET_NULL;

	if (IP_VALID (port)) {
		ip_lock(port);
		if (ip_active(port) && (ip_kotype(port) == IKOT_LOCK_SET)) {
			lock_set = (lock_set_t) port->ip_kobject;
			lock_set_reference(lock_set);
		}
		ip_unlock(port);
	}

	return (lock_set);
}

ipc_port_t
convert_lock_set_to_port (lock_set_t lock_set)
{
	ipc_port_t port;

	if (lock_set != LOCK_SET_NULL)
		port = ipc_port_make_send(lock_set->port);
	else
		port = IP_NULL;

	return (port);
}

