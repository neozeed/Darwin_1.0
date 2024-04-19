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
static const char rcsid[] = "$Id: parseresp.c,v 1.1.1.1 1999/10/04 22:25:53 wsanchez Exp $";
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

#if defined(BSD) && (BSD >= 199103) && defined(AF_INET6)
# include <string.h>
#else
/* # include "../conf/portability.h" */
#endif

#ifndef DNSKEY_NO_OPTIONS
#include "getkeyby.h"
#endif

extern int h_errno;

struct malloc_bundle {
	struct dns_response	dns_response[1];
	struct dns_rr		dns_rr_s[1];
};

/*
 * String space for expanded dnames in response.  FIXME, make sure we
 * check properly for overflow here, and write code to re-malloc and
 * do it again larger.
 */
#define	SCRATCH_FOR_STRINGS	112		/* 128 - malloc overhead */
#define	MAX_STRINGSPACE		1024*128	/* 128kbytes of names! */

/* 
 * Parse a raw DNS response into a dns_response struct.
 *
 * The result contains pointers into the answer argument; don't free
 * the "answer" memory before freeing the result of this function.
 */
struct dns_response *
__res_parse_response (answer, anslen)
	const u_char *answer;
	int anslen;
{
	register const HEADER *hp;
	register const u_char *cp;
	register int n;
	struct dns_response *result;
	int rrcount, stringlen;
	char *nextstring;
	int len;
	struct dns_rr *nextrr;
	struct malloc_bundle *temp;
	unsigned stringspace = SCRATCH_FOR_STRINGS;

	hp = (const HEADER *)answer;

moremem:
	/* Examine the query itself, first.  */
	cp = answer + HFIXEDSZ;
	if (ntohs(hp->qdcount) != 1) {		/* FIXME, handle >1 */
		h_errno = NO_RECOVERY;
		return (NULL);
	}

	/*
	 * Get memory for all the RR's in all sections at once.
	 * Also for the strings (FIXME realloc if too small)...
	 */
	rrcount = ntohs(hp->ancount) + ntohs(hp->nscount) + ntohs(hp->arcount);
	temp = (struct malloc_bundle *)
		malloc (sizeof(struct malloc_bundle) +
			rrcount * sizeof(struct dns_rr) + stringspace);
	result = temp->dns_response;
	nextrr = temp->dns_rr_s;
	nextstring = (char *)(nextrr + rrcount);	
	stringlen = stringspace;

	/* Fill in global fields in resulting dns_response struct */
	result->ans_count   = ntohs(hp->ancount);
	result->auth_count  = ntohs(hp->nscount);
	result->glue_count  = ntohs(hp->arcount);

	result->answers	    = nextrr;
	result->authorities = result->answers     + result->ans_count;
	result->glue	    = result->authorities + result->auth_count;

	result->msg = (char *)answer;
	result->eom = (char *)answer + anslen;

	/* Expand the query name and stash it in the result.  */
	n = dn_expand(result->msg, result->eom, cp, nextstring, stringlen);
	if (n < 0) {
		/* We can't tell a malformed DNS response from a
		   too-short string space.  Guess a few times,
		   then declare it an error.  */
		free((void *)result);
		if (stringspace < MAX_STRINGSPACE) {
			stringspace += stringspace;	/* Double it */
			goto moremem;
		} else {
			h_errno = NO_RECOVERY;	/* Bad query name */
			return (NULL);
		}
	}
	result->queryname = nextstring;
	len = strlen(nextstring)+1;
	nextstring += len;
	stringlen  -= len;
	cp += n + QFIXEDSZ;

	/* Now examine all the answers.  */

	while (rrcount-- > 0 && cp < (u_char *)result->eom) {

		/* For later dn_expands, each RR points to the whole msg.  */
		nextrr->response = result;

		n = dn_expand(result->msg, result->eom, cp,
			      nextstring, stringlen);
		if (n < 0) {
			/* We can't tell a malformed DNS response from a
			   too-short string space.  Guess a few times,
			   then declare it an error.  */
			free((void *)result);
			if (stringspace < MAX_STRINGSPACE) {
				stringspace += stringspace;	/* Double it */
				goto moremem;
			} else {
				h_errno = NO_RECOVERY;	/* Bad RR name */
				return (NULL);
			}
		}
		nextrr->name = nextstring;
		len = strlen(nextstring)+1;
		nextstring += len;
		stringlen  -= len;
		cp += n;			/* name */

		nextrr->type = _getshort(cp);
 		cp += INT16SZ;			/* type */

		nextrr->class = _getshort(cp);
 		cp += INT16SZ;			/* class */

		nextrr->ttl = _getlong(cp);
 		cp += INT32SZ;			/* TTL */

		nextrr->rdlength = _getshort(cp);
		cp += INT16SZ;			/* len */

		/* Caller gets to interpret and dn_expand the rdata's... */
		nextrr->rdata = (u_char *)cp;

		/* Go to next RR */
		cp += nextrr->rdlength;
		nextrr++;
	}

	/* Check that all RR's got fully parsed.  */
	if (rrcount != -1 || cp != result->eom) {
		free((void *)result);
		h_errno = NO_RECOVERY;	/* Malformed DNS response */
		return (NULL);
	}

	/* If we knew realloc wouldn't move our block, we could shorten it
	   to give back the unused string space.  FIXME?  */
	return result;
}


#if 0
/*
 * Sample code for handling res_parse_response rdata fields.
 * FIXME: move this to the documentation!
 */

		switch (rr->type) {
		case T_PTR:
			n = dn_expand(rr->response->msg, rr->response->eom,
				      rr->rdata, bp, buflen);
			if (n < 0) {
				had_error++;
				break;
			}
			if (n != rr->rdlength)
				abort();	/* PTR name with other stuff */
			break;
		}
#endif /* 0 */
