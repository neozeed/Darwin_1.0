/*
 * Get public keys from the Domain Name System.
 * John Gilmore, 14 June 1996.
 */

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

#if defined(LIBC_SCCS) && !defined(lint)
static const char rcsid[] = "$Id: getkeyby.c,v 1.1.1.1 1999/10/04 22:25:53 wsanchez Exp $";
#endif

#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>

#include <stdio.h>
#include <netdb.h>
#include <resolv.h>
#include <ctype.h>
#include <errno.h>
#include <syslog.h>
#if defined(BSD) && (BSD >= 199306)
# include <stdlib.h>
# include <string.h>
# include <unistd.h>
#else
/* # include "../conf/portability.h" */
#endif

#ifndef DNSKEY_NO_OPTIONS
#include "getkeyby.h"
#endif


/*
 * Retrieve one or more keys from the Domain Name System.
 * 
 * Result is negative if an error occurs.  Error codes are in h_errno
 * and can be printed with XXX (FIXME).
 *
 * The flags, protocol, algorithm fields in the first struct passed in
 * must be zeroed or set before calling,
 * since they determine which of several possible keys should be returned.
 *
 * In each struct passed in, if the keylen field is nonzero, it's taken as
 * the length of a pre-allocated memory area pointed to by the key field.
 * Best is to zero the whole set of structures, then fill it in.
 */
int
getkeybyname(dk, nk, options)
	struct dnskey *dk;		/* dname to look up; results go here */
	unsigned nk;			/* Number of dnskey structs passed in */
	unsigned options;		/* Options for key resolution */
{
	u_char answer[1024];		/* FIXME - remalloc if needed. */
	int anscount;
	int proto, alg;
	unsigned int keyflags;
	int keylen;
	int res_len;
	struct dns_response *resp;
	struct dns_rr *rr;
	u_char *cp;
	unsigned result_count;
	char *wantname = dk->dname;
	unsigned wantflags;
	unsigned wantproto;
	unsigned wantalg;

	if (options != 0)		/* No options supported yet.  */
		return (-1);

	res_len = res_query(wantname, C_IN, T_KEY, answer, sizeof(answer));
	if (res_len < 0)
		return (res_len);

	/* Eventually change this to check for signatures, FIXME.  */
	resp = res_parse_response(answer, res_len);
	if (resp == 0) {
		h_errno = NO_RECOVERY /* CANT_PARSE_RESPONSE */;
		return (-1);
	}

	/*
	 * OK, we got some sort of result.  Parse our way through it,
	 * converting to local byte-order, and see if we got a key that
	 * matches the flags we want.
	 */
	wantflags = dk->keyflags;
	wantproto = dk->protocol;
	wantalg   = dk->algorithm;

	anscount = resp->ans_count;
	rr = resp->answers;
	result_count = 0;

	for (; anscount-- > 0; rr++) {
		/* Parse an individual RR */
		if (rr->type != T_KEY) 
			continue;			/* Not a key RR... */

		if (0 != strcmp(rr->name, wantname))
			continue;			/* Not our RR... */

		keylen = rr->rdlength - (INT16SZ + 1 + 1);
		if (keylen < 0)
			continue;			/* Invalid KEY RR */

		cp = rr->rdata;
		keyflags = _getshort(cp);
		cp += INT16SZ;
		proto = *cp++;
		alg = *cp++;

		/*
		 * Check key flags, proto, alg for compatability with
		 * caller's desires.  FIXME, is this good enough? 
		 */
		if (wantalg != 0 && wantalg != alg)
			continue;
		if (wantproto != 0 && wantproto != proto && proto != 255)
			continue;
		if (wantflags != 0 && (wantflags != (keyflags&wantflags)))
			continue;

		/* Make sure our caller has given us enough space. */
		if (++result_count > nk) {
			h_errno = NO_RECOVERY /* XXX too many for caller buf */;
			return (-1);
		}

		/* OK, this is it.  Return this key.  */
		dk->dname     = wantname;
		dk->ttl       = rr->ttl;
		dk->keyflags  = keyflags;
		dk->protocol  = proto;
		dk->algorithm = alg;

		/*
		 * If caller supplied a key buffer, ensure it's long enough.
		 * If not, allocate one.
		 */
		if (dk->key) {
			if (dk->keylen < keylen) {
				h_errno = NO_RECOVERY /* XXX */;
				return (-1);
			}
		} else {
			dk->key = (unsigned char *)malloc(keylen);
			if (dk->key == NULL) {
				h_errno = NO_RECOVERY /* XXXY */;
				return (-1);
			}
		}
		dk->keylen    = keylen;
		memcpy(dk->key, cp, keylen);
		dk++;
	}

	/*
	 * We may or may not have found a key that matched the caller's desires.
	 * Tell them how many we are returning.
	 */
	return (result_count);
}
