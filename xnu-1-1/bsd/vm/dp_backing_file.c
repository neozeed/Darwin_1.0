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
#include <mach_nbc.h>
#include <mach/boolean.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/uio.h>
#include <sys/vnode.h>
#include <ufs/ufs/quota.h>
#include <ufs/ufs/inode.h>
#include <sys/namei.h>
#include <sys/mach_swapon.h>
#include <ufs/ffs/fs.h>
#include <sys/mount.h>
#include <net/if.h>
#include <netinet/in.h>
#include <nfs/rpcv2.h>
#include <nfs/nfsproto.h>
#include <nfs/nfs.h>
#undef	fs_fsok
#undef	fs_tsize
#undef	fs_bsize
#undef	fs_blocks
#undef	fs_bfree
#undef	fs_bavail

#include <mach/mach_types.h>
#include <vm/vm_map.h>
#include <vm/vm_kern.h>
#include <kern/parallel.h>
#include <kern/zalloc.h>
#include <kern/kalloc.h>
#include <libkern/libkern.h>
#include <sys/malloc.h>
#include <sys/resourcevar.h>
#include <sys/signalvar.h>

#include <vm/vnode_pager.h>
#include <kern/mapfs.h>

default_pager_init_flag = 0;  /* temporary support for delayed instantiation */
			      /* of default_pager */




/*  Get rid of this when component interface is in place */
struct  host {
        void *host_self;
        void *host_priv_self;
        void *host_security_self;
};      
typedef struct host     host_data_t;
 
extern host_data_t      realhost;
struct vnode		*default_pager_vnode = 0;

struct bs_map		bs_port_table[MAX_BACKING_STORE] = { 
	{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
	{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
	{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
	{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
	{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
	{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
	{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
	{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
	{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},
	{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0}};

/* ###################################################### */


#include <kern/assert.h>

/*
 *	Routine:	macx_swapon
 *	Function:
 *		Syscall interface to add a file to backing store
 */
int
macx_swapon(
	char 	*filename,
	int	flags,
	long	size,
	long	priority)
{
	struct vnode		*vp = 0; 
	struct nameidata 	nd, *ndp;
	struct proc		*p =  current_proc();
	pager_file_t		pf;
	register int		error;
	kern_return_t		kr;
	mach_port_t		backing_store;
	mach_port_t		default_pager_port = MACH_PORT_NULL;
	int			i;

	struct vattr	vattr;

/*
printf("macx_swapon: function called\n");
*/
	ndp = &nd;


	if ((error = suser(p->p_ucred, &p->p_acflag)))
		goto swapon_bailout;

	unix_master();

	if(default_pager_init_flag == 0) {
		start_def_pager(NULL);
		default_pager_init_flag = 1;
	}

	/*
	 * Get a vnode for the paging area.
	 */
	NDINIT(ndp, LOOKUP, FOLLOW | LOCKLEAF, UIO_USERSPACE,
	    filename, p);

	if ((error = namei(ndp)))
		goto swapon_bailout;
	vp = ndp->ni_vp;

	if (vp->v_type != VREG) {
		error = EINVAL;
	        VOP_UNLOCK(vp, 0, p);
		goto swapon_bailout;
	}

	if (!vp->v_vm_info) {
		vm_info_init(vp);
	}

	if (error = VOP_GETATTR(vp, &vattr, p->p_ucred, p)) {
	        VOP_UNLOCK(vp, 0, p);
		goto swapon_bailout;
	}

/*
printf("macx_swapon: check size va_size = 0x%x, lowat = 0x%x\n", (int)vattr.va_size, size);
*/
        if (vattr.va_size < (u_quad_t)size) {
                vattr_null(&vattr);
                vattr.va_size = (u_quad_t)size;
                error = VOP_SETATTR(vp, &vattr, p->p_ucred, p);
                if (error) {
	       		VOP_UNLOCK(vp, 0, p);
                        goto swapon_bailout;
                }
        }


	vp->v_vm_info->vnode_size = vattr.va_size;

	/* add new backing store to list */
	i = 0;
	while(bs_port_table[i].vp != 0) {
		if(i == MAX_BACKING_STORE)
			break;
		i++;
	}
	if(i == MAX_BACKING_STORE) {
	   	error = ENOMEM;
	        VOP_UNLOCK(vp, 0, p);
		goto swapon_bailout;
	}
	bs_port_table[i].vp = vp;
	

	/*
	 * Look to see if we are already paging to this file.
	 */
	/* make certain the copy send of kernel call will work */
/*
put this back in when component interfaces are available
	kr = host_default_memory_manager(mach_host_self(),
					&default_pager_port, 0);
*/
	kr = host_default_memory_manager(realhost.host_priv_self, &default_pager_port, 0);
	if(kr != KERN_SUCCESS) {
	   error = EAGAIN;
	   VOP_UNLOCK(vp, 0, p);
	   bs_port_table[i].vp = 0;
	   goto swapon_bailout;
	}

	kr = default_pager_backing_store_create(default_pager_port, 
					-1, /* default priority */
					0, /* default cluster size */
					&backing_store);
	if(kr != KERN_SUCCESS) {
	   error = ENOMEM;
	   VOP_UNLOCK(vp, 0, p);
	   bs_port_table[i].vp = 0;
	   goto swapon_bailout;
	}


/* NOTE: we are able to supply PAGE_SIZE here instead of an actual */
/* record size or block number because:  a: we do not support offsets */
/* from the beginning of the file (allowing for non page size/record */
/* modulo offsets.  b: because allow paging will be done modulo page size */

/*
printf("macx_swapon: calling default_pager_add_file, bs port 0x%x, vnode 0x%x, record_size 0x%x, size 0x%x\n",backing_store, vp, PAGE_SIZE, (int)vattr.va_size);
*/
	VOP_UNLOCK(vp, 0, p);
	kr = default_pager_add_file(backing_store, vp, PAGE_SIZE, 
			((int)vattr.va_size)/PAGE_SIZE);
	if(kr != KERN_SUCCESS) {
	   bs_port_table[i].vp = 0;
	   if(kr == KERN_INVALID_ARGUMENT)
		error = EINVAL;
	   else 
		error = ENOMEM;
	   goto swapon_bailout;
	}
	bs_port_table[i].bs = (void *)backing_store;
	/* grab a reference to hold on to the paging file vnode */
	VREF(vp);
	VREF(vp);
	VREF(vp);
	VREF(vp);
	error = 0;

swapon_bailout:
	if (vp) {
		vrele(vp);
	}
	unix_release();
	return(error);
}

/*
 *	Routine:	macx_swapoff
 *	Function:
 *		Syscall interface to remove a file from backing store
 */
int
macx_swapoff(
	char 	*filename,
	int	flags)
{
	kern_return_t	kr;
	mach_port_t	backing_store;

	struct vnode		*vp = 0; 
	struct nameidata 	nd, *ndp;
	struct proc		*p =  current_proc();
	int			i;
	int			error;

/*
printf("macx_swapoff: function called\n");
*/
	backing_store = NULL;
	ndp = &nd;


	if ((error = suser(p->p_ucred, &p->p_acflag)))
		goto swapoff_bailout;

	unix_master();

	/*
	 * Get the vnode for the paging area.
	 */
	NDINIT(ndp, LOOKUP, FOLLOW | LOCKLEAF, UIO_USERSPACE,
	    filename, p);

	if ((error = namei(ndp)))
		goto swapoff_bailout;
	vp = ndp->ni_vp;

	if (vp->v_type != VREG) {
		error = EINVAL;
		VOP_UNLOCK(vp, 0, p);
		goto swapoff_bailout;
	}

	for(i = 0; i < MAX_BACKING_STORE; i++) {
		if(bs_port_table[i].vp == vp) {
			backing_store; 
			break;
		}
	}
	if (i == MAX_BACKING_STORE) {
		error = EINVAL;
		VOP_UNLOCK(vp, 0, p);
		goto swapoff_bailout;
	}
	backing_store = (mach_port_t)bs_port_table[i].bs;
	

	VOP_UNLOCK(vp, 0, p);
	kr = default_pager_backing_store_delete(backing_store);
	switch (kr) {
		case KERN_SUCCESS:
			error = 0;
			bs_port_table[i].vp = 0;
			vrele(vp);
			vrele(vp);
			vrele(vp);
			vrele(vp);
			break;
		case KERN_FAILURE:
			error = EAGAIN;
			break;
		default:
			error = EAGAIN;
			break;
	}
swapoff_bailout:
	if (vp) {
		vrele(vp);
	}
	unix_release();
	return(error);
}

/*
 *	Routine:	mach_swapon
 *	Function:
 *		Syscall interface to add a file to backing store
 */
int
mach_swapon(
	char 	*filename,
	int	flags,
	long	lowat,
	long	hiwat)
{
	struct vnode		*vp = 0; 
	struct nameidata 	nd, *ndp;
	struct proc		*p =  current_proc();
	pager_file_t		pf;
	register int		error;
	kern_return_t		kr;
	mach_port_t		backing_store;
	mach_port_t		default_pager_port = MACH_PORT_NULL;

	struct vattr	vattr;

	ndp = &nd;

	if ((error = suser(p->p_ucred, &p->p_acflag)))
		goto bailout;

	unix_master();

	if(default_pager_init_flag == 0) {
		start_def_pager(NULL);
		default_pager_init_flag = 1;
	}

	/*
	 * Get a vnode for the paging area.
	 */
	NDINIT(ndp, LOOKUP, FOLLOW | LOCKLEAF, UIO_USERSPACE,
	    filename, p);

	if ((error = namei(ndp)))
		goto bailout;
	vp = ndp->ni_vp;

	if (vp->v_type != VREG) {
		error = EINVAL;
		goto bailout;
	}

	if (!vp->v_vm_info) {
		vm_info_init(vp);
	}

	if (error = VOP_GETATTR(vp, &vattr, p->p_ucred, p)) {
		goto bailout;
	}

/*
printf("mach_swapon: check lowat va_size = 0x%x, lowat = 0x%x\n", (int)vattr.va_size, lowat);
*/
        if (vattr.va_size < (u_quad_t)lowat) {
                vattr_null(&vattr);
                vattr.va_size = (u_quad_t)lowat;
                error = VOP_SETATTR(vp, &vattr, p->p_ucred, p);
                if (error) {
                        goto bailout;
                }
        }


	vp->v_vm_info->vnode_size = vattr.va_size;

	/*
	 * Look to see if we are already paging to this file.
	 */
	/* make certain the copy send of kernel call will work */
/*
put this back in when component interfaces are available
	kr = host_default_memory_manager(mach_host_self(),
					&default_pager_port, 0);
*/
	kr = host_default_memory_manager(realhost.host_priv_self, &default_pager_port, 0);
	if(kr != KERN_SUCCESS) {
	   error = kr;
	   goto bailout;
	}

	kr = default_pager_backing_store_create(default_pager_port, 
					-1, /* default priority */
					0, /* default cluster size */
					&backing_store);
	if(kr != KERN_SUCCESS) {
	   error = kr;
	   goto bailout;
	}


/* NOTE: we are able to supply PAGE_SIZE here instead of an actual */
/* record size or block number because:  a: we do not support offsets */
/* from the beginning of the file (allowing for non page size/record */
/* modulo offsets.  b: because allow paging will be done modulo page size */

/*
printf("mach_swapon: calling default_pager_add_file, bs port 0x%x, vnode 0x%x, record_size 0x%x, size 0x%x\n",backing_store, vp, PAGE_SIZE, (int)vattr.va_size);
*/
	kr = default_pager_add_file(backing_store, vp, PAGE_SIZE, 
			((int)vattr.va_size)/PAGE_SIZE);
	if(kr != KERN_SUCCESS) {
	   error = kr;
	   goto bailout;
	}
	/* grab a reference to hold on to the paging file vnode */
	VREF(vp);
	default_pager_vnode = vp;

	error = 0;

bailout:
	if (vp) {
		VOP_UNLOCK(vp, 0, p);
		vrele(vp);
	}
	unix_release();
	return(error);
}
