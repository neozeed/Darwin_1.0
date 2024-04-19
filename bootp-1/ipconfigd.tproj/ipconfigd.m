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
 * ipconfigd.m
 * - multi-threaded IP config daemon that configures an interface
 *   using manual settings, manual with DHCP INFORM, BOOTP, or DHCP
 */
#import <stdlib.h>
#import <unistd.h>
#import <string.h>
#import <stdio.h>
#import <sys/types.h>
#import <sys/wait.h>
#import <sys/errno.h>
#import <sys/socket.h>
#import <ctype.h>
#import <net/if.h>
#import <net/etherdefs.h>
#import <netinet/in.h>
#import <netinet/udp.h>
#import <netinet/in_systm.h>
#import <netinet/ip.h>
#import <netinet/bootp.h>
#import <arpa/inet.h>

#import <objc/objc.h>
#import <objc/objc-runtime.h>

#import <syslog.h>
#import <varargs.h>

#import "rfc_options.h"
#import "dhcpOptions.h"
#import "dhcpOptionsPrivate.h"
#import "macNCOptions.h"
#import "dhcp.h"
#import "interfaces.h"
#import "util.h"
#import "lockc.h"
#import <net/if_types.h>
#import "ts_log.h"
#import "host_identifier.h"
#import "threadcompat.h"

static lockc_t		completion;

#import "dhcplib.h"

#import "ARPProbe.h"
#import "Packet.h"
#import "PacketQueue.h"
#import "Timer.h"
#import "Receiver.h"
#import "Transmitter.h"
#import "ipcfg.h"
#import "ipconfigd.h"
#import "server.h"

#import "dprintf.h"

#define USER_ERROR			1
#define UNEXPECTED_ERROR 		2
#define TIMEOUT_ERROR			3

#define MAX_RETRIES			2
#define RECEIVE_TIMEOUT_SECS		0
#define GATHER_TIME_SECS		2
#define INITIAL_WAIT_SECS		4
#define MAX_WAIT_SECS			60
#define RAND_SECS			1

#define LEASE_DIR			"/var/run/dhcpclient"

struct ifstatus;

struct ifstatus {
    /* pointer to form link list */
    struct ifstatus *		next;

    /* variables considered read-only hence unprotected */
    interface_t			ifa;
    ipcfg_mode_t		mode;
    UTHREAD_T			thread;

    /* read/write past this point... */

    volatile boolean_t		in_use;		/* address in use */
    id				q;		/* to wakeup the thread */
    volatile boolean_t		please_exit;	/* stop the thread */

    /* if ready is set, pkt and options may be read */
    volatile boolean_t		ready;
    volatile id			pkt;
    volatile id			options;
    volatile struct in_addr	from;
};

static u_short 		client_port = IPPORT_BOOTPC;
static boolean_t 	exit_quick = FALSE;
static u_short 		server_port = IPPORT_BOOTPS;
static u_long		gather_secs = GATHER_TIME_SECS;
static u_long		max_retries = MAX_RETRIES;
static boolean_t 	must_broadcast = FALSE;
static struct ifstatus *ifstatus_list = NULL;
static int    		ifstatus_count = 0;
static boolean_t	verbose = FALSE;
static boolean_t	debug = FALSE;

/* tags_search: these are the tags we look for: */
static u_char       	tags_search[] = { 
    dhcptag_host_name_e,
    dhcptag_subnet_mask_e, 
    dhcptag_router_e,
    dhcptag_netinfo_server_address_e,
    dhcptag_netinfo_server_tag_e,
};
int			n_tags_search = sizeof(tags_search) 
				        / sizeof(tags_search[0]);
#define IDEAL_RATING	n_tags_search

/* tags_print: these are the tags we print in the response */
static u_char	tags_print[] = { 
    dhcptag_host_name_e,
    dhcptag_subnet_mask_e, 
    dhcptag_router_e, 
    dhcptag_domain_name_server_e,
    dhcptag_domain_name_e,
    dhcptag_netinfo_server_address_e,
    dhcptag_netinfo_server_tag_e,
};
int n_tags_print = sizeof(tags_print) / sizeof(tags_print[0]);

unsigned char	rfc_magic[4] = RFC_OPTIONS_MAGIC;

static const struct in_addr	ip_broadcast = { INADDR_BROADCAST };
static const struct in_addr	ip_zeroes = { 0 };
static const struct sockaddr	blank_sin = { sizeof(blank_sin), AF_INET };

extern struct ether_addr *ether_aton(char *);

static id
S_packet_options(struct dhcp * dhcp, int pkt_len)
{
    id options;

    if (bcmp(dhcp->dp_options, rfc_magic, sizeof(rfc_magic)))
	return (nil);
    
    options = [[dhcpOptions alloc] 
	       initWithBuffer:dhcp->dp_options + RFC_MAGIC_SIZE
	       Size:pkt_len - sizeof(struct dhcp) - RFC_MAGIC_SIZE];
    if (options == nil) {
	ts_log(LOG_ERR, "S_packet_options: options nil");
	return nil;
    }
    if ([options parse] == FALSE) {
	ts_log(LOG_ERR, "S_packet_options: options parse failed");
	[options free];
	return nil;
    }
    return (options);
}

#if 0
static void
print_packet(struct dhcp *dp, int bp_len)
{
	int i, j, len;
	printf("bp_op = ");
	if (dp->dp_op == BOOTREQUEST) printf("BOOTREQUEST\n");
	else if (dp->dp_op == BOOTREPLY) printf("BOOTREPLY\n");
	else
	{
		i = dp->dp_op;
		printf("%d\n", i);
	}

	i = dp->dp_htype;
	printf("bp_htype = %d\n", i);

	printf("dp_flags = %x\n", dp->dp_flags);
	len = dp->dp_hlen;
	printf("bp_hlen = %d\n", len);

	i = dp->dp_hops;
	printf("bp_hops = %d\n", i);

	printf("bp_xid = %lu\n", (u_long)ntohl(dp->dp_xid));

	printf("bp_secs = %hu\n", dp->dp_secs);

	printf("bp_ciaddr = " IP_FORMAT "\n", IP_LIST(&dp->dp_ciaddr));
	printf("bp_yiaddr = " IP_FORMAT "\n", IP_LIST(&dp->dp_yiaddr));
	printf("bp_siaddr = " IP_FORMAT "\n", IP_LIST(&dp->dp_siaddr));
	printf("bp_giaddr = " IP_FORMAT "\n", IP_LIST(&dp->dp_giaddr));

	printf("bp_chaddr = ");
	for (j = 0; j < len; j++)
	{
		i = dp->dp_chaddr[j];
		printf("%0x", i);
		if (j < (len - 1)) printf(":");
	}
	printf("\n");

	printf("bp_sname = %s\n", dp->dp_sname);
	printf("bp_file = %s\n", dp->dp_file);

	{
	    id			options;

	    options = S_packet_options(dp, bp_len);
	    if (options != nil) {
		printf("options: packet size %d\n", bp_len);
		[options print];
		[options free];
	    }
	}
}

#endif 0

#ifdef DEBUG
static void
dump_ifstatus_list(struct ifstatus * list_p)
{
    printf("-------start--------\n");
    while (list_p) {
	printf("%s: %s\n", if_name(&list_p->ifa), 
	       ipcfg_mode_string(list_p->mode));
	list_p = list_p->next;
    }
    printf("-------end--------\n");
    return;
}
#endif DEBUG

/*
 * Function: ip_valid
 * Purpose:
 *   Perform some cursory checks on the IP address
 *   supplied by the server
 */
static __inline__ boolean_t
ip_valid(struct in_addr ip)
{
    if (ip.s_addr == 0
	|| ip.s_addr == INADDR_BROADCAST)
	return (FALSE);
    return (TRUE);
}

/*
 * Function: make_bootp_request
 * Purpose:
 *   Create a "blank" bootp packet.
 */
static void
make_bootp_request(struct bootp * pkt, 
		   u_char * hwaddr, u_char hwtype, u_char hwlen,
		   struct in_addr ciaddr)
{
    bzero(pkt, sizeof (*pkt));
    pkt->bp_op = BOOTREQUEST;
    pkt->bp_htype = hwtype;
    pkt->bp_hlen = hwlen;
    if (must_broadcast)
	pkt->bp_unused = htons(DHCP_FLAGS_BROADCAST);
    bcopy(hwaddr, pkt->bp_chaddr, hwlen);
    bcopy(rfc_magic, pkt->bp_vend, sizeof(rfc_magic));
    pkt->bp_vend[4] = dhcptag_end_e;
    return;
}


#if 0
static void
on_alarm(int sigraised)
{
    exit(0);
    return;
}

static void
wait_for_bootp_responses(int sockfd)
{
    u_char 		buf[2048];
    int			buf_len = sizeof(buf);
    int			count = 0;
    struct sockaddr_in 	from;
    int 		fromlen;

    bzero(buf, buf_len);

    signal(SIGALRM, on_alarm);
    ualarm(gather_secs * USECS_PER_SEC, 0);

    for(;;) {
	int 		n_recv;

	from.sin_family = AF_INET;
	fromlen = sizeof(struct sockaddr);
	
	n_recv = recvfrom(sockfd, buf, buf_len, 0,
			   (struct sockaddr *)&from, &fromlen);
	if (n_recv > 0) {
	    printf("\nreply #%d from " IP_FORMAT "\n", ++count,
		   IP_LIST(&from.sin_addr));
	    print_packet((struct dhcp *)buf, n_recv);
	}
    }
    return;
}
#endif 0

static unsigned
count_params(id options, u_char * tags, int size)
{
    int				i;
    int				rating = 0;

    if (options != nil) {
	for (i = 0; i < size; i++) {
	    if ([options findOptionWithTag:tags[i] Length:NULL] != NULL)
		rating++;
	}
    }
    return (rating);
}

void
print_option(FILE * file, void * option, int len, int tag)
{
    int 		i;
    int 		count;
    int			size;
    tag_info_t * 	tag_info = [dhcpOptions tagInfo:tag];
    int			type;
    type_info_t * 	type_info;

    if (tag_info == NULL)
	return;
    type = tag_info->type;
    type_info = [dhcpOptions typeInfo:type];
    if (type_info == NULL)
	return;
    size = 0;
    count = 1;
    if (type_info->multiple_of != dhcptype_none_e) {
	type_info_t * base_type_info = [dhcpOptions 
				        typeInfo:type_info->multiple_of];
	size = base_type_info->size;
	count = len / size;
	len = size;
	type = type_info->multiple_of;
	fprintf(file, "%s_count=%d\n", tag_info->name, count);
    }
    for (i = 0; i < count; i++) {
	u_char tmp[256];
	
	if ([dhcpOptions str:tmp FromOption:option Length:(int)len 
	     Type:type ErrorString:NULL]) {
	    fprintf(file, "%s", tag_info->name);
	    if (i > 0)
		fprintf(file, "%d", i + 1);
	    fprintf(file, "=%s\n", tmp);
	}
	option += size;
    }
    return;
}

void
echo_variables(FILE * file, void * pkt, int pkt_len, 
	       struct in_addr from, id options, ipcfg_mode_t mode)
{
    struct bootp * bp = (struct bootp *)pkt;

    switch (mode) {
      case ipcfg_mode_dhcp_e:
      case ipcfg_mode_bootp_e:
      case ipcfg_mode_inform_e:
	  fprintf(file, "mode=%s\n", ipcfg_mode_string(mode));
	  break;
      default:
	  break;
    }
    if (from.s_addr)
	fprintf(file, "from=" IP_FORMAT "\n", IP_LIST(&from));
    if (bp->bp_yiaddr.s_addr)
	fprintf(file, "ip_address=" IP_FORMAT "\n", IP_LIST(&bp->bp_yiaddr));
    else
	fprintf(file, "ip_address=" IP_FORMAT "\n", IP_LIST(&bp->bp_ciaddr));

    if (options != nil) {
	int	i;
	int 	len;
	void * 	option;

	for (i = 0; i < n_tags_print; i++) {
	    option = [options findOptionWithTag:tags_print[i] Length:&len];
	    if (option)
		print_option(file, option, len, tags_print[i]);
	}
    }
    if (bp->bp_siaddr.s_addr)
	fprintf(file, "server_ip_address=" IP_FORMAT "\n", 
		IP_LIST(&bp->bp_siaddr));

    if (bp->bp_sname[0])
	fprintf(file, "server_name=%s\n", bp->bp_sname);

    return;
}

static __inline__ int
next_set_bit(u_int mask, int start_bit)
{
    int i;

    for (i = start_bit; i < 32; i++) {
	if (mask & (1 << i))
	    return (i);
    }
    return (-1);
}

static __inline__ int
count_set_bits(u_int mask)
{
    int i;
    int count = 0;

    for (i = 0; i < 32; i++) {
	if (mask & (1 << i))
	    count++;
    }
    return (count);
}

static id		arp = nil;
static id 		receiver = nil;
static id		transmitter = nil;

static __inline__ boolean_t
packet_match(struct bootp * packet, unsigned long xid, interface_t * if_p)
{
    if (packet->bp_op != BOOTREPLY
	|| ntohl(packet->bp_xid) != xid
	|| (packet->bp_htype != if_link_arptype(if_p))
	|| (packet->bp_hlen != if_link_length(if_p))
	|| bcmp(packet->bp_chaddr, if_link_address(if_p),
		if_link_length(if_p))) {
	return (FALSE);
    }
    return (TRUE);
}

#define TMP_IPCONFIGD_DIR		"/tmp/.ipconfigd"
#define IPCONFIGD_DIR			"/var/run/ipconfigd"

static void
clear_ifstatus(struct ifstatus * ifstatus)
{
#ifdef WRITE_IFSTATUS
    char	buf[256];
#endif WRITE_IFSTATUS

    ifstatus->ready = FALSE;
    ifstatus->in_use = FALSE;
#ifdef WRITE_IFSTATUS
    sprintf(buf, TMP_IPCONFIGD_DIR "/_%s", if_name(&ifstatus->ifa));
    unlink(buf);
    sprintf(buf, IPCONFIGD_DIR "/%s", if_name(&ifstatus->ifa));
    unlink(buf);
#endif WRITE_IFSTATUS
    [ifstatus->options free];
    [ifstatus->pkt free];
    ifstatus->options = ifstatus->pkt = nil;
    return;
}

static void
post_ifstatus(struct ifstatus * ifstatus, id pkt, id options)
{
#ifdef WRITE_IFSTATUS
    char	fbuf[256];
    char	tbuf[256];
    FILE * 	f;
#endif WRITE_IFSTATUS
    
    ifstatus->pkt = pkt;
    ifstatus->options = options;
#ifdef WRITE_IFSTATUS
    sprintf(fbuf, TMP_IPCONFIGD_DIR "/_%s", if_name(&ifstatus->ifa));
    sprintf(tbuf, IPCONFIGD_DIR "/%s", if_name(&ifstatus->ifa));
    f = fopen(fbuf, "w");
    if (f == NULL) {
	ts_log(LOG_ERR, "post_ifstatus(%s): failed to create %s, %m", 
	       if_name(&ifstatus->ifa), fbuf);
    }
    else {
	if (pkt)
	    echo_variables(f, [pkt data], [pkt size], [pkt from], options,
			   ifstatus->mode);
	fflush(f);
	fclose(f);
	if (rename(fbuf, tbuf) < 0) {
	    ts_log(LOG_ERR, "post_ifstatus(%s): rename %s -> %s failed, %m",
		   if_name(&ifstatus->ifa), fbuf, tbuf);
	}
	ifstatus->ready = TRUE;
    }
#else WRITE_IFSTATUS
    ifstatus->ready = TRUE;
#endif WRITE_IFSTATUS
}

#import <sys/ioctl.h>
#import <sys/sockio.h>

static int
ifconfig_socket()
{
    return (socket(AF_INET, SOCK_DGRAM, 0));
}

static int
ifconfig_delete(int s, char * name, struct in_addr * addr)
{
    struct ifreq	ifr;

    bzero(&ifr, sizeof(ifr));
    strncpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
    if (addr) {
	ifr.ifr_addr = blank_sin;
	((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr = *addr;
    }
    return (ioctl(s, SIOCDIFADDR, &ifr));
}

static int
ifconfig(int s, char * name, struct in_addr * addr, struct in_addr * mask,
	 struct in_addr * broadcast)
{
    struct ifaliasreq	ifra;

    bzero(&ifra, sizeof(ifra));
    strncpy(ifra.ifra_name, name, sizeof(ifra.ifra_name));
    if (addr) {
	ifra.ifra_addr = blank_sin;
	((struct sockaddr_in *)&ifra.ifra_addr)->sin_addr = *addr;
    }
    if (mask) {
	ifra.ifra_mask = blank_sin;
	((struct sockaddr_in *)&ifra.ifra_mask)->sin_addr = *mask;
    }
    if (broadcast) {
	ifra.ifra_mask = blank_sin;
	((struct sockaddr_in *)&ifra.ifra_broadaddr)->sin_addr = *broadcast;
    }
    return (ioctl(s, SIOCAIFADDR, &ifra));
}

static int
ifconfig_flags(int s, char * name, short flags)
{
    struct ifreq	ifr;

    strncpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
    ifr.ifr_flags = flags;
    return (ioctl(s, SIOCSIFFLAGS, &ifr));
}

static __inline__ boolean_t
ip_in_use(interface_t * if_p, struct in_addr ip)
{
    boolean_t in_use = FALSE;

    if (if_flags(if_p) & IFF_BROADCAST) {
	if ([arp probeInterface:if_name(if_p) 
		 SenderHW:if_link_address(if_p)
		 SenderIP:ip_zeroes TargetIP:ip
		 InUse:&in_use] < 0) {
	    ts_log(LOG_ERR, "ip_in_use(%s): ARPProbe failed, %m", 
		   if_name(if_p));
	}
    }
    return (in_use);
}

static boolean_t
set_ip_and_mask(interface_t * if_p, struct in_addr ip, struct in_addr mask)
{
    boolean_t		ret = FALSE;
    int 		s = ifconfig_socket();

    ts_log(LOG_DEBUG, "configuring %s to " IP_FORMAT " netmask " IP_FORMAT,
	   if_name(if_p), IP_LIST(&ip), IP_LIST(&mask));
    if (s < 0) {
	ts_log(LOG_ERR, "set_ip_and_mask(%s):ifconfig_socket() failed, %m",
	       if_name(if_p));
    }
    else {
	ifconfig_delete(s, if_name(if_p), NULL);
	if (ifconfig(s, if_name(if_p), &ip, &mask, NULL) < 0) {
	    ts_log(LOG_ERR, "set_ip_and_mask(%s) to " 
		   IP_FORMAT " failed, %m", if_name(if_p),
		   IP_LIST(&ip));
	}
	else {
	    if_p->inet_valid = TRUE;
	    if_p->addr = ip;
	    if_p->mask = mask;
	    ret = TRUE;
	}
	close(s);
    }
    return (ret);
}


static __inline__ void
mark_if_zeroes(interface_t * if_p)
{
    int s = ifconfig_socket();

    if (s < 0)
	;
    else {
	if (ifconfig_delete(s, if_name(if_p), NULL) < 0)
	    ts_log(LOG_ERR, "ifconfig_delete %s failed, %m", if_name(if_p));
	if (ifconfig(s, if_name(if_p), (struct in_addr *)&ip_zeroes, 
		     (struct in_addr *)&ip_zeroes, NULL) < 0)
	    ts_log(LOG_ERR, "ifconfig %s failed, %m", if_name(if_p));
	close(s);
    }
    return;
}

static __inline__ void
mark_if_down(interface_t * if_p)
{
    int s = ifconfig_socket();
    if (s < 0)
	;
    else {
	ifconfig_delete(s, if_name(if_p), NULL);
	ifconfig_flags(s, if_name(if_p), if_flags(if_p) & ~IFF_UP);
	close(s);
    }
    if_p->inet_valid = FALSE;
}

static void
bootp_thread(struct ifstatus * ifstatus)
{
    struct in_addr	ciaddr = { 0 };
    boolean_t		gathering = FALSE;
    interface_t * 	if_p = &ifstatus->ifa;
    id 			q = nil;
    struct bootp	request;
    id 			saved_pkt = nil;
    id			saved_options = nil;
    unsigned 		saved_rating = 0;
    int			start_secs = current_secs();
    id			timer = nil;
    int			try;
    dhcp_time_secs_t	wait_secs = INITIAL_WAIT_SECS;
    u_long		xid;

    ts_log(LOG_DEBUG, "%s: BOOTP starting", if_name(if_p));
    xid = random();

    clear_ifstatus(ifstatus);

    q = [[PacketQueue alloc] init];
    [receiver attach:q]; /* start receiving packets */
    timer = [[Timer alloc] init];

    make_bootp_request(&request, if_link_address(if_p), 
		       if_link_arptype(if_p),
		       if_link_length(if_p), ciaddr);

    /* send (max_retries + 1) times */
    for (try = 0; ifstatus->please_exit == FALSE && try < (max_retries + 1); 
	 try++) {
	struct timeval tv;

	request.bp_secs = htons((u_short) current_secs() - start_secs);
	request.bp_xid = htonl(xid);

	/* send the packet */
	if ([transmitter transmit:if_name(if_p) 
			 DestHWType:request.bp_htype
			 DestHWAddr:NULL
			 DestHWLen:0
			 DestIP:ip_broadcast SourceIP:if_p->addr
			 DestPort:server_port SourcePort:client_port
			 Data:&request Length:sizeof(request)] < 0) {
	    ts_log(LOG_DEBUG, "%s: transmit failed", if_name(if_p));
	    break;
	}

	/* wait for responses */
	tv.tv_sec = wait_secs;
	tv.tv_usec = random_range(0, USECS_PER_SEC - 1);
	ts_log(LOG_DEBUG, "%s: waiting at %d for %d.%06d", if_name(if_p), 
	       current_secs() - start_secs,
	       tv.tv_sec, tv.tv_usec);
	[timer cancel];
	[q clearTimeout];
	[timer wake:q Interval:tv];
	[q lock];
	while (1) {
	    id 			pkt;
	    id			reply_options;

	    pkt = [q remove];
	    if ([q timedOut]) {
		[pkt free];
		[q unlock];
		ts_log(LOG_DEBUG, "%s timed out", if_name(if_p));
		break; /* out of while */
	    }
	    if (pkt == nil) {
		[q wait];
	    }
	    else { /* got a packet */
		struct bootp *	reply;

		[q unlock];
		reply = (struct bootp *)[pkt data];
#ifdef DEBUG
		print_packet(reply, [pkt size]);
#endif DEBUG

		if ([pkt size] < sizeof(struct bootp)
		    || (ip_valid(reply->bp_yiaddr) == FALSE
			&& ip_valid(reply->bp_ciaddr) == FALSE)
		    || packet_match(reply, xid, if_p) == FALSE) {
		    /* not our packet, drop the packet */
		    [pkt free];
		}
		else {
		    unsigned rating;

		    reply_options 
			= S_packet_options((struct dhcp *)[pkt data], 
					   [pkt size]);
		    rating = count_params(reply_options, 
					  tags_search, n_tags_search);
		    if (saved_pkt == nil
			|| rating > saved_rating) {
			[saved_pkt free];
			[saved_options free];
			saved_pkt = pkt;
			saved_rating = rating;
			saved_options = reply_options;
			if (rating == IDEAL_RATING) {
			    [timer cancel];
			    break; /* out of while */
			}
			if (gathering == FALSE) {
			    struct timeval t = {0,0};

			    ts_log(LOG_DEBUG, "%s: gathering began at %d", 
				   if_name(if_p), 
				   current_secs() - start_secs);
			    gathering = TRUE;
			    [timer cancel];
			    [q clearTimeout];
			    t.tv_sec = gather_secs;
			    [timer wake:q Interval:t];
			}
		    }
		    else {
			[pkt free];
			[reply_options free];
		    }
		}
		[q lock];
	    } /* got a packet */
	} /* while */
	if (saved_pkt != nil)
	    break;
	wait_secs = tv.tv_sec * 2;
	if (wait_secs > MAX_WAIT_SECS)
	    wait_secs = MAX_WAIT_SECS;
	xid++;
    } /* for */
    [timer cancel];
    [timer free];
    [receiver detach:q]; /* stop receiving packets */
    [[q freeValues] free];

    ts_log(LOG_DEBUG, "%s: BOOTP ended at %d", if_name(if_p), 
	   current_secs() - start_secs);
    ifstatus->in_use = FALSE;
    if (saved_pkt != nil) {
	struct bootp * 	reply = (struct bootp *)[saved_pkt data];
	
	if (ip_in_use(if_p, reply->bp_yiaddr)) {
	    ts_log(LOG_ERR, "%s: BOOTP-supplied IP address " IP_FORMAT 
		   " in use, server IP " IP_FORMAT, 
		   if_name(if_p), IP_LIST(&reply->bp_yiaddr),
		   IP_LIST(&reply->bp_siaddr));
	    ifstatus->in_use = TRUE;
	}
	else {
	    int			len;
	    struct in_addr	mask = {0};
	    void *		option;
	    
	    option = [saved_options findOptionWithTag:dhcptag_subnet_mask_e
				    Length:&len];
	    if (option)
		mask = *((struct in_addr *)option);
	    
	    set_ip_and_mask(if_p, reply->bp_yiaddr, mask);
	}
	post_ifstatus(ifstatus, saved_pkt, saved_options);
    }
    if (if_p->addr.s_addr == 0) /* clean-up all-zeroes address */
	mark_if_down(if_p);
    ifstatus->ready = TRUE;
    ifstatus->thread = NULL;
    lockc_signal(&completion);
    UTHREAD_EXIT(0);
    return;
}

static __inline__ boolean_t
get_server_identifier(id options, struct in_addr * server_ip)
{
    struct in_addr * 	ipaddr_p;

    ipaddr_p = (struct in_addr *) 
	[options findOptionWithTag:dhcptag_server_identifier_e Length:NULL];
    if (ipaddr_p)
	*server_ip = *ipaddr_p;
    return (ipaddr_p != NULL);
}

static __inline__ boolean_t
get_lease(id options, dhcp_lease_t * lease_time)
{
    dhcp_lease_t *	lease_p;

    lease_p = (dhcp_lease_t *)
	[options findOptionWithTag:dhcptag_lease_time_e Length:NULL];
    if (lease_p) {
	*lease_time = dhcp_lease_ntoh(*lease_p);
    }
    return (lease_p != NULL);
}

static u_char dhcp_params[] = {
    dhcptag_subnet_mask_e, 
    dhcptag_router_e,
    dhcptag_host_name_e,
    dhcptag_domain_name_server_e,
    dhcptag_domain_name_e,
    dhcptag_netinfo_server_address_e,
    dhcptag_netinfo_server_tag_e,
};

int	n_dhcp_params = sizeof(dhcp_params) / sizeof(dhcp_params[0]);

static struct dhcp * 
make_dhcp_request(struct dhcp * request, int pkt_size,
		  dhcp_msgtype_t msg, u_char * hwaddr, u_char hwtype, 
		  u_char hwlen, id * options_p)
{
    id options;

    bzero(request, pkt_size);
    request->dp_op = BOOTREQUEST;
    request->dp_htype = hwtype;
    request->dp_hlen = hwlen;
    if (must_broadcast)
	request->dp_flags = htons(DHCP_FLAGS_BROADCAST);
    bcopy(hwaddr, request->dp_chaddr, hwlen);
    bcopy(rfc_magic, request->dp_options, sizeof(rfc_magic));
    options = [[dhcpOptions alloc] 
	      initWithBuffer:request->dp_options + sizeof(rfc_magic) 
	      Size:DHCP_MIN_OPTIONS_SIZE - sizeof(rfc_magic)];
    if (options == nil) {
	ts_log(LOG_ERR, "make_dhcp_request: init options failed");
	goto err;
    }
    
    /* make the request a dhcp message */
    if ([options dhcpMessage:msg] == FALSE) {
	ts_log(LOG_ERR,
	       "make_dhcp_request: couldn't add dhcp message tag %d, %s", msg,
	       [options errString]);
	goto err;
    }

    if (msg != dhcp_msgtype_decline_e) {
	/* add the list of required parameters */
	if ([options addOption:dhcptag_parameter_request_list_e
		     Length:sizeof(dhcp_params) Data:dhcp_params] == FALSE) {
	    ts_log(LOG_ERR, "make_dhcp_request: "
		    "couldn't add parameter request list, %s",
		    [options errString]);
	    goto err;
	}
    }

#ifdef USE_CLIENT_IDENTIFIER
    {
	char * buf;
	
	buf = malloc(hwlen + 1);
	if (buf == NULL) {
	    ts_log(LOG_ERR, "make_dhcp_request: malloc failed, %m")
	    goto err;
	}
	bcopy(hwaddr, buf + 1, hwlen);
	*buf = hwtype;
	if ([options addOption:dhcptag_client_identifier_e
		     Length:hwlen + 1 Data:buf] == FALSE) {
	    free(buf);
	    ts_log(LOG_ERR, "make_dhcp_request: "
		    "couldn't add client identifier, %s",
		    [options errString]);
	    goto err;
	}
	free(buf);
    }
#endif USE_CLIENT_IDENTIFIER

    *options_p = options;
    return (request);
  err:
    [options free];
    *options_p = nil;
    return (NULL);
}

/* 
 * Function: read_ip_lease
 *
 * Purpose:
 *   Retrieve a saved lease from the filesystem.  If it's
 *   still valid, return TRUE and the IP address. 
 *   Otherwise, return FALSE.
 */
#define IP_LEASE_INFO_MAGIC		"dhcp"
#define IP_LEASE_INFO_MAGIC_SIZE	4
#define IP_LEASE_INFO_VERSION		0
#define IP_LEASE_INFO_LENGTH		16
#define IP_LEASE_INFO_VERSION_OFFSET	4
#define IP_LEASE_INFO_LENGTH_OFFSET	6
#define IP_LEASE_INFO_EXPIRY_OFFSET	8
#define IP_LEASE_INFO_IP_OFFSET		12

static boolean_t
read_ip_lease(char * idstr, struct in_addr * ip, dhcp_time_secs_t * expiry)
{
    FILE * 		f;
    char		filename[256];
    char		info[IP_LEASE_INFO_LENGTH];
    u_int16_t		length;
    boolean_t		ret = FALSE;
    u_int16_t		version;
	
    sprintf(filename, LEASE_DIR "/%s", idstr);

    f = fopen(filename, "r");
    if (f == NULL)
	return (FALSE);
    
    if (fread(info, IP_LEASE_INFO_LENGTH, 1, f) != 1
	|| bcmp(info, IP_LEASE_INFO_MAGIC, IP_LEASE_INFO_MAGIC_SIZE))
	goto done;

    version = ntohs(*(u_int16_t *)&info[IP_LEASE_INFO_VERSION_OFFSET]);
    length = ntohs(*(u_int16_t *)&info[IP_LEASE_INFO_LENGTH_OFFSET]);

    if (length == IP_LEASE_INFO_LENGTH && version == IP_LEASE_INFO_VERSION) {
	*expiry = dhcp_time_ntoh(*((dhcp_time_secs_t *)
				   &info[IP_LEASE_INFO_EXPIRY_OFFSET]));
	/* ip is stored in network byte order, no conversion */
	ip->s_addr = *((u_int32_t *)&info[IP_LEASE_INFO_IP_OFFSET]);
	ret = TRUE;
    }
 done:
    fclose(f);
    return (ret);
}

/* 
 * Function: write_ip_lease
 *
 * Purpose:
 *   Write an IP lease for this interface.
 */
static boolean_t
write_ip_lease(char * idstr, struct in_addr ip, dhcp_time_secs_t expiry)
{
    FILE * 		f;
    char		filename[256];
    char		info[IP_LEASE_INFO_LENGTH];
    boolean_t		ret = FALSE;
	
    sprintf(filename, LEASE_DIR "/%s", idstr);

    f = fopen(filename, "w");
    if (f == NULL) {
	ts_log(LOG_ERR, "write_ip_lease(%s) fopen(%s) failed, %m", idstr,
	       filename);
	return (FALSE);
    }

    bcopy(IP_LEASE_INFO_MAGIC, info, IP_LEASE_INFO_MAGIC_SIZE);
    *((u_int16_t *)&info[IP_LEASE_INFO_VERSION_OFFSET]) 
	= htons(IP_LEASE_INFO_VERSION);
    *((u_int16_t *)&info[IP_LEASE_INFO_LENGTH_OFFSET]) 
	= htons(IP_LEASE_INFO_LENGTH);
    *((dhcp_time_secs_t *)&info[IP_LEASE_INFO_EXPIRY_OFFSET]) 
	= dhcp_time_hton(expiry);

    /* ip is stored in network byte order, no conversion */
    *((u_int32_t *)&info[IP_LEASE_INFO_IP_OFFSET]) = ip.s_addr;
    if (fwrite(info, IP_LEASE_INFO_LENGTH, 1, f) != 1)
	syslog(LOG_ERR, "write_lease_info(%s) fwrite() failed, %m", idstr);
    else
	ret = TRUE;
    fclose(f);
    return (ret);
}

/*
 * Function: clear_ip_lease
 * Purpose:
 *   Remove the lease file so we don't try to use it again.
 */
void
clear_ip_lease(char * idstr)
{
    char		filename[256];
	
    sprintf(filename, LEASE_DIR "/%s", idstr);
    unlink(filename);
    return;
}


/*
 * Function: receive_packet
 * Purpose:
 *   Retrieve the next matching packet for the DHCP client, or
 *   nil if we timed out.
 */
static id 
receive_packet(id q, u_long xid, interface_t * if_p, 
	       id * options_p, dhcp_msgtype_t * msgtype_p, 
	       struct in_addr * server_ip)
{
    id 		pkt = nil;
    id		options = nil;

    [q lock];
    while ([q timedOut] == FALSE) {
	pkt = [q remove];
	if (pkt == nil) {
	    [q wait];
	    continue;
	}
	[q unlock];
#ifdef DEBUG
	print_packet([pkt data], [pkt size]);
#endif DEBUG

	if (packet_match((struct bootp *)[pkt data], xid, if_p)) {
	    options = S_packet_options((struct dhcp *)[pkt data], 
				       [pkt size]);
	    /* 
	     * A BOOTP packet should be one that doesn't contain
	     * a dhcp message.  Unfortunately, NeXT's BOOTP
	     * server is unaware of DHCP and RFC-standard
	     * options, and simply echoes back what we sent
	     * in the options area.  This is the reason for
	     * checking for DISCOVER, REQUEST and INFORM: they are
	     * normally invalid responses in the DHCP protocol.
	     * Normally we would get a corresponding OFFER, ACK, or ACK
	     * respectively.
	     */
	    if (options == nil
		|| is_dhcp_packet(options, msgtype_p) == FALSE
		|| *msgtype_p == dhcp_msgtype_discover_e
		|| *msgtype_p == dhcp_msgtype_request_e
		|| *msgtype_p == dhcp_msgtype_inform_e) {
		/* BOOTP packet */
		*msgtype_p = dhcp_msgtype_none_e;
		break;
	    }
	    else if (get_server_identifier(options, server_ip)) {
		/* matching DHCP packet */
		break;
	    }
	}
	[pkt free];
	[options free];

	pkt = nil;
	options = nil;
	[q lock];
    }
    [q unlock];
    *options_p = options;
    return (pkt);
}


static void
inform_thread(struct ifstatus * ifstatus)
{
    u_char *		buf = NULL;
    int			bufsize;
    interface_t * 	if_p = &ifstatus->ifa;
    id 			q = nil;
    struct dhcp * 	request = NULL;
    int 		request_size = 0;
    boolean_t		saved_is_dhcp = FALSE;
    id			saved_options = nil;
    int			saved_rating = 0;
    id			saved_pkt = nil;
    dhcp_time_secs_t	start_secs = current_secs();
    id			timer = nil;
    int			try;
    dhcp_time_secs_t	wait_secs = INITIAL_WAIT_SECS;
    u_long		xid;

    ts_log(LOG_DEBUG, "%s: INFORM starting", if_name(if_p));
    q = [[PacketQueue alloc] init];
    clear_ifstatus(ifstatus);
    timer = [[Timer alloc] init];
    xid = random();
    [receiver attach:q]; /* start receiving packets */
    bufsize = sizeof(struct dhcp) + DHCP_MIN_OPTIONS_SIZE;
    buf = malloc(bufsize);
    if (buf == NULL)
	goto done;

    {
	id			options = nil;
	
	request = make_dhcp_request((struct dhcp *)buf, bufsize,
				    dhcp_msgtype_inform_e,
				    if_link_address(if_p), 
				    if_link_arptype(if_p),
				    if_link_length(if_p), &options);
	if (request == NULL)
	    goto done;
	request->dp_ciaddr = if_addr(if_p);
	if ([options addOption:dhcptag_end_e Length:0 Data:0] == FALSE) {
	    ts_log(LOG_DEBUG, "ipconfigd: couldn't add end tag");
	    [options free];
	    goto done;
	}
#ifdef DEBUG
	/* make sure the options parse OK */
	if ([options parse] == FALSE) {
	    ts_log(LOG_DEBUG, "ipconfigd: couldn't parse options in reply");
	    [options free];
	    goto done;
	}
#endif DEBUG
	request_size = sizeof(*request) + sizeof(rfc_magic) 
	    + [options bufferUsed];
	[options free];
    }
    if (request_size < sizeof(struct bootp)) {
	/* pad out to BOOTP-sized packet */
	request_size = sizeof(struct bootp);
    }
    for (try = 0; ifstatus->please_exit == FALSE && try < (max_retries + 1); 
	 try++) {
	boolean_t		gathering = FALSE;
	id			pkt = nil;
	id			reply_options = nil;
	dhcp_msgtype_t		reply_msg = dhcp_msgtype_none_e;
	struct in_addr		ip;
	struct timeval		tv;
	
	request->dp_xid = htonl(++xid);
#ifdef DEBUG
	print_packet(request, request_size);
#endif DEBUG
	
	/* send the packet */
	if ([transmitter transmit:if_name(if_p) 
			 DestHWType:request->dp_htype
			 DestHWAddr:NULL
			 DestHWLen:0
			 DestIP:ip_broadcast SourceIP:request->dp_ciaddr
			 DestPort:server_port SourcePort:client_port
			 Data:request Length:request_size] < 0) {
	    goto done;
	}

	/* wait for responses */
	tv.tv_sec = wait_secs;
	tv.tv_usec = random_range(0, USECS_PER_SEC - 1);
	ts_log(LOG_DEBUG, "%s: waiting at %d for %d.%06d", if_name(if_p), 
	       current_secs() - start_secs,
	       tv.tv_sec, tv.tv_usec);
	[timer cancel];
	[q clearTimeout];
	[timer wake:q Interval:tv];
	while ((pkt = receive_packet(q, xid, if_p, &reply_options,
				     &reply_msg, &ip)) != nil) {
	    boolean_t 		is_dhcp = TRUE;
	    struct dhcp * 	reply;
	    
	    reply = (struct dhcp *)[pkt data];
	    if (reply_msg == dhcp_msgtype_none_e)
		is_dhcp = FALSE;
	    if (is_dhcp == FALSE || reply_msg == dhcp_msgtype_ack_e) {
		int rating = 0;
		
		rating = count_params(reply_options, dhcp_params,
				      n_dhcp_params);
		/* 
		 * The new packet is "better" than the saved
		 * packet if:
		 * - there was no saved packet, or
		 * - the new packet is a DHCP packet and the saved
		 *   one is a BOOTP packet or a DHCP packet with
		 *   a lower rating, or
		 * - the new packet and the saved packet are both
		 *   BOOTP but the new one has a higher rating
		 * All this to allow BOOTP/DHCP interoperability
		 * ie. we accept a BOOTP response if it's
		 * the only one we've got
		 * we expect/favour a DHCP response.
		 */
		if (saved_pkt == nil 
		    || (is_dhcp	== TRUE
			&& (saved_is_dhcp == FALSE || rating > saved_rating))
		    || (is_dhcp == FALSE && saved_is_dhcp == FALSE 
			&& rating > saved_rating)) {
		    [saved_pkt free];
		    [saved_options free];
		    saved_pkt = pkt;
		    saved_is_dhcp = is_dhcp;
		    saved_options = reply_options;
		    saved_rating = rating;
		    if (rating == n_dhcp_params) {
			goto got_response;
		    }
		    if (gathering == FALSE) {
			struct timeval t = {0,0};
			ts_log(LOG_DEBUG, 
			       "%s: gathering started at %d", 
			       if_name(if_p),
			       current_secs() - start_secs);
			gathering = TRUE;
			[timer cancel];
			[q clearTimeout];
			t.tv_sec = gather_secs;
			[timer wake:q Interval:t];
		    }
		    continue;
		}
	    }
	    [pkt free];
	    [reply_options free];
	} /* while */
     got_response:
	[timer cancel];
	if (ifstatus->please_exit)
	    break;
	    
	if (saved_pkt != nil)
	    break; /* out of for */
	wait_secs *= 2;
	if (wait_secs > MAX_WAIT_SECS)
	    wait_secs = MAX_WAIT_SECS;
    } /* for */
 done:
    if (buf)
	free(buf);
    [receiver detach:q]; /* stop receiving packets */
    [[q freeValues] free];
    
    if (saved_pkt != nil) {
	int 		len;
	struct in_addr	mask = {0};
	void *		option;
	
	option = [saved_options findOptionWithTag:dhcptag_subnet_mask_e
				Length:&len];
	if (option) {
	    mask = *((struct in_addr *)option);

	    if (if_p->mask.s_addr != mask.s_addr) {
		set_ip_and_mask(if_p, if_addr(if_p), mask);
	    }
	}
	post_ifstatus(ifstatus, saved_pkt, saved_options);
    }
    ts_log(LOG_DEBUG, "%s: INFORM loop stopped at %d", if_name(if_p), 
	   current_secs() - start_secs);
    ifstatus->thread = NULL;
    ifstatus->ready = TRUE;
    lockc_signal(&completion);
    UTHREAD_EXIT(0);
    return;
}

#define SUGGESTED_LEASE_LENGTH		(60 * 60 * 24) /* 1 day */

static void
dhcp_thread(struct ifstatus * ifstatus)
{
    u_char *		buf = NULL;
    int			bufsize;
    struct in_addr	dest_ip = ip_broadcast;
    dhcp_time_secs_t	expiration = 0;
    u_char *		idstr = NULL;
    interface_t * 	if_p = &ifstatus->ifa;
    int			init_try = 0;
    dhcp_lease_t	lease_length = 0;
    dhcp_time_secs_t	lease_start = 0;
    dhcp_cstate_t	next_state;
    struct in_addr	our_ip = { 0 };
    id 			q = nil;
    int			retries;
    id			saved_pkt = nil;
    boolean_t		saved_is_dhcp = FALSE;
    int			saved_rating = 0;
    id			saved_options = nil;
    struct in_addr	server_ip = { 0 };
    struct in_addr	src_ip = { 0 };
    dhcp_cstate_t	state;
    dhcp_time_secs_t	start_secs = current_secs();
    id			timer = nil;
    dhcp_time_secs_t	t1 = 0;
    dhcp_time_secs_t	t2 = 0;
    u_long		xid;
    dhcp_time_secs_t	wait_secs = INITIAL_WAIT_SECS;

    idstr = identifierToString(if_link_arptype(if_p), 
			       if_link_address(if_p), 
			       if_link_length(if_p));
    ts_log(LOG_DEBUG, "%s: h/w %s DHCP starting", if_name(if_p),
	   idstr);
    q = [[PacketQueue alloc] init];

    clear_ifstatus(ifstatus);

    timer = [[Timer alloc] init];

    xid = random();

    state = dhcp_cstate_init_e;

    /* use the previous lease if it exists */
    if (read_ip_lease(idstr, &our_ip, &expiration)
	&& (expiration == DHCP_INFINITE_SECS || start_secs < expiration)) {
	/* try to keep same address if there's time left on the lease */
	state = dhcp_cstate_init_reboot_e;
    }

    retries = max_retries;
    [receiver attach:q]; /* start receiving packets */

    bufsize = sizeof(struct dhcp) + DHCP_MIN_OPTIONS_SIZE;
    buf = malloc(bufsize);
    if (buf == NULL)
	goto done;

    do {
	id		options = nil;
	struct dhcp * 	request = NULL;
	int 		request_size = 0;
	int		try;

	ts_log(LOG_DEBUG, "%s state %s", 
	       if_name(if_p), dhcp_cstate_str(state));

	switch (state) {
	case dhcp_cstate_declining_e: {
	    /* decline the address */
	    request = make_dhcp_request((struct dhcp *)buf, bufsize,
					dhcp_msgtype_decline_e,
					if_link_address(if_p), 
					if_link_arptype(if_p),
					if_link_length(if_p), &options);
	    if (request == NULL)
		goto done;
	    if ([options addOption:dhcptag_requested_ip_address_e
			 Length:sizeof(our_ip) Data:&our_ip] == FALSE) {
		ts_log(LOG_ERR, "dhcp_thread: couldn't add our ip: %s",
		       [options errString]);
		[options free];
		goto done;
	    }
	    if ([options addOption:dhcptag_server_identifier_e
			 Length:sizeof(server_ip) Data:&server_ip] == FALSE) {
		ts_log(LOG_ERR, 
			"dhcp_thread(select): couldn't add server id: %s",
			[options errString]);
		[options free];
		goto done;
	    }
	    retries = 0;
	    break;
	}
	case dhcp_cstate_unbound_e:
	    /* stop using the IP address immediately */
	    mark_if_zeroes(if_p);
	    state = dhcp_cstate_init_e;
	    wait_secs = INITIAL_WAIT_SECS;
	    our_ip.s_addr = 0;
	    src_ip.s_addr = 0;
	    {
		/* wait a random time between 1 & 10 secs */
		struct timeval t;
	    
		t.tv_sec = random_range(1, 9);
		t.tv_usec = random_range(0, USECS_PER_SEC - 1);

		[q clearTimeout];
		[timer wake:q Interval:t];
		[q lock];
		while ([q timedOut] == FALSE)
		    [q wait];
		[q unlock];
		[q clearTimeout];
	    }
	    continue;

	case dhcp_cstate_init_e: {
#ifdef RANDOMIZE_START
	    /* wait a random time between 1 & 10 secs */
	    struct timeval t;
	    
	    t.tv_sec = random_range(1, 9);
	    t.tv_usec = random_range(0, USECS_PER_SEC - 1);
	    
	    [q clearTimeout];
	    [timer wake:q Interval:t];
	    [q lock];
	    while ([q timedOut] == FALSE)
		[q wait];
	    [q unlock];
	    [q clearTimeout];
#endif RANDOMIZE_START
	    request = make_dhcp_request((struct dhcp *)buf, bufsize,
					dhcp_msgtype_discover_e,
					if_link_address(if_p), 
					if_link_arptype(if_p),
					if_link_length(if_p), &options);
	    if (request == NULL)
		goto done;
	    {
		dhcp_lease_t 	l = dhcp_lease_hton(SUGGESTED_LEASE_LENGTH);
		if ([options addOption:dhcptag_lease_time_e 
		    Length:sizeof(l) Data:&l] == FALSE) {
		    ts_log(LOG_ERR, "dhcp_thread: couldn't add"
			   " lease time: %s", [options errString]);
		    [options free];
		    goto done;
		}
	    }
	    retries = max_retries;
	    dest_ip = ip_broadcast;
	    init_try++;
	    break;
	}

	case dhcp_cstate_select_e:
	    request = make_dhcp_request((struct dhcp *)buf, bufsize,
					dhcp_msgtype_request_e,
					if_link_address(if_p), 
					if_link_arptype(if_p),
					if_link_length(if_p), &options);
	    if (request == NULL)
		goto done;
	    retries = 1;
	    wait_secs = INITIAL_WAIT_SECS;

	    /* insert server identifier and requested ip address */
	    if ([options addOption:dhcptag_requested_ip_address_e
		Length:sizeof(our_ip) Data:&our_ip] == FALSE) {
		ts_log(LOG_ERR, "dhcp_thread(select): couldn't add our ip:"
			" %s", [options errString]);
		[options free];
		goto done;
	    }	    
	    if ([options addOption:dhcptag_server_identifier_e
		Length:sizeof(server_ip) Data:&server_ip] == FALSE) {
		ts_log(LOG_ERR, 
			"dhcp_thread(select): couldn't add server id: %s", 
			[options errString]);
		[options free];
		goto done;
	    }	    
	    dest_ip = ip_broadcast;
	    break;

	case dhcp_cstate_init_reboot_e:
	    request = make_dhcp_request((struct dhcp *)buf, bufsize,
					dhcp_msgtype_request_e,
					if_link_address(if_p), 
					if_link_arptype(if_p),
					if_link_length(if_p), &options);
	    if (request == NULL)
		goto done;
	    retries = 1;
	    if ([options addOption:dhcptag_requested_ip_address_e
		Length:sizeof(our_ip) Data:&our_ip] == FALSE) {
		ts_log(LOG_ERR, "dhcp_thread: couldn't add our ip: %s",
			[options errString]);
		[options free];
		goto done;
	    }	    
	    {
		dhcp_lease_t 	l = dhcp_lease_hton(SUGGESTED_LEASE_LENGTH);
		if ([options addOption:dhcptag_lease_time_e 
		    Length:sizeof(l) Data:&l] == FALSE) {
		    ts_log(LOG_ERR, "dhcp_thread: couldn't add"
			   " lease time: %s", [options errString]);
		    [options free];
		    goto done;
		}
	    }
	    dest_ip = ip_broadcast;
	    break;

	case dhcp_cstate_bound_e: {
	    /* make sure that the IP address is the same as the old one */
	    /* ie. check that if src_ip is nonzero, that src_ip == our_ip */
	    /* XXX */

	    src_ip = our_ip;

	    /* write the lease data (for init-reboot case) */
	    (void)write_ip_lease(idstr, our_ip, expiration);
	    
	    { /* set the interface's address and output the status */
		int		len;
		struct in_addr	mask = {0};
		void *		option;
		
		/* if IP address is in use, decline it */
		if (if_addr(if_p).s_addr == 0
		    && (ip_in_use(if_p, our_ip))) {
		  ts_log(LOG_ERR, 
			 "%s: supplied address " IP_FORMAT 
			 " in use, sending decline to " IP_FORMAT, 
			 if_name(if_p), IP_LIST(&our_ip),
			 IP_LIST(&server_ip));
		  if (saved_is_dhcp)
		      state = dhcp_cstate_declining_e;
		  else
		      state = dhcp_cstate_init_e;
		  ifstatus->in_use = TRUE;
		  clear_ip_lease(idstr);
		  continue;
		}
		option = [saved_options 
			     findOptionWithTag:dhcptag_subnet_mask_e
			     Length:&len];
		if (option)
		    mask = *((struct in_addr *)option);
		set_ip_and_mask(if_p, our_ip, mask);
	    }
	    clear_ifstatus(ifstatus);
	    post_ifstatus(ifstatus, saved_pkt, saved_options);

	    saved_pkt = saved_options = nil;
	    if (lease_length == DHCP_INFINITE_LEASE) {
		/* don't need to talk to server anymore, stop the thread */
		ts_log(LOG_DEBUG, "%s has infinite lease, thread stopping", 
		       if_name(if_p));
		goto done;
	    }
	    lockc_signal(&completion);
	    state = dhcp_cstate_renew_e;
	    init_try = 0;
	    continue;
	}
	case dhcp_cstate_renew_e:
	case dhcp_cstate_rebind_e: {
	    dhcp_time_secs_t 	current_time = current_secs();
	    dhcp_time_secs_t	wakeup_time;
#define RENEW_WAIT_SECS		60

	    ts_log(LOG_DEBUG, "current_time %d, expiration %d", 
		   current_time - start_secs,
		   expiration - start_secs);
	    if (current_time >= expiration) {
		ts_log(LOG_DEBUG, "current_time %d >= expiration %d", 
		       current_time - start_secs,
		       expiration - start_secs);
		/* lease has expired */
		state = dhcp_cstate_unbound_e;
		continue;
	    }
	    if (current_time < t1) {
		state = dhcp_cstate_renew_e;
		wakeup_time = t1;
		dest_ip = server_ip;
	    }
	    else if (current_time < t2) {
		state = dhcp_cstate_renew_e;
		wakeup_time = current_time + (t2 - current_time) / 2;
		dest_ip = server_ip;
	    }
	    else {
		state = dhcp_cstate_rebind_e;
		wakeup_time = current_time + (expiration - current_time) / 2;
		dest_ip = ip_broadcast;
	    }

	    /* while we sleep, detach our receive Q */
	    [receiver detach:q];
	    [timer cancel];
	    [q clearTimeout];
	    [q drain];

	    /* wait until wakeup_time */
	    [timer wake:q At:timeval_from_secs(wakeup_time)];

	    ts_log(LOG_DEBUG, "%s: sleeping until %d", if_name(if_p), 
		   wakeup_time - start_secs);
	    [q lock];
	    while ([q timedOut] == FALSE)
		[q wait];
	    [q unlock];
	    
	    if (ifstatus->please_exit) {
		ts_log(LOG_DEBUG, "DHCP over %s: exiting", if_name(if_p));
		goto done;
	    }
	    [receiver attach:q];
	    ts_log(LOG_DEBUG, "%s: waking up at %d", if_name(if_p), 
		   current_secs() - start_secs);
	    wait_secs = RENEW_WAIT_SECS;
	    request = make_dhcp_request((struct dhcp *)buf, bufsize,
					dhcp_msgtype_request_e,
					if_link_address(if_p), 
					if_link_arptype(if_p),
					if_link_length(if_p), &options);
	    if (request == NULL)
		goto done;
	    request->dp_ciaddr = our_ip;
	    {
		dhcp_lease_t 	l = dhcp_lease_hton(SUGGESTED_LEASE_LENGTH);
		if ([options addOption:dhcptag_lease_time_e 
		    Length:sizeof(l) Data:&l] == FALSE) {
		    ts_log(LOG_DEBUG, "dhcp_thread: couldn't add"
			   " lease time: %s", [options errString]);
		    [options free];
		    goto done;
		}
	    }
	    /* no retries */
	    retries = 0;
	    break;
	}
	default:
	    ts_log(LOG_DEBUG, "%s: state %d - top switch statement",
		   if_name(if_p), state);
	    goto done;
	}

	[saved_pkt free];
	[saved_options free];
	saved_pkt = saved_options = nil;
	
	if ([options addOption:dhcptag_end_e Length:0 Data:0] == FALSE) {
	    ts_log(LOG_DEBUG, "ipconfigd: couldn't add end tag");
	    [options free];
	    goto done;
	}

#ifdef DEBUG
	/* make sure the options parse OK */
	if ([options parse] == FALSE) {
	    ts_log(LOG_DEBUG, "ipconfigd: couldn't parse options in reply");
	    [options free];
	    goto done;
	}
#endif DEBUG

	request_size = sizeof(*request) + sizeof(rfc_magic) 
	    + [options bufferUsed];
	[options free];
	options = nil;

	if (request_size < sizeof(struct bootp)) {
	    /* pad out to BOOTP-sized packet */
	    request_size = sizeof(struct bootp);
	}
	/* send (retries + 1) times */
	next_state = state;
	for (try = 0; try < (retries + 1); try++) {
	    boolean_t		gathering = FALSE;
	    id			pkt = nil;
	    id			reply_options = nil;
	    dhcp_msgtype_t	reply_msg = dhcp_msgtype_none_e;
	    struct in_addr	ip;
	    struct timeval	tv;

	    request->dp_xid = htonl(++xid);
#ifdef DEBUG
	    print_packet(request, request_size);
#endif DEBUG

	    /* send the packet */
	    if ([transmitter transmit:if_name(if_p) 
			     DestHWType:request->dp_htype
			     DestHWAddr:NULL
			     DestHWLen:0
			     DestIP:dest_ip SourceIP:src_ip
			     DestPort:server_port SourcePort:client_port
			     Data:request Length:request_size] < 0) {
		goto done;
	    }

	    /* if we sent a decline, break out of the loop */
	    if (state == dhcp_cstate_declining_e)
		break; /* out of for */

	    /* wait for responses */
	    tv.tv_sec = wait_secs;
	    tv.tv_usec = random_range(0, USECS_PER_SEC - 1);
	    ts_log(LOG_DEBUG, "%s: waiting at %d for %d.%06d", if_name(if_p), 
		   current_secs() - start_secs,
		   tv.tv_sec, tv.tv_usec);
	    [timer cancel];
	    [q clearTimeout];
	    [timer wake:q Interval:tv];
	    while ((pkt = receive_packet(q, xid, if_p, &reply_options,
					 &reply_msg, &ip)) != nil) {
		struct dhcp * 	reply;
		dhcp_lease_t	l = 0;

		reply = (struct dhcp *)[pkt data];
		switch (state) {
		case dhcp_cstate_init_reboot_e:
		     if (reply_msg == dhcp_msgtype_nak_e) {
			 next_state = dhcp_cstate_unbound_e;
			 saved_pkt = pkt;
			 saved_options = reply_options;
			 goto got_response;
		     } 
		     /* fall through */
		case dhcp_cstate_init_e: {
		    boolean_t is_dhcp = TRUE;
		    if (ip_valid(reply->dp_yiaddr) == FALSE)
			goto reject_packet;
		    if (reply_msg == dhcp_msgtype_none_e)
			is_dhcp = FALSE;
		    if (is_dhcp == FALSE
			|| (((state == dhcp_cstate_init_e
			      && reply_msg == dhcp_msgtype_offer_e)
			     || (state == dhcp_cstate_init_reboot_e
				 && reply_msg == dhcp_msgtype_ack_e))
			    && get_lease(reply_options, &l))) {
			int rating = 0;

			rating = count_params(reply_options, dhcp_params,
					      n_dhcp_params);
			/* 
			 * The new packet is "better" than the saved
			 * packet if:
			 * - there was no saved packet, or
			 * - the new packet is a DHCP packet and the saved
			 *   one is a BOOTP packet or a DHCP packet with
			 *   a lower rating, or
			 * - the new packet and the saved packet are both
			 *   BOOTP but the new one has a higher rating
			 * All this to allow BOOTP/DHCP interoperability
			 * ie. we accept a BOOTP response if it's
			 * the only one we've got
			 * we expect/favour a DHCP response.
			 */
			if (saved_pkt == nil 
			    || (is_dhcp	== TRUE
				&& (saved_is_dhcp == FALSE 
				    || rating > saved_rating))
			    || (is_dhcp == FALSE 
				&& saved_is_dhcp == FALSE
				&& rating > saved_rating)) {
 			    [saved_pkt free];
			    [saved_options free];
			    server_ip = ip;
			    saved_pkt = pkt;
			    our_ip = reply->dp_yiaddr;
			    saved_is_dhcp = is_dhcp;
			    saved_options = reply_options;
			    saved_rating = rating;
			    lease_start = current_secs();
			    if (is_dhcp == FALSE) {
				/* BOOTP server responded, infinite lease */
				next_state = dhcp_cstate_bound_e;
				lease_length = DHCP_INFINITE_LEASE;
			    }
			    else {
				lease_length = l;
				if (state == dhcp_cstate_init_e) {
				    next_state = dhcp_cstate_select_e;
				}
				else {
				    lease_start = current_secs();
				    
				    next_state = dhcp_cstate_bound_e;
				    /* t1,t2 is overrideable by server XXX */
				    if (lease_length == DHCP_INFINITE_LEASE) {
					t1 = t2 = expiration 
					    = DHCP_INFINITE_SECS;
				    }
				    else {
					expiration 
					    = lease_start + lease_length;
					t1 = lease_start + (dhcp_lease_t) 
					    ((double)lease_length * 0.5);
					t2 = lease_start + (dhcp_lease_t) 
					    ((double)lease_length * 0.875);
				    }
				    ts_log(LOG_DEBUG, 
					   "t1 = %d, t2 = %d, expiration %d", 
					   t1 - start_secs, t2 - start_secs,
					   expiration - start_secs);
				}
				if (rating == n_dhcp_params) {
				    goto got_response;
				}
			    }
			    if (gathering == FALSE) {
				struct timeval t = {0,0};
				ts_log(LOG_DEBUG, 
				       "%s: gathering started at %d", 
				       if_name(if_p),
				       current_secs() - start_secs);
				gathering = TRUE;
				[timer cancel];
				[q clearTimeout];
				t.tv_sec = gather_secs;
				[timer wake:q Interval:t];
			    }
			    continue;
			}
		    }
		    break; /* init, init_reboot */
		}
		case dhcp_cstate_renew_e:
		case dhcp_cstate_rebind_e:
		case dhcp_cstate_select_e:
		    if (reply_msg != dhcp_msgtype_ack_e
			|| get_lease(reply_options, &l) == FALSE
			|| server_ip.s_addr != ip.s_addr) {
			if (reply_msg == dhcp_msgtype_nak_e
			    && server_ip.s_addr == ip.s_addr) {
			    next_state = dhcp_cstate_unbound_e;
			    saved_is_dhcp = TRUE;
			    saved_pkt = pkt;
			    saved_options = reply_options;
			    goto got_response;
			}
			break; /* out of switch */
		    }
		    lease_start = current_secs();
		    next_state = dhcp_cstate_bound_e;
		    lease_length = l;

		    /* need to check whether t1,t2 overrided by server XXX */

		    if (lease_length == DHCP_INFINITE_LEASE) {
			t1 = t2 = expiration = DHCP_INFINITE_SECS;
		    }
		    else {
			expiration = lease_start + lease_length;
			t1 = lease_start + 
			    (dhcp_lease_t) ((double)lease_length * 0.5);
			t2 = lease_start +
			    (dhcp_lease_t) ((double)lease_length * 0.875);
		    }
		    ts_log(LOG_DEBUG, "t1 = %d, t2 = %d, expiration %d", 
			   t1 - start_secs, t2 - start_secs,
			   expiration - start_secs);
		    saved_pkt = pkt;
		    saved_options = reply_options;
		    goto got_response;
		    break; /* select, renew, rebind */
		default:
		    /* not reached */
		    ts_log(LOG_DEBUG, "%s: retry loop state %s????", 
			   if_name(if_p), dhcp_cstate_str(state));
		    goto done;
		    break;
		}
	    reject_packet:
		[pkt free];
		[reply_options free];
	    } /* while */
	    
	got_response:
	    [timer cancel];
	    if (ifstatus->please_exit)
		break;

	    if (saved_pkt != nil || next_state != state) {
		break; /* out of for */
	    }
	    wait_secs *= 2;
	    if (wait_secs > MAX_WAIT_SECS)
		wait_secs = MAX_WAIT_SECS;
	} /* for */
	
	state = next_state;
	if (saved_pkt == nil) {
	    switch (state) {
	    case dhcp_cstate_select_e:
	    case dhcp_cstate_init_reboot_e:
	    case dhcp_cstate_declining_e:
		state = dhcp_cstate_init_e;
		wait_secs = INITIAL_WAIT_SECS;
		break;
	    case dhcp_cstate_init_e:
	    case dhcp_cstate_rebind_e:
	    case dhcp_cstate_renew_e:
	    case dhcp_cstate_unbound_e:
		break;
	    default:
		/* not reached */
		ts_log(LOG_DEBUG, "%s: after retry state %s???", 
		       if_name(if_p), dhcp_cstate_str(state));
		goto done;
		break;
	    }
	}
	if (ifstatus->please_exit)
	    break; /* out of do-while */
#define MAX_INIT_TRIES	1
	if (state == dhcp_cstate_init_e) {
	    if (init_try >= MAX_INIT_TRIES)
		break; /* out of do-while */
	}
    } while (1); /* do-while */
 done:
    if (buf)
	free(buf);
    if (idstr)
	free(idstr);
    [receiver detach:q]; /* stop receiving packets */
    [[q freeValues] free];
    
    ts_log(LOG_DEBUG, "%s: DHCP loop stopped at %d", if_name(if_p), 
	   current_secs() - start_secs);
    if (if_addr(if_p).s_addr == 0)
	mark_if_down(if_p);
    ifstatus->ready = TRUE;
    ifstatus->thread = NULL;
    lockc_signal(&completion);
    UTHREAD_EXIT(0);
    return;
}

void
usage(u_char * progname)
{
    fprintf(stderr, "useage: %s <options>\n"
	    "<options> are:\n"
	    "-g secs    : gather time in seconds [default 2]\n"
	    "-r count   : retry count [default: 2] \n",
	    progname);
    exit(USER_ERROR);
}

static int
bootp_socket()
{
    struct sockaddr_in 		me;
    int 			status;
    int 			opt;
    int				sockfd;
    struct timeval		timeout;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
	perror("socket");
	exit(UNEXPECTED_ERROR);
    }
    bzero((char *)&me, sizeof(me));
    me.sin_family = AF_INET;
    me.sin_port = htons(client_port);
    me.sin_addr.s_addr = htonl(INADDR_ANY);
    
    status = bind(sockfd, (struct sockaddr *)&me, sizeof(me));
    if (status != 0) {
	ts_log(LOG_ERR, "bind failed");
	exit(UNEXPECTED_ERROR);
    }
    opt = 1;
    status = setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &opt, 
			sizeof(opt));
    if (status < 0)	{
	ts_log(LOG_ERR, "setsockopt SO_BROADCAST");
	exit(UNEXPECTED_ERROR);
    }
#if 0
    opt = 1;
    status = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, 
			sizeof(opt));
    if (status < 0) {
	ts_log(LOG_ERR, "setsockopt SO_SOREUSEADDR");
	exit(UNEXPECTED_ERROR);
    }
#endif 0
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (caddr_t)&timeout,
		   sizeof(timeout)) < 0) {
	ts_log(LOG_ERR, "setsockopt SO_RCVTIMEO");
	exit(UNEXPECTED_ERROR);
    }
    return sockfd;
}

#if 0
u_int
make_if_bit_list(interface_list_t * list_p)
{
    int i;
    u_int bits = 0;

    for (i = 0; i < if_count(list_p) && i < 32; i++) {
	interface_t * if_p = if_at_index(list_p, i);
	if (if_link_arptype(if_p) == ARPHRD_ETHER) {
	    bits |= (1 << i);
	}
    }
    return (bits);
}
#endif 0

#define IFTAB		"/etc/iftab"

static __inline__ boolean_t
change_in_ipcfg()
{
    boolean_t			changed = FALSE;
    static struct timespec	modtime = {0,0};
    struct stat			sb;

    if (stat(IFTAB, &sb) < 0) {
	changed = TRUE;
	ts_log(LOG_DEBUG, "couldn't stat '" IFTAB "', %m");
    }
    else {
	if (bcmp(&sb.st_mtimespec, &modtime, sizeof(modtime))) {
	    changed = TRUE;
	}
	modtime = sb.st_mtimespec;
    }
    return (changed);
}

static __inline__ ipcfg_table_t *
read_ipcfg()
{
    ipcfg_table_t * 	cfg = NULL;
    FILE * 		f;
    char 		msg[512];

    /* get the interface configuration information */
    /* read /etc/iftab */
    f = fopen(IFTAB, "r");
    if (f == NULL) {
	ts_log(LOG_ERR, "couldn't open '" IFTAB "', %m");
    }
    else {
	cfg = ipcfg_from_iftab(f, msg);
	fclose(f);
	if (cfg == NULL) {
	    ts_log(LOG_ERR, "error parsing '" IFTAB "', '%s'", msg);
	}
    }
    return (cfg);
}

static __inline__ boolean_t
ifstatus_all_done()
{
    struct ifstatus * 	s;

    for (s = ifstatus_list; s; s = s->next) {
	switch (s->mode) {
	  case ipcfg_mode_inform_e:
	  case ipcfg_mode_dhcp_e:
	  case ipcfg_mode_bootp_e:
	      if (s->ready == FALSE)
		  return (FALSE);
	      break;
	  default:
	      break;
	}
    }
    return (TRUE);
}

int
get_if_count()
{
    return (ifstatus_count);
}

boolean_t 
get_if_name(int intface, char * name)
{
    int 		i;
    boolean_t		ret = FALSE;
    struct ifstatus * 	s;

    lockc_lock(&completion);
    for (s = ifstatus_list, i = 0; s; s = s->next, i++) {
	if (i == intface) {
	    strcpy(name, if_name(&s->ifa));
	    ret = TRUE; /* out of for loop */
	    break;
	}
    }
    lockc_unlock(&completion);
    return (ret);
}

boolean_t 
get_if_addr(char * name, u_int32_t * addr)
{
    boolean_t		ret = FALSE;
    struct ifstatus * 	s;

    lockc_lock(&completion);
    for (s = ifstatus_list; s; s = s->next) {
	boolean_t name_match = (strcmp(if_name(&s->ifa), name) == 0);
	if (name_match) {
	    if (s->ifa.inet_valid) {
		*addr = if_addr(&s->ifa).s_addr;
		ret = TRUE;
	    }
	    break; /* out of for loop */
	}
    }
    lockc_unlock(&completion);
    return (ret);
}

boolean_t
get_if_option(char * name, int option_code, void * option_data, 
	      unsigned int * option_dataCnt)
{
    boolean_t		ret = FALSE;
    struct ifstatus * 	s;

    lockc_lock(&completion);
    for (s = ifstatus_list; s; s = s->next) {
	boolean_t name_match = (strcmp(if_name(&s->ifa), name) == 0);
	if (name[0] == '\0' || name_match) {
	    switch (s->mode) {
	      case ipcfg_mode_inform_e:
	      case ipcfg_mode_dhcp_e:
	      case ipcfg_mode_bootp_e: {
		  void * data;
		  int	 len;

		  if (s->ready == FALSE || s->in_use)
		      break; /* out of switch */
		  data = [s->options findOptionWithTag:option_code 
			   Length:&len];
		  if (data) {
		      if (len > *option_dataCnt) {
			  ret = FALSE;
			  break;
		      }
		      *option_dataCnt = len;
		      bcopy(data, option_data, *option_dataCnt);
		      ret = TRUE;
		  }
		  break;
	      }
	      default:
		  break;
	    } /* switch */
	    if (ret == TRUE || name_match)
		break; /* out of for */
	} /* if */
    } /* for */
    lockc_unlock(&completion);
    return (ret);
}

boolean_t
get_if_packet(char * name, void * packet_data, unsigned int * packet_dataCnt)
{
    boolean_t		ret = FALSE;
    struct ifstatus * 	s;

    lockc_lock(&completion);
    for (s = ifstatus_list; s; s = s->next) {
	if (strcmp(if_name(&s->ifa), name) == 0) {
	    switch (s->mode) {
	      case ipcfg_mode_inform_e:
	      case ipcfg_mode_dhcp_e:
	      case ipcfg_mode_bootp_e: {
		  if (s->ready == FALSE)
		      break; /* out of switch */
		
		  if ([s->pkt size] > *packet_dataCnt) {
		      ret = FALSE;
		      break;
		  }
		  *packet_dataCnt = [s->pkt size];
		  bcopy([s->pkt data], packet_data, *packet_dataCnt);
		  ret = TRUE;
		  break;
	      }
	      default:
		  break;
	    } /* switch */
	    break; /* out of for */
	} /* if */
    } /* for */
    lockc_unlock(&completion);
    return (ret);
}

boolean_t
wait_any_if()
{
    boolean_t		ret = FALSE;
    struct ifstatus * 	s;

    lockc_lock(&completion);
    do {
	for (s = ifstatus_list; s; s = s->next) {
	    switch (s->mode) {
	      case ipcfg_mode_inform_e:
	      case ipcfg_mode_dhcp_e:
	      case ipcfg_mode_bootp_e:
		  if (s->ready == TRUE) {
		      ret = TRUE;
		      goto done;
		  }
		  break;
	      default:
		  break;
	    }
	}
	lockc_wait(&completion);
    } while (1);
 done:
    lockc_unlock(&completion);
    return (ret);
}

boolean_t
wait_if(char * name)
{
    boolean_t		ret = FALSE;
    struct ifstatus * 	s;

    if (name[0] == '\0')
	return (wait_any_if());

    lockc_lock(&completion);
    for (s = ifstatus_list; s; s = s->next) {
	if (strcmp(name, if_name(&s->ifa)) == 0) {
	    switch (s->mode) {
	      case ipcfg_mode_inform_e:
	      case ipcfg_mode_dhcp_e:
	      case ipcfg_mode_bootp_e:
		  while (s->ready == FALSE)
		      lockc_wait(&completion);
		  break;
	      default:
		  break;
	    }
	    ret = TRUE;
	    break; /* out of for */
	}
    }
    lockc_unlock(&completion);
    return (ret);
}

void
wait_all()
{
    lockc_lock(&completion);
    while (ifstatus_all_done() == FALSE)
	lockc_wait(&completion);
    lockc_unlock(&completion);
    return;
}

static void
init_dynamic()
{
    static boolean_t 	inited = FALSE;
    int 		sockfd;

    if (inited)
	return;

    inited = TRUE;
    sockfd = bootp_socket();
    transmitter = [[[Transmitter alloc] init] setSocket:sockfd];
    if (transmitter == nil) {
	ts_log(LOG_ERR, "unable to allocate Transmitter");
	exit(UNEXPECTED_ERROR);
    }

    receiver = [[Receiver alloc] initWithSocket:sockfd];
    if (receiver == nil) {
	ts_log(LOG_ERR, "unable to allocate Receiver");
	exit(UNEXPECTED_ERROR);
    }
    [receiver start];
    return;
}

static void
config_if()
{
    ipcfg_table_t *	ipcfg_list = NULL;
    int			i;
    interface_list_t *	iflist_p;

#if 0
    if (!change_in_ipcfg())
	return;
#endif 0

    ipcfg_list = read_ipcfg();
    if (ipcfg_list == NULL) {
	ts_log(LOG_ERR, "config_if(): failed to get ip config");
	exit(UNEXPECTED_ERROR);
    }

#ifdef DEBUG
    ipcfg_print(ipcfg_list);
#endif DEBUG

    iflist_p = if_init();
    if (iflist_p == NULL) {
	ts_log(LOG_ERR, "config_if(): if_init failed");
	exit(UNEXPECTED_ERROR);
    }

    for (i = 0; i < if_count(iflist_p); i++) {
	interface_t * 	intface = if_at_index(iflist_p, i);
	interface_t *	if_p = NULL;
	ipcfg_t *	ipcfg = ipcfg_lookup(ipcfg_list, if_name(intface));
	struct ifstatus *r;

	r = calloc(1, sizeof(*r));
	if (r == NULL) {
	    ts_log(LOG_ERR, "config_if(): calloc ifstatus failed");
	    exit (1);
	}

	r->ifa = *intface;
	if_p = &r->ifa;

	if (ifstatus_list == NULL)
	    ifstatus_list = r;
	else {
	    r->next = ifstatus_list;
	    ifstatus_list = r;
	}
	ifstatus_count++;

	if (ipcfg == NULL) {
	    r->mode = ipcfg_mode_none_e;
	}
	else {
	    r->mode = ipcfg->mode;
	    /*
	     * The correct check is for broadcast interfaces, but
	     * since bpf only works with ethernet currently, 
	     * we make sure it's ethernet as well.
	     */
	    if ((if_flags(if_p) & IFF_BROADCAST) == 0
		|| (if_link_arptype(if_p) != ARPHRD_ETHER)) {
		switch (ipcfg->mode) {
		  case ipcfg_mode_inform_e:
		  case ipcfg_mode_dhcp_e:
		  case ipcfg_mode_bootp_e:
		      /* can't do DHCP/BOOTP over non-broadcast interfaces */
		      r->mode = ipcfg_mode_none_e;
		      break;
		  default:
		      break;
		}
	    }
	}
	switch (r->mode) {
	  case ipcfg_mode_inform_e:
	  case ipcfg_mode_manual_e: {
	      if (if_flags(if_p) & IFF_LOOPBACK) {
		  continue;
	      }
	      if ((ipcfg->up && (if_flags(if_p) & IFF_UP) == 0)
		  || (ipcfg->up == FALSE && (if_flags(if_p) & IFF_UP))) {
		  short 	flags = if_flags(if_p);
		  int 		s = ifconfig_socket();

		  if (s >= 0) {
		      if (ipcfg->up)
			  flags |= IFF_UP;
		      else
			  flags &= ~IFF_UP;
		      ifconfig_flags(s, if_name(if_p), flags);
		      if_setflags(if_p, flags);
		      
		      close(s);
		  }
	      }
	      set_ip_and_mask(if_p, ipcfg->addr, ipcfg->mask);
	      if (r->mode == ipcfg_mode_inform_e) {
		  UTHREAD_FUNC_T func;
		  func = (UTHREAD_FUNC_T)inform_thread;
		  init_dynamic();
		  if (UTHREAD_CREATE(&r->thread, func, r)) {
		      ts_log(LOG_ERR, "unable to create thread on %s",
			     if_name(if_p));
		  }
	      }
	      break;
	  }

	  case ipcfg_mode_dhcp_e:
	  case ipcfg_mode_bootp_e: {
	      UTHREAD_FUNC_T func;

	      if (r->mode == ipcfg_mode_dhcp_e)
		  func = (UTHREAD_FUNC_T)dhcp_thread;
	      else
		  func = (UTHREAD_FUNC_T)bootp_thread;

	      if (if_p->inet_valid == FALSE)
		  set_ip_and_mask(if_p, ip_zeroes, ip_zeroes);
	      init_dynamic();
	      if (UTHREAD_CREATE(&r->thread, func, r)) {
		  ts_log(LOG_ERR, "unable to create thread on %s",
			 if_name(if_p));
	      }
	      break;
	  }

	  case ipcfg_mode_off_e: {
	      int s = ifconfig_socket();
	      
	      if (s < 0)
		  ;
	      else {
		  if (if_p->inet_valid) {
		      if (ifconfig_delete(s, if_name(if_p), NULL) < 0) {
			  ts_log(LOG_ERR, "%s: ifconfig_delete() failed, %m",
				 if_name(if_p));
		      }
		  }
		  if (if_flags(if_p) & IFF_UP) {
		      if (ifconfig_flags(s, if_name(if_p), 
					 if_flags(if_p) & ~IFF_UP) < 0) {
			  ts_log(LOG_ERR, 
				 "%s: ifconfig_flags(down) failed, %m",
				 if_name(if_p));
		      }
		  }
		  close(s);
	      }
	      break;
	  }

	  case ipcfg_mode_none_e:
	  default: {
	      /* leave the interface alone */
	      break;
	  }
	} /* switch */
    } /* for */
#ifdef DEBUG
    dump_ifstatus_list(ifstatus_list);
#endif DEBUG
    if_free(&iflist_p);
}

int 
main(int argc, char *argv[])
{
    int 		ch;
#if 0
    u_int		interfaces = 0;
#endif 0
    u_char *		progname = argv[0];
    boolean_t		testing = FALSE;

    objc_setMultithreaded(YES);

    { /* set the random seed */
	struct timeval	start_time;

	gettimeofday(&start_time, 0);
	srandom(start_time.tv_usec & ~start_time.tv_sec);
    }

    while ((ch = getopt(argc, argv, "bc:deghH:r:s:Tv")) != EOF) {
	switch ((char) ch) {
#if 0
	case 'a':
	    interfaces = make_if_bit_list(iflist_p);
	    break;
#endif 0
	case 'b':
	    must_broadcast = TRUE;
	    break;
	case 'c': /* client port - for testing */
	    client_port = atoi(optarg);
	    break;
	case 'd':
	    debug = TRUE;
	    break;
	case 'e': /* send the request and exit */
	    exit_quick = TRUE;
	    break;
	case 'g': /* gather time */
	    gather_secs = strtoul(optarg, NULL, NULL);
	    break;
	case 'v':
	    verbose = TRUE;
	    break;
#if 0
	case 'i': {
	    interface_t * if_p = if_lookupbyname(iflist_p, optarg);
	    if (if_p == NULL) {
		fprintf(stderr, "%s: no such interface '%s'\n", 
			progname, optarg);
		exit(USER_ERROR);
	    }
	    if (if_link_arptype(if_p) != ARPHRD_ETHER) {
		fprintf(stderr, "%s: '%s' not ethernet\n", progname,
			optarg);
		exit(USER_ERROR);
	    }
	    interfaces |= 1 << if_index(iflist_p, if_p);
	    break;
	}
#endif 0
	case 'r': /* retry count */
	    max_retries = strtoul(optarg, NULL, NULL);
	    break;
	case 's': /* server port - for testing */
	    server_port = atoi(optarg);
	    break;
	case 'T': /* log and wait for all responses */
	    testing = TRUE;
	    break;
	case 'H':
	case 'h':
	    usage(progname);
	    break;
	}
    }


    if ((argc - optind) != 0)
	usage(progname);

    lockc_init(&completion);

    ts_log_init(verbose);

    if (debug)
	(void) openlog("ipconfigd", LOG_PERROR | LOG_PID, LOG_DAEMON);
    else {
	daemon(0, 0);
	(void) openlog("ipconfigd", LOG_CONS | LOG_PID, LOG_DAEMON);
    }

#ifdef WRITE_IFSTATUS
    if (create_path(TMP_IPCONFIGD_DIR, 0700) < 0) {
	ts_log(LOG_DEBUG, "config_if(): failed to create " TMP_IPCONFIGD_DIR 
		", %m");
	exit(UNEXPECTED_ERROR);
    }
    if (create_path(IPCONFIGD_DIR, 0700) < 0) {
	ts_log(LOG_DEBUG, "config_if(): failed to create " 
		IPCONFIGD_DIR ", %m");
	exit(UNEXPECTED_ERROR);
    }
#endif WRITE_IFSTATUS
    if (create_path(LEASE_DIR, 0700) < 0) {
	ts_log(LOG_DEBUG, "config_if(): failed to create " 
		LEASE_DIR ", %m");
	exit(UNEXPECTED_ERROR);
    }
    server_init();

    arp = [[ARPProbe alloc] init];

    config_if(testing);
    server_loop();

    ts_log(LOG_DEBUG, "Waiting for receiver termination");
    [receiver free];
    ts_log(LOG_DEBUG, "Receiver terminated");
    [transmitter free];

    exit(0);
}
