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
 *	Copyright (c) 1988, 1989, 1997, 1998 Apple Computer, Inc. 
 *
 *   Modified for MP, 1996 by Tuyen Nguyen
 *   Modified, March 17, 1997 by Tuyen Nguyen for MacOSX.
 */

/* ddp_proto.c: 2.0, 1.23; 10/18/93; Apple Computer, Inc. */

#include <sys/errno.h>
#include <sys/types.h>
#include <sys/param.h>
#include <machine/spl.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/filedesc.h>
#include <sys/fcntl.h>
#include <sys/mbuf.h>
#include <sys/ioctl.h>
#include <sys/malloc.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/buf.h>

#include <net/if.h>

#include <netat/sysglue.h>
#include <netat/appletalk.h>
#include <netat/at_var.h>
#include <netat/ddp.h>
#include <netat/zip.h>
#include <netat/at_pcb.h>
#include <netat/asp.h>
#include <netat/atp.h>
#include <netat/debug.h>
#include <netat/adsp.h>
#include <netat/adsp_internal.h>

extern atlock_t ddpall_lock;
extern atlock_t ddpinp_lock;

extern struct atpcb ddp_head;
extern gref_t *atp_inputQ[];
extern struct atp_state *atp_used_list;
extern asp_scb_t *asp_scbQ[];
extern asp_scb_t *scb_used_list;
extern CCB *adsp_inputQ[];
extern CCB *ccb_used_list;

void ddp_putmsg(gref, mp)
	gref_t *gref;
	gbuf_t *mp;
{
	u_char socket;
	register ioc_t	*iocbp;
	register int	error;
	at_ddp_t *ddp;

	switch(gbuf_type(mp)) {
	case MSG_DATA :
		/* If this message is going out on a socket that's not bound, 
		 * nail it.
		 */
		ddp = (at_ddp_t *)gbuf_rptr(mp);
		if ((ddp->type == DDP_ATP) || (ddp->type == DDP_ADSP)) {
			if ((gref == 0) || (gref->lport == 0)) {
				int src_addr_included = 
				  ((ddp->type==DDP_ATP) && ddp->src_node)? 1 : 0;
				(void)ddp_output(&mp, ddp->src_socket, 
						 src_addr_included);
				return;
			}
		}

		if (gref && (gref->lport == 0)) {
			gbuf_freel(mp);
			atalk_notify(gref, ENOTCONN);
			return;
		}
		if ((error = ddp_output(&mp, gref->lport, 0)) != 0) {
			if (gref)
				atalk_notify(gref, error);
		}
		return;

	case MSG_IOCTL :
		iocbp = (ioc_t *)gbuf_rptr(mp);
		if (DDP_IOC_MYIOCTL(iocbp->ioc_cmd)) {
			switch(iocbp->ioc_cmd) {
			case DDP_IOC_GET_CFG :
			  /* *** to be replaced with DDP_GETSOCKNAME sockopt *** */
#ifdef APPLETALK_DEBUG
				kprintf("ddp_putmsg: DDP_IOC_GET_CFG\n");
#endif
				if (gbuf_cont(mp))
					gbuf_freem(gbuf_cont(mp));
				if ((gbuf_cont(mp) = 
				     gbuf_alloc(sizeof(at_inet_t),
						PRI_MED)) == NULL) {
					ioc_ack(ENOBUFS, mp, gref);
					break;
				}
				{
				/* *** was ddp_get_cfg() *** */
				  ddp_addr_t *cfgp = 
				    (ddp_addr_t *)gbuf_rptr(gbuf_cont(mp));
				  cfgp->inet.net = gref->laddr.s_net;
				  cfgp->inet.node = gref->laddr.s_node;
				  cfgp->inet.socket = gref->lport;
				  cfgp->ddptype = gref->ddptype;
				}
				gbuf_wset(gbuf_cont(mp), sizeof(ddp_addr_t));
				iocbp->ioc_count = sizeof(ddp_addr_t);
				ioc_ack(0, mp, gref);
				break;
			case DDP_IOC_GET_STATS :
#ifdef APPLETALK_DEBUG
				kprintf("ddp_putmsg: DDP_IOC_GET_STATS\n");
#endif
				if (gbuf_cont(mp))
					gbuf_freem(gbuf_cont(mp));
				if ((gbuf_cont(mp) = gbuf_alloc(sizeof(at_ddp_stats_t),
					PRI_MED)) == NULL) {
					ioc_ack(ENOBUFS, mp, gref);
					break;
				}
				ddp_get_stats(
					(at_ddp_stats_t *)gbuf_rptr(gbuf_cont(mp)));
				gbuf_wset(gbuf_cont(mp),sizeof(at_ddp_stats_t));
				iocbp->ioc_count = sizeof(at_ddp_stats_t);
				ioc_ack(0, mp, gref);
				break;
			}
		} else {
			/* Unknown ioctl */
			ioc_ack(EINVAL, mp, gref);
		}
		break;
	default :
#ifdef APPLETALK_DEBUG
		kprintf("unexpected message type in ddp_putmsg: %d/n", 
			gbuf_type(mp));
#endif
		gbuf_freem(mp);
		break;
	}
	return;
} /* ddp_putmsg */

gbuf_t  *ddp_compress_msg(mp)
register gbuf_t	*mp;
{
        register gbuf_t   *tmp;

        while (gbuf_len(mp) == 0) {
                tmp = mp;
                mp = gbuf_cont(mp);
                gbuf_freeb(tmp);

		if (mp == NULL)
		        break;
	}
	return (mp);
}

int
list_pids(pids, max_size)
     int pids[];
     int max_size;
{
	int 
	  s,
	  i = 0, 
	  k,
	  *debugP = &pids[i],
	  space = max_size;

	asp_scb_t *scb;
	struct atp_state *atp;
	gref_t *gref;
	CCB *sp;
	
#ifdef NOT_YET
/* *** This doesn't work because the socket being used to decide whether
       AppleTalk can be terminated prevents it from being terminated. *** */

	for (gref = ddp_head.atpcb_next; gref != &ddp_head;
	       gref = gref->atpcb_next) {
		pids[i++] = gref->pid;
		space--;
	}
#endif
	while (debugP != &pids[i]) {
		dPrintf(D_M_DDP,D_L_TRACE, ("ddp pid=%d\n", *debugP));
		debugP++;
	}

	/* originally from asp_stop() */
	ATDISABLE(s, aspall_lock);
	for (scb = scb_used_list; scb && space; scb = scb->next_scb) {
		pids[i++] = scb->pid;
		space--;
	}
	for (k=0; k < 256 && space ; k++) {
	    if ((scb = asp_scbQ[k]))
		do {
			pids[i++] = scb->pid;
			space--;
		} while (space && (scb = scb->next_scb) != 0);
	}
	ATENABLE(s, aspall_lock);
	while (debugP != &pids[i]) {
		dPrintf(D_M_ASP, D_L_TRACE, ("asp pid=%d\n", *debugP));
		debugP++;
	}

	/* originally from atp_stop */
	ATDISABLE(s, atpall_lock);
	for (atp = atp_used_list; atp && space; atp = atp->atp_trans_waiting) {
		pids[i++] = atp->atp_pid;
		space--;
	}
	for (k=0; k < 256 && space; k++) {
	  if ((gref = atp_inputQ[k]) && (gref != (gref_t *)1)) {
		atp = (struct atp_state *)gref->info;	
		if (!atp->dflag) {
		  pids[i++] = atp->atp_pid;
		  space--;
		}
	  }
	}
	ATENABLE(s, atpall_lock);
	while (debugP != &pids[i]) {
		dPrintf(D_M_ATP, D_L_TRACE, ("atp pid=%d\n", *debugP));
		debugP++;
	}

	/* originally from adsp_stop() */
	ATDISABLE(s, adspall_lock);
	for (sp = ccb_used_list; sp && space ; sp = sp->otccbLink) {
		pids[i++] = sp->pid;
		space--;
	}
	for (k=0; k < 256 && space ; k++) {
	    if ((sp = adsp_inputQ[k]))
		do {
		pids[i++] = sp->pid;
		space--;
		} while ((sp = sp->otccbLink) != 0);
	}
	ATENABLE(s, adspall_lock);

	while (debugP != &pids[i]) {
		dPrintf(D_M_ADSP, D_L_TRACE, ("adsp pid=%d\n", *debugP));
		debugP++;
	}

	return(i);
}
