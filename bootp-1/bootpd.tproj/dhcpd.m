/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
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
 * dhcpd.m
 * - netinfo-aware DHCP server
 */

/* 
 * Modification History
 * June 17, 1998 	Dieter Siegmund (dieter@apple.com)
 * - initial revision
 */
#import <unistd.h>
#import <stdlib.h>
#import <sys/stat.h>
#import <sys/socket.h>
#import <sys/ioctl.h>
#import <sys/file.h>
#import <sys/time.h>
#import <net/if.h>
#import <netinet/in.h>
#import <netinet/in_systm.h>
#import <netinet/ip.h>
#import <netinet/udp.h>
#import <netinet/bootp.h>
#import <netinet/if_ether.h>
#import <syslog.h>
#import <arpa/inet.h>
#import <net/if_arp.h>
#import <mach/boolean.h>
#import "netinfo.h"
#import "dhcp.h"
#import "rfc_options.h"
#import "subnetDescr.h"
#import "dhcpOptions.h"
#import "dhcpOptionsPrivate.h"
#import "host_identifier.h"
#import "hostlist.h"
#import "interfaces.h"
#import "bootpd.h"
#import "bootpdfile.h"
#import "dhcpd.h"
#import "NIDomain.h"
#import "NIHosts.h"
#import "dhcplib.h"

#define MAX_RETRY	5

static boolean_t	S_extend_leases = TRUE;

static boolean_t
S_remove_host(id domain, ni_id child)
{
    int		i;
    ni_id 	parent;
    ni_status	status;

    status = ni_parent([domain handle], &child, &parent.nii_object);
    if (status != NI_OK) {
	syslog(LOG_ERR, "dhcpd: can't get parent in %s\n", 
	       [domain domain_name]);
	return (FALSE);
    }
    i = 0;
    do {
	ni_self([domain handle], &child);
	ni_self([domain handle], &parent);
	status = ni_destroy([domain handle], &parent, child);
	i++;
    } while (status == NI_STALE && i <= MAX_RETRY);
    if (status == NI_OK)
	return (TRUE);
    syslog(LOG_ERR, "dhcpd: failed to delete directory, %s",
	   ni_error(status));
    return (FALSE);
}

static void
S_set_prop(ni_proplist * pl_p, ni_name prop, ni_name value, int * modified)
{
    ni_index		where;
    
    where = ni_proplist_match(*pl_p, prop, NULL);
    if (where != NI_INDEX_NULL) {
	if (where == ni_proplist_match(*pl_p, prop, value)) {
	    if (debug)
		printf("property %s=%s already set\n", prop,
		       value ? value:"<no value>");
	    return; /* already set */
	}
	ni_proplist_delete(pl_p, where);
    }
    ni_proplist_insertprop(pl_p, prop, value, where);
    *modified = TRUE;
    return;
}

static void
S_delete_prop(ni_proplist * pl_p, ni_name prop, boolean_t * modified)
{
    int where;

    where = ni_proplist_match(*pl_p, prop, NULL);
    if (where != NI_INDEX_NULL) {
	ni_proplist_delete(pl_p, where);
	*modified = TRUE;
    }
    return;
}

static boolean_t
S_commit_mods(id domain, ni_id * dir_p, ni_proplist * pl_p)
{
    int			i;
    ni_status 		status;

    status = ni_write([domain handle], dir_p, *pl_p);
    i = 0;
    while (status == NI_STALE) { /* refresh and try again */
	if (++i == MAX_RETRY)
	    break;
	if (verbose)
	    syslog(LOG_INFO, 
		   "dhcpd: lease update got stale handle, retrying %d", i);
	ni_self([domain handle], dir_p);
	status = ni_write([domain handle], dir_p, *pl_p);
    }
    if (status != NI_OK) {
	syslog(LOG_ERR, "dhcpd: ni_write domain %s failed, %s",
	       [domain domain_name], ni_error(status));
	return (FALSE);
    }
    return (TRUE);
}

static __inline__ u_char *
S_lease_propval(ni_proplist * pl_p)
{
    return (ni_valforprop(pl_p, HOSTPROP__DHCP_LEASE));
}

#define LEASE_FORMAT	"0x%x"

static void
S_set_lease(ni_proplist * pl_p, dhcp_time_secs_t lease_expiry, boolean_t * mod)
{
    u_char buf[32];

    sprintf(buf, LEASE_FORMAT, lease_expiry);
    S_set_prop(pl_p, HOSTPROP__DHCP_LEASE, buf, mod);
    return;
}

#import <errno.h>

static dhcp_time_secs_t
S_lease_expiry(ni_proplist * pl_p)
{
    dhcp_time_secs_t 	expiry = DHCP_INFINITE_SECS;
    ni_name 		str = S_lease_propval(pl_p);

    if (str) {
	expiry = strtoul(str, NULL, NULL);
	if (expiry == ULONG_MAX && errno == ERANGE) {
	    syslog(LOG_INFO, "S_lease_expiry: lease '%s' bad", str);
	    return (0);
	}
    }
    return (expiry);
    
}

static dhcp_lease_t	max_lease_length = DHCP_MAX_LEASE;
static dhcp_lease_t	min_lease_length = DHCP_MIN_LEASE;

static void
lease_from_subnet(id subnet, dhcp_lease_t * min_lease, 
		  dhcp_lease_t * max_lease)
{
    ni_namelist * nl_p;

    nl_p = [subnet lookup:SUBNETPROP_LEASE_MIN];
    if (nl_p != NULL && nl_p->ninl_len) {
	*min_lease = (dhcp_lease_t) strtoul(nl_p->ninl_val[0], NULL, NULL);
	if (debug)
	    printf("min_lease is %d\n", *min_lease); 
    }

    nl_p = [subnet lookup:SUBNETPROP_LEASE_MAX];
    if (nl_p != NULL && nl_p->ninl_len) {
	*max_lease = (dhcp_lease_t) strtoul(nl_p->ninl_val[0], NULL, NULL);
	if (debug)
	    printf("max_lease is %d\n", *max_lease); 
    }
    return;
}

static struct dhcp * 
make_dhcp_reply(struct dhcp * reply, int pkt_size, 
		struct in_addr server_id, dhcp_msgtype_t msg, 
		struct dhcp * request, id * options_p)
{
    id options = nil;

    bzero(reply, pkt_size);
    *reply = *request;
    reply->dp_hops = 0;
    reply->dp_secs = 0;
    reply->dp_op = BOOTREPLY;
    bcopy(rfc_magic, reply->dp_options, sizeof(rfc_magic));
    options = [[dhcpOptions alloc] 
	       initWithBuffer:reply->dp_options + sizeof(rfc_magic) 
	       Size:DHCP_MIN_OPTIONS_SIZE - sizeof(rfc_magic)];
    if (options == nil) {
	syslog(LOG_INFO, "make_dhcp_reply: init options failed");
	goto err;
    }
    /* make the reply a dhcp message */
    if ([options dhcpMessage:msg] == FALSE) {
	syslog(LOG_INFO, 
	       "make_dhcp_reply: couldn't add dhcp message tag %d: %s", msg,
	       [options errString]);
	goto err;
    }
    /* add our server identifier */
    if ([options addOption:dhcptag_server_identifier_e
	Length:sizeof(server_id) Data:&server_id] == FALSE) {
	syslog(LOG_INFO, 
	       "make_dhcp_reply: couldn't add server identifier tag: %s",
	       [options errString]);
	goto err;
    }
    *options_p = options;
    return (reply);
  err:
    if (options != nil)
	[options free];
    *options_p = nil;
    return (NULL);
}

static __inline__ struct dhcp * 
make_dhcp_nak(struct dhcp * reply, int pkt_size, 
	      struct in_addr server_id, dhcp_msgtype_t * msg_p, 
	      char * nak_msg, struct dhcp * request, id * options_p)
{
    id options = nil;
    struct dhcp * r;

    if (debug)
	printf("sending a NAK: '%s'\n", nak_msg);
    r = make_dhcp_reply(reply, pkt_size, server_id, dhcp_msgtype_nak_e, 
			request, &options);
    if (r == NULL)
	return (NULL);

    r->dp_ciaddr.s_addr = 0;
    r->dp_yiaddr.s_addr = 0;

    if ([options addOption:dhcptag_message_e FromString:nak_msg] == FALSE) {
	syslog(LOG_INFO, "dhcpd: couldn't add NAK message type: %s",
	       [options errString]);
	goto err;
    }
    if ([options addOption:dhcptag_end_e Length:0 
		 Data:NULL] == FALSE) {
	syslog(LOG_INFO, "dhcpd: couldn't add end tag: %s",
	       [options errString]);
	goto err;
    }
    *msg_p = dhcp_msgtype_nak_e;
    *options_p = options;
    return (r);
 err:
    [options free];
    return (NULL);
}

static struct hosts *		S_pending_hosts = NULL;

#define DEFAULT_PENDING_SECS	60

static boolean_t
S_ipinuse(void * arg, struct in_addr ip)
{
    u_char * 		host;
    ni_proplist 	pl;
    struct timeval * 	time_in_p = (struct timeval *)arg;
    
    if (lookup_host_by_ip(ip, &host, NULL, NULL, NULL, &pl)) {
	ni_proplist_free(&pl);
	if (verbose)
	    syslog(LOG_INFO, "dhcpd: %s is in use %s%s\n", inet_ntoa(ip),
		   host[0] ? "by " : "",
		   host[0] ? host : (u_char *) "");
	free(host);
	return (TRUE);
    }
    else {
	struct hosts * hp = hostbyip(S_pending_hosts, ip);
	if (hp) {
	    u_long pending_secs = time_in_p->tv_sec - hp->tv.tv_sec;

	    if (pending_secs < DEFAULT_PENDING_SECS) {
		if (verbose)
		    syslog(LOG_INFO, "dhcpd: %s will remain pending %d secs\n",
			   inet_ntoa(ip), DEFAULT_PENDING_SECS - pending_secs);
		return (TRUE);
	    }
	    hostfree(&S_pending_hosts, hp); /* remove it from the list */
	}
    }
    return (FALSE);
}

#define DHCPD_CREATOR		"dhcpd"

static __inline__ u_char *
S_host_by_addr(struct in_addr iaddr)
{
    struct hostent *	h;

    h = gethostbyaddr((char *)&iaddr, sizeof(iaddr), AF_INET);
    if (!h)
	return (NULL);
    return (h->h_name);
}

static __inline__ u_char *
S_host_from_ip(struct in_addr iaddr)
{
    u_char 	host[256];
    u_char * 	hostname;

    hostname = S_host_by_addr(iaddr);
    if (hostname == NULL) {
	sprintf(host, "dhcp%lx", ntohl(iaddr.s_addr));
	hostname = host;
    }
    return (strdup(hostname));
}

static boolean_t
S_create_host(id domain, char cid_type, void * cid, int cid_len, 
	      struct in_addr iaddr, u_char * * hostname, u_char * * bootfile, 
	      dhcp_time_secs_t lease_expiry)
{
    u_char		lease_str[128];
    boolean_t		ret;
    ni_proplist 	pl;

    /* add DHCP-specific properties */
    NI_INIT(&pl);
    sprintf(lease_str, LEASE_FORMAT, lease_expiry);
    ni_proplist_addprop(&pl, HOSTPROP__DHCP_LEASE, (ni_name)lease_str);
    ni_proplist_addprop(&pl, NIPROP__CREATOR, DHCPD_CREATOR);

    *hostname = S_host_from_ip(iaddr);
    *bootfile = NULL; /* XXX */
		  
    ret = (ni_createhost([domain handle], &pl, *hostname, 
			 cid_type, cid, cid_len,
			 iaddr, *bootfile, use_en_address) == 0);
    ni_proplist_free(&pl);
    return (ret);
}

#define TXBUF_SIZE	(sizeof(struct dhcp) + DHCP_MIN_OPTIONS_SIZE)
static char	txbuf[TXBUF_SIZE];

void
dhcp_request(dhcp_msgtype_t msgtype, interface_t * if_p,
	     u_char * rxpkt, int n, id rq_options, struct in_addr * dstaddr_p,
	     struct timeval * time_in_p)
{
    u_char *		bootfile = NULL;
    boolean_t		bound = FALSE;
    char		cid_type;
    int			cid_len;
    void *		cid;
    id			domain = nil;
    ni_id		dir;
    u_char *		hostname = NULL;
    int			index;
    u_char *		idstr;
    struct in_addr	iaddr;
    dhcp_time_secs_t	lease_expiry = 0;
    int			len;
    dhcp_lease_t	min_lease = min_lease_length;
    dhcp_lease_t	max_lease = max_lease_length;
    boolean_t		modified = FALSE;
    id			options = nil;
    ni_proplist		pl;
    struct dhcp *	reply = NULL;
    dhcp_msgtype_t	reply_msgtype = dhcp_msgtype_none_e;
    struct dhcp *	rq = (struct dhcp *)rxpkt;
    id 			subnet = nil;
    dhcp_lease_t *	suggested_lease = NULL;
    dhcp_cstate_t	state = dhcp_cstate_none_e;
    boolean_t		use_broadcast = FALSE;

    iaddr.s_addr = 0;

    /* check for a client identifier */
    cid = [rq_options findOptionWithTag:dhcptag_client_identifier_e
		      Length:&cid_len];
    if (cid && cid_len > 1) {
	/* use the client identifier as provided */
	cid_type = *((char *)cid);
	cid_len--;
	cid++;
    }
    else {
	/* use the hardware address as the identifier */
	cid = rq->dp_chaddr;
	cid_type = rq->dp_htype;
	cid_len = rq->dp_hlen;
    }

    idstr = identifierToString(cid_type, cid, cid_len);
    if (!quiet) {
	syslog(LOG_INFO, "DHCP %s [%s]: %s\n", 
	       dhcp_msgtype_names(msgtype), if_name(if_p), idstr);
	if (debug)
	    [rq_options print];
    }

    NI_INIT(&pl);
    domain = lookup_host(cid_type, cid, cid_len, rq->dp_giaddr, if_p,
			 &dir, &pl, &index);
    if (domain != nil) {
	if (index != -1) {
	    bound = TRUE;
	    host_parms_from_proplist(&pl, index, &iaddr, &hostname, &bootfile);
	    subnet = [subnets entry:iaddr];
	    lease_from_subnet(subnet, &min_lease, &max_lease);
	    lease_expiry = S_lease_expiry(&pl);
	    if (msgtype != dhcp_msgtype_release_e) /* undo any releases */
		S_delete_prop(&pl, HOSTPROP__DHCP_RELEASED, &modified);
	}
    }
    suggested_lease = 
	(dhcp_lease_t *)[rq_options findOptionWithTag:dhcptag_lease_time_e
				    Length:&len];

    switch (msgtype) {
      case dhcp_msgtype_discover_e: {
	  dhcp_lease_t		lease;

	  state = dhcp_cstate_init_e;

	  { /* delete the pending host entry */
	      struct hosts *	hp;
	      hp = hostbyaddr(S_pending_hosts, cid_type, cid, cid_len);
	      if (hp)
		  hostfree(&S_pending_hosts, hp);
	  }

	  if (bound) { /* client has a host entry already */
	      if (lease_expiry == DHCP_INFINITE_SECS) /* permanent entry */
		  lease = DHCP_INFINITE_LEASE;
	      else {
		  if (suggested_lease) {
		      lease = dhcp_lease_ntoh(*suggested_lease);
		      if (lease > max_lease)
			  lease = max_lease;
		      else if (lease < min_lease)
			  lease = min_lease;
		  }
		  else if (time_in_p->tv_sec >= lease_expiry) {
		      /* expired lease: give it the default lease */
		      lease = min_lease;
		  }
		  else { /* give the host the remaining time on the lease */
		      lease = lease_expiry - time_in_p->tv_sec;
		  }
	      }
	  }
	  else { /* find an ip address */
	      /* allocate a new ip address */
	      if (rq->dp_giaddr.s_addr)
		  iaddr = rq->dp_giaddr;
	      else
		  iaddr = if_p->netaddr;

	      subnet = [subnets acquireIpSupernet:&iaddr 
				ClientType:DHCP_CLIENT_TYPE Func:S_ipinuse 
				Arg:time_in_p];
	      if (subnet == nil) {
		  if (debug) {
		      printf("no ip addresses\n");
		  }
		  goto no_reply; /* out of ip addresses */
	      }
	      if ([[subnet domain] isMaster] == FALSE) {
		  if (debug) {
		      printf("Not master: can't lease %s", inet_ntoa(iaddr));
		  }
		  goto no_reply; /* not master */
	      }
	      lease_from_subnet(subnet, &min_lease, &max_lease);
	      domain = [subnet domain];
	      hostname = S_host_from_ip(iaddr);
	      if (suggested_lease) {
		  lease = dhcp_lease_ntoh(*suggested_lease);
		  if (lease > max_lease)
		      lease = max_lease;
		  else if (lease < min_lease)
		      lease = min_lease;
	      }
	      else 
		  lease = min_lease;
	  }
	  { /* keep track of this offer in the pending hosts list */
	      struct hosts *	hp;

	      hp = hostadd(&S_pending_hosts, time_in_p, cid_type, cid, cid_len,
			   &iaddr, NULL, NULL);
	      if (hp == NULL)
		  goto no_reply;
	      hp->lease = lease;
	  }
	  /*
	   * allow for drift between server/client clocks by offering
	   * a lease shorter than the recorded value
	   */
	  if (lease == DHCP_INFINITE_LEASE)
	      lease = dhcp_lease_hton(lease);
	  else
	      lease = dhcp_lease_hton(lease_prorate(lease));

	  /* form a reply */
	  reply = make_dhcp_reply((struct dhcp *)txbuf, sizeof(txbuf),
				  if_p->addr, 
				  reply_msgtype = dhcp_msgtype_offer_e,
				  rq, &options);
	  if (reply == NULL)
	      goto no_reply;
	  reply->dp_ciaddr.s_addr = 0;
	  reply->dp_yiaddr = iaddr;
	  if ([options addOption:dhcptag_lease_time_e
	       Length:sizeof(lease) Data:&lease] == FALSE) {
	      syslog(LOG_INFO, "dhcpd: couldn't add lease time tag: %s",
		     [options errString]);
	      goto no_reply;
	  }
	  break;
      }
      case dhcp_msgtype_request_e: {
	  dhcp_lease_t		lease = 0;
	  u_char * 		nak = NULL;
	  int			optlen;
	  struct in_addr * 	req_ip;
	  struct in_addr * 	server_id;

	  server_id = (struct in_addr *)
	      [rq_options findOptionWithTag:dhcptag_server_identifier_e
	       Length:&optlen];
	  req_ip = (struct in_addr *)
	      [rq_options findOptionWithTag:dhcptag_requested_ip_address_e
	       Length:&optlen];
	  if (server_id) { /* SELECT */
	      struct hosts *	hp = hostbyaddr(S_pending_hosts, cid_type,
						cid, cid_len);
	      if (debug)
		  printf("SELECT\n");
	      state = dhcp_cstate_select_e;

	      if (server_id->s_addr != if_p->addr.s_addr) {
		  if (debug)
		      printf("chose %s over us\n", inet_ntoa(*server_id));
		  /* clean up */
		  if (hp)
		      hostfree(&S_pending_hosts, hp);
		  
		  if (bound && lease_expiry != DHCP_INFINITE_SECS) {
		      if ([domain isMaster]) {
			  /* this needs to be revisited if #servers > 1 */
			  S_remove_host(domain, dir);
		      }
		      bound = FALSE;
		  }
		  goto no_reply;
	      }
	      if (hp == NULL || req_ip == NULL 
		  || req_ip->s_addr != hp->iaddr.s_addr) {
		  if (hp && req_ip) {
		      syslog(LOG_INFO, 
			     "dhcpd: host %s sends SELECT with wrong"
			     " IP address %s, should be " IP_FORMAT,
			     idstr, inet_ntoa(*req_ip), IP_LIST(&iaddr));
		  }
		  else {
		      if (hp == NULL) {
			  syslog(LOG_INFO,
				 "dhcpd: host %s sends SELECT without OFFER",
				 idstr);
		      }
		      if (req_ip == NULL) {
			  syslog(LOG_INFO,
				 "dhcpd: host %s sends SELECT without"
				 " Requested IP option", idstr);
		      }
		  }
		  use_broadcast = TRUE;
		  reply = make_dhcp_nak((struct dhcp *)txbuf, sizeof(txbuf),
					if_p->addr, &reply_msgtype, 
					"protocol error in SELECT state",
					rq, &options);
		  if (reply)
		      goto reply;
		  goto no_reply;
	      }

	      iaddr = hp->iaddr;

	      if (bound) { /* check whether we need to update the lease */
		  /* if the lease is non-permanent, or used to be
		   * non-permanent and is becoming permanent,
		   * update the lease
		   */
		  if (hp->lease != DHCP_INFINITE_LEASE
		      || (lease_expiry != DHCP_INFINITE_LEASE)) {
		      if (hp->lease == DHCP_INFINITE_LEASE)
			  lease_expiry = DHCP_INFINITE_LEASE;
		      else
			  lease_expiry = hp->lease + time_in_p->tv_sec;
		      S_set_lease(&pl, lease_expiry, &modified);
		  }
	      }
	      else { /* create a new host entry */
		  id subnet;
		  
		  if (hp->lease == DHCP_INFINITE_LEASE)
		      lease_expiry = DHCP_INFINITE_LEASE;
		  else
		      lease_expiry = hp->lease + time_in_p->tv_sec;

		  subnet = [subnets entry:iaddr];
		  domain = [subnet domain];
		  if (subnet == nil
		      || S_create_host(domain, cid_type, cid, cid_len,
				       iaddr, &hostname,
				       &bootfile, lease_expiry) == FALSE) {
		      hostfree(&S_pending_hosts, hp);
		      hp = NULL;
		      reply = make_dhcp_nak((struct dhcp *)txbuf, 
					    sizeof(txbuf),
					    if_p->addr, &reply_msgtype, 
					    "unexpected server failure",
					    rq, &options);
		      if (reply)
			  goto reply;
		      goto no_reply;
		  }
	      }
	      hostfree(&S_pending_hosts, hp);
	      lease = hp->lease;
	  } /* select */
	  else { /* init-reboot/renew/rebind */
	      if (debug)
		  printf("init-reboot/renew/rebind\n");
	      if (req_ip) { /* init-reboot */
		  if (debug) 
		      printf("init-reboot\n");
		  state = dhcp_cstate_init_reboot_e;
		  if (bound == FALSE) {
		      if (domain == nil) {
			  if (verbose)
			      syslog(LOG_INFO, "dhcpd: INIT-REBOOT host "
				     "%s has no binding for %s",
				     idstr, inet_ntoa(*req_ip));
			  goto no_reply;
		      }
		      /* has a binding, but it is on a different subnet */
		      nak = "changed subnets - INIT_REBOOT";
		  }
		  else if (req_ip->s_addr != iaddr.s_addr) {
		      nak = "requested address incorrect";
		  }
		  if (nak) {
		      /* must broadcast since client's notion of IP is wrong */
		      use_broadcast = TRUE;
		      goto send_nak;
		  }

	      } /* init-reboot */
	      else if (rq->dp_ciaddr.s_addr) { /* renew/rebind */
		  if (debug) {
		      if (dstaddr_p == NULL 
			  || ntohl(dstaddr_p->s_addr) == INADDR_BROADCAST)
			  printf("rebind\n");
		      else
			  printf("renew\n");
		  }
		  if (bound == FALSE) {
		      if (debug) {
			  if (domain != nil)
			      printf("Client binding is not applicable\n");
			  else
			      printf("No binding for client\n");
		      }
		      goto no_reply;
		  }
		  if (dstaddr_p == NULL 
		      || ntohl(dstaddr_p->s_addr) == INADDR_BROADCAST
		      || rq->dp_giaddr.s_addr) { /* REBIND */
		      state = dhcp_cstate_rebind_e;
		      if (rq->dp_ciaddr.s_addr != iaddr.s_addr) {
			  if (debug)
			      printf("Incorrect ciaddr " IP_FORMAT 
				     " should be " IP_FORMAT "\n",
				     IP_LIST(&rq->dp_ciaddr), 
				     IP_LIST(&iaddr));
			  goto no_reply;
		      }
		  }
		  else { /* RENEW */
		      state = dhcp_cstate_renew_e;
		      if (rq->dp_ciaddr.s_addr != iaddr.s_addr) {
			  syslog(LOG_INFO, 
				 "dhcpd: client ciaddr=%s should use "
				 IP_FORMAT, inet_ntoa(rq->dp_ciaddr), 
				 IP_LIST(&iaddr));
			  iaddr = rq->dp_ciaddr; /* trust it anyways */
		      }
		  }
	      } /* renew/rebind */
	      else {
		  if (verbose)
		      syslog(LOG_INFO,
			     "dhcpd: host %s in unknown state", idstr);
		  goto no_reply;
	      }

	      if (lease_expiry == DHCP_INFINITE_LEASE)
		  lease = DHCP_INFINITE_LEASE;
	      else if ([domain isMaster] == FALSE) {
		  nak = "server not authorized";
		  goto send_nak;
	      }
	      else if (suggested_lease || S_extend_leases) {
		  if (suggested_lease) {
		      lease = dhcp_lease_ntoh(*suggested_lease);
		      if (lease > max_lease)
			  lease = max_lease;
		      else if (lease < min_lease)
			  lease = min_lease;
		  }
		  else {
		      /* automatically extend the lease */
		      lease = min_lease;
		      if (verbose)
			  syslog(LOG_INFO, 
				 "dhcpd: %s lease extended to %s client",
				 inet_ntoa(iaddr), dhcp_cstate_str(state));
		  }
		  if (lease == DHCP_INFINITE_LEASE)
		      lease_expiry = DHCP_INFINITE_LEASE;
		  else
		      lease_expiry = lease + time_in_p->tv_sec;
		  S_set_lease(&pl, lease_expiry, &modified);
	      }
	      else if (time_in_p->tv_sec >= lease_expiry) {
		  /* send a nak */
		  nak = "lease expired";
		  goto send_nak;
	      }
	      else /* give the host the remaining time on the lease */
		  lease = lease_expiry - time_in_p->tv_sec;
	  }
      send_nak:
	  if (nak) {
	      reply = make_dhcp_nak((struct dhcp *)txbuf, sizeof(txbuf),
				    if_p->addr, &reply_msgtype, nak,
				    rq, &options);
	      if (reply)
		  goto reply;
	      goto no_reply;
	  }
	  /*
	   * allow for drift between server/client clocks by offering
	   * a lease shorter than the recorded value
	   */
	  if (lease == DHCP_INFINITE_LEASE)
	      lease = dhcp_lease_hton(lease);
	  else
	      lease = dhcp_lease_hton(lease_prorate(lease));
	  reply = make_dhcp_reply((struct dhcp *)txbuf, sizeof(txbuf),
				  if_p->addr, 
				  reply_msgtype = dhcp_msgtype_ack_e,
				  rq, &options);
	  reply->dp_yiaddr = iaddr;
	  if ([options addOption:dhcptag_lease_time_e
	       Length:sizeof(lease) Data:&lease] == FALSE) {
	      syslog(LOG_INFO, "dhcpd: couldn't add lease time tag: %s",
		     [options errString]);
	      goto no_reply;
	  }
	  break;
      }
      case dhcp_msgtype_decline_e: {
	  if (bound && rq->dp_ciaddr.s_addr == iaddr.s_addr) {
	      S_delete_prop(&pl, NIPROP_ENADDR, &modified);
	      S_delete_prop(&pl, NIPROP_IDENTIFIER, &modified);
	      S_delete_prop(&pl, HOSTPROP__DHCP_LEASE, &modified);
	      S_delete_prop(&pl, HOSTPROP__DHCP_RELEASED, &modified);
	      S_set_prop(&pl, HOSTPROP__DHCP_DECLINED, NULL, &modified);
	      syslog(LOG_INFO, "dhcpd: IP %s declined by %s",
		     inet_ntoa(iaddr), idstr);
	      if (debug) {
		  printf("marking host %s as declined\n", inet_ntoa(iaddr));
	      }
	  }
	  break;
      }
      case dhcp_msgtype_release_e: {
	  if (bound) {
	      if (debug)
		  printf("marking host %s as released\n", 
			 inet_ntoa(iaddr));
	      /* mark host as released so we can re-use the address */
	      S_set_prop(&pl, HOSTPROP__DHCP_RELEASED, NULL, &modified);
	  }
	  break;
      }
      case dhcp_msgtype_inform_e: {
	  iaddr = rq->dp_ciaddr;
	  if (hostname == NULL) {
	      hostname = S_host_by_addr(iaddr);
	      if (hostname)
		  hostname = strdup(hostname);
	  }
	  reply = make_dhcp_reply((struct dhcp *)txbuf, sizeof(txbuf),
				  if_p->addr, 
				  reply_msgtype = dhcp_msgtype_ack_e,
				  rq, &options);
	  break;
      }
      default: {
	  if (debug)
	      printf("unknown message ignored\n");
      }
      break;
    }

  reply:
    if (debug)
	printf("state=%s\n", dhcp_cstate_str(state));
    if (bound && modified && [domain isMaster]) {
	if (S_commit_mods(domain, &dir, &pl) == FALSE)
	    goto no_reply;
    }
    if (reply) {
	if (reply_msgtype == dhcp_msgtype_ack_e || 
	    reply_msgtype == dhcp_msgtype_offer_e) {
	    if (bootfile) {
		if (bootp_add_bootfile(rq->dp_file, hostname, boot_home_dir,
				       bootfile, boot_default_file, 
				       boot_tftp_dir, reply->dp_file) == FALSE)
		    goto no_reply;
	    }
	    else
		bzero(reply->dp_file, sizeof(reply->dp_file));

	    reply->dp_siaddr = if_p->addr;
	    strcpy(reply->dp_sname, if_p->hostname);

	    { /* add the client-specified parameters */
		int		num_params;
		u_char *	params;
		params = (char *) 
		    [rq_options 
			findOptionWithTag:dhcptag_parameter_request_list_e
			Length:&num_params];
		add_subnet_options(domain, hostname, iaddr, if_p, options, 
				   params, num_params);
	    }
	    /* terminate the options */
	    if ([options 
		    addOption:dhcptag_end_e Length:0 Data:NULL] == FALSE) {
		syslog(LOG_INFO, "couldn't add end tag: %s",
		       [options errString]);
		goto no_reply;
	    }
	}
	{
	    int size = sizeof(struct dhcp) + sizeof(rfc_magic) 
		+ [options bufferUsed];
	    
	    if (size < sizeof(struct bootp)) {
		/* pad out to BOOTP-sized packet */
		size = sizeof(struct bootp);
	    }
	    if (debug) {
		printf("\nSending: DHCP %s (size %d)\n", 
		       dhcp_msgtype_names(reply_msgtype), size);
		print_packet((struct bootp *)reply, size);
		[options parse];
		[options print];
	    }
	    if (sendreply(if_p, (struct bootp *)reply, size, 
			  use_broadcast, &iaddr)) {
		if (!quiet)
		    syslog(LOG_INFO, "%s sent %s %s pktsize %d",
			   dhcp_msgtype_names(reply_msgtype),
			   (hostname != NULL) 
			   ? hostname : (u_char *)"<no hostname>", 
			   inet_ntoa(iaddr), size);
	    }
	}
    }
 no_reply:
    if (idstr)
	free(idstr);
    if (hostname)
	free(hostname);
    if (bootfile)
	free(bootfile);
    if (options != nil)
	[options free];
    ni_proplist_free(&pl);
    return;
}

