/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").	You may not use this file except in compliance with the
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
 *	Copyright (c) 1999 Apple Computer, Inc. 
 *
 *	Data Link Inteface Layer
 *	Author: Ted Walker
 */



#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <net/if_dl.h>
#include <net/if.h>
#include <net/if_var.h>
#include <net/dlil.h>



#define MAX_DL_TAGS 50
#define MAX_DLIL_FILTERS 50
#define MAX_FRAME_TYPE_SIZE 4 /* LONGWORDS */
#define MAX_LINKADDR	    4 /* LONGWORDS */
#define M_NKE M_IFADDR

#define PFILT(x) ((struct dlil_filterq_entry *) (x))->variants.pr_filter
#define IFILT(x) ((struct dlil_filterq_entry *) (x))->variants.if_filter

struct dl_tag_str {
    struct ifnet	*ifp;
    struct if_proto	*proto;
    struct dlil_filterq_head *pr_flt_head;
};


struct dlil_stats_str {
    int	   inject_pr_in1;    
    int	   inject_pr_in2;
    int	   inject_pr_out1;
    int	   inject_pr_out2;
    int	   inject_if_in1;
    int	   inject_if_in2;
    int	   inject_if_out1;
    int	   inject_if_out2;
};


struct dlil_filter_id_str {
    int			      type;
    struct dlil_filterq_head  *head;
    struct dlil_filterq_entry *filter_ptr;
    struct ifnet	      *ifp;
    struct if_proto	      *proto;
};



struct if_family_str {
    TAILQ_ENTRY(if_family_str) if_fam_next;
    u_long	if_family;
    int		refcnt;
    int		flags;

#define DLIL_SHUTDOWN 1

    int (*add_if)(struct ifnet *ifp);
    int (*del_if)(struct ifnet *ifp);
    int (*add_proto)(struct ddesc_head_str *demux_desc_head,
		     struct if_proto  *proto, u_long dl_tag);
    int (*del_proto)(struct if_proto  *proto, u_long dl_tag);
    int (*shutdown)();
};



struct dlil_stats_str dlil_stats;

static
struct dlil_filter_id_str dlil_filters[MAX_DLIL_FILTERS+1];

static
struct dl_tag_str    dl_tag_array[MAX_DL_TAGS+1];

static 
TAILQ_HEAD(, if_family_str) if_family_head;

static		    ifnet_inited = 0;

/*
 * Internal functions.
 */

static 
struct if_family_str *find_family_module(u_long if_family)
{
    struct if_family_str  *mod = NULL;

    TAILQ_FOREACH(mod, &if_family_head, if_fam_next) {
	if (mod->if_family == if_family) 
	    break;
    }

    return mod;
}


/*
 * Public functions.
 */

struct ifnet *ifbyfamily(u_long family, short unit)
{
    struct ifnet *ifp;

    TAILQ_FOREACH(ifp, &ifnet, if_link)
	if ((family == ifp->if_family) &&
	    (ifp->if_unit == unit))
	    return ifp;

    return 0;
}

struct if_proto *dlttoproto(dl_tag)
    u_long dl_tag;
{
    return dl_tag_array[dl_tag].proto;
}


u_long	ifptodlt(struct ifnet *ifp, u_long proto_family)
{
    struct if_proto *proto;
    struct dlil_proto_head  *tmp = (struct dlil_proto_head *) &ifp->proto_head;


    TAILQ_FOREACH(proto, tmp, next)
	if (proto->ifp == ifp)
	    if (proto->protocol_family == proto_family)
		return proto->dl_tag;

    return 0;
}

    
int  dlil_find_dltag(u_long if_family, short unit, u_long proto_family, u_long *dl_tag)
{
    struct ifnet  *ifp;

    ifp = ifbyfamily(if_family, unit);
    if (!ifp)
	return ENOENT;

    *dl_tag = ifptodlt(ifp, proto_family);
    if (*dl_tag == 0)
	return EPROTONOSUPPORT;
    else
	return 0;
}


int dlil_get_next_dl_tag(u_long current_tag, struct dl_tag_attr_str *next)
{
    int i;

    for (i = (current_tag+1); i < MAX_DL_TAGS; i++)
	if (dl_tag_array[i].ifp) {
	    next->dl_tag   = i;
	    next->if_flags = dl_tag_array[i].ifp->if_flags;
	    next->if_unit  = dl_tag_array[i].ifp->if_unit;
	    next->protocol_family = dl_tag_array[i].proto->protocol_family;
	    next->if_family = dl_tag_array[i].ifp->if_family;
	    return 0;
	}

    /*
     * If we got here, there are no more entries
     */

    return ENOENT;
} 


void
dlil_init()
{
    int i;

    TAILQ_INIT(&if_family_head);
    for (i=0; i < MAX_DL_TAGS; i++)
	dl_tag_array[i].ifp = 0;

    for (i=0; i < MAX_DLIL_FILTERS; i++)
	dlil_filters[i].type = 0;

    bzero(&dlil_stats, sizeof(dlil_stats));
}


u_long get_new_filter_id()
{
    u_long i;

    for (i=1; i < MAX_DLIL_FILTERS; i++)
	if (dlil_filters[i].type == 0)
	    return i;
 
    return 0;
}


int   dlil_attach_interface_filter(struct ifnet *ifp,
				   struct dlil_if_flt_str  *if_filter,
				   u_long		   *filter_id,
				   int			   insertion_point)
{
    int s;
    int retval;
    struct dlil_filterq_entry	*tmp_ptr;
    struct dlil_filterq_entry	*if_filt;
    struct dlil_filterq_head *fhead = (struct dlil_filterq_head *) &ifp->if_flt_head;


    MALLOC(tmp_ptr, struct dlil_filterq_entry *, sizeof(*tmp_ptr), M_NKE, M_WAITOK);
    bcopy((caddr_t) if_filter, (caddr_t) &tmp_ptr->variants.if_filter, 
	  sizeof(struct dlil_if_flt_str));

    s = splnet();

    if (insertion_point != DLIL_LAST_FILTER) {
	TAILQ_FOREACH(if_filt, fhead, que)
	    if (insertion_point == if_filt->filter_id) {
		TAILQ_INSERT_BEFORE(if_filt, tmp_ptr, que);
		break;
	    }
    }
    else 
	TAILQ_INSERT_TAIL(fhead, tmp_ptr, que);

    if (*filter_id = get_new_filter_id()) {
	dlil_filters[*filter_id].filter_ptr = tmp_ptr;
	dlil_filters[*filter_id].head = (struct dlil_filterq_head *) &ifp->if_flt_head;
	dlil_filters[*filter_id].type = DLIL_IF_FILTER;
	dlil_filters[*filter_id].ifp = ifp;
	tmp_ptr->filter_id = *filter_id;
	tmp_ptr->type	   = DLIL_IF_FILTER;
	retval = 0;
    }
    else {
	kprintf("dlil_attach_interface_filter - can't alloc filter_id\n");
	TAILQ_REMOVE(fhead, tmp_ptr, que);
	FREE(tmp_ptr, M_NKE);
	retval = EOVERFLOW;
    }

    splx(s);
    return retval;
}


int   dlil_attach_protocol_filter(u_long			 dl_tag,
				  struct dlil_pr_flt_str	 *pr_filter,
				  u_long			 *filter_id,
				  int				 insertion_point)
{
    struct dlil_filterq_entry	*tmp_ptr;
    struct dlil_filterq_entry	*pr_filt;
    int s;
    int retval;


    if (dl_tag > MAX_DL_TAGS)
	return ERANGE;

    if (dl_tag_array[dl_tag].ifp == 0)
	return ENOENT;

    MALLOC(tmp_ptr, struct dlil_filterq_entry *, sizeof(*tmp_ptr), M_NKE, M_WAITOK);
    bcopy((caddr_t) pr_filter, (caddr_t) &tmp_ptr->variants.pr_filter, 
	  sizeof(struct dlil_pr_flt_str));


    s = splnet();
    if (insertion_point != DLIL_LAST_FILTER) {
	TAILQ_FOREACH(pr_filt, dl_tag_array[dl_tag].pr_flt_head, que)
	    if (insertion_point == pr_filt->filter_id) {
		TAILQ_INSERT_BEFORE(pr_filt, tmp_ptr, que);
		break;
	    }
    }
    else 
	TAILQ_INSERT_TAIL(dl_tag_array[dl_tag].pr_flt_head, tmp_ptr, que);

    
    if (*filter_id = get_new_filter_id()) {
	dlil_filters[*filter_id].filter_ptr = tmp_ptr; 
	dlil_filters[*filter_id].head = dl_tag_array[dl_tag].pr_flt_head;
	dlil_filters[*filter_id].type = DLIL_PR_FILTER;
	dlil_filters[*filter_id].proto = dl_tag_array[dl_tag].proto;
	dlil_filters[*filter_id].ifp   = dl_tag_array[dl_tag].ifp;
	tmp_ptr->filter_id = *filter_id;
	tmp_ptr->type	   = DLIL_PR_FILTER;
	retval = 0;
    }
    else {
	kprintf("dlil_attach_protocol_filter - can't alloc filter_id\n");
	TAILQ_REMOVE(dl_tag_array[dl_tag].pr_flt_head, tmp_ptr, que);
	FREE(tmp_ptr, M_NKE);
	retval =  EOVERFLOW;
    }

    splx(s);
    return retval;
}


int
dlil_detach_filter(u_long	filter_id)
{
    struct dlil_filter_id_str *flt;
    int s;

    if (filter_id > MAX_DLIL_FILTERS) {
	kprintf("dlil_detach_filter - Bad filter_id value %d\n", filter_id);
	return ERANGE;
    }

    flt = &dlil_filters[filter_id];
    if (flt->type == 0) {
	kprintf("dlil_detach_filter - no such filter_id %d\n", filter_id);
	return ENOENT;
    }

    s = splnet();
    if (flt->type == DLIL_IF_FILTER) {
	if (IFILT(flt->filter_ptr).filter_detach)
	    (*IFILT(flt->filter_ptr).filter_detach)(IFILT(flt->filter_ptr).cookie);
    }
    else {
	if (flt->type == DLIL_PR_FILTER) {
	    if (PFILT(flt->filter_ptr).filter_detach)
		(*PFILT(flt->filter_ptr).filter_detach)(PFILT(flt->filter_ptr).cookie);
	}
    }

    TAILQ_REMOVE(flt->head, flt->filter_ptr, que);
    FREE(flt->filter_ptr, M_NKE);
    flt->type = 0;
    splx(s);
    return 0;
}




int
dlil_input(struct ifnet	 *ifp, struct mbuf *m, 
	   char *frame_header)
{
	boolean_t funnel_state;
	int ret;

	funnel_state = thread_set_funneled(TRUE);

	ret = dlil_input_funneled(ifp, m, frame_header);

	(void) thread_set_funneled(funnel_state);

	return(ret);
}

int
dlil_input_funneled(struct ifnet  *ifp, struct mbuf *m, 
	   char *frame_header)
{
    struct ifnet		 *orig_ifp = 0;
    struct dlil_filterq_entry	 *tmp;
    int				 retval;
    struct if_proto		 *ifproto = 0;
    struct if_proto		 *proto;
    struct dlil_filterq_head *fhead = (struct dlil_filterq_head *) &ifp->if_flt_head;

    /*
     * Temporary, until all drivers do this.
     */

    ifp->if_ibytes += m->m_pkthdr.len;


   /*
    * Run interface filters
    */

    while (orig_ifp != ifp) {
	orig_ifp = ifp;
	
	TAILQ_FOREACH_REVERSE(tmp, fhead, que, dlil_filterq_head) {
	    if (IFILT(tmp).filter_if_input) {
		retval = (*IFILT(tmp).filter_if_input)(IFILT(tmp).cookie,
						       &ifp,
						       &m,
						       &frame_header);
		if (retval) {
		    if (retval == EJUSTRETURN)
			return 0;
		    else {
			m_freem(m);
			return retval;
		    }
		}
	    }

	    if (ifp != orig_ifp)
		break;
	}
    }

    ifp->if_lastchange = time;
 
    /*
     * Call family demux module. If the demux module finds a match
     * for the frame it will fill-in the ifproto pointer.
     */

    retval = (*ifp->if_demux)(ifp, m, frame_header, &ifproto );

    if (m->m_flags & (M_BCAST|M_MCAST))
	ifp->if_imcasts++;
    
    if ((retval) && (ifp->offercnt)) {
	/*
	 * No match was found, look for any offers.
	 */
	struct dlil_proto_head	*tmp = (struct dlil_proto_head *) &ifp->proto_head;
	TAILQ_FOREACH(proto, tmp, next) {
	    if ((proto->dl_offer) && (proto->dl_offer(m, frame_header) == 0)) {
		ifproto = proto;
		retval = 0;
		break;
	    }
	}
    }

    if (retval) {
	if (retval != EJUSTRETURN) {
	    m_freem(m);
	    return retval;
	}
	else
	    return 0;
    } 
    else
	if (ifproto == 0) {
	    printf("ERROR - dlil_input - if_demux didn't return an if_proto pointer\n");
	    m_freem(m);
	    return 0;
	}

/*
 * Call any attached protocol filters.
 */

    TAILQ_FOREACH_REVERSE(tmp, &ifproto->pr_flt_head, que, dlil_filterq_head) { 
	if (PFILT(tmp).filter_dl_input) { 
	    retval = (*PFILT(tmp).filter_dl_input)(PFILT(tmp).cookie, 
						   &m,	
						   &frame_header,
						   &ifp);

	    if (retval) {
		if (retval == EJUSTRETURN)
		    return 0;
		else {
		    m_freem(m);
		    return retval;
		}
	    }
	} 
    }		  



    retval = (*ifproto->dl_input)(m, frame_header, 
				  ifp, ifproto->dl_tag, 
				  (ifp->if_eflags & IFEF_DVR_REENTRY_OK)); 
    
    if (retval == EJUSTRETURN)
	retval = 0;
    else 
	if (retval)
	    m_freem(m);

    return retval;
}



void ether_input(ifp, eh, m)
    struct ifnet *ifp;
    struct ether_header	 *eh;
    struct mbuf		 *m;

{
    kprintf("Someone is calling ether_input!!\n");

    dlil_input(ifp, m, (char *) eh);
}


int
dlil_event(struct ifnet *ifp, struct event_msg *event)
{
    return 0;
}



int
dlil_output(u_long		dl_tag,
	    struct mbuf		*m,
	    caddr_t		route,
	    struct sockaddr	*dest,
	    int			raw
	    )
{
    char			 *frame_type;
    char			 *dst_linkaddr;
    struct ifnet		 *orig_ifp = 0;
    struct ifnet		 *ifp;
    struct if_proto		 *proto;
    struct dlil_filterq_entry	 *tmp;
    int				 retval = 0;
    char			 frame_type_buffer[MAX_FRAME_TYPE_SIZE * 4];
    char			 dst_linkaddr_buffer[MAX_LINKADDR * 4];
    struct dlil_filterq_head	 *fhead;




    /*
     * Temporary hackery until all the existing protocols can become fully
     * "dl_tag aware". Some things only have the ifp, so this handles that
     * case for the time being.
     */

    if (dl_tag > MAX_DL_TAGS) {
	/* dl_tag is really an ifnet pointer! */

	ifp = (struct ifnet *) dl_tag;
	dl_tag = ifp->if_data.default_proto;
	if (dl_tag)
	    proto = dl_tag_array[dl_tag].proto;
    }
    else {
	if ((dl_tag == 0) || (dl_tag_array[dl_tag].ifp == 0))
	    retval = ENOENT;
	else {
	    ifp = dl_tag_array[dl_tag].ifp;
	    proto = dl_tag_array[dl_tag].proto;
	}
    }
    
    if (retval) {
	m_freem(m);
	return retval;
    }

    frame_type	   = frame_type_buffer;
    dst_linkaddr   = dst_linkaddr_buffer;

    fhead = (struct dlil_filterq_head *) &ifp->if_flt_head;

    if ((raw == 0) && (proto->dl_pre_output)) {
	retval = (*proto->dl_pre_output)(ifp, &m, dest, route, 
					 frame_type, dst_linkaddr, dl_tag);
	if (retval) {
	    if (retval == EJUSTRETURN)
		return 0;
	    else {
		m_freem(m);
		return retval;
	    }
	}
    }
    
/*
 * Run any attached protocol filters.
 */

    if (TAILQ_EMPTY(dl_tag_array[dl_tag].pr_flt_head) == 0) {
	TAILQ_FOREACH(tmp, dl_tag_array[dl_tag].pr_flt_head, que) {
	    if (PFILT(tmp).filter_dl_output) {
		retval = (*PFILT(tmp).filter_dl_output)(PFILT(tmp).cookie, 
							 &m, &ifp, &dest, dst_linkaddr, frame_type);
		if (retval) {
		    if (retval == EJUSTRETURN)
			return 0;
		    else {
			m_freem(m);
			return retval;
		    }
		}
	    }
	}
    }


/*
 * Call framing module 
 */
    if ((raw == 0) && (ifp->if_framer)) {
	retval = (*ifp->if_framer)(ifp, &m, dest, dst_linkaddr, frame_type);
	if (retval) {
	    if (retval == EJUSTRETURN)
		return 0;
	    else
	    {
		m_freem(m);
		return retval;
	    }
	}
    }

#if BRIDGE
    if (do_bridge) {
	struct mbuf *m0 = m ;
	
	if (m->m_pkthdr.rcvif)
	    m->m_pkthdr.rcvif = NULL ;
	ifp = bridge_dst_lookup(m);
	bdg_forward(&m0, ifp);
	if (m0)
	    m_freem(m0);

	return 0;
    }
#endif


/* 
 * Let interface filters (if any) do their thing ...
 */

    fhead = (struct dlil_filterq_head *) &ifp->if_flt_head;
    if (TAILQ_EMPTY(fhead) == 0) {
	while (orig_ifp != ifp) {
	    orig_ifp = ifp;
	    TAILQ_FOREACH(tmp, fhead, que) {
		if (IFILT(tmp).filter_if_output) {
		    retval = (*IFILT(tmp).filter_if_output)(IFILT(tmp).cookie,
							     &ifp,
							     &m);
		    if (retval) {
			if (retval == EJUSTRETURN)
			    return 0;
			else {
			    m_freem(m);
			    return retval;
			}
		    }

		}
		
		if (ifp != orig_ifp)
		    break;
	    }
	}
    }

/*
 * Finally, call the driver.
 */
    if (m->m_hdr.mh_type == 0) {
	printf("ERROR - dlil_output - sending free mbuf\n");
	m_freem(m);
	return 0;
    }
    /*
     * Temporary, until all drivers do this.
     */

    ifp->if_obytes += m->m_pkthdr.len;

    retval = (*ifp->if_output)(ifp, m);
    if ((retval == 0) || (retval == EJUSTRETURN))
	return 0;
    else 
	return retval;
}


int
dlil_ioctl(u_long	dl_tag,
	   struct ifnet *ifp,
	   u_long	ioctl_code,
	   caddr_t	ioctl_arg)
{
    struct dlil_filterq_entry	 *tmp;
    struct dlil_filterq_head	 *fhead;
    int				 retval;


    if (dl_tag) {
	if (dl_tag > MAX_DL_TAGS)
	    return ERANGE;

	if (dl_tag_array[dl_tag].ifp == 0)
	    return ENOENT;

	
/*
 * Run any attached protocol filters.
 */
	TAILQ_FOREACH(tmp, dl_tag_array[dl_tag].pr_flt_head, que) {
	    if (PFILT(tmp).filter_dl_ioctl) {
		retval = 
		    (*PFILT(tmp).filter_dl_ioctl)(PFILT(tmp).cookie, 
						  dl_tag_array[dl_tag].ifp,
						  ioctl_code, 
						  ioctl_arg);
								   
		if (retval) {
		    return 0;
		}
	    }
	}

	if (dl_tag_array[dl_tag].proto->dl_ioctl)
	    retval =  (*dl_tag_array[dl_tag].proto->dl_ioctl)(dl_tag,
							      dl_tag_array[dl_tag].ifp, 
							      ioctl_code, 
							      ioctl_arg);

	else
	    retval = EINVAL;
    }
    else {
	if (ifp) {
	    fhead = (struct dlil_filterq_head *) &ifp->if_flt_head;
	    TAILQ_FOREACH(tmp, fhead, que) {
		if (IFILT(tmp).filter_if_ioctl) {
		    retval = (*IFILT(tmp).filter_if_ioctl)(IFILT(tmp).cookie, ifp,
							   ioctl_code, ioctl_arg);
		    if (retval) {
			return 0;
		    }
		}
	    }
	    if (ifp->if_ioctl)
		retval = (*ifp->if_ioctl)(ifp, ioctl_code, ioctl_arg);
	    else
		return 0;
	}
	else
	    retval =  EINVAL;
    }

    return retval;
}


int
dlil_attach_protocol(struct dlil_proto_reg_str	 *proto,
		     u_long			 *dl_tag)
{
    struct ifnet     *ifp;
    struct if_proto  *ifproto;
    u_long	     i;
    struct if_family_str *if_family;
    int		     error;
    struct dlil_proto_head  *tmp;
    int	 s;


    if ((proto->protocol_family == 0) || (proto->interface_family == 0))
	return EINVAL;

    s = splnet();
    if_family = find_family_module(proto->interface_family);
    if ((!if_family) || (if_family->flags & DLIL_SHUTDOWN)) {
	kprintf("dlil_attach_protocol -- no interface family module %d", 
	       proto->interface_family);
	splx(s);
	return ENOENT;
    }

    ifp = ifbyfamily(proto->interface_family, proto->unit_number);
    if (!ifp) {
	kprintf("dlil_attach_protocol -- no such interface %d unit %d\n", 
	       proto->interface_family, proto->unit_number);
	splx(s);
	return ENOENT;
    }

    if (dlil_find_dltag(proto->interface_family, proto->unit_number,
			proto->protocol_family, &i) == 0)
	return EEXIST;

    for (i=1; i < MAX_DL_TAGS; i++)
	if (dl_tag_array[i].ifp == 0)
	    break;

    if (i >= MAX_DL_TAGS) {
	splx(s);
	return EOVERFLOW;
    }

    /*
     * Allocate and init a new if_proto structure
     */

    ifproto = _MALLOC(sizeof(struct if_proto), M_IFADDR, M_NOWAIT);
    if (!ifproto) {
	printf("ERROR - DLIL failed if_proto allocation\n");
	return ENOMEM;
    }
    
    bzero(ifproto, sizeof(struct if_proto));

    dl_tag_array[i].ifp = ifp;
    dl_tag_array[i].proto = ifproto;
    dl_tag_array[i].pr_flt_head = &ifproto->pr_flt_head;
    ifproto->dl_tag = i;
    *dl_tag = i;

    if (proto->default_proto) {
	if (ifp->if_data.default_proto == 0)
	    ifp->if_data.default_proto = i;
	else 
	    printf("ERROR - dlil_attach_protocol -- Attempt to attach more than one default protocol\n");
    }

    ifproto->protocol_family	= proto->protocol_family;
    ifproto->dl_input		= proto->input;
    ifproto->dl_pre_output	= proto->pre_output;
    ifproto->dl_event		= proto->event;
    ifproto->dl_offer		= proto->offer;
    ifproto->dl_ioctl		= proto->ioctl;
    ifproto->ifp		= ifp;
    TAILQ_INIT(&ifproto->pr_flt_head);

    /*
     * Call family module add_proto routine so it can refine the
     * demux descriptors as it wishes.
     */
    error = (*if_family->add_proto)(&proto->demux_desc_head, ifproto, *dl_tag);
    if (error) {
	dl_tag_array[*dl_tag].ifp = 0;
	FREE(ifproto, M_IFADDR);
	splx(s);
	return error;
    }
    

    /*
     * Add to if_proto list for this interface
     */

    tmp = (struct dlil_proto_head *) &ifp->proto_head;
    TAILQ_INSERT_TAIL(tmp, ifproto, next);
    ifp->refcnt++;
    if (ifproto->dl_offer)
	ifp->offercnt++;

    splx(s);
    return 0;
}



int
dlil_detach_protocol(u_long	dl_tag)
{
    struct ifnet    *ifp;
    struct ifnet    *orig_ifp=0;
    struct if_proto *proto;
    struct dlil_proto_head  *tmp; 
    struct if_family_str   *if_family;
    struct dlil_filterq_entry *filter;
    int s, retval;
    struct dlil_filterq_head *fhead;



    if (dl_tag > MAX_DL_TAGS) 
	return ERANGE;

    s = splnet();
    if (dl_tag_array[dl_tag].ifp == 0) {
	splx(s);
	return ENOENT;
    }

    ifp = dl_tag_array[dl_tag].ifp;
    proto = dl_tag_array[dl_tag].proto;

    if_family = find_family_module(ifp->if_family);
    if (if_family == NULL)
	 return ENOENT;

    tmp = (struct dlil_proto_head *) &ifp->proto_head;

    /*
     * Call family module del_proto
     */

    (*if_family->del_proto)(proto, dl_tag);


    /*
     * Remove and deallocate any attached protocol filters
     */

    while (filter = TAILQ_FIRST(&proto->pr_flt_head)) 
	dlil_detach_filter(filter->filter_id);
    
    if (proto->dl_offer)
	ifp->offercnt--;

    dl_tag_array[dl_tag].ifp = 0;

    TAILQ_REMOVE(tmp, proto, next);
    FREE(proto, M_IFADDR);

    if (--ifp->refcnt == 0) {
	if (ifp->if_flags & IFF_UP) 
	    printf("WARNING - dlil_detach_protocol - ifp refcnt 0, but IF still up\n");

	TAILQ_REMOVE(&ifnet, ifp, if_link);

	if (if_family->del_if)
	    (*if_family->del_if)(ifp);
	

	fhead = (struct dlil_filterq_head *) &ifp->if_flt_head;
	while (orig_ifp != ifp) {
	    orig_ifp = ifp;

	    TAILQ_FOREACH(filter, fhead, que) {
		if (IFILT(filter).filter_if_free) {
		    retval = (*IFILT(filter).filter_if_free)(IFILT(filter).cookie, ifp);
		    if (retval) {
			splx(s);
			return 0;
		    }
		}
		if (ifp != orig_ifp)
		    break;
	    }
	}
	
	(*ifp->if_free)(ifp);
    }

    splx(s);
    return 0;
}





int
dlil_if_attach(struct ifnet	*ifp)
{
    u_long		    interface_family = ifp->if_family;
    struct if_family_str    *if_family;
    struct dlil_proto_head  *tmp;
    int			    stat;
    int s;



    s = splnet();
    if (ifnet_inited == 0) {
	TAILQ_INIT(&ifnet);
	ifnet_inited = 1;
    }

    if_family = find_family_module(interface_family);

    if ((!if_family) || (if_family->flags & DLIL_SHUTDOWN)) {
	splx(s);
	kprintf("Attempt to attach interface without family module - %d\n", 
	       interface_family);
	return ENODEV;
    }


    /*
     * Call the family module to fill in the appropriate fields in the
     * ifnet structure.
     */

    stat = (*if_family->add_if)(ifp);
    if (stat) {
	splx(s);
	kprintf("dlil_if_attach -- add_if failed with %d\n", stat);
	return stat;
    }

    /*
     * Add the ifp to the interface list.
     */

    tmp = (struct dlil_proto_head *) &ifp->proto_head;
    TAILQ_INIT(tmp);
    
    ifp->if_data.default_proto = 0;
    ifp->refcnt = 1;
    ifp->offercnt = 0;
    TAILQ_INIT(&ifp->if_flt_head);
    old_if_attach(ifp);
    if_family->refcnt++;

    splx(s);
    return 0;
}


int
dlil_if_detach(struct ifnet *ifp)
{
    struct if_proto  *proto;
    struct dlil_filterq_entry *if_filter;
    struct if_family_str    *if_family;
    struct dlil_filterq_head *fhead = (struct dlil_filterq_head *) &ifp->if_flt_head;
    int s;

    s = splnet();
    if (ifp->if_flags & IFF_UP)
	printf("WARNING - dlil_if_detach called for UP interface\n");

    if_family = (struct if_family_str *) ifp->if_family;

    if (!if_family) {
	kprintf("Attempt to detach interface without family module - %s\n", 
	       ifp->if_name);
	splx(s);
	return ENODEV;
    }

    while (if_filter = TAILQ_FIRST(fhead)) 
	   dlil_detach_filter(if_filter->filter_id);

    if (--ifp->refcnt == 0) {
	TAILQ_REMOVE(&ifnet, ifp, if_link);
	
	if (if_family->del_if)
	    (*if_family->del_if)(ifp);

	if (--if_family->refcnt == 0) {
	    if (if_family->shutdown)
		(*if_family->shutdown)();
	    
	    TAILQ_REMOVE(&if_family_head, if_family, if_fam_next);
	    FREE(if_family, M_IFADDR);
	}
	splx(s);
	return 0;
    }
    else
    {
	splx(s);
	return DLIL_WAIT_FOR_FREE;
    }
}


int
dlil_reg_if_modules(u_long  interface_family,
		    int (*add_if)(struct ifnet	  *ifp),
		    int (*del_if)(struct ifnet	  *ifp),
		    int (*add_proto)(struct ddesc_head_str *head,
				     struct if_proto  *proto, u_long dl_tag),
		    int (*del_proto)(struct if_proto  *proto, u_long dl_tag),
		    int (*shutdown)())
{
    struct if_family_str *if_family;
    int s;


    s = splnet();
    if (find_family_module(interface_family))  {
	kprintf("Attempt to register dlil family module more than once - %d\n", 
	       interface_family);
	splx(s);
	return EEXIST;
    }

    if ((!add_if) || (!del_if) ||
	(!add_proto) || (!del_proto)) {
	kprintf("dlil_reg_if_modules passed at least one null pointer\n");
	splx(s);
	return EINVAL;
    }

    if_family = (struct if_family_str *) _MALLOC(sizeof(struct if_family_str), M_IFADDR, M_NOWAIT);
    if (!if_family) {
	kprintf("dlil_reg_if_modules failed allocation\n");
	splx(s);
	return ENOMEM;
    }
    
    bzero(if_family, sizeof(struct if_family_str));

    if_family->if_family	= interface_family;
    if_family->shutdown		= shutdown;
    if_family->add_if		= add_if;
    if_family->del_if		= del_if;
    if_family->add_proto	= add_proto;
    if_family->del_proto	= del_proto;
    if_family->refcnt		= 1;
    if_family->flags		= 0;

    TAILQ_INSERT_TAIL(&if_family_head, if_family, if_fam_next);
    splx(s);
    return 0;
}

int dlil_dereg_if_modules(u_long interface_family)
{
    struct if_family_str  *if_family;
    int s;


    s = splnet();
    if_family = find_family_module(interface_family);
    if (if_family == 0) {
	splx(s);
	return ENOENT;
    }

    if (--if_family->refcnt) {
	if (if_family->shutdown)
	    (*if_family->shutdown)();
	
	TAILQ_REMOVE(&if_family_head, if_family, if_fam_next);
	FREE(if_family, M_IFADDR);
    }	
    else
	if_family->flags |= DLIL_SHUTDOWN;

    splx(s);
    return 0;
}
					    
	    



/*
 * Old if_attach no-op'ed function defined here for temporary backwards compatibility
 */

void if_attach(ifp)
    struct ifnet *ifp;
{
    dlil_if_attach(ifp);
}



int
dlil_inject_if_input(struct mbuf *m, char *frame_header, u_long from_id)
{
    struct ifnet		 *orig_ifp = 0;
    struct ifnet		 *ifp;
    struct if_proto		 *ifproto;
    struct if_proto		 *proto;
    struct dlil_filterq_entry	 *tmp;
    int				 retval = 0;
    struct dlil_filterq_head	 *fhead;
    int				 match_found;


    dlil_stats.inject_if_in1++;
    if (from_id > MAX_DLIL_FILTERS)
	return ERANGE;

    if (dlil_filters[from_id].type != DLIL_IF_FILTER)
	return ENOENT;

    ifp = dlil_filters[from_id].ifp;

/* 
 * Let interface filters (if any) do their thing ...
 */

    fhead = (struct dlil_filterq_head *) &ifp->if_flt_head;
    if (from_id == DLIL_NULL_FILTER)
	match_found = 1;
    else
	match_found = 0;

    if (TAILQ_EMPTY(fhead) == 0) {
	while (orig_ifp != ifp) {
	    orig_ifp = ifp;
	    TAILQ_FOREACH_REVERSE(tmp, fhead, que, dlil_filterq_head) {
		if ((match_found) && (IFILT(tmp).filter_if_input)) {
		    retval = (*IFILT(tmp).filter_if_input)(IFILT(tmp).cookie,
							   &ifp,
							   &m,
							   &frame_header);
		    if (retval) {
			if (retval == EJUSTRETURN)
			    return 0;
			else {
			    m_freem(m);
			    return retval;
			}
		    }
		    
		}
		
		if (ifp != orig_ifp)
		    break;
		
		if (from_id == tmp->filter_id)
		    match_found = 1;
	    }
	}
    }

    ifp->if_lastchange = time;

    /*
     * Call family demux module. If the demux module finds a match
     * for the frame it will fill-in the ifproto pointer.
     */
 
    retval = (*ifp->if_demux)(ifp, m, frame_header, &ifproto );

    if (m->m_flags & (M_BCAST|M_MCAST))
	ifp->if_imcasts++;
    
    if ((retval) && (ifp->offercnt)) {
	/*
	 * No match was found, look for any offers.
	 */
	struct dlil_proto_head	*tmp = (struct dlil_proto_head *) &ifp->proto_head;
	TAILQ_FOREACH(proto, tmp, next) {
	    if ((proto->dl_offer) && (proto->dl_offer(m, frame_header) == 0)) {
		ifproto = proto;
		retval = 0;
		break;
	    }
	}
    }

    if (retval) {
	if (retval != EJUSTRETURN) {
	    m_freem(m);
	    return retval;
	}
	else
	    return 0;
    } 
    else
	if (ifproto == 0) {
	    printf("ERROR - dlil_inject_if_input -- if_demux didn't return an if_proto pointer\n");
	    m_freem(m);
	    return 0;
	}
    
/*
 * Call any attached protocol filters.
 */
    TAILQ_FOREACH_REVERSE(tmp, &ifproto->pr_flt_head, que, dlil_filterq_head) { 
	if (PFILT(tmp).filter_dl_input) { 
	    retval = (*PFILT(tmp).filter_dl_input)(PFILT(tmp).cookie, 
						   &m,	
						   &frame_header,
						   &ifp);

	    if (retval) {
		if (retval == EJUSTRETURN)
		    return 0;
		else {
		    m_freem(m);
		    return retval;
		}
	    }
	} 
    }		  



    retval = (*ifproto->dl_input)(m, frame_header, 
				  ifp, ifproto->dl_tag, 
				  (ifp->if_eflags & IFEF_DVR_REENTRY_OK)); 
    
    dlil_stats.inject_if_in2++;
    if (retval == EJUSTRETURN)
	retval = 0;
    else 
	if (retval)
	    m_freem(m);

    return retval;

}





int
dlil_inject_pr_input(struct mbuf *m, char *frame_header, u_long from_id)
{
    struct ifnet		 *orig_ifp = 0;
    struct dlil_filterq_entry	 *tmp;
    int				 retval;
    struct if_proto		 *ifproto = 0;
    int				 match_found;
    struct ifnet		 *ifp;
    

    dlil_stats.inject_pr_in1++;
    if (from_id > MAX_DLIL_FILTERS)
	return ERANGE;

    if (dlil_filters[from_id].type != DLIL_PR_FILTER)
	return ENOENT;

    ifproto = dlil_filters[from_id].proto;
    ifp	  = dlil_filters[from_id].ifp;


/*
 * Call any attached protocol filters.
 */

    if (from_id == DLIL_NULL_FILTER)
	match_found = 1;
    else
	match_found = 0;
    TAILQ_FOREACH_REVERSE(tmp, &ifproto->pr_flt_head, que, dlil_filterq_head) { 
	if ((match_found) && (PFILT(tmp).filter_dl_input)) { 
	    retval = (*PFILT(tmp).filter_dl_input)(PFILT(tmp).cookie, 
						   &m,	
						   &frame_header,
						   &ifp);

	    if (retval) {
		if (retval == EJUSTRETURN)
		    return 0;
		else {
		    m_freem(m);
		    return retval;
		}
	    }
	} 
	
	if (tmp->filter_id == from_id)
	    match_found = 1;
    }		  
    
    
    retval = (*ifproto->dl_input)(m, frame_header, 
				  ifp, ifproto->dl_tag, 
				  (ifp->if_eflags & IFEF_DVR_REENTRY_OK)); 
    
    if (retval == EJUSTRETURN)
	retval = 0;
    else 
	if (retval)
	    m_freem(m);

    dlil_stats.inject_pr_in2++;
    return retval;
}



int
dlil_inject_pr_output(struct mbuf		*m,
		      struct sockaddr		*dest,
		      int			raw, 
		      char			*frame_type,
		      char			*dst_linkaddr,
		      u_long			from_id)
{
    struct ifnet		 *orig_ifp = 0;
    struct ifnet		 *ifp;
    struct dlil_filterq_entry	 *tmp;
    int				 retval = 0;
    char			 frame_type_buffer[MAX_FRAME_TYPE_SIZE * 4];
    char			 dst_linkaddr_buffer[MAX_LINKADDR * 4];
    struct dlil_filterq_head	 *fhead;
    int				 match_found;
    u_long			 dl_tag;


    dlil_stats.inject_pr_out1++;
    if (raw == 0) { 
	if (frame_type)
	    bcopy(frame_type, &frame_type_buffer[0], MAX_FRAME_TYPE_SIZE * 4);
	else
	    return EINVAL;

	if (dst_linkaddr)
	    bcopy(dst_linkaddr, &dst_linkaddr_buffer, MAX_LINKADDR * 4);
	else
	    return EINVAL;
    }

    if (from_id > MAX_DLIL_FILTERS)
	return ERANGE;

    if (dlil_filters[from_id].type != DLIL_PR_FILTER)
	return ENOENT;

    ifp	  = dlil_filters[from_id].ifp;
    dl_tag = dlil_filters[from_id].proto->dl_tag;
    

    frame_type	   = frame_type_buffer;
    dst_linkaddr   = dst_linkaddr_buffer;

    fhead = (struct dlil_filterq_head *) &ifp->if_flt_head;
    
/*
 * Run any attached protocol filters.
 */
    if (from_id == DLIL_NULL_FILTER)
	match_found = 1;
    else
	match_found = 0;

    if (TAILQ_EMPTY(dl_tag_array[dl_tag].pr_flt_head) == 0) {
	TAILQ_FOREACH(tmp, dl_tag_array[dl_tag].pr_flt_head, que) {
	    if ((match_found) && (PFILT(tmp).filter_dl_output)) {
		retval = (*PFILT(tmp).filter_dl_output)(PFILT(tmp).cookie, 
							 &m, &ifp, &dest, dst_linkaddr, frame_type);
		if (retval) {
		    if (retval == EJUSTRETURN)
			return 0;
		    else {
			m_freem(m);
			return retval;
		    }
		}
	    }

	    if (tmp->filter_id == from_id)
		match_found = 1;
	}
    }


/*
 * Call framing module 
 */
    if ((raw == 0) && (ifp->if_framer)) {
	retval = (*ifp->if_framer)(ifp, &m, dest, dst_linkaddr, frame_type);
	if (retval) {
	    if (retval == EJUSTRETURN)
		return 0;
	    else
	    {
		m_freem(m);
		return retval;
	    }
	}
    }
    

#if BRIDGE
    if (do_bridge) {
	struct mbuf *m0 = m ;
	
	if (m->m_pkthdr.rcvif)
	    m->m_pkthdr.rcvif = NULL ;
	ifp = bridge_dst_lookup(m);
	bdg_forward(&m0, ifp);
	if (m0)
	    m_freem(m0);

	return 0;
    }
#endif


/* 
 * Let interface filters (if any) do their thing ...
 */

    fhead = (struct dlil_filterq_head *) &ifp->if_flt_head;
    if (TAILQ_EMPTY(fhead) == 0) {
	while (orig_ifp != ifp) {
	    orig_ifp = ifp;
	    TAILQ_FOREACH(tmp, fhead, que) {
		if (IFILT(tmp).filter_if_output) {
		    retval = (*IFILT(tmp).filter_if_output)(IFILT(tmp).cookie,
							     &ifp,
							     &m);
		    if (retval) {
			if (retval == EJUSTRETURN)
			    return 0;
			else {
			    m_freem(m);
			    return retval;
			}
		    }

		}
		
		if (ifp != orig_ifp)
		    break;
	    }
	}
    }

/*
 * Finally, call the driver.
 */
    /*
     * Temporary, until all drivers do this.
     */

    ifp->if_obytes += m->m_pkthdr.len;

    retval = (*ifp->if_output)(ifp, m);
    dlil_stats.inject_pr_out2++;
    if ((retval == 0) || (retval == EJUSTRETURN))
	return 0;
    else 
	return retval;
}


int
dlil_inject_if_output(struct mbuf *m, u_long from_id)
{
    struct ifnet		 *orig_ifp = 0;
    struct ifnet		 *ifp;
    struct dlil_filterq_entry	 *tmp;
    int				 retval = 0;
    struct dlil_filterq_head	 *fhead;
    int				 match_found;


    dlil_stats.inject_if_out1++;
    if (from_id > MAX_DLIL_FILTERS)
	return ERANGE;

    if (dlil_filters[from_id].type != DLIL_IF_FILTER)
	return ENOENT;

    ifp = dlil_filters[from_id].ifp;

/* 
 * Let interface filters (if any) do their thing ...
 */

    fhead = (struct dlil_filterq_head *) &ifp->if_flt_head;

    if (from_id == DLIL_NULL_FILTER)
	match_found = 1;
    else
	match_found = 0;

    if (TAILQ_EMPTY(fhead) == 0) {
	while (orig_ifp != ifp) {
	    orig_ifp = ifp;
	    TAILQ_FOREACH(tmp, fhead, que) {
		if ((match_found) && (IFILT(tmp).filter_if_output)) {
		    retval = (*IFILT(tmp).filter_if_output)(IFILT(tmp).cookie,
							     &ifp,
							     &m);
		    if (retval) {
			if (retval == EJUSTRETURN)
			    return 0;
			else {
			    m_freem(m);
			    return retval;
			}
		    }

		}
		
		if (ifp != orig_ifp)
		    break;

		if (from_id == tmp->filter_id)
		    match_found = 1;
	    }
	}
    }

/*
 * Finally, call the driver.
 */
    /*
     * Temporary, until all drivers do this.
     */

    ifp->if_obytes += m->m_pkthdr.len;

    retval = (*ifp->if_output)(ifp, m);
    dlil_stats.inject_if_out2++;
    if ((retval == 0) || (retval == EJUSTRETURN))
	return 0;
    else 
	return retval;
}
