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
 * ARPProbe.m
 * - check whether an IP address is in use
 * - uses BPF to send/receive
 */

/*
 * Modification History:
 * 
 * May 19, 1999	Dieter Siegmund (dieter@apple.com)
 * - initial revision
 */

#import <stdlib.h>
#import <unistd.h>
#import <string.h>
#import <stdio.h>
#import <sys/types.h>
#import <sys/time.h>
#import <sys/errno.h>
#import <sys/socket.h>
#import <sys/ioctl.h>
#import <ctype.h>
#import <net/if.h>
#import <net/etherdefs.h>
#import <netinet/in.h>
#import <netinet/udp.h>
#import <netinet/in_systm.h>
#import <netinet/ip.h>
#import <arpa/inet.h>
#import <net/if_arp.h>
#import <netinet/if_ether.h>
#import <net/bpf.h>
#import <syslog.h>
#import "ts_log.h"

#import "ARPProbe.h"
#import "bpflib.h"
#import "util.h"
#import "dprintf.h"

extern char *  			ether_ntoa(struct ether_addr *e);
extern struct ether_addr *	ether_aton(char *);

static __inline__ char *
arpop_name(u_int16_t op)
{
    switch (op) {
    case ARPOP_REQUEST:
	return "ARP REQUEST";
    case ARPOP_REPLY:
	return "ARP REPLY";
    case ARPOP_REVREQUEST:
	return "REVARP REQUEST";
    case ARPOP_REVREPLY:
	return "REVARP REPLY";
    default:
	break;
    }
    return ("<unknown>");
}

int
make_arp_request(char * buf, struct ether_addr * eaddr,
		 struct in_addr sender_ip, struct in_addr target_ip)
{
    struct arphdr *	hdr;
    char * 		pos;
    
    /* fill in the fixed-length header */
    hdr = (struct arphdr *)buf;
    hdr->ar_hrd = htons(ARPHRD_ETHER);
    hdr->ar_pro = htons(ETHERTYPE_IP);
    hdr->ar_hln = sizeof(*eaddr);
    hdr->ar_pln = sizeof(sender_ip);
    hdr->ar_op = htons(ARPOP_REQUEST);
    
    pos = buf + sizeof(*hdr);
    
    bcopy(eaddr, pos, sizeof(*eaddr)); 		/* SENDER H/W ADDR */
    pos += sizeof(*eaddr);
    bcopy(&sender_ip, pos, sizeof(sender_ip));	/* SENDER IP */ 
    pos += sizeof(sender_ip);
    bzero(pos, sizeof(*eaddr));			/* TARGET H/W ADDR */
    pos += sizeof(*eaddr);
    bcopy(&target_ip, pos, sizeof(target_ip));	/* TARGET IP */
    pos += sizeof(target_ip);
    dprintf(("ARP Request size is %d\n", pos - buf));
    return (pos - buf);
}

#if 0
static __inline__ void
dump_arp(char * pkt, int len)
{
    struct in_addr   	ip;
    struct arphdr * 	arp;
    struct bpf_hdr * 	bpf = (struct bpf_hdr *)pkt;
    char *		pkt_start;
    char *	     	pos;
	    

    pkt_start = pkt + bpf->bh_hdrlen;
    arp = (struct arphdr *) (pkt_start + sizeof(struct ether_header));
    dprintf(("\n%s\n", arpop_name(ntohs(arp->ar_op))));
    pos = ((char *)arp) + sizeof(*arp);
    dprintf(("Sender H/W\t%s\n", 
	     ether_ntoa((struct ether_addr *)pos)));
    pos += sizeof(*sender_eaddr);
    ip = *((struct in_addr *)pos);
    dprintf(("Sender IP\t%s\n", inet_ntoa(ip)));
    pos += sizeof(ip);
    dprintf(("Target H/W\t%s\n", 
	     ether_ntoa((struct ether_addr *)pos)));
    pos += sizeof(*sender_eaddr);
    ip = *((struct in_addr *)pos);
    dprintf(("Target IP\t%s\n", inet_ntoa(ip)));
    return;
}
#endif

static struct ether_addr ether_broadcast = { 
    {0xff, 0xff, 0xff, 0xff, 0xff, 0xff} 
};

@implementation ARPProbe
- (int) transmit:(int) bpf_fd
	    Type:(short) type
	    Data:(void *) data 
	  Length:(int) len
{
    struct ether_header * 	eh_p;
    char *			payload;
    int 			status = 0;

    eh_p = (struct ether_header *)sendbuf;
    payload = sendbuf + sizeof(*eh_p);

    bcopy(&ether_broadcast, eh_p->ether_dhost,
	  sizeof(eh_p->ether_dhost));
    eh_p->ether_type = htons(type);
    bcopy(data, payload, len);
    status = bpf_write(bpf_fd, sendbuf, sizeof(*eh_p) + len);
    if (status < 0)
	ts_log(LOG_ERR, "ARPProbe transmit: bpf_write() failed: %m");
    return (status);
}

- init
{
    [super init];
    mutex_init(&lock);
    condition_init(&wakeup);
    busy = FALSE;
    return self;
}

- free
{
    mutex_clear(&lock);
    condition_clear(&wakeup);
    return [super free];
}

- (int)probeInterface:(char *) if_name 
	     SenderHW:(struct ether_addr *) sender_eaddr
	     SenderIP:(struct in_addr) sender_ip 
	     TargetIP:(struct in_addr) target_ip
		InUse:(boolean_t *) in_use
{
    struct timeval	current_time;
    boolean_t 		done;
    int 		fd;
    int			n;
    char *		rxbuf = NULL;
    u_int		rxbufsize;
    struct timeval 	start_time;
    int 		status = 0;
    char		txbuf[64];
    int			txsize = 0;
    
    /* only allow a single thread */
    mutex_lock(&lock);
    while (busy == TRUE) {
	dprintf(("waiting for ARPProbe %s\n", if_name));
	condition_wait(&wakeup, &lock);
    }
    busy = TRUE;
    mutex_unlock(&lock);
    dprintf(("ARPProbe starting for %s\n", if_name));
    
    *in_use = FALSE;

    fd = bpf_new();
    if (fd < 0) {
	ts_log(LOG_ERR, "ARPProbe probe: bpf_new(%s) failed, %m", if_name);
	goto error;
    }

    /* don't wait for packets to be buffered */
    bpf_set_immediate(fd, 1);

    /* get the receive buffer size */
    status = bpf_get_blen(fd, &rxbufsize);
    if (status < 0) {
	ts_log(LOG_ERR, 
	       "ARPProbe probe: bpf_get_blen(%s) failed, %m", if_name);
	goto error;
    }

    /* create a buffer of that size */
    rxbuf = malloc(rxbufsize);
    if (rxbuf == NULL) {
	ts_log(LOG_ERR, "ARPProbe probe: malloc failed, %m");
	goto error;
    }

    /* associate it with the given interface */
    status = bpf_setif(fd, if_name);
    if (status < 0) {
	ts_log(LOG_ERR, "ARPProbe probe: bpf_setif(%s) failed: %m", if_name);
	goto error;
    }

    { /* set the receive timeout */
	struct timeval tv;

	tv.tv_sec = 1;
	tv.tv_usec = 0;
	/* tv.tv_usec = random_range(0, USECS_PER_SEC - 1); */
	status = bpf_set_timeout(fd, &tv);
	if (status < 0) {
	    ts_log(LOG_ERR, "ARPProbe probe: bpf_set_timeout(%s) failed: %m", 
		   if_name);
	    goto error;
	}
    }

    /* set the filter to return only ARP packets */
    status = bpf_arp_filter(fd);
    if (status < 0) {
	ts_log(LOG_ERR, "ARPProbe probe: bpf_arp_filter(%s) failed: %m", 
	       if_name);
	goto error;
    }

    /* create the ARP request packet */
    bzero(txbuf, sizeof(txbuf));
    txsize = make_arp_request(txbuf, sender_eaddr, sender_ip, target_ip);

#define WAIT_SECS	2

    dprintf(("Transmitting ARP request for %s\n", inet_ntoa(target_ip)));
    status = [self transmit:fd Type:ETHERTYPE_ARP 
		   Data:txbuf Length:txsize];
    if (status < 0) {
	ts_log(LOG_ERR, "ARPProbe probe: transmit(%s) failed, %m", if_name);
	goto error;
    }
    gettimeofday(&start_time, 0);
    for (done = FALSE; !done; ) {
	n = read(fd, rxbuf, rxbufsize);
	if (n < 0) {
	    status = n;
	    ts_log(LOG_ERR, "ARPProbe probe: read(%s) failed, %m", if_name);
	    break;
	}
	gettimeofday(&current_time, 0);
	if ((current_time.tv_sec - start_time.tv_sec) >= WAIT_SECS)
	    done = TRUE;
	if (n > 0) {
	    u_int16_t		op;
	    struct in_addr   	ip;
	    struct arphdr * 	arp;
	    struct bpf_hdr * 	bpf = (struct bpf_hdr *)rxbuf;
	    char *		pkt_start;
	    char *	     	pos;

	    dprintf(("read %d captured %d\n", n, bpf->bh_caplen));
	    
	    pkt_start = rxbuf + bpf->bh_hdrlen;
	    arp = (struct arphdr *) (pkt_start + sizeof(struct ether_header));
	    op = ntohs(arp->ar_op);
	    dprintf(("\n%s\n", arpop_name(op)));
	    if (op != ARPOP_REPLY)
		continue;
	    pos = ((char *)arp) + sizeof(*arp);
	    dprintf(("Sender H/W\t%s\n", 
		     ether_ntoa((struct ether_addr *)pos)));
	    pos += sizeof(*sender_eaddr);
	    ip = *((struct in_addr *)pos);
	    dprintf(("Sender IP\t%s\n", inet_ntoa(ip)));
	    if (ip.s_addr != target_ip.s_addr)
		continue;
	    pos += sizeof(ip);
	    dprintf(("Target H/W\t%s\n", 
		     ether_ntoa((struct ether_addr *)pos)));
	    if (bcmp(pos, sender_eaddr, sizeof(*sender_eaddr))) 
		continue;
	    pos += sizeof(*sender_eaddr);
	    ip = *((struct in_addr *)pos);
	    dprintf(("Target IP\t%s\n", inet_ntoa(ip)));
	    if (ip.s_addr != sender_ip.s_addr)
		continue;
	    *in_use = TRUE;
	    done = TRUE;
	}
	else {
	    dprintf(("No packets at %d\n", 
		     current_time.tv_sec - start_time.tv_sec));
	    if (done == TRUE)
		break;
	}
    }
 error:
    if (rxbuf != NULL)
	free(rxbuf);
    if (fd >= 0)
	bpf_dispose(fd);

    mutex_lock(&lock);
    busy = FALSE;
    condition_signal(&wakeup);
    mutex_unlock(&lock);
    return (status);
}
@end

#ifdef TESTING
#import "interfaces.h"

int
main(int argc, char * argv[])
{
    interface_t *	if_p;
    interface_list_t *	list_p;
    boolean_t 		in_use = FALSE;
    id			p;
    struct in_addr 	sender_ip;
    struct in_addr     	target_ip;

    if (argc < 4) {
	printf("usage: arpcheck <ifname> <sender ip> <target ip>\n");
	exit(1);
    }

    list_p = if_init();
    if (list_p == NULL) {
	printf("couldn't get interface list\n");
	exit(1);
    }

    if_p = if_lookupbyname(list_p, argv[1]);
    if (if_p == NULL) {
	printf("interface %s does not exist\n", argv[1]);
	exit(1);
    }

    p = [[ARPProbe alloc] init];
    if (inet_aton(argv[2], &sender_ip) == 0) {
	printf("invalid ip address %s\n", argv[3]);
	exit(1);
    }
    if (inet_aton(argv[3], &target_ip) == 0) {
	printf("invalid ip address %s\n", argv[4]);
	exit(1);
    }

    if ([p probeInterface:if_p->name SenderHW:if_link_address(if_p)
	   SenderIP:sender_ip TargetIP:target_ip
	   InUse:&in_use] < 0) {
	perror("ARPProbe failed\n");
    }
    else {
	if (in_use)
	    printf("%s in use\n", argv[3]);
	else
	    printf("%s not in use\n", argv[3]);
    }
    [p free];
    exit(0);
}
#endif TESTING
