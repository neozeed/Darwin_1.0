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
 * bootpd.m
 * - BOOTP/DHCP server main
 * - see RFC951, RFC2131, RFC2132 for details on the BOOTP protocol, 
 *   BOOTP extensions/DHCP options, and the DHCP protocol
 */

/*
 * Modification History
 * 01/22/86	Croft	created.
 *
 * 03/19/86	Lougheed  Converted to run under 4.3 BSD inetd.
 *
 * 09/06/88	King	Added NeXT interrim support.
 *
 * 02/23/98	Dieter Siegmund (dieter@apple.com)
 *		- complete overhaul
 *		- added specialized Mac NC support
 *		- removed the NeXT "Sexy Net Init" code that performed
 *		  a proprietary form of dynamic BOOTP, since this
 *		  functionality is replaced by DHCP
 *		- added ability to respond to requests originating from 
 *		  a specific set of interfaces
 *		- added rfc2132 option handling
 *
 * June 5, 1998 	Dieter Siegmund (dieter@apple.com)
 * - do lookups using netinfo calls directly to be able to read/write
 *   entries and get subnet-specific bindings
 *
 * Oct 19, 1998		Dieter Siegmund (dieter@apple.com)
 * - provide domain name servers for this server if not explicitly 
 *   configured otherwise
 * Mar 29, 1999		Dieter Siegmund (dieter@apple.com)
 * - added code to do ethernet lookups with or without leading zeroes
 */

#import <unistd.h>
#import <stdlib.h>
#import <sys/stat.h>
#import <sys/socket.h>
#import <sys/ioctl.h>
#import <sys/file.h>
#import <sys/time.h>
#import <sys/types.h>
#import <net/if.h>
#import <netinet/in.h>
#import <netinet/in_systm.h>
#import <netinet/ip.h>
#import <netinet/udp.h>
#import <netinet/bootp.h>
#import <netinet/if_ether.h>
#import <net/if_arp.h>
#import <mach/boolean.h>
#import <signal.h>
#import <stdio.h>
#import <string.h>
#import <errno.h>
#import <ctype.h>
#import <netdb.h>
#import <setjmp.h>
#import <syslog.h>
#import <varargs.h>
#import <arpa/inet.h>
#import <arpa/nameser.h>
#import <sys/uio.h>
#import <resolv.h>

#import "arp.h"
#import "netinfo.h"
#import "interfaces.h"
#import "inetroute.h"
#import "subnetDescr.h"
#import "dhcpOptions.h"
#import "dhcpOptionsPrivate.h"
#import "rfc_options.h"
#import "bootpd.h"
#import "hostlist.h"
#import "bootpdfile.h"
#import "macNC.h"
#import "NIHosts.h"
#import "host_identifier.h"
#import "dhcpd.h"
#import "Transmitter.h"

/* external functions */
extern int			useni();
extern char *  			ether_ntoa(struct ether_addr *e);
extern struct ether_addr *	ether_aton(char *);

/* local defines */
#define	MAXIDLE			(5*60)	/* we hang around for five minutes */
#define	IGNORETIME		(5*60)	/* ignore host with no binding */
#define DOMAIN_HIERARCHY	"..."	/* ... means open the hierarchy */

/* global variables: */
int		debug = 0;
id		ni_local = nil; /* local netinfo domain */
List *		niSearchDomains = nil;
int		quiet = 0;
unsigned char	rfc_magic[4] = RFC_OPTIONS_MAGIC;
id		subnets = nil;
boolean_t	use_en_address = TRUE;
int		verbose = 0;

/* local types */

/* local variables */
static boolean_t		S_bootfile_noexist_reply = TRUE;
static boolean_t		S_do_dhcp = FALSE;
static boolean_t		S_do_macNC = FALSE;
static id			S_domain_list = nil;
static struct in_addr *		S_dns_servers = NULL;
static int			S_dns_servers_count = 0;
static char *			S_domain_name = NULL;
static id			S_if_list = nil;
static struct hosts *		S_ignore_hosts = NULL;
static interface_list_t *	S_interfaces;
static inetroute_list_t *	S_inetroutes = NULL;
static u_short			S_ipport_client = IPPORT_BOOTPC;
static u_short			S_ipport_server = IPPORT_BOOTPS;
static struct timeval		S_lastmsgtime;
static u_char 			S_rxpkt[2048];/* receive packet buffer */
static boolean_t		S_sighup = TRUE; /* fake the 1st sighup */
#ifndef SYSTEM_ARP
static int			S_rtsockfd = -1;
#endif SYSTEM_ARP
static int			S_sockfd;
static id			S_transmitter = nil;
static boolean_t		S_use_file = FALSE;

/* forward function declarations */
static int 		issock(int fd);
static void		on_alarm(int sigraised);
static void		on_sighup(int sigraised);
static void		bootp_request(interface_t *, void * bp, int len,
				      struct timeval *);
static void		setarp(struct in_addr * ia, u_char * ha, int len);
static void		S_server_loop(struct in_addr * relay, int max_hops);

#define PID_FILE "/var/run/bootpd.pid"
static void
writepid(void)
{
    FILE *fp;
    
    fp = fopen(PID_FILE, "w");
    if (fp != NULL) {
	fprintf(fp, "%d\n", getpid());
	(void) fclose(fp);
    }
}

/*
 * Function: background
 *
 * Purpose:
 *   Daemon-ize ourselves.
 */
static void
background()
{
    if (fork())
	exit(0);
    { 
	int s;
	for (s = 0; s < 10; s++)
	    (void) close(s);
    }
    (void) open("/", O_RDONLY);
    (void) dup2(0, 1);
    (void) dup2(0, 2);
    {
	int tt = open("/dev/tty", O_RDWR);
	if (tt > 0) {
	    ioctl(tt, TIOCNOTTY, 0);
	    close(tt);
	}
    }
    writepid();
}

/*
 * Function: S_ni_in_list
 *
 * Purpose:
 *   Returns whether the given domain is in the list yet or not,
 *   using the host/tag pair as the key.
 */
static id
S_ni_in_list(id list, id domain)
{
    int i;
    for (i = 0; i < [list count]; i++) {
	id obj = [list objectAt:i];
	
	if (strcmp([obj tag], [domain tag]) == 0
	    && [obj ip].s_addr == [domain ip].s_addr)
	    return (obj);
    }
    return (nil);
}

/*
 * Function: S_ni_domains_init
 *
 * Purpose:
 *   Given the list of domain paths in S_domain_list,
 *   open a connection to it, and store the NIDomain object
 *   in the niSearchDomain list.
 *   The code makes sure it only opens each domain once by
 *   checking for uniqueness of the host/tag combination.
 *   The code also pays attention to the the special path "...",
 *   which means open the hierarchy starting from the local domain
 *   on up.
 */
static boolean_t
S_ni_domains_init()
{
    boolean_t	hierarchy_done = FALSE;
    int 	i;

    if (niSearchDomains != nil) {
	[[niSearchDomains freeObjects] free];
    }
    ni_local = nil;
    niSearchDomains = [[List alloc] initCount:[S_domain_list count]];
    for (i = 0; i < [S_domain_list count]; i++) {
	NIDomain * domain;
	u_char *   dstr = (u_char *)[S_domain_list objectAt:i];

	if (strcmp(dstr, DOMAIN_HIERARCHY) == 0) {
	    id domain;

	    if (hierarchy_done)
		continue;
	    hierarchy_done = TRUE;
	    if (verbose)
		syslog(LOG_INFO, "opening hierarchy starting at .");
	    domain = [[NIDomain alloc] initWithDomain:NI_DOMAIN_LOCAL 
				       Interfaces:S_interfaces];
	    while (TRUE) {
		id obj;

		if (domain == nil)
		    break; /* we're done */
		obj = S_ni_in_list(niSearchDomains, domain);
		if (obj != nil) {
		    if (debug)
			printf("%s/%s already in the list: %s\n",
			       [obj tag], inet_ntoa([obj ip]),
			       [obj domain_name]);
		    [domain free];
		    domain = obj;
		}
		else {
		    if (verbose)
			syslog(LOG_INFO, "opened domain %s/%s", 
			       inet_ntoa([domain ip]),
			       [domain tag]);
		    [niSearchDomains addObject:domain];
		}
		domain = [[NIDomain alloc] initParentDomain:domain
					   Interfaces:S_interfaces];
	    }
	}
	else {
	    if (verbose) {
		syslog(LOG_INFO, "opening domain %s", dstr);
	    }
	    domain = [[NIDomain alloc] initWithDomain:dstr 
				       Interfaces:S_interfaces];
	    if (domain != nil) {
		if (S_ni_in_list(niSearchDomains, domain) != nil) {
		    /* already in the list */
		    if (debug)
			printf("%s/%s already in the list\n",
			       [domain tag], inet_ntoa([domain ip]));
		    [domain free];
		    continue;
		}
		[niSearchDomains addObject:domain];
		if (verbose)
		    syslog(LOG_INFO, "opened domain %s/%s", 
			   inet_ntoa([domain ip]),
			   [domain tag]);
	    }
	    else {
		syslog(LOG_INFO, "unable to open domain '%s'", dstr);
	    }
	}
    }
    if ([niSearchDomains count] == 0) {
	[niSearchDomains free];
	niSearchDomains = nil;
	return (FALSE);
    }
    { /* find the "local" netinfo domain */
	id  domain;
	int i;

	for (i = 0; i < [niSearchDomains count]; i++) {
	    domain = [niSearchDomains objectAt:i];
	    if (if_lookupbyip(S_interfaces, [domain ip])
		&& strcmp([domain tag], "local") == 0) {
		ni_local = domain;
	    }
	}
	if (ni_local == nil && S_do_macNC) {
	    syslog(LOG_INFO, 
		   "macNC operation requires local netinfo domain, adding");
	    ni_local = [[NIDomain alloc] initWithDomain:NI_DOMAIN_LOCAL
					 Interfaces:S_interfaces];
	    if (ni_local == nil)
		exit(1);
	    [niSearchDomains insertObject:ni_local at:0];
	}
    }		
    return (TRUE);
}

static void
S_init_dns()
{
    int i;

    res_init(); /* figure out the default dns servers */

    S_dns_servers_count = _res.nscount;
    if (S_dns_servers_count == 1) {
      if (_res.nsaddr_list[0].sin_addr.s_addr == 0)
	S_dns_servers_count = 0;	/* if not set, DNS is 0.0.0.0 */
    }
    if (S_dns_servers_count) {
	S_dns_servers = (struct in_addr *)malloc(sizeof(*S_dns_servers) 
						 * S_dns_servers_count);
	if (_res.defdname[0]) {
	    S_domain_name = _res.defdname;
	    if (debug)
		printf("%s\n", S_domain_name);
	}
	for (i = 0; i < S_dns_servers_count; i++) {
	    S_dns_servers[i] = _res.nsaddr_list[i].sin_addr;
	    if (debug)
		printf("DNS %s\n", inet_ntoa(S_dns_servers[i]));
	}
    }
    return;
}

/*
 * Function: S_string_in_list
 *
 * Purpose:
 *   Given a List object, return boolean whether the C string is
 *   in the list.
 */
static boolean_t
S_string_in_list(id list, u_char * str)
{
    int i;
    for (i = 0; i < [list count]; i++) {
	u_char * lstr = (u_char *)[list objectAt:i];
	if (strcmp(str, lstr) == 0)
	    return (TRUE);
    }
    return (FALSE);
}

/*
 * Function: S_log_interfaces
 *
 * Purpose:
 *   Log which interfaces we will respond on.
 */
void
S_log_interfaces() 
{
    int i;
    int count = 0;
    
    for (i = 0; i < S_interfaces->count; i++) {
	interface_t * 	if_p = S_interfaces->list + i;
	
	if ((S_if_list == nil || S_string_in_list(S_if_list, if_p->name))
	    && if_p->inet_valid
	    && !(if_p->flags & IFF_LOOPBACK)) {
	    char 		addr[32];

	    strcpy(addr, inet_ntoa(if_p->addr));
	    syslog(LOG_INFO, "interface %s: %s ip address %s mask %s", 
		   if_p->name, if_p->hostname, addr, 
		   inet_ntoa(if_p->mask));
	    count++;
	}
    }
    if (count == 0) {
	syslog(LOG_INFO, "no available interfaces");
	exit(2);
    }
}

/*
 * Function: S_get_interfaces
 * 
 * Purpose:
 *   Get the list of interfaces we will use.
 */
void
S_get_interfaces()
{
    interface_list_t *	new_list;
    
    new_list = if_init();
    if (new_list == NULL) {
	syslog(LOG_INFO, "interface list initialization failed");
	exit(1);
    }
    if_free(&S_interfaces);
    S_interfaces = new_list;
    S_log_interfaces();
    return;
}

/*
 * Function: S_get_network_routes
 *
 * Purpose:
 *   Get the list of network routes.
 */
void
S_get_network_routes()
{
    inetroute_list_t * new_list;
    
    new_list = inetroute_list_init();
    if (new_list == NULL) {
	syslog(LOG_INFO, "can't get inetroutes list");
	exit(1);
    }
    
    inetroute_list_free(&S_inetroutes);
    S_inetroutes = new_list;
    if (debug)
	inetroute_list_print(S_inetroutes);
}


int
useni()
{
	static int useit = -1;
	extern int _lu_running();

	if (useit == -1){
		useit = _lu_running();
	}
	return (useit);
}

void
usage()
{
    fprintf(stderr, "useage: bootpd <options>\n"
	    "<options> are:\n"
	    "[ -i <interface> [ -i <interface> ... ] ]\n"
	    "[ -n <domain> [ -n <domain ... ] ]\n"
	    "[ -bdDmqv ]\n"
	    "[ -r <BOOTP/DHCP server ip> [ -o <max hops> ] ]\n"
	    );
    exit(1);
}

int
main(int argc, char * argv[])
{
    int			logopt = LOG_CONS;
    int			max_hops = 4;
    boolean_t		relay = FALSE;
    struct in_addr	relay_server = { 0 };
    int			ch;

    debug = 0;			/* no debugging ie. go into the background */
    verbose = 0;		/* don't print extra information */

    while ((ch =  getopt(argc, argv, "bc:DdeFhHi:mn:o:qr:s:v")) != EOF) {
	switch ((char)ch) {
	case 'b':
	    S_bootfile_noexist_reply = FALSE; 
	    /* reply only if bootfile exists */
	    break;
	case 'c':		/* specify the client ip port */
	    S_ipport_client = atoi(optarg);
	    break;
	case 'D':		/* answer DHCP requests as a DHCP server */
	    S_do_dhcp = TRUE;
	    break;
	case 'd':		/* stay in the foreground, extra printf's */
	    debug = 1;
	    break;
	case 'e':
	    /* don't use en_address to lookup/store ethernet hosts */
	    use_en_address = FALSE;
	    break;
	case 'F':
	    S_use_file = TRUE;
	    break;
	case 'h':
	case 'H':
	    usage();
	    exit(1);
	    break;
	case 'i':	/* user specified interface(s) to use */
	    if (S_if_list == nil)
		S_if_list = [[List alloc] init];
	    if (S_string_in_list(S_if_list, optarg) == FALSE) {
		[S_if_list addObject:(id)optarg];
	    }
	    else {
		syslog(LOG_INFO, "interface %s already specified",
		       optarg);
	    }
	    break;
	case 'm':	/* act as an NC Boot Server */
	    S_do_macNC = TRUE;
	    break;
	case 'n':	/* specify netinfo domain search hierarchy */
	    if (S_domain_list == nil)
		S_domain_list = [[List alloc] init];
	    if (S_string_in_list(S_domain_list, optarg) == FALSE) {
		[S_domain_list addObject:(id)optarg];
	    }
	    break;
	case 'o': {
	    int h;
	    h = atoi(optarg);
	    if (h > 16 || h < 1) {
		printf("max hops value %s must be in the range 1..16\n",
		       optarg);
		exit(1);
	    }
	    max_hops = h;
	    break;
	}
	case 'q':
	    quiet = 1;
	    break;
	case 'r':
	    relay = TRUE;
	    if (inet_aton(optarg, &relay_server) == 0
		|| relay_server.s_addr == 0 
		|| relay_server.s_addr == INADDR_BROADCAST) {
		printf("Invalid relay server ip address %s\n", optarg);
		exit(1);
	    }
	    break;
	case 's':	/* specify the server ip port */
	    S_ipport_server = atoi(optarg);
	    break;
	case 'v':	/* extra info to syslog */
	    verbose++;
	    break;
	default:
	    break;
	}
    }
    if (debug)
	quiet = 0;
    else if (quiet)
	verbose = 0;
    if (!issock(0)) { /* started by user */
	struct sockaddr_in Sin = { sizeof(Sin), AF_INET };
	int i;
	
	if (!debug)
	    background();
	
	if ((S_sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
	    syslog(LOG_INFO, "socket call failed");
	    exit(1);
	}
	Sin.sin_port = htons(S_ipport_server);
	Sin.sin_addr.s_addr = htonl(INADDR_ANY);
	i = 0;
	while (bind(S_sockfd, (struct sockaddr *)&Sin, sizeof(Sin)) < 0) {
	    syslog(LOG_INFO, "bind call failed: %s", strerror(errno));
	    if (errno != EADDRINUSE)
		exit(1);
	    i++;
	    if (i == 10) {
		syslog(LOG_INFO, "exiting");
		exit(1);
	    }
	    sleep(10);
	}
    } 
    else { /* started by inetd */
	S_sockfd = 0;
	signal(SIGALRM, on_alarm);
	gettimeofday(&S_lastmsgtime, 0);
	alarm(15);
    }

    if (debug)
	logopt = LOG_PERROR;

    if (relay)
	(void) openlog("bootp_relay", logopt | LOG_PID, LOG_DAEMON);
    else
	(void) openlog("bootpd", logopt | LOG_PID, LOG_DAEMON);
	
    syslog(LOG_DEBUG, "server starting");

    S_get_interfaces();
    S_get_network_routes();

    { 
	int opt = 1;

#if defined(IP_RECVIF)
	if (setsockopt(S_sockfd, IPPROTO_IP, IP_RECVIF, (caddr_t)&opt,
		       sizeof(opt)) < 0) {
	    syslog(LOG_INFO, "setsockopt(IP_RECVIF) failed: %s", 
		   strerror(errno));
	    exit(1);
	}
#elif defined (SO_RCVIF)
	/* indicate that we want receive interface information as well */
	if (setsockopt(S_sockfd, SOL_SOCKET, SO_RCVIF, (caddr_t)&opt,
		       sizeof(opt)) < 0) {
	    syslog(LOG_INFO, "setsockopt(SO_RCVIF) failed: %s", 
		   strerror(errno));
	    exit(1);
	}
#endif
	
	if (setsockopt(S_sockfd, SOL_SOCKET, SO_BROADCAST, (caddr_t)&opt,
		       sizeof(opt)) < 0) {
	    syslog(LOG_INFO, "setsockopt(SO_BROADCAST) failed");
	    exit(1);
	}
	if (setsockopt(S_sockfd, IPPROTO_IP, IP_RECVDSTADDR, (caddr_t)&opt,
		       sizeof(opt)) < 0) {
	    syslog(LOG_INFO, "setsockopt(IPPROTO_IP, IP_RECVDSTADDR) failed");
	    exit(1);
	}
	if (setsockopt(S_sockfd, SOL_SOCKET, SO_REUSEADDR, (caddr_t)&opt,
		       sizeof(opt)) < 0) {
	    syslog(LOG_INFO, "setsockopt(SO_REUSEADDR) failed");
	    exit(1);
	}
    }
    
    S_transmitter = [[[Transmitter alloc] init] setSocket:S_sockfd];
    if (S_transmitter == nil) {
	syslog(LOG_INFO, "bootpd: couldn't allocate Transmitter");
	exit(1);
    }

    /* install our sighup handler */
    signal(SIGHUP, on_sighup);

    if (relay)
	S_do_macNC = FALSE;
    else {
	if (useni() == FALSE)
	    S_use_file = TRUE;
	if (S_use_file) {
	    if (S_do_macNC) {
		syslog(LOG_INFO, "file-based NC operation not supported");
		S_do_macNC = FALSE;
	    }
	    if (S_do_dhcp) {
		syslog(LOG_INFO, "file-based DHCP operation not supported");
		S_do_dhcp = FALSE;
	    }
	}
	else {
	    /* initialize our netinfo search domains */
	    if (S_domain_list == nil) {
		S_domain_list = [[List alloc] init];
		[S_domain_list addObject:(id)DOMAIN_HIERARCHY];
	    }
	    if (S_ni_domains_init() == FALSE) {
		syslog(LOG_INFO, "domain initialization failed");
		exit (1);
	    }
	}

	S_init_dns();
    }
#ifndef SYSTEM_ARP
    S_rtsockfd = arp_get_routing_socket();
    if (S_rtsockfd < 0) {
	syslog(LOG_INFO, "couldn't get routing socket: %s",
	       strerror(errno));
	exit(1);
    }
#endif SYSTEM_ARP
	
    S_server_loop((relay == TRUE) ? (&relay_server) : (struct in_addr *)NULL, 
		  max_hops);
    exit (0);
}

/* 
 * Function: timestamp_syslog
 *
 * Purpose:
 *   Log a timestamped event message to the syslog.
 */
void
timestamp_syslog(char * msg)
{
    static struct timeval	tvp = {0,0};
    struct timeval		tv;

    if (verbose) {
	gettimeofday(&tv, 0);
	if (tvp.tv_sec) {
	    struct timeval result;

	    timeval_subtract(tv, tvp, &result);
	    syslog(LOG_INFO, "%d.%06d (%d.%06d): %s", 
		   tv.tv_sec, tv.tv_usec, result.tv_sec, result.tv_usec, msg);
	}
	else 
	    syslog(LOG_INFO, "%d.%06d (%d.%06d): %s", 
		   tv.tv_sec, tv.tv_usec, 0, 0, msg);
	tvp = tv;
    }
}

/*
 * Function: subnetAddressAndMask
 *
 * Purpose:
 *   Given the gateway address field from the request and the 
 *   interface the packet was received on, determine the subnet
 *   address and mask.
 * Note:
 *   This currently does not support "super-netting", in which
 *   more than one proper subnet shares the same physical subnet.
 */
boolean_t
subnetAddressAndMask(struct in_addr giaddr, interface_t * intface,
		     struct in_addr * addr, struct in_addr * mask)
{
    /* gateway specified, find a subnet description on the same subnet */
    if (giaddr.s_addr) {
	id subnet;
	/* find a subnet entry on the same subnet as the gateway */
	if (subnets == nil 
	    || (subnet = [subnets entrySameSubnet:giaddr]) == nil)
	    return (FALSE);
	*addr = giaddr;
	*mask = [subnet mask];
    }
    else {
	*addr = intface->netaddr;
	*mask = intface->mask;
    }
    return (TRUE);
}

/*
 * Function: issock
 *
 * Purpose:
 *   Determine if a descriptor belongs to a socket or not
 */
static int
issock(fd)
     int fd;
{
    struct stat st;
    
    if (fstat(fd, &st) < 0) {
	return (0);
    } 
    /*	
     * SunOS returns S_IFIFO for sockets, while 4.3 returns 0 and
     * does not even have an S_IFIFO mode.  Since there is confusion 
     * about what the mode is, we check for what it is not instead of 
     * what it is.
     */
    switch (st.st_mode & S_IFMT) {
      case S_IFCHR:
      case S_IFREG:
      case S_IFLNK:
      case S_IFDIR:
      case S_IFBLK:
	return (0);
      default:	
	return (1);
    }
}


/*
 * Function: on_sighup
 *
 * Purpose:
 *   If we get a sighup, re-read the subnet descriptions.
 */
static void
on_sighup(int sigraised)
{
    if (sigraised == SIGHUP)
	S_sighup = TRUE;
    return;
}

/*
 * Function: on_alarm
 *
 * Purpose:
 *   If we were started by inetd, we kill ourselves during periods of
 *   inactivity.  If we've been idle for MAXIDLE, exit.
 */
static void
on_alarm(int sigraised)
{
    struct timeval tv;
    
    gettimeofday(&tv, 0);
    
    if ((tv.tv_sec - S_lastmsgtime.tv_sec) >= MAXIDLE)
	exit(0);
    alarm(15);
    return;
}

/*
 * Function: bootp_add_bootfile
 *
 * Purpose:
 *   Verify that the specified bootfile exists, and add it to the given
 *   packet.  Handle <bootfile>.<hostname> to allow a specific host to
 *   get its own version of the bootfile.
 */
boolean_t
bootp_add_bootfile(char * request_file, char * hostname, char * homedir, 
		   char * bootfile, char * default_bootfile, 
		   char * tftp_homedir, char * reply_file)
{
    boolean_t 	dothost = FALSE;	/* file.host was found */
    char 	file[PATH_MAX];
    char 	path[PATH_MAX];

    strcpy(path, homedir);
    strcat(path, "/");
    if (request_file && request_file[0])
	strcpy(file, request_file);
    else if (bootfile && bootfile[0])
	strcpy(file, bootfile);
    else
	strcpy(file, default_bootfile);

    if (file[0] == '/')	/* if absolute pathname */
	strcpy(path, file);
    else
	strcat(path, file);

    /* try first to find the file with a ".host" suffix */
    if (hostname) {
	int 	n;

	n = strlen(path);
	strcat(path, ".");
	strcat(path, hostname);
	if (access(path, R_OK) >= 0)
	    dothost = TRUE;
	else
	    path[n] = 0;	/* try it without the suffix */
    }
    
    if (dothost == FALSE) {
	if (access(path, R_OK) < 0) {
	    if (S_bootfile_noexist_reply == FALSE) {
		syslog(LOG_INFO, 
		       "boot file %s* missing - not replying", path);
		return (FALSE);
	    }
	    if (verbose)
		syslog(LOG_INFO, "boot file %s* missing", path);
	}
    }

    if (tftp_homedir != NULL && tftp_homedir[0] != 0) {
	/* Rebuild the path with tftp_homedir at the front */
	strcpy(path, tftp_homedir);
	if (tftp_homedir[strlen(tftp_homedir) - 1] != '/') {
	    strcat(path, "/");
	}
	if (file[0] == '/') /* might not work in secure case */
	    strcpy(path, file);
	else
	    strcat(path, file);
	if (dothost) {
	    strcat(path, ".");
	    strcat(path, hostname);
	}
    }
    if (verbose)
	syslog(LOG_INFO,"replyfile %s", path);
    strcpy(reply_file, path);
    return (TRUE);
}

void
host_parms_from_proplist(ni_proplist * pl_p, int index, struct in_addr * ip, 
			 u_char * * name, u_char * * bootfile)
{
    ni_name ipstr;

    /* retrieve the ip address */
    if (ip) { /* return the ip address */
	ipstr = (ni_nlforprop(pl_p, NIPROP_IP_ADDRESS))->ninl_val[index];
	ip->s_addr = inet_addr(ipstr);
    }

    /* retrieve the host name */
    if (name) {
	ni_name 	str;
	
	*name = NULL;
	str = ni_valforprop(pl_p, NIPROP_NAME);
	if (str)
	    *name = ni_name_dup(str);
    }
    
    /* retrieve the bootfile */
    if (bootfile) {
	ni_name str;
	
	*bootfile = NULL;
	str = ni_valforprop(pl_p, NIPROP_BOOTFILE);
	if (str)
	    *bootfile = ni_name_dup(str);
    }
    return;
}

/*
 * Function: lookup_host_by_ip
 *
 * Purpose:
 *   Search netinfo for an entry that contains the given ip address.
 *   Retrieve the hostname, and bootfile if the corresponding
 *   argument pointer is not NULL.
 */
boolean_t
lookup_host_by_ip(struct in_addr ip, u_char * * name, u_char * * bootfile,
		  id * domain_p, ni_id * dir_p, ni_proplist * pl_p)
{
    char 	ipstr[32];
    ni_id	dir;
    id		domain;
    ni_proplist	pl;

    NI_INIT(&pl);
    strcpy(ipstr, inet_ntoa(ip));
    domain = [NIHosts lookupKey:NIPROP_IP_ADDRESS Value:ipstr
	      DomainList:niSearchDomains
	      PropList:pl_p Dir:&dir];
    if (domain == nil)
	return (FALSE);

    host_parms_from_proplist(pl_p, 0, NULL, name, bootfile);

    /* return the domain */
    if (domain_p != NULL)
	*domain_p = domain;
    /* return the directory */
    if (dir_p != NULL)
	*dir_p = dir;
    return (TRUE);
}

/*
 * Function: ip_address_reachable
 *
 * Purpose:
 *   Determine whether the given ip address is directly reachable from
 *   the given interface or gateway.
 *
 *   Directly reachable means without using a router ie. share the same wire.
 */
boolean_t
ip_address_reachable(struct in_addr ip, struct in_addr giaddr, 
		    interface_t * intface)
{
    int i;

    if (giaddr.s_addr) { /* gateway'd */
	/* find a subnet entry on the same subnet as the gateway */
	if (subnets == nil 
	    || [subnets ip:ip SameSupernet:giaddr] == FALSE)
	    return (FALSE);
	return (TRUE);
    }

    for (i = 0; i < S_inetroutes->count; i++) {
	inetroute_t * inr_p = S_inetroutes->list + i;

	if (inr_p->gateway.link.sdl_family == AF_LINK
	    && (if_lookupbylinkindex(S_interfaces, 
				     inr_p->gateway.link.sdl_index) 
		== intface)) {
	    /* reachable? */
	    if (in_subnet(inr_p->dest, inr_p->mask, ip))
		return (TRUE);
	}
    }
    return (FALSE);
}

/*
 * Function: prop_index_subnet
 *
 * Purpose:
 *   Given a property name and value, find the property value index 
 *   that has an IP_ADDRESS value reachable through the given
 *   interface or gateway.
 * Returns:
 *   -1 if not found, index if found.
 * Note:
 *   If a host entry has multiple hardware and ip addresses, the
 *   namelists for certain properties are treated as parallel arrays.
 *   This is to allow a host to have a single hardware address
 *   bound to more than one ip address, and to have multiple hardware
 *   addresses bound to multiple ip addresses.
 *   eg.
 *
 *   1. identifier = { "1/0:1:2:3:4:5", "1/0:1:2:3:4:5" };
 *      ip_address = { "17.202.40.191", "17.201.16.20" };
 *      - host has single interface, that will have ip address 17.202.40.191
 *        on subnet 17.202.40.0 and ip address 17.201.16 on subnet 17.202.16.0.
 *
 *   2. identifier = { "1/0:1:2:3:4:5", "1/0:5:5:5:5:5" };
 *      ip_address = { "17.202.40.191", "17.201.16.40" };
 *      - host has multiple interfaces:
 *        interface 0:1:2:3:4:5 has ip 17.202.40.191
 *        interface 0:5:5:5:5:5 has ip 17.201.16.40
 */

int
prop_index_subnet(ni_name prop, ni_name value, struct in_addr giaddr,
		  interface_t * intface, ni_proplist * proplist, 
		  boolean_t * error)
{
    struct in_addr 	ip;
    ni_namelist * 	ip_nl_p;
    int			p;

    p = ni_indexforprop(proplist, prop, value);
    ip_nl_p = ni_nlforprop(proplist, NIPROP_IP_ADDRESS);
    if (ip_nl_p == NULL) {
	*error = TRUE;
	if (verbose)
	    syslog(LOG_INFO, "bad host entry (%s=%s), missing %s",
		   prop, value, NIPROP_IP_ADDRESS);
	return -1;
    }
    if (p >= ip_nl_p->ninl_len) {
	if (ip_nl_p->ninl_len == 1)
	    p = 0;
	else {
	    *error = TRUE;
	    if (verbose)
		syslog(LOG_INFO, "bad host entry (%s=%s): too few %s values",
		       prop, value, NIPROP_IP_ADDRESS);
	    return (-1);
	}
    }
    ip.s_addr = inet_addr(ip_nl_p->ninl_val[p]);
    if (ip.s_addr != -1) {
	if (ip_address_reachable(ip, giaddr, intface))
	    return (p);
    }
    else if (verbose)
	syslog(LOG_INFO, "bad host entry (%s=%s): %s=%s",
	       prop, value, NIPROP_IP_ADDRESS, ip_nl_p->ninl_val[p]);
    return (-1);
}

typedef struct subnetMatchArgs {
    ni_name		key;
    ni_name		value;
    struct in_addr	giaddr;
    interface_t *	intface;
    int			index;
} subnetMatchArgs_t;

static boolean_t 
S_subnet_match(void * arg, ni_proplist * pl_p)
{
    int				index;
    boolean_t			error = FALSE;
    subnetMatchArgs_t * 	s = (subnetMatchArgs_t *)arg;

    index = prop_index_subnet(s->key, s->value, s->giaddr, 
			      s->intface, pl_p, &error);
    if (index == -1)
	return (FALSE);
    s->index = index;
    return (TRUE);
}

/*
 * Function: lookup_host
 *
 * Purpose:
 *   Retrieve the hardware address -> ip address binding by looking 
 *   up the given host's hardware address.  Searches for one that is
 *   on the given subnet.  If a hard address match is found, but
 *   it is on the wrong subnet, index will be -1.  Otherwise, index
 *   will be >= 0.
 */
id
lookup_host(u_char hwtype, void * hwaddr, int hwlen, 
	    struct in_addr giaddr, interface_t * intface,
	    ni_id * dir_p, ni_proplist * pl_p, int * index)
{
    int			key_matches = 0;
    int			matches = 0;
    ni_id		dir;
    id			domain = nil;
    subnetMatchArgs_t	s;

    NI_INIT(pl_p);
    s.giaddr = giaddr;
    s.intface = intface;
    s.index = -1;

    if (use_en_address
	&& hwtype == ARPHRD_ETHER) {
	/* lookup using en_address property */
	u_char * 	c = (u_char *)hwaddr;
	int		i;
	int		try_count = 1;

	/* try with leading zeroes only if it makes sense */
	if (c[0] < 0x10 || c[1] < 0x10 || c[2] < 0x10
	    || c[3] < 0x10 || c[4] < 0x10 || c[5] < 0x10)
	    try_count = 2;
	
	for (i = 0; i < try_count; i++) {
	    u_char		enstr[32];
#define EN_ADDRESS_CANONICAL	"%x:%x:%x:%x:%x:%x"
#define EN_ADDRESS_ZEROES 	"%02x:%02x:%02x:%02x:%02x:%02x"

	    s.key = NIPROP_EN_ADDRESS;
	    s.value = enstr;
	    if (i == 0)
		sprintf(enstr, EN_ADDRESS_CANONICAL,
			c[0], c[1], c[2], c[3], c[4], c[5]);
	    else
		sprintf(enstr, EN_ADDRESS_ZEROES,
			c[0], c[1], c[2], c[3], c[4], c[5]);
	    domain = [NIHosts lookupKey:s.key Value:s.value
			      Func:S_subnet_match Arg:(void *)&s
			      DomainList:niSearchDomains
			      KeyMatches:&key_matches
			      Matches:&matches
			      PropList:pl_p Dir:&dir];
	    if (domain != nil) {
		if (verbose)
		    syslog(LOG_INFO, "%s = %s: matches %d, exact matches %d\n",
			   s.key, s.value, key_matches, matches);
		break; /* out of for */
	    }
	}
    }
    else { /* lookup using identifier property */
	u_char *		idstr = NULL;

	idstr = identifierToString(hwtype, hwaddr, hwlen); /* malloc'd */
	if (idstr == NULL) {
	    syslog(LOG_ERR, "bootpd: identifierToString failed, %m\n");
	    return (nil);
	}
	s.key = NIPROP_IDENTIFIER;
	s.value = idstr;
	domain = [NIHosts lookupKey:s.key Value:s.value
			  Func:S_subnet_match Arg:(void *)&s
			  DomainList:niSearchDomains
			  KeyMatches:&key_matches
			  Matches:&matches
			  PropList:pl_p Dir:&dir];
	if (domain != nil && verbose) {
	    syslog(LOG_INFO, "%s = %s: matches %d, exact matches %d\n",
		   s.key, s.value, key_matches, matches);
	}
	if (idstr) {
	    free(idstr);
	    s.value = idstr = NULL;
	}
    }
    if (dir_p)
	*dir_p = dir;
    if (domain != nil && matches)
	*index = s.index;
    else
	*index = -1;
    return (domain);
}

boolean_t
lookup_host_by_identifier(u_char hwtype, void * hwaddr, int hwlen, 
			  struct in_addr giaddr, interface_t * intface, 
			  struct in_addr * ip, u_char * * name,
			  u_char * * bootfile, id * domain_p,
			  ni_id * dir_p, ni_proplist * pl_p)
{
    id 		domain;
    int	 	index = -1;

    domain = lookup_host(hwtype, hwaddr, hwlen, giaddr, intface, dir_p, pl_p,
			 &index);
    if (domain == nil)
	return (FALSE);
    if (index == -1) {
	ni_proplist_free(pl_p);
	return (FALSE);
    }
    if (domain_p)
      *domain_p = domain;
    host_parms_from_proplist(pl_p, index, ip, name, bootfile);
    return (TRUE);
}

void
print_packet(struct bootp *bp, int bp_len)
{
	int i, j, len;

	printf("bp_op = ");
	if (bp->bp_op == BOOTREQUEST) printf("BOOTREQUEST\n");
	else if (bp->bp_op == BOOTREPLY) printf("BOOTREPLY\n");
	else
	{
		i = bp->bp_op;
		printf("%d\n", i);
	}

	i = bp->bp_htype;
	printf("bp_htype = %d\n", i);

	printf("bp_flags = %x\n", bp->bp_unused);

	len = bp->bp_hlen;
	printf("bp_hlen = %d\n", len);

	i = bp->bp_hops;
	printf("bp_hops = %d\n", i);

	printf("bp_xid = %lu\n", (u_long)ntohl(bp->bp_xid));

	printf("bp_secs = %hu\n", bp->bp_secs);

	printf("bp_ciaddr = %s\n", inet_ntoa(bp->bp_ciaddr));
	printf("bp_yiaddr = %s\n", inet_ntoa(bp->bp_yiaddr));
	printf("bp_siaddr = %s\n", inet_ntoa(bp->bp_siaddr));
	printf("bp_giaddr = %s\n", inet_ntoa(bp->bp_giaddr));

	printf("bp_chaddr = ");
	for (j = 0; j < len; j++)
	{
		i = bp->bp_chaddr[j];
		printf("%0x", i);
		if (j < (len - 1)) printf(":");
	}
	printf("\n");

	printf("bp_sname = %s\n", bp->bp_sname);
	printf("bp_file = %s\n", bp->bp_file);
}

/*
 * Function: bootp_request
 *
 * Purpose:
 *   Process BOOTREQUEST packet.
 *
 * Note:
 *   This version of the bootp.c server never forwards 
 *   the request to another server.  In our environment the 
 *   stand-alone gateways perform that function.
 *
 *   Also this version does not interpret the hostname field of
 *   the request packet;  it COULD do a name->address lookup and
 *   forward the request there.
 */
static void
bootp_request(interface_t * intface, void * rxpkt, int rxpkt_len,
	      struct timeval * time_in_p)
{
    u_char *		bootfile = NULL;
    id			domain = nil;
    u_char *		hostname = NULL;
    struct in_addr	iaddr;
    boolean_t		netinfo_host = FALSE;
    static u_char	netinfo_options[] = {
	dhcptag_subnet_mask_e, 
	dhcptag_router_e, 
	dhcptag_host_name_e,
	dhcptag_netinfo_server_address_e,
	dhcptag_netinfo_server_tag_e,
    };
    static int		n_netinfo_options 
	= sizeof(netinfo_options) / sizeof(netinfo_options[0]);
    struct bootp 	rp;
    struct bootp *	rq = (struct bootp *)rxpkt;

    if (rxpkt_len < sizeof(struct bootp))
	return;

    rp = *rq;	/* copy request into reply */
    rp.bp_op = BOOTREPLY;

    if (rq->bp_ciaddr.s_addr == 0) { /* client doesn't specify ip */
	if (S_use_file) {
	    if (bootp_getbyhw_file(rq->bp_htype, rq->bp_chaddr, rq->bp_hlen,
				   &iaddr, &hostname, &bootfile) == FALSE)
		return;
	}
	else {
	    struct hosts * 	hp;
	    ni_proplist 	pl;
    
	    /* check for host entry in the ignore queue */
	    hp = hostbyaddr(S_ignore_hosts, rq->bp_htype, rq->bp_chaddr,
			    rq->bp_hlen);
	    if (hp) {
		if ((time_in_p->tv_sec - hp->tv.tv_sec) <= IGNORETIME)
		    return;
		hostfree(&S_ignore_hosts, hp);
		hp = NULL;
	    }
	    if (lookup_host_by_identifier(rq->bp_htype, rq->bp_chaddr, 
					  rq->bp_hlen, rq->bp_giaddr,
					  intface, &iaddr,
					  &hostname, &bootfile, &domain, NULL, 
					  &pl) == FALSE) {
		/* remember that we didn't reply before */
		struct hosts * hp;
		hp = hostadd(&S_ignore_hosts, time_in_p, rq->bp_htype, 
			     rq->bp_chaddr, rq->bp_hlen, NULL, NULL, NULL);
		if (verbose && hp) {
		    syslog(LOG_INFO, 
			   "ignoring the following host for %d seconds",
			   IGNORETIME);
		    hostprint(hp);
		}
		return;
	    }
	    if (ni_valforprop(&pl, NIPROP_SERVES))
		netinfo_host = TRUE;

	    ni_proplist_free(&pl);
	}
	rp.bp_yiaddr = iaddr;
    }
    else { /* client specified ip address */
	if (S_use_file) {
	    if (bootp_getbyip_file(rq->bp_ciaddr, &hostname, &bootfile) 
		== FALSE)
		return;
	}
	else {
	    ni_proplist pl;

	    NI_INIT(&pl);
	    iaddr = rq->bp_ciaddr;
	    if (lookup_host_by_ip(iaddr, &hostname, &bootfile, &domain,
				  NULL, &pl) == FALSE)
		return; /* unknown ip address */
	    ni_proplist_free(&pl);
	}
    }
    if (!quiet)
	syslog(LOG_INFO,"BOOTP request [%s]: %s requested file '%s'",
	       intface->name, hostname ? hostname : (u_char *)inet_ntoa(iaddr),
	       rq->bp_file);
    if (bootp_add_bootfile(rq->bp_file, hostname, boot_home_dir,
			   bootfile, boot_default_file, boot_tftp_dir,
			   rp.bp_file) == FALSE)
	/* client specified a bootfile but it did not exist */
	goto no_reply;
    
    if (bcmp(rq->bp_vend, rfc_magic, sizeof(rfc_magic)) == 0) {
	/* insert the usual set of options/extensions if possible */
	id options;
		
	options = [[dhcpOptions alloc] 
		   initWithBuffer:rp.bp_vend + sizeof(rfc_magic)
		   Size:sizeof(rp.bp_vend) - sizeof(rfc_magic)];
	if (options == nil)
	    syslog(LOG_INFO, "init options failed");
	else {
	    if (netinfo_host) {
		if (verbose)
		    syslog(LOG_INFO, "netinfo client");
		add_subnet_options(domain, hostname, iaddr, intface, options, 
				   netinfo_options, n_netinfo_options);
	    }
	    else
		add_subnet_options(domain, hostname, iaddr, intface, options, 
				   NULL, 0);
	    if (verbose) 
		syslog(LOG_INFO, "added vendor extensions");
	    if ([options addOption:dhcptag_end_e Length:0 
	         Data:NULL] == FALSE) {
		syslog(LOG_INFO, "couldn't add end tag: %s",
		       [options errString]);
	    }
	    else
		bcopy(rfc_magic, rp.bp_vend, sizeof(rfc_magic));
	    if (options != nil)
		[options free];
	}
    } /* if RFC magic number */
    else if (bcmp(rq->bp_vend, VM_NEXT, sizeof(VM_NEXT)) == 0) {
	struct nextvend *nv_rq = (struct nextvend *)&rq->bp_vend;
	struct nextvend *nv_rp = (struct nextvend *)&rp.bp_vend;
	if (nv_rq->nv_version == 1) {
	    nv_rp->nv_opcode = BPOP_OK;
	    nv_rp->nv_xid = 0;
	}
    }
    rp.bp_siaddr = intface->addr;
    strcpy(rp.bp_sname, intface->hostname);
    if (sendreply(intface, &rp, sizeof(rp), FALSE, NULL))
	if (!quiet)
	    syslog(LOG_INFO, "reply sent %s %s pktsize %d",
		   hostname, inet_ntoa(iaddr), sizeof(rp));

  no_reply:
    if (hostname)
	free(hostname);
    if (bootfile)
	free(bootfile);
    return;
}


/*
 * Send a reply packet to the client.  'forward' flag is set if we are
 * not the originator of this reply packet.
 */
boolean_t
sendreply(interface_t * intface, struct bootp * bp, int n, 
	  boolean_t broadcast, struct in_addr * dest_p)
{
    struct in_addr 		dst;
    u_short			dest_port = S_ipport_client;
    void *			hwaddr = NULL;
    u_short			src_port = S_ipport_server;

    /*
     * If the client IP address is specified, use that
     * else if gateway IP address is specified, use that
     * else make a temporary arp cache entry for the client's NEW 
     * IP/hardware address and use that.
     */
    if (bp->bp_ciaddr.s_addr) {
	dst = bp->bp_ciaddr;
	if (verbose) 
	    syslog(LOG_DEBUG, "reply ciaddr %s", inet_ntoa(dst));
    }
    else if (bp->bp_giaddr.s_addr) {
	dst = bp->bp_giaddr;
	dest_port = S_ipport_server;
	src_port = S_ipport_client;
	if (verbose) 
	    syslog(LOG_INFO, "reply giaddr %s", inet_ntoa(dst));
	if (broadcast) /* tell the gateway to broadcast */
	    bp->bp_unused = htons(ntohs(bp->bp_unused | DHCP_FLAGS_BROADCAST));
    } 
    else { /* local net request */
	if (broadcast || (ntohs(bp->bp_unused) & DHCP_FLAGS_BROADCAST)) {
	    if (verbose)
		syslog(LOG_INFO, "replying using broadcast IP address");
	    dst.s_addr = htonl(INADDR_BROADCAST);
	}
	else {
	    if (dest_p)
		dst = *dest_p;
	    else
		dst = bp->bp_yiaddr;
	    setarp(&dst, bp->bp_chaddr, bp->bp_hlen);
	    hwaddr = bp->bp_chaddr;
	}
	if (verbose) 
	    syslog(LOG_INFO, "replying to %s", inet_ntoa(dst));
    }
    if ([S_transmitter transmit:intface->name 
		       DestHWType:bp->bp_htype
		       DestHWAddr:hwaddr
		       DestHWLen:bp->bp_hlen
		       DestIP:dst SourceIP:intface->addr
		       DestPort:dest_port SourcePort:src_port
		       Data:bp Length:n] < 0) {
	syslog(LOG_INFO, "send failed, %m");
	return (FALSE);
    }
    return (TRUE);
}

/*
 * Function: get_dhcp_option
 *
 * Purpose:
 *   Get a dhcp option from subnet description.
 */
boolean_t
get_dhcp_option(id subnet, int tag, void * buf, int * len_p)
{
    unsigned char	err[256];
    unsigned char	propname[128];
    ni_namelist	*	nl_p;
    tag_info_t * 	tag_info = [dhcpOptions tagInfo:tag];

    if (dhcptag_subnet_mask_e == tag)
	strcpy(propname, NIPROP_NET_MASK);
    else
	sprintf(propname, "%s%s", DHCP_OPTION_PREFIX, tag_info->name);
    nl_p = [subnet lookup:propname];
    if (nl_p == NULL) {
	if (verbose)
	    syslog(LOG_INFO, "subnet entry %s is missing option %s",
		   [subnet name:err], propname);
	return (FALSE);
    }
    
    if ([dhcpOptions strList:(unsigned char * *)nl_p->ninl_val 
         Number:nl_p->ninl_len Tag:tag Buffer:buf Length:len_p
         ErrorString:err] == FALSE) {
	if (verbose)
	    syslog(LOG_INFO, "couldn't add option '%s': %s",
		   propname, err);
	return (FALSE);
    }
    return (TRUE);
}

/*
 * Function: add_subnet_options
 *
 * Purpose:
 *   Given a list of tags, retrieve them from the subnet entry and
 *   insert them into the message options.
 */
void
add_subnet_options(id domain, u_char * hostname, 
		   struct in_addr iaddr, interface_t * intface,
		   id options, u_char * tags, int n)
{
    char		buf[DHCP_OPTION_MAX];
    int			len;
    static u_char 	default_tags[] = { 
	dhcptag_subnet_mask_e, 
	dhcptag_router_e, 
	dhcptag_host_name_e,
	dhcptag_domain_name_server_e,
	dhcptag_domain_name_e,
    };
    boolean_t		netinfo_done = FALSE;
    static int		n_default_tags 
	= sizeof(default_tags) / sizeof(default_tags[0]);
    int			i;
    id			subnet = [subnets entry:iaddr];

    if (tags == NULL) {
	tags = default_tags;
	n = n_default_tags;
    }
			
    for (i = 0; i < n; i++ ) {
	len = [options freeSpace];
	if (len > sizeof(buf))
	    len = sizeof(buf);
	if (tags[i] == dhcptag_host_name_e) {
	    if (hostname) {
		if ([options addOption:dhcptag_host_name_e 
			     FromString:hostname] == FALSE) {
		    syslog(LOG_INFO, "couldn't add hostname: %s",
			   [options errString]);
		}
	    }
	}
	else if (subnet != nil 
	    && get_dhcp_option(subnet, tags[i], buf, &len)) {
	    if ([options addOption:tags[i] Length:len Data:buf] == FALSE) {
		if (!quiet)
		    syslog(LOG_INFO, "couldn't add option %d: %s",
			   tags[i], [options errString]);
	    }
	}
	else { /* try to use defaults if no explicit configuration */
	    struct in_addr * def_route;

	    switch (tags[i]) {
	      case dhcptag_netinfo_server_tag_e:
	      case dhcptag_netinfo_server_address_e: {
		  struct in_addr 	ip;
		  ni_name 		tag;

		  if (netinfo_done)
		      continue;

		  netinfo_done = TRUE;
		  if (domain == nil)
		      continue;
		  tag = [domain tag];
#define LOCAL_NETINFO_TAG	"local"
		  if (strcmp(tag, LOCAL_NETINFO_TAG) == 0)
		      continue; /* can't bind to a server's local domain */

		  ip = [domain ip];
		  if ([options addOption:dhcptag_netinfo_server_address_e
			       Length:sizeof(ip)
			       Data:&ip] == FALSE) {
		      if (!quiet)
			  syslog(LOG_INFO, 
				 "couldn't add netinfo server address: %s",
				 [options errString]);
		      continue;
		  }
		  if ([options addOption:dhcptag_netinfo_server_tag_e
			       FromString:tag] == FALSE) {
		      if (!quiet)
			  syslog(LOG_INFO, 
				 "couldn't add netinfo server tag: %s",
				 [options errString]);
		      continue;
		  }
		  break;
	      }
	      case dhcptag_subnet_mask_e:
		if (if_lookupbysubnet(S_interfaces, iaddr) != intface)
		    continue;
		if ([options addOption:dhcptag_subnet_mask_e 
		     Length:sizeof(intface->mask) 
		     Data:&intface->mask] == FALSE) {
		    if (!quiet)
			syslog(LOG_INFO, "couldn't add subnet_mask: %s",
			       [options errString]);
		    continue;
		}
		if (verbose)
		    syslog(LOG_INFO, 
			   "subnet mask %s derived from %s",
			   inet_ntoa(intface->mask), intface->name);
		break;
	      case dhcptag_router_e:
		def_route = inetroute_default(S_inetroutes);
		if (def_route == NULL
		    || in_subnet(intface->netaddr, intface->mask,
				   *def_route) == FALSE
		    || in_subnet(intface->netaddr, intface->mask,
				 iaddr) == FALSE)
		    /* don't respond if default route not on same subnet */
		    continue;
		if ([options addOption:dhcptag_router_e
		     Length:sizeof(*def_route) Data:def_route] == FALSE) {
		    if (!quiet)
			syslog(LOG_INFO, "couldn't add router: %s",
			       [options errString]);
		    continue;
		}
		if (verbose)
		    syslog(LOG_INFO, "default route added as router");
		break;
	      case dhcptag_domain_name_server_e:
		if (S_dns_servers_count == 0)
		    continue;
		if ([options addOption:dhcptag_domain_name_server_e
		     Length:S_dns_servers_count * sizeof(*S_dns_servers)
		     Data:S_dns_servers] == FALSE) {
		    if (!quiet)
			syslog(LOG_INFO, "couldn't add dns servers: %s",
			       [options errString]);
		    continue;
		}
		if (verbose)
		    syslog(LOG_INFO, "default dns servers added");
		break;
	      case dhcptag_domain_name_e:
		if (S_domain_name) {
		    if ([options addOption:dhcptag_domain_name_e 
		         FromString:S_domain_name] == FALSE) {
			if (!quiet)
			    syslog(LOG_INFO, "couldn't add domain name: %s",
				   [options errString]);
			continue;
		    }
		    if (verbose)
			syslog(LOG_INFO, "default domain name added");
		}
		break;
	      default:
		break;
	    }
	}
    }
    return;
}


#ifndef SYSTEM_ARP
/*
 * Function: setarp
 *
 * Purpose:
 *   Temporarily bind IP address 'ia'  to hardware address 'ha' of 
 *   length 'len'.  Uses the arp_set/arp_delete routines.
 */
void
setarp(struct in_addr * ia, u_char * ha, int len)
{
    int arp_ret;
    route_msg msg;

    if (S_rtsockfd == -1) {
	syslog(LOG_ERR, "setarp: routing socket not initialized");
	exit(1);
    }
    arp_ret = arp_get(S_rtsockfd, &msg, ia);
    if (arp_ret == 0) {
   	struct sockaddr_inarp *	sin;
    	struct sockaddr_dl *	sdl;

	sin = (struct sockaddr_inarp *)msg.m_space;
	sdl = (struct sockaddr_dl *)(sin->sin_len + (char *)sin);
	if (sdl->sdl_alen == len
	    && bcmp(ha, sdl->sdl_data + sdl->sdl_nlen, len) == 0) {
	    if (msg.m_rtm.rtm_rmx.rmx_expire == 0) {
		if (debug)
		    printf("permanent entry for %s - leaving it alone\n",
			   inet_ntoa(*ia));
		return;
	    }
	}
    }
    else if (verbose)
	syslog(LOG_INFO, "arp_get(%s) failed, %d", inet_ntoa(*ia), arp_ret);
    arp_ret = arp_delete(S_rtsockfd, ia, FALSE);
    if (arp_ret != 0 && verbose)
	syslog(LOG_INFO, "arp_delete(%s) failed, %d", inet_ntoa(*ia), arp_ret);
    arp_ret = arp_set(S_rtsockfd, ia, (void *)ha, len, TRUE, FALSE);
    if (verbose) {
	if (arp_ret == 0)
	    syslog(LOG_INFO, "arp_set(%s, %s) succeeded", inet_ntoa(*ia), 
		   ether_ntoa((struct ether_addr *)ha));
	else
	    syslog(LOG_INFO, "arp_set(%s, %s) failed: %s", inet_ntoa(*ia), 
		   ether_ntoa((struct ether_addr *)ha),
		   arp_strerror(arp_ret));
    }
    return;
}
#else SYSTEM_ARP
/* 
 * SYSTEM_ARP: use system("arp") to set the arp entry
 */
/*
 * Setup the arp cache so that IP address 'ia' will be temporarily
 * bound to hardware address 'ha' of length 'len'.
 */
static void
setarp(struct in_addr * ia, u_char * ha, int len)
{
    char buf[256];
    int status;
    
    sprintf(buf, "/usr/sbin/arp -d %s", inet_ntoa(*ia));
    if (verbose) 
	syslog(LOG_INFO, buf);
    status = system(buf);
    if (status && verbose)
	syslog(LOG_INFO, "arp -d failed, exit code=0x%x", status);
    sprintf(buf, "/usr/sbin/arp -s %s %s temp",
	    inet_ntoa(*ia), ether_ntoa((struct ether_addr *)ha));;
    if (verbose) syslog(LOG_INFO, buf);
    status = system(buf);
    if (status && verbose)
	syslog(LOG_INFO, "arp failed, exit code=0x%x", status);
    return;
}
#endif SYSTEM_ARP

/**
 ** Server Main Loop
 **/
static char 		control[1024];
static struct iovec  	iov;
static struct msghdr 	msg;

static void
S_init_msg()
{
    msg.msg_name = 0;
    msg.msg_namelen = 0;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = control;
    msg.msg_controllen = sizeof(control);
    msg.msg_flags = 0;
    iov.iov_base = (caddr_t)S_rxpkt;
    iov.iov_len = sizeof(S_rxpkt);
    return;
}

static void
S_relay_packet(struct in_addr * relay, int max_hops, struct bootp * bp, int n, 
	       interface_t * ifa)
{
    if (n < sizeof(struct bootp))
	return;

    switch (bp->bp_op) {
    case BOOTREQUEST:
	if (bp->bp_hops >= max_hops)
	    return;
	if (bp->bp_giaddr.s_addr == 0) {
	    /* fill it in with our interface address */
	    bp->bp_giaddr = ifa->addr;
	}
	bp->bp_hops++;
	if (relay->s_addr == if_broadcast(ifa).s_addr)
	    return; /* don't rebroadcast */
	if ([S_transmitter transmit:ifa->name 
			   DestHWType:bp->bp_htype
			   DestHWAddr:NULL
			   DestHWLen:0
			   DestIP:*relay SourceIP:ifa->addr
			   DestPort:S_ipport_server SourcePort:S_ipport_client
			   Data:bp Length:n] < 0) {
	    syslog(LOG_INFO, "send failed, %m");
	    return;
	}
	break;
    case BOOTREPLY: {
	interface_t * 	if_p;
	struct in_addr	dst;

	if (bp->bp_giaddr.s_addr == 0)
	    return;
	if_p = if_lookupbyip(S_interfaces, bp->bp_giaddr);
	if (if_p == NULL) { /* we aren't the gateway - discard */
	    return;
	}
	
	if ((ntohs(bp->bp_unused) & DHCP_FLAGS_BROADCAST)) {
	    if (verbose)
		syslog(LOG_INFO, "replying using broadcast IP address");
	    dst.s_addr = htonl(INADDR_BROADCAST);
	}
	else {
	    dst = bp->bp_yiaddr;
	    if (dst.s_addr != 0)
		setarp(&dst, bp->bp_chaddr, bp->bp_hlen);
	}
	if (verbose) 
	    syslog(LOG_INFO, "relaying from server '%s' to %s", 
		   bp->bp_sname, inet_ntoa(dst));

	if ([S_transmitter transmit:if_p->name
			   DestHWType:bp->bp_htype
			   DestHWAddr:bp->bp_chaddr
			   DestHWLen:bp->bp_hlen
			   DestIP:dst SourceIP:if_p->addr
			   DestPort:S_ipport_client SourcePort:S_ipport_server
			   Data:bp Length:n] < 0) {
	    syslog(LOG_INFO, "send failed, %m");
	    return;
	}
	break;
    }

    default:
	break;
    }
    if (verbose) {
	struct timeval now;
	struct timeval result;

	gettimeofday(&now, 0);
	timeval_subtract(now, S_lastmsgtime, &result);
	if (!quiet)
	    syslog(LOG_INFO, "relay time %d.%06d seconds",
		   result.tv_sec, result.tv_usec);
    }
    return;
}

static void
S_dispatch_packet(struct bootp * bp, int n, interface_t * ifa,
		  struct in_addr * dstaddr_p)
{
    id 			options = nil;
    boolean_t		dhcp_pkt = FALSE;
    dhcp_msgtype_t	dhcp_msgtype = dhcp_msgtype_none_e;
    
    /* get the packet options, check for dhcp */
    if (bcmp(bp->bp_vend, rfc_magic, sizeof(rfc_magic)) == 0) {
	options = [[dhcpOptions alloc] 
		   initWithBuffer:bp->bp_vend + sizeof(rfc_magic)
		   Size:n - sizeof(struct dhcp) - sizeof(rfc_magic)];
	if ([options parse]) {
	    dhcp_pkt = is_dhcp_packet(options, &dhcp_msgtype);
	}
	else {
	    [options free];
	    options = nil;
	}
	    
    }
    switch (bp->bp_op) {
      case BOOTREQUEST:
	if (bp->bp_sname[0] != '\0' 
	    && strcmp(bp->bp_sname, ifa->hostname) != 0)
	    break;
	if (bp->bp_siaddr.s_addr != 0
	    && ntohl(bp->bp_siaddr.s_addr) != ntohl(ifa->addr.s_addr))
	    break;
	if (dhcp_pkt && S_do_dhcp) { /* this is a DHCP packet */
	    dhcp_request(dhcp_msgtype, ifa, S_rxpkt, n, options, 
			 dstaddr_p, &S_lastmsgtime);
	}
#ifndef MOSX
	else if (S_do_macNC && options != nil
		 && NC_request(ifa, S_rxpkt, n, options, &S_lastmsgtime)) {
	    break;
	}
#endif MOSX
	else
	    bootp_request(ifa, S_rxpkt, n, &S_lastmsgtime);
	break;

      case BOOTREPLY:
	/* we're not a relay, sorry */
	break;

      default:
	break;
    }
    if (options != nil)
	[options free];
    if (verbose) {
	struct timeval now;
	struct timeval result;

	gettimeofday(&now, 0);
	timeval_subtract(now, S_lastmsgtime, &result);
	if (!quiet)
	    syslog(LOG_INFO, "service time %d.%06d seconds",
		   result.tv_sec, result.tv_usec);
    }
    return;
}

static __inline__ void *
S_parse_control(int level, int type, int * len)
{
    struct cmsghdr *	cmsg;

    *len = 0;
    for (cmsg = CMSG_FIRSTHDR(&msg); cmsg; cmsg = CMSG_NXTHDR(&msg, cmsg)) {
	if (cmsg->cmsg_level == level 
	    && cmsg->cmsg_type == type) {
	    if (cmsg->cmsg_len < sizeof(*cmsg))
		return (NULL);
	    *len = cmsg->cmsg_len - sizeof(*cmsg);
	    return (CMSG_DATA(cmsg));
	}
    }
    return (NULL);
}

#if defined(IP_RECVIF)
static __inline__ interface_t *
S_which_interface()
{
    struct sockaddr_dl *dl_p;
    char		ifname[IFNAMSIZ + 1];
    interface_t *	if_p = NULL;
    int 		len = 0;

    dl_p = (struct sockaddr_dl *)S_parse_control(IPPROTO_IP, IP_RECVIF, &len);
    if (dl_p == NULL || len == 0 || dl_p->sdl_nlen >= sizeof(ifname)) {
	return (NULL);
    }
    
    bcopy(dl_p->sdl_data, ifname, dl_p->sdl_nlen);
    ifname[dl_p->sdl_nlen] = '\0';
    if_p = if_lookupbyname(S_interfaces, ifname);
    if (if_p == NULL) {
	if (verbose)
	    syslog(LOG_INFO, "unknown interface %s\n", ifname);
	return (NULL);
    }
    if (if_p->inet_valid == FALSE)
	return (NULL);
    if (if_p->hostname[0] == '\0') {
	if (verbose)
	    syslog(LOG_INFO, 
		   "ignoring request on %s - hostname not defined", 
		   ifname);
	return (NULL);
    }
    if (S_if_list != nil 
	&& S_string_in_list(S_if_list, ifname) == FALSE) {
	if (verbose)
	    syslog(LOG_INFO, "ignoring request on %s", ifname);
	return (NULL);
    }
    return (if_p);
}

#elif defined(SO_RCVIF)
static __inline__ interface_t *
S_which_interface()
{
    void *		data;
    interface_t *	if_p = NULL;
    char 		ifname[32]; 
    int 		len = 0;

    data = S_parse_control(SOL_SOCKET, SO_RCVIF, &len);
    if (data == NULL || len == 0 || len >= sizeof(ifname))
	return (NULL);
    bcopy(data, ifname, len);
    ifname[len] = '\0';
    if_p = if_lookupbyname(S_interfaces, ifname);
    if (if_p == NULL) {
	if (verbose)
	    syslog(LOG_INFO, "unknown interface %s\n", ifname);
	return (NULL);
    }
    if (if_p->inet_valid == FALSE)
	return (NULL);
    if (if_p->hostname[0] == '\0') {
	if (verbose)
	    syslog(LOG_INFO, 
		   "ignoring request on %s - hostname not defined", 
		   ifname);
	return (NULL);
    }
    if (S_if_list != nil 
	&& S_string_in_list(S_if_list, ifname) == FALSE) {
	if (verbose)
	    syslog(LOG_INFO, "ignoring request on %s", ifname);
	return (NULL);
    }
    return (if_p);
}
#endif

static __inline__ struct in_addr *
S_which_dstaddr()
{
    void *	data;
    int		len = 0;
    
    data = S_parse_control(IPPROTO_IP, IP_RECVDSTADDR, &len);
    if (data && len == sizeof(struct in_addr))
	return ((struct in_addr *)data);
    return (NULL);
}

/*
 * Function: S_server_loop
 *
 * Purpose:
 *   This is the main "wait for packets and reply" loop.
 */
static void
S_server_loop(struct in_addr * relay, int max_hops)
{
    struct in_addr * 	dstaddr_p = NULL;
    struct sockaddr_in 	from = { sizeof(from), AF_INET };
    interface_t *	ifa;
    int 		mask;
    int			n;

    for (;;) {
	S_init_msg();
	msg.msg_name = (caddr_t)&from;
	msg.msg_namelen = sizeof(from);
	n = recvmsg(S_sockfd, &msg, 0);
	if (n < 0) {
	    if (verbose)
		syslog(LOG_DEBUG, "recvmsg failed, %m");
	    errno = 0;
	    continue;
	}

	if (relay == NULL)
	    bootp_readtab();	/* (re)read the bootptab */

	if (S_sighup) {
	    static boolean_t first = TRUE;

	    hostlistfree(&S_ignore_hosts); /* clean-up ignore queue */
	    if (first == FALSE) {
		S_get_interfaces();
		S_get_network_routes();
	    }
	    first = FALSE;
	    { /* get the new subnet descriptions */
		u_char		err[256];
		subnetListNI *	new_subnets;

		err[0] = '\0';
		new_subnets = [[subnetListNI alloc] 
				  initFromDomains:niSearchDomains Err:err];
		if (new_subnets != nil) {
		    [subnets free];
		    subnets = new_subnets;
		    if (debug)
			[subnets print];
		}
		else {
		    syslog(LOG_INFO, "subnets init failed: %s", err);
		}
	    }
#if !defined(MOSX)
	    /* re-read the NC configuration */
	    if (S_do_macNC) {
		if (NC_cfg_init() == FALSE)
		    S_do_macNC = FALSE; /* shut off NC operation */
	    }
#endif
            S_sighup = FALSE;
	}

	dstaddr_p = S_which_dstaddr();
	if (debug) {
	    if (dstaddr_p == NULL)
		printf("no destination address\n");
	    else
		printf("destination address %s\n", inet_ntoa(*dstaddr_p));
	}

#if defined(IP_RECVIF) || defined(SO_RCVIF)
	ifa = S_which_interface();
	if (ifa == NULL)
	  continue;
#else 
	ifa = if_first_broadcast_inet(S_interfaces);
#endif

	gettimeofday(&S_lastmsgtime, 0);

        mask = sigblock(sigmask(SIGALRM));
	if (relay)
	    S_relay_packet(relay, max_hops, (struct bootp *)S_rxpkt, n, ifa);
	else
	    S_dispatch_packet((struct bootp *)S_rxpkt, n, ifa, dstaddr_p);
	sigsetmask(mask);
    }
    exit (0); /* not reached */
}
