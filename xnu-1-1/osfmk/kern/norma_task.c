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
/*
 * HISTORY
 * 
 * Revision 1.1.1.1  1998/09/22 21:05:34  wsanchez
 * Import of Mac OS X kernel (~semeria)
 *
 * Revision 1.2  1998/04/29 17:36:06  mburg
 * MK7.3 merger
 *
 * Revision 1.1.24.1  1998/02/03  09:28:42  gdt
 * 	Merge up to MK7.3
 * 	[1998/02/03  09:13:51  gdt]
 *
 * Revision 1.1.22.1  1997/06/17  02:57:54  devrcs
 * 	Add node incarnation number to uid definition.
 * 	[1996/09/03  19:31:17  watkins]
 * 
 * Revision 1.1.7.3  1995/10/09  17:15:22  devrcs
 * 	Added ETAP argument to `mutex_init()' call.
 * 	[1995/09/13  16:04:48  joe]
 * 
 * Revision 1.1.7.2  1995/09/18  19:15:00  devrcs
 * 	Added ETAP argument to `mutex_init()' call.
 * 	[1995/09/13  16:04:48  joe]
 * 
 * Revision 1.1.7.1  1995/02/23  17:31:49  alanl
 * 	DIPC:  Merge from nmk17b2 to nmk18b8.
 * 	[95/01/05            alanl]
 * 
 * Revision 1.1.4.4  1994/12/06  20:11:26  alanl
 * 	Include norma_protos.h.
 * 	[94/12/05            mmp]
 * 
 * Revision 1.1.4.3  1994/09/09  23:12:57  alanl
 * 	Merge up to NMK17.3.  Note:  chose to preserve most of
 * 	the mmp	task_set_inherited_ports solution, with small
 * 	modifications from the paire solution.
 * 
 * 	Revision 1.4.15.3  1994/08/31  14:13:44  paire
 * 		In task_set_inherited_ports routine, the ipc_port_release_send
 * 		routine must not be called while holding the task simple lock.
 * 		[94/08/26            paire]
 * 	[94/09/07            alanl]
 * 
 * Revision 1.1.4.2  1994/09/01  12:52:22  alanl
 * 	Fix task_set_inherited_ports to drop ports after releasing the
 * 	task lock.  (With mmp.)
 * 	[94/08/29            alanl]
 * 
 * Revision 1.1.4.1  1994/08/04  02:25:24  mmp
 * 	DIPC:  Merge up from NMK16.
 * 	DIPC:  replace use of norma_get_special_port with
 * 	dipc_host_priv_port.  Eliminate norma_ipc.h.
 * 	[1994/04/27  23:52:52  alanl]
 * 
 * 	DIPC:  separated out from NORMA_IPC.  Include
 * 	<dipc/dipc_funcs.h> rather than <norma/ipc_node.h>.
 * 	NORMA_TASK must presuppose DIPC.
 * 	[1994/04/20  18:31:04  alanl]
 * 
 * Revision 1.4.21.4  1994/09/23  02:49:55  ezf
 * 	change marker to not FREE
 * 	[1994/09/22  21:46:48  ezf]
 * 
 * Revision 1.4.21.3  1994/09/10  21:47:13  bolinger
 * 	Merge up to NMK17.3
 * 	[1994/09/08  19:58:01  bolinger]
 * 
 * Revision 1.4.21.2  1994/06/21  19:43:02  dlb
 * 	Use mutex_init to initialize new task's lock.
 * 	Add additional argument to task_create_local().
 * 	[94/06/16            dlb]
 * 
 * Revision 1.4.21.1  1994/06/14  22:39:26  robert
 * 	merge forward
 * 	[1994/06/14  22:04:58  robert]
 * 
 * Revision 1.4.19.2  1994/06/14  13:29:35  dswartz
 * 	Preemption merge
 * 
 * Revision 1.4.15.3  1994/08/31  14:13:44  paire
 * 	In task_set_inherited_ports routine, the ipc_port_release_send
 * 	routine must not be called while holding the task simple lock.
 * 	[94/08/26            paire]
 * 
 * Revision 1.4.15.2  1994/03/11  15:27:17  bernadat
 * 	Fixed misordered parameters in norma_task_common().
 * 	Fixed test against TASK_PORT_REGISTER_MAX in
 * 		task_set_inherited_ports().
 * 	[94/03/11            bernadat]
 * 
 * Revision 1.4.15.1  1994/03/07  16:52:36  paire
 * 	Implemented new exception ports.
 * 	[94/02/22            paire]
 * 
 * 	Added ANSI prototypes.
 * 	[94/02/15            paire]
 * 
 * 	Added mcmsg.h (machine dependent, needs to be fixed)
 * 	Change from NMK16.1 [93/12/03            bernadat]
 * 
 * 	Added init. of new_task->itk_lock_data in norma_task_common().
 * 	Fixed a typo from last merge.
 * 	In task_[sg]et_inherited_ports(), task_[sg]et_special_port()
 * 	must be called without any lock held.
 * 	Change from NMK16.1 [93/11/30            paire]
 * 
 * 	Added missing semicolon preventing sequent MP build.
 * 	Change from NMK16.1 [93/08/18            bernadat]
 * 
 * 	Fixes for ANSI C
 * 	Change from NMK16.1 [93/07/20            bernadat]
 * 
 * 	[Joseph Barerra:joebar@microsoft.com] Added norma_task_teleport
 * 	call for task migration.
 * 	Change from NORMA_MK14.6 [93/03/08            sjs]
 * 	[94/02/04            paire]
 * 
 * Revision 1.4.2.2  1993/06/09  02:46:16  gm
 * 	Added to OSF/1 R1.3 from NMK15.0.
 * 	[1993/06/02  21:21:05  jeffc]
 * 
 * Revision 1.4  1993/04/19  16:57:22  devrcs
 * 	Use task_{set,get}_special_port for norma_task_create
 * 	instead of referencing itk_exception which does not exist
 * 	anymore. For the final solution (transfering all exception
 * 	ports) we need to change the internal MIG stub and probably
 * 	and need also to split the exception_action structure to pass
 * 	it through mig.
 * 	[93/03/02            bernadat]
 * 
 * Revision 1.3  1992/12/07  21:30:14  robert
 * 	integrate any changes below for 14.0 (branch from 13.16 base)
 * 
 * 	Dave Mitchell (dwm) at Open Software Foundation 23-Oct-92
 * 	Rearrange task_create() et al. to prevent user code from
 * 	being able to pass in a null parent.	(#478)
 * 
 * 	Joseph Barrera (jsb) at Carnegie-Mellon University 11-Sep-92
 * 	This version now supports VM_INHERIT_SHARE.
 * 	Added norma_task_clone to support task migration.
 * 	Use norma_get_special_port() instead of remote_host().
 * 	Moved task_copy_vm to norma/vm_copy.c. Removed babble routine.
 * 	Placed task_create_remote_node feature under TASK_CREATE_USE_REMOTE
 * 	conditional. Folded task initialization calls into norma_task_allocate.
 * 	Removed norma_task_server and associated lock. Check for null parent
 * 	task in task_create. Terminate task upon address space copy failure.
 * 	[1992/12/06  20:51:48  robert]
 * 
 * Revision 1.2  1992/11/25  01:15:11  robert
 * 	fix history
 * 	[1992/11/09  21:58:59  robert]
 * 
 * 	integrate changes below for norma_14
 * 	[1992/11/09  16:47:47  robert]
 * 
 * Revision 1.1  1992/11/05  21:00:06  robert
 * 	Initial revision
 * 	[92/10/23            dwm]
 * 
 * Revision 0.0  92/10/23            dwm
 * 	Terminate remote_task in norma_task_create() error path,
 * 	to avoid leaving remote orphans hanging around, and avoid
 * 	returning random MiG errors.  (#471, 475).
 * 	Rearrange task_create() et al. to prevent user code from
 * 	being able to pass in a null parent.	(#478)
 * 
 * $EndLog$
 */
/* CMU_HIST */
/*
 * Revision 2.7.2.7  92/09/15  17:34:52  jeffreyh
 * 	When inheriting memory in task_copy_vm, set protection in
 * 	locally forked map to max.  This deals with regions that are
 * 		currently unreadable but could become readable in the future.
 * 	Don't set up cross node copy logic for permanently unreadable
 * 	regions.  Don't copy read-only regions.
 * 	[92/08/14            dlb]
 * 
 * 	Add bounds checking on the node to task_set_child_node
 * 	and norma_task_create.
 * 	[92/07/23            jeffreyh]
 * 
 * Revision 2.7.2.6  92/05/28  18:20:32  jeffreyh
 * 	Add new type argument to remote_host call
 * 
 * Revision 2.7.2.5  92/05/27  00:53:43  jeffreyh
 * 	norma_task_create inits mcmsg_task in MCMSG ifdef
 * 	[regnier@ssd.intel.com]
 * 
 * Revision 2.7.2.4  92/03/04  16:07:16  jeffreyh
 * 	Converted to use out-of-line forms of task_{get,set}_emulation_vector.
 * 	[92/03/04  14:55:51  jsb]
 * 
 * Revision 2.7.2.3  92/02/21  11:25:15  jsb
 * 	Release send right to memory_object after mapping it in remote task
 * 	in task_copy_vm.
 * 	[92/02/20  10:29:45  jsb]
 * 
 * 	Set copy flag TRUE in r_vm_map call in task_copy_vm.
 * 	This is now practical due to smarter copy strategy management
 * 	in xmm_svm (compared to when it always used MEMORY_OBJECT_COPY_NONE).
 * 	It also keeps xmm_copy from having to deal with data writes without
 * 	having to use an xmm_shadow layer.
 * 	[92/02/11  11:39:23  jsb]
 * 
 * 	Release map reference in task_copy_vm.
 * 	[92/01/22  10:29:57  jsb]
 * 
 * Revision 2.7.2.2  92/01/09  18:46:08  jsb
 * 	Added logic in task_create to alternate task creation between local
 * 	node and a patchable remote node. For testing purposes only.
 * 	[92/01/08  16:41:27  jsb]
 * 
 * 	Use varargs for debugging printfs.
 * 	[92/01/08  10:22:12  jsb]
 * 
 * 	Use remote_host() instead of norma_get_special_port().
 * 	[92/01/04  18:17:46  jsb]
 * 
 * Revision 2.7.2.1  92/01/03  16:38:29  jsb
 * 	Removed unused routine ipc_task_reinit.
 * 	[91/12/28  17:59:18  jsb]
 * 
 * Revision 2.7  91/12/13  13:53:22  jsb
 * 	Changed name of task_create_remote to norma_task_create.
 * 	Added check for local case in norma_task_create.
 * 
 * Revision 2.6  91/12/10  13:26:19  jsb
 * 	Changed printfs to frets.
 * 	[91/12/10  11:34:35  jsb]
 * 
 * Revision 2.5  91/11/14  16:51:51  rpd
 * 	Use new child_node task field in place of task_server_node to decide
 * 	upon what node to create child task. Also add task_set_child_node().
 * 	[91/09/23  09:22:18  jsb]
 * 
 * Revision 2.4  91/08/28  11:16:18  jsb
 * 	Turned off remaining printf.
 * 	[91/08/26  11:16:11  jsb]
 * 
 * 	Added support for remote task creation with inherited memory.
 * 	Remove task creation is accessible either via task_create_remote,
 * 	or task_create with task_server_node patched to some reasonable value.
 * 	[91/08/15  13:55:54  jsb]
 * 
 * Revision 2.3  91/06/17  15:48:07  jsb
 * 	Moved routines here from kern/ipc_tt.c and kern/task.c.
 * 	[91/06/17  11:01:51  jsb]
 * 
 * Revision 2.2  91/06/06  17:08:15  jsb
 * 	First checkin.
 * 	[91/05/25  11:47:39  jsb]
 * 
 */
/* CMU_ENDHIST */
/* 
 * Mach Operating System
 * Copyright (c) 1991 Carnegie Mellon University
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
 *	File:	norma/kern_task.c
 *	Author:	Joseph S. Barrera III
 *
 *	NORMA task support.
 */

#include <dipc.h>
#include <norma_vm.h>
#include <norma_task.h>

#include <mach/machine/vm_types.h>
#include <mach/vm_param.h>
#include <mach/mach_norma_server.h>
#include <mach/task_server.h>
#include <mach/thread_act_server.h>
#include <mach/vm_task_server.h>
#include <kern/norma_task_server.h>
#include <xmm/xmm_internal.h>
#include <mach/task_info.h>
#include <mach/task_special_ports.h>
#include <mach/norma_special_ports.h>
#include <ipc/ipc_space.h>
#include <kern/mach_param.h>
#include <kern/task.h>
#include <kern/host.h>
#include <kern/thread.h>
#include <kern/zalloc.h>
#include <kern/kalloc.h>
#include <kern/processor.h>
#include <kern/ipc_tt.h>
#include <kern/misc_protos.h>
#include <kern/norma_protos.h>
#include <kern/norma_task.h>		/* mig-generated */
#if	DIPC
#include <dipc/dipc_funcs.h>
#include <dipc/special_ports.h>
#endif


/*
 * XXX This definition should be elsewhere
 */
kern_return_t
norma_node_self(
	host_t host,
	int *node)
{
#if	DIPC
	*node = dipc_node_self();
	return KERN_SUCCESS;
#else
	panic("norma_node_self:  no transport");
	return KERN_INVALID_ARGUMENT;
#endif
}

#if	NORMA_TASK

/*
 * Forward.
 */
kern_return_t	task_get_inherited_ports(
			task_t				task,
			ipc_port_t			*bootstrap,
			norma_registered_port_array_t	registered,
			unsigned			*count,
			exception_mask_array_t		exc_masks,
			unsigned			*exc_count,
			exception_port_array_t		exc_ports,
			exception_behavior_array_t	exc_behaviors,
			exception_flavor_array_t	exc_flavors);

kern_return_t	task_set_inherited_ports(
			task_t				task,
			ipc_port_t			bootstrap,
			norma_registered_port_array_t	registered,
			unsigned			count,
			exception_mask_array_t		exc_masks,
			unsigned			exc_count,
			exception_port_array_t		exc_ports,
			exception_behavior_array_t	exc_behaviors,
			exception_flavor_array_t	exc_flavors);

kern_return_t	norma_task_common(
			task_t		parent_task,
			boolean_t	inherit_memory,
			boolean_t	clone,
			boolean_t	kill_parent,
			int		child_node,
			task_t		*child_task);

extern zone_t task_zone;

/*
 * This is a debugging/testing hack to allow a system with an unmodified
 * server to run tasks automatically alternately on local node and some
 * remote node specified by a patchable variable (task_create_remote_node).
 */
#define	TASK_CREATE_USE_REMOTE	1
#if	TASK_CREATE_USE_REMOTE
int task_create_remote_node = -1;
boolean_t task_create_use_remote = FALSE;
#endif	/* TASK_CREATE_USE_REMOTE */

kern_return_t
task_create(
	task_t		parent_task,
        ledger_port_array_t ledger_ports,
        mach_msg_type_number_t ledger_portsCnt,
	boolean_t	inherit_memory,
	task_t		*child_task)		/* OUT */
{
#if	TASK_CREATE_USE_REMOTE
	if (task_create_remote_node != -1) {
		task_create_use_remote = ! task_create_use_remote;
		if (task_create_use_remote) {
			return norma_task_create(parent_task, inherit_memory,
						 task_create_remote_node,
						 child_task);
		} else {
			return task_create_local(parent_task, inherit_memory,
						 FALSE, child_task);
		}
	}
#endif	/* TASK_CREATE_USE_REMOTE */
	if (! parent_task || parent_task->child_node == -1) {
		return task_create_local(parent_task, inherit_memory,
					 FALSE, child_task);
	} else {
		return norma_task_create(parent_task, inherit_memory,
					 (int) parent_task->child_node,
					 child_task);
	}
}

kern_return_t
task_set_child_node(
	task_t	task,
	int	child_node)
{
#if	DIPC
	if (task == TASK_NULL || !dipc_node_is_valid (child_node)) {
		return KERN_INVALID_ARGUMENT;
	} else {
		task->child_node = child_node;
		return KERN_SUCCESS;
	}
#else	/* DIPC */
	printf("task_set_child_node:  no underlying transport!\n");
	return KERN_INVALID_ARGUMENT;
#endif	/* DIPC */
}

/*
 * This allows us to create a task without providing a parent.
 *
 * XXX
 * Should deallocate resouces upon failure
 */
kern_return_t
norma_task_allocate(
	host_t				host,
	int				vector_start,
	emulation_vector_t		entry_vector,
	unsigned int			entry_vector_count,
	ipc_port_t			bootstrap,
	norma_registered_port_array_t	registered,
	unsigned			count,
	exception_mask_array_t		exc_masks,
	unsigned			exc_count,
	exception_port_array_t		exc_ports,
	exception_behavior_array_t	exc_behaviors,
	exception_flavor_array_t	exc_flavors,
	task_t				*result)	/* OUT */
{
	kern_return_t kr;
	task_t task;

	if (host == HOST_NULL) {
		return KERN_INVALID_ARGUMENT;
	}
	kr = task_create_local(TASK_NULL, FALSE, FALSE, &task);
	if (kr != KERN_SUCCESS) {
		return kr;
	}
	kr = task_set_emulation_vector(task, vector_start, entry_vector,
				       entry_vector_count);
	if (kr != KERN_SUCCESS) {
		return kr;
	}
	kr = task_set_inherited_ports(task, bootstrap, registered, count,
				      exc_masks, exc_count, exc_ports,
				      exc_behaviors, exc_flavors);
	if (kr != KERN_SUCCESS) {
		printf("task_set_inherited_ports failed: kr %d %x\n", kr, kr);
		return kr;
	}
	*result = task;
	return KERN_SUCCESS;
}

kern_return_t
task_get_inherited_ports(
	task_t				task,
	ipc_port_t			*bootstrap,
	norma_registered_port_array_t	registered,
	unsigned			*count,
	exception_mask_array_t		exc_masks,
	unsigned			*exc_count,
	exception_port_array_t		exc_ports,
	exception_behavior_array_t	exc_behaviors,
	exception_flavor_array_t	exc_flavors)
{
	unsigned i;
	unsigned j;
	unsigned n;

	if (task == TASK_NULL) {
		return KERN_INVALID_ARGUMENT;
	}

	itk_lock(task);
	*bootstrap = ipc_port_copy_send(task->itk_bootstrap);

	for (i = 0; i < TASK_PORT_REGISTER_MAX; i++)
		registered[i] = ipc_port_copy_send(task->itk_registered[i]);
	*count = TASK_PORT_REGISTER_MAX;

	n = 0;
	for (i = FIRST_EXCEPTION; i < EXC_TYPES_COUNT; i++) {
		for (j = 0; j < n; j++) {
			/*
			 * Search for an identical entry, if found
			 * set corresponding mask for this exception.
			 */
			if (task->exc_actions[i].port == exc_ports[j] &&
			    task->exc_actions[i].behavior == exc_behaviors[j] &&
			    task->exc_actions[i].flavor == exc_flavors[j]) {
				exc_masks[j] |= (1 << i);
				break;
			}
		}
		if (j == n) {
			exc_masks[j] = (1 << i);
			exc_ports[j] =
				ipc_port_copy_send(task->exc_actions[i].port);
			exc_behaviors[j] = task->exc_actions[i].behavior;
			exc_flavors[j] = task->exc_actions[i].flavor;
			n++;
		}
	}
	*exc_count = n;
	itk_unlock(task);

	return KERN_SUCCESS;
}


/*
 *	It's not a good idea to be releasing send rights to ports
 *	while holding the task lock -- the send rights could get
 *	deallocated or could result in message traffic (at least
 *	in the DIPC case).  So save pointers to all the ports we
 *	would deallocate, and drop them at the end.
 *
 *	Possible ports to deallocate:
 *		itk_bootstrap (per-task)
 *		itk_registered[TASK_PORT_REGISTER_MAX] (per-task)
 *		exc_actions[EXC_TYPES_COUNT] (per-task)
 *		exc_ports[EXC_TYPES_COUNT] (passed in by caller)
 */
#define	TSIP_DROP_MAX	(1 + TASK_PORT_REGISTER_MAX + (EXC_TYPES_COUNT * 2))

kern_return_t
task_set_inherited_ports(
	task_t				task,
	ipc_port_t			bootstrap,
	norma_registered_port_array_t	registered,
	unsigned			count,
	exception_mask_array_t		exc_masks,
	unsigned			exc_count,
	exception_port_array_t		exc_ports,
	exception_behavior_array_t	exc_behaviors,
	exception_flavor_array_t	exc_flavors)
{
	exception_mask_t mask;
	ipc_port_t	drop_ports[TSIP_DROP_MAX];
	unsigned int	drop_port_count = 0;
	unsigned i;
	unsigned j;

	if (task == TASK_NULL)
		return KERN_INVALID_ARGUMENT;

	if (count > TASK_PORT_REGISTER_MAX+1)
		return KERN_INVALID_ARGUMENT;

	mask = 0;
	for (i = 0; i < exc_count; i++) {
		if ((exc_masks[i] & ~EXC_MASK_ALL) || (mask & exc_masks[i]))
			return KERN_INVALID_ARGUMENT;
		mask |= exc_masks[i];
	}

	if (exc_count >= EXC_TYPES_COUNT)
		return KERN_INVALID_ARGUMENT;

	itk_lock(task);
	/*
	 * First, set up bootstrap port.
	 */
	if (IP_VALID(task->itk_bootstrap)) {
		assert(drop_port_count < TSIP_DROP_MAX);
		drop_ports[drop_port_count++] = task->itk_bootstrap;
	}
	task->itk_bootstrap = bootstrap;

	/*
	 * Next, set up registered ports.
	 */
	for (i = 0; i < count; i++) {
		if (IP_VALID(task->itk_registered[i])) {
			assert(drop_port_count < TSIP_DROP_MAX);
			drop_ports[drop_port_count++] = task->itk_registered[i];
		}
		task->itk_registered[i] = registered[i];
	}

	/*
	 * Finally, set up exception ports.
	 */
	for (i = 0; i < exc_count; i++) {
		mask = exc_masks[i] & ~EXC_MASK_ALL;
		j = 0;
		while (mask) {
			if (mask & 1) {
				if (IP_VALID(task->exc_actions[j].port)) {
					assert(drop_port_count < TSIP_DROP_MAX);
					drop_ports[drop_port_count++] =
						task->exc_actions[j].port;
				}
				task->exc_actions[j].port =
					ipc_port_copy_send(exc_ports[i]);
				task->exc_actions[j].behavior =
					exc_behaviors[i];
				task->exc_actions[j].flavor = exc_flavors[i];
			}
			mask >>= 1;
			j++;
		}
		if (IP_VALID(exc_ports[i])) {	/* consume send right */
			assert(drop_port_count < TSIP_DROP_MAX);
			drop_ports[drop_port_count++] = exc_ports[i];
		}
	}
	itk_unlock(task);

	for (i = 0; i < drop_port_count; ++i)
		ipc_port_release_send(drop_ports[i]);

	return KERN_SUCCESS;
}

kern_return_t
norma_task_common(
	task_t		parent_task,
	boolean_t	inherit_memory,
	boolean_t	clone,
	boolean_t	kill_parent,
	int		child_node,
	task_t		*child_task)
{
	ipc_port_t remote_task, remote_host;
	task_t new_task;
	kern_return_t kr;
	int vector_start;
	unsigned int entry_vector_count;
	ipc_port_t bootstrap;
	emulation_vector_t entry_vector;
	ipc_port_t registered[TASK_PORT_REGISTER_MAX];
	exception_mask_t exc_masks[EXC_TYPES_COUNT];
	ipc_port_t exc_ports[EXC_TYPES_COUNT];
	exception_behavior_t exc_behaviors[EXC_TYPES_COUNT];
	thread_state_flavor_t exc_flavors[EXC_TYPES_COUNT];
	unsigned count, exc_count;

#if	DIPC
	if (!dipc_node_is_valid (child_node)) {
		return KERN_INVALID_ARGUMENT;
	}

	if (child_node == dipc_node_self()) {
		if (clone) {
			/*
			 * Cloning: easy if kill_parent;
			 * (currently) impossible otherwise.
			 */
			if (kill_parent) {
				/*
				 * Just return the parent -- nothing says that
				 * the clone has to be a different task.
				 */
				*child_task = parent_task;
				return KERN_SUCCESS;
			} else {
				/*
				 * XXX
				 * There is no local call we can use with the
				 * same memory semantics -- we'd have to
				 * modify task_create_local, and vm_map_fork.
				 * Not hard, just probably unnecessary except
				 * for orthogonality.
				 */
				return KERN_INVALID_ARGUMENT;
			}
		} else {
			/*
			 * Not cloning: just use task_create_local,
			 * and task_terminate if appropriate.
			 */
			kr = task_create_local(parent_task, inherit_memory,
					       FALSE, child_task);
			if (kr != KERN_SUCCESS) {
				return kr;
			}
			if (kill_parent) {
				kr = task_terminate(parent_task);
				if (kr != KERN_SUCCESS) {
					/* cleanup */
					(void) task_terminate(*child_task);
					return kr;
				}
			}
			return KERN_SUCCESS;
		}
	}

	kr = task_get_emulation_vector(parent_task, &vector_start,
				       &entry_vector, &entry_vector_count);
	if (kr != KERN_SUCCESS) {
		printf("task_get_emulation_vector failed: kr %d %x\n", kr, kr);
		return kr;
	}

	kr = task_get_inherited_ports(parent_task, &bootstrap, registered,
				      &count, exc_masks, &exc_count,
				      exc_ports, exc_behaviors, exc_flavors);
	if (kr != KERN_SUCCESS) {
		printf("task_get_inherited_ports failed: kr %d %x\n", kr, kr);
		return kr;
	}

	remote_host = dipc_host_priv_port(child_node);
	if (remote_host == IP_NULL) {
		panic("norma_task_create:  no priv port for node %d\n",
		      child_node);
		return KERN_INVALID_ARGUMENT;
	}

	kr = r_norma_task_allocate(remote_host, vector_start, entry_vector,
				   entry_vector_count, bootstrap,
				   registered, count, exc_masks, exc_count,
				   exc_ports, exc_behaviors, exc_flavors,
				   &remote_task);
	if (kr != KERN_SUCCESS) {
		return kr;
	}

	if (inherit_memory) {
		task_copy_vm(remote_host, parent_task->map, clone,
			     kill_parent, remote_task);
	}

	if (kill_parent) {
		(void) task_terminate(parent_task);
	}

	/*
	 * Create a placeholder task for the benefit of convert_task_to_port.
	 * Set new_task->map to VM_MAP_NULL so that task_deallocate will
	 * know that this is only a placeholder task.
	 */
	new_task = (task_t) zalloc(task_zone);
	if (new_task == TASK_NULL) {
		panic("task_create: no memory for task structure");
	}

#if     MCMSG
	new_task->mcmsg_task = 0;
#endif  /* MCMSG */

	/* only one ref, for our caller */
	new_task->ref_count = 1;

	new_task->map = VM_MAP_NULL;
	new_task->itk_self = remote_task;
	mutex_init(&new_task->lock, ETAP_NORMA_TASK);
	itk_lock_init(new_task);

	*child_task = new_task;
	return(KERN_SUCCESS);
#else	/* DIPC */
	printf("norma_task_common:  no underlying transport!\n");
	return KERN_INVALID_ARGUMENT;
#endif	/* DIPC */
}


#if	!NORMA_VM
/*
 *	task_vm_copy
 *
 *	Provide a stub for task_copy_vm when we have norma_task but not
 *	norma_vm. This allows us to build and link kernels which support
 *	dipc and norma_node_self without turning on norma_vm.
 *
 *	XXX We need to rearrange things so we can have a dipc subset and
 *	    basic node information system calls without turning EITHER
 *	    norma_task or norma_vm on. This is needed for RFT ports.
 */
void
task_copy_vm(
        ipc_port_t      host,
        vm_map_t        old_map,
        boolean_t       clone,
        boolean_t       kill_parent,
        ipc_port_t      to)
{

	panic("task_copy_vm: norma_vm option not turned on");	

}
#endif	/* !NORMA_VM */

kern_return_t
norma_task_create(
	task_t		parent_task,
	boolean_t	inherit_memory,
	int		child_node,
	task_t		*child_task)		/* OUT */
{
	return norma_task_common(parent_task, inherit_memory, FALSE,
				 FALSE, child_node, child_task);
}

kern_return_t
norma_task_clone(
	task_t		parent_task,
	boolean_t	inherit_memory,
	int		child_node,
	task_t		*child_task)		/* OUT */
{
	return norma_task_common(parent_task, inherit_memory, TRUE,
				 FALSE, child_node, child_task);
}

kern_return_t
norma_task_teleport(
	task_t		parent_task,
	boolean_t	inherit_memory,
	int		child_node,
	task_t		*child_task)		/* OUT */
{
	return norma_task_common(parent_task, inherit_memory, TRUE,
				 TRUE, child_node, child_task);
}

#else	/* NORMA_TASK */

/*ARGSUSED*/
kern_return_t
norma_task_allocate(
	host_t				host,
	int				vector_start,
	emulation_vector_t		entry_vector,
	unsigned int			entry_vector_count,
	ipc_port_t			bootstrap,
	norma_registered_port_array_t	registered,
	exception_mask_array_t		exc_masks,
	unsigned			count,
	exception_port_array_t		exc_ports,
	exception_behavior_array_t	exc_behaviors,
	exception_flavor_array_t	exc_flavors
	task_t				*result)
{
	return KERN_FAILURE;
}

/*ARGSUSED*/
kern_return_t
norma_task_create(
	task_t		parent_task,
	boolean_t	inherit_memory,
	int		child_node,
	task_t		*child_task)		/* OUT */
{
	return KERN_FAILURE;
}

/*ARGSUSED*/
kern_return_t
norma_task_clone(
	task_t		parent_task,
	boolean_t	inherit_memory,
	int		child_node,
	task_t		*child_task)		/* OUT */
{
	return KERN_FAILURE;
}

/*ARGSUSED*/
kern_return_t
norma_task_teleport(
	task_t		parent_task,
	boolean_t	inherit_memory,
	int		child_node,
	task_t		*child_task)		/* OUT */
{
	return KERN_FAILURE;
}
#endif	/* NORMA_TASK */
