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
/*
 * Copyright (c) 1994, 1995, 1996
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code distributions
 * retain the above copyright notice and this paragraph in its entirety, (2)
 * distributions including binary code include the above copyright notice and
 * this paragraph in its entirety in the documentation or other materials
 * provided with the distribution, and (3) all advertising materials mentioning
 * features or use of this software display the following acknowledgement:
 * ``This product includes software developed by the University of California,
 * Lawrence Berkeley Laboratory and its contributors.'' Neither the name of
 * the University nor the names of its contributors may be used to endorse
 * or promote products derived from this software without specific prior
 * written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Format and print Novell IPX packets.
 * Contributed by Brad Parker (brad@fcr.com).
 */

#ifndef lint
static const char rcsid[] =
    "@(#) $Header: /cvs/Darwin/Commands/NeXT/network_cmds/tcpdump.tproj/print-ipx.c,v 1.1.1.1 1999/05/02 03:58:33 wsanchez Exp $";
#endif

#include <sys/param.h>
#include <sys/time.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>
#include <netinet/tcp.h>
#include <netinet/tcpip.h>

#ifdef __STDC__
#include <stdlib.h>
#endif
#include <stdio.h>
#include <string.h>

#include "interface.h"
#include "addrtoname.h"
#include "ipx.h"
#include "extract.h"


static const char *ipxaddr_string(u_int32_t, const u_char *);
void ipx_decode(const struct ipxHdr *, const u_char *, u_int);
void ipx_sap_print(const u_short *, u_int);
void ipx_rip_print(const u_short *, u_int);

/*
 * Print IPX datagram packets.
 */
void
ipx_print(const u_char *p, u_int length)
{
	const struct ipxHdr *ipx = (const struct ipxHdr *)p;

	TCHECK(ipx->srcSkt);
	(void)printf("%s.%x > ",
		     ipxaddr_string(EXTRACT_32BITS(ipx->srcNet), ipx->srcNode),
		     EXTRACT_16BITS(&ipx->srcSkt));

	(void)printf("%s.%x:",
		     ipxaddr_string(EXTRACT_32BITS(ipx->dstNet), ipx->dstNode),
		     EXTRACT_16BITS(&ipx->dstSkt));

	/* take length from ipx header */
	TCHECK(ipx->length);
	length = EXTRACT_16BITS(&ipx->length);

	ipx_decode(ipx, (u_char *)ipx + ipxSize, length - ipxSize);
	return;
trunc:
	printf("[|ipx %d]", length);
}

static const char *
ipxaddr_string(u_int32_t net, const u_char *node)
{
    static char line[256];

    sprintf(line, "%x.%02x:%02x:%02x:%02x:%02x:%02x",
	    net, node[0], node[1], node[2], node[3], node[4], node[5]);

    return line;
}

void
ipx_decode(const struct ipxHdr *ipx, const u_char *datap, u_int length)
{
    register u_short dstSkt;

    dstSkt = EXTRACT_16BITS(&ipx->dstSkt);
    switch (dstSkt) {
      case IPX_SKT_NCP:
	(void)printf(" ipx-ncp %d", length);
	break;
      case IPX_SKT_SAP:
	ipx_sap_print((u_short *)datap, length);
	break;
      case IPX_SKT_RIP:
	ipx_rip_print((u_short *)datap, length);
	break;
      case IPX_SKT_NETBIOS:
	(void)printf(" ipx-netbios %d", length);
	break;
      case IPX_SKT_DIAGNOSTICS:
	(void)printf(" ipx-diags %d", length);
	break;
      default:
	(void)printf(" ipx-#%x %d", dstSkt, length);
	break;
    }
}

void
ipx_sap_print(const u_short *ipx, u_int length)
{
    int command, i;

    TCHECK(ipx[0]);
    command = EXTRACT_16BITS(ipx);
    ipx++;
    length -= 2;

    switch (command) {
      case 1:
      case 3:
	if (command == 1)
	    (void)printf("ipx-sap-req");
	else
	    (void)printf("ipx-sap-nearest-req");

	if (length > 0) {
	    TCHECK(ipx[1]);
	    (void)printf(" %x '", EXTRACT_16BITS(&ipx[0]));
	    fn_print((u_char *)&ipx[1], (u_char *)&ipx[1] + 48);
	    putchar('\'');
	}
	break;

      case 2:
      case 4:
	if (command == 2)
	    (void)printf("ipx-sap-resp");
	else
	    (void)printf("ipx-sap-nearest-resp");

	for (i = 0; i < 8 && length > 0; i++) {
	    TCHECK2(ipx[27], 1);
	    (void)printf(" %x '", EXTRACT_16BITS(&ipx[0]));
	    fn_print((u_char *)&ipx[1], (u_char *)&ipx[1] + 48);
	    printf("' addr %s",
		ipxaddr_string(EXTRACT_32BITS(&ipx[25]), (u_char *)&ipx[27]));
	    ipx += 32;
	    length -= 64;
	}
	break;
      default:
	    (void)printf("ipx-sap-?%x", command);
	break;
    }
	return;
trunc:
	printf("[|ipx %d]", length);
}

void
ipx_rip_print(const u_short *ipx, u_int length)
{
    int command, i;

    TCHECK(ipx[0]);
    command = EXTRACT_16BITS(ipx);
    ipx++;
    length -= 2;

    switch (command) {
      case 1:
	(void)printf("ipx-rip-req");
	if (length > 0) {
	    TCHECK(ipx[3]);
	    (void)printf(" %u/%d.%d", EXTRACT_32BITS(&ipx[0]),
			 EXTRACT_16BITS(&ipx[2]), EXTRACT_16BITS(&ipx[3]));
	}
	break;
      case 2:
	(void)printf("ipx-rip-resp");
	for (i = 0; i < 50 && length > 0; i++) {
	    TCHECK(ipx[3]);
	    (void)printf(" %u/%d.%d", EXTRACT_32BITS(&ipx[0]),
			 EXTRACT_16BITS(&ipx[2]), EXTRACT_16BITS(&ipx[3]));

	    ipx += 4;
	    length -= 8;
	}
	break;
      default:
	    (void)printf("ipx-rip-?%x", command);
    }
	return;
trunc:
	printf("[|ipx %d]", length);
}

