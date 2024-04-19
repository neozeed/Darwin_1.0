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
 * Mach Operating System
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/*
 *	File:	mapfs.c
 *	Author:	Avadis Tevanian, Jr.
 *
 *	Copyright (C) 1987, Avadis Tevanian, Jr.
 *
 *	Support for mapped file system implementation.
 *
 * HISTORY
 * 6-Dev-1997 A.Ramesh at Apple
 * 	Made the chages for MacOSX; Reanamed mfs to mapfs to avoid confusion
 *	with memory based filesystem.
 *
 * 18-Nov-92 Phillip Dibner at NeXT
 *	Made the i/o throttle global.   This is a hack on top of a hack and 
 *	should be fixed properly, probably in the vm system.
 *
 *  3-Sep-92 Joe Murdock at NeXT
 *	Added an i/o throttle to mfs_io as a cheap work-around for a i/o buffer
 *	resource conflict with usr-space system bottle-necks (nfs servers, etc)
 *
 *  7-Feb-92 Jim Hays
 *	There are still bugs in this code dealing with vmp_pushing wired 
 *	pages. We need to modify the sound drivers locks to be breakable
 *	except during the actual playing. 
 *
 *  3-Aug-90  Doug Mitchell at NeXT
 *	Added primitives for loadable file system support.
 *
 *  7-Mar-90  Brian Pinkerton (bpinker) at NeXT
 *	Changed mfs_trunc to return an indication of change.
 *
 *  9-Mar-88  John Seamons (jks) at NeXT
 *	SUN_VFS: allocate vm_info structures from a zone.
 *
 * 29-Jan-88  David Golub (dbg) at Carnegie-Mellon University
 *	Corrected calls to inode_pager_setup and kmem_alloc.
 *
 * 15-Sep-87  Michael Young (mwyoung) at Carnegie-Mellon University
 *	De-linted.
 *
 * 18-Jun-87  Michael Young (mwyoung) at Carnegie-Mellon University
 *	Make most of this file dependent on MACH_NBC.
 *
 * 30-Apr-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Created.
 */


#include <kern/mapfs.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mount.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/vnode.h>
#include <sys/uio.h>
#include <ufs/ufs/quota.h>
#include <ufs/ufs/inode.h>
#include <sys/dir.h>
#include <vm/vm_kern.h>
#include <mach/vm_param.h>
#include <mach/machine.h>
#include <vm/vnode_pager.h>
#include <sys/lock.h>

struct zone	*vm_info_zone;

/*
 *	Private variables and macros.
 */

queue_head_t		vm_info_queue;		/* lru list of structures */
decl_simple_lock_data(,	vm_info_lock_data)	/* lock for lru list */
int			vm_info_version = 0;	/* version number */

#define	vm_info_lock()		simple_lock(&vm_info_lock_data)
#define	vm_info_unlock()	simple_unlock(&vm_info_lock_data)




/*
 *	mapfs_init:
 *
 *	Initialize the mapped FS module.
 */
int
mapfs_init()
{
	int			i;

	queue_init(&vm_info_queue);
	simple_lock_init(&vm_info_lock_data);

	i = (vm_size_t) sizeof (struct vm_info);
	vm_info_zone = zinit (i, 10000*i, 8192, "vm_info zone");
	return(0);
}

/*
 *	vm_info_init:
 *
 *	Initialize a vm_info structure for a vnode.
 */
int
vm_info_init(vp)
	struct vnode *vp;
{
	register struct vm_info	*vmp;

	vmp = vp->v_vm_info;
	if (vmp == VM_INFO_NULL)
		vmp = (struct vm_info *) zalloc(vm_info_zone);
	vmp->pager = 0;
	vmp->map_count = 0;
	vmp->use_count = 0;
	vmp->va = 0;
	vmp->size = 0;
	vmp->offset = 0;
	vmp->cred = (struct ucred *) NULL;
	vmp->error = 0;
	vmp->queued = FALSE;
	vmp->dirty = FALSE;
	vmp->close_flush = TRUE;	/* for safety, reconsider later */
	vmp->mapped = FALSE;
	vmp->invalidate = FALSE;
	vmp->vnode_size = 0;
#if FIXME
	lock_init(&vmp->lock, TRUE);	/* sleep lock */
#endif /* FIXME */
	vmp->object = VM_OBJECT_NULL;
	vp->v_vm_info = vmp;
	return(0);
}

/*
 * Loadable file system support to avoid exporting struct vm_info.
 */
void vm_info_free(struct vnode *vp)
{
	zfree(vm_info_zone, (vm_offset_t)vp->v_vm_info);
}

