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
 *	File:	bsd/kern/kern_shutdown.c
 *
 *	Copyright (C) 1989, NeXT, Inc.
 *
 * History:
 *	Jan 29, 1992 mike@next.com
 *		Created by munging machdep/m68k/shutdown.c and
 *		machdep/m68k/machdep.c into machine-independence.
 *
 */

#import <mach_nbc.h>

#import <sys/param.h>
#import <sys/systm.h>
#import <sys/kernel.h>
#import <sys/vm.h>
#import <sys/proc.h>
#import <sys/user.h>
#import <sys/buf.h>
#import <sys/reboot.h>
#import <sys/conf.h>
#import <sys/vnode.h>
#import <sys/file.h>
#import <sys/clist.h>
#import <sys/callout.h>
#import <sys/mbuf.h>
#import <sys/msgbuf.h>
#import <sys/ioctl.h>
#import <sys/signal.h>
#import <sys/tty.h>
#import <kern/task.h>
#import <ufs/ufs/quota.h>
#import <ufs/ufs/inode.h>
#if	NCPUS > 1
#import <kern/processor.h>
#import <kern/thread.h>
#import <sys/lock.h>
#endif	/* NCPUS > 1 */
#import <vm/vm_kern.h>
#import <mach/vm_param.h>
#import <sys/filedesc.h>
#import <mach/host_reboot.h>

int	waittime = -1;

boot(paniced, howto, command)
	int paniced, howto;
	char *command;
{
	register int i;
	int s;
	struct proc *p = current_proc();	/* XXX */
	int hostboot_option=0;
	/* md_prepare_for_shutdown(paniced, howto, command); */

	if ((howto&RB_NOSYNC)==0 && waittime < 0) {
		register struct buf *bp;
		int iter, nbusy;

		waittime = 0;
		
		(void) spl0();

		printf("syncing disks... ");

		/*
		 * Release vnodes held by texts before sync.
		 */
		 
		proc_shutdown();		/* handle live procs (deallocate their
					   				root and current directories). */
#if 0 /* [ */
		/* NOT MY PROBLEM XXX */
		kill_tasks();			/* get rid of all task memory */
#endif  /* 0 ] */

#if MACH_NBC
		mapfs_cache_clear();
		vm_object_cache_clear();	
#endif /* MACH_NBC */
		if (panicstr == 0)
			vnode_pager_shutdown();		/* NO MORE PAGING - release vnodes */

		sync(p, (void *)NULL, (int *)NULL);

		/*
		 * Unmount filesystems
		 */
		if (panicstr == 0)
			vfs_unmountall();

		for (iter = 0; iter < 20; iter++) {
			nbusy = 0;
			for (bp = &buf[nbuf]; --bp >= buf; )
				if ((bp->b_flags & (B_BUSY|B_INVAL)) == B_BUSY)
					nbusy++;
			if (nbusy == 0)
				break;
			printf("%d ", nbusy);
			IOSleep( 4 * nbusy );
		}
		if (nbusy)
			printf("giving up\n");
		else
			printf("done\n");

		/*
		 * If we've been adjusting the clock, the todr
		 * will be out of synch; adjust it now.
		 */
	}

	/*
	 * Can't just use an splnet() here to disable the network
	 * because that will lock out softints which the disk
	 * drivers depend on to finish DMAs.
	 */
	if_down_all();
#define BSD_DUMMY_HOST 1
	if (howto & RB_POWERDOWN)
		hostboot_option = HOST_REBOOT_HALT;
	if (howto & RB_HALT)
		hostboot_option = HOST_REBOOT_HALT;
	if (paniced == RB_PANIC)
		hostboot_option = HOST_REBOOT_HALT;

	if (hostboot_option == HOST_REBOOT_HALT)
	        IOSleep( 1 * 1000 );

	host_reboot(BSD_DUMMY_HOST, hostboot_option);
}

#if 0 /* [ */
kill_tasks()
{
	processor_set_t	pset;
	task_t		task;
	vm_map_t	map, empty_map;

	empty_map = vm_map_create(pmap_create(0), 0, 0, TRUE);

	/*
	 * Destroy all but the default processor_set.
	 */
	simple_lock(&all_psets_lock);
	pset = (processor_set_t) queue_first(&all_psets);
	while (!queue_end(&all_psets, (queue_entry_t) pset)) {
		if (pset == &default_pset) {
			pset = (processor_set_t) queue_next(&pset->all_psets);
			continue;
		}
		simple_unlock(&all_psets_lock);
		processor_set_destroy(pset);
		simple_lock(&all_psets_lock);
		pset = (processor_set_t) queue_first(&all_psets);
	}
	simple_unlock(&all_psets_lock);

	/*
	 * Kill all the tasks in the default processor set.
	 */
	pset = &default_pset;
	pset_lock(pset);
	task = (task_t) queue_first(&pset->tasks);
	while (pset->task_count) {
		pset_remove_task(pset, task);
		map = task->map;
		if ((map != kernel_map) && (map != empty_map)) {
			task->map = empty_map;
			vm_map_reference(empty_map);
			pset_unlock(pset);
			vm_map_remove(map, vm_map_min(map), vm_map_max(map));
			pset_lock(pset);
		}
		task = (task_t) queue_first(&pset->tasks);
	}
	pset_unlock(pset);
}
#endif  /* 0 ] */

#if NeXT
extern int reaper_queue;
#endif
/*
 * proc_shutdown()
 *
 *	Shutdown down proc system (release references to current and root
 *	dirs for each process).
 *
 * POSIX modifications:
 *
 *	For POSIX fcntl() file locking call vno_lockrelease() on 
 *	the file to release all of its record locks, if any.
 */

proc_shutdown()
{
	struct proc	*p, *self;
	struct vnode	**cdirp, **rdirp, *vp;
	int		restart, i, TERM_catch;

	/*
	 *	Kill as many procs as we can.  (Except ourself...)
	 */
	self = (struct proc *)(get_bsdtask_info(current_task()));
	
	/*
	 * Suspend /etc/init
	 */
	p = pfind(1);
	if (p && p != self)
		task_suspend(p->task);		/* stop init */

	/*
	 * Suspend mach_init
	 */
	p = pfind(2);
	if (p && p != self)
		task_suspend(p->task);		/* stop mach_init */

	printf("Killing all processes ");

	/*
	 * send SIGTERM to those procs interested in catching one
	 */
	for (p = allproc.lh_first; p; p = p->p_list.le_next) {
	        if (((p->p_flag&P_SYSTEM) == 0) && (p->p_pptr->p_pid != 0) && (p != self)) {
		        if (p->p_sigcatch & sigmask(SIGTERM))
			        psignal(p, SIGTERM);
		}
	}
	/*
	 * now wait for up to 5 seconds to allow those procs catching SIGTERM
	 * to digest it
	 * as soon as these procs have exited, we'll continue on to the next step
	 */
	for (i = 0; i < 50; i++) {
	        /*
		 * sleep for a tenth of a second
		 * and then check to see if the tasks that were sent a
		 * SIGTERM have exited
		 */
	        IOSleep(100);   
		TERM_catch = 0;

	        for (p = allproc.lh_first; p; p = p->p_list.le_next) {
		        if (((p->p_flag&P_SYSTEM) == 0) && (p->p_pptr->p_pid != 0) && (p != self)) {
			        if (p->p_sigcatch & sigmask(SIGTERM))
				        TERM_catch++;
			}
		}
		if (TERM_catch == 0)
		        break;
	}

	/*
	 * send a SIGKILL to all the procs still hanging around
	 */
	for (p = allproc.lh_first; p; p = p->p_list.le_next) {
	        if (((p->p_flag&P_SYSTEM) == 0) && (p->p_pptr->p_pid != 0) && (p != self))
		        psignal(p, SIGKILL);
	}
	/*
	 * wait for up to 2 seconds to allow these procs to exit normally
	 */
	for (i = 0; i < 20; i++) {
		IOSleep(100);

	        for (p = allproc.lh_first; p; p = p->p_list.le_next) {
		        if (((p->p_flag&P_SYSTEM) == 0) && (p->p_pptr->p_pid != 0) && (p != self))
			        break;
		}
		if (!p)
		        break;
	}

	/*
	 * if we still have procs that haven't exited, then brute force 'em
	 */
	p = allproc.lh_first;
	while (p) {
	        if ((p->p_flag&P_SYSTEM) || (p->p_pptr->p_pid == 0) || (p == self)) {
		        p = p->p_list.le_next;
		}
		else {
		        /*
			 * NOTE: following code ignores sig_lock and plays
			 * with exit_thread correctly.  This is OK unless we
			 * are a multiprocessor, in which case I do not
			 * understand the sig_lock.  This needs to be fixed.
			 * XXX
			 */
		        if (p->exit_thread) {	/* someone already doing it */
			        thread_block(0);/* give him a chance */
			}
			else {
			        p->exit_thread = current_thread();
				printf(".");
				exit1(p, 1);
			}
			p = allproc.lh_first;
		}
	}
	printf("\n");
	/*
	 *	Forcibly free resources of what's left.
	 */
	p = allproc.lh_first;
	while (p) {
	/*
	 * Close open files and release open-file table.
	 * This may block!
	 */
#ifdef notyet
	/* panics on reboot due to "zfree: non-allocated memory in collectable zone" message */
	fdfree(p);
#endif /* notyet */
	p = p->p_list.le_next;
	}
	/* Wait for the reaper thread to run, and clean up what we have done 
	 * before we proceed with the hardcore shutdown. This reduces the race
	 * between kill_tasks and the reaper thread.
	 */
	/* thread_wakeup(&reaper_queue); */
	/*	IOSleep( 1 * 1000);      */
	printf("continuing\n");
}

/*
 *	Close all file descriptors, called at shutdown time.
 */
fd_shutdown()
{
	register struct file *fp, *nextfp;

	for (fp = filehead.lh_first; fp != 0; fp = nextfp) {
		nextfp = fp->f_list.le_next;
		while (fp->f_count > 1)
			closef(fp);
		closef(fp);
	}
}
