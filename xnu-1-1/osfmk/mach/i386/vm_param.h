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
 * Revision 1.2  1998/09/30 21:21:01  wsanchez
 * Merged in IntelMerge1 (mburg: Intel support)
 *
 * Revision 1.1.1.1.6.1  1998/09/30 18:14:00  mburg
 * Changes for Intel port
 *
 * Revision 1.1.1.1  1998/09/22 21:05:31  wsanchez
 * Import of Mac OS X kernel (~semeria)
 *
 * Revision 1.1.1.1  1998/03/07 02:25:47  wsanchez
 * Import of OSF Mach kernel (~mburg)
 *
 * Revision 1.2.45.1  1997/03/27  18:53:17  barbou
 * 	Replaced reference to cpu_x86_model in PMAP_ENTER_386_CHECK() macro
 * 	(replaced by cpuid_family).
 * 	[addis]
 * 	Remove i386 specific PMAP_ENTER macro.
 * 	[95/08/29            rwd]
 * 	[97/02/26            barbou]
 *
 * Revision 1.2.9.13  1996/07/31  09:57:05  paire
 * 	Merged with nmk20b7_shared (1.2.39.1)
 * 	[96/07/24            paire]
 * 
 * Revision 1.2.39.1  1996/06/13  12:39:45  bernadat
 * 	Linux drivers support:
 * 	     Arrange so that physical memory is mapped at
 * 	identical virtual addresses.
 * 	User segment linear address remains 0
 * 	kernel segment linear address is now LINEAR_KERNEL_ADDRESS
 * 	Kernel and user segments linear address now differ.
 * 	[96/06/04            bernadat]
 * 
 * Revision 1.2.9.12  1996/02/02  12:19:49  emcmanus
 * 	Merged with nmk20b5_shared.
 * 	[1996/02/02  11:57:41  emcmanus]
 * 
 * 	Checked in prior to merge.
 * 	[1996/02/02  11:40:14  emcmanus]
 * 
 * Revision 1.2.9.11  1996/01/29  13:15:45  bernadat
 * 	0xefffffff is much to large for VM_MAX_KERNEL_LOADED_ADDRESS.
 * 	Page tables overwrite BIOS ! Must find away to dynamically
 * 	allocate kernel page tables. Set it to 0xdfffffff.
 * 	[96/01/29            bernadat]
 * 
 * Revision 1.2.9.10  1996/01/25  12:58:26  bernadat
 * 	Linux Server Support:
 * 	increased VM_MAX_KERNEL_LOADED_ADDRESS.
 * 	[95/12/28            barbou]
 * 
 * Revision 1.2.34.1  1995/12/30  17:12:54  emcmanus
 * 	Defined machine_btop here to avoid ugly #ifdefs in drivers.
 * 	[1995/12/30  17:04:42  emcmanus]
 * 
 * Revision 1.2.9.9  1995/06/13  18:40:24  bolinger
 * 	Broader fix for ri-osc CR1387:  for x86 only, change PMAP_ENTER
 * 	to ensure that VM_PROT_WRITE is always set on calls to change
 * 	kernel_pmap.
 * 	[1995/06/12  19:16:51  bolinger]
 * 
 * Revision 1.2.9.8  1995/05/22  20:26:50  dwm
 * 	ri-osc CR1340 - turning off TASK_SWAPPER doesn't eliminate
 * 	thread swapping, so kernel stack size must be congruent with
 * 	zero mod page_size unless both TASK+THREAD_SWAPPER are off.
 * 	[1995/05/22  20:26:15  dwm]
 * 
 * Revision 1.2.9.7  1995/05/14  18:19:59  dwm
 * 	ri-osc CR1304 - merge (nmk19_latest - nmk19b1) diffs into mainline.
 * 	fix use of constant in comment, breaks ancient grep script
 * 	[1995/05/14  17:34:44  dwm]
 * 
 * Revision 1.2.9.6  1995/04/07  19:05:17  barbou
 * 	Kernel stack needs to be a multiple of page_size if TASK_SWAPPER is on,
 * 	or we won't be able to unwire them when swapping out.
 * 	[95/03/10            barbou]
 * 
 * Revision 1.2.9.5  1995/03/15  17:19:33  bruel
 * 	Increased Vm Min Kernel Loaded Address
 * 	(Corollary kernels are link edited at 0xc4000000)
 * 	[95/02/27            bernadat]
 * 	[95/03/06            bruel]
 * 
 * Revision 1.2.18.1  1995/05/02  21:57:51  travos
 * 	Use large stacks for XKMACHKERNEL (same as NORMA_VM)
 * 	[1995/05/02  21:56:52  travos]
 * 
 * Revision 1.2.9.4  1994/09/23  02:37:53  ezf
 * 	change marker to not FREE
 * 	[1994/09/22  21:40:25  ezf]
 * 
 * Revision 1.2.9.3  1994/08/31  15:55:53  dwm
 * 	mk6 CR300 - INTSTACK_SIZE needs to be one whole page
 * 	          - bump collo range for larger MP configs
 * 	[1994/08/31  15:51:37  dwm]
 * 
 * Revision 1.2.9.2  1994/07/14  20:47:05  bolinger
 * 	Put use of kernel feature header file under #ifdef MACH_KERNEL.
 * 	[1994/07/14  20:46:45  bolinger]
 * 
 * Revision 1.2.9.1  1994/07/14  16:01:49  bolinger
 * 	If NORMA_VM, make kernel thread and interrupt stack size be 8KB.
 * 	(NORMA-associated threads seem to need at least 5KB.)
 * 	[1994/07/14  16:01:01  bolinger]
 * 
 * Revision 1.2.6.5  1994/02/16  20:27:59  condict
 * 	Export the range of kernel virtual addresses that are reserved
 * 	for use by kernel-loaded servers.
 * 	[1994/02/16  19:42:48  condict]
 * 
 * Revision 1.2.6.4  1994/02/09  00:40:49  dwm
 * 	Reduce kernel & interrupt stack size to 1/2 page, in the
 * 	never-ending quest to reduce wired memory consumption.
 * 	[1994/02/09  00:34:14  dwm]
 * 
 * Revision 1.2.6.3  1993/12/10  19:37:00  dwm
 * 	Re-hack of workaround: KERNEL_STACK_SIZE back to 1 page;
 * 	lower THREAD_STATE_MAX to 64 ints instead.
 * 	[1993/12/10  19:36:34  dwm]
 * 
 * Revision 1.2.6.2  1993/10/26  21:54:01  dwm
 * 	Coloc: Bump kernel stack size to 2 pages temporarily; mach/exc_user.c
 * 	builds the exception msg on the stack, mig seemingly ignores -maxonstack,
 * 	and I don't want to lower THREAD_STATE_MAX and have to rebuild the server
 * 	and risk breaking some RISC variant for now.  Yuck.
 * 	(all this in preparation for RPC short-circuiting of exception path).
 * 	[1993/10/26  20:55:16  dwm]
 * 
 * Revision 1.2.2.3  1993/07/12  18:07:25  gm
 * 	CR9339: Added trailing 'U' to VM_*_ADDRESS definitions.
 * 	[1993/07/12  13:36:47  gm]
 * 
 * Revision 1.2.2.2  1993/06/09  02:40:56  gm
 * 	Added to OSF/1 R1.3 from NMK15.0.
 * 	[1993/06/02  21:16:35  jeffc]
 * 
 * Revision 1.2  1993/04/19  16:34:29  devrcs
 * 	ansi C conformance changes
 * 	[1993/02/02  18:56:25  david]
 * 
 * Revision 1.1  1992/09/30  02:30:55  robert
 * 	Initial revision
 * 
 * $EndLog$
 */
/* CMU_HIST */
/*
 * Revision 2.5.2.1  92/03/03  16:21:49  jeffreyh
 * 	Changes from TRUNK
 * 	[92/02/26  12:01:03  jeffreyh]
 * 
 * Revision 2.6  92/01/03  20:20:33  dbg
 * 	Drop back to 1-page kernel stacks, since emulation_vector calls
 * 	now pass data out-of-line.
 * 	[92/01/03            dbg]
 * 
 * Revision 2.5  91/11/19  08:08:35  rvb
 * 	NORMA needs a larger stack so we do it for everyone,
 * 	since stack space usage does not matter anymore.
 * 
 * Revision 2.4  91/05/14  16:52:50  mrt
 * 	Correcting copyright
 * 
 * Revision 2.3  91/02/05  17:32:30  mrt
 * 	Changed to new Mach copyright
 * 	[91/02/01  17:10:30  mrt]
 * 
 * Revision 2.2  90/05/03  15:48:20  dbg
 * 	Move page-table definitions into i386/pmap.h.
 * 	[90/04/05            dbg]
 * 
 * 	Remove misleading comment about kernel stack size.
 * 	[90/02/05            dbg]
 * 
 * Revision 1.3  89/03/09  20:20:06  rpd
 * 	More cleanup.
 * 
 * Revision 1.2  89/02/26  13:01:13  gm0w
 * 	Changes for cleanup.
 * 
 * 31-Dec-88  Robert Baron (rvb) at Carnegie-Mellon University
 *	Derived from MACH2.0 vax release.
 *
 * 16-Jan-87  David Golub (dbg) at Carnegie-Mellon University
 *	Made vax_ptob return 'unsigned' instead of caddr_t.
 *
 */
/* CMU_ENDHIST */
/* 
 * Mach Operating System
 * Copyright (c) 1991,1990,1989,1988 Carnegie Mellon University
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
 * Copyright (c) 1994 The University of Utah and
 * the Computer Systems Laboratory at the University of Utah (CSL).
 * All rights reserved.
 *
 * Permission to use, copy, modify and distribute this software is hereby
 * granted provided that (1) source code retains these copyright, permission,
 * and disclaimer notices, and (2) redistributions including binaries
 * reproduce the notices in supporting documentation, and (3) all advertising
 * materials mentioning features or use of this software display the following
 * acknowledgement: ``This product includes software developed by the
 * Computer Systems Laboratory at the University of Utah.''
 *
 * THE UNIVERSITY OF UTAH AND CSL ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS
 * IS" CONDITION.  THE UNIVERSITY OF UTAH AND CSL DISCLAIM ANY LIABILITY OF
 * ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * CSL requests users of this software to return to csl-dist@cs.utah.edu any
 * improvements that they make and grant CSL redistribution rights.
 *
 */

/*
 *	File:	vm_param.h
 *	Author:	Avadis Tevanian, Jr.
 *	Date:	1985
 *
 *	I386 machine dependent virtual memory parameters.
 *	Most of the declarations are preceeded by I386_ (or i386_)
 *	which is OK because only I386 specific code will be using
 *	them.
 */

#ifndef	_MACH_I386_VM_PARAM_H_
#define _MACH_I386_VM_PARAM_H_

#define BYTE_SIZE	8	/* byte size in bits */

#define I386_PGBYTES	4096	/* bytes per 80386 page */
#define I386_PGSHIFT	12	/* number of bits to shift for pages */

/*
 *	Convert bytes to pages and convert pages to bytes.
 *	No rounding is used.
 */

#define i386_btop(x)		(((unsigned)(x)) >> I386_PGSHIFT)
#define machine_btop(x)		i386_btop(x)
#define i386_ptob(x)		(((unsigned)(x)) << I386_PGSHIFT)

/*
 *	Round off or truncate to the nearest page.  These will work
 *	for either addresses or counts.  (i.e. 1 byte rounds to 1 page
 *	bytes.
 */

#define i386_round_page(x)	((((unsigned)(x)) + I386_PGBYTES - 1) & \
					~(I386_PGBYTES-1))
#define i386_trunc_page(x)	(((unsigned)(x)) & ~(I386_PGBYTES-1))

#define VM_MIN_ADDRESS		((vm_offset_t) 0)
#define VM_MAX_ADDRESS		((vm_offset_t) 0xc0000000U)

#define LINEAR_KERNEL_ADDRESS	((vm_offset_t) 0xc0000000)

#define VM_MIN_KERNEL_ADDRESS	((vm_offset_t) 0x00000000U)
#define VM_MAX_KERNEL_ADDRESS	((vm_offset_t) 0x3fffffffU)

#define VM_MIN_KERNEL_LOADED_ADDRESS	((vm_offset_t) 0x0c000000U)
#define VM_MAX_KERNEL_LOADED_ADDRESS	((vm_offset_t) 0x1fffffffU)

/* FIXME  - always leave like this? */
#define	INTSTACK_SIZE	(I386_PGBYTES*4)
#define	KERNEL_STACK_SIZE	(I386_PGBYTES*4)

#if 0		/* FIXME */

#include <norma_vm.h>
#include <xkmachkernel.h>
#include <task_swapper.h>
#include <thread_swapper.h>

#if defined(AT386)
#include <i386/cpuid.h>
#endif

#if !NORMA_VM && !XKMACHKERNEL
#if !TASK_SWAPPER && !THREAD_SWAPPER
#define KERNEL_STACK_SIZE	(I386_PGBYTES/2)
#else
/* stack needs to be a multiple of page size to get unwired  when swapped */
#define KERNEL_STACK_SIZE	(I386_PGBYTES)
#endif	/* TASK || THREAD SWAPPER */
#define INTSTACK_SIZE		(I386_PGBYTES)	/* interrupt stack size */
#else	/* NORMA_VM  && !XKMACHKERNEL */
#define KERNEL_STACK_SIZE	(I386_PGBYTES*2)
#define INTSTACK_SIZE		(I386_PGBYTES*2)	/* interrupt stack size */
#endif	/* NORMA_VM && !XKMACHKERNEL */
#endif	/* MACH_KERNEL */

/*
 *	Conversion between 80386 pages and VM pages
 */

#define trunc_i386_to_vm(p)	(atop(trunc_page(i386_ptob(p))))
#define round_i386_to_vm(p)	(atop(round_page(i386_ptob(p))))
#define vm_to_i386(p)		(i386_btop(ptoa(p)))

/*
 *	Physical memory is mapped 1-1 with virtual memory starting
 *	at VM_MIN_KERNEL_ADDRESS.
 */
#define phystokv(a)	((vm_offset_t)(a) + VM_MIN_KERNEL_ADDRESS)

/*
 *	For 386 only, ensure that pages are installed in the
 *	kernel_pmap with VM_PROT_WRITE enabled.  This avoids
 *	code in pmap_enter that disallows a read-only mapping
 *	in the kernel's pmap.  (See ri-osc CR1387.)
 *
 *	An entry in kernel_pmap is made only by the kernel or
 *	a collocated server -- by definition (;-)), the requester
 *	is trusted code.  If it asked for read-only access,
 *	it won't attempt a write.  We don't have to enforce the
 *	restriction.  (Naturally, this assumes that any collocated
 *	server will _not_ depend on trapping write accesses to pages
 *	mapped read-only; this cannot be made to work in the current
 *	i386-inspired pmap model.)
 */

/*#if defined(AT386)

#define PMAP_ENTER_386_CHECK \
	if (cpuid_family == CPUID_FAMILY_386)

#else -- FIXME? We're only running on Pentiums or better */

#define PMAP_ENTER_386_CHECK

/*#endif*/

#define PMAP_ENTER(pmap, virtual_address, page, protection, wired) \
	MACRO_BEGIN					\
	vm_prot_t __prot__ =				\
		(protection) & ~(page)->page_lock;	\
							\
    	PMAP_ENTER_386_CHECK				\
	if ((pmap) == kernel_pmap)			\
		__prot__ |= VM_PROT_WRITE;		\
	pmap_enter(					\
		(pmap),					\
		(virtual_address),			\
		(page)->phys_addr,			\
		__prot__,				\
		(wired)					\
	 );						\
	MACRO_END

#endif	/* _MACH_I386_VM_PARAM_H_ */
