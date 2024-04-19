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
 *	File:	vnode_pager.c
 *
 *	"Swap" pager that pages to/from vnodes.  Also
 *	handles demand paging from files.
 *
 * HISTORY
 * 12-Mar-86  David Golub (dbg) at Carnegie-Mellon University
 *	Created.
 */

#include <mach_nbc.h>

#include <mach/boolean.h>
#include <sys/param.h>
#include <sys/systm.h>
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
#include <sys/lock.h>
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
#include	<libkern/libkern.h>

#include <vm/vnode_pager.h>
#include <kern/mapfs.h>

#include <kern/assert.h>

int print_nfs_addr=0;

extern struct proc proc0;
extern struct vnode   *default_pager_vnode;

pager_return_t
pager_vnode_pageout(struct vnode *vp,
	void *addr,
	vm_offset_t f_offset,
	vm_size_t size,
	int * errorp)
{
	int		result = PAGER_SUCCESS;
	pf_entry	entry;
	struct proc 	*p = &proc0;
	int		error = 0;
	vm_offset_t	ioaddr = (vm_offset_t)addr;
	struct uio	auio;
	struct iovec	aiov;
	int file_max_size = 0;
	int sub_size = 0;
	boolean_t	funnel_state;

	funnel_state = thread_set_funneled(TRUE);	

	if (ioaddr) {

	   while(TRUE) {
		auio.uio_iov = &aiov;
		auio.uio_iovcnt = 1;
		auio.uio_offset = f_offset;
		auio.uio_segflg = UIO_SYSSPACE;
		auio.uio_rw = UIO_WRITE;
		auio.uio_procp = NULL;

#if MACH_NBC
		auio.uio_resid = size;
		aiov.iov_len = size;
		sub_size = size;
#else
		auio.uio_resid = PAGE_SIZE;
		aiov.iov_len = PAGE_SIZE;
		sub_size = PAGE_SIZE;
#endif
		aiov.iov_base = (caddr_t)ioaddr;


/* this is necessary if we are locking only to sync read/write */
#if     MACH_NBC
		{
#define VNODE_LOCK_RETRY_COUNT  1
#define VNODE_LOCK_RETRY_TICKS  2

			int retry = VNODE_LOCK_RETRY_COUNT;
vnode_lock_retry:
			error = vn_lock(vp, LK_EXCLUSIVE | LK_NOWAIT | LK_CANRECURSE, p);
			if (error) {
				/*
				 * Retry the lock after yielding to other threads.
				 * Not doing this makes the pageout thread compute
				 * bound.  Yielding lets I/O threads run
				 * and make forward progress.
				 */
				if (retry-- > 0) {
					assert_wait_timeout(
						       VNODE_LOCK_RETRY_TICKS,
						       THREAD_INTERRUPTIBLE);
					thread_block((void (*)(void)) 0);
					goto vnode_lock_retry;
				}
							
				result = KERN_FAILURE;
				break;
			}
		}
#else /* MACH_NBC */
		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY | LK_CANRECURSE, p);
#endif /* MACH_NBC */

/*
		error = VOP_PAGEOUT(vp, &auio, IO_SYNC, p->p_ucred);
*/
		error = VOP_PAGEOUT(vp, &auio, 0, p->p_ucred);

		if (error) {
		   if (error == ENOSPC)
		      result = KERN_RESOURCE_SHORTAGE;
		   else
		      result = KERN_FAILURE;

	           error = PAGER_ERROR;
		}
		if(vp->v_vm_info)
			vp->v_vm_info->error = error;

		VOP_UNLOCK(vp, 0, p);

		/* vnode_pageio_complete(m, ioaddr); */
#if MACH_NBC
		break;
#else
		size -= PAGE_SIZE;
		if ((size <= 0) || error)
			break;
		f_offset += PAGE_SIZE;
		ioaddr += PAGE_SIZE;
#endif
	       }
	    }
	    else {
		result = KERN_FAILURE;
		error = PAGER_ERROR;
	    }

	if (errorp)
	    *errorp = result;

	(void) thread_set_funneled(funnel_state);

	return (error);
}

pager_return_t
vnode_pageout(struct vnode *vp,
	void *addr,
	vm_offset_t f_offset,
	vm_size_t size,
	int * errorp)
{
	int		result = PAGER_SUCCESS;
	pf_entry	entry;
	struct proc 	*p = current_proc();
	int		error = 0;
	vm_offset_t	ioaddr = (vm_offset_t)addr;
	struct uio	auio;
	struct iovec	aiov;
	int file_max_size = 0;
	int sub_size = 0;
	boolean_t	funnel_state;

	funnel_state = thread_set_funneled(TRUE);	

	if (ioaddr) {

	   while(TRUE) {
		auio.uio_iov = &aiov;
		auio.uio_iovcnt = 1;
		auio.uio_offset = f_offset;
		auio.uio_segflg = UIO_SYSSPACE;
		auio.uio_rw = UIO_WRITE;
		auio.uio_procp = NULL;

#if MACH_NBC
		auio.uio_resid = size;
		aiov.iov_len = size;
		sub_size = size;
#else
		auio.uio_resid = PAGE_SIZE;
		aiov.iov_len = PAGE_SIZE;
		sub_size = PAGE_SIZE;
#endif
		aiov.iov_base = (caddr_t)ioaddr;

/* this is necessary if we are locking only to sync read/write */
/* The MACH_NBC flag has been used below also to support multiple pageout */
/* request when the file system can only support one.  			  */
#if     MACH_NBC
		{
#define VNODE_LOCK_RETRY_COUNT  1
#define VNODE_LOCK_RETRY_TICKS  2

			int retry = VNODE_LOCK_RETRY_COUNT;
vnode_lock_retry:
			error = vn_lock(vp, LK_EXCLUSIVE | LK_NOWAIT | LK_CANRECURSE, p);
			if (error) {
				/*
				 * Retry the lock after yielding to other threads.
				 * Not doing this makes the pageout thread compute
				 * bound.  Yielding lets I/O threads run
				 * and make forward progress.
				 */
				if (retry-- > 0) {
					assert_wait_timeout(
						    VNODE_LOCK_RETRY_TICKS,
						    THREAD_INTERRUPTIBLE);
					thread_block((void (*)(void)) 0);
					goto vnode_lock_retry;
				}
				
				result = KERN_FAILURE;
				break;
			}
		}
#else /* MACH_NBC */
		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY | LK_CANRECURSE, p);
#endif /* MACH_NBC */



		error = VOP_PAGEOUT(vp, &auio, 0, p->p_ucred);

		if (error) {
		    result = PAGER_ERROR;
		    error = PAGER_ERROR;
		}

		if(vp->v_vm_info)
			vp->v_vm_info->error = error;

		VOP_UNLOCK(vp, 0, p);

		/* vnode_pageio_complete(m, ioaddr); */
#if MACH_NBC
		break;
#else
		size -= PAGE_SIZE;
		if (size <= 0)
			break;
		f_offset += PAGE_SIZE;
		ioaddr += PAGE_SIZE;
#endif
	       }
	    }
	    else {
		result = PAGER_ERROR;
		error = PAGER_ERROR;
	    }

	if (errorp)
	    *errorp = result;

	(void) thread_set_funneled(funnel_state);

	return (error);
}


pager_return_t
pager_vnode_pagein(struct vnode *vp,
	void *addr,
	vm_offset_t f_offset,
	vm_size_t size,
	int * errorp)
{
	int	result = PAGER_SUCCESS;
	pf_entry	entry;
	struct proc	*p = &proc0;
	int		error = 0;
	vm_offset_t		ioaddr = (vm_offset_t) addr;
	struct uio		auio;
	struct iovec	aiov;
	int file_max_size =0;
	boolean_t	funnel_state;

	funnel_state = thread_set_funneled(TRUE);	
		
	if (ioaddr) {
		auio.uio_iov = &aiov;
		auio.uio_iovcnt = 1;
		auio.uio_offset = f_offset;
		auio.uio_segflg = UIO_SYSSPACE;
		auio.uio_rw = UIO_READ;
		auio.uio_resid = size;
		auio.uio_procp = NULL;

		aiov.iov_len = size;
		aiov.iov_base = (caddr_t)ioaddr;

		vn_lock(vp, LK_SHARED | LK_RETRY | LK_CANRECURSE, p);

		error = VOP_PAGEIN(vp, &auio, 0, p->p_ucred);
		if (error) {
		    result = PAGER_ERROR;
		}
		if(vp->v_vm_info)
			vp->v_vm_info->error = error;

		VOP_UNLOCK(vp, 0, p);

		if (!error && auio.uio_resid > 0)
		    (void) memset((void *)(ioaddr + size -
					   auio.uio_resid), 0, auio.uio_resid);

		/* vnode_pageio_complete(m, ioaddr); */
#ifdef	ppc
		/*
		 * After a pagein, we must synchronize the processor caches.
		 * On PPC, the i-cache is not coherent in all models, thus
		 * it needs to be invalidated.
		 */
		/* flush_cache(VM_PAGE_TO_PHYS(m), PAGE_SIZE); */
#endif /* ppc */
	    }
	    else
		result = PAGER_ERROR;

	if (errorp)
	    *errorp = result;

	(void) thread_set_funneled(funnel_state);

	return (error);
}


pager_return_t
vnode_pagein(struct vnode *vp,
	void *addr,
	vm_offset_t f_offset,
	vm_size_t size,
	int * errorp)
{
	int	result = PAGER_SUCCESS;
	pf_entry	entry;
	struct proc	*p = current_proc();
	int		error = 0;
	vm_offset_t		ioaddr = (vm_offset_t) addr;
	struct uio		auio;
	struct iovec	aiov;
	int file_max_size =0;
	boolean_t	funnel_state;

	funnel_state = thread_set_funneled(TRUE);
		
	if (ioaddr) {
		auio.uio_iov = &aiov;
		auio.uio_iovcnt = 1;
		auio.uio_offset = f_offset;
		auio.uio_segflg = UIO_SYSSPACE;
		auio.uio_rw = UIO_READ;
		auio.uio_resid = size;
		auio.uio_procp = NULL;

		aiov.iov_len = size;
		aiov.iov_base = (caddr_t)ioaddr;

		vn_lock(vp, LK_SHARED | LK_RETRY | LK_CANRECURSE, p);

		error = VOP_PAGEIN(vp, &auio, 0, p->p_ucred);
		if (error)
		    result = PAGER_ERROR;

		if(vp->v_vm_info)
			vp->v_vm_info->error = error;

		VOP_UNLOCK(vp, 0, p);

		if (!error && auio.uio_resid > 0)
		    (void) memset((void *)(ioaddr + size -
					   auio.uio_resid), 0, auio.uio_resid);

		/* vnode_pageio_complete(m, ioaddr); */
#ifdef	ppc
		/*
		 * After a pagein, we must synchronize the processor caches.
		 * On PPC, the i-cache is not coherent in all models, thus
		 * it needs to be invalidated.
		 */
		/* flush_cache(VM_PAGE_TO_PHYS(m), PAGE_SIZE); */
#endif /* ppc */
	    }
	    else
		result = PAGER_ERROR;

	if (errorp)
	    *errorp = result;

	(void) thread_set_funneled(funnel_state);

	return (error);
}

void
vnode_pager_shutdown()
{
	int i;
	extern struct bs_map  bs_port_table[];

	if (default_pager_vnode) {
		vrele(default_pager_vnode);
	}
	for(i = 0; i < MAX_BACKING_STORE; i++) {
		if((bs_port_table[i]).vp) {
			vrele((bs_port_table[i]).vp);
			vrele((bs_port_table[i]).vp);
			vrele((bs_port_table[i]).vp);
			vrele((bs_port_table[i]).vp);
			(bs_port_table[i]).vp = 0;
		}
	}
}



/*
 *	Remove an vnode from the object cache.
 */
int
vnode_uncache(vp)
	register struct vnode	*vp;
{
	return(1);
}

void
vnode_pager_setsize(vp, nsize)
struct vnode *vp;
u_long nsize;
{
	if (vp->v_vm_info)
		vp->v_vm_info->vnode_size = nsize;
}


/*
 * Remove vnode associated object from the object cache.
 *
 * XXX unlock the vnode if it is currently locked.
 * We must do this since uncaching the object may result in its
 * destruction which may initiate paging activity which may necessitate
 * re-locking the vnode.
 */
boolean_t
vnode_pager_uncache(vp)
    register struct vnode *vp;
{
    /*
	 * Not a mapped vnode
	 */
	if (vp->v_type != VREG)
		return (TRUE);
#ifdef DEBUG
    if (!VOP_ISLOCKED(vp)) {
	extern int (**nfsv2_vnodeop_p)();

		if (vp->v_op != nfsv2_vnodeop_p)
			panic("vnode_pager_uncache: vnode not locked!");
	}
#endif
	return(vnode_uncache(vp));
}

void
vnode_pager_umount(mp)
    register struct mount *mp;
{
	struct proc *p = current_proc();
	struct vnode *vp, *nvp;

loop:
	for (vp = mp->mnt_vnodelist.lh_first; vp; vp = nvp) {
		if (vp->v_mount != mp)
			goto loop;
		nvp = vp->v_mntvnodes.le_next;
		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, p);
		(void) vnode_pager_uncache(vp);
		VOP_UNLOCK(vp, 0, p);
	}
}
int
vnode_pager_create(vp,pager)
	register struct vnode	*vp;
	void * pager;
{
	vnode_pager_t	vs;
	struct vattr	vattr;
	vm_size_t	size;
	struct proc	*p = current_proc();
	int		error;

	/*
	 *	XXX This can still livelock -- if the
	 *	pageout daemon needs a vnode_pager record
	 *	it won't get one until someone else
	 *	refills the zone.
	 */
#if 0
	if (!vp->v_vm_info) {
		vm_info_init(vp);
	}
#endif
	vp->v_vm_info->pager = pager;

	error = VOP_GETATTR(vp, &vattr, p->p_ucred, p);
	if (!error) {
	    size = vattr.va_size;
	    vp->v_vm_info->vnode_size = size;
	}
	else
	    vp->v_vm_info->vnode_size = 0;
	
	VREF(vp);


	return(0);
}

void
ubc_truncate( vp, length)
	register struct vnode	*vp;
	register vm_offset_t	length;
{
	int error;
	vm_map_t user_map;
	vm_offset_t addr, addr1;
	vm_size_t size, pageoff;
	void *	object;
	void * pager_cport;
	kern_return_t kret;
	register struct vm_info	*vmp;

	vmp = vp->v_vm_info;
	size = round_page(length);

	
	if (ISMAPPEDFILE(vp)) {
		if (length >= vp->v_vm_info->vnode_size) 
			return;

		pageoff = vp->v_vm_info->vnode_size - size;

		if (pager_cport = vnode_pager_lookup(vp,
				vp->v_vm_info->pager)) {
			if(object = vm_object_lookup(pager_cport)) {
				kret = memory_object_lock_request(object,
					size,
					round_page(pageoff),
					MEMORY_OBJECT_RETURN_NONE,TRUE,
					VM_PROT_NO_CHANGE,MACH_PORT_NULL);
#if DIAGNOSTIC
				if (kret != KERN_SUCCESS) {
					printf("ubc:failed to invalidate in truncate\n");
				}
#endif /* DIAGNOSTIC */
			}
		}
	}

}

/* called only if ISMAPPEDFILE */
void
ubc_unlink(vp)
	register struct vnode	*vp;
{
	mach_port_t cport;

	if (cport = vnode_pager_lookup(vp, vp->v_vm_info->pager)){
		memory_object_remove_cached_object(cport);
	}
}
