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
/* Copyright (c) 1995 NeXT Computer, Inc. All Rights Reserved */
/*-
 * Copyright (c) 1982, 1986, 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Mike Karels at Berkeley Software Design, Inc.
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
 *	@(#)kern_sysctl.c	8.4 (Berkeley) 4/14/94
 */

/*
 * sysctl system call.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/file.h>
#include <sys/vnode.h>
#include <sys/unistd.h>
#include <sys/buf.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/disklabel.h>
#include <sys/vm.h>
#include <sys/sysctl.h>
#include <mach/mach_types.h>
#include <mach/vm_param.h>
#include <kern/task.h>
#include <vm/vm_kern.h>

extern vm_map_t bsd_pageable_map;

#include <sys/mount.h>
#import <sys/kdebug.h>

#include <IOKit/IOPlatformExpert.h>

sysctlfn kern_sysctl;
sysctlfn hw_sysctl;
#ifdef DEBUG
sysctlfn debug_sysctl;
#endif
extern sysctlfn vm_sysctl;
extern sysctlfn vfs_sysctl;
extern sysctlfn net_sysctl;
extern sysctlfn cpu_sysctl;


int
userland_sysctl(struct proc *p, int *name, u_int namelen, void *old, size_t 
		*oldlenp, int inkernel, void *new, size_t newlen, size_t *retval);


/*
 * temporary location for vm_sysctl.  This should be machine independant
 */
vm_sysctl(name, namelen, oldp, oldlenp, newp, newlen, p)
	int *name;
	u_int namelen;
	void *oldp;
	size_t *oldlenp;
	void *newp;
	size_t newlen;
	struct proc *p;
{
	int error, level, inthostid;
	extern long avenrun[3], mach_factor[3];
	struct loadavg loadinfo;

	//if (namelen != 1 && !(name[0] == VM_LOADAVG))
		//return (ENOTDIR);		/* overloaded */

	switch (name[0]) {
	case VM_LOADAVG:
		loadinfo.ldavg[0] = avenrun[0];
		loadinfo.ldavg[1] = avenrun[1];
		loadinfo.ldavg[2] = avenrun[2];
		loadinfo.fscale = LSCALE;
		return (sysctl_struct(oldp, oldlenp, newp, newlen, &loadinfo, sizeof(struct loadavg)));
	case VM_MACHFACTOR:
		loadinfo.ldavg[0] = mach_factor[0];
		loadinfo.ldavg[1] = mach_factor[1];
		loadinfo.ldavg[2] = mach_factor[2];
		loadinfo.fscale = LSCALE;
		return (sysctl_struct(oldp, oldlenp, newp, newlen, &loadinfo, sizeof(struct loadavg)));
	case VM_METER:
		return (EOPNOTSUPP);
	case VM_MAXID:
		return (EOPNOTSUPP);
	default:
		return (EOPNOTSUPP);
	}
	/* NOTREACHED */
	return (EOPNOTSUPP);
}

/*
 * Locking and stats
 */
static struct sysctl_lock {
	int	sl_lock;
	int	sl_want;
	int	sl_locked;
} memlock;

struct __sysctl_args {
	int *name;
	u_int namelen;
	void *old;
	size_t *oldlenp;
	void *new;
	size_t newlen;
};
int
__sysctl(p, uap, retval)
	struct proc *p;
	register struct __sysctl_args *uap;
	register_t *retval;
{
	int error, dolock = 1;
	size_t savelen, oldlen = 0;
	sysctlfn *fn;
	int name[CTL_MAXNAME];
	int i;

	if (uap->new != NULL &&
	    (error = suser(p->p_ucred, &p->p_acflag)))
		return (error);
	/*
	 * all top-level sysctl names are non-terminal
	 */
	if (uap->namelen > CTL_MAXNAME || uap->namelen < 2)
		return (EINVAL);
	if (error =
	    copyin(uap->name, &name, uap->namelen * sizeof(int)))
		return (error);

	switch (name[0]) {
	case CTL_KERN:
		fn = kern_sysctl;
		if (name[2] != KERN_VNODE)	/* XXX */
			dolock = 0;
		break;
	case CTL_HW:
		fn = hw_sysctl;
		break;
	case CTL_VM:
		fn = vm_sysctl;
		break;
	case CTL_NET:
		fn = net_sysctl;
		break;
	case CTL_VFS:
		fn = vfs_sysctl;
		break;
#if FIXME  /* [ */
	case CTL_MACHDEP:
		fn = cpu_sysctl;
		break;
#endif  /* FIXME ] */
#ifdef DEBUG
	case CTL_DEBUG:
		fn = debug_sysctl;
		break;
#endif
	default:
		fn = 0;
	}

	if (uap->oldlenp &&
	    (error = copyin(uap->oldlenp, &oldlen, sizeof(oldlen))))
		return (error);

	if (uap->old != NULL) {
		if (!useracc(uap->old, oldlen, B_WRITE))
			return (EFAULT);
		while (memlock.sl_lock) {
			memlock.sl_want = 1;
			sleep((caddr_t)&memlock, PRIBIO+1);
			memlock.sl_locked++;
		}
		memlock.sl_lock = 1;
		if (dolock)
			vslock(uap->old, oldlen);
		savelen = oldlen;
	}

	if (fn)
	    error = (*fn)(name + 1, uap->namelen - 1, uap->old,
			  &oldlen, uap->new, uap->newlen, p);
	else
	    error = 1;

	if (error)
		error = userland_sysctl(p, name, uap->namelen,
					uap->old, uap->oldlenp, 0,
					uap->new, uap->newlen, &oldlen);

	if (uap->old != NULL) {
		if (dolock)
			vsunlock(uap->old, savelen, B_WRITE);
		memlock.sl_lock = 0;
		if (memlock.sl_want) {
			memlock.sl_want = 0;
			wakeup((caddr_t)&memlock);
		}
	}
	if ((error) && (error != ENOMEM))
		return (error);

	if (uap->oldlenp) {
		i = copyout(&oldlen, uap->oldlenp, sizeof(oldlen));
		if (i) 
		    return i;
	}

	return (error);
}

/*
 * Attributes stored in the kernel.
 */
extern char hostname[MAXHOSTNAMELEN]; /* defined in bsd/kern/init_main.c */
extern int hostnamelen;
extern char domainname[MAXHOSTNAMELEN];
extern int domainnamelen;
extern long hostid;
#ifdef INSECURE
int securelevel = -1;
#else
int securelevel;
#endif

int get_kernel_symfile( struct proc *p, char **symfile );

/*
 * kernel related system variables.
 */
kern_sysctl(name, namelen, oldp, oldlenp, newp, newlen, p)
	int *name;
	u_int namelen;
	void *oldp;
	size_t *oldlenp;
	void *newp;
	size_t newlen;
	struct proc *p;
{
	int error, level, inthostid;
	unsigned int oldval=0;
	extern char ostype[], osrelease[], version[];

	/* all sysctl names at this level are terminal */
	if (namelen != 1 && !(name[0] == KERN_PROC || name[0] == KERN_PROF 
		|| name[0] == KERN_KDEBUG
		|| name[0] == KERN_PROCARGS
                || name[0] == KERN_PCSAMPLES
	))
		return (ENOTDIR);		/* overloaded */

	switch (name[0]) {
	case KERN_OSTYPE:
		return (sysctl_rdstring(oldp, oldlenp, newp, ostype));
	case KERN_OSRELEASE:
		return (sysctl_rdstring(oldp, oldlenp, newp, osrelease));
	case KERN_OSREV:
		return (sysctl_rdint(oldp, oldlenp, newp, BSD));
	case KERN_VERSION:
		return (sysctl_rdstring(oldp, oldlenp, newp, version));
	case KERN_MAXVNODES:
		oldval = desiredvnodes;
		error = sysctl_int(oldp, oldlenp, newp, 
				newlen, &desiredvnodes);
		reset_vmobjectcache(oldval, desiredvnodes);
		return(error);
	case KERN_MAXPROC:
		return (sysctl_int(oldp, oldlenp, newp, newlen, &maxproc));
	case KERN_MAXFILES:
		return (sysctl_int(oldp, oldlenp, newp, newlen, &maxfiles));
	case KERN_ARGMAX:
		return (sysctl_rdint(oldp, oldlenp, newp, ARG_MAX));
	case KERN_SECURELVL:
		level = securelevel;
		if ((error = sysctl_int(oldp, oldlenp, newp, newlen, &level)) ||
		    newp == NULL)
			return (error);
		if (level < securelevel && p->p_pid != 1)
			return (EPERM);
		securelevel = level;
		return (0);
	case KERN_HOSTNAME:
		error = sysctl_string(oldp, oldlenp, newp, newlen,
		    hostname, sizeof(hostname));
		if (newp && !error)
			hostnamelen = newlen;
		return (error);
	case KERN_DOMAINNAME:
		error = sysctl_string(oldp, oldlenp, newp, newlen,
		    domainname, sizeof(domainname));
		if (newp && !error)
			domainnamelen = newlen;
		return (error);
	case KERN_HOSTID:
		inthostid = hostid;  /* XXX assumes sizeof long <= sizeof int */
		error =  sysctl_int(oldp, oldlenp, newp, newlen, &inthostid);
		hostid = inthostid;
		return (error);
	case KERN_CLOCKRATE:
		return (sysctl_clockrate(oldp, oldlenp));
#if FIXME  /* [ */
	case KERN_BOOTTIME:
		return (sysctl_rdstruct(oldp, oldlenp, newp, &boottime,
		    sizeof(struct timeval)));
#endif  /* FIXME ] */
	case KERN_VNODE:
		return (sysctl_vnode(oldp, oldlenp));
	case KERN_PROC:
		return (sysctl_doproc(name + 1, namelen - 1, oldp, oldlenp));
	case KERN_FILE:
		return (sysctl_file(oldp, oldlenp));
#ifdef GPROF
	case KERN_PROF:
		return (sysctl_doprof(name + 1, namelen - 1, oldp, oldlenp,
		    newp, newlen));
#endif
	case KERN_POSIX1:
		return (sysctl_rdint(oldp, oldlenp, newp, _POSIX_VERSION));
	case KERN_NGROUPS:
		return (sysctl_rdint(oldp, oldlenp, newp, NGROUPS_MAX));
	case KERN_JOB_CONTROL:
		return (sysctl_rdint(oldp, oldlenp, newp, 1));
	case KERN_SAVED_IDS:
#ifdef _POSIX_SAVED_IDS
		return (sysctl_rdint(oldp, oldlenp, newp, 1));
#else
		return (sysctl_rdint(oldp, oldlenp, newp, 0));
#endif
#if FIXME  /* [ */
	case KERN_MAXPARTITIONS:
		return (sysctl_rdint(oldp, oldlenp, newp, MAXPARTITIONS));
#endif  /* FIXME ] */
	case KERN_KDEBUG:
		return (kdebug_ops(name + 1, namelen - 1, oldp, oldlenp, p));
	case KERN_PCSAMPLES:
	        return (pcsamples_control(name+1, namelen-1, oldp, oldlenp));
	case KERN_PROCARGS:
		/* new one as it does not use kinfo_proc */
		return (sysctl_procargs(name + 1, namelen - 1, oldp, oldlenp));
        case KERN_SYMFILE:
                {
                    char *str;
                    error = get_kernel_symfile( p, &str );
                    if ( error ) return error;
                    return (sysctl_rdstring(oldp, oldlenp, newp, str));
                }    
	default:
		return (EOPNOTSUPP);
	}
	/* NOTREACHED */
}

/*
 * hardware related system variables.
 */
hw_sysctl(name, namelen, oldp, oldlenp, newp, newlen, p)
	int *name;
	u_int namelen;
	void *oldp;
	size_t *oldlenp;
	void *newp;
	size_t newlen;
	struct proc *p;
{
	char dummy[65];
	int  epochTemp;
	extern int vm_page_wire_count;


	/* all sysctl names at this level are terminal */
	if (namelen != 1)
		return (ENOTDIR);		/* overloaded */

	switch (name[0]) {
	case HW_MACHINE:
		if(!PEGetMachineName(dummy,64))
			return(EINVAL);
		return (sysctl_rdstring(oldp, oldlenp, newp, dummy));
	case HW_MODEL:
		if(!PEGetModelName(dummy,64))
			return(EINVAL);
		return (sysctl_rdstring(oldp, oldlenp, newp, dummy));
	case HW_NCPU:
		return (sysctl_rdint(oldp, oldlenp, newp, 1));	/* XXX */
	case HW_BYTEORDER:
		return (sysctl_rdint(oldp, oldlenp, newp, BYTE_ORDER));
	case HW_PHYSMEM:
		return (sysctl_rdint(oldp, oldlenp, newp, mem_size));
	case HW_USERMEM:
		return (sysctl_rdint(oldp, oldlenp, newp,
		    (mem_size - vm_page_wire_count * page_size)));
	case HW_PAGESIZE:
		return (sysctl_rdint(oldp, oldlenp, newp, page_size));
	case HW_EPOCH:
	        epochTemp = PEGetPlatformEpoch();
		if (epochTemp == -1) return(EINVAL);
		return (sysctl_rdint(oldp, oldlenp, newp, epochTemp));
	default:
		return (EOPNOTSUPP);
	}
}

#ifdef DEBUG
/*
 * Debugging related system variables.
 */
#if DIAGNOSTIC
extern
#endif /* DIAGNOSTIC */
struct ctldebug debug0, debug1;
struct ctldebug debug2, debug3, debug4;
struct ctldebug debug5, debug6, debug7, debug8, debug9;
struct ctldebug debug10, debug11, debug12, debug13, debug14;
struct ctldebug debug15, debug16, debug17, debug18, debug19;
static struct ctldebug *debugvars[CTL_DEBUG_MAXID] = {
	&debug0, &debug1, &debug2, &debug3, &debug4,
	&debug5, &debug6, &debug7, &debug8, &debug9,
	&debug10, &debug11, &debug12, &debug13, &debug14,
	&debug15, &debug16, &debug17, &debug18, &debug19,
};
int
debug_sysctl(name, namelen, oldp, oldlenp, newp, newlen, p)
	int *name;
	u_int namelen;
	void *oldp;
	size_t *oldlenp;
	void *newp;
	size_t newlen;
	struct proc *p;
{
	struct ctldebug *cdp;

	/* all sysctl names at this level are name and field */
	if (namelen != 2)
		return (ENOTDIR);		/* overloaded */
	cdp = debugvars[name[0]];
	if (cdp->debugname == 0)
		return (EOPNOTSUPP);
	switch (name[1]) {
	case CTL_DEBUG_NAME:
		return (sysctl_rdstring(oldp, oldlenp, newp, cdp->debugname));
	case CTL_DEBUG_VALUE:
		return (sysctl_int(oldp, oldlenp, newp, newlen, cdp->debugvar));
	default:
		return (EOPNOTSUPP);
	}
	/* NOTREACHED */
}
#endif /* DEBUG */

/*
 * Validate parameters and get old / set new parameters
 * for an integer-valued sysctl function.
 */
sysctl_int(oldp, oldlenp, newp, newlen, valp)
	void *oldp;
	size_t *oldlenp;
	void *newp;
	size_t newlen;
	int *valp;
{
	int error = 0;

	if (oldp && *oldlenp < sizeof(int))
		return (ENOMEM);
	if (newp && newlen != sizeof(int))
		return (EINVAL);
	*oldlenp = sizeof(int);
	if (oldp)
		error = copyout(valp, oldp, sizeof(int));
	if (error == 0 && newp)
		error = copyin(newp, valp, sizeof(int));
	return (error);
}

/*
 * As above, but read-only.
 */
sysctl_rdint(oldp, oldlenp, newp, val)
	void *oldp;
	size_t *oldlenp;
	void *newp;
	int val;
{
	int error = 0;

	if (oldp && *oldlenp < sizeof(int))
		return (ENOMEM);
	if (newp)
		return (EPERM);
	*oldlenp = sizeof(int);
	if (oldp)
		error = copyout((caddr_t)&val, oldp, sizeof(int));
	return (error);
}

/*
 * Validate parameters and get old / set new parameters
 * for a string-valued sysctl function.
 */
sysctl_string(oldp, oldlenp, newp, newlen, str, maxlen)
	void *oldp;
	size_t *oldlenp;
	void *newp;
	size_t newlen;
	char *str;
	int maxlen;
{
	int len, error = 0;

	len = strlen(str) + 1;
	if (oldp && *oldlenp < len)
		return (ENOMEM);
	if (newp && newlen >= maxlen)
		return (EINVAL);
	if (oldp) {
		*oldlenp = len;
		error = copyout(str, oldp, len);
	}
	if (error == 0 && newp) {
		error = copyin(newp, str, newlen);
		str[newlen] = 0;
	}
	return (error);
}

/*
 * As above, but read-only.
 */
sysctl_rdstring(oldp, oldlenp, newp, str)
	void *oldp;
	size_t *oldlenp;
	void *newp;
	char *str;
{
	int len, error = 0;

	len = strlen(str) + 1;
	if (oldp && *oldlenp < len)
		return (ENOMEM);
	if (newp)
		return (EPERM);
	*oldlenp = len;
	if (oldp)
		error = copyout(str, oldp, len);
	return (error);
}

/*
 * Validate parameters and get old / set new parameters
 * for a structure oriented sysctl function.
 */
sysctl_struct(oldp, oldlenp, newp, newlen, sp, len)
	void *oldp;
	size_t *oldlenp;
	void *newp;
	size_t newlen;
	void *sp;
	int len;
{
	int error = 0;

	if (oldp && *oldlenp < len)
		return (ENOMEM);
	if (newp && newlen > len)
		return (EINVAL);
	if (oldp) {
		*oldlenp = len;
		error = copyout(sp, oldp, len);
	}
	if (error == 0 && newp)
		error = copyin(newp, sp, len);
	return (error);
}

/*
 * Validate parameters and get old parameters
 * for a structure oriented sysctl function.
 */
sysctl_rdstruct(oldp, oldlenp, newp, sp, len)
	void *oldp;
	size_t *oldlenp;
	void *newp, *sp;
	int len;
{
	int error = 0;

	if (oldp && *oldlenp < len)
		return (ENOMEM);
	if (newp)
		return (EPERM);
	*oldlenp = len;
	if (oldp)
		error = copyout(sp, oldp, len);
	return (error);
}

/*
 * Get file structures.
 */
sysctl_file(where, sizep)
	char *where;
	size_t *sizep;
{
	int buflen, error;
	struct file *fp;
	char *start = where;

	buflen = *sizep;
	if (where == NULL) {
		/*
		 * overestimate by 10 files
		 */
		*sizep = sizeof(filehead) + (nfiles + 10) * sizeof(struct file);
		return (0);
	}

	/*
	 * first copyout filehead
	 */
	if (buflen < sizeof(filehead)) {
		*sizep = 0;
		return (0);
	}
	if (error = copyout((caddr_t)&filehead, where, sizeof(filehead)))
		return (error);
	buflen -= sizeof(filehead);
	where += sizeof(filehead);

	/*
	 * followed by an array of file structures
	 */
	for (fp = filehead.lh_first; fp != 0; fp = fp->f_list.le_next) {
		if (buflen < sizeof(struct file)) {
			*sizep = where - start;
			return (ENOMEM);
		}
		if (error = copyout((caddr_t)fp, where, sizeof (struct file)))
			return (error);
		buflen -= sizeof(struct file);
		where += sizeof(struct file);
	}
	*sizep = where - start;
	return (0);
}

/*
 * try over estimating by 5 procs
 */
#define KERN_PROCSLOP	(5 * sizeof (struct kinfo_proc))

sysctl_doproc(name, namelen, where, sizep)
	int *name;
	u_int namelen;
	char *where;
	size_t *sizep;
{
	register struct proc *p;
	register struct kinfo_proc *dp = (struct kinfo_proc *)where;
	register int needed = 0;
	int buflen = where != NULL ? *sizep : 0;
	int doingzomb;
	struct kinfo_proc kproc;
	int error = 0;

	if (namelen != 2 && !(namelen == 1 && name[0] == KERN_PROC_ALL))
		return (EINVAL);
	p = allproc.lh_first;
	doingzomb = 0;
again:
	for (; p != 0; p = p->p_list.le_next) {
		/*
		 * Skip embryonic processes.
		 */
		if (p->p_stat == SIDL)
			continue;
		/*
		 * TODO - make more efficient (see notes below).
		 * do by session.
		 */
		switch (name[0]) {

		case KERN_PROC_PID:
			/* could do this with just a lookup */
			if (p->p_pid != (pid_t)name[1])
				continue;
			break;

		case KERN_PROC_PGRP:
			/* could do this by traversing pgrp */
			if (p->p_pgrp->pg_id != (pid_t)name[1])
				continue;
			break;

		case KERN_PROC_TTY:
			if ((p->p_flag & P_CONTROLT) == 0 ||
			    p->p_session->s_ttyp == NULL ||
			    p->p_session->s_ttyp->t_dev != (dev_t)name[1])
				continue;
			break;

		case KERN_PROC_UID:
			if (p->p_ucred->cr_uid != (uid_t)name[1])
				continue;
			break;

		case KERN_PROC_RUID:
			if (p->p_cred->p_ruid != (uid_t)name[1])
				continue;
			break;
		}
		if (buflen >= sizeof(struct kinfo_proc)) {
			fill_proc(p, &kproc);
			if (error = copyout((caddr_t)&kproc, &dp->kp_proc,
			    sizeof(struct kinfo_proc)))
				return (error);
			dp++;
			buflen -= sizeof(struct kinfo_proc);
		}
		needed += sizeof(struct kinfo_proc);
	}
	if (doingzomb == 0) {
		p = zombproc.lh_first;
		doingzomb++;
		goto again;
	}
	if (where != NULL) {
		*sizep = (caddr_t)dp - where;
		if (needed > *sizep)
			return (ENOMEM);
	} else {
		needed += KERN_PROCSLOP;
		*sizep = needed;
	}
	return (0);
}

void
fill_proc(p,kp)
	register struct proc *p;
	register struct kinfo_proc *kp;
{
	fill_externproc(p, &kp->kp_proc);
	fill_eproc(p, &kp->kp_eproc);
}
/*
 * Fill in an eproc structure for the specified process.
 */
void
fill_eproc(p, ep)
	register struct proc *p;
	register struct eproc *ep;
{
	register struct tty *tp;

	ep->e_paddr = p;
	ep->e_sess = p->p_pgrp->pg_session;
	ep->e_pcred = *p->p_cred;
	ep->e_ucred = *p->p_ucred;
	if (p->p_stat == SIDL || p->p_stat == SZOMB) {
		ep->e_vm.vm_rssize = 0;
		ep->e_vm.vm_tsize = 0;
		ep->e_vm.vm_dsize = 0;
		ep->e_vm.vm_ssize = 0;
		/* ep->e_vm.vm_pmap = XXX; */
	} else {
#if FIXME  /* [ */
		register vm_map_t vm = ((task_t)p->task)->map;

		ep->e_vm.vm_rssize = pmap_resident_count(vm->pmap); /*XXX*/
//		ep->e_vm.vm_tsize = vm->vm_tsize;
//		ep->e_vm.vm_dsize = vm->vm_dsize;
//		ep->e_vm.vm_ssize = vm->vm_ssize;
#else  /* FIXME ][ */
		ep->e_vm.vm_rssize = 0; /*XXX*/
#endif  /* FIXME ] */
	}
	if (p->p_pptr)
		ep->e_ppid = p->p_pptr->p_pid;
	else
		ep->e_ppid = 0;
	ep->e_pgid = p->p_pgrp->pg_id;
	ep->e_jobc = p->p_pgrp->pg_jobc;
	if ((p->p_flag & P_CONTROLT) &&
	     (tp = ep->e_sess->s_ttyp)) {
		ep->e_tdev = tp->t_dev;
		ep->e_tpgid = tp->t_pgrp ? tp->t_pgrp->pg_id : NO_PID;
		ep->e_tsess = tp->t_session;
	} else
		ep->e_tdev = NODEV;
	ep->e_flag = ep->e_sess->s_ttyvp ? EPROC_CTTY : 0;
	if (SESS_LEADER(p))
		ep->e_flag |= EPROC_SLEADER;
	if (p->p_wmesg)
		strncpy(ep->e_wmesg, p->p_wmesg, WMESGLEN);
	ep->e_xsize = ep->e_xrssize = 0;
	ep->e_xccount = ep->e_xswrss = 0;
}
/*
 * Fill in an eproc structure for the specified process.
 */
void
fill_externproc(p, exp)
	register struct proc *p;
	register struct extern_proc *exp;
{
	exp->p_forw = exp->p_back = NULL;
	exp->p_vmspace = NULL;
	exp->p_sigacts = p->p_sigacts;
	exp->p_flag  = p->p_flag;
	exp->p_stat  = p->p_stat ;
	exp->p_pid  = p->p_pid ;
	exp->p_oppid  = p->p_oppid ;
	exp->p_dupfd  = p->p_dupfd ;
	/* Mach related  */
	exp->user_stack  = p->user_stack ;
	exp->exit_thread  = p->exit_thread ;
	exp->p_debugger  = p->p_debugger ;
	exp->sigwait  = p->sigwait ;
	/* scheduling */
	exp->p_estcpu  = p->p_estcpu ;
	exp->p_cpticks  = p->p_cpticks ;
	exp->p_pctcpu  = p->p_pctcpu ;
	exp->p_wchan  = p->p_wchan ;
	exp->p_wmesg  = p->p_wmesg ;
	exp->p_swtime  = p->p_swtime ;
	exp->p_slptime  = p->p_slptime ;
	bcopy(&p->p_realtimer, &exp->p_realtimer,sizeof(struct itimerval));
	bcopy(&p->p_rtime, &exp->p_rtime,sizeof(struct timeval));
	exp->p_uticks  = p->p_uticks ;
	exp->p_sticks  = p->p_sticks ;
	exp->p_iticks  = p->p_iticks ;
	exp->p_traceflag  = p->p_traceflag ;
	exp->p_tracep  = p->p_tracep ;
	exp->p_siglist  = p->p_siglist ;
	exp->p_textvp  = p->p_textvp ;
	exp->p_holdcnt = 0 ;
	exp->p_sigmask  = p->p_sigmask ;
	exp->p_sigignore  = p->p_sigignore ;
	exp->p_sigcatch  = p->p_sigcatch ;
	exp->p_priority  = p->p_priority ;
	exp->p_usrpri  = p->p_usrpri ;
	exp->p_nice  = p->p_nice ;
	bcopy(&p->p_comm, &exp->p_comm,MAXCOMLEN);
	exp->p_comm[MAXCOMLEN] = '\0';
	exp->p_pgrp  = p->p_pgrp ;
	exp->p_addr  = NULL;
	exp->p_xstat  = p->p_xstat ;
	exp->p_acflag  = p->p_acflag ;
	exp->p_ru  = p->p_ru ;
}

kdebug_ops(name, namelen, where, sizep, p)
int *name;
u_int namelen;
char *where;
size_t *sizep;
struct proc *p;
{
int size=*sizep;
int ret=0;
extern int kdbg_control(int *name, u_int namelen, char * where,size_t * sizep);

	switch(name[0]) {
	case KERN_KDEFLAGS:
	case KERN_KDDFLAGS:
	case KERN_KDENABLE:
	case KERN_KDGETBUF:
	case KERN_KDSETUP:
	case KERN_KDREMOVE:
	case KERN_KDSETREG:
	case KERN_KDGETREG:
	case KERN_KDREADTR:
	case KERN_KDPIDTR:
	case KERN_KDTHRMAP:
	case KERN_KDPIDEX:
	        ret = kdbg_control(name, namelen, where, sizep);
		break;
	case KERN_KDSETRTCDEC:
	case KERN_KDSETBUF:
	        if (ret = suser(p->p_ucred, &p->p_acflag))
	            return(ret);
	        else
	            ret = kdbg_control(name, namelen, where, sizep);
	        break;
	default:
		ret= EOPNOTSUPP;
		break;
	}
	return(ret);
}

/*
 * try over estimating by 5 procs
 */
#define KERN_PROCSLOP	(5 * sizeof (struct kinfo_proc))

sysctl_procargs(name, namelen, where, sizep)
	int *name;
	u_int namelen;
	char *where;
	size_t *sizep;
{
	register struct proc *p;
	register int needed = 0;
	int buflen = where != NULL ? *sizep : 0;
	int error = 0;
	struct vm_map *proc_map;
	struct task * task;
	vm_map_copy_t	tmp;
	vm_offset_t	arg_addr;
	vm_size_t	arg_size;
	caddr_t data;
	unsigned size;
	vm_offset_t	copy_start, copy_end;
	vm_offset_t	dealloc_start;	/* area to remove from kernel map */
	vm_offset_t	dealloc_end;
	int		*ip;
	kern_return_t ret;
	int pid;


	pid = name[0];

	p = pfind(pid);
	if (p == NULL) {
		return(EINVAL);
	}

	if (p->task == NULL)
		return(EINVAL);
	
	arg_size = buflen;
	/*
	*	Returns the top N bytes of the user stack, with
	*	everything below the first argument character
	*	zeroed for security reasons.
	*	Odd data structure is for compatibility.
	*/
	/*
	 *	Lookup process by pid
	 */
	/*
	*	Get map for process
	*/
	proc_map = get_task_map((task_t)p->task);

	/*
	 *	Copy the top N bytes of the stack.
	 *	On all machines we have so far, the stack grows
	 *	downwards.
	 *
	 *	If the user expects no more than N bytes of
	 *	argument list, use that as a guess for the
	 *	size.
	 */

	if (buflen == 0) {
		return(EINVAL);
	}

	if (!p->user_stack)
		return(EINVAL);

#if	STACK_GROWTH_UP
	arg_addr = p->user_stack;
#else	STACK_GROWTH_UP
	arg_addr = p->user_stack - arg_size;
#endif	/* STACK_GROWTH_UP */

	/*
	 *	Before we can block (any VM code), make another
	 *	reference to the map to keep it alive.
	 */

	ret = kmem_alloc_pageable(bsd_pageable_map, &copy_start,
					round_page(arg_size));
	if (ret != KERN_SUCCESS) 
		return(ENOMEM);

	copy_end = round_page(copy_start + arg_size);

	/* vm_map_reference(proc_map); */

	if( vm_map_copyin(proc_map, trunc_page(arg_addr), round_page(arg_size), 
			FALSE, &tmp) != KERN_SUCCESS) {
			kmem_free(bsd_pageable_map, copy_start,
					round_page(arg_size));
			/* vm_map_deallocate(proc_map); */
			return (EIO);
	}
	if( vm_map_copy_overwrite(bsd_pageable_map, copy_start, 
		tmp, FALSE) != KERN_SUCCESS) {
			kmem_free(bsd_pageable_map, copy_start,
					round_page(arg_size));
			/* vm_map_deallocate(proc_map); */
			return (EIO);
	}

	/*
	*	Now that we've done the copy, we can release
	*	the process' map.
	*/
	/* vm_map_deallocate(proc_map); */
			
#if	STACK_GROWTH_UP
	data = (caddr_t)copy_start;
	ip = (int *) ((*(int *)copy_start) - arg_addr + data);
	/*
	 * sanity check ip since it comes from user-accessible
	 * stack area
	 */
	if (((vm_offset_t)ip > copy_end) ||
			((vm_offset_t)ip < copy_start))
				ip = (int *)copy_end;
	/*
	 * relocate so that end of string area is at end
	 * of buffer.
	 */
	size = (unsigned) ((int)ip - (int)copy_start);
	data = (caddr_t)(copy_end - size);
	bcopy(copy_start, data, size);
	/*
	 * now find beginning of string area so we can
	 * clear out data user should not see
	 */
	ip = (int *)copy_end;	// start at new end
	ip -= 2; /*skip trailing 0 word and assume at least one
		  argument.  The last word of argN may be just
		  the trailing 0, in which case we'd stop
		  there */
	while (*--ip)
		if (ip == (int *)data)
				break;			
	bzero(copy_start,
		(unsigned) ((int)ip - (int)copy_start));
	/*
	 * now prepare data/size for the copy's out of the
	 * switch.  We copy the last arg_size bytes from
	 * our data.
	 */
	size = arg_size;
	data = (caddr_t)(copy_end - size);
#else	STACK_GROWTH_UP
	data = (caddr_t) (copy_end - arg_size);
	ip = (int *) copy_end;		
	size = arg_size;

	/*
	 *	Now look down the stack for the bottom of the
	 *	argument list.  Since this call is otherwise
	 *	unprotected, we can't let the nosy user see
	 *	anything else on the stack.
	 *
	 *	The arguments are pushed on the stack by
	 *	execve() as:
	 *
	 *		.long	0
	 *		arg 0	(null-terminated)
	 *		arg 1
	 *		...
	 *		arg N
	 *		.long	0
	 *
	 */

	ip -= 2; /*skip trailing 0 word and assume at least one
		  argument.  The last word of argN may be just
		  the trailing 0, in which case we'd stop
		  there */
	while (*--ip)
		if (ip == (int *)data)
			break;			
	bzero(data, (unsigned) ((int)ip - (int)data));
#endif	/* STACK_GROWTH_UP */

	dealloc_start = copy_start;
	dealloc_end = copy_end;


	size = MIN(size, buflen);
	error = copyout(data, where, size);

	if (dealloc_start != (vm_offset_t) 0) {
		kmem_free(bsd_pageable_map, dealloc_start,
			dealloc_end - dealloc_start);
	}
	if (error) {
		return(error);
	}

	if (where != NULL)
		*sizep = size;
	return (0);
}
