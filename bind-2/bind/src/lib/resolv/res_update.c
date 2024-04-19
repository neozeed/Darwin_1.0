#if !defined(lint) && !defined(SABER)
static char rcsid[] = "$Id: res_update.c,v 1.1.1.1 1999/10/04 22:24:50 wsanchez Exp $";
#endif /* not lint */

/*
 * Copyright (c) 1996-1999 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

/*
 * Based on the Dynamic DNS reference implementation by Viraj Bais
 * <viraj_bais@ccm.fm.intel.com>
 */

#include "port_before.h"

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/time.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>

#include <errno.h>
#include <limits.h>
#include <netdb.h>
#include <resolv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "port_after.h"

/* Options. Leave them on. */
#define DEBUG
#include "res_debug.h"

/*
 * Separate a linked list of records into groups so that all records
 * in a group will belong to a single zone on the nameserver.
 * Create a dynamic update packet for each zone and send it to the
 * nameservers for that zone, and await answer.
 * Abort if error occurs in updating any zone.
 * Return the number of zones updated on success, < 0 on error.
 *
 * On error, caller must deal with the unsynchronized zones
 * eg. an A record might have been successfully added to the forward
 * zone but the corresponding PTR record would be missing if error
 * was encountered while updating the reverse zone.
 */

#define NSMAX 16

struct ns1 {
	char nsname[MAXDNAME];
	struct in_addr nsaddr1;
};

struct zonegrp {
	char 		z_origin[MAXDNAME];
	int16_t		z_class;
	char		z_soardata[MAXDNAME + 5 * INT32SZ];
	struct ns1 	z_ns[NSMAX];
	int		z_nscount;
	ns_updrec *	z_rr;
	struct zonegrp *z_next;
};

static char *		skip_label(char *);
static void		canon(char *);

int
res_nupdate(res_state statp, ns_updrec *rrecp_in) {
	ns_updrec *rrecp, *tmprrecp;
	u_char buf[PACKETSZ], answer[PACKETSZ], packet[2*PACKETSZ];
	char name[MAXDNAME], zname[MAXDNAME], primary[MAXDNAME],
	     mailaddr[MAXDNAME];
	u_char soardata[2*MAXCDNAME+5*INT32SZ];
	char *dname, *svdname, *cp1, *target;
	u_char *cp, *eom;
	HEADER *hp = (HEADER *) answer;
	struct zonegrp *zptr = NULL, *tmpzptr, *prevzptr, *zgrp_start = NULL;
	int i, j, k = 0, n, ancount, nscount, arcount, rcode, rdatasize,
	    newgroup, done, myzone, seen_before, numzones = 0;
	u_int16_t dlen, class, qclass, type, qtype;
	u_int32_t ttl;

	for (rrecp = rrecp_in; rrecp; rrecp = rrecp->r_next) {
		dname = rrecp->r_dname;
		canon(dname);
		qtype = T_SOA;
		qclass = rrecp->r_class;
		done = 0;
		seen_before = 0;

		while (!done && dname) {
		    if (qtype == T_SOA) {
			for (tmpzptr = zgrp_start;
			     tmpzptr && !seen_before;
			     tmpzptr = tmpzptr->z_next) {
				if (strcasecmp(dname,
					       tmpzptr->z_origin) == 0 &&
				    tmpzptr->z_class == qclass)
					seen_before++;
				for (tmprrecp = tmpzptr->z_rr;
				     tmprrecp && !seen_before;
				     tmprrecp = tmprrecp->r_grpnext)
				if (strcasecmp(dname, tmprrecp->r_dname) == 0
				    && tmprrecp->r_class == qclass) {
					seen_before++;
					break;
				}
				if (seen_before) {
					/*
					 * Append to the end of
					 * current group.
					 */
					for (tmprrecp = tmpzptr->z_rr;
					     tmprrecp->r_grpnext;
					     tmprrecp = tmprrecp->r_grpnext)
						(void)NULL;
					tmprrecp->r_grpnext = rrecp;
					rrecp->r_grpnext = NULL;
					done = 1;
					break;
				}
			}
		} else if (qtype == T_A) {
		    for (tmpzptr = zgrp_start;
			 tmpzptr && !done;
			 tmpzptr = tmpzptr->z_next)
			    for (i = 0; i < tmpzptr->z_nscount; i++)
				if (tmpzptr->z_class == qclass &&
				    strcasecmp(tmpzptr->z_ns[i].nsname,
					       dname) == 0 &&
				    tmpzptr->z_ns[i].nsaddr1.s_addr != 0) {
					zptr->z_ns[k].nsaddr1.s_addr =
					 tmpzptr->z_ns[i].nsaddr1.s_addr;
					done = 1;
					break;
				}
		}
		if (done)
		    break;

#ifdef notyet
		oldoptions = statp->options
		statp->options &= ~RES_RECURSE;
#endif
		n = res_nmkquery(statp, QUERY, dname, qclass, qtype, NULL,
				 0, NULL, buf, sizeof buf);
#ifdef notyet
		statp->options = oldoptions;
#endif
		if (n <= 0) {
 			Dprint(statp->options & RES_DEBUG,
			       (stderr, "res_update: mkquery failed\n"));
		    return (n);
		}
		n = res_nsend(statp, buf, n, answer, sizeof answer);
		if (n < 0) {
 			Dprint(statp->options & RES_DEBUG,
			       (stderr, "res_update: send error for %s\n",
				rrecp->r_dname));
			return (n);
		}
		if (n < HFIXEDSZ)
			return (-1);
		ancount = ntohs(hp->ancount);
		nscount = ntohs(hp->nscount);
		arcount = ntohs(hp->arcount);
		rcode = hp->rcode;
		cp = answer + HFIXEDSZ;
		eom = answer + n;
		/* skip the question section */
		n = dn_skipname(cp, eom);
		if (n < 0 || cp + n + 2 * INT16SZ > eom)
			return (-1);
		cp += n + 2 * INT16SZ;

		if (qtype == T_SOA) {
		    if (ancount == 0 && nscount == 0) {
			/*
			 * if (rcode == NOERROR) then the dname exists but
			 * has no soa record associated with it.
			 * if (rcode == NXDOMAIN) then the dname does not
			 * exist and the server is replying out of NCACHE.
			 * in either case, proceed with the next try
			 */
			dname = skip_label(dname);
			continue;
		    } else if ((rcode == NOERROR || rcode == NXDOMAIN) &&
			       ancount == 0 && nscount == 1) {
			/*
			 * name/data does not exist, soa record supplied in the
			 * authority section
			 */
			/* authority section must contain the soa record */
			if ((n = dn_expand(answer, eom, cp, zname,
					sizeof zname)) < 0)
			    return (n);
			cp += n;
			if (cp + 2 * INT16SZ > eom)
				return (-1);
			GETSHORT(type, cp);
			GETSHORT(class, cp);
			if (type != T_SOA || class != qclass) {
			    Dprint(statp->options & RES_DEBUG,
				   (stderr, "unknown answer\n"));
			    return (-1);
			}
			myzone = 0;
			svdname = dname;
			while (dname)
			    if (strcasecmp(dname, zname) == 0) {
				myzone = 1;
				break;
			    } else 
				dname = skip_label(dname);
			if (!myzone) {
			    dname = skip_label(svdname);
			    continue;
			}
			nscount = 0;
			/* fallthrough */
		    } else if ((rcode == NOERROR || rcode == NXDOMAIN)
			       && ancount > 0) {
			/*
			 * rcode == NOERROR:
			 * We potentially have a CNAME chain maybe followed
			 * by a SOA record.
			 * If we don't have a CNAME chain we will have a soa
			 * record.
			 * rcode == NXDOMAIN:
			 * We will have a CNAME chain.
			 */
			if ((n = dn_expand(answer, eom, cp, zname,
			 	       	   sizeof zname)) < 0)
				return (n);
			else
				cp += n;
			if (cp + 2 * INT16SZ > eom)
				return (-1);
			GETSHORT(type, cp);
			GETSHORT(class, cp);
			if (type == T_CNAME) {
				dname = skip_label(dname);
				continue;
			}
			if (strcasecmp(dname, zname) != 0 ||
			    type != T_SOA ||
			    class != rrecp->r_class) {
				Dprint(statp->options & RES_DEBUG,
				       (stderr, "unknown answer\n"));
				return (-1);
			}
			/* FALLTHROUGH */
		    } else {
			Dprint(statp->options & RES_DEBUG,
			       (stderr,
		       "unknown response: ans=%d, auth=%d, add=%d, rcode=%d\n",
				ancount, nscount, arcount, hp->rcode));
			return (-1);
		    }
		    if (cp + INT32SZ + INT16SZ > eom)
			    return (-1);
		    /* continue processing the soa record */
		    GETLONG(ttl, cp);
		    GETSHORT(dlen, cp);
		    if (cp + dlen > eom)
			    return (-1);
		    newgroup = 1;
		    zptr = zgrp_start;
		    prevzptr = NULL;
		    while (zptr) {
			if (strcasecmp(zname, zptr->z_origin) == 0 &&
			    type == T_SOA && class == qclass) {
				newgroup = 0;
				break;
			}
			prevzptr = zptr;
			zptr = zptr->z_next;
		    }
		    if (!newgroup) {
			for (tmprrecp = zptr->z_rr;
			     tmprrecp->r_grpnext;
			     tmprrecp = tmprrecp->r_grpnext)
				    ;
			tmprrecp->r_grpnext = rrecp;
			rrecp->r_grpnext = NULL;
			done = 1;
			cp += dlen;
			break;
		    } else {
			if ((n = dn_expand(answer, eom, cp, primary,
				       	   sizeof primary)) < 0)
			    return (n);
			cp += n;
			/* 
			 * We don't have to bounds check here because the
			 * next use of 'cp' is in dn_expand().
			 */
			cp1 = (char *)soardata;
			strcpy(cp1, primary);
			cp1 += strlen(cp1) + 1;
			if ((n = dn_expand(answer, eom, cp, mailaddr,
				       	   sizeof mailaddr)) < 0)
			    return (n);
			cp += n;
			strcpy(cp1, mailaddr);
			cp1 += strlen(cp1) + 1;
			if (cp + 5*INT32SZ > eom)
				return (-1);
			memcpy(cp1, cp, 5*INT32SZ);
			cp += 5*INT32SZ;
			cp1 += 5*INT32SZ;
			rdatasize = (u_char *)cp1 - soardata;
			zptr = calloc(1, sizeof(struct zonegrp));
			if (zptr == NULL)
                	    return (-1);
			if (zgrp_start == NULL)
			    zgrp_start = zptr;
			else
			    prevzptr->z_next = zptr;
			zptr->z_rr = rrecp;
			rrecp->r_grpnext = NULL;
			strcpy(zptr->z_origin, zname);
			zptr->z_class = class;
			memcpy(zptr->z_soardata, soardata, rdatasize);
			/* fallthrough to process NS and A records */
		    }
		} else if (qtype == T_NS) {
		    if (rcode == NOERROR && ancount > 0) {
			strcpy(zname, dname);
			for (zptr = zgrp_start; zptr; zptr = zptr->z_next) {
			    if (strcasecmp(zname, zptr->z_origin) == 0)
				break;
			}
			if (zptr == NULL)
			    /* should not happen */
			    return (-1);
			if (nscount > 0) {
			    /*
			     * answer and authority sections contain
			     * the same information, skip answer section
			     */
			    for (j = 0; j < ancount; j++) {
				n = dn_skipname(cp, eom);
				if (n < 0)
					return (-1);
				n += 2*INT16SZ + INT32SZ;
				if (cp + n + INT16SZ > eom)
					return (-1);
				cp += n;
				GETSHORT(dlen, cp);
				cp += dlen;
			    }
			} else
			    nscount = ancount;
			/* fallthrough to process NS and A records */
		    } else {
			Dprint(statp->options & RES_DEBUG,
			       (stderr,
    "cannot determine nameservers for %s: ans=%d, auth=%d, add=%d, rcode=%d\n",
				dname, ancount, nscount, arcount, hp->rcode));
			return (-1);
		    }
		} else if (qtype == T_A) {
		    if (rcode == NOERROR && ancount > 0) {
			arcount = ancount;
			ancount = nscount = 0;
			/* fallthrough to process A records */
		    } else {
			Dprint(statp->options & RES_DEBUG,
			       (stderr,
	"cannot determine address for %s: ans=%d, auth=%d, add=%d, rcode=%d\n",
				dname, ancount, nscount, arcount, hp->rcode));
			return (-1);
		    }
		}
		/* process NS records for the zone */
		j = 0;
		for (i = 0; i < nscount; i++) {
		    if ((n = dn_expand(answer, eom, cp, name,
					sizeof name)) < 0)
			return (n);
		    cp += n;
		    if (cp + 3 * INT16SZ + INT32SZ > eom)
			    return (-1);
		    GETSHORT(type, cp);
		    GETSHORT(class, cp);
		    GETLONG(ttl, cp);
		    GETSHORT(dlen, cp);
		    if (cp + dlen > eom)
			return (-1);
		    if (strcasecmp(name, zname) == 0 &&
			type == T_NS && class == qclass) {
				if ((n = dn_expand(answer, eom, cp,
						   name, sizeof name)) < 0)
					return (n);
			    target = zptr->z_ns[j++].nsname;
			    strcpy(target, name);
		    }
		    cp += dlen;
		}
		if (zptr->z_nscount == 0)
		    zptr->z_nscount = j;
		/* get addresses for the nameservers */
		for (i = 0; i < arcount; i++) {
		    if ((n = dn_expand(answer, eom, cp, name,
					sizeof name)) < 0)
			return (n);
		    cp += n;
		    if (cp + 3 * INT16SZ + INT32SZ > eom)
			return (-1);
		    GETSHORT(type, cp);
		    GETSHORT(class, cp);
		    GETLONG(ttl, cp);
		    GETSHORT(dlen, cp);
		    if (cp + dlen > eom)
			    return (-1);
		    if (type == T_A && dlen == INT32SZ && class == qclass) {
			for (j = 0; j < zptr->z_nscount; j++)
			    if (strcasecmp(name, zptr->z_ns[j].nsname) == 0) {
				memcpy(&zptr->z_ns[j].nsaddr1.s_addr, cp,
				       INT32SZ);
				break;
			    }
		    }
		    cp += dlen;
		}
		if (zptr->z_nscount == 0) {
		    dname = zname;
		    qtype = T_NS;
		    continue;
		}
		done = 1;
		for (k = 0; k < zptr->z_nscount; k++)
		    if (zptr->z_ns[k].nsaddr1.s_addr == 0) {
			done = 0;
			dname = zptr->z_ns[k].nsname;
			qtype = T_A;
		    }

 	    } /* while */
	}

	for (zptr = zgrp_start; zptr; zptr = zptr->z_next) {

		/* append zone section */
		rrecp = res_mkupdrec(ns_s_zn, zptr->z_origin,
				     zptr->z_class, ns_t_soa, 0);
		if (rrecp == NULL) {
			Dprint(statp->options & RES_DEBUG,
			       (stderr, "saverrec error\n"));
			fflush(stderr);
			return (-1);
		}
		rrecp->r_grpnext = zptr->z_rr;
		zptr->z_rr = rrecp;

		n = res_nmkupdate(statp, zptr->z_rr, packet, sizeof packet);
		if (n < 0) {
			Dprint(statp->options & RES_DEBUG,
			       (stderr, "res_mkupdate error\n"));
			fflush(stderr);
			return (-1);
		} else
			Dprint(statp->options & RES_DEBUG,
			       (stdout,
				"res_mkupdate: packet size = %d\n", n));

		/*
		 * Override the list of NS records from res_init() with
		 * the authoritative nameservers for the zone being updated.
		 * Sort primary to be the first in the list of nameservers.
		 */
		for (i = 0; i < zptr->z_nscount; i++) {
			if (strcasecmp(zptr->z_ns[i].nsname,
				       zptr->z_soardata) == 0) {
				struct in_addr tmpaddr;

				if (i != 0) {
					strcpy(zptr->z_ns[i].nsname,
					       zptr->z_ns[0].nsname);
					strcpy(zptr->z_ns[0].nsname,
					       zptr->z_soardata);
					tmpaddr = zptr->z_ns[i].nsaddr1;
					zptr->z_ns[i].nsaddr1 =
						zptr->z_ns[0].nsaddr1;
					zptr->z_ns[0].nsaddr1 = tmpaddr;
				}
				break;
			}
		}
		for (i = 0; i < MAXNS; i++) {
			struct sockaddr_in *sin = &statp->nsaddr_list[i];

			sin->sin_addr = zptr->z_ns[i].nsaddr1;
			sin->sin_family = AF_INET;
			sin->sin_port = htons(NAMESERVER_PORT);
		}
		statp->nscount = (zptr->z_nscount < MAXNS) ? 
					zptr->z_nscount : MAXNS;
		n = res_nsend(statp, packet, n, answer, sizeof(answer));
		if (n < 0) {
			Dprint(statp->options & RES_DEBUG,
			       (stderr, "res_send: send error, n=%d\n", n));
			break;
		} else if (hp->rcode == NOERROR)
			numzones++;
	}

	/* free malloc'ed memory */
	while(zgrp_start) {
		zptr = zgrp_start;
		zgrp_start = zgrp_start->z_next;
		res_freeupdrec(zptr->z_rr);  /* Zone section we allocated. */
		free((char *)zptr);
	}

	return (numzones);
}

/*
 * Private
 */

static char *
skip_label(char *dname) {
	int escaped = 0;

	/* At root? */
	if (*dname == '\0')
		return (NULL);

	while (*dname != '\0' && (escaped || *dname != '.')) {
		escaped = escaped ? 0 : *dname == '\\';
		dname++;
	}

	if (*dname == '.')
		return (dname + 1);
	/* root */
	return (dname);
}

static void
canon(char *dname) {
	int escaped = 0, last = 0;

	if (*dname == '\0')
		return;

	while (*dname != '\0') {
		last = escaped;
		escaped = escaped ? 0 : *dname == '\\';
		dname++;
	}

	if (!last && dname[-1] == '.')
		dname[-1] = '\0';
}
