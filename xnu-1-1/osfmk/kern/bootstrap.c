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
 * @OSF_FREE_COPYRIGHT@
 */
/* 
 * Mach Operating System
 * Copyright (c) 1991,1990,1989 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */
/*
 */
/*
 * Bootstrap the various built-in servers.
 */

#include <mach_kdb.h>
#include <bootstrap_symbols.h>
#include <dipc.h>

#include <cputypes.h>
#include <mach/port.h>
#include <mach/message.h>
#include <mach/machine/vm_types.h>
#include <mach/vm_prot.h>
#include <mach/boolean.h>
#include <mach/boot_info.h>
#include <mach/mach_port_server.h>
#include <mach/bootstrap_server.h>

#include <device/device_port.h>
#include <ipc/ipc_space.h>

#include <kern/thread.h>
#include <ipc/ipc_port.h>
#include <ipc/ipc_entry.h>
#include <kern/host.h>
#include <kern/ledger.h>
#include <kern/processor.h>
#include <kern/zalloc.h>
#include <kern/misc_protos.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <vm/vm_fault.h>

#if	MACH_KDB
#include <ddb/db_sym.h>

#if defined(__alpha)
extern boolean_t	use_kdb;
extern int		stpages;
#endif /* defined(__alpha) */

#endif	/* MACH_KDB */

vm_offset_t	kern_sym_start = 0;	/* pointer to kernel symbols */
vm_size_t	kern_sym_size = 0;	/* size of kernel symbols */
vm_offset_t	kern_args_start = 0;	/* kernel arguments */
vm_size_t	kern_args_size = 0;	/* size of kernel arguments */
vm_offset_t	boot_sym_start = 0;	/* pointer to bootstrap symbols */
vm_size_t	boot_sym_size = 0;	/* size of bootstrap symbols */
vm_offset_t	boot_args_start = 0;	/* bootstrap arguments */
vm_size_t	boot_args_size = 0;	/* size of bootstrap arguments */
vm_offset_t	boot_start = 0;		/* pointer to bootstrap image */
vm_size_t	boot_size = 0;		/* size of bootstrap image */
vm_offset_t	boot_region_desc = 0;	/* bootstrap region descriptions */
vm_size_t	boot_region_count = 0;	/* number of regions */
int		boot_thread_state_flavor = 0;
thread_state_t	boot_thread_state = 0;
unsigned int	boot_thread_state_count = 0;
vm_offset_t	env_start = 0;		/* environment */
vm_size_t	env_size = 0;		/* size of environment */

vm_offset_t	load_info_start = 0;	/* pointer to bootstrap load info */
vm_size_t	load_info_size = 0;	/* size of bootstrap load info */

kern_return_t
do_bootstrap_ports(
        ipc_port_t bootstrap,
        ipc_port_t *priv_hostp,
        ipc_port_t *priv_devicep,
        ipc_port_t *wired_ledgerp,
        ipc_port_t *paged_ledgerp,
        ipc_port_t *host_securityp)
{
#ifdef	lint
    bootstrap = ipc_port_make_send(realhost.host_priv_self);
#endif	/* lint */

    *priv_hostp = ipc_port_make_send(realhost.host_priv_self);
    *priv_devicep = ipc_port_make_send(master_device_port);
    *wired_ledgerp = ipc_port_make_send(root_wired_ledger_port);
    *paged_ledgerp = ipc_port_make_send(root_paged_ledger_port);
    *host_securityp = ipc_port_make_send(realhost.host_security_self);
    return KERN_SUCCESS;
}

kern_return_t
do_bootstrap_arguments(
	ipc_port_t		bootstrap_port,
	task_t			task,
	vm_offset_t		*arguments_ptr,
	mach_msg_type_number_t	*arguments_count)
{
	vm_offset_t args_addr;
	unsigned int args_size;
	kern_return_t kr;
	vm_map_copy_t copy;

#ifdef	lint
	bootstrap_port = (ipc_port_t) 0;
	task->map = VM_MAP_NULL;
#endif	/* lint */

	if (boot_args_size == 0)
		args_size = PAGE_SIZE;
	else
		args_size = round_page(boot_args_size);
	kr = kmem_alloc_pageable(ipc_kernel_map, &args_addr, args_size);
	if (kr != KERN_SUCCESS)
		return kr;
	if (boot_args_size)
		bcopy((char *)boot_args_start, (char *)args_addr,
		      boot_args_size);
	if (args_size != boot_args_size)
		bzero((char *) args_addr + boot_args_size,
		      args_size - boot_args_size);
	kr = vm_map_copyin(ipc_kernel_map, args_addr, args_size, TRUE, &copy);
	assert(kr == KERN_SUCCESS);
	*arguments_ptr = (vm_offset_t) copy;
	*arguments_count = boot_args_size;
	return KERN_SUCCESS;
}


kern_return_t
do_bootstrap_environment(
	ipc_port_t		bootstrap_port,
	task_t			task,
	vm_offset_t		*environment_ptr,
	mach_msg_type_number_t	*environment_count)
{
	vm_offset_t env_addr;
	vm_size_t alloc_size;
	kern_return_t kr;
	vm_map_copy_t copy;

#ifdef	lint
	task->map = VM_MAP_NULL;
	bootstrap_port = (ipc_port_t) 0;
#endif	/* lint */

	if (env_size == 0)
		alloc_size = PAGE_SIZE;
	else
		alloc_size = round_page(env_size);
	kr = kmem_alloc_pageable(ipc_kernel_map, &env_addr, alloc_size);
	if (kr != KERN_SUCCESS)
		return kr;
	if (env_size)
		bcopy((char *)env_start, (char *)env_addr, env_size);
	if (alloc_size != env_size)
		bzero((char *) env_addr + env_size,
		      alloc_size - env_size);
	kr = vm_map_copyin(ipc_kernel_map, env_addr, alloc_size, TRUE, &copy);
	assert(kr == KERN_SUCCESS);
	*environment_ptr = (vm_offset_t) copy;
	*environment_count = env_size;
	return KERN_SUCCESS;
}

kern_return_t 
do_bootstrap_completed(
	ipc_port_t bootstrap_port,
	task_t task)
{
    /* Need do nothing; only the bootstrap task cares when a
       server signals bootstrap_completed.  */
    return KERN_SUCCESS;
}

void	load_info_print(void);

#if	DEBUG
void
load_info_print(void)
{
	struct loader_info *lp = (struct loader_info *)load_info_start;

	printf("Load info: text (%#x, %#x, %#x)\n",
		lp->text_start, lp->text_size, lp->text_offset);
	printf("           data (%#x, %#x, %#x)\n",
		lp->data_start, lp->data_size, lp->data_offset);
	printf("           bss  (%#x)\n", lp->bss_size);
	printf("           syms (%#x, %#x)\n",
		lp->sym_offset, lp->sym_size);
	printf("	   entry(%#x, %#x)\n",
		lp->entry_1, lp->entry_2);
}
#endif
