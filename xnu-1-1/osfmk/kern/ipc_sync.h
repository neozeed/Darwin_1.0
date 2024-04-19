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
/*
 * HISTORY
 * 
 * Revision 1.1.1.1  1998/09/22 21:05:34  wsanchez
 * Import of Mac OS X kernel (~semeria)
 *
 * Revision 1.1.1.1  1998/03/07 02:25:55  wsanchez
 * Import of OSF Mach kernel (~mburg)
 *
 * Revision 1.1.4.1  1994/08/16  16:10:28  joe
 * 	Original rt2_shared version
 * 	[1994/08/15  18:33:34  joe]
 *
 * Revision 1.1.2.1  1994/07/21  19:33:01  joe
 * 	Initial check-in: synchronizer package.
 * 	[1994/07/21  19:32:36  joe]
 * 
 * $EndLog$
 */

#ifndef _KERN_IPC_SYNC_H_
#define _KERN_IPC_SYNC_H_

#include <ipc/ipc_types.h>
#include <kern/sync_sema.h>
#include <kern/sync_lock.h>

semaphore_t convert_port_to_semaphore (ipc_port_t port);
ipc_port_t  convert_semaphore_to_port (semaphore_t semaphore);

lock_set_t  convert_port_to_lock_set  (ipc_port_t port);
ipc_port_t  convert_lock_set_to_port  (lock_set_t lock_set);

#endif /* _KERN_IPC_SYNC_H_ */
