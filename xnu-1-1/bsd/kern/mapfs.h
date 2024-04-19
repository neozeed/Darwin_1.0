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
 * HISTORY
 *  3-Aug-90  Doug Mitchell at NeXT
 *	Added prototypes for exported functions.
 *
 * 18-Feb-90  Gregg Kellogg (gk) at NeXT
 *	Merged in with Mach 2.5 stuff.
 *
 *  9-Mar-88  John Seamons (jks) at NeXT
 *	Allocate vm_info structures from a zone.
 *
 * 11-Jun-87  William Bolosky (bolosky) at Carnegie-Mellon University
 *	Changed pager_id to pager.
 *
 * 30-Apr-87  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Created.
 */
/*
 *	File:	mapfs.h
 *	Author:	Avadis Tevanian, Jr.
 *	Copyright (C) 1987, Avadis Tevanian, Jr.
 *
 *	Header file for mapped file system support.
 *
 */ 

#ifndef	_KERN_MAPFS_H_
#define	_KERN_MAPFS_H_

#include <vm/vnode_pager.h>
#include <kern/queue.h>
#include <sys/types.h>
#include <sys/vnode.h>

#include <sys/lock.h>
/*
 *	Associated with each mapped file is information about its
 *	corresponding VM window.  This information is kept in the following
 *	vm_info structure.
 */
struct vm_info {
	vnode_pager_t	pager;		/*  pager */
	short		map_count;	/* number of times mapped */
	short		use_count;	/* number of times in use */
	vm_offset_t	va;		/* mapped virtual address */
	vm_size_t	size;		/* mapped size */
	vm_offset_t	offset;		/* offset into file at va */
	vm_size_t	vnode_size;	/* vnode size (not reflected in ip) */
	void *		lock;		/* lock for changing window */
	void *		object;		/* object [for KERNEL flushing] */
	queue_chain_t	lru_links;	/* lru queue links */
	struct ucred	*cred;		/* vnode credentials */
	int		error;		/* holds error codes */
	u_int	queued:1,	/* on lru queue? */
			dirty:1,	/* range needs flushing? */
			close_flush:1,	/* flush on close */
			invalidate:1,	/* is mapping invalid? */
			busy:1,			/* are we busy conducting an uiomove */
			delayed_fsync:1,/* need to do a push after the uiomove completes */
			filesize:1,		/* want size as reflected in ip */
			mapped:1,		/* mapped into KERNEL VM? */
			dying:1;		/* vm_info being freed */
	vm_size_t	dirtysize;	/* unwritten data so far */
	vm_offset_t	dirtyoffset;	/* unwritten data so far */
};

#define VM_INFO_NULL	((struct vm_info *) 0)

extern struct zone	*vm_info_zone;

/*
 * exported primitives for loadable file systems.
 */
int vm_info_init __P((struct vnode *));
void vm_info_free  __P((struct vnode *));

#define ISMAPPABLEFILE(vp)	\
	(((vp)->v_type == VREG) && ((vp)->v_vm_info))

#define ISMAPPEDFILE(vp)	\
	(((vp)->v_type == VREG) &&	\
	((vp)->v_vm_info) && ((vp)->v_vm_info->mapped))

#define NOTMAPPEDFILE(vp)	\
	(((vp)->v_type == VREG) &&	\
	(((vp)->v_vm_info) && !((vp)->v_vm_info->mapped)))

#define ISMAPFILEFLUSH(vp)	\
	(((vp)->v_type == VREG) &&	\
	((vp)->v_vm_info) && ((vp)->v_vm_info->mapped) && \
	((vp)->v_vm_info->dirty))



#endif	/* _KERN_MAPFS_H_ */

