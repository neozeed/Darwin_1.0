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
/*
 * Copyright (c) 1982, 1986, 1989, 1993
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
 *	@(#)sys_generic.c	8.9 (Berkeley) 2/14/95
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/filedesc.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <sys/proc.h>
#include <sys/socketvar.h>
#include <sys/uio.h>
#include <sys/kernel.h>
#include <sys/stat.h>
#include <sys/malloc.h>

#if KTRACE
#include <sys/ktrace.h>
#endif

#include <sys/mount.h>
#include <sys/protosw.h>
#include <sys/ev.h>
#include <sys/user.h>
#include <sys/kdebug.h>
#include <kern/assert.h>
#include <kern/thread_act.h>

#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/errno.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet/ip_var.h>
#include <netinet/tcp.h>
#include <netinet/tcp_fsm.h>
#include <netinet/tcp_seq.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <netinet/tcpip.h>
#include <netinet/tcp_debug.h>

/*
 * Read system call.
 */
struct read_args {
	int fd;
	char *cbuf;
	u_int nbyte;
};
/* ARGSUSED */
read(p, uap, retval)
	struct proc *p;
	register struct read_args *uap;
	register_t *retval;
{
	struct uio auio;
	struct iovec aiov;

	aiov.iov_base = (caddr_t)uap->cbuf;
	aiov.iov_len = uap->nbyte;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_rw = UIO_READ;
	return (rwuio(p, uap->fd, &auio, UIO_READ, retval));	
}

struct readv_args {
	int fd;
	struct iovec *iovp;
	u_int iovcnt;
};
readv(p, uap, retval)
	struct proc *p;
	register struct readv_args *uap;
	int *retval;
{
	struct uio auio;
	register struct iovec *iov;
	int error;
	struct iovec aiov[UIO_SMALLIOV];

	if (uap->iovcnt > UIO_SMALLIOV) {
		if (uap->iovcnt > UIO_MAXIOV)
			return (EINVAL);	
		if ((iov = (struct iovec *)
			    kalloc(sizeof(struct iovec) * (uap->iovcnt))) == 0)
			return (ENOMEM);
	} else
		iov = aiov;
	auio.uio_iov = iov;
	auio.uio_iovcnt = uap->iovcnt;
	auio.uio_rw = UIO_READ;
	error = copyin((caddr_t)uap->iovp, (caddr_t)iov,
	    uap->iovcnt * sizeof (struct iovec));
	if (!error)
		error = rwuio(p, uap->fd, &auio, UIO_READ, retval);
	if (uap->iovcnt > UIO_SMALLIOV)
		kfree(iov, sizeof(struct iovec)*uap->iovcnt);
	return (error);
}

/*
 * Write system call
 */
struct write_args {
	int fd;
	char *cbuf;
	u_int nbyte;
};
write(p, uap, retval)
	struct proc *p;
	register struct write_args *uap;
	int *retval;
{
	struct uio auio;
	struct iovec aiov;

	aiov.iov_base = uap->cbuf;
	aiov.iov_len = uap->nbyte;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_rw = UIO_WRITE;
	return (rwuio(p, uap->fd, &auio, UIO_WRITE, retval));
}

struct writev_args {
	int fd;
	struct iovec *iovp;
	u_int iovcnt;
};
writev(p, uap, retval)
	struct proc *p;
	register struct writev_args *uap;
	int *retval;
{
	struct uio auio;
	register struct iovec *iov;
	int error;
	struct iovec aiov[UIO_SMALLIOV];

	if (uap->iovcnt > UIO_SMALLIOV) {
		if (uap->iovcnt > UIO_MAXIOV)
			return (EINVAL);	
		if ((iov = (struct iovec *)
			    kalloc(sizeof(struct iovec) * (uap->iovcnt))) == 0)
			return (ENOMEM);
	} else
		iov = aiov;
	auio.uio_iov = iov;
	auio.uio_iovcnt = uap->iovcnt;
	auio.uio_rw = UIO_WRITE;
	error = copyin((caddr_t)uap->iovp, (caddr_t)iov,
	    uap->iovcnt * sizeof (struct iovec));
	if (!error)
		error = rwuio(p, uap->fd, &auio, UIO_WRITE, retval);
	if (uap->iovcnt > UIO_SMALLIOV)
		kfree(iov, sizeof(struct iovec)*uap->iovcnt);
	return (error);
}

rwuio(p, fdes, uio, rw, retval)
	struct proc *p;
	int fdes;
	register struct uio *uio;
	enum uio_rw rw;
	int *retval;
{
	register struct file *fp;
	register struct iovec *iov;
	int i, count, flag, error;

	if (error = fdgetf(p, fdes, &fp))
		return (error);

	if ((fp->f_flag&(rw==UIO_READ ? FREAD : FWRITE)) == 0) {
		return(EBADF);
	}
	uio->uio_resid = 0;
	uio->uio_segflg = UIO_USERSPACE;
	uio->uio_procp = p;
	iov = uio->uio_iov;
	for (i = 0; i < uio->uio_iovcnt; i++) {
		if (iov->iov_len < 0) {
			return(EINVAL);
		}
		uio->uio_resid += iov->iov_len;
		if (uio->uio_resid < 0) {
			return(EINVAL);
		}
		iov++;
	}
	count = uio->uio_resid;
	if (rw == UIO_READ) {
		if (error = (*fp->f_ops->fo_read)(fp, uio, fp->f_cred))
				if (uio->uio_resid != count && (error == ERESTART ||
		    			error == EINTR || error == EWOULDBLOCK))
						error = 0;
	} else {
		if (error = (*fp->f_ops->fo_write)(fp, uio, fp->f_cred)) {
			if (uio->uio_resid != count && (error == ERESTART ||
		    	error == EINTR || error == EWOULDBLOCK))
				error = 0;
			if (error == EPIPE)
				psignal(p, SIGPIPE);
		}
	}
	*retval = count - uio->uio_resid;
	return(error);
}

/*
 * Ioctl system call
 */
struct ioctl_args {
	int fd;
	u_long com;
	caddr_t data;
};
/* ARGSUSED */
ioctl(p, uap, retval)
	struct proc *p;
	register struct ioctl_args *uap;
	register_t *retval;
{
	register struct file *fp;
	register u_long com;
	register int error;
	register u_int size;
	caddr_t data, memp;
	int tmp;
#define STK_PARAMS	128
	char stkbuf[STK_PARAMS];

	if (error = fdgetf(p, uap->fd, &fp))
		return (error);

	if ((fp->f_flag & (FREAD | FWRITE)) == 0)
		return (EBADF);
		
	/*### LD 6/11/97 Hack Alert: this is to get AppleTalk to work
	 * while implementing an ATioctl system call
	 */
#if NETAT
	{
		extern int appletalk_inited;

		if (appletalk_inited && ((uap->com & 0x0000FFFF) == 0xff99)) {
#ifdef APPLETALK_DEBUG
			kprintf("ioctl: special AppleTalk \n");
#endif
			error = (*fp->f_ops->fo_ioctl)(fp, uap->com, uap->data, p);
			return(error);
		}
	}

#endif /* NETAT */


	switch (com = uap->com) {
	case FIONCLEX:
		*fdflags(p, uap->fd) &= ~UF_EXCLOSE;
		return (0);
	case FIOCLEX:
		*fdflags(p, uap->fd) |= UF_EXCLOSE;
		return (0);
	}

	/*
	 * Interpret high order word to find amount of data to be
	 * copied to/from the user's address space.
	 */
	size = IOCPARM_LEN(com);
	if (size > IOCPARM_MAX)
		return (ENOTTY);
	memp = NULL;
	if (size > sizeof (stkbuf)) {
		if ((memp = (caddr_t)kalloc(size)) == 0)
			return(ENOMEM);
		data = memp;
	} else
		data = stkbuf;
	if (com&IOC_IN) {
		if (size) {
			error = copyin(uap->data, data, (u_int)size);
			if (error) {
				if (memp)
					kfree(memp, size);
				return (error);
			}
		} else
			*(caddr_t *)data = uap->data;
	} else if ((com&IOC_OUT) && size)
		/*
		 * Zero the buffer so the user always
		 * gets back something deterministic.
		 */
		bzero(data, size);
	else if (com&IOC_VOID)
		*(caddr_t *)data = uap->data;

	switch (com) {

	case FIONBIO:
		if (tmp = *(int *)data)
			fp->f_flag |= FNONBLOCK;
		else
			fp->f_flag &= ~FNONBLOCK;
		error = (*fp->f_ops->fo_ioctl)(fp, FIONBIO, (caddr_t)&tmp, p);
		break;

	case FIOASYNC:
		if (tmp = *(int *)data)
			fp->f_flag |= FASYNC;
		else
			fp->f_flag &= ~FASYNC;
		error = (*fp->f_ops->fo_ioctl)(fp, FIOASYNC, (caddr_t)&tmp, p);
		break;

	case FIOSETOWN:
		tmp = *(int *)data;
		if (fp->f_type == DTYPE_SOCKET) {
			((struct socket *)fp->f_data)->so_pgid = tmp;
			error = 0;
			break;
		}
		if (tmp <= 0) {
			tmp = -tmp;
		} else {
			struct proc *p1 = pfind(tmp);
			if (p1 == 0) {
				error = ESRCH;
				break;
			}
			tmp = p1->p_pgrp->pg_id;
		}
		error = (*fp->f_ops->fo_ioctl)
			(fp, (int)TIOCSPGRP, (caddr_t)&tmp, p);
		break;

	case FIOGETOWN:
		if (fp->f_type == DTYPE_SOCKET) {
			error = 0;
			*(int *)data = ((struct socket *)fp->f_data)->so_pgid;
			break;
		}
		error = (*fp->f_ops->fo_ioctl)(fp, TIOCGPGRP, data, p);
		*(int *)data = -*(int *)data;
		break;

	default:
		error = (*fp->f_ops->fo_ioctl)(fp, com, data, p);
		/*
		 * Copy any data to user, size was
		 * already set and checked above.
		 */
		if (error == 0 && (com&IOC_OUT) && size)
			error = copyout(data, uap->data, (u_int)size);
		break;
	}
	if (memp)
		kfree(memp, size);
	return (error);
}


int	selwait, nselcoll;

/*
 * Select system call.
 */
struct select_args {
	u_int nd;
	fd_set *in;
	fd_set *ou;
	fd_set *ex;
	struct timeval *tv;
};

int selcontinue(int error);

select(p, uap, retval)
	register struct proc *p;
	register struct select_args *uap;
	register_t *retval;
{
	int s, ncoll, error = 0, timo;
	u_int ni;
	struct thread *th;
	thread_act_t th_act;
	struct uthread	*uth;
	struct _select *sel;

	th = current_thread();
	th_act = current_act();
	uth = get_bsdthread_info(th_act);
	sel = &uth->uu_state.ss_select;

	bzero((caddr_t)&sel->ibits[0], sizeof(sel->ibits));
	bzero((caddr_t)&sel->obits[0], sizeof(sel->obits));

	if (uap->nd > FD_SETSIZE)
		return (EINVAL);
	if (uap->nd > p->p_fd->fd_nfiles) {
		/* forgiving; slightly wrong */
		uap->nd = p->p_fd->fd_nfiles;
	}
	ni = howmany(uap->nd, NFDBITS) * sizeof(fd_mask);

#define	getbits(name, x) \
	if (uap->name && (error = copyin((caddr_t)uap->name, \
	    (caddr_t)&sel->ibits[x], ni))) \
		goto contin;
	getbits(in, 0);
	getbits(ou, 1);
	getbits(ex, 2);
#undef	getbits

	if (uap->tv) {
		error = copyin((caddr_t)uap->tv, (caddr_t)&sel->atv,
			sizeof (sel->atv));
		if (error)
			goto contin;
		if (itimerfix(&sel->atv)) {
			error = EINVAL;
			goto contin;
		}
		s = splhigh();
		timeradd(&sel->atv, &time, &sel->atv);
		timo = hzto(&sel->atv);
		splx(s);
	} else
		timo = 0;
	sel->poll = timo;
contin:
	selcontinue(error);
}

int
selcontinue(error)
{
	int s, ncoll, timo;
	u_int ni;
	struct thread *th;
	thread_act_t th_act;
	struct uthread	*uth;
	struct proc *p;
	struct select_args *uap;
	int *retval;
	struct _select *sel;

	th = (struct thread *)current_thread();
	th_act = current_act();
	uth = get_bsdthread_info(th_act);
	p = (struct proc *)get_bsdtask_info(current_task());
	uap = (struct select_args *)get_bsduthreadarg(th_act);
	retval = (int *)get_bsduthreadrval(th_act);
	sel = &uth->uu_state.ss_select;

retry:
	if (error != 0)
	  goto done;
	ncoll = nselcoll;
	p->p_flag |= P_SELECT;
	error = selscan(p, &sel->ibits[0], &sel->obits[0], uap->nd, retval);
	if (error || *retval)
		goto done;
	s = splhigh();
	/* this should be timercmp(&time, &atv, >=) */
	if (uap->tv && (time.tv_sec > sel->atv.tv_sec ||
	    time.tv_sec == sel->atv.tv_sec && time.tv_usec >= sel->atv.tv_usec)) {
		splx(s);
		goto done;
	}
	/*
	 * To effect a poll, the timeout argument should be
	 * non-nil, pointing to a zero-valued timeval structure.
	 */
	timo = sel->poll;

	if (uap->tv && (timo == 0)) {
		splx(s);
		goto done;
	}
	if ((p->p_flag & P_SELECT) == 0 || nselcoll != ncoll) {
		splx(s);
		goto retry;
	}
	p->p_flag &= ~P_SELECT;

#if 1
        error = tsleep0((caddr_t)&selwait, PSOCK | PCATCH, "select", timo, selcontinue);
        /* NOTREACHED */
#else
        error = tsleep((caddr_t)&selwait, PSOCK | PCATCH, "select", timo);
#endif
	splx(s);
	if (error == 0)
		goto retry;
done:
	p->p_flag &= ~P_SELECT;
	/* select is not restarted after signals... */
	if (error == ERESTART)
		error = EINTR;
	if (error == EWOULDBLOCK)
		error = 0;
	ni = howmany(uap->nd, NFDBITS) * sizeof(fd_mask);
#define	putbits(name, x) \
	if (uap->name && (error2 = copyout((caddr_t)&sel->obits[x], \
	    (caddr_t)uap->name, ni))) \
		error = error2;
	if (error == 0) {
		int error2;

		putbits(in, 0);
		putbits(ou, 1);
		putbits(ex, 2);
#undef putbits
	}

        unix_syscall_return(error);
}

selscan(p, ibits, obits, nfd, retval)
	struct proc *p;
	fd_set *ibits, *obits;
	int nfd;
	register_t *retval;
{
	register struct filedesc *fdp = p->p_fd;
	register int msk, i, j, fd;
	register fd_mask bits;
	struct file *fp;
	int n = 0;
	static int flag[3] = { FREAD, FWRITE, 0 };

	/* Problems when reboot; due to MacOSX signal probs
	 * in Beaker1C ; verify that the p->p_fd is valid
	 */
	if (fdp == NULL) {
		*retval=0;
		return(EIO);
	}

	for (msk = 0; msk < 3; msk++) {
		for (i = 0; i < nfd; i += NFDBITS) {
			bits = ibits[msk].fds_bits[i/NFDBITS];
			while ((j = ffs(bits)) && (fd = i + --j) < nfd) {
				bits &= ~(1 << j);
				fp = fdp->fd_ofiles[fd];
				if (fp == NULL || (fdp->fd_ofileflags[fd] &
								UF_RESERVED))
					return (EBADF);
				if (fp->f_ops && (*fp->f_ops->fo_select)(fp, flag[msk], p)) {
					FD_SET(fd, &obits[msk]);
					n++;
				}
			}
		}
	}
	*retval = n;
	return (0);
}

/*ARGSUSED*/
seltrue(dev, flag, p)
	dev_t dev;
	int flag;
	struct proc *p;
{

	return (1);
}

/*
 * Record a select request.
 */
void
selrecord(selector, sip)
	struct proc *selector;
	struct selinfo *sip;
{
	int 		oldpri = splhigh();
	thread_t	my_thread = current_thread();
	thread_t	selthread;

	selthread = sip->si_thread;
	
	if (selthread == my_thread) {
		splx(oldpri);
		return;
	}
	
	if (selthread && is_thread_active(selthread) &&
	    get_thread_waitevent(selthread) == (caddr_t)&selwait) {
			sip->si_flags |= SI_COLL;
			splx(oldpri);
	}
	else {
			sip->si_thread = my_thread;
			splx(oldpri);
			if (selthread) {
				/* thread_deallocate(selthread); */
				act_deallocate(getact_thread(selthread));
			}
			/* do I need act reference ??? */
			/* thread_reference(sip->si_thread); */
			act_reference(getact_thread(sip->si_thread));
	}

	return;
}

void
selwakeup(sip)
	register struct selinfo *sip;
{
	register thread_t 	the_thread = (thread_t)sip->si_thread;
	int oldpri;
	struct proc *p;
	thread_act_t th_act;
	
	if (the_thread == 0)
		return;

	if (sip->si_flags & SI_COLL) {
		nselcoll++;
		sip->si_flags &= ~SI_COLL;
		wakeup((caddr_t)&selwait);
	}
	
	oldpri = splhigh();

	th_act = getact_thread(the_thread);

	if (is_thread_active(the_thread)) {
		/* protect p_flag minimally at splsched() */
 		if (get_thread_waitevent(the_thread) == &selwait)
			clear_wait(the_thread, THREAD_AWAKENED, TRUE);
		if (p=(struct proc *)(get_bsdtask_info(get_threadtask(th_act))))
			p->p_flag &= ~P_SELECT;
	}
	
	th_act = getact_thread(the_thread);

	act_deallocate(th_act);
	
	sip->si_thread = 0;
	
	splx(oldpri);
	
}

void selthreadclear(sip)
	register struct selinfo *sip;
{
	thread_act_t th_act;

	if (sip->si_thread) {
			th_act = getact_thread(sip->si_thread);
			act_deallocate(th_act);
	}

}
/*
 * called upon socket close. deque and free all events for
 * the socket
 */
evsofree(struct socket *sp)
{
  struct eventqelt *eqp, *next;

  if (sp == NULL) return;

  for (eqp = sp->so_evlist.tqh_first; eqp != NULL; eqp = next) {
    next = eqp->ee_slist.tqe_next;
    evprocdeque(eqp->ee_proc, eqp); // remove from proc q if there
    TAILQ_REMOVE(&sp->so_evlist, eqp, ee_slist); // remove from socket q
    FREE(eqp, M_TEMP);
  }
}


#define DBG_EVENT 0x10

#define DBG_POST 0x10
#define DBG_WATCH 0x11
#define DBG_WAIT 0x12
#define DBG_MOD 0x13
#define DBG_EWAKEUP 0x14

#define DBG_MISC_POST MISCDBG_CODE(DBG_EVENT,DBG_POST)
#define DBG_MISC_WATCH MISCDBG_CODE(DBG_EVENT,DBG_WATCH)
#define DBG_MISC_WAIT MISCDBG_CODE(DBG_EVENT,DBG_WAIT)
#define DBG_MISC_MOD MISCDBG_CODE(DBG_EVENT,DBG_MOD)
#define DBG_MISC_EWAKEUP MISCDBG_CODE(DBG_EVENT,DBG_EWAKEUP)



/*
 * enque this event if it's not already queued. wakeup
   the proc if we do queue this event to it.
 */
evprocenque(struct eventqelt *eqp)
{
  struct proc *p;

  assert(eqp);
  if (eqp->ee_flags & EV_QUEUED) {
    return;
  }
  eqp->ee_flags |= EV_QUEUED;
  eqp->ee_eventmask = 0;  // disarm
  p = eqp->ee_proc;
  TAILQ_INSERT_TAIL(&p->p_evlist, eqp, ee_plist);
  KERNEL_DEBUG(DBG_MISC_EWAKEUP,0,0,0,eqp,0);
  wakeup(&p->p_evlist);
}

/*
 * given either a sockbuf or a socket run down the
 * event list and queue ready events found
 */
postevent(struct socket *sp, struct sockbuf *sb, int event)
{
  int mask;
  struct eventqelt *evq;
  register struct tcpcb *tp;

  if (sb) sp = sb->sb_so;
  if (!sp || sp->so_evlist.tqh_first == NULL) return;

  KERNEL_DEBUG(DBG_MISC_POST|DBG_FUNC_START, 0,0,0,0,0);

  for (evq = sp->so_evlist.tqh_first;
       evq != NULL; evq = evq->ee_slist.tqe_next) {

    mask = 0;

    /* ready for reading:
       - byte cnt >= receive low water mark
       - read-half of conn closed
       - conn pending for listening sock
       - socket error pending

       ready for writing
       - byte cnt avail >= send low water mark
       - write half of conn closed
       - socket error pending
       - non-blocking conn completed successfully

       exception pending
       - out of band data
       - sock at out of band mark

    */
    switch (event & EV_DMASK) {

    case EV_RWBYTES:
    case EV_OOB:
    case EV_RWBYTES|EV_OOB:
      if (event & EV_OOB) {
      if ((evq->ee_eventmask & EV_EX)) {
	if (sp->so_oobmark || ((sp->so_state & SS_RCVATMARK))) {
	  mask |= EV_EX|EV_OOB;
	}
      }
      }
      if (event & EV_RWBYTES) {
      if ((evq->ee_eventmask & EV_RE) && soreadable(sp)) {
	if ((sp->so_type == SOCK_STREAM) && (sp->so_error == ECONNREFUSED) ||
	    (sp->so_error == ECONNRESET)) {
	  if ((sp->so_pcb == 0) ||
	      !(tp = sototcpcb(sp)) ||
	      (tp->t_state == TCPS_CLOSED)) {
	    mask |= EV_RE|EV_RESET;
	    break;
	  }
	}
	if (sp->so_state & SS_CANTRCVMORE) {
	  mask |= EV_RE|EV_FIN;
	  evq->ee_req.er_rcnt = sp->so_rcv.sb_cc;
	  break;
	}
	mask |= EV_RE;
	evq->ee_req.er_rcnt = sp->so_rcv.sb_cc;
      }

      if ((evq->ee_eventmask & EV_WR) && sowriteable(sp)) {
	if ((sp->so_type == SOCK_STREAM) &&(sp->so_error == ECONNREFUSED) ||
	    (sp->so_error == ECONNRESET)) {
	  if ((sp->so_pcb == 0) ||
	      !(tp = sototcpcb(sp)) ||
	      (tp->t_state == TCPS_CLOSED)) {
	  mask |= EV_WR|EV_RESET;
	  break;
	  }
	}
	mask |= EV_WR;
	evq->ee_req.er_wcnt = sbspace(&sp->so_snd);
      }
      }
    break;

    case EV_RCONN:
      if ((evq->ee_eventmask & EV_RE)) {
	evq->ee_req.er_rcnt = sp->so_qlen + 1;  // incl this one
	mask |= EV_RE|EV_RCONN;
      }
      break;

    case EV_WCONN:
      if ((evq->ee_eventmask & EV_WR)) {
	mask |= EV_WR|EV_WCONN;
      }
      break;

    case EV_RCLOSED:
      if ((evq->ee_eventmask & EV_RE)) {
	mask |= EV_RE|EV_RCLOSED;
      }
      break;

    case EV_WCLOSED:
      if ((evq->ee_eventmask & EV_WR)) {
	mask |= EV_WR|EV_WCLOSED;
      }
      break;

    case EV_FIN:
      if (evq->ee_eventmask & EV_RE) {
	mask |= EV_RE|EV_FIN;
      }
      break;

    case EV_RESET:
    case EV_TIMEOUT:
      if (evq->ee_eventmask & EV_RE) {
	mask |= EV_RE | event;
      } 
      if (evq->ee_eventmask & EV_WR) {
	mask |= EV_WR | event;
      }
      break;

    default:
      return;
    } /* switch */

    if (mask) {
      evq->ee_req.er_eventbits |= mask;
      evprocenque(evq);
    }
  }
  KERNEL_DEBUG(DBG_MISC_POST|DBG_FUNC_END, 0,0,0,0,0);
}

/*
 * remove and return the first event (eqp=NULL) or a specific
 * event, or return NULL if no events found
 */
struct eventqelt *
evprocdeque(struct proc *p, struct eventqelt *eqp)
{

  
  if (eqp && ((eqp->ee_flags & EV_QUEUED) == NULL))
    return(NULL);
  if (p->p_evlist.tqh_first == NULL)
    return(NULL);
  if (eqp == NULL) {  // remove first
    eqp = p->p_evlist.tqh_first;
  }
  TAILQ_REMOVE(&p->p_evlist, eqp, ee_plist);
  eqp->ee_flags &= ~EV_QUEUED;
  return(eqp);
}

struct evwatch_args {
  struct eventreq  *u_req;
  int               u_eventmask;
};


/*
 * watchevent system call. user passes us an event to watch
 * for. we malloc an event object, initialize it, and queue
 * it to the open socket. when the event occurs, postevent()
 * will enque it back to our proc where we can retrieve it
 * via waitevent().
 *
 * should this prevent duplicate events on same socket?
 */
int
watchevent(p, uap, retval)
     struct proc *p;
     struct evwatch_args *uap;
     register_t *retval;
{
  struct eventqelt *eqp = (struct eventqelt *)0;
  struct eventqelt *np;
  struct eventreq *erp;
  struct file *fp;
  struct socket *sp;
  int error;

  // get a qelt and fill with users req
  MALLOC(eqp, struct eventqelt *, sizeof(struct eventqelt), M_TEMP, M_WAITOK);
  if (!eqp) panic("can't MALLOC eqp");
  erp = &eqp->ee_req;
  // get users request pkt
  if (error = copyin((caddr_t)uap->u_req, (caddr_t)erp,
		     sizeof(struct eventreq))) {
    FREE(eqp, M_TEMP);
    KERNEL_DEBUG(DBG_MISC_WATCH|DBG_FUNC_END, 0,0,0,0,0);
    return(error);
  }
  KERNEL_DEBUG(DBG_MISC_WATCH|DBG_FUNC_START, 0,
	       erp->er_handle,uap->u_eventmask,eqp,0);
  // validate, freeing qelt if errors
  error = 0;
  if (erp->er_type != EV_FD) {
    error = EINVAL;
  } else  if (erp->er_handle < 0) {
    error = EBADF;
  } else  if (erp->er_handle > p->p_fd->fd_nfiles) {
    error = EBADF;
  } else if ((fp = *fdfile(p, erp->er_handle)) == NULL) {
    error = EBADF;
  } else if (fp->f_type != DTYPE_SOCKET) {
    error = EINVAL;
  }
  if (error) {
    FREE(eqp,M_TEMP);
    KERNEL_DEBUG(DBG_MISC_WATCH|DBG_FUNC_END, 0,0,0,0,0);
    return(error);
  }

  erp->er_rcnt = erp->er_wcnt = erp->er_eventbits = 0;
  eqp->ee_proc = p;
  eqp->ee_eventmask = uap->u_eventmask & EV_MASK;
  eqp->ee_flags = 0;

  sp = (struct socket *)fp->f_data;
  assert(sp != NULL);

  // only allow one watch per file per proc
  for (np = sp->so_evlist.tqh_first; np != NULL; np = np->ee_slist.tqe_next) {
    if (np->ee_proc == p) {
      FREE(eqp,M_TEMP);
      KERNEL_DEBUG(DBG_MISC_WATCH|DBG_FUNC_END, 0,0,0,0,0);
      return(EINVAL);
    }
  }

  TAILQ_INSERT_TAIL(&sp->so_evlist, eqp, ee_slist);
  postevent(sp, 0, EV_RWBYTES); // catch existing events
  KERNEL_DEBUG(DBG_MISC_WATCH|DBG_FUNC_END, 0,0,0,0,0);
  return(0);
}

struct evwait_args {
  struct eventreq *u_req;
  struct timeval *tv;
};

/*
 * waitevent system call.
 * grabs the next waiting event for this proc and returns
 * it. if no events, user can request to sleep with timeout
 * or poll mode (tv=NULL);
 */
int
waitevent(p, uap, retval)
     struct proc *p;
     struct evwait_args *uap;
     register_t *retval;
{
  int error = 0;
  struct eventqelt *eqp;
  int timo;
  struct timeval atv;
  int s;

	if (uap->tv) {
		error = copyin((caddr_t)uap->tv, (caddr_t)&atv,
			sizeof (atv));
		if (error)
		        return(error);
		if (itimerfix(&atv)) {
			error = EINVAL;
			return(error);
		}
		s = splhigh();
		timeradd(&atv, &time, &atv);
		timo = hzto(&atv);
		splx(s);
	} else
		timo = 0;

  KERNEL_DEBUG(DBG_MISC_WAIT|DBG_FUNC_START, 0,0,0,0,0);

retry:
  s = splhigh();
  if ((eqp = evprocdeque(p,NULL)) != NULL) {
    splx(s);
    error = copyout((caddr_t)&eqp->ee_req, (caddr_t)uap->u_req,
		    sizeof(struct eventreq));
    KERNEL_DEBUG(DBG_MISC_WAIT|DBG_FUNC_END, 0,
		 eqp->ee_req.er_handle,eqp->ee_req.er_eventbits,eqp,0);
    return(error);
  } else {
    if (uap->tv && (timo == 0)) {
        splx(s);
        *retval = 1;  // poll failed
	return(error);
    }

    KERNEL_DEBUG(DBG_MISC_WAIT, 1,0,0,0,0);
    error = tsleep(&p->p_evlist, PSOCK | PCATCH, "waitevent", timo);
    KERNEL_DEBUG(DBG_MISC_WAIT, 0,0,2,p->p_evlist.tqh_first,0);
    splx(s);
    if (error == 0)
      goto retry;
    if (error == ERESTART)
      error = EINTR;
    if (error == EWOULDBLOCK) {
      *retval = 1;
      error = 0;
    }
  }
  KERNEL_DEBUG(DBG_MISC_WAIT|DBG_FUNC_END, 0,0,0,0,0);
  return(error);
}

struct modwatch_args {
  struct eventreq *u_req;
  int               u_eventmask;
};

/*
 * modwatch system call. user passes in event to modify.
 * if we find it we reset the event bits and que/deque event
 * it needed.
 */
int
modwatch(p, uap, retval)
     struct proc *p;
     struct modwatch_args *uap;
     register_t *retval;
{
  struct eventreq er;
  struct eventreq *erp = &er;
  struct eventqelt *evq;
  int error;
  struct file *fp;
  struct socket *sp;
  int flag;


  // get users request pkt
  if (error = copyin((caddr_t)uap->u_req, (caddr_t)erp,
		     sizeof(struct eventreq))) return(error);

  if (erp->er_type != EV_FD) return(EINVAL);
  if (erp->er_handle < 0) return(EBADF);
  if (erp->er_handle > p->p_fd->fd_nfiles) return(EBADF);
  if ((fp = *fdfile(p, erp->er_handle)) == NULL)
    return(EBADF);
  if (fp->f_type != DTYPE_SOCKET) return(EINVAL); // for now must be sock
  sp = (struct socket *)fp->f_data;
  assert(sp != NULL);

  // locate event if possible
  for (evq = sp->so_evlist.tqh_first;
       evq != NULL; evq = evq->ee_slist.tqe_next) {
    if (evq->ee_proc == p) break;
  }
  if (evq == NULL) {
    return(EINVAL);
  }


    if (uap->u_eventmask == EV_RM) {
    evprocdeque(p, evq);
    TAILQ_REMOVE(&sp->so_evlist, evq, ee_slist);
    FREE(evq, M_TEMP);
    return(0);
    }

  switch (uap->u_eventmask & EV_MASK) {
 
  case 0:
    flag = 0;
    break;

  case EV_RE:
  case EV_WR:
  case EV_RE|EV_WR:
    flag = EV_RWBYTES;
    break;

  case EV_EX:
    flag = EV_OOB;
    break;

  case EV_EX|EV_RE:
  case EV_EX|EV_WR:
  case EV_EX|EV_RE|EV_WR:
    flag = EV_OOB|EV_RWBYTES;
    break;

  default:
    return(EINVAL);
  }

   evq->ee_eventmask = uap->u_eventmask & EV_MASK;
   evprocdeque(p, evq);
   evq->ee_req.er_eventbits = 0;
   postevent(sp, 0, flag);
   return(0);
}
