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
/* Copyright (c) 1997, 1998 Apple Computer, Inc. All Rights Reserved */
/*
 *	@(#)ndrv.c	1.1 (MacOSX) 6/10/43
 * Justin Walker, 970604
 *   AF_NDRV support
 * 980130 - Cleanup, reorg, performance improvemements
 */

/*
 * PF_NDRV allows raw access to a specified network device, directly
 *  with a socket.  Expected use involves a socket option to request
 *  protocol packets.  This lets ndrv_output() call dlil_output(), and
 *  lets DLIL find the proper recipient for incoming packets.
 *  The purpose here is for user-mode protocol implementation.
 * Note that "pure raw access" will still be accomplished with BPF.
 *
 * In addition to the former use, when combined with socket NKEs,
 * PF_NDRV permits a fairly flexible mechanism for implementing
 * strange protocol support.  One of the main ones will be the
 * BlueBox/Classic Shared IP Address support.
 *
 * TODO: Add user access to protocol registration
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/domain.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/syslog.h>
#include <sys/proc.h>

#include <kern/queue.h>

#include <net/if.h>
#include <net/netisr.h>
#include <net/route.h>
#include <net/if_llc.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include "if_blue.h"		/* TEMP!  Until BB gets in synch */
#include "ndrv.h"

#if INET
#include <netinet/in.h>
#include <netinet/in_var.h>
#endif
#include <netinet/if_ether.h>

#if NS
#include <netns/ns.h>
#include <netns/ns_if.h>
#endif

#if ISO
#include <netiso/argo_debug.h>
#include <netiso/iso.h>
#include <netiso/iso_var.h>
#include <netiso/iso_snpac.h>
#endif

#if LLC
#include <netccitt/dll.h>
#include <netccitt/llc_var.h>
#endif

#include <machine/spl.h>

int local_ndrv_attach(struct socket *, int);
int local_ndrv_detach(struct ndrv_cb *);
int local_ndrv_bind(struct socket *, struct sockaddr_ndrv *);
int local_ndrv_disconnect(struct ndrv_cb *);

extern int splitter_ctl(struct socket *, int, caddr_t, struct ifnet *);

void local_ndrv_dinit(void);

unsigned long  ndrv_sendspace = NDRVSNDQ;
unsigned long  ndrv_recvspace = NDRVRCVQ;
struct ndrv_cb ndrvl;		/* Head of controlblock list */

#define BLUEQMAXLEN	50
int blueqmaxlen = BLUEQMAXLEN;	/* Default */

/* Domain init function for AF_NDRV - noop */
void
local_ndrv_dinit(void)
{
}

/*
 * Protocol init function for NDRV protocol
 * Init the control block list.
 */
void
ndrv_init()
{	ndrvl.nd_next = ndrvl.nd_prev = &ndrvl;
	blueq.ifq_maxlen = blueqmaxlen;
}

/*
 * Protocol output - Called to output a raw network packet directly
 *  to the driver.  If we're splitting, maybe loop it back.
 */
int
ndrv_output(register struct mbuf *m,
	    register struct socket *so)
{	register struct ndrv_cb *np = sotondrvcb(so);
	register struct ifnet *ifp = np->nd_if;
	int s, error;
	extern struct ifnet_blue *blue_if;
	extern void kprintf(const char *, ...);
	extern int Filter_check(struct mbuf **);
#define senderr(e) { error = (e); goto bad;}

#if NDRV_DEBUG
	kprintf("NDRV output: %x, %x, %x\n", m, so, np);
#endif

	/*
	 * No header is a format error
	 */
	if ((m->m_flags&M_PKTHDR) == 0)
		return(ENOBUFS); /* EINVAL??? */

	/* If we're splitting,  */
	if (ifp->if_flags&IFF_SPLITTER) /* Splitter is turned on */
	{	register struct mbuf *m0;
		struct mbuf *m1;
		register int rv;
		extern struct mbuf *m_dup(struct mbuf *, int);
#if 0
		kprintf("NDRV_OUTPUT: m0 = %x\n", m0);
#endif
		
		m1 = m;
		rv = Filter_check(&m1);
		m = m1;
		/*
		 * -1 => Not For MacOSX
		 * 0 => For Both
		 * 1 => For MacOSX
		 */
		if (rv >= 0)
		{	register struct ether_header *eh;

			if (rv == 0)
			{	if ((m0 = m_dup(m, M_WAIT)) == NULL)
					((struct ifnet_blue *)ifp->if_Y)->no_bufs2++;
			} else
			{	m0 = m;
				m = NULL;
			}
			/* Hack alert! */
			eh = mtod(m0, struct ether_header *);
			m0->m_data += sizeof(struct ether_header);
			m0->m_len -= sizeof (struct ether_header);
			m0->m_pkthdr.len -= sizeof(struct ether_header);
			m0->m_flags |= 0x80;
#if NDRV_DEBUG
			kprintf("NDRV_OUTPUT: m0 = %x\n", m0);
#endif
			new_ether_input(m0, eh, (struct ifnet *)blue_if, 0, 1);
			if (!m)
				return(0);
		}
	}

	/*
	 * Output to real device.
	 *
	 * Can't do multicast accounting because we don't know
	 *  (a) if our interface does multicast; and
	 *  (b) what a multicast address looks like
	 */
	s = splimp();

	/*
	 * Can't call DLIL to do the job - we don't have a tag
	 *  and we aren't really a protocol
	 */

        (*ifp->if_output)(ifp, m);
	splx(s);
	ifp->if_obytes += m->m_len; /* MP alert! */
	blue_if->pkts_out++;
	return (0);
bad:
	if (m)
		m_freem(m);
	return (error);
}





int ndrv_control(struct socket *so, u_long cmd, caddr_t data,
		  struct ifnet *ifp, struct proc *p) 
{
		return (splitter_ctl(so, (int)cmd, data,
				     ifp));
}

int ndrv_attach(struct socket *so, int proto,
		    struct proc *p)
{

	return local_ndrv_attach(so, proto);
}



/*
 * Destroy state just before socket deallocation.
 * Flush data or not depending on the options.
 */

int	ndrv_detach(struct socket *so) 
{
	register struct ndrv_cb *np = sotondrvcb(so);

	if (np == 0) 
		return EINVAL;
	return local_ndrv_detach(np);
}


/*
 * If a socket isn't bound to a single address,
 * the ndrv input routine will hand it anything
 * within that protocol family (assuming there's
 * nothing else around it should go to).
 *
 * Don't expect this to be used.
 */

int ndrv_connect(struct socket *so, struct sockaddr *nam,
			    struct proc *p)
{
	register struct ndrv_cb *np = sotondrvcb(so);

	if (np == 0) 
		return EINVAL;

	if (np->nd_faddr) 
		return EISCONN;
	
	bcopy((caddr_t) nam, (caddr_t) np->nd_faddr, sizeof(struct sockaddr_ndrv));
	soisconnected(so);
	return 0;
}

/*
 * This is the "driver open" hook - we 'bind' to the
 *  named driver.
 */
int	ndrv_bind(struct socket *so, struct sockaddr *nam,
		  struct proc *p)
{
	register struct ndrv_cb *np = sotondrvcb(so);

	if (np == 0) 
		return EINVAL;

	if (np->nd_laddr) 
		return EINVAL;			/* XXX */

	return local_ndrv_bind(so, (struct sockaddr_ndrv *) nam);
}




int	ndrv_disconnect(struct socket *so)
{
	register struct ndrv_cb *np = sotondrvcb(so);

	if (np == 0) 
		return EINVAL;

	if (np->nd_faddr == 0)
		return ENOTCONN;

	local_ndrv_disconnect(np);
	soisdisconnected(so);
	return 0;
}

/*
 * Mark the connection as being incapable of further input.
 */
int	ndrv_shutdown(struct socket *so)
{
	socantsendmore(so);
	return 0;
}

/*
 * Ship a packet out.  The ndrv output will pass it
 *  to the appropriate driver.  The really tricky part
 *  is the destination address...
 */
int	ndrv_send(struct socket *so, int flags, struct mbuf *m, 
		  struct sockaddr *addr, struct mbuf *control,
		  struct proc *p)
{
	int error;

	if (control) 
		return EOPNOTSUPP;

	error = ndrv_output(m, so);
	m = NULL;
	return error;
}


int	ndrv_abort(struct socket *so)
{
	register struct ndrv_cb *np = sotondrvcb(so);

	if (np == 0) 
		return EINVAL;

	local_ndrv_disconnect(np);
	sofree(so);
	soisdisconnected(so);
	return 0;
}

int	ndrv_sense(struct socket *so, struct stat *sb)
{
	/*
	 * stat: don't bother with a blocksize.
	 */
	return (0);
}

int	ndrv_sockaddr(struct socket *so, 
		       struct sockaddr **nam)
{
	register struct ndrv_cb *np = sotondrvcb(so);
	int len;

	if (np == 0) 
		return EINVAL;

	if (np->nd_laddr == 0) 
		return EINVAL;

	len = np->nd_laddr->snd_len;
	bcopy((caddr_t)np->nd_laddr, *nam,
	      (unsigned)len);
	return 0;
}


int	ndrv_peeraddr(struct socket *so, 
		       struct sockaddr **nam)
{
	register struct ndrv_cb *np = sotondrvcb(so);
	int len;

	if (np == 0) 
		return EINVAL;

	if (np->nd_faddr == 0) 
		return ENOTCONN;

	len = np->nd_faddr->snd_len;
	bcopy((caddr_t)np->nd_faddr, *nam,
	      (unsigned)len);
	return 0;
}


/* Control input */

void	ndrv_ctlinput(int dummy1, struct sockaddr *dummy2, void *dummy3)

{
}

/* Control output */

int	ndrv_ctloutput(struct socket *dummy1, struct sockopt *dummy2)
{
	return(0);
}

/* Drain the queues */
void
ndrv_drain()
{
}

/* Sysctl hook for NDRV */
int
ndrv_sysctl()
{
	return(0);
}

/*
 * Allocate an ndrv control block and some buffer space for the socket
 */
int
local_ndrv_attach(register struct socket *so,
	    register int proto)
{	int error;
	register struct ndrv_cb *np = sotondrvcb(so);

	if ((so->so_state & SS_PRIV) == 0)
		return(EPERM);

#if NDRV_DEBUG
	kprintf("NDRV attach: %x, %x, %x\n", so, proto, np);
#endif
	MALLOC(np, struct ndrv_cb *, sizeof(*np), M_PCB, M_WAITOK);
#if NDRV_DEBUG
	kprintf("NDRV attach: %x, %x, %x\n", so, proto, np);
#endif
	if ((so->so_pcb = (caddr_t)np))
		bzero(np, sizeof(*np));
	else
		return(ENOBUFS);
	if ((error = soreserve(so, ndrv_sendspace, ndrv_recvspace)))
		return(error);
	np->nd_signature = NDRV_SIGNATURE;
	np->nd_socket = so;
	np->nd_proto.sp_family = so->so_proto->pr_domain->dom_family;
	np->nd_proto.sp_protocol = proto;
	insque((queue_t)np, (queue_t)&ndrvl);
//##### eeccckkk added here to get the kext filter "attached"... TEMP 
//	sfilter_init(so);
	return(0);
}

int
local_ndrv_detach(register struct ndrv_cb *np)
{	register struct socket *so = np->nd_socket;
	extern struct ifnet_blue *blue_if;
	extern void splitter_close(struct ndrv_cb *);

#if NDRV_DEBUG
	kprintf("NDRV detach: %x, %x\n", so, np);
#endif
	if (blue_if)
		splitter_close(np); /* 'np' is freed within */
	so->so_pcb = 0;
	sofree(so);
	return(0);
}

int
local_ndrv_disconnect(register struct ndrv_cb *np)
{
#if NDRV_DEBUG
	kprintf("NDRV disconnect: %x\n", np);
#endif
	if (np->nd_faddr)
	{	m_freem(dtom(np->nd_faddr));
		np->nd_faddr = 0;
	}
	if (np->nd_socket->so_state & SS_NOFDREF)
		local_ndrv_detach(np);
	return(0);
}

/*
 * Here's where we latch onto the driver and make it ours.
 */
int
local_ndrv_bind(register struct socket *so,
	  register struct sockaddr_ndrv *sa)
{	register char *dname;
	register struct ndrv_cb *np;
	register struct ifnet *ifp;
	extern int name_cmp(struct ifnet *, char *);

	if TAILQ_EMPTY(&ifnet)
		return(EADDRNOTAVAIL); /* Quick sanity check */
	np = sotondrvcb(so);

	/* I think we just latch onto a copy here; the caller frees */

	
	np->nd_laddr = _MALLOC(sizeof(struct sockaddr_ndrv), M_IFADDR, M_WAITOK);
	bcopy((caddr_t) sa, (caddr_t) np->nd_laddr, sizeof(struct sockaddr_ndrv));
	dname = sa->snd_name;
	if (dname == NULL)
		return(EINVAL);
#if NDRV_DEBUG
	kprintf("NDRV bind: %x, %x, %s\n", so, np, dname);
#endif
	/* Track down the driver and its ifnet structure.
	 * There's no internal call for this so we have to dup the code
	 *  in if.c/ifconf()
	 */
	TAILQ_FOREACH(ifp, &ifnet, if_link) {
		if (name_cmp(ifp, dname) == 0)
			break;
	}

	if (ifp == NULL)
		return(EADDRNOTAVAIL);
	/*
	 * Now, at this point, we should force open the driver and somehow
	 *  register ourselves to receive packets (a la the bpf).
	 * However, we have this groaty hack in place that makes it
	 *  not necessary for blue box purposes (the 'splitter' trick).
	 * If we want this to be a full-fledged AF, we have to force the
	 *  open and implement a filter mechanism.
	 */
	np->nd_if = ifp;
	return(0);
}

/*
 * Try to compare a device name (q) with one of the funky ifnet
 *  device names (ifp).
 */
int name_cmp(register struct ifnet *ifp, register char *q)
{	register char *r;
	register int len;
	char buf[IFNAMSIZ];
	static char *sprint_d();

	r = buf;
	len = strlen(ifp->if_name);
	strncpy(r, ifp->if_name, IFNAMSIZ);
	r += len;
	(void)sprint_d(ifp->if_unit, r, r-buf);
#if NDRV_DEBUG
	kprintf("Comparing %s, %s\n", buf, q);
#endif
	return(strncmp(buf, q, IFNAMSIZ));
}

/* Hackery - return a string version of a decimal number */
static char *
sprint_d(n, buf, buflen)
        u_int n;
        char *buf;
        int buflen;
{
        register char *cp = buf + buflen - 1;

        *cp = 0;
        do {
                cp--;
                *cp = "0123456789"[n % 10];
                n /= 10;
        } while (n != 0);
        return (cp);
}

/*
 * When closing, dump any enqueued mbufs.
 */
void
ndrv_flushq(register struct ifqueue *q)
{	register struct mbuf *m;
	register int s;
	for (;;)
	{	s = splimp();
		IF_DEQUEUE(q, m);
		if (m == NULL)
			break;
		IF_DROP(q);
		splx(s);
		if (m)
			m_freem(m);
	}
	splx(s);
}


struct pr_usrreqs ndrv_usrreqs = {
	ndrv_abort, pru_accept_notsupp, ndrv_attach, ndrv_bind,
	ndrv_connect, pru_connect2_notsupp, ndrv_control, ndrv_detach,
	ndrv_disconnect, pru_listen_notsupp, ndrv_peeraddr, pru_rcvd_notsupp,
	pru_rcvoob_notsupp, ndrv_send, ndrv_sense, ndrv_shutdown,
	ndrv_sockaddr, sosend, soreceive, sopoll
};

struct domain ndrvdomain;

struct protosw ndrvsw[] =
{	{	SOCK_RAW, &ndrvdomain, 0, PR_ATOMIC|PR_ADDR,
		0, ndrv_output, ndrv_ctlinput, ndrv_ctloutput,
		0, ndrv_init, 0, 0,
		ndrv_drain, ndrv_sysctl, &ndrv_usrreqs
	}
};


struct domain ndrvdomain =
{	AF_NDRV, "NetDriver", NULL, NULL, NULL,
	ndrvsw,
	NULL, NULL, 0, 0, 0, 0
};
