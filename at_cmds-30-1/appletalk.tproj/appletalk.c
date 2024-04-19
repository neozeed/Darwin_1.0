/*
 *	Copyright (c) 1988-91 Apple Computer, Inc. 
 *
 *	The information contained herein is subject to change without
 *	notice and  should not be  construed as a commitment by Apple
 *	Computer, Inc. Apple Computer, Inc. assumes no responsibility
 *	for any errors that may appear.
 *
 *	Confidential and Proprietary to Apple Computer, Inc.
 */

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/signal.h>

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>
#include <mach/boolean.h>
#include <net/if.h>

#include <netat/appletalk.h>
#include <netat/ddp.h>
#include <netat/nbp.h>
#include <netat/zip.h>
#include <netat/at_var.h>
#include <netat/atp.h>
#include <netat/at_config.h>  
#include <netat/routing_tables.h>

#include <AppleTalk/at_proto.h>
#include <AppleTalk/at_paths.h>

#include <varargs.h>

/* *** *********************************************************** *** */

#define MAX_ZONES		50	/* max cfg file zone count */

/* version checking */
#define MIN_MAJOR       0                   /* minimum required  version for */
#define MIN_MINOR       8                   /* advanced features */

#define   ATOL(str)       strtol(str, (char **) NULL, 0)

static char *usage = 
"  Startup in single port mode:\n\
	-u <interface, e.g. en0>  Bring up in single-interface mode.\n\
    modifier:\n\
	-q don't ask for zones (non-interactive mode)\n\
  Startup for multiple ports:\n\
	-r bring up Appletalk in routing mode\n\
	-x bring up Appletalk in multihoming mode\n\
    modifiers:\n\
	-f <router config file>\n\
	-c check config file only\n\
	-e check & display configuration only\n\
	-q don't ask for zones (non-interactive mode)\n\
	-v <value> maximum routing table entries\n\
	-w <value> maximum zip table entries\n\
  Other commands:\n\
	-a check state\n\
	-d shut down AppleTalk\n\
	-h check/modify default zone\n\
	-n print network number and node id\n\
	-p print saved PRAM AppleTalk information\n\
	-s show statistics & error counts\n\
  Other routing commands:\n\
	-j print router status\n\
	-m <value> maximum routing (pack/sec)\n\
	-t show routing table\n\
	-z show zone list\n\
" ;

extern char	*optarg;

static char	*et_interface = AT_DEF_ET_INTERFACE;
static char	*progname = NULL;
static int	router = 0;
static at_if_cfg_t elapcfg[IF_TOTAL_MAX];		
static if_zone_info_t if_zones[MAX_ZONES]; 	/* zone info from cfg file */
static at_if_cfg_t cfg;		/* configuration to use for printing status */

static at_router_params_t rt_param = {0, 0, 0, 0};
static char  *cfgFileName;      /* optional alternate router.cfg file */
static short checkCfg;       /* if true, just check config file */
static short displayCfg;       /* display configuration only */

/* XXX extern char *lap_default (); */

int	getConfigInfo(at_if_cfg_t elapcfgp[], if_zone_info_t zonep[],
		      char  *cfgFileName, short checkCfg, short displayCfg,
		      short mh), 
  	add_local_zones(if_zone_info_t if_zones[]),
	set_local_zones(),
  	showRoutes(),
  	showZones(),
	atalkState(char, char **),
	print_pram_info(char *);

static int do_init();
static void displayZoneDef(int);
static void	print_nodeid(void);
static void	print_statistics();
static void	do_shutdown();
static void	down_error(int *);
static int	register_this_node(int), showRouterStats();

static FILE *STDOUT = stdout;

#define IFR_NEXT(ifr)   \
  ((struct ifreq *) ((char *) (ifr) + sizeof(*(ifr)) + \
      MAX(0, (int) (ifr)->ifr_addr.sa_len - (int) sizeof((ifr)->ifr_addr))))
		
int main(argc, argv)
     int	argc;
     char	*argv[];
{
	int *pid_buf;
	int opterr;
	int ch;
	int fd;
	at_state_t global_state;

	int	no_option = 1,
	 	d_option = 0,
	   	r_option = 0,
		j_option = 0,
		h_option = 0,	/* check/modify future default zone */
		v_option = 0,
	  	w_option = 0,
	  	z_option = 0,
		t_option = 0,
	  	m_option = 0,
		u_option = 0,
		n_option = 0,
		s_option = 0,
		p_option = 0,
		q_option = 0,	/* don't ask for zones */
		a_option = 0;

	int router_mix = 0;
	int	retval = 0;		/* return value (for -t opt) */

	checkCfg = 0;
	displayCfg = 0;
	cfgFileName = NULL;
	q_option = FALSE;		/* default, ask for zones on startup */

	/* find out what this program is called and other miscellaneous stuff */
	progname = argv[0];

	/* check whether at least one argument was provided */
	opterr = argc < 2;

	/* process the arguments */
	while (!opterr && (ch = getopt(argc, argv, "acdef:hjm:npqrstu:v:w:xz")) != EOF) {
		no_option = 0;
		switch (ch) {

			/* option '-h': check/modify future default zone */
		case 'h':
		  	h_option++;
			break;

			/* option '-j': print router stats */
		case 'j':
			j_option++; 
			break;

			/* option '-n': print node and network */
		case 'n':
			n_option++; 
			break;

			/* option '-u': start the network up */
		case 'u':
			u_option++; 
			et_interface = optarg;
			break;

			/* option '-q': run quiet, don't ask for zones */
		case 'q':
			q_option = TRUE;
			break;

			/* option '-d': stop AppleTalk stack */
		case 'd':
			d_option++; 
			break;

			/* option '-s': print statistics and error counts */
		case 's':
			s_option++; 
			break;
		case 'p':
			p_option++;
			break;

		case 'a':
			a_option++;
			break;

		/* router command only */
			/* option '-r': start the network up - router mode*/
		case 'r':
			r_option++; 
			break;

			/* option '-x': start the network up - multihoming*/
		case 'x':
			rt_param.multihome = 1;
			break;

			/* option '-e': display config only */
		case 'e':
			displayCfg = TRUE;
			break;

			/* option '-f': specify router config file */ 
		case 'f':
			cfgFileName = optarg;
			break;

			/* option '-c': test config file only */
		case 'c':
			checkCfg = TRUE;
			break;

			/* option '-m': mix of routing vs home stack selection*/
		case 'm': 
			 m_option++;
			 rt_param.router_mix = ATOL(optarg) & 0xFFFF;
			 router_mix = (int)(ATOL(optarg) & 0xFFFF);
			 break;

		case 'v': 
			 v_option++;
			 rt_param.rtmp_table_sz = ATOL(optarg) & 0xFFFF;
			 if (!rt_param.rtmp_table_sz) 
				rt_param.rtmp_table_sz = RT_DEFAULT;
			 else if (rt_param.rtmp_table_sz < RT_MIN)
			 	rt_param.rtmp_table_sz = RT_MIN;
			 else if (rt_param.rtmp_table_sz > RT_MAX)
				rt_param.rtmp_table_sz = RT_MAX;
			 else
				/* it's correct */
				break;
			 fprintf(stderr, 
			 "%s: Routing table size should be between %d and %d; using %d\n", 
				 progname, RT_MIN, RT_MAX, 
				 rt_param.rtmp_table_sz);
			 break;

			/* option '-w': zip table size*/
		case 'w': 
			 w_option++;
			 rt_param.zone_table_sz = ATOL(optarg) & 0xFFFF;
			 if (!rt_param.zone_table_sz)
			 	rt_param.zone_table_sz = ZT_DEFAULT;
			 else if (rt_param.zone_table_sz < ZT_MIN)
				rt_param.zone_table_sz = ZT_MIN;
			 else if (rt_param.zone_table_sz > ZT_MAX)
				rt_param.zone_table_sz = ZT_MAX;
			 else
				/* it's correct */
				break;
			 fprintf(stderr, 
			 "%s: Zone table size should be between %d and %d; using %d\n", 
				 progname, ZT_MIN, ZT_MAX, 
				 rt_param.zone_table_sz);
			 break;

			 /* option '-t' display routing table */
		case 't': 
			 t_option++;
			 break;

			 /* option '-z' display zone table */
		case 'z': 
			 z_option++;
			 break;

		default:
			opterr++; 
			break;
		}
	}/* of while */

	if (a_option) {
		atalkState(0, (char **)&pid_buf);
		if (pid_buf) {
			fprintf(STDOUT, 
				"The following processes are using AppleTalk services:\n");
			while (*pid_buf != 0) {
				fprintf(STDOUT, "  %d\n", *pid_buf);
					pid_buf++;
			}
			exit(EBUSY);
		}
		else
		  fprintf(STDOUT, "No processes are currently using AppleTalk services.\n");
		exit(0);
	}

	if (u_option || d_option || r_option || rt_param.multihome || h_option || m_option) {
	  /* single-port startup, stop, router startup, multihome startup, and
	     change default zone require root access */
	  if (getuid () != 0) {
	    fprintf (stderr, 
		     "%s: Permission denied; must be super-user.\n", progname);
	    exit(1);
	  }
	}

	if ((u_option + r_option + rt_param.multihome) > 1) {
	    fprintf(stderr, "%s: Only one of [-u -r -x] may be used  at the same time.\n",
		    progname);
	    opterr++;
	}

	if (u_option || r_option || rt_param.multihome) {
	  if (d_option) {
	    fprintf(stderr, "%s: -%c and -d options are incompatible\n",
		    progname, (r_option)? 'r': (rt_param.multihome)? 'x' : 'u');
	    opterr++;
	  }
	  if (u_option && (checkCfg || displayCfg || cfgFileName || v_option || w_option)) {
	    fprintf (stderr, "%s: -c, -e, -f, -v and -w can only be used with -r or -x option\n",
		     progname);
	    opterr++;
	  }
	}
	else {
	    if (q_option) {
		fprintf (stderr, "%s: -q can only be used with -u, -r, or -x option\n",
			 progname);
		opterr++;
	    }
	    if (checkCfg || displayCfg || cfgFileName || v_option || w_option) {
		fprintf (stderr, "%s: -c, -e, -f, -v and -w can only be used with -r or -x option\n",
			 progname);
		opterr++;
	    }
	}

	if (opterr || no_option) {
		printf(usage, progname);
		exit(1);
	}

	if (u_option || r_option || rt_param.multihome) {
	    if (checkATStack() == RUNNING) {
	        fprintf(stderr,"The AppleTalk stack is already running.\n");
	    } else {
		if (r_option || rt_param.multihome) {
			router = TRUE;
			if (!cfgFileName) /* alternate cfg file specified? */
			   if (rt_param.multihome)
			      cfgFileName = MH_CFG_FILE;
			   else
			      cfgFileName = AT_CFG_FILE;

			/* does cfg file exist? */
			if (access(cfgFileName,0)) {
				fprintf(stderr,
					"Error, configuration file %s not found\n",
					cfgFileName);
				exit(1);
			}
		}

	        if (do_init() == -1) {
		    exit(1);
		}
		if (checkCfg || displayCfg) {
		    exit(0);
		}
		if (!q_option)
			h_option = TRUE; /* if not -q, ask for default zone */
	    }
	}
	if ((fd = socket(AF_APPLETALK, SOCK_RAW, 0)) < 0) 
		return(-1);
	if ((ioctl(fd, AIOCGETSTATE, (caddr_t)&global_state)) < 0) {
		(void)close(fd);
		return(-1);
	}
	if (global_state.flags & AT_ST_STARTED) {
	    /* do the printing after it's been started, but before it's stopped */

	    if (h_option || n_option || s_option || j_option) {
	      if ((!(global_state.flags & AT_ST_MULTIHOME)) && 
		  (s_option || h_option)) {

		/* if in routing and single-port modes, only get the 
		   default interface */
		cfg.ifr_name[0] = '\0';
		if (ioctl(fd, AIOCGETIFCFG, (caddr_t)&cfg) < 0) {
			(void)close(fd);
			return(-1);
		}
		/* if there's room, terminate the zone string for printing */
		if (cfg.zonename.len < NBP_NVE_STR_SIZE)
		  cfg.zonename.str[cfg.zonename.len] = '\0';

		if (h_option) {
		    if (r_option) {
		    	if (!q_option)
				displayZoneDef(fd);
		    } else
		    	displayZoneDef(fd);
		} else if (s_option)
			print_statistics();

	      } else {
	        /* for each interface that is configured for Appletalk */
		struct ifconf ifc;
		struct ifreq ifrbuf[30], *ifr;

		ifc.ifc_buf = (caddr_t)ifrbuf;
	        ifc.ifc_len = sizeof (ifrbuf);
		if (ioctl(fd, SIOCGIFCONF, &ifc) < 0) {
			fprintf(stderr, "error calling SIOCGIFCONF");
			(void)close(fd);
			return(-1);
		}

		for (ifr = (struct ifreq *) ifc.ifc_buf;
		     (char *) ifr < &ifc.ifc_buf[ifc.ifc_len];
		     ifr = IFR_NEXT(ifr)) {
		  	unsigned char *p, c;

			if (ifr->ifr_addr.sa_family != AF_APPLETALK)
				continue;

			if (*ifr->ifr_name == '\0')
				continue;

			/*
			 * Adapt to buggy kernel implementation (> 9 of a type)
			 */
			p = &ifr->ifr_name[strlen(ifr->ifr_name)-1];
			if ((c = *p) > '0'+9)
			  	sprintf(p, "%d", c-'0');

			strcpy(cfg.ifr_name, ifr->ifr_name);
			if (ioctl(fd, AIOCGETIFCFG, (caddr_t)&cfg) < 0)
				continue;

			/* if there's room, terminate the zone string for printing */
			if (cfg.zonename.len < NBP_NVE_STR_SIZE)
			  cfg.zonename.str[cfg.zonename.len] = '\0';

			if (h_option) {
			  if (u_option || rt_param.multihome) {
			    if (!q_option)
				displayZoneDef(fd);
			  } else
			  	displayZoneDef(fd);
			}
			else if (n_option)
				print_nodeid();
			else if (s_option)
				print_statistics();
			else if (j_option)
				showRouterStats();
		}
	      }
	    }
	    if (t_option) {
		showRoutes();
	    }
	    if (z_option) {
		showZones();
	    }
	    if (m_option && !r_option) {
	        size_t size = (size_t)sizeof(int);

	        /* set the router mix in the kernel */
	        if (sysctlbyname("net.appletalk.routermix", 
				 0, 0, (void *)&router_mix, size) < 0)
		    printf("error setting router mix in kernel\n");
	    }
	    if (d_option)
	    	do_shutdown();
	}
	else { /* not RUNNING */
	    if (!p_option)
		fprintf(stderr, "The AppleTalk stack is not running.\n");
	}
	if (p_option) {
		struct ifconf ifc;
		struct ifreq ifrbuf[30], *ifr;

		ifc.ifc_buf = (caddr_t)ifrbuf;
	        ifc.ifc_len = sizeof (ifrbuf);
		if (ioctl(fd, SIOCGIFCONF, &ifc) < 0) {
			fprintf(stderr, "error calling SIOCGIFCONF");
			(void)close(fd);
			return(-1);
		}

		for (ifr = (struct ifreq *) ifc.ifc_buf;
		     (char *) ifr < &ifc.ifc_buf[ifc.ifc_len];
		     ifr = IFR_NEXT(ifr)) {

			unsigned char *p, c;
			struct ifreq *ifr2;

			/* skip duplicate names */
			for (ifr2 = (struct ifreq *) ifc.ifc_buf; ifr2 < ifr;
			     ifr2 = IFR_NEXT(ifr2))
			  if (strncmp(ifr2->ifr_name, ifr->ifr_name,
				      sizeof(ifr->ifr_name)) == 0)
			    break;
			if (ifr2 < ifr)
			  continue;

			if (*ifr->ifr_name == '\0')
			  continue;
			/*
			 * Adapt to buggy kernel implementation (> 9 of a type)
			 */
			p = &ifr->ifr_name[strlen(ifr->ifr_name)-1];
			if ((c = *p) > '0'+9)
			  sprintf(p, "%d", c-'0');

			(void)print_pram_info(ifr->ifr_name);
		}
	}
	(void)close(fd);
	exit (retval);
} /* main */


static void print_nodeid()
{
	printf("%s: if %s, zone %s\n\n", progname, cfg.ifr_name, cfg.zonename.str);

	printf("\tNetwork Number .................. %u (0x%x)\n", 
		cfg.node.s_net, cfg.node.s_net);
	printf("\tNode ID ......................... %u (0x%x)\n",
		cfg.node.s_node, cfg.node.s_node);

	return;
}


static void
print_statistics()
{
	at_ddp_stats_t ddp_stats;

	printf("%s: if %s, zone %s\n\n", progname, cfg.ifr_name, cfg.zonename.str);

	printf("\tAppleTalk interface.............. %s\n", cfg.ifr_name);

	if (ddp_statistics(&ddp_stats) != 0) {
		fprintf(stderr, 
			"%s: Can't get the network statistics (%s)\n",
			progname, sys_errlist[errno]);
		exit(1);
	}

	printf("\tNetwork Number .................. %u (0x%x)\n", 
		cfg.node.s_net, cfg.node.s_net);
	printf("\tNode ID ......................... %u (0x%x)\n",
		cfg.node.s_node, cfg.node.s_node);

	/* for now, we'll ignore the possibility of null characters 
	   embedded in the zonename (as we have been in the past */
	cfg.zonename.str[cfg.zonename.len] = '\0';

	printf("\tCurrent Zone .................... %s\n",
		cfg.zonename.str);
	printf("\n");
	
	printf("\tBridge net ...................... %u (0x%x)\n",
	       cfg.router.s_net, cfg.router.s_net);
	printf("\tBridge number ................... %u (0x%x)\n",
	       cfg.router.s_node, cfg.router.s_node);
	printf("\tPackets Transmitted ............. %u\n",
		ddp_stats.xmit_packets);
	printf("\tBytes Transmitted ............... %u\n",
		ddp_stats.xmit_bytes);
	printf("\tBest Router Cache used (pkts) ... %u\n",
		ddp_stats.xmit_BRT_used);
	printf("\tPackets Received ................ %u\n",
		ddp_stats.rcv_packets);
	printf("\tBytes Received .................. %u\n",
		ddp_stats.rcv_bytes);
	printf("\tPackets for unregistered socket . %u\n",
		ddp_stats.rcv_unreg_socket);
	printf("\tPackets for out of range socket . %u\n",
		ddp_stats.rcv_bad_socket);
	printf("\tLength errors ................... %u\n",
		ddp_stats.rcv_bad_length);
	printf("\tChecksum errors ................. %u\n",
		ddp_stats.rcv_bad_checksum);
	printf("\tPackets dropped (no buffers) .... %u\n",
		ddp_stats.rcv_dropped_nobuf + 
		ddp_stats.xmit_dropped_nobuf);

	return;
}

int routerStartup(s)
     int s;
{
	at_kern_err_t ke;

	/* start the router */
	memset(&ke, 0, sizeof(ke));
	if ((ioctl(s, AIOCSTARTROUTER, (caddr_t)&ke)) < 0) {
		fprintf(stderr, 
			"%s: AIOCSTARTROUTER failed (%d)\n", 
			progname, errno);
		return(-1);
	} else if (ke.error) {
		switch (ke.error) {
			case KE_CONF_RANGE:
				fprintf(stderr, 
"Conflict between port %d (%s) and port %d (%s)\n\
they are using the same net range (%d-%d)\n",
					ke.port1, ke.name1, ke.port2, ke.name2,
					ke.netr1b, ke.netr1e);
				break;
			case KE_CONF_SEED_RNG:
				fprintf(stderr, 
"Conflict on port %d (%s): Router %d:%d seeds net %d-%d\n\
and not %d:%d as asked in our configuration\n",
					ke.port1, ke.name1, ke.net, ke.node, 
					ke.netr1b, 
					ke.netr1e, ke.netr2b, ke.netr2e);
				break;
			case KE_CONF_SEED1:
				fprintf(stderr, 
"Conflict on port %d (%s): Router %d seeds net %d\n\
and not %d as asked in our configuration\n",
					ke.port1, ke.name1, ke.node, 
					ke.netr1e, ke.netr2e);
				break;
			case KE_CONF_SEED_NODE:
				fprintf(stderr, 
"Conflict on port %d (%s): Node %d:%d seeds %d:%d instead of %d:%d\n",
					ke.port1, ke.name1, ke.net, ke.node, 
					ke.netr1b, 
					ke.netr1e, ke.netr2b, ke.netr2e);
				break;
			case KE_NO_ZONES_FOUND:
				fprintf(stderr, 
"No Zones names received for Port %d (%s) on net %d:%d\n",
					ke.port1, ke.name1, ke.netr1b, ke.netr1e);
				break;
			case KE_NO_SEED:
				fprintf(stderr, 
"No seed information for port %d (%s) was found on the net\n",
					ke.port1, ke.name1);
				break;
			case KE_INVAL_RANGE:
				fprintf(stderr, 
"Port %d (%s) is using an invalid network range (%d:%d)\n",
					ke.port1, ke.name1, ke.netr1b, ke.netr1e);
				break;
			case KE_SEED_STARTUP:
				fprintf(stderr, 
"Problem, port %d (%s) Router %d:%d seeds in the startup range\n",
					ke.port1, ke.name1, ke.net, ke.node);
				break;
			case KE_BAD_VER:
				fprintf(stderr, 
"Received a bad version (v%d) RTMP packet from node %d:%d\n",
					ke.rtmp_id, ke.net, ke.node);
				break;
			case KE_RTMP_OVERFLOW:
				fprintf(stderr, 
"RTMP Route Table overflow. Too many routes. Increase RTMP Table size > %d\n",
					(rt_param.rtmp_table_sz)?
					rt_param.rtmp_table_sz: RT_DEFAULT);
				break;
			case KE_ZIP_OVERFLOW:
				fprintf(stderr,
"ZIP Zone Table overflow. Too many zones. Increase ZIP Table size > %d\n",
					(rt_param.zone_table_sz)?
					rt_param.zone_table_sz: ZT_DEFAULT);
				break;
			default:
				fprintf(stderr,
					"Unknown kernel error code:%d\n", 
					ke.error);
		}
		return(-1);
	}
	return(0);
} /* routerStartup */

int do_init()
{
	int i, s, tmp;
	char *p;
	struct ifreq ifr;
	char ifname[128];

	/* get the device information:
	 	from the config file, for router and multihome mode, and 
		from the command line, for single-port mode
	*/
	memset(&elapcfg[0], 0, sizeof(elapcfg));
	if (router) {
	  	/* read & validate config file */
	    	if (getConfigInfo(elapcfg, if_zones, cfgFileName, 
				  checkCfg, displayCfg, rt_param.multihome)) {
			return(-1);
		}
		if (checkCfg)
			printf("Configuration file checked.\n");
		if (displayCfg || checkCfg)
			return(0);	/* if just checking cfg, we passed */
	} else {
	        at_nvestr_t zone_name;
		struct at_addr init_addr;

		elapcfg[0].flags |= ELAP_CFG_HOME;
		strcpy(elapcfg[0].ifr_name, et_interface);

		/* in single-port mode, if a default zone can be read from 
		   the nvram file for the interface, it will be sent to the 
		   kernel via the AIOCSIFADDR IOCTL */
		if (at_getdefaultzone(et_interface, &zone_name) == 0)
			if (!DEFAULT_ZONE(&zone_name)) {
			  elapcfg[0].zonename = zone_name;
			  /* Check if we can reuse the same net/node address
			     we have saved.  Don't try to reuse the old address
			     unless there's a good zone to go with it. */
			  if (at_getdefaultaddr(et_interface, &init_addr) == 0)
                                elapcfg[0].node = init_addr;
			}
	}

	/* open AppleTalk control socket */
	if ((s = socket(AF_APPLETALK, SOCK_RAW, 0)) < 0) {
	  	fprintf(stderr, "%s: socket failed\n", progname);
		return -1;
	}

	/* set routing/multihome parameters */
	if (router) {
	  	if (ioctl(s, AIOCSETROUTER, (caddr_t)&rt_param) < 0) {
			fprintf(stderr,
                                "%s: AIOCSETROUTER returned %s (%d)\n", 
				progname, sys_errlist[errno], errno);
			goto error;
		}
	}

	/* configure all of the interfaces up */
	for (i=0; elapcfg[i].ifr_name[0] ; i++) {
	        /* make sure that the interface name is valid */
		for (p = elapcfg[i].ifr_name; *p != '\0'; p++) {
			if (isdigit(*p))
				break;
		}
		if (*p == '\0' || !isdigit(*p)) {
			fprintf(stderr,	"%s: %s: bad device name\n", progname, 
				elapcfg[i].ifr_name);
			goto error;
		}

		/* do an ifconfig on the interface name */
		sprintf(ifname, "%s %s up", IFCONFIG_CMD, elapcfg[i].ifr_name);
		if (0 != (system(ifname))) {
			fprintf(stderr, "%s: '%s' failed\n", progname, ifname);
			goto error;
		}

		/* Initialize the AT kernel structures associated with the 
		   interface using SIOCSIFADDR and AIOCSIFADDR IOCTLs */
		strncpy(ifr.ifr_name, elapcfg[i].ifr_name, sizeof(ifr.ifr_name));
		ifr.ifr_addr.sa_family = AF_APPLETALK;
		if (ioctl(s, SIOCSIFADDR, (caddr_t)&ifr) < 0) {
			fprintf(stderr, 
				"%s: SIOCSIFADDR for %s returned %s (%d)\n", 
				progname, ifr.ifr_name, 
				sys_errlist[errno], errno);
			goto error;
		}

		if (ioctl(s, AIOCSIFADDR, (caddr_t)&elapcfg[i]) < 0) {
			fprintf(stderr, 
				"%s: AIOCSIFADDR for %s returned %s (%d)\n", 
				progname, elapcfg[i].ifr_name, 
				sys_errlist[errno], errno);
			switch (errno) {
			case EEXIST :
			  fprintf(stderr,
				  "%s: another home port already designated\n",
				progname);
			  break;
			case EACCES :
			  fprintf(stderr,
				  "%s: permission denied\n", progname);
			  break;
			case EPERM :
			  fprintf(stderr,
				  "%s: port already up, can't designate as home port\n",
				progname);
			  break;
			case EINVAL :
			  fprintf(stderr,
				  "%s: invalid interface specified\n", 
				  progname);
			  break;
			case EFAULT :
			  fprintf(stderr,
				  "%s: can't change range in current i/f state\n",
				  progname);
			  break;
			case EOPNOTSUPP :
			  fprintf(stderr,
				  "%s: error registering packet type\n", 
				  progname);
			  break;
			case EALREADY :
			  fprintf(stderr,
				  "%s: interface %s is already running\n", 
				  progname, et_interface);
			  break;
			default :
			  fprintf(stderr, "%s: %s (%d)\n", progname, 
				  sys_errlist[errno], errno);
			  break;
			}
			goto error;
		}
	  }

	/* *** add_local_zones() and set_local_zones() will be moved from
	       lap_init.c to appletalk.c as soon as the LAP_IOCs are
	       converted to standard IOCTLs *** */
	if (router) {
		/* in router mode, add zones */
		if (!rt_param.multihome)
			(void)add_local_zones(if_zones);
		if (routerStartup(s) < 0)
			goto error;
	} else {
	    if (set_local_zones() < 0)
		goto error;
	}

	if(register_this_node(s) != 0) {
		fprintf(stderr,"%s: node registration failed\n", progname);
		goto error;
	}

	for (i=0; elapcfg[i].ifr_name[0] ; i++) {
		if ((elapcfg[i].flags & ELAP_CFG_HOME) ||
		    rt_param.multihome) {

		    /* in router / multihome mode, if a default zone can be 
		       read from the nvram file for the interface, the 
		       kernel is updated after startup has taken place */
		    if (router) {
		  	at_nvestr_t nvramzone;

			if (!at_getdefaultzone(elapcfg[i].ifr_name, 
					       &nvramzone)) {
			      at_def_zone_t defzone;

			      strcpy(defzone.ifr_name, elapcfg[i].ifr_name);
			      defzone.zonename = nvramzone;
							  
			      if ((ioctl(s, AIOCSETDEFZONE, (caddr_t)&defzone))) {
				if (nvramzone.len < NBP_NVE_STR_SIZE)
					nvramzone.str[nvramzone.len] = '\0';
				fprintf(stderr, 
					"%s: AIOCSETDEFZONE failed for %s %s\n", 
					progname, elapcfg[i].ifr_name, nvramzone.str);
			      }
			}
		    }

		    /* in any case, save the values AppleTalk came up with
		       in the nvram file, for next time */
		    (void)at_savecfgdefaults(s, elapcfg[i].ifr_name);
		}
	  }
	close(s);
	return(0);

 error:
	(void)ioctl(s, AIOCSTOPATALK, (caddr_t)&tmp);
	close(s);
	fprintf(stderr, "Failed to start the AppleTalk stack.\n");
	return(-1);

} /* do_init */

static void
do_shutdown ()
{
	int *pid_buf, s, tmp;

	if (atalkState(0, (char **)&pid_buf) == 0) {
	  	if (pid_buf) {
			down_error(pid_buf);
			exit(1);
		}
	}

	if ((s = socket(AF_APPLETALK, SOCK_RAW, 0)) < 0) 
	  	fprintf(stderr, "%s: socket = %d\n", progname, s);
	else {
		if ((ioctl(s, AIOCSTOPATALK, (caddr_t)&tmp)) < 0) {
			if (errno == EACCES)
			  fprintf(stderr, "%s: permission denied\n", progname);
			else
			  fprintf(stderr, "%s: error: %s\n", progname, 
				  sys_errlist[errno]);
			close(s);
			exit(1);
		}
	}
	close(s);
} /* do_shutdown */

static void
down_error(pid_buf)
	int *pid_buf;
{
	/* pid_buf contains ids of processes still holding on to resources */
	fprintf(stderr, "Cannot bring down AppleTalk.\n");
	fprintf(stderr, "You must first terminate the following processes:\n");
	while (*pid_buf != 0) {
		fprintf(stderr, "PID=%d\n", *pid_buf);
		pid_buf++;
	}
}

static int
register_this_node(fd)
     int fd;		/* control socket */
{
	struct utsname	u_name;
	at_nbp_reg_t	reg;
	at_retry_t retry = {1, 1, 1};

	if (uname(&u_name) < 0)
		return (-1);
	nbp_make_entity(&reg.name, u_name.nodename, u_name.sysname, "*");

	if (nbp_reg_lookup(&reg.name, &retry) != 0) {
		fprintf(stderr,
			"%s: identical node was previously registered\n", 
			progname);
		return (-1);
	}

	/* the net and node will be automatically filled in with address
	   information from the default interface. */
	reg.addr.net = 0;
	reg.addr.node = 0;
	reg.addr.socket = DDP_SOCKET_1st_DYNAMIC;

	reg.ddptype = 0;

	if ((ioctl(fd, AIOCNBPREG, (caddr_t)&reg)) < 0) {	
		fprintf(stderr,
			"%s: AIOCNBPREG failed\n", progname);
		return(-1);
	}

	return(0);
} /* register_this_node */

static void
displayZoneDef(fd)
     int fd;
{
#define DISPLAY_ZONES 10
	at_nvestr_t *zp;
	at_def_zone_t defzone;
	u_char buf[ATP_DATA_SIZE+1];
	char input_buff[80], 
	     zone[DISPLAY_ZONES][ZIP_MAX_ZONE_LENGTH],
	     *cp;
	int start = ZIP_FIRST_ZONE, 
	    i = 0, times = 0, count = 0, zone_index = 0;

	if (!fd) 
		return;

	printf("Current zone for interface %s is %s\n",
	       cfg.ifr_name, cfg.zonename.str);

	while (times < DISPLAY_ZONES && zone_index < DISPLAY_ZONES && 
	       start != ZIP_NO_MORE_ZONES && count != -1) {
		if ((count = 
		     zip_getlocalzones(cfg.ifr_name, &start, buf, sizeof(buf))) > 0) {
		  if (count && !zone_index) 
		  	printf("Zones found:\n");
		  for (i=zone_index, cp=buf, zp = (at_nvestr_t *)cp; 
		       i < (count+zone_index) && i < DISPLAY_ZONES && zp->len;
		       i++, cp += (zp->len+1), zp = (at_nvestr_t *)cp) {
			strncpy(zone[i], zp->str, zp->len);
			zone[i][zp->len] = '\0';
			printf("%2d: %s\n", i+1, zone[i]);
		  }
		}
		times++;
		zone_index += count;
	}
	if (zone_index > 1) {
	  	zone_index = 0;
		printf("\nEnter <return> to exit.\n");
again:
		printf ("\nZone Number? ");
		gets(input_buff);
		if (input_buff[0] == '\0')
		  return;;
		if (!isdigit(input_buff[strspn(input_buff, " \t")]))
		  goto again;
		zone_index = atoi(input_buff);
		if (zone_index < 1 || zone_index > i) {
		  printf("That's not a valid choice. Try again.\n");
		  goto again;
		}

		strcpy(defzone.ifr_name, cfg.ifr_name);
		strcpy(defzone.zonename.str, zone[zone_index-1]);
		defzone.zonename.len = strlen(zone[zone_index-1]);

		/* set the default in the config file, to be used  next time
		   appletalk is started */
		(void)at_setdefaultzone(cfg.ifr_name, &defzone.zonename);

		/* change the default zone now, without restarting appletalk */
		if ((ioctl(fd, AIOCSETDEFZONE, (caddr_t)&defzone)) < 0) {
			printf("AIOCSETDEFZONE errno = %d\n", errno);
		} else {
			printf("Default zone changed to %s\n", defzone.zonename.str);
		}
	} else {
		if (!zone_index)
			printf("No local zones found.\n");
	}
}

#define STATS_HEADER1 \
"\n-------- Appletalk Configuration -----------\n"
#define STATS_HEADER2 \
"                             Network:\n"
#define STATS_HEADER3 \
" I/F  State             Range       Node      Default Zone\n"
#define STATS_HEADER4 \
" ---- ----------------- ----------- --------- -------------------------\n"

int first = 0;
static int showRouterStats()
{
	u_char state = cfg.flags & LAP_STATE_MASK;

	if (!first++)
		fprintf(STDOUT, "%s%s%s%s",
			STATS_HEADER1,
			STATS_HEADER2,
			STATS_HEADER3,
			STATS_HEADER4);
	fprintf(STDOUT, "%c%-4s %-18s",
		(cfg.flags & AT_IFF_DEFAULT)? '*' : ' ',
		cfg.ifr_name, 
		(state == LAP_OFFLINE)? "Offline":
		(state == LAP_ONLINE)? "Online":
		(state == LAP_ONLINE_FOR_ZIP)? "Online for ZIP":
		(state == LAP_ONLINE_ZONELESS)? "Online zoneless": "Unknown");

	fprintf(STDOUT, "%5d-%-5d %5d:%-3d ", 
		    cfg.netStart,
		    cfg.netEnd,
		    cfg.node.s_net,
		    cfg.node.s_node);
	fprintf(STDOUT, "%s\n", cfg.zonename.str);

	/* *** Should stats be printed too? *** */

	return(0);
} /* showRouterStats */
