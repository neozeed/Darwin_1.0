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
 * Copyright (c) 1982, 1986, 1989, 1991, 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)init_main.c	8.16 (Berkeley) 5/14/95
 */

/* 
 *
 * Mach Operating System
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/*
 * HISTORY
 * 16-Apr-98  A. Ramesh at Apple
 *	Created for Apple Core from DR2 init_main.c.
 */

#include <quota.h>

#include <sys/param.h>
#include <sys/filedesc.h>
#include <sys/kernel.h>
#include <sys/mount.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/vnode.h>
#include <sys/conf.h>
#include <sys/buf.h>
#include <sys/clist.h>
#include <sys/user.h>
#include <ufs/ufs/quota.h>

#include <sys/malloc.h>
#include <sys/dkstat.h>

#include <machine/spl.h>
#include <kern/thread.h>
#include <kern/task.h>
#include <kern/ast.h>

#include <mach/vm_param.h>

#include <vm/vm_map.h>
#include <vm/vm_kern.h>

#include <sys/ux_exception.h>

#include <sys/reboot.h>
#include <mach/exception_types.h>
#include <dev/busvar.h>

char    copyright[] =
"Copyright (c) 1982, 1986, 1989, 1991, 1993\n\tThe Regents of the University of California.  All rights reserved.\n\n";

extern void	ux_handler();

/* Components of the first process -- never freed. */
struct	proc proc0;
struct	session session0;
struct	pgrp pgrp0;
struct	pcred cred0;
struct	filedesc filedesc0;
struct	plimit limit0;
struct	pstats pstats0;
struct	sigacts sigacts0;
struct	proc *kernproc, *initproc;


long cp_time[CPUSTATES];
long dk_seek[DK_NDRIVE];
long dk_time[DK_NDRIVE];
long dk_wds[DK_NDRIVE];
long dk_wpms[DK_NDRIVE];
long dk_xfer[DK_NDRIVE];
long dk_bps[DK_NDRIVE];

int dk_busy;
int dk_ndrive;

long tk_cancc;
long tk_nin;
long tk_nout;
long tk_rawcc;

/* Global variables to make pstat happy. We do swapping differently */
int nswdev, nswap;
int nswapmap;
void *swapmap;
struct swdevt swdevt[1];

dev_t	rootdev;		/* device of the root */
dev_t	dumpdev;		/* device to take dumps on */
long	dumplo;			/* offset into dumpdev */
long	hostid;
char	hostname[MAXHOSTNAMELEN];
int	hostnamelen;
char	domainname[MAXDOMNAMELEN];
int	domainnamelen;

char rootdevice[16]; 	/* hfs device names have at least 9 chars */
struct	timeval boottime;		/* GRODY!  This has to go... */
#if FIXME  /* [ */
struct	timeval time;
#endif  /* FIXME ] */

#ifdef  KMEMSTATS
struct	kmemstats kmemstats[M_LAST];
#endif

int	lbolt;				/* awoken once a second */
struct	vnode *rootvp;
int boothowto = RB_DEBUG;

vm_map_t	bsd_pageable_map;
vm_map_t	mb_map;

int	cmask = CMASK;

int parse_bsd_args(void);
extern int bsd_hardclockinit;

/*
 * Initialization code.
 * Called from cold start routine as
 * soon as a stack and segmentation
 * have been established.
 * Functions:
 *	clear and free user core
 *	turn on clock
 *	hand craft 0th process
 *	call all initialization routines
 *	fork - process 0 to schedule
 *	     - process 1 execute bootstrap
 *	     - process 2 to page out
 */

/*
 *	Sets the name for the given task.
 */
void proc_name(s, p)
	char		*s;
	struct proc *p;
{
	int		length = strlen(s);

	bcopy(s, p->p_comm,
		length >= sizeof(p->p_comm) ? sizeof(p->p_comm) :
			length + 1);
}


/* To allow these values to be patched, they're globals here */
#include <machine/vmparam.h>
struct rlimit vm_initial_limit_stack = { DFLSSIZ, MAXSSIZ };
struct rlimit vm_initial_limit_data = { DFLDSIZ, MAXDSIZ };
struct rlimit vm_initial_limit_core = { DFLCSIZ, MAXCSIZ };

extern thread_t first_thread;

#define SPL_DEBUG	0
#if	SPL_DEBUG
#define	dprintf(x)	printf x
#else	SPL_DEBUG
#define dprintf(x)
#endif	/* SPL_DEBUG */

void
bsd_init()
{
	register struct proc *p;
	extern struct ucred *rootcred;
	register int i;
	int s;
	thread_t	th;
	extern void	bsdinit_task();
	extern void	netisr_thread();
	void		lightning_bolt(void );
	kern_return_t	ret;
	boolean_t funnel_state;
	extern thread_t	cloneproc();

	extern int (*mountroot) __P((void));

	funnel_state = thread_set_funneled(TRUE);

	printf(copyright);

	kmeminit();
	
	parse_bsd_args();

	bsd_bufferinit();

	/*
	 * Initialize process and pgrp structures.
	 */
	procinit();

	kernproc = &proc0;

	p = kernproc;

	/* kernel_task->proc = kernproc; */
	set_bsdtask_info(kernel_task,(void *)kernproc);
	p->p_pid = 0;

	/* give kernproc a name */
	proc_name("kernel_task", p);

	if (current_task() != kernel_task)
		printf("We are in for problem, current task in not kernel task\n");
	
	/*
	 * Create process 0.
	 */
	LIST_INSERT_HEAD(&allproc, p, p_list);
	p->p_pgrp = &pgrp0;
	LIST_INSERT_HEAD(PGRPHASH(0), &pgrp0, pg_hash);
	LIST_INIT(&pgrp0.pg_members);
	LIST_INSERT_HEAD(&pgrp0.pg_members, p, p_pglist);

	pgrp0.pg_session = &session0;
	session0.s_count = 1;
	session0.s_leader = p;

	p->task = kernel_task;
	
	p->p_stat = SRUN;
	p->p_flag = P_INMEM|P_SYSTEM;
	p->p_nice = NZERO;
	p->p_pptr = p;
	simple_lock_init(&p->siglock);
	p->sigwait = FALSE;
	p->sigwait_thread = THREAD_NULL;
	p->exit_thread = THREAD_NULL;

	/* Create credentials. */
	lockinit(&cred0.pc_lock, PLOCK, "proc0 cred", 0, 0);
	cred0.p_refcnt = 1;
	p->p_cred = &cred0;
	p->p_ucred = crget();
	p->p_ucred->cr_ngroups = 1;	/* group 0 */

	/* Create the file descriptor table. */
	filedesc0.fd_refcnt = 1+1;	/* +1 so shutdown will not _FREE_ZONE */
	p->p_fd = &filedesc0;
	filedesc0.fd_cmask = cmask;

	/* Create the limits structures. */
	p->p_limit = &limit0;
	for (i = 0; i < sizeof(p->p_rlimit)/sizeof(p->p_rlimit[0]); i++)
		limit0.pl_rlimit[i].rlim_cur = 
			limit0.pl_rlimit[i].rlim_max = RLIM_INFINITY;
	limit0.pl_rlimit[RLIMIT_NOFILE].rlim_cur = NOFILE;
	limit0.pl_rlimit[RLIMIT_NPROC].rlim_cur = MAXUPRC;
	limit0.pl_rlimit[RLIMIT_STACK] = vm_initial_limit_stack;
	limit0.pl_rlimit[RLIMIT_DATA] = vm_initial_limit_data;
	limit0.pl_rlimit[RLIMIT_CORE] = vm_initial_limit_core;
	limit0.p_refcnt = 1;

	p->p_stats = &pstats0;
	p->p_sigacts = &sigacts0;

	/*
	 * Charge root for one process.
	 */
	(void)chgproccnt(0, 1);

	
	/*
	 *	Allocate a kernel submap for pageable memory
	 *	for temporary copying (table(), execve()).
	 */
	{
	    vm_offset_t	min, max;

   	 ret = kmem_suballoc(kernel_map,
			&min,
			(vm_size_t)512*1024,
			TRUE,
			TRUE,
			&bsd_pageable_map);
	if (ret != KERN_SUCCESS) 
		panic("Failed to allocare bsd pageable map\n");
	}

	/*
	* Initialize the calendar by
	* reading the BBC, if not already set.
	*/
	IOKitResetTime();

	mapfs_init();

	/* Initialize the file systems. */
	vfsinit();

	/* Initialize mbuf's. */
	mbinit();

	/* Initialize syslog */
	log_init();

	/* Initialize SysV shm */
	shminit();

	/*
	 * Initialize protocols.  Block reception of incoming packets
	 * until everything is ready.
	 */
	s = splimp();
	sysctl_register_fixed(); 
	dlil_init();
	socketinit();
	domaininit();
	splx(s);

	/*
	 *	Create kernel idle cpu processes.  This must be done
 	 *	before a context switch can occur (and hence I/O can
	 *	happen in the binit() call).
	 */
	p->p_fd->fd_cdir = NULL;
	p->p_fd->fd_rdir = NULL;


#ifdef GPROF
	/* Initialize kernel profiling. */
	kmstartup();
#endif

	/* kick off timeout driven events by calling first time */
	lightning_bolt();

	/*
	 * Start up netisr thread now in case we are doing an nfs_mountroot.
	 */
	(void) kernel_thread(kernel_task, netisr_thread);

	bsd_autoconf();

	/*
	 * We attach the loopback interface *way* down here to ensure
	 * it happens after autoconf(), otherwise it becomes the
	 * "primary" interface.
	 */
#include <loop.h>
#if NLOOP > 0
	loopattach();			/* XXX */
#endif


	/* Mount the root file system. */
	while( TRUE) {
	    int err;

            setconf();
	    /* read the time after clock_initialize_calendar()
	     * and before nfs mount */
            microtime(&time);

            if (0 == (err = vfs_mountroot()))
		break;
            printf("cannot mount root, errno = %d\n", err);
	    boothowto |= RB_ASKNAME;
	}

	mountlist.cqh_first->mnt_flag |= MNT_ROOTFS;

	/* Get the vnode for '/'.  Set fdp->fd_fd.fd_cdir to reference it. */
	if (VFS_ROOT(mountlist.cqh_first, &rootvnode))
		panic("cannot find root vnode");
	filedesc0.fd_cdir = rootvnode;
	VREF(rootvnode);
	VOP_UNLOCK(rootvnode, 0, p);
	

	/*
	 * Now can look at time, having had a chance to verify the time
	 * from the file system.  Reset p->p_rtime as it may have been
	 * munched in mi_switch() after the time got set.
	 */
	p->p_stats->p_start = boottime = time;
	p->p_rtime.tv_sec = p->p_rtime.tv_usec = 0;

#ifdef DEVFS
	{
	    extern void devfs_kernel_mount(char * str);
	    
	    devfs_kernel_mount("/dev");
	}
#endif DEVFS
	
	/* Initialize signal state for process 0. */
	siginit(p);

	vnode_pager_bootstrap();
	/* printf("vnode pager  initialized\n"); */

	/* printf("Launching user process\n"); */

	bsd_utaskbootstrap();

	(void) thread_set_funneled(funnel_state);
}

void
bsdinit_task()
{
	struct proc *p = current_proc();
	struct uthread *ut;
	kern_return_t	kr;
	thread_act_t th_act;
	boolean_t funnel_state;

	funnel_state = thread_set_funneled(TRUE);

#if FIXME  /* [ */

	ipc_port_t	master_bootstrap_port;
	task_t		bootstrap_task;
	thread_act_t	bootstrap_thr_act;
	ipc_port_t	root_device_port;

	master_bootstrap_port = ipc_port_alloc_kernel();
	if (master_bootstrap_port == IP_NULL)
		panic("can't allocate master bootstrap port");
	task_set_special_port(bootstrap_task,
			      TASK_BOOTSTRAP_PORT,
			      ipc_port_make_send(master_bootstrap_port));
	printf("setting bootstrap port \n");
	printf("Setting exception port for the init task\n");
	(void) task_set_exception_ports(get_threadtask(th),
					EXC_MASK_ALL &
					~(EXC_MASK_SYSCALL |
			  EXC_MASK_MACH_SYSCALL | EXC_MASK_RPC_ALERT),
					ux_exception_port,
					EXCEPTION_DEFAULT, 0);

#endif /* FIXME  ] */
	proc_name("init", p);

	ux_handler_init();
	/* port_reference(ux_exception_port);*/

	th_act = current_act();
	(void) task_set_exception_ports(get_threadtask(th_act),
					EXC_MASK_ALL &
					~(EXC_MASK_SYSCALL |
			  EXC_MASK_MACH_SYSCALL | EXC_MASK_RPC_ALERT),
					ux_exception_port,
					EXCEPTION_DEFAULT, 0);




	ut = (uthread_t)get_bsdthread_info(th_act);
	ut->uu_ar0 = (void *)get_user_regs(th_act);

	bsd_hardclockinit = 1;	/* Start bsd hardclock */
	load_init_program(p);

	(void) thread_set_funneled(funnel_state);
	
}

void
lightning_bolt()
{			
	thread_wakeup(&lbolt);
	timeout(lightning_bolt,0,hz);
}

bsd_autoconf(){
        extern kern_return_t IOKitBSDInit( void );

	kminit();

	/* 
	 * Early startup for bsd pseudodevices.
	 */
	{
	    struct pseudo_init *pi;
	
	    for (pi = pseudo_inits; pi->ps_func; pi++)
		(*pi->ps_func) (pi->ps_count);
	}

        return( IOKitBSDInit());
}


#include <sys/disklabel.h>  // for MAXPARTITIONS

setconf()
{	
	extern kern_return_t IOFindBSDRoot( char * rootName,
				dev_t * root, u_int32_t * flags );

	extern int 	(*mountroot) __P((void));
	extern int 	nfs_mountroot(); 	/* nfs_vfsops.c */

	u_int32_t	flags;
	kern_return_t	err;

	err = IOFindBSDRoot( rootdevice, &rootdev, &flags );
	if( err) {
		printf("setconf: IOFindBSDRoot returned an error (%d); setting rootdevice to 'sd0a'.\n", err); /* XXX DEBUG TEMP */
		rootdev = makedev( 6, 0 );
		strcpy( rootdevice, "sd0a" );
		flags = 0;
	}

	/* if network device then force nfs root */
	if( flags & 1 ) {
		printf("mounting nfs root\n");
		mountroot = nfs_mountroot;
	} else {
		/* otherwise have vfs determine root filesystem */
		printf("probing for root file system\n");
		mountroot = NULL;
	}

}

bsd_utaskbootstrap()
{
	thread_act_t th_act;

	th_act = cloneproc(kernproc);
	initproc = pfind(1);				
	thread_hold(th_act);
	(void) thread_stop_wait(getshuttle_thread(th_act));
	thread_ast_set(th_act,AST_BSD_INIT);
	thread_release(th_act);
	thread_unstop(getshuttle_thread(th_act));
	(void) thread_resume(th_act);
}

parse_bsd_args()
{
	extern char init_args[];
	char	namep[16];
	extern int boothowto;
	extern int srv;
	extern int ncl;
	extern int bufpages;
	extern int nbuf;

	int len;

	if (PE_parse_boot_arg("-s", namep)) {
		boothowto |= RB_SINGLE;
		len = strlen(init_args);
		if(len != 0)
			strcat(init_args," -s");
		else
			strcat(init_args,"-s");
	}
	if (PE_parse_boot_arg("-b", namep)) {
		boothowto |= RB_NOBOOTRC;
		len = strlen(init_args);
		if(len != 0)
			strcat(init_args," -b");
		else
			strcat(init_args,"-b");
	}

	if (PE_parse_boot_arg("-F", namep)) {
		len = strlen(init_args);
		if(len != 0)
			strcat(init_args," -F");
		else
			strcat(init_args,"-F");
	}

	if (PE_parse_boot_arg("-v", namep)) {
		len = strlen(init_args);
		if(len != 0)
			strcat(init_args," -v");
		else
			strcat(init_args,"-v");
	}

	PE_parse_boot_arg("srv", &srv);
	PE_parse_boot_arg("ncl", &ncl);
	PE_parse_boot_arg("nbuf", &nbuf);
	PE_parse_boot_arg("bufpages", &bufpages);

	return 0;
}
