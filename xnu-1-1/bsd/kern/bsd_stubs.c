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
#include <sys/time.h>
#include <kern/task.h>
#include <kern/thread.h>
#import <mach/mach_types.h>
#include <mach/vm_prot.h>
#include <vm/vm_kern.h>
#include <vm/vm_map.h>
#include <sys/systm.h>
#include <sys/conf.h>

kmem_mb_alloc(vm_map_t  mbmap, int size) 
{
        vm_offset_t addr;
	if (kernel_memory_allocate(mbmap, &addr, size,
   		0,
		KMA_NOPAGEWAIT|KMA_KOBJECT) == KERN_SUCCESS)
                                return((void *)addr);
	else
		return(0);
		
}

vm_offset_t kalloc_noblock(size)
	vm_size_t size;
{	
	return(kalloc(size));
}
vm_offset_t zalloc_noblock(zone)
	register void * zone;
{
	return(zalloc(zone));
}

pcb_synch() {}
task_dowait() {}
task_hold(task_t t) 
{
	return(task_hold_locked(t));
}
task_suspend_nowait(task_t task) 
{
	return(task_suspend(task));
}
task_tllock() {}
task_tlunlock() {}


unix_master() {}
unix_release() {}
vm_map_check_protection() {}
vm_map_find() {}
vm_object_cache_clear() {}

#undef current_proc
struct proc *
current_proc(void)
{
	return((struct proc *)get_bsdtask_info(current_task()));
}
/* Device switch add delete routines */

extern int nblkdev, nchrdev;

struct bdevsw nobdev = NO_BDEVICE;
struct cdevsw nocdev = NO_CDEVICE;
/* 
 *	if index is -1, return a free slot if avaliable
 *	  else see whether the index is free
 *	return the major number that is free else -1
 *
 */
int
bdevsw_isfree(int index)
{
	struct bdevsw *devsw;
	if (index == -1) {
	    devsw = bdevsw;
	    for(index=0; index < nblkdev; index++, devsw++) {
		if(memcmp((char *)devsw, 
			    (char *)&nobdev, 
			    sizeof(struct bdevsw)) == 0)
		    break;
	    }
	}

	if ((index < 0) || (index >= nblkdev) ||
	    (memcmp((char *)devsw, 
		          (char *)&nobdev, 
			  sizeof(struct bdevsw)) != 0)) {
		return(-1);
	}
	return(index);
}

/* 
 *	if index is -1, find a free slot to add
 *	  else see whether the slot is free
 *	return the major number that is used else -1
 */
int
bdevsw_add(int index, struct bdevsw * bsw) 
{
	struct bdevsw *devsw;

	if (index == -1) {
	    devsw = bdevsw;
	    for(index=0; index < nblkdev; index++, devsw++) {
		if(memcmp((char *)devsw, 
			    (char *)&nobdev, 
			    sizeof(struct bdevsw)) == 0)
		    break;
	    }
	}
	devsw = &bdevsw[index];
	if ((index < 0) || (index >= nblkdev) ||
	    (memcmp((char *)devsw, 
		          (char *)&nobdev, 
			  sizeof(struct bdevsw)) != 0)) {
		return(-1);
	}
	bdevsw[index] = *bsw;
	return(index);
}
/* 
 *	if the slot has the same bsw, then remove
 *	else -1
 */
int
bdevsw_remove(int index, struct bdevsw * bsw) 
{
	struct bdevsw *devsw;

	devsw = &bdevsw[index];
	if ((index < 0) || (index >= nblkdev) ||
	    (memcmp((char *)devsw, 
		          (char *)bsw, 
			  sizeof(struct bdevsw)) != 0)) {
		return(-1);
	}
	bdevsw[index] = nobdev;
	return(index);
}

/* 
 *	if index is -1, return a free slot if avaliable
 *	  else see whether the index is free
 *	return the major number that is free else -1
 */
int
cdevsw_isfree(int index)
{
	struct cdevsw *devsw;

	if (index == -1) {
	    devsw = cdevsw;
	    for(index=0; index < nchrdev; index++, devsw++) {
		if(memcmp((char *)devsw, 
			    (char *)&nocdev, 
			    sizeof(struct cdevsw)) == 0)
		    break;
	    }
	}
	devsw = &cdevsw[index];
	if ((index < 0) || (index >= nchrdev) ||
	    (memcmp((char *)devsw, 
		          (char *)&nocdev, 
			  sizeof(struct cdevsw)) != 0)) {
		return(-1);
	}
	return(index);
}

/* 
 *	if index is -1, find a free slot to add
 *	  else see whether the slot is free
 *	return the major number that is used else -1
 */
int
cdevsw_add(int index, struct cdevsw * csw) 
{
	struct cdevsw *devsw;

	if (index == -1) {
	    devsw = cdevsw;
	    for(index=0; index < nchrdev; index++, devsw++) {
		if(memcmp((char *)devsw, 
			    (char *)&nocdev, 
			    sizeof(struct cdevsw)) == 0)
		    break;
	    }
	}
	devsw = &cdevsw[index];
	if ((index < 0) || (index >= nchrdev) ||
	    (memcmp((char *)devsw, 
		          (char *)&nocdev, 
			  sizeof(struct cdevsw)) != 0)) {
		return(-1);
	}
	cdevsw[index] = *csw;
	return(index);
}
/*
 *	if the index has the same bsw, then remove
 *	else -1
 */
int
cdevsw_remove(int index, struct cdevsw * csw) 
{
	struct cdevsw *devsw;

	devsw = &cdevsw[index];
	if ((index < 0) || (index >= nchrdev) ||
	    (memcmp((char *)devsw, 
		          (char *)csw, 
			  sizeof(struct cdevsw)) != 0)) {
		return(-1);
	}
	cdevsw[index] = nocdev;
	return(index);
}

int
memcmp(s1, s2, n)
	register char *s1, *s2;
	register n;
{
	while (--n >= 0)
		if (*s1++ != *s2++)
			return (*--s1 - *--s2);
	return (0);
}
