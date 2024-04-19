/* $Header: /cvs/Darwin/Services/bind/bind/contrib/dns_signer/parse/bind_support.c,v 1.1.1.1 1999/10/04 22:25:52 wsanchez Exp $ */
#include <stdio.h>

#include <sys/types.h>
#include <arpa/nameser.h>

#ifndef _NETINET_IN_H_
#include <netinet/in.h>
#ifdef FreeBSD
/* The following should (have) be(en) defined in /usr/include/netinet/in.h */
struct in6_addr
{
	u_int8_t	s6_addr[16];
};
#endif
#endif

#include <arpa/nameser.h>
#include <resolv.h>
#include <string.h>
#include <netdb.h>
#include "bind_support.h"

/*
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
*/
#ifndef TRUE
#define TRUE	1
#endif

#ifndef FALSE
#define FALSE	0
#endif

void print_response (u_int8_t *ans, int resplen)
{ 
	/* fp_nquery is a resolver debug routine (I think), the rest
		would dump the response in byte form, formatted to match
		the query's structure */
	fp_nquery(ans, resplen, stdout);
}

int dns_lookup (const char *name, u_int32_t class, u_int16_t type,
				u_int8_t* answer, int anslen)
{
	u_char buf[1024];
	int n;

	if ((_res.options & RES_INIT) == 0 && res_init() == -1)
	{
		h_errno = NETDB_INTERNAL;
		return (-1);
	}

	n = res_mkquery(QUERY, name, class, type, NULL, 0, NULL,
			buf, sizeof(buf));
	if (n <= 0)
	{
		h_errno = NO_RECOVERY;
		return (n);
	}
	n = res_send(buf, n, answer, anslen);
	if (n < 0)
	{
		h_errno = TRY_AGAIN;
		return (n);
	}

	return (n);
}

int	wire_to_ascii_name (char *name, u_int8_t *wire, int name_length)
{
	int ret_val;
	memset (name, 0, name_length);
	ret_val = ns_name_ntop (wire, name, name_length-1);
	if (name[strlen(name)-1]!='.')
		strcat (name, ".");
	else
		ret_val--;
	return ret_val;
}

/* Portions Copyright (c) 1995,1996,1997,1998 by Trusted Information Systems, Inc.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND TRUSTED INFORMATION SYSTEMS DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL TRUSTED INFORMATION 
 * SYSTEMS BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 *
 *
 * Trusted Information Systems, Inc. has received approval from the
 * United States Government for export and reexport of TIS/DNSSEC
 * software from the United States of America under the provisions of
 * the Export Administration Regulations (EAR) General Software Note
 * (GSN) license exception for mass market software.  Under the
 * provisions of this license, this software may be exported or
 * reexported to all destinations except for the embargoed countries of
 * Cuba, Iran, Iraq, Libya, North Korea, Sudan and Syria.  Any export
 * or reexport of TIS/DNSSEC software to the embargoed countries
 * requires additional, specific licensing approval from the United
 * States Government. 
 */




