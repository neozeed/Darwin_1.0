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
 * Copyright (c) 1982, 1986, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)sys_socket.c	8.1 (Berkeley) 6/10/93
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/file.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/filio.h>			/* XXX */
#include <sys/sockio.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/filedesc.h>

#include <net/if.h>
#include <net/route.h>

int soo_read __P((struct file *fp, struct uio *uio, 
		struct ucred *cred));
int soo_write __P((struct file *fp, struct uio *uio, 
		struct ucred *cred));
int soo_close __P((struct file *fp, struct proc *p));

int soo_select __P((struct file *fp, int which, struct proc *p));

struct	fileops socketops =
    { soo_read, soo_write, soo_ioctl, soo_select, soo_close };

/* ARGSUSED */
int
soo_read(fp, uio, cred)
	struct file *fp;
	struct uio *uio;
	struct ucred *cred;
{
	struct socket *so = (struct socket *)fp->f_data;
	struct kextcb *kp;
	int (*fsoreceive) __P((struct socket *so, 
			       struct sockaddr **paddr,
			       struct uio *uio, struct mbuf **mp0,
			       struct mbuf **controlp, int *flagsp));

	fsoreceive = so->so_proto->pr_usrreqs->pru_soreceive;
	if (fsoreceive != soreceive)
	{	kp = sotokextcb(so);
		while (kp)
		{	if (kp->e_soif && kp->e_soif->sf_soreceive)
				(*kp->e_soif->sf_soreceive)(so, 0, &uio,
							    0, 0, 0, kp);
			kp = kp->e_next;
		}

	}
	return (*fsoreceive)(so, 0, uio, 0, 0, 0);
}

/* ARGSUSED */
int
soo_write(fp, uio, cred)
	struct file *fp;
	struct uio *uio;
	struct ucred *cred;
{
	struct socket *so = (struct socket *)fp->f_data;
	int	(*fsosend) __P((struct socket *so, struct sockaddr *addr,
				struct uio *uio, struct mbuf *top,
				struct mbuf *control, int flags));
	struct kextcb *kp;

	fsosend = so->so_proto->pr_usrreqs->pru_sosend;
	if (fsosend != sosend)
	{	kp = sotokextcb(so);
		while (kp)
		{	if (kp->e_soif && kp->e_soif->sf_sosend)
			(*kp->e_soif->sf_sosend)(so, 0, &uio,
						 0, 0, 0, kp);
			kp = kp->e_next;
		}
	}

	return (*fsosend)(so, 0, uio, 0, 0, 0);
}

int
soo_ioctl(fp, cmd, data, p)
	struct file *fp;
	u_long cmd;
	register caddr_t data;
	struct proc *p;
{
	register struct socket *so = (struct socket *)fp->f_data;

	struct sockopt sopt;
	struct kextcb *kp;

	kp = sotokextcb(so);
	sopt.sopt_level = cmd;
	sopt.sopt_name = (int)data;
	sopt.sopt_p = p;
	while (kp)
	{	if (kp->e_soif && kp->e_soif->sf_socontrol)
			(*kp->e_soif->sf_socontrol)(so, &sopt, kp);
		kp = kp->e_next;
	}

	switch (cmd) {

	case FIONBIO:
		if (*(int *)data)
			so->so_state |= SS_NBIO;
		else
			so->so_state &= ~SS_NBIO;
		return (0);

	case FIOASYNC:
		if (*(int *)data) {
			so->so_state |= SS_ASYNC;
			so->so_rcv.sb_flags |= SB_ASYNC;
			so->so_snd.sb_flags |= SB_ASYNC;
		} else {
			so->so_state &= ~SS_ASYNC;
			so->so_rcv.sb_flags &= ~SB_ASYNC;
			so->so_snd.sb_flags &= ~SB_ASYNC;
		}
		return (0);

	case FIONREAD:
		*(int *)data = so->so_rcv.sb_cc;
		return (0);

	case SIOCSPGRP:
		so->so_pgid = *(int *)data;
		return (0);

	case SIOCGPGRP:
		*(int *)data = so->so_pgid;
		return (0);

	case SIOCATMARK:
		*(int *)data = (so->so_state&SS_RCVATMARK) != 0;
		return (0);
    case SIOCSETOT: {
        /*
         * Set socket level options here and then call protocol
         * specific routine.
         */
        struct socket	*cloned_so = NULL;
        int				cloned_fd = *(int *)data;
        int				error = 0;

        /* let's make sure it's either -1 or a valid file descriptor */
        if (cloned_fd != -1) {
            struct file     *cloned_fp;
            error = getsock(p->p_fd, cloned_fd, &cloned_fp);
            if (error)
                return (error);
            cloned_so = (struct socket *)cloned_fp->f_data;
        }

        /* Always set socket non-blocking for OT */
        so->so_state |= SS_NBIO;
        so->so_options |= SO_DONTTRUNC | SO_WANTMORE;

        if (cloned_so && so != cloned_so) {
            /* Flags options */
            so->so_options |= cloned_so->so_options;

            /* SO_LINGER */
            if (so->so_options & SO_LINGER)
                so->so_linger = cloned_so->so_linger;

            /* SO_SNDBUF, SO_RCVBUF */
            if (sbreserve(&so->so_snd, cloned_so->so_snd.sb_hiwat) == 0) {
                error = ENOBUFS;
                return (error);
            }
            if (sbreserve(&so->so_rcv, cloned_so->so_rcv.sb_hiwat) == 0) {
                error = ENOBUFS;
                return (error);
            }

            /* SO_SNDLOWAT, SO_RCVLOWAT */
            so->so_snd.sb_lowat =
                (cloned_so->so_snd.sb_lowat > so->so_snd.sb_hiwat) ?
                so->so_snd.sb_hiwat : cloned_so->so_snd.sb_lowat;
            so->so_rcv.sb_lowat =
                (cloned_so->so_rcv.sb_lowat > so->so_rcv.sb_hiwat) ?
                so->so_rcv.sb_hiwat : cloned_so->so_rcv.sb_lowat;

            /* SO_SNDTIMEO, SO_RCVTIMEO */
            so->so_snd.sb_timeo = cloned_so->so_snd.sb_timeo;
            so->so_rcv.sb_timeo = cloned_so->so_rcv.sb_timeo;
        }

        error = (*so->so_proto->pr_usrreqs->pru_control)(so, cmd, data, 0, p);
        /* Just ignore protocols that do not understand it */
        if (error == EOPNOTSUPP)
            error = 0;
        return (error);
        }
	}
	/*
	 * Interface/routing/protocol specific ioctls:
	 * interface and routing ioctls should have a
	 * different entry since a socket's unnecessary
	 */
	if (IOCGROUP(cmd) == 'i')
		return (ifioctl(so, cmd, data, p));
	if (IOCGROUP(cmd) == 'r')
		return (rtioctl(cmd, data, p));
	return ((*so->so_proto->pr_usrreqs->pru_control)(so, cmd, data, 0, p));
}

int
soo_select(fp, which, p)
	struct file *fp;
	int which;
	struct proc *p;
{
	register struct socket *so = (struct socket *)fp->f_data;
	register int s = splnet();

	switch (which) {

	case FREAD:
		if (soreadable(so)) {
			splx(s);
			return (1);
		}
		selrecord(p, &so->so_rcv.sb_sel);
		so->so_rcv.sb_flags |= SB_SEL;
		break;

	case FWRITE:
		if (sowriteable(so)) {
			splx(s);
			return (1);
		}
		selrecord(p, &so->so_snd.sb_sel);
		so->so_snd.sb_flags |= SB_SEL;
		break;

	case 0:
		if (so->so_oobmark || (so->so_state & SS_RCVATMARK)) {
			splx(s);
			return (1);
		}
		selrecord(p, &so->so_rcv.sb_sel);
		so->so_rcv.sb_flags |= SB_SEL;
		break;
	}
	splx(s);
	return (0);
}


int
soo_stat(so, ub)
	register struct socket *so;
	register struct stat *ub;
{

	bzero((caddr_t)ub, sizeof (*ub));
	ub->st_mode = S_IFSOCK;
	return ((*so->so_proto->pr_usrreqs->pru_sense)(so, ub));
}

/* ARGSUSED */
int
soo_close(fp, p)
	struct file *fp;
	struct proc *p;
{
	int error = 0;

	if (fp->f_data)
		error = soclose((struct socket *)fp->f_data);
	fp->f_data = 0;
	return (error);
}
