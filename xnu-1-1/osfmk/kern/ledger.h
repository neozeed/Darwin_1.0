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
 */
#ifndef _KERN_LEDGER_H_
#define _KERN_LEDGER_H_

#include <kern/lock.h>
#include <ipc/ipc_port.h>
#include <mach/mach_types.h>

#define LEDGER_ITEM_INFINITY	(~0)

struct ledger {
        ipc_port_t	ledger_self;
        ipc_port_t	ledger_service_port;
        ledger_item_t	ledger_balance;
        ledger_item_t	ledger_limit;
        struct ledger	*ledger_ledger;
        struct ledger	*ledger_parent;
	decl_simple_lock_data(,lock)
};

typedef struct ledger ledger_data_t;

#define LEDGER_NULL ((ledger_t)0)

#define ledger_lock(ledger)	simple_lock(&(ledger)->lock)
#define ledger_unlock(ledger)	simple_unlock(&(ledger)->lock)
#define	ledger_lock_init(ledger) \
	simple_lock_init(&(ledger)->lock, ETAP_MISC_LEDGER)

extern ledger_t	root_wired_ledger;
extern ledger_t	root_paged_ledger;

#define root_wired_ledger_port root_wired_ledger->ledger_self
#define root_paged_ledger_port root_paged_ledger->ledger_self

extern ipc_port_t convert_ledger_to_port(ledger_t);
extern ipc_port_t ledger_copy(ledger_t);

extern kern_return_t ledger_enter(ledger_t, ledger_item_t);
extern void ledger_init(void);

#endif	/* _KERN_LEDGER_H_ */
