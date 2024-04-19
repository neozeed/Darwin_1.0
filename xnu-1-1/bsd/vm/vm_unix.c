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
 */
#include <mach_nbc.h>
#include <meta_features.h>

#include <kern/task.h>
#include <kern/thread.h>
#include <mach/time_value.h>
#include <mach/vm_param.h>
#include <mach/vm_prot.h>
#include <mach/port.h>

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/dir.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/vm.h>
#include <sys/file.h>
#include <sys/vnode.h>
#include <sys/buf.h>
#include <sys/mount.h>
#include <sys/trace.h>
#include <sys/kernel.h>

#include <kern/kalloc.h>
#include <kern/parallel.h>
#include <kern/mapfs.h>
#include <vm/vm_map.h>
#include <vm/vm_kern.h>

#include <machine/spl.h>
#include <mach/shared_memory_server.h>

useracc(addr, len, prot)
	caddr_t	addr;
	u_int	len;
	int	prot;
{
	return (vm_map_check_protection(
			current_map(),
			trunc_page(addr), round_page(addr+len),
			prot == B_READ ? VM_PROT_READ : VM_PROT_WRITE));
}

vslock(addr, len)
	caddr_t	addr;
	int	len;
{
	vm_map_wire(current_map(), trunc_page(addr),
			round_page(addr+len), 
			VM_PROT_READ | VM_PROT_WRITE ,FALSE);
}

vsunlock(addr, len, dirtied)
	caddr_t	addr;
	int	len;
	int dirtied;
{
	pmap_t		pmap;
#if FIXME  /* [ */
	vm_page_t	pg;
#endif  /* FIXME ] */
	vm_offset_t	vaddr, paddr;

#if FIXME  /* [ */
	if (dirtied) {
		pmap = get_task_pmap(current_task());
		for (vaddr = trunc_page(addr); vaddr < round_page(addr+len);
				vaddr += PAGE_SIZE) {
			paddr = pmap_extract(pmap, vaddr);
			pg = PHYS_TO_VM_PAGE(paddr);
			vm_page_set_modified(pg);
		}
	}
#endif  /* FIXME ] */
#ifdef	lint
	dirtied++;
#endif	/* lint */
	vm_map_unwire(current_map(), trunc_page(addr),
				round_page(addr+len), FALSE);
}

#if	defined(sun) || BALANCE || defined(m88k)
#else	/*defined(sun) || BALANCE || defined(m88k)*/
subyte(addr, byte)
	void * addr;
	int byte;
{
	char character;
	
	character = (char)byte;
	return (copyout((void *)&(character), addr, sizeof(char)) == 0 ? 0 : -1);
}

suibyte(addr, byte)
	void * addr;
	int byte;
{
	char character;
	
	character = (char)byte;
	return (copyout((void *) &(character), addr, sizeof(char)) == 0 ? 0 : -1);
}

int fubyte(addr)
	void * addr;
{
	unsigned char byte;

	if (copyin(addr, (void *) &byte, sizeof(char)))
		return(-1);
	return(byte);
}

int fuibyte(addr)
	void * addr;
{
	unsigned char byte;

	if (copyin(addr, (void *) &(byte), sizeof(char)))
		return(-1);
	return(byte);
}

suword(addr, word)
	void * addr;
	long word;
{
	return (copyout((void *) &word, addr, sizeof(int)) == 0 ? 0 : -1);
}

long fuword(addr)
	void * addr;
{
	long word;

	if (copyin(addr, (void *) &word, sizeof(int)))
		return(-1);
	return(word);
}

/* suiword and fuiword are the same as suword and fuword, respectively */

suiword(addr, word)
	void * addr;
	long word;
{
	return (copyout((void *) &word, addr, sizeof(int)) == 0 ? 0 : -1);
}

long fuiword(addr)
	void * addr;
{
	long word;

	if (copyin(addr, (void *) &word, sizeof(int)))
		return(-1);
	return(word);
}
#endif	/* defined(sun) || BALANCE || defined(m88k) || defined(i386) */

int
swapon()
{
	return(EOPNOTSUPP);
}

thread_t procdup(
	struct proc		*child,
	struct proc		*parent)
{
	thread_t		thread;
	task_t			task;
 	kern_return_t	result;

	if (parent->task == kernel_task)
		result = task_create_local(TASK_NULL, FALSE, FALSE, &task);
	else
		result = task_create_local(parent->task, TRUE, FALSE, &task);
	if (result != KERN_SUCCESS)
	    printf("fork/procdup: task_create failed. Code: 0x%x\n", result);
	child->task = task;
	/* task->proc = child; */
	set_bsdtask_info(task, child);

	task_deallocate(task); // extra ref for convert_task_to_port()

	result = thread_create(task, &thread);
	if (result != KERN_SUCCESS)
	    printf("fork/procdup: thread_create failed. Code: 0x%x\n", result);
	act_deallocate(thread); 

#if FIXME /* [ */
	thread_deallocate(thread); // extra ref

	/*
	 *	Don't need to lock thread here because it can't
	 *	possibly execute and no one else knows about it.
	 */
	/* compute_priority(thread, FALSE); */
#endif /* ] */
	return(thread);
}

kern_return_t	pid_for_task(t, x)
	mach_port_t	t;
	int	*x;
{
	struct proc * p;
	task_t		t1;
	extern task_t port_name_to_task(mach_port_t t);
	int	pid = -1;
	kern_return_t	err;

	t1 = port_name_to_task(t);

	if (t1 == TASK_NULL) {
		err = KERN_FAILURE;
	} else {
		p = get_bsdtask_info(t1);
		if (p) {
			pid  = p->p_pid;
			err = KERN_SUCCESS;
		} else {
			err = KERN_FAILURE;
		}
	}
	task_deallocate(t1);
	(void) copyout((char *) &pid, (char *) x, sizeof(*x));
	return err;
}

/*
 *	Routine:	task_by_unix_pid
 *	Purpose:
 *		Get the task port for another "process", named by its
 *		process ID on the same host as "target_task".
 *
 *		Only permitted to privileged processes, or processes
 *		with the same user ID.
 */
kern_return_t	task_for_pid(target_tport, pid, t)
	mach_port_t	target_tport;
	int		pid;
	mach_port_t	*t;
{
	struct proc	*p;
	struct proc *p1;
	task_t		t1;
	mach_port_t	tret;
	extern task_t port_name_to_task(mach_port_t tp);
	void * sright;

	unix_master();
	t1 = port_name_to_task(target_tport);
	if (t1 == TASK_NULL) {
		*t = TASK_NULL;
		unix_release();
		return (KERN_FAILURE);
	} 

	p1 = get_bsdtask_info(t1);
	if (
		((p = pfind(pid)) != (struct proc *) 0)
		&& (p1 != (struct proc *) 0)
		&& ((p->p_ucred->cr_uid == p1->p_ucred->cr_uid)
		|| !(suser(p1->p_ucred, &p1->p_acflag)))
		&& (p->p_stat != SZOMB)
		) {
	                task_deallocate(t1);
			if (p->task != TASK_NULL) {
				task_reference(p->task);
#if 0
				sright = retrieve_task_self_fast(p->task);
#else
				sright = convert_task_to_port(p->task);
#endif
				tret = ipc_port_copyout_send(sright, get_task_ipcspace(current_task()));
			} else
				tret  = MACH_PORT_NULL;
			(void ) copyout((char *)&tret, (char *) t, sizeof(mach_port_t));
			/* compatibility with NeXT 2.0 debuggers, launchers */
			if (suser(p->p_ucred, &p->p_acflag)) {
				/* don't set it for anyone; could exec a careless nameserver */
			struct proc *p=get_bsdtask_info(current_task());

				if (p)
					p->p_debugger = 1;
			}
			unix_release();
			return(KERN_SUCCESS);
	}
        task_deallocate(t1);
	tret = MACH_PORT_NULL;
	(void) copyout((char *) &tret, (char *) t, sizeof(mach_port_t));

	unix_release();
	return(KERN_FAILURE);
}

#if 0
/*
 *	Routine:	task_by_pid
 *	Purpose:
 *		Trap form of "task_by_unix_pid"; soon to be eliminated.
 */
mach_port_t		task_by_pid(pid)
	int		pid;
{
	task_t		self = current_task();
	port_t		t = PORT_NULL;
	task_t		result_task;

	if (task_by_unix_pid(self, pid, &result_task) == KERN_SUCCESS) {
		t = convert_task_to_port(result_task);
		if (t != PORT_NULL)
			object_copyout(self, t, MSG_TYPE_PORT, &t);
	}

	return(t);
}
#endif /* 0 */




int
load_shared_file(
	caddr_t		mapped_file_addr,
	u_long		mapped_file_size,
        caddr_t		*base_address,
        int             map_cnt,
        sf_mapping_t       *mappings,
        char            *filename,
        int             *flags)
{
	struct vnode		*vp = 0; 
	struct nameidata 	nd, *ndp;
	struct proc		*p =  current_proc();
	register int		error;
	kern_return_t		kr;

	struct vattr	vattr;
	void 		*pager_cport;
	void 		*object;
	void		*file_object;
        sf_mapping_t    *map_list;
        caddr_t		local_base;
	int		local_flags;
	kern_return_t	kret;

	ndp = &nd;



	unix_master();

	/* Retrieve the base address */
	if (copyin(base_address, &local_base, sizeof (caddr_t))) {
			error = EINVAL;
			goto lsf_bailout;
        }
	if (copyin(flags, &local_flags, sizeof (int))) {
			error = EINVAL;
			goto lsf_bailout;
        }
	kret = kmem_alloc(kernel_map, &map_list,
			(vm_size_t)(map_cnt*sizeof(sf_mapping_t)));
	if (kret != KERN_SUCCESS) {
			error = ENOMEM;
			goto lsf_bailout;
	}

	if (copyin(mappings, map_list, (map_cnt*sizeof(sf_mapping_t)))) {
			kmem_free(kernel_map, map_list, 
				(vm_size_t)(map_cnt*sizeof(sf_mapping_t)));
			error = EINVAL;
			goto lsf_bailout;
	}

	/*
	 * Get a vnode for the target file
	 */
	NDINIT(ndp, LOOKUP, FOLLOW | LOCKLEAF, UIO_SYSSPACE,
	    filename, p);

	if ((error = namei(ndp))) {
		error = EINVAL;
		kmem_free(kernel_map, map_list, 
				(vm_size_t)(map_cnt*sizeof(sf_mapping_t)));
		goto lsf_bailout;
	}

	vp = ndp->ni_vp;

	if (vp->v_type != VREG) {
		error = EINVAL;
		kmem_free(kernel_map, map_list, 
				(vm_size_t)(map_cnt*sizeof(sf_mapping_t)));
	        VOP_UNLOCK(vp, 0, p);
		goto lsf_bailout;
	}

	if (!vp->v_vm_info) {
		vm_info_init(vp);
	}

	if (error = VOP_GETATTR(vp, &vattr, p->p_ucred, p)) {
	        VOP_UNLOCK(vp, 0, p);
		kmem_free(kernel_map, map_list, 
				(vm_size_t)(map_cnt*sizeof(sf_mapping_t)));
		goto lsf_bailout;
	}


	if(pager_cport = (void *)vnode_pager_lookup(vp, vp->v_vm_info->pager)) {
		if(file_object = (void *)vm_object_lookup(pager_cport)) {
			error = 0;
		} else {
	       		VOP_UNLOCK(vp, 0, p);
			kmem_free(kernel_map, map_list, 
				(vm_size_t)(map_cnt*sizeof(sf_mapping_t)));
			error = EINVAL;
			goto lsf_bailout;
		}
	} else {
	       	VOP_UNLOCK(vp, 0, p);
		error = EINVAL;
		goto lsf_bailout;
	}

	vp->v_vm_info->vnode_size = vattr.va_size;

	if(vattr.va_size != mapped_file_size) {
	       	VOP_UNLOCK(vp, 0, p);
		kmem_free(kernel_map, map_list, 
				(vm_size_t)(map_cnt*sizeof(sf_mapping_t)));
		error = EINVAL;
		goto lsf_bailout;
	}

	VREF(vp);
	if(copyin_shared_file(mapped_file_addr, mapped_file_size, &local_base,
			map_cnt, map_list, file_object, &local_flags)) {
		vrele(vp);
		error = EINVAL;
	} else {
		copyout(&local_flags, flags, sizeof (int));
		copyout(&local_base, base_address, sizeof (caddr_t));
		if (local_flags & SF_PREV_LOADED) {
			vrele(vp);
		}
	}
	kmem_free(kernel_map, map_list, 
				(vm_size_t)(map_cnt*sizeof(sf_mapping_t)));
	VOP_UNLOCK(vp, 0, p);

lsf_bailout:
	/* deallocate any remaining remnants of mapped file sent by caller */
	vm_deallocate(current_map(), mapped_file_addr, mapped_file_size);
	unix_release();
	return error;
}
