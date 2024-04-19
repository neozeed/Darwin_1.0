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
 * Revision 1.1.1.1  1998/09/22 21:05:29  wsanchez
 * Import of Mac OS X kernel (~semeria)
 *
 * Revision 1.1.1.1  1998/03/07 02:26:15  wsanchez
 * Import of OSF Mach kernel (~mburg)
 *
 * Revision 1.2.35.1  1997/10/15  15:54:14  barbou
 * 	Made the "ipc_space" and "ipc_tree_entry" zones non-exhaustible. They
 * 	need to grow with the "task" zone which is itself non-exhaustible.
 * 	[1997/10/15  15:52:29  barbou]
 *
 * Revision 1.2.24.7  1995/06/09  02:26:59  mmp
 * 	Add a call to initialize the ikm cache.
 * 	[95/04/20            alanl]
 * 
 * Revision 1.2.24.6  1995/02/24  16:58:12  alanl
 * 	Promoted MACH_RT to a full-fledged build option.  (w/mmp)
 * 	[95/02/21            alanl]
 * 
 * 	DIPC:  Merge from nmk17b2 to nmk18b8.
 * 	[95/01/03            mmp]
 * 
 * Revision 1.2.21.3  1994/12/06  20:09:33  alanl
 * 	Intel merge, Oct 94 code drop.
 * 
 * 	Revision 1.6  1994/07/12  19:21:45  andyp
 * 	Merge of the NORMA2 branch back to the mainline.
 * 
 * 	[...]
 * 
 * 	Add hook to initialize ipc object subsystem for debugging.
 * 	[alanl@osf.org]
 * 	[94/11/10            alanl]
 * 
 * Revision 1.2.21.2  1994/08/11  14:42:31  rwd
 * 	Update zone intialization to use zchange.
 * 	[94/08/04            rwd]
 * 
 * Revision 1.2.15.3  1994/05/20  18:32:49  alanl
 * 	Initialize ipc_space_remote for DIPC.
 * 	[94/05/17            rwd]
 * 
 * Revision 1.2.15.2  1994/04/28  00:26:52  alanl
 * 	Removed redundant dipc_bootstrap call, which was only required
 * 	when all ipc_ports were also having dipc_ports allocated for
 * 	them, automatically.
 * 	[1994/04/27  23:33:49  alanl]
 * 
 * Revision 1.2.15.1  1994/04/20  18:41:46  alanl
 * 	DIPC:  Delete all support for send notifications.  They have
 * 	been removed from the Mach specification and DIPC is easier
 * 	to implement without this feature.  In this file:  removed
 * 	initialization of marequest module.
 * 	DIPC:  add call to bootstrap DIPC.
 * 	[1994/04/20  18:29:30  alanl]
 * 
 * Revision 1.2.21.1  1994/08/04  02:21:21  mmp
 * 	NOTE: file was moved back to b11 version for dipc2_shared.
 * 	Initialize ipc_space_remote for DIPC.
 * 	[94/05/17            rwd]
 * 
 * Revision 1.2.29.2  1994/12/09  22:16:19  dwm
 * 	mk6 CR801 - merge up from nmk18b4 to nmk18b7
 * 	* Rev 1.2.24.4  1994/10/14  10:29:31  bolinger
 * 	  Fix ri-osc CR665:  re-enable the use of a conventional IPC
 * 	  namespace for collocated tasks.
 * 	[1994/12/09  20:53:10  dwm]
 * 
 * Revision 1.2.29.1  1994/11/04  10:05:04  dwm
 * 	mk6 CR668 - 1.3b26 merge
 * 	* Revision 1.2.5.6  1993/11/08  15:04:09  gm
 * 	CR9710: Updated to new zinit() and zone_change() interfaces.
 * 	* End1.3merge
 * 	[1994/11/04  09:16:22  dwm]
 * 
 * Revision 1.2.24.3  1994/09/23  02:07:44  ezf
 * 	change marker to not FREE
 * 	[1994/09/22  21:28:59  ezf]
 * 
 * Revision 1.2.24.2  1994/08/18  23:10:37  widyono
 * 	ipc_init(), init msg_ool_size_small_rt
 * 	[1994/07/28  22:15:24  widyono]
 * 
 * Revision 1.2.24.1  1994/08/07  20:45:30  bolinger
 * 	Merge up to colo_shared (post-b7, pre-MP).
 * 	[1994/08/07  18:34:17  bolinger]
 * 
 * Revision 1.2.12.8  1994/07/13  03:53:06  dwm
 * 	Back out zalloc module changes in the interest of stability.
 * 	[1994/07/13  03:51:03  dwm]
 * 
 * Revision 1.2.12.7  1994/07/07  06:42:03  dwm
 * 	mk6 CR219 - pick up changes from IK (via 1.3) to zalloc module;
 * 	convert zchange() to new zone_change() interface.
 * 	[1994/07/07  06:40:15  dwm]
 * 
 * Revision 1.2.12.6  1994/06/14  19:11:36  dwm
 * 	CR107: back out misguided conditional ipc_entry storage changes.
 * 	[1994/06/14  18:37:10  dwm]
 * 
 * Revision 1.2.12.5  1994/06/08  20:11:28  dswartz
 * 	Preemption merge.
 * 	[1994/06/08  20:09:02  dswartz]
 * 
 * Revision 1.2.12.4  1994/05/13  20:38:33  dwm
 * 	Make ipc_entry storage conditional on is_is_kloaded_space,
 * 	using a separate object type.
 * 	[1994/05/13  20:32:02  dwm]
 * 
 * Revision 1.2.12.3  1994/04/19  02:18:23  bolinger
 * 	Under MACH_DEBUG, enable access to all of the ports in an IPC space
 * 	for kernel-loaded tasks.
 * 	[1994/04/19  02:13:45  bolinger]
 * 
 * Revision 1.2.12.2  1994/01/22  03:38:34  bolinger
 * 	Add code to create new IPC space for kernel-loaded tasks.
 * 	[1994/01/21  22:32:28  bolinger]
 * 
 * Revision 1.2.12.1  1994/01/05  19:42:49  bolinger
 * 	Update calls to kmem_suballoc().
 * 	[1994/01/05  17:41:22  bolinger]
 * 
 * Revision 1.2.5.5  1993/09/09  16:06:26  jeffc
 * 	CR9745 - Delete message accepted notifications
 * 	[1993/09/03  20:44:41  jeffc]
 * 
 * Revision 1.2.5.4  1993/08/05  19:07:06  jeffc
 * 	CR9508 - delete dead Mach3 code. Remove MACH_IPC_TYPED
 * 	[1993/08/04  17:11:50  jeffc]
 * 
 * Revision 1.2.5.3  1993/07/22  16:16:01  rod
 * 	Add ANSI prototypes.  CR #9523.
 * 	[1993/07/22  13:29:53  rod]
 * 
 * Revision 1.2.5.2  1993/06/09  02:31:32  gm
 * 	CR9176 - ANSI C violations: trailing tokens on CPP
 * 	directives, extra semicolons after decl_ ..., asm keywords
 * 	[1993/06/07  19:01:21  jeffc]
 * 
 * 	Added to OSF/1 R1.3 from NMK15.0.
 * 	[1993/06/02  21:09:26  jeffc]
 * 
 * Revision 1.2  1993/04/19  16:19:54  devrcs
 * 	Allocate ipc_kernel_copy_map under !MACH_IPC_TYPED.	[rod.osf.org]
 * 	[1993/04/06  14:32:18  travos]
 * 
 * 	Untyped ipc merge:
 * 	Put mig_init() @ #if !MACH_IPC_TYPED
 * 	[1993/03/04  13:42:28  travos]
 * 	New dispatching model for kernel services: hash instead of linear search
 * 	[1993/03/01  21:51:28  travos]
 * 	Initialization of NDR_record
 * 	[1993/02/17  22:13:07  travos]
 * 	Make msg_ool_size_small a function of kalloc_max_prerounded.
 * 	[1993/02/16  15:09:03  rod]
 * 	Allocate kernel submap for eager ipc copyin
 * 	and mark it wait_for_space and no_zero_fill. [rod@osf.org]
 * 	[1993/01/27  00:46:09  rod]
 * 
 * Revision 1.1  1992/09/30  02:07:22  robert
 * 	Initial revision
 * 
 * $EndLog$
 */
/* CMU_HIST */
/*
 * Revision 2.12.2.2  92/03/03  16:18:29  jeffreyh
 * 	19-Feb-92 David L. Black (dlb) at Open Software Foundation
 * 	Don't make port zone exhaustible.  Kernel panics when
 * 	it fails to allocate an internal port after exhaustion.
 * 	[92/02/26  11:41:12  jeffreyh]
 * 
 * Revision 2.12.2.1  92/01/03  16:34:52  jsb
 * 	Corrected log.
 * 	[91/12/24  14:18:23  jsb]
 * 
 * Revision 2.12  91/12/10  13:25:41  jsb
 * 	Removed reference counting bug workaround.
 * 	[91/12/10  11:17:21  jsb]
 * 
 * Revision 2.11  91/11/19  09:54:12  rvb
 * 	Added reference counting bug workaround.
 * 	[91/11/00            jsb]
 * 
 * Revision 2.10  91/08/03  18:18:12  jsb
 * 	Removed call to ipc_clport_init.
 * 	[91/07/24  22:11:04  jsb]
 * 
 * Revision 2.9  91/06/17  15:46:00  jsb
 * 	Renamed NORMA conditionals.
 * 	[91/06/17  10:45:48  jsb]
 * 
 * Revision 2.8  91/06/06  17:05:46  jsb
 * 	Added call to ipc_clport_init.
 * 	[91/05/13  17:17:10  jsb]
 * 
 * Revision 2.7  91/05/14  16:32:33  mrt
 * 	Correcting copyright
 * 
 * Revision 2.6  91/02/05  17:21:37  mrt
 * 	Changed to new Mach copyright
 * 	[91/02/01  15:45:02  mrt]
 * 
 * Revision 2.5  91/01/08  15:13:40  rpd
 * 	Changed zchange calls to make the IPC zones non-collectable.
 * 	[90/12/29            rpd]
 * 
 * Revision 2.4  90/12/20  16:38:41  jeffreyh
 * 	Changes to zchange to account for new collectable field. Made all
 * 	ipc zones collectable.
 * 	[90/12/11            jeffreyh]
 * 
 * Revision 2.3  90/09/28  16:54:44  jsb
 * 	Added NORMA_IPC support.
 * 	[90/09/28  14:02:05  jsb]
 * 
 * Revision 2.2  90/06/02  14:49:55  rpd
 * 	Created for new IPC.
 * 	[90/03/26  20:55:13  rpd]
 * 
 */
/* CMU_ENDHIST */
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
 *	File:	ipc/ipc_init.c
 *	Author:	Rich Draves
 *	Date:	1989
 *
 *	Functions to initialize the IPC system.
 */

#include <mach_debug.h>
#include <dipc.h>
#include <mach_rt.h>

#include <mach/kern_return.h>
#include <kern/mach_param.h>
#include <kern/ipc_host.h>
#include <kern/misc_protos.h>
#include <vm/vm_map.h>
#include <vm/vm_kern.h>
#include <ipc/ipc_entry.h>
#include <ipc/ipc_space.h>
#include <ipc/ipc_object.h>
#include <ipc/ipc_port.h>
#include <ipc/ipc_pset.h>
#include <ipc/ipc_notify.h>
#include <ipc/ipc_kmsg.h>
#include <ipc/ipc_hash.h>
#include <ipc/ipc_init.h>
#include <mach/machine/ndr_def.h>   /* NDR_record */

vm_map_t ipc_kernel_map;
vm_size_t ipc_kernel_map_size = 1024 * 1024;

vm_map_t ipc_kernel_copy_map;
#define IPC_KERNEL_COPY_MAP_SIZE (8 * 1024 * 1024)
vm_size_t ipc_kernel_copy_map_size = IPC_KERNEL_COPY_MAP_SIZE;
vm_size_t ipc_kmsg_max_vm_space = (IPC_KERNEL_COPY_MAP_SIZE * 7)/8;

int ipc_space_max = SPACE_MAX;
int ipc_tree_entry_max = ITE_MAX;
int ipc_port_max = PORT_MAX;
int ipc_pset_max = SET_MAX;

extern void mig_init(void);
extern void ikm_cache_init(void);

/*
 *	Routine:	ipc_bootstrap
 *	Purpose:
 *		Initialization needed before the kernel task
 *		can be created.
 */

void
ipc_bootstrap(void)
{
	kern_return_t kr;

	ipc_port_multiple_lock_init();

	ipc_port_timestamp_lock_init();
	ipc_port_timestamp_data = 0;

	/* all IPC zones should be exhaustible */

	ipc_space_zone = zinit(sizeof(struct ipc_space),
			       ipc_space_max * sizeof(struct ipc_space),
			       sizeof(struct ipc_space),
			       "ipc spaces");
#if 0
	/* make it exhaustible */
	zone_change(ipc_space_zone, Z_EXHAUST, TRUE);
#endif

	ipc_tree_entry_zone =
		zinit(sizeof(struct ipc_tree_entry),
			ipc_tree_entry_max * sizeof(struct ipc_tree_entry),
			sizeof(struct ipc_tree_entry),
			"ipc tree entries");
#if 0
	/* make it exhaustible */
	zone_change(ipc_tree_entry_zone, Z_EXHAUST, TRUE);
#endif

	/*
	 * populate all port(set) zones
	 */
	ipc_object_zones[IOT_PORT] =
		zinit(sizeof(struct ipc_port),
		      ipc_port_max * sizeof(struct ipc_port),
		      sizeof(struct ipc_port),
		      "ipc ports");
	/*
	 * XXX  Can't make the port zone exhaustible because the kernel
	 * XXX	panics when port allocation for an internal object fails.
	 *zone_change(ipc_object_zones[IOT_PORT], Z_EXHAUST, TRUE);
	 */

	ipc_object_zones[IOT_PORT_SET] =
		zinit(sizeof(struct ipc_pset),
		      ipc_pset_max * sizeof(struct ipc_pset),
		      sizeof(struct ipc_pset),
		      "ipc port sets");
	/* make it exhaustible */
	zone_change(ipc_object_zones[IOT_PORT_SET], Z_EXHAUST, TRUE);

	/* create special spaces */

	kr = ipc_space_create_special(&ipc_space_kernel);
	assert(kr == KERN_SUCCESS);


	kr = ipc_space_create_special(&ipc_space_reply);
	assert(kr == KERN_SUCCESS);

#if	DIPC
	kr = ipc_space_create_special(&ipc_space_remote);
	assert(kr == KERN_SUCCESS);
#endif	/* DIPC */

	/* initialize modules with hidden data structures */

#if	MACH_ASSERT
	ipc_port_debug_init();
#endif
	mig_init();
	ipc_table_init();
	ipc_notify_init();
	ipc_hash_init();
	ikm_cache_init();
}

/* 
 * XXX tunable, belongs in mach.message.h 
 */
#define MSG_OOL_SIZE_SMALL 2049
vm_size_t msg_ool_size_small;

#if	MACH_RT
vm_size_t msg_ool_size_small_rt;
#endif	/* MACH_RT */

/*
 *	Routine:	ipc_init
 *	Purpose:
 *		Final initialization of the IPC system.
 */

void
ipc_init(void)
{
	kern_return_t retval;
	vm_offset_t min, max;
	extern vm_size_t kalloc_max_prerounded;
#if	MACH_RT
	extern vm_size_t rtmalloc_max_prerounded;
#endif	/* MACH_RT */

	retval = kmem_suballoc(kernel_map, &min, ipc_kernel_map_size,
			       TRUE, TRUE, &ipc_kernel_map);
	if (retval != KERN_SUCCESS)
		panic("ipc_init: kmem_suballoc of ipc_kernel_map failed");

	retval = kmem_suballoc(kernel_map, &min, ipc_kernel_copy_map_size,
			       TRUE, TRUE, &ipc_kernel_copy_map);
	if (retval != KERN_SUCCESS)
		panic("ipc_init: kmem_suballoc of ipc_kernel_copy_map failed");

	ipc_kernel_copy_map->no_zero_fill = TRUE;
	ipc_kernel_copy_map->wait_for_space = TRUE;

	/*
	 * As an optimization, 'small' out of line data regions using a 
	 * physical copy strategy are copied into kalloc'ed buffers.
	 * The value of 'small' is determined here.  Requests kalloc()
	 * with sizes greater or equal to kalloc_max_prerounded may fail.
	 */
	if (kalloc_max_prerounded <=  MSG_OOL_SIZE_SMALL) {
		msg_ool_size_small = kalloc_max_prerounded;
	}
	else {
		msg_ool_size_small = MSG_OOL_SIZE_SMALL;
	}

#if	MACH_RT
	/*
	 * Real-time IPC, on the other hand, requires that all out-of-line
	 * data be obtained from rtmalloc'ed buffers.
	 */
	msg_ool_size_small_rt = rtmalloc_max_prerounded;
#endif	/* MACH_RT */
	
	ipc_host_init();
}
