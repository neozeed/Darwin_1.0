#if !defined(lint) && !defined(SABER)
static char rcsid[] = "$Id: res_sendupdate.c,v 1.1.1.1 1999/10/04 22:24:50 wsanchez Exp $";
#endif /* not lint */

/*
 * Copyright (c) 1996,1999 by Internet Software Consortium.
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

int
res_nsendupdate(res_state statp, ns_updrec *rrecp_in, ns_tsig_key *key,
		char *zname, struct in_addr addr) {
	ns_updrec *rrecp;
	u_char buf[PACKETSZ], answer[PACKETSZ], packet[2*PACKETSZ];
	int i, j, k = 0, n, ancount, nscount, arcount, rcode, rdatasize,
	    newgroup, done, myzone, seen_before, numzones = 0;
	struct sockaddr_in *sin;

	/* append zone section */
	rrecp = res_mkupdrec(ns_s_zn, zname, rrecp_in->r_class, ns_t_soa, 0);
	if (rrecp == NULL) {
		fprintf(stderr, "saverrec error\n");
		fflush(stderr);
		return (-1);
	}
	rrecp->r_grpnext = rrecp_in;
	rrecp_in = rrecp;

	n = res_nmkupdate(statp, rrecp_in, packet, sizeof packet);
	if (n < 0) {
		fprintf(stderr, "res_mkupdate error\n");
		fflush(stderr);
		return (-1);
	} else
		fprintf(stdout, "res_mkupdate: packet size = %d\n", n);

	sin = &statp->nsaddr_list[0];
	sin->sin_addr = addr;
	sin->sin_family = AF_INET;
	sin->sin_port = htons(NAMESERVER_PORT);

	statp->nscount = 1;
	if (key == NULL)
		n = res_nsend(statp, packet, n, answer, sizeof(answer));
	else
		n = res_nsendsigned(statp, packet, n, key,
					    answer, sizeof(answer));
	if (n < 0) {
		fprintf(stderr, "res_send: send error, n=%d\n", n);
		return (n);
	}

	return (0);
}
