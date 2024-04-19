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
static const char rcsid[] = "$Id: getkey.c,v 1.1.1.1 1999/10/04 22:25:53 wsanchez Exp $";
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

/* Test scaffolding - a program to test getkeybyname.
 *
 * TODO:  
 *	Use getopt.
 * 	Add -a to insist on authoritative data.
 *	Add -s to insist on signed data.
 *	Add option for binary output?  Or ascii hex?
 *	Add options to specify key types (user, host, ipsec, auth, conf).
 */
void
main(argc, argv)
	int argc;
	char **argv;
{
	/* FIXME -- update resolver(3) man page for return codes! */
#define	NUMKEYS	20	
	struct dnskey akey[NUMKEYS];
	char print_key[2048];
	int sult, sult2, i;

	bzero((char *)akey, sizeof(akey));
	akey->dname = argv[1];
	if (argc > 2) akey->protocol  = atoi(argv[2]);
	if (argc > 3) akey->algorithm = atoi(argv[3]);
	if (argc > 4) akey->keyflags  = atoi(argv[4]);

	sult = getkeybyname(akey, NUMKEYS, DNSKEY_NO_OPTIONS);
	printf("getkeybyname(%s) => %d\n", argv[1], sult);
	printf("h_errno = %d\n", h_errno);
	for (i = 0; i < sult; i++) {
		printf("\n");
		printf("\tdname = %s\n", akey[i].dname);
		printf("\tttl = %lu\n", akey[i].ttl);
		printf("\tflags = 0x%x\n", akey[i].keyflags);
		printf("\tprotocol = %u\n", akey[i].protocol);
		printf("\talgorithm = %u\n", akey[i].algorithm);
		printf("\tkeylen = %u\n", akey[i].keylen);
#if 0		
		sult2 = b64_ntop(akey[i].key, akey[i].keylen, print_key,
				 sizeof(print_key));
#else
		sult2 = -1;
#endif
		if (sult2 < 0) {
			printf("\tkey = [base64 encoding failed]\n");
		} else {
			printf("\tkey = %s\n", print_key);
		}
	}
	exit(sult);
}
