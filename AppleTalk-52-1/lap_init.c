/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/* Title:	lap_init.c
 *
 * Facility:	Generic AppleTalk Link Access Protocol Interface
 *		(ALAP, ELAP, etc...)
 *
 * Author:	Gregory Burns, Creation Date: April-1988
 *
 ******************************************************************************
 *                                                                            *
 *        Copyright (c) 1988, 1998 Apple Computer, Inc.                       *
 *                                                                            *
 *        The information contained herein is subject to change without       *
 *        notice and  should not be  construed as a commitment by Apple       *
 *        Computer, Inc. Apple Computer, Inc. assumes no responsibility       *
 *        for any errors that may appear.                                     *
 *                                                                            *
 *        Confidential and Proprietary to Apple Computer, Inc.                *
 *                                                                            *
 ******************************************************************************
 */

/* "@(#)lap_init.c: 2.0, 1.19; 2/26/93; Copyright 1988-92, Apple Computer, Inc." */

#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
#include <string.h>
#include <mach/boolean.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/sockio.h>
#include <net/if.h>

#include <netat/appletalk.h>
#include <netat/lap.h>
#include <netat/ddp.h>
#include <netat/nbp.h>
#include <netat/atp.h>
#include <netat/at_var.h>
#include <netat/at_config.h>
#include <netat/routing_tables.h>

#include <AppleTalk/at_proto.h>
#include <AppleTalk/at_paths.h>

#define	SET_ERRNO(e) errno = e

typedef struct if_cfg {
        unsigned        avail[IF_TYPENO_CNT];
        char            state[IF_TYPENO_CNT][IF_TOTAL_MAX];
        unsigned        max[IF_TYPENO_CNT];
	unsigned        home_type;              /* type of home interface */
        unsigned        home_number;
} if_cfg_t;

#define IF_TYPE_1 	"en"	/* ethernet */

/*#define MSGSTR(num,str)		catgets(catd, MS_LAP_INIT, num,str) */
#define MSGSTR(num,str)		str 

	/* multi-port setup defines */
#define MAX_ZONES		50	/* max cfg file zone count */
#define MAX_LINE_LENGTH		240	/* max size cfg file line */
#define INVALID_ZIP_CHARS	"=@*:\377"
#define MAX_NET_NO		0xFEFF	/* max legal net number */
#define NO_HOME_PORT		999	/* indicates no home port
											   selected in home_number */

#define COMMENT_CHAR 		'#'	/* version checking */

	/* printout format defines */
#define ZONE_IFS_PER_LINE	6	/* # of if's to list per line for
				           each zone for showCfg() */
#define Z_MAX_PRINT		15	/* maximum # zones to print */

/* a lot of things are indexed by interface type and unit */
#ifdef NOT_USED
char *if_types[] = { IF_TYPE_1, IF_TYPE_2, IF_TYPE_3, IF_TYPE_4, IF_TYPE_5}; 
#else
char *if_types[] = { IF_TYPE_1 }; 
#endif
unsigned		seed[IF_TYPENO_CNT];	/* I/F bit-mapped seed port ID */
#define IF_NO(c)    (atoi(&c[2]))       /* return i/f number from h/w name
					   (e.g. 'et2' returns 2) */

char			homePort[5];			/* name of home port */

/* prototypes */

int at_setdefaultzone(char * if_name, at_nvestr_t *zone_name);
int at_getdefaultzone(char * if_name, at_nvestr_t *zone_name);
int at_getdefaultaddr(char * if_name, struct at_addr *init_address);

int add_local_zones(), set_local_zones();
void showCfg();

int stateBuff[0x400];
int pidBuff[0x400];

static FILE *STDOUT = stdout;

/* These functions set/get the defaults in/from persistent storage */

/* PRAM state information */
#define PRAM_FILE             NVRAM
#define NVRAMSIZE		256

#define INTERFACE_OFFSET	1
#define ADDRESS_OFFSET		(INTERFACE_OFFSET+IFNAMESIZ)
#define ZONENAME_OFFSET		(ADDRESS_OFFSET+sizeof(struct at_addr))

char pram_file[80];

static int readxpram(if_name, buf, count, offset)
	char *if_name, *buf;
	int count;
	int offset;
{
	int fd;

	sprintf(pram_file, "%s.%s", PRAM_FILE, if_name);

	if ((fd = open(pram_file, O_RDONLY)) == -1) {
		return(-1);
	}

	if ((int)lseek(fd, (off_t)offset, SEEK_SET) != offset) {
		close(fd);
		return(-1);
	}

	if (read(fd, buf, count) != count) {
		close(fd);
		return(-1);
	}
#ifdef COMMENT_OUT
	printf("readxpram: read %s %d bytes offset %d: %s\n", 
	       pram_file, count, offset,
	       (offset==ZONENAME_OFFSET)? ((at_nvestr_t *)buf)->str: "" );
#endif
	close(fd);
	return(0);
}

static int writexpram(if_name, buf, count, offset)
	char *if_name, *buf;
	int count;
	int offset;
{
	int fd;
	char buffer[NVRAMSIZE];

	sprintf(pram_file, "%s.%s", PRAM_FILE, if_name);

	if ((fd = open(pram_file, O_RDWR)) == -1) {
		if (errno == ENOENT) {
			fd = open(pram_file, O_RDWR|O_CREAT, 0644);
			if (fd == -1)
				return(-1);
			memset(buffer, 0, sizeof(buffer));
			if (write(fd, buffer, sizeof(buffer)) 
			    != sizeof(buffer)) {
				close(fd);
 				unlink(pram_file);
				return(-1);
			}
			(void) lseek(fd, (off_t)0, 0);
		}
		else
			return(0);
	}

	if ((int)lseek(fd, (off_t)offset, SEEK_SET) != offset) {
		close(fd);
		return(-1);
	}
#ifdef COMMENT_OUT
	printf("writexpram: wrote %s %d bytes offset %d\n", pram_file, 
	       count, offset);
#endif
	if (write(fd, buf, count) != count) {
		close(fd);
		return(-1);
	}

	close(fd);
	return(0);
}

/* save the current configuration in pram */
int at_savecfgdefaults(fd, if_name)
     int fd;
     char *if_name;
{
	int tmp_fd = 0;  
	at_if_cfg_t cfg;	/* used to read cfg */

	if (!fd) {
	  if ((tmp_fd = socket(AF_APPLETALK, SOCK_RAW, 0)) < 0)
		return(-1);
	  fd = tmp_fd;
	}		

	strncpy(cfg.ifr_name, if_name, sizeof(cfg.ifr_name));
	if (ioctl(fd, AIOCGETIFCFG, (caddr_t)&cfg) < 0) {
		fprintf(stderr, "AIOCGETIFCFG failed for %s (%d)\n", 
			cfg.ifr_name, errno);
		if (tmp_fd)
			(void)close(tmp_fd);
		return(-1);
	} else {
		(void)writexpram(if_name, &cfg.node, sizeof(struct at_addr), 
				 ADDRESS_OFFSET);
		(void)at_setdefaultzone(if_name, &cfg.zonename);
		(void)writexpram(if_name, if_name, IFNAMESIZ, 
				 INTERFACE_OFFSET);
	}
	if (tmp_fd)
		(void)close(tmp_fd);
	return(0);
}

int at_getdefaultaddr(if_name, init_address)
     char *if_name;
     struct at_addr *init_address;
{
	if (!readxpram(if_name, init_address, sizeof(struct at_addr), ADDRESS_OFFSET)){
/*		printf("at_getdefaultaddr: readx net number %d, node %d\n", 
		init_address->s_net, init_address->s_node);
*/
		return(0);
	}
	else
		return (-1);
}

int at_getdefaultzone(if_name, zone_name)
     char *if_name;
     at_nvestr_t	*zone_name;
{
	if (!if_name || !strlen(if_name))
        	return(-1);

	if (!readxpram(if_name, zone_name, sizeof(at_nvestr_t), ZONENAME_OFFSET))  {
		return(0);
	}
	else {
		memset(zone_name, 0, sizeof(at_nvestr_t));
		return(-1);
	}
}

int at_setdefaultzone(if_name, zone_name)
     char *if_name;
     at_nvestr_t	*zone_name;
{
	if (!if_name || !strlen(if_name))
        	return(-1);

	if (DEFAULT_ZONE(zone_name)) {	/* If we don't have a router  */
		return(0);		/* return w/o writing to PRAM */
	}

	if (!writexpram(if_name, zone_name, sizeof(at_nvestr_t), ZONENAME_OFFSET)) {
		return(0);
	}
	else
		return(-1);
}

int print_pram_info(if_name)
     char *if_name;
{
	at_nvestr_t zonename;
	struct at_addr netnumber;
	char zone[64];
	int config_err = 0;
	char if_name_buf[IFNAMESIZ];

	if (!readxpram(if_name, if_name_buf, IFNAMESIZ, INTERFACE_OFFSET)) {
	  printf("\n\tAppleTalk interface.............. %s\n", if_name_buf);

	  if (readxpram(if_name, &zonename, sizeof(at_nvestr_t), ZONENAME_OFFSET)) {
	  	fprintf(stderr, "PRAM Zone info not found (%d) \n", errno);
		config_err++;
	  }

	  if (readxpram(if_name, &netnumber, sizeof(struct at_addr), ADDRESS_OFFSET)) {
		  fprintf(stderr, "PRAM Address info not found (%d) \n", errno);
		  config_err++;
	  }

	  strncpy(zone, zonename.str, zonename.len);
	  zone[zonename.len] = '\0';

	  if (zonename.len == 0)
	  	strcpy(zone, "*");

	  printf(MSGSTR(M_PRM_ZONE, "\tPRAM default zonename ........... %s\n"),
		 zone);
	  printf(MSGSTR(M_PRM_NET, "\tPRAM netnumber .................. %u (%#x)\n"),
		 netnumber.s_net, netnumber.s_net);
	  printf(MSGSTR(M_PRM_NODE, "\tPRAM node id .................... %u (%#x)\n"),
		 netnumber.s_node, netnumber.s_node);
	}
	if (config_err)
		return(-1);

	return(0);
} /* print_pram_info */


/* in router mode, before router startup */
int add_local_zones(if_zones)
     if_zone_info_t if_zones[]; /* zone info from cfg file */
{
	int size, if_id, i, error = 0;

	if ((if_id = socket(AF_APPLETALK, SOCK_RAW, 0)) < 0) {
		fprintf(stderr, "routerStartup: socket failed (%d)\n", errno);
		return(-1);
	}		

	for (i=0; i < MAX_ZONES && if_zones[i].zone_name.len; i++) {
#ifdef APPLETALK_DEBUG
		printf("adding zone %s\n",if_zones[i].zone_name.str);
#endif
		size = sizeof(if_zone_info_t);
		if (at_send_to_dev(if_id, LAP_IOC_ADD_ZONE, &if_zones[i], 
				   &size) == -1) {
			/* if length permits null terminate string for printing */
			if (if_zones[i].zone_name.len < NBP_NVE_STR_SIZE)
				if_zones[i].zone_name.str[if_zones[i].zone_name.len] = '\0';
			fprintf(stderr, "routerStartup: error adding zone %s\n",
				if_zones[i].zone_name.str);
			error = -1;
			/* but keep going */
		}
	}
	(void)close(if_id);
	return(error);
}

/* in single-port mode, after startup is essentially complete */
int set_local_zones()
{
	int size = 0;
	int i;
	int if_id;
	char buf[ATP_DATA_SIZE+1], *cp;
	at_nvestr_t *zp;
	int count = 0, context = ZIP_FIRST_ZONE;

	if ((if_id = socket(AF_APPLETALK, SOCK_RAW, 0)) < 0)
		return(-1);

	/* *** need AIOCADDLOCALZONE no-filler zonename list *** */

	/* Local zones need to be set so that the kernel will be able
	   to figure out whether a zone is local, when an NBP entity is
	   registered later. */

	while (context != ZIP_NO_MORE_ZONES) {
		if ((count = zip_getlocalzones(ZIP_DEF_INTERFACE,
					       &context, 
					       &(buf[0]),
					       (ATP_DATA_SIZE+1))) > 0) {
		       for (i=0, cp=buf, zp = (at_nvestr_t *)cp; 
			    i < count && zp->len;
			    i++, cp += (zp->len+1), zp = (at_nvestr_t *)cp) {
			 ;
		       }
		
		       size = (int)(cp-buf);
		       /* feed local zones to NBP, in the kernel */
		       i = at_send_to_dev(if_id, LAP_IOC_SET_LOCAL_ZONES, 
					   &buf[0], &size);
		       if (i) {
				fprintf(stderr, 
					"error setting localzones %d\n",
					errno);
				goto error;

			}
		} else {
			break;  
		}
	}

	(void)close(if_id);
	return(0);
error:
	(void)close(if_id);
	return(-1);
} /* set_local_zones */

int getConfigInfo(elapcfgp, zonep, cfgFileName, checkCfg, displayCfg, mh)
     at_if_cfg_t elapcfgp[];
     if_zone_info_t zonep[];
     char  *cfgFileName;      /* optional alternate router.cfg file */
     short checkCfg;       /* if true, just check config file */
     short displayCfg;       /* display configuration only */
     short mh;       /* multihoming mode of router */

/* obtains and checks zone and port configuration information from the 
   configuration file. fills in the 2 arrays passed

   returns   0 if no problems were encountered.
			-1 if any error occurred
*/

{
	if_cfg_t filecfg;

	if (getIFInfo(elapcfgp, &filecfg, cfgFileName, checkCfg, displayCfg, mh))
		return(-1);
	if (!elapcfgp->ifr_name[0]) {
		fprintf(stderr, 
			MSGSTR(M_NO_IF_CFG, 
			"There are no interfaces configured\n"));
		return(-1);
	}
	
 	if (getZoneInfo(zonep, &filecfg, cfgFileName, checkCfg, mh) ||
	    checkSeeds(elapcfgp, zonep))
   		return(-1);

	if (displayCfg)
		showCfg(elapcfgp, zonep);

	return(0);
} /* getConfigInfo */


int getIFInfo(elapcfgp, filecfgp, cfgFileName, checkCfg, displayCfg, mh)
     at_if_cfg_t elapcfgp[];
     if_cfg_t	*filecfgp;
     char  *cfgFileName;	/* optional alternate router.cfg file */
     short checkCfg;		/* if true, just check config file */
     short displayCfg;		/* display configuration only */
     short mh;			/* multihoming mode of router */

/* reads interface configuration information from source file and
   places valid entries into cfgp 

   returns  0 on success
            -1 if any errors were encountered
*/
{
	FILE 	*pFin;			/* input cfg file */
	char	linein[MAX_LINE_LENGTH], 
	  	buf[MAX_LINE_LENGTH], 
	  	buf1[MAX_LINE_LENGTH], 
	  	*pc1,
		errbuf[100];
	int	i,x;
	int	ifno=0;
	int	parmno;
	int	lineno=0;
	int	home = FALSE;
	int	bad = TRUE;
	int	gotNetStart;		/* true if entry had starting network # */
	int	done;
	int	ifType;
	int	error = FALSE;
	int	unitNo;			/* I/F unit no ( the n in etn) */

	if (!(pFin = fopen(cfgFileName, "r"))) {
		fprintf(stderr,  MSGSTR(M_OPEN_CFG,
			"error opening configuration file: %s\n"), cfgFileName);
		return(-1);
	}
	filecfgp->home_number = NO_HOME_PORT;
	
	while (!feof(pFin)) {
	
		if (!fgets(linein, sizeof(linein)-1, pFin)) {
			continue;
		}
		lineno++;
		if (linein[0] == COMMENT_CHAR)  	/* if comment line */
			continue;
		if (linein[0] == ':')			/* if zone entry, skip */
			continue;

		strcpy(buf,linein);

		pc1 = strtok(buf,":");			/* pc1 -> first parm */
		parmno=0;
		bad = FALSE;
		gotNetStart = FALSE;
		done = FALSE;

		if (pc1) do {				/* do loop start for current line */
			if (sscanf(pc1," %s",buf1) != 1) /* skip whitespace */
				break;
			switch(parmno) {
			case 0:		/* I/F name */
				if ((ifType = getIFType(buf1)) != -1) {
					unitNo = IF_NO(buf1);
					/* avail is a bitmap which shows whether
					   the interface has been used, and is
					   indexed by ifType */
					if (filecfgp->avail[ifType] & 1<<unitNo) {
						sprintf(errbuf, MSGSTR(M_IF_USED,
							"interface %s already used"), buf1);
						bad = TRUE;
					}
					if (!bad)
						strncpy(elapcfgp[ifno].ifr_name, buf1, 4);
				}
				else {
					sprintf(errbuf, 
						MSGSTR(M_BAD_IF, 
						       "bad interface %s"), buf1);
					bad = TRUE;
			 	}	
				break;
			case 1:		/* net range or home desig */
			case 2:
				if (sscanf(buf1,"%d",&x) != 1){	
				    if (buf1[0] == '*') {		/* must be home then */
				      	if (home++) {
					    sprintf(errbuf, 
						    MSGSTR(M_MULT_HOME,
							   "multiple home designations (%d) "), parmno);
					    bad = TRUE;
					}
					elapcfgp[ifno].flags = ELAP_CFG_HOME;
					done = TRUE;			/* '*' must be last */
					sprintf(homePort,elapcfgp[ifno].ifr_name);
				    }
				    else {
				        sprintf(errbuf, MSGSTR(M_BAD_FMT, "invalid format "));
					bad = TRUE;
				    }
				} else	{	/* this is an actual net number */
				    if (x <= 0 || x >= MAX_NET_NO) {
				        sprintf(errbuf, MSGSTR(M_BAD_NET, 
							       "invalid net:%d "),x);
					bad = TRUE;
				    }
				    if (parmno == 1) {
					elapcfgp[ifno].netStart = x;
					gotNetStart = TRUE;
				    }
				    else {
				        if (elapcfgp[ifno].netStart > x) {
					    sprintf(errbuf, 
						    MSGSTR(M_END_LT_START,
							   "ending net less than start"));
					    bad = TRUE;
					}
					else	
					    elapcfgp[ifno].netEnd = x;
				    }
				}
				break;
			case 3:
				if (buf1[0] == '*') {	/* only valid entry for parm 3 */
				    if (home++) {
				        sprintf(errbuf, 
						MSGSTR(M_MULT_HOME,
						       "multiple home designations (%d) "), parmno);
					bad = TRUE;
				    }
				    elapcfgp[ifno].flags = ELAP_CFG_HOME;
				    sprintf(homePort,elapcfgp[ifno].ifr_name);
				    done = TRUE;
				    break;
				}
				/* fall through */
			default:
				fprintf(stderr,
					"extra input ignored:(%s)\n", buf1);
				done = TRUE;


			} /* end switch */
			if (bad)
				break;
			parmno++;
		} while(!done && pc1 && (pc1  = strtok(NULL,":")));
		if (!bad) {
			if (elapcfgp[ifno].ifr_name[0]) {	/* if entry used, check it */
			    if (gotNetStart) {
			        if (!elapcfgp[ifno].netEnd)	/* if no end, end = start */
				    elapcfgp[ifno].netEnd = elapcfgp[ifno].netStart;

				/* check for range conflicts */
				for (i=0; i<ifno; i++) {
				    
				    if ((elapcfgp[ifno].netEnd >= elapcfgp[i].netStart &&
					 elapcfgp[ifno].netEnd <= elapcfgp[i].netEnd)  ||
					(elapcfgp[ifno].netStart >= elapcfgp[i].netStart &&
					 elapcfgp[ifno].netStart <= elapcfgp[i].netEnd) ||
					(elapcfgp[i].netEnd >= elapcfgp[ifno].netStart &&
					 elapcfgp[i].netEnd <= elapcfgp[ifno].netEnd)  ||
					(elapcfgp[i].netStart >= elapcfgp[ifno].netStart &&
					 elapcfgp[i].netStart <= elapcfgp[ifno].netEnd))  {
				        sprintf(errbuf,  
						MSGSTR(M_RANGE_CONFLICT,
						       "%s net range conflict with %s"),
						elapcfgp[ifno].ifr_name,
						elapcfgp[i].ifr_name);
					bad = TRUE;
					break;
				    }
				}
			    }
			    if (!bad) {		
			        /* set avail here */
			        filecfgp->avail[ifType] |= 1<<IF_NO(elapcfgp[ifno].ifr_name);
				if (elapcfgp[ifno].flags & ELAP_CFG_HOME) {
				    filecfgp->home_type   = ifType;
				    filecfgp->home_number = IF_NO(elapcfgp[ifno].ifr_name);
				}
				if (gotNetStart)    /* identify as seed */
				    seed[ifType] |= 1<<unitNo;
				
				ifno++;		
			    }
			} /* end if if_name */
		} 
		if (bad ) {
 			/* reset this entry */
			memset(&elapcfgp[ifno], NULL,sizeof(elapcfgp[ifno]));
			if (checkCfg)
				/* finish error msg */
				fprintf(stderr,  MSGSTR(M_LINE1,
					"error,  %s\n"),errbuf);
			else
				fprintf(stderr,  MSGSTR(M_LINE,
				"error, line %d. %s\n%s\n"),lineno,errbuf,linein);
			error = TRUE;
			break;
		}	/* end if bad */
	}
	if (!mh && !home && !error && ifno) {
		if (checkCfg || displayCfg) 
			fprintf(stderr, MSGSTR(M_NO_HOME_PT_W, 
				"Warning, no home port specified.\n"\
				"(You must designate one interface as the home port before\n"\
				"starting Appletalk in router mode)\n\n"));
		else {
			error = TRUE;
			fprintf(stderr,  MSGSTR(M_NO_HOME_PT,
				"error, no home port specified\n"));
		}
	}
	if (mh && !home)
		elapcfgp[0].flags = ELAP_CFG_HOME;
	fclose(pFin);
	return(error ? -1 : 0);
} /* getIFInfo */

int getZoneInfo(zonep, filecfgp, cfgFileName, checkCfg, mh)
     if_zone_info_t	zonep[];
     if_cfg_t	*filecfgp;
     char  *cfgFileName;   /* optional alternate router.cfg file */
     short checkCfg;    /* if true, just check config file */
     short mh; 		/* multihoming mode of router */
/* reads zone information from configuration file and places valid entries 
   into zonep 

   returns  0 on success
	   -1 if any errors were encountered
*/
{
	FILE 	*pFin;			/* input cfg file */
	char	linein[MAX_LINE_LENGTH], buf[MAX_LINE_LENGTH], 
			buf1[MAX_LINE_LENGTH], *pc1, *pc2;
	char	tbuf1[NBP_NVE_STR_SIZE+20];
	char	tbuf2[NBP_NVE_STR_SIZE+20];
	char	curzone[NBP_NVE_STR_SIZE+20];
	char	errbuf[100];
	int	i,j;
	int	parmno;
	int	lineno=0, homeLine=0;
	int	home  = FALSE;
	int	bad   = TRUE;
	int	done  = FALSE;
	int	error = FALSE;
	int	zoneno = 0;
	int	ifType;
	int 	len;
	int	isseed;
	int	gotHomePort =  ( filecfgp->home_number != NO_HOME_PORT );

	if (!(pFin = fopen(cfgFileName, "r"))) {
		fprintf(stderr,  MSGSTR(M_OPEN_CFG,
			"error opening configuration file: %s\n"), cfgFileName);
		return(-1);
	}

	while (!feof(pFin)) {
		if (!fgets(linein, sizeof(linein)-1, pFin)) {
			continue;
		}
		lineno++;
		if (linein[0] == COMMENT_CHAR)  /* if comment line */
			continue;
		if (linein[0] != ':')		/* zone entries start with ':' */
			continue;						
		strcpy(buf,linein);

		pc1 = strtok(&buf[1],":");
		parmno=0;
		if (!bad)
			zoneno++;
		bad = FALSE;
		done = FALSE;

		if (pc1) do {			/* do loop start for current line */
			if (sscanf(pc1," %[^:\n]",buf1) != 1)	/* skip whitespace */
				break;
			switch(parmno) {
			case 0:			/* zone name */
				strcpy(curzone,buf1);
				/* chk length */
				if ((len = strlen(buf1)) > NBP_NVE_STR_SIZE) {
					sprintf(errbuf,  MSGSTR(M_ZONE_TOO_LONG,
						"zone name too long"));
					bad = TRUE;
					break;
				}
				/* chk for bad chars */
				if (pc2 = strpbrk(buf1, INVALID_ZIP_CHARS)) {
					sprintf(errbuf,  MSGSTR(M_IVAL_CHAR,
						"invalid character in zone name (%c)"),*pc2);
					bad = TRUE;
					break;
				}	
				/* check for dupe zones */
				for (i=0; !mh && i<zoneno; i++) {
					strcpy(tbuf1, buf1);
					strcpy(tbuf2, zonep[i].zone_name.str);
					/* compare same case */
					for (j=0; j<NBP_NVE_STR_SIZE; j++) {
						if (!tbuf1[j]) break;
						tbuf1[j] = toupper(tbuf1[j] );
						tbuf2[j] = toupper(tbuf2[j] );
					}
					if (!strcmp(tbuf1,tbuf2)) {				
						sprintf(errbuf,  MSGSTR(M_DUPE_ZONE,
							"duplicate zone entry"));
						bad = TRUE;
						break;
					}
				}
				/* save zone name */
				strcpy(zonep[zoneno].zone_name.str,buf1);
				zonep[zoneno].zone_name.len = len;
				break;
			default:	/* I/F's or home designation */
				if (buf1[0] == '*') {
					if (home++) {
						sprintf(errbuf, MSGSTR(M_DUPE_HOME,
							"home already designated on line %d"),homeLine);
						bad = TRUE;
						break;
					}
					homeLine = lineno;

					/* home zone must exist on home I/F */
					if ( gotHomePort && 
						( !(zonep[zoneno].zone_ifs[ifType] & 
						  1<<filecfgp->home_number))) {
						sprintf(errbuf, MSGSTR(M_HOME_MUST,
							"Zone designated as 'home' must contain\n"\
								       "the home I/F (%s)"),homePort);
						bad = TRUE;
						break;
					}
					done = TRUE;
					zonep[zoneno].zone_home = TRUE;
				}
				else {		/* not home, must be another I/F */
					/* check for valid type and make
					   sure it has been defined too */
					isseed = TRUE;				
					if ((ifType = getIFType(buf1)) != -1     &&
					    filecfgp->avail[ifType] & 1<<IF_NO(buf1) &&
					    (mh || (isseed = (seed[ifType] & 1<<IF_NO(buf1)))) 
					   ) {
						zonep[zoneno].zone_ifs[ifType] |= 1<<IF_NO(buf1);
					} else {
						if (mh)
							/* zone is from previoiusly 
							   used I/F, just ignore it */
							continue;
						if (isseed)
							sprintf(errbuf,  
								MSGSTR(M_INVAL_IF,
								"invalid interface (%s)"), buf1);
						else
							sprintf(errbuf, 
								MSGSTR(M_ALL_IFS_ASSOC,
								"all interfaces associated with a "\
							    "zone must be seed type\n"\
							    "interface for this zone is non-seed (%s)"),
								buf1);
						bad = TRUE;
						break;
					}
				}
			} /* end switch */
			parmno++;

			if (bad) {
				/* finish error msg */
				if(checkCfg) {
					sscanf(pc1,":%[^:\n]",linein);
					fprintf(stderr, MSGSTR(M_EZONE,
						"error, %s\nzone:%s\n"),errbuf,curzone);	
				}
				else
					fprintf(stderr, MSGSTR(M_LINE,
						"error, line %d. %s\n%s\n"),lineno,errbuf,linein);	
				error = TRUE;
				break;
			}
	
		} while(!done && pc1 && (pc1  = strtok(NULL,":")));
	}
	/* if no home zone set and home port is seed type, check to
	   see if there is only one zone assigned to that i/f. If
	   so, then make it the home zone
	*/
	if ( !mh && gotHomePort && !home && 
		(seed[filecfgp->home_type] & 1<<filecfgp->home_number)) {
		j = -1; 	/* j will be set to home zone */
		for (i=0; i<MAX_ZONES; i++) {
			if (!zonep[i].zone_name.len)
				break;
			if (zonep[i].zone_ifs[filecfgp->home_type] & 
				1<<filecfgp->home_number)
				if (j >=0 )	{
				    home = 0;	/* more than one zone assigned to home
						   port, we can't assume correct one */
				    break;
				}
				else {
				    home = 1;
				    j = i;	/* save 1st zone found */
				}
		}
		if (home) 
			zonep[j].zone_home = TRUE;
		else {
			error = TRUE; 
			fprintf(stderr, MSGSTR(M_NO_HOME_ZN,
				"error, no home zone designated\n")); 
		}
	}
	fclose(pFin);
	return(error? -1:0);
} /* getZoneInfo */

int getIFType(ifname)
     char *ifname;

/* checks  the interface for a valid type, returns the if Type number
   as defined by IF_TYPENO_xx 
*/
{
	int ifType;
	int i;

	for (ifType=0; ifType<IF_TYPENO_CNT; ifType++) {
	 	if (!strncmp(if_types[ifType], ifname, strlen(if_types[ifType]))) {
		  /* a valid type, check unit number */
			if (sscanf(ifname+strlen(if_types[ifType]),"%d",&i) == 1)
				return(ifType);
			else
				return(-1);
		} else
			continue;
	}
	return(-1);
} /* getIFType */

int ifcompare(v1,v2)
void *v1, *v2;
{
	return(strcmp(((at_if_cfg_t *)v1)->ifr_name, ((at_if_cfg_t *)v2)->ifr_name));
}

void showCfg(cfgp, if_zones)
     at_if_cfg_t *cfgp;
     if_zone_info_t if_zones[];         /* zone info from cfg file;
					   not used in multihoming mode	*/
{
	int i,j,k, cnt=0, seed=FALSE;
	char range[40];
	qsort((void*)cfgp, IF_TOTAL_MAX, sizeof(*cfgp), ifcompare);
	for (i=0; i<IF_TOTAL_MAX; i++) {
		if (cfgp[i].ifr_name[0]) {
			if (!cnt++) {
				fprintf(STDOUT, 
					MSGSTR(M_RTR_CFG,"Router mode configuration:\n\n"));
				fprintf(STDOUT,
					MSGSTR(M_RTR_CFG_HDR,"H I/F  Network Range\n"));
				fprintf(STDOUT,"- ---  -------------\n");
			}
			range[0] = '\0';
			if (cfgp[i].netStart || cfgp[i].netEnd) {
				sprintf(range,"%5d - %d", cfgp[i].netStart, cfgp[i].netEnd); 
				seed=TRUE;
			}
			fprintf(STDOUT,"%c %-3s  %s\n", cfgp[i].flags & ELAP_CFG_HOME ? '*' : ' ',
				cfgp[i].ifr_name,
				range[0] ? range : MSGSTR(M_NONSEED,"(non-seed)"));
		}
	}
	if (!cnt) {
		fprintf(STDOUT, 
			MSGSTR(M_NO_IF_CFG, 
			"There are no interfaces configured\n"));
		return;
	}
	if (seed) {
		fprintf(STDOUT, "\n\n  %-32s  %s\n", 
			MSGSTR(M_ZONE_DEF,"Defined zones"),
			MSGSTR(M_IF_DEF_ZONE,"Interfaces Defining Zone"));
			fprintf(STDOUT,"  %-32s  %s\n",
    					 	   "-------------",
					   "------------------------");
	}

	for (i=0; i<MAX_ZONES && seed; i++)  {
		int ifcnt=0;
		if (!if_zones[i].zone_name.str[0])
			break;
		fprintf(STDOUT, "%c %-32s  ", if_zones[i].zone_home ? '*' : ' ',
			if_zones[i].zone_name.str);
		for (j=0; j<IF_TYPENO_CNT; j++) {
			for (k=0; k<IF_TOTAL_MAX; k++)
				if (if_zones[i].zone_ifs[j] &1<<k) {
					if (ifcnt && !((ifcnt)%ZONE_IFS_PER_LINE))
						fprintf(STDOUT,"\n%36s","");
					ifcnt++;
					fprintf(STDOUT, "%s%c ",if_types[j],
						   k<=9 ? '0' + k : 'a' + k);
				}
			}
		fprintf(STDOUT, "\n");
	}
	fprintf(STDOUT, 
		MSGSTR(M_HOME_Z_IND,
		"\n* indicates home port and home zone\n"\
  	      "  (if home port is a seed port)\n"));
		
}


int checkSeeds(elapcfgp,zonep)
     at_if_cfg_t elapcfgp[];
     if_zone_info_t zonep[];
{
	int i,j;
	int	ifType;
	int	unitNo;
	int	ok;
	int	error=FALSE;

	for (i=0; i<IF_TOTAL_MAX; i++) {
		if (!elapcfgp[i].ifr_name[0])	
			break;
		if (!elapcfgp[i].netStart)		/* if no seed, skip */
			continue;
	 	ifType = getIFType(elapcfgp[i].ifr_name);
		unitNo = IF_NO(elapcfgp[i].ifr_name);
		ok = FALSE;
		for (j=0;j<MAX_ZONES; j++) {
			if (!zonep[j].zone_name.str[0])
				break;
			if (zonep[j].zone_ifs[ifType] & 1<<unitNo) {
				ok = TRUE;
				break;
			}
		} /* for each zone */
		if (!ok) {
			fprintf(stderr, MSGSTR(M_SEED_WO_ZONE,
				"error, seed I/F without any zones: %s\n"),elapcfgp[i].ifr_name);
			error = -1;
		}
	}     /* for I/F type */
	return(error);
} /* checkSeeds */

int
showZones()
{
	int status;
	int size;
	int if_id;
	int done = FALSE;
	ZT_entryno zte;
	int did_header=0;

	if ((if_id = socket(AF_APPLETALK, SOCK_RAW, 0)) < 0)
		return(-1);

	*(int *)&zte = 1;
	while (1) {
		size = sizeof(ZT_entryno);
		status = at_send_to_dev(if_id, LAP_IOC_GET_ZONE, &zte, &size);
		if (status <0)
			switch (errno) {
				case EINVAL:
					done = TRUE;
					break;
				case 0:
					break;
				default:
					fprintf(stderr, MSGSTR(M_RET_ZONES,
						"showZones: error retrieving zone list\n"));
					goto error;
			}
		if (done)
			break;
		
		if (!did_header++) {
			fprintf(STDOUT, MSGSTR(M_ZONES,"..... Zones ......\n"));
			fprintf(STDOUT, MSGSTR(M_ZONE_HDR,"zno zcnt zone\n"));
		}
		zte.zt.Zone.str[zte.zt.Zone.len] = '\0';
		fprintf(STDOUT, "%3d  %3d %s\n", zte.entryno+1,zte.zt.ZoneCount, zte.zt.Zone.str);
		*(int *)&zte = 0;
	}
	if (*(int *)&zte == 1)
		fprintf(STDOUT, MSGSTR(M_NO_ZONES,"no zones found\n"));
	(void) close(if_id);
	return(0);
error:
	(void) close(if_id);
	return(-1);
}

int
showRoutes()
{
	int status;
	int size;
	int if_id;
	int i,j;
	int zcnt,gap;
	int done = FALSE;
	int did_header = FALSE;
	RT_entry rt;
	char state[10];

	if ((if_id = socket(AF_APPLETALK, SOCK_RAW, 0)) < 0)
		return(-1);

	*(int *)&rt = 1;
	state[9] = '\0';
	while (1) {
		size = sizeof(RT_entry);
		status = at_send_to_dev(if_id, LAP_IOC_GET_ROUTE, &rt, &size);
		if (status < 0)
			switch (errno) {
				case EINVAL:
					done = TRUE;
					break;
				case 0:
					break;
				default:
					fprintf(STDOUT, MSGSTR(M_RET_ROUTES,
						"showRoutes: error retrieving route table\n"));
					goto error;
			}
		if (done)
			break;
		if (!did_header++)	{
			fprintf(STDOUT, MSGSTR(M_ROUTES,
				"............ Routes ................\n"));
			fprintf(STDOUT, MSGSTR(M_NXT_STATE,
				"                next             state\n"));
			fprintf(STDOUT, MSGSTR(M_RTR_HDR,
				"start-stop    net:node   d  p  PBTZ GSBU zones\n"));
		}
		gap = 0;
		for (i=0; i<8; i++) {
			if (i==4) {
				gap =1; 
				state[4] = ' ';
			}
			state[i+gap] =  (rt.EntryState & 1<<(7-i)) ? '1' : '0';
		}
		fprintf(STDOUT, "%5d-%-5d %5d:%-05d %2d %2d  %s ", 
			rt.NetStart, rt.NetStop, rt.NextIRNet, rt.NextIRNode, 
			rt.NetDist, rt.NetPort, state);
		zcnt = 0;
		for (i=0; i<ZT_BYTES; i++) {
			for (j=0; j<8; j++)
				if ((rt.ZoneBitMap[i] <<j) & 0x80) {
					if (zcnt >= Z_MAX_PRINT) { 
						fprintf(STDOUT, MSGSTR(M_MORE,",more..."));
						i = ZT_BYTES;
						break;
					}
					fprintf(STDOUT, zcnt ? ",%d" : " %d",i*8+j+1);	/* 1st zone is 1 not 0 */
					zcnt++;
				}
		}
		fprintf(STDOUT, "\n");
		*(int *)&rt = 0;
	}
	if (*(int *)&rt == 1)
		fprintf(STDOUT, MSGSTR(M_NO_ROUTES,"no routes found\n"));
	(void) close(if_id);
	return(0);
error:
	(void) close(if_id);
	return(-1);
}

/*
 * Name: atalkState()
 */
int atalkState(flag, pid_buf)
	char flag;
	char **pid_buf;
{
	int if_id, rc;
	int size = 1;
	int i, j, k, pid;

	*pid_buf = 0;
	*((char *)&stateBuff[0]) = flag;

	if ((if_id = socket(AF_APPLETALK, SOCK_RAW, 0)) < 0)
		return(-1);

	rc = at_send_to_dev(if_id, LAP_IOC_CHECK_STATE, stateBuff, &size);
	close(if_id);
	size = size/sizeof(int);
	if (size >= 1) {
		stateBuff[size] = 0;
		for (i=0, j=0; i < size; i++) {
			pid = stateBuff[i];
			for (k=0; k < j; k++) {
				if (pid == pidBuff[k]) {
					pid = 0;
					break;
				}
			}
			if (pid)
				pidBuff[j++] = pid;
		}
		pidBuff[j] = 0;
		*pid_buf = (char *)pidBuff;
		SET_ERRNO(EBUSY);
	}
	return rc;
}

void aurpd(fd)
	int fd;
{
	ATgetmsg(fd, 0, 0, 0);
}
