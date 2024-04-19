/*
 * Copyright (c) 1992, 1993, 1994, 1995, 1996
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
 */
#ifndef lint
char copyright[] =
    "@(#) Copyright (c) 1992, 1993, 1994, 1995, 1996\n\
The Regents of the University of California.  All rights reserved.\n";
static char rcsid[] =
    "@(#) $Header: /cvs/Darwin/Services/bind/bind/contrib/nslint-1.5.1/nslint.c,v 1.1.1.1 1999/10/04 22:25:55 wsanchez Exp $ (LBL)";
#endif
/*
 * nslint - perform consistency checks on dns files
 */

#include <sys/types.h>

#include <netinet/in.h>

#include <arpa/inet.h>

#include <ctype.h>
#include <errno.h>
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "gnuc.h"
#ifdef HAVE_OS_PROTO_H
#include "os-proto.h"
#endif

#define NSLINTBOOT "nslint.boot"	/* default nslint.boot file */

/* item struct */
struct item {
	char	*host;		/* pointer to hostname */
	u_long	addr;		/* ip address */
	int	records;	/* resource records seen */
	int	flags;		/* flags word */
};

/* Resource records seen */
#define REC_A		0x0001
#define REC_PTR		0x0002
#define REC_WKS		0x0004
#define REC_HINFO	0x0008
#define REC_MX		0x0010
#define REC_CNAME	0x0020
#define REC_NS		0x0040
#define REC_SOA		0x0080
#define REC_RP		0x0100
#define REC_TXT		0x0200

/* These aren't real records */
#define REC_OTHER	0x0400
#define REC_REF		0x0800
#define REC_UNKNOWN	0x1000

/* Test for records we want to map to REC_OTHER */
#define MASK_TEST_REC (REC_WKS | REC_HINFO | \
    REC_MX | REC_NS | REC_SOA | REC_RP | REC_TXT | REC_UNKNOWN)

/* Mask away records we don't care about in the final processing to REC_OTHER */
#define MASK_CHECK_REC \
    (REC_A | REC_PTR | REC_CNAME | REC_REF | REC_OTHER)

/* Test for records we want to check for duplicate name detection */
#define MASK_TEST_DUP \
    (REC_A | REC_HINFO)

/* Flags */
#define FLG_SELFMX	0x001	/* mx record refers to self */
#define FLG_MXREF	0x002	/* this record refered to by a mx record */
#define FLG_SMTPWKS	0x004	/* saw wks with smtp/tcp */
#define FLG_ALLOWDUPA	0x008	/* allow duplicate a records */

/* Test for smtp problems */
#define MASK_TEST_SMTP \
    (FLG_SELFMX | FLG_SMTPWKS)


#define ITEMSIZE (1 << 16)	/* power of two */
#define ITEMHASH(str, h, p) \
    for (p = str, h = 0; *p != '.' && *p != '\0';) h = (h << 5) - h + *p++

struct	item items[ITEMSIZE];
int	itemcnt;		/* count of items */

/* Hostname string storage */
#define STRSIZE 8192;		/* size to malloc when more space is needed */
char	*strptr;		/* pointer to string pool */
int	strsize;		/* size of space left in pool */

int	debug;
int	errors;
char	*bootfile = "/etc/named.boot";
char	*nslintboot;
char	*prog;
char	*cwd = ".";

char *tcpserv[512];		/* valid tcp service names */
char *udpserv[512];		/* valid udp service names */

/* Forwards */
static	inline void add_domain __P((char *, char *));
void	checkdups __P((struct item *, int));
int	checkserv __P((char *, char **p));
int	checkdots __P((char *));
int	checkwks __P((FILE *, char *, int *, char **));
int	cmpitem __P((const void *, const void *));
int	doit __P((char *, int));
void	initserv __P((void));
char	*intoa __P((u_long));
int	nslint __P((void));
int	main __P((int, char **));
u_long	parseinaddr __P((char *));
u_long	parseptr __P((char *, u_long, char **));
char	*parsequoted __P((char *));
void	process __P((char *, char *));
char	*savestr __P((char *));
void	updateitem __P((char *, u_long, int, int));
__dead	void usage __P((void)) __attribute__((volatile));

extern	char *optarg;
extern	int optind, opterr;

/* add domain if necessary */
static inline void
add_domain(name, domain)
	register char *name, *domain;
{
	register char *cp;

	cp = name + strlen(name) - 1;
	/* Kill trailing white space */
	while (cp > name && isspace(*cp))
		*cp-- = '\0';
	/* If necessary, append domain */
	if (cp > name && *cp++ != '.') {
		if (*domain != '.')
			*cp++ = '.';
		(void)strcpy(cp, domain);
	}
	/* XXX should we insure a trailing dot? */
}

int
main(argc, argv)
	int argc;
	char **argv;
{
	register char *cp;
	register int op, status;

	if ((cp = strrchr(argv[0], '/')) != NULL)
		prog = cp + 1;
	else
		prog = argv[0];

	while ((op = getopt(argc, argv, "b:B:d")) != -1)
		switch (op) {

		case 'b':
			bootfile = optarg;
			break;

		case 'B':
			nslintboot = optarg;
			break;

		case 'd':
			++debug;
			break;

		default:
			usage();
		}
	if (optind != argc)
		usage();

	initserv();
	status = doit(bootfile, 1);
	if (nslintboot != NULL)
		status |= doit(nslintboot, 1);
	else
		status |= doit(NSLINTBOOT, 0);
	status |= nslint();
	exit (status);
}

/* List of keywords we can safely ignore */
static char *okkeywords[] = {
	"bogusns",
	"cache",
	"domain",
	"forwarders",
	"max-fetch",
	"options",
	"secondary",
	"slave",
	"sortlist",
	"stub",
	"tcplist",
	"xfrnets",
	NULL
};

int
doit(file, mustexist)
	char *file;
	int mustexist;
{
	register int n;
	register char *cp, *cp2, **pp;
	register FILE *f;
	char buf[1024], name[128];

	errno = 0;
	f = fopen(file, "r");
	if (f == NULL) {
		/* Not an error if it doesn't exist */
		if (!mustexist && errno == ENOENT) {
			if (debug > 1)
				printf(
				    "%s: doit: %s doesn't exist (ignoring)\n",
				    prog, file);
			return (0);
		}
		fprintf(stderr, "%s: %s: %s\n", prog, file, strerror(errno));
		exit(1);
	}
	if (debug > 1)
		printf("%s: doit: opened %s\n", prog, file);

	n = 0;
	while (fgets(buf, sizeof(buf), f) != NULL) {
		++n;

		/* Skip comments */
		if (buf[0] == ';')
			continue;
		cp = strchr(buf, ';');
		if (cp)
			*cp = '\0';
		cp = buf;

		/* Eat leading whitespace */
		while (isspace(*cp))
			++cp;

		/* Skip blank lines */
		if (*cp == '\n' || *cp == '\0')
			continue;

		/* Get name */
		cp2 = cp;
		while (!isspace(*cp) && *cp != '\0')
			++cp;
		*cp++ = '\0';

		/* Find next keyword */
		while (isspace(*cp))
			++cp;
		if (strcasecmp(cp2, "directory") == 0) {
			/* Terminate directory */
			cp2 = cp;
			while (!isspace(*cp) && *cp != '\0')
				++cp;
			*cp = '\0';
			if (chdir(cp2) < 0) {
				++errors;
				fprintf(stderr, "%s: can't chdir %s: %s\n",
				    prog, cp2, strerror(errno));
				exit(1);
			}
			cwd = savestr(cp2);
			continue;
		}
		if (strcasecmp(cp2, "primary") == 0) {
			/* Extract domain */
			cp2 = name;
			while (!isspace(*cp) && *cp != '\0')
				*cp2++ = *cp++;
			/* Insure trailing dot */
			if (cp2 > name && cp2[-1] != '.')
				*cp2++ = '.';
			*cp2 = '\0';

			/* Find file */
			while (isspace(*cp))
				++cp;

			/* Terminate directory */
			cp2 = cp;
			while (!isspace(*cp) && *cp != '\0')
				++cp;
			*cp = '\0';

			/* Process it! */
			process(cp2, name);
			continue;
		}
		for (pp = okkeywords; *pp != NULL; ++pp)
			if (strcasecmp(cp2, *pp) == 0)
				break;
		if (*pp != NULL)
			continue;
		++errors;
		fprintf(stderr, "%s: %s:%d: unknown record type \"%s\"\n",
		    prog, file, n, cp2);
	}
	(void)fclose(f);

	return (errors != 0);
}

char inaddr[] = ".in-addr.arpa.";

void
process(file, domain)
	char *file, *domain;
{
	register FILE *f;
	register char *cp, *cp2, *cp3, *rtype;
	register int n, sawsoa, flags;
	register u_long addr, net;
	int smtp;
	char buf[1024], name[128], lastname[128];
	char *errstr;
	char *dotfmt = "%s: %s/%s:%d \"%s\" target missing trailing dot: %s\n";

	f = fopen(file, "r");
	if (f == NULL) {
		fprintf(stderr, "%s: %s/%s: %s\n",
		    prog, cwd, file, strerror(errno));
		++errors;
		return;
	}
	if (debug > 1)
		printf("%s: process: opened %s/%s\n", prog, cwd, file);

	/* Are we doing an in-addr.arpa domain? */
	n = 0;
	cp = domain + strlen(domain) - sizeof(inaddr) + 1;
	if (cp >= domain && strcasecmp(cp, inaddr) == 0) {
		net = parseinaddr(domain);
		if (net == 0) {
			++errors;
			fprintf(stderr,
			    "%s: %s/%s:%d bad in-addr.arpa domain\n",
			    prog, cwd, file, n);
			return;
		}
	} else
		net = 0;

	lastname[0] = '\0';
	sawsoa = 0;
	while (fgets(buf, sizeof(buf), f) != NULL) {
		++n;
		cp = buf;
		while (*cp != '\0') {
			/* Handle quoted strings (but don't report errors) */
			if (*cp == '"') {
				++cp;
				while (*cp != '"' && *cp != '\n' && *cp != '\0')
					++cp;
				continue;
			}
			if (*cp == '\n' || *cp == ';')
				break;
			++cp;
		}
		*cp-- = '\0';

		/* Nuke trailing white space */
		while (cp >= buf && isspace(*cp))
			*cp-- = '\0';

		cp = buf;
		if (*cp == '\0')
			continue;

		/* Eat multi-line soa records */
		if (sawsoa) {
			if (strchr(cp, ')') != NULL)
				sawsoa = 0;
			continue;
		}
		if (debug > 3)
			printf(">%s<\n", cp);

		/* Look for name */
		if (isspace(*cp)) {
			/* Same name as last record */
			if (lastname[0] == '\0') {
				++errors;
				fprintf(stderr,
				    "%s: %s/%s:%d no default name\n",
				    prog, cwd, file, n);
				continue;
			}
			(void)strcpy(name, lastname);
		} else {
			/* Extract name */
			cp2 = name;
			while (!isspace(*cp) && *cp != '\0')
				*cp2++ = *cp++;
			*cp2 = '\0';

			/* Check for domain shorthand */
			if (name[0] == '@' && name[1] == '\0')
				(void)strcpy(name, domain);
			(void)strcpy(lastname, name);
		}

		/* Find next token */
		while (isspace(*cp))
			++cp;

		/* Handle includes (gag) */
		if (name[0] == '$' && strcasecmp(name, "$include") == 0) {
			/* Extract filename */
			cp2 = name;
			while (!isspace(*cp) && *cp != '\0')
				*cp2++ = *cp++;
			*cp2 = '\0';

			/* Look for optional domain */
			while (isspace(*cp))
				++cp;
			if (*cp == '\0')
				process(name, domain);
			else {
				cp2 = cp;
				/* Terminate optional domain */
				while (!isspace(*cp) && *cp != '\0')
					++cp;
				*cp = '\0';
				process(name, cp2);
			}
			continue;
		}

		/* Handle $origin */
		if (name[0] == '$' && strcasecmp(name, "$origin") == 0) {
			/* Extract domain */
			cp2 = domain;
			while (!isspace(*cp) && *cp != '\0')
				*cp2++ = *cp++;
			*cp2 = '\0';
			lastname[0] = '\0';

			/* Are we doing an in-addr.arpa domain? */
			cp = domain + strlen(domain) - sizeof(inaddr) + 1;
			if (cp >= domain && strcasecmp(cp, inaddr) == 0)
				net = parseinaddr(domain);
			else
				net = 0;
			continue;
		}

		/* Eat ttl if present */
		if (isdigit(*cp)) {
			do {
				++cp;
			} while (isdigit(*cp));

			if (!isspace(*cp)) {
				++errors;
				fprintf(stderr, "%s: %s/%s:%d bad ttl\n",
				    prog, cwd, file, n);
				continue;
			}

			/* Find next token */
			++cp;
			while (isspace(*cp))
				++cp;
		}

		/* Eat optional "in" */
		if ((cp[0] == 'i' || cp[0] == 'I') &&
		    (cp[1] == 'n' || cp[1] == 'N') && isspace(cp[2])) {
			/* Find next token */
			cp += 3;
			while (isspace(*cp))
				++cp;
		}

		/* Find end of record type, converting to lower case */
		rtype = cp;
		for (rtype = cp; !isspace(*cp) && *cp != '\0'; ++cp)
			if (isupper(*cp))
				*cp = tolower(*cp);
		*cp++ = '\0';

		/* Find "the rest" */
		while (isspace(*cp))
			++cp;

		/* Check for names with dots but no trailing dot */
		if (!isdigit(*name) && checkdots(name)) {
			++errors;
			fprintf(stderr,
			  "%s: %s/%s:%d \"%s\" name missing trailing dot: %s\n",
			    prog, cwd, file, n, rtype, name);
		}

#define CHECK3(p, a, b, c) \
    (p[0] == (a) && p[1] == (b) && p[2] == (c) && p[3] == '\0')
#define CHECK2(p, a, b) \
    (p[0] == (a) && p[1] == (b) && p[2] == '\0')
#define CHECKDOT(p) \
    (p[0] == '.' && p[1] == '\0')

		if (rtype[0] == 'a' && rtype[1] == '\0') {
			/* Handle "a" record */
			add_domain(name, domain);
			addr = htonl(inet_addr(cp));
			if ((int)addr == -1) {
				++errors;
				cp2 = cp + strlen(cp) - 1;
				if (cp2 >= cp && *cp2 == '\n')
					*cp2 = '\0';
				fprintf(stderr,
			    "%s: %s/%s:%d bad \"a\" record ip addr \"%s\"\n",
				    prog, cwd, file, n, cp);
				continue;
			}
			updateitem(name, addr, REC_A, 0);
		} else if (CHECK3(rtype, 'p', 't', 'r')) {
			/* Handle "ptr" record */
			if (checkdots(cp)) {
				++errors;
				fprintf(stderr, dotfmt,
				    prog, cwd, file, n, rtype, cp);
			}
			add_domain(cp, domain);
			errstr = NULL;
			addr = parseptr(name, net, &errstr);
			if (errstr != NULL) {
				++errors;
				fprintf(stderr,
			"%s: %s/%s:%d bad \"ptr\" record (%s) ip addr \"%s\"\n",
				    prog, cwd, file, n, errstr, name);
				continue;
			}
			updateitem(cp, addr, REC_PTR, 0);
		} else if (CHECK3(rtype, 's', 'o', 'a')) {
			/* Handle "soa" record */
			if (!CHECKDOT(name)) {
				add_domain(name, domain);
				updateitem(name, 0, REC_SOA, 0);
			}
			/* Don't eat lines unless a multi-line SOA */
			if (strchr(cp, ')') == NULL)
				++sawsoa;
		} else if (CHECK3(rtype, 'w', 'k', 's')) {
			/* Handle "wks" record */
			addr = htonl(inet_addr(cp));
			if ((int)addr == -1) {
				++errors;
				cp2 = cp;
				while (!isspace(*cp2) && *cp2 != '\0')
					++cp2;
				*cp2 = '\0';
				fprintf(stderr,
			    "%s: %s/%s:%d bad \"wks\" record ip addr \"%s\"\n",
				    prog, cwd, file, n, cp);
				continue;
			}
			/* Step over ip address */
			while (*cp == '.' || isdigit(*cp))
				++cp;
			while (isspace(*cp))
				*cp++ = '\0';
			/* Make sure services are legit */
			errstr = NULL;
			n += checkwks(f, cp, &smtp, &errstr);
			if (errstr != NULL) {
				++errors;
				fprintf(stderr,
				    "%s: %s/%s:%d bad \"wks\" record (%s)\n",
				    prog, cwd, file, n, errstr);
				continue;
			}
			add_domain(name, domain);
			updateitem(name, addr, REC_WKS, smtp ? FLG_SMTPWKS : 0);
			/* XXX check to see if ip address records exists? */
		} else if (rtype[0] == 'h' && strcmp(rtype, "hinfo") == 0) {
			/* Handle "hinfo" record */
			add_domain(name, domain);
			updateitem(name, 0, REC_HINFO, 0);
			cp2 = cp;
			cp = parsequoted(cp);
			if (cp == NULL) {
				++errors;
				fprintf(stderr,
			    "%s: %s/%s:%d \"hinfo\" missing quote: %s\n",
				    prog, cwd, file, n, cp2);
				continue;
			}
			if (!isspace(*cp)) {
				++errors;
				fprintf(stderr,
			    "%s: %s/%s:%d \"hinfo\" missing white space: %s\n",
				    prog, cwd, file, n, cp2);
				continue;
			}
			++cp;
			while (isspace(*cp))
				++cp;
			if (*cp == '\0') {
				++errors;
				fprintf(stderr,
			    "%s: %s/%s:%d \"hinfo\" missing keyword: %s\n",
				    prog, cwd, file, n, cp2);
				continue;
			}
			cp = parsequoted(cp);
			if (cp == NULL) {
				++errors;
				fprintf(stderr,
			    "%s: %s/%s:%d \"hinfo\" missing quote: %s\n",
				    prog, cwd, file, n, cp2);
				continue;
			}
			if (*cp != '\0') {
				++errors;
				fprintf(stderr,
			"%s: %s/%s:%d \"hinfo\" garbage after keywords: %s\n",
				    prog, cwd, file, n, cp2);
				continue;
			}
		} else if (CHECK2(rtype, 'm', 'x')) {
			/* Handle "mx" record */
			add_domain(name, domain);
			updateitem(name, 0, REC_MX, 0);

			/* Look for priority */
			if (!isdigit(*cp)) {
				++errors;
				fprintf(stderr,
				    "%s: %s/%s:%d bad \"mx\" priority: %s\n",
				    prog, cwd, file, n, cp);
			}

			/* Skip over priority */
			++cp;
			while (isdigit(*cp))
				++cp;
			while (isspace(*cp))
				++cp;
			if (*cp == '\0') {
				++errors;
				fprintf(stderr,
				    "%s: %s/%s:%d missing \"mx\" hostname\n",
				    prog, cwd, file, n);
			}
			if (checkdots(cp)) {
				++errors;
				fprintf(stderr, dotfmt,
				    prog, cwd, file, n, rtype, cp);
			}

			/* Check to see if mx host exists */
			add_domain(cp, domain);
			flags = FLG_MXREF;
			if (*name == *cp && strcmp(name, cp) == 0)
				flags |= FLG_SELFMX;
			updateitem(cp, 0, REC_REF, flags);
		} else if (rtype[0] == 'c' && strcmp(rtype, "cname") == 0) {
			/* Handle "cname" record */
			add_domain(name, domain);
			updateitem(name, 0, REC_CNAME, 0);
			if (checkdots(cp)) {
				++errors;
				fprintf(stderr, dotfmt,
				    prog, cwd, file, n, rtype, cp);
			}

			/* Make sure cname points somewhere */
			add_domain(cp, domain);
			updateitem(cp, 0, REC_REF, 0);
		} else if (CHECK3(rtype, 't', 'x', 't')) {
			/* Handle "txt" record */
			add_domain(name, domain);
			updateitem(name, 0, REC_TXT, 0);
			cp2 = cp;
			cp = parsequoted(cp);
			if (cp == NULL) {
				++errors;
				fprintf(stderr,
				    "%s: %s/%s:%d \"txt\" missing quote: %s\n",
				    prog, cwd, file, n, cp2);
				continue;
			}
			while (isspace(*cp))
				++cp;
			if (*cp != '\0') {
				++errors;
				fprintf(stderr,
			    "%s: %s/%s:%d \"txt\" garbage after text: %s\n",
				     prog, cwd, file, n, cp2);
				continue;
			}
		} else if (CHECK2(rtype, 'n', 's')) {
			/* Handle "ns" record */
			if (checkdots(cp)) {
				++errors;
				fprintf(stderr, dotfmt,
				    prog, cwd, file, n, rtype, cp);
			}
			add_domain(cp, domain);
			updateitem(cp, 0, REC_NS, 0);
		} else if (CHECK2(rtype, 'r', 'p')) {
			/* Handle "rp" record */
			add_domain(name, domain);
			updateitem(name, 0, REC_RP, 0);
			cp2 = cp;

			/* Step over mailbox name */
			/* XXX could add_domain() and check further */
			while (!isspace(*cp) && *cp != '\0')
				++cp;
			if (*cp == '\0') {
				++errors;
				fprintf(stderr,
			    "%s: %s/%s:%d \"rp\" missing text name: %s\n",
				    prog, cwd, file, n, cp2);
				continue;
			}
			++cp;
			cp3 = cp;

			/* Step over text name */
			while (!isspace(*cp) && *cp != '\0')
				++cp;

			if (*cp != '\0') {
				++errors;
				fprintf(stderr,
			    "%s: %s/%s:%d \"rp\" garbage after text name: %s\n",
				     prog, cwd, file, n, cp2);
				continue;
			}

			/* Make sure text name points somewhere (if not ".") */
			if (!CHECKDOT(cp3)) {
				add_domain(cp3, domain);
				updateitem(cp3, 0, REC_REF, 0);
			}
		} else if (rtype[0] == 'a' && strcmp(rtype, "allowdupa") == 0) {
			/* Handle "allow duplicate a" record */
			add_domain(name, domain);
			addr = htonl(inet_addr(cp));
			if ((int)addr == -1) {
				++errors;
				cp2 = cp + strlen(cp) - 1;
				if (cp2 >= cp && *cp2 == '\n')
					*cp2 = '\0';
				fprintf(stderr,
		    "%s: %s/%s:%d bad \"allowdupaa\" record ip addr \"%s\"\n",
				    prog, cwd, file, n, cp);
				continue;
			}
			updateitem(name, addr, 0, FLG_ALLOWDUPA);
		} else {
			/* Unknown record type */
			++errors;
			fprintf(stderr,
			    "%s: %s/%s:%d unknown record type \"%s\"\n",
			    prog, cwd, file, n, rtype);
			add_domain(name, domain);
			updateitem(name, 0, REC_UNKNOWN, 0);
		}
	}
	(void)fclose(f);
	return;
}

char *
savestr(str)
	register char *str;
{
	register int i;

	i = strlen(str) + 1;
	if (i > strsize) {
		strsize = STRSIZE;
		strptr = malloc(strsize);
		if (strptr == NULL) {
			fprintf(stderr, "%s: savestr: malloc: %s\n",
			    prog, strerror(errno));
			exit(1);
		}
	}
	(void)strcpy(strptr, str);
	str = strptr;
	strptr += i;
	strsize -= i;
	return (str);
}

/* Records we use to detect duplicates */
static struct duprec {
	int record;
	char *name;
} duprec[] = {
	{ REC_A, "a" },
	{ REC_HINFO, "hinfo" },
	{ 0, NULL },
};

void
checkdups(ip, records)
	register struct item *ip;
	register int records;
{
	register struct duprec *dp;

	records &= (ip->records & MASK_TEST_DUP);
	if (records == 0)
		return;
	for (dp = duprec; dp->name != NULL; ++dp)
		if ((records & dp->record) != 0) {
			++errors;
			fprintf(stderr, "%s: multiple \"%s\" records for %s\n",
			    prog, dp->name, ip->host);
			records &= ~dp->record;
		}
	if (records != 0)
		fprintf(stderr, "%s: checkdups: records not zero (%d)\n",
		    prog, records);
}

void
updateitem(host, addr, records, flags)
	register char *host;
	register u_long addr;
	register int records, flags;
{
	register char *p;
	register int n;
	register u_int i;
	register struct item *ip;
	int foundsome;

	n = 0;
	foundsome = 0;
	ITEMHASH(host, i, p);
	ip = &items[i & (ITEMSIZE - 1)];
	while (n < ITEMSIZE && ip->host) {
		if ((addr == 0 || addr == ip->addr || ip->addr == 0) &&
		    *host == *ip->host && strcmp(host, ip->host) == 0) {
			++foundsome;
			if (ip->addr == 0)
				ip->addr = addr;
			if ((records & MASK_TEST_DUP) != 0)
				checkdups(ip, records);
			ip->records |= records;
			ip->flags |= flags;
			/* Not done if we wildcard matched the name */
			if (addr)
				return;
		}
		++n;
		++ip;
		if (ip >= &items[ITEMSIZE])
			ip = items;
	}

	if (n >= ITEMSIZE) {
		fprintf(stderr, "%s: out of item slots (max %d)\n",
		    prog, ITEMSIZE);
		exit(1);
	}

	/* Done if we were wildcarding the name (and found entries for it) */
	if (addr == 0 && foundsome)
		return;

	/* Didn't find it, make new entry */
	++itemcnt;
	if (ip->host) {
		fprintf(stderr, "%s: reusing bucket!\n", prog);
		exit(1);
	}
	ip->addr = addr;
	ip->host = savestr(host);
	if ((records & MASK_TEST_DUP) != 0)
		checkdups(ip, records);
	ip->records |= records;
	ip->flags |= flags;
}

int
nslint()
{
	register int n, i, records, flags;
	register char *cp;
	register struct item *ip, **ipp, **itemlist;
	register u_long addr, last;

	itemlist = (struct item **)calloc(itemcnt, sizeof(*ipp));
	if (itemlist == NULL) {
		fprintf(stderr, "%s: nslint: calloc: %s\n",
		    prog, strerror(errno));
		exit(1);
	}
	ipp = itemlist;
	for (n = 0, ip = items; n < ITEMSIZE; ++n, ++ip) {
		if (ip->host == NULL)
			continue;

		/* Save entries with addresses for later check */
		if (ip->addr != 0)
			*ipp++ = ip;

		if (debug > 1) {
			if (debug > 2)
				printf("%d\t", n);
			printf("%s\t%s\t0x%x\t0x%x\n",
			    ip->host, intoa(ip->addr), ip->records, ip->flags);
		}

		/* Check for illegal hostnames (rfc1034) */
		cp = ip->host;
		if (!isalpha(*cp) && !isdigit(*cp)) {
			++errors;
			fprintf(stderr,
		"%s: illegal hostname \"%s\" (starts with non-alpha/numeric)\n",
			    prog, ip->host);
		}
		i = 0;
		for (++cp; *cp != '.' && *cp != '\0'; ++cp)
			if (!isalpha(*cp) && !isdigit(*cp) && *cp != '-') {
				++errors;
				fprintf(stderr,
		    "%s: illegal hostname \"%s\" ('%c' illegal character)\n",
				    prog, ip->host, *cp);
				break;
			}
		if (--cp >= ip->host && *cp == '-') {
			++errors;
			fprintf(stderr,
			    "%s: illegal hostname \"%s\" (ends with '-')\n",
			    prog, ip->host);
		}

		/* Check for missing ptr records (ok if also an ns record) */
		records = ip->records & MASK_CHECK_REC;
		if ((ip->records & MASK_TEST_REC) != 0)
			records |= REC_OTHER;
		switch (records) {

		case REC_A | REC_OTHER | REC_PTR | REC_REF:
		case REC_A | REC_OTHER | REC_PTR:
		case REC_A | REC_PTR | REC_REF:
		case REC_A | REC_PTR:
		case REC_CNAME | REC_REF:
		case REC_CNAME:
			/* These are O.K. */
			break;

		case REC_OTHER | REC_REF:
		case REC_OTHER:
			/*
			 * This is only an error if there is an address
			 * associated with the hostname; this means
			 * there was a wks entry with bogus address.
			 * Otherwise, we have an mx or hinfo.
			 */
			if (ip->addr != 0) {
				++errors;
				fprintf(stderr,
			    "%s: \"wks\" without \"a\" and \"ptr\": %s -> %s\n",
				    prog, ip->host, intoa(ip->addr));
			}
			break;

		case REC_REF:
			++errors;
			fprintf(stderr,
			    "%s: name referenced without other records: %s\n",
			    prog, ip->host);
			break;

		case REC_A | REC_OTHER | REC_REF:
		case REC_A | REC_OTHER:
		case REC_A | REC_REF:
		case REC_A:
			++errors;
			fprintf(stderr, "%s: missing \"ptr\": %s -> %s\n",
			    prog, ip->host, intoa(ip->addr));
			break;

		case REC_OTHER | REC_PTR | REC_REF:
		case REC_OTHER | REC_PTR:
		case REC_PTR | REC_REF:
		case REC_PTR:
			++errors;
			fprintf(stderr, "%s: missing \"a\": %s -> %s\n",
			    prog, ip->host, intoa(ip->addr));
			break;

		case REC_A | REC_CNAME | REC_OTHER | REC_PTR | REC_REF:
		case REC_A | REC_CNAME | REC_OTHER | REC_PTR:
		case REC_A | REC_CNAME | REC_OTHER | REC_REF:
		case REC_A | REC_CNAME | REC_OTHER:
		case REC_A | REC_CNAME | REC_PTR | REC_REF:
		case REC_A | REC_CNAME | REC_PTR:
		case REC_A | REC_CNAME | REC_REF:
		case REC_A | REC_CNAME:
		case REC_CNAME | REC_OTHER | REC_PTR | REC_REF:
		case REC_CNAME | REC_OTHER | REC_PTR:
		case REC_CNAME | REC_OTHER | REC_REF:
		case REC_CNAME | REC_OTHER:
		case REC_CNAME | REC_PTR | REC_REF:
		case REC_CNAME | REC_PTR:
			++errors;
			fprintf(stderr, "%s: \"cname\" %s has other records\n",
			    prog, ip->host);
			break;

		case 0:
			/* Second level test */
			if ((ip->records & ~(REC_NS | REC_TXT)) == 0)
				break;
			/* Fall through... */

		default:
			++errors;
			fprintf(stderr,
			    "%s: records == 0x%x: can't happen (%s 0x%x)\n",
			    prog, records, ip->host, ip->records);
			break;
		}

		/* Check for smtp problems */
		flags = ip->flags & MASK_TEST_SMTP;
		switch (flags) {

		case 0:
		case FLG_SELFMX | FLG_SMTPWKS:
			/* These are O.K. */
			break;

		case FLG_SELFMX:
			if ((ip->records & REC_WKS) != 0) {
				++errors;
				fprintf(stderr,
				    "%s: smtp/tcp missing from \"wks\": %s\n",
				    prog, ip->host);
			}
			break;

		case FLG_SMTPWKS:
			++errors;
			fprintf(stderr,
			    "%s: saw smtp/tcp without self \"mx\": %s\n",
			    prog, ip->host);
			break;

		default:
			++errors;
			fprintf(stderr,
			    "%s: flags == 0x%x: can't happen (%s)\n",
			    prog, flags, ip->host);
		}

		/* Check for chained MX records */
		if ((ip->flags & (FLG_SELFMX | FLG_MXREF)) == FLG_MXREF &&
		    (ip->records & REC_MX) != 0) {
			++errors;
			fprintf(stderr,
			    "%s: \"mx\" referenced by other \"mx\" record: %s\n",
			    prog, ip->host);
		}
	}

	/* Check for doubly booked addresses */
	n = ipp - itemlist;
	qsort(itemlist, n, sizeof(itemlist[0]), cmpitem);
	last = 0;
	ip = NULL;
	for (ipp = itemlist; n > 0; ++ipp, --n) {
		addr = (*ipp)->addr;
		if (last == addr &&
		    ((*ipp)->flags & FLG_ALLOWDUPA) == 0 &&
		    (ip->flags & FLG_ALLOWDUPA) == 0) {
			++errors;
			fprintf(stderr, "%s: %s in use by %s and %s\n",
			    prog, intoa(addr), (*ipp)->host, ip->host);
		}
		last = addr;
		ip = *ipp;
	}

	if (debug)
		printf("%s: %d/%d items used, %d error%s\n", prog, itemcnt,
		    ITEMSIZE, errors, errors == 1 ? "" : "s");
	return (errors != 0);
}

/* Similar to inet_ntoa() */
char *
intoa(addr)
	u_long addr;
{
	register char *cp;
	register u_int byte;
	register int n;
	static char buf[sizeof(".xxx.xxx.xxx.xxx")];

	cp = &buf[sizeof buf];
	*--cp = '\0';

	n = 4;
	do {
		byte = addr & 0xff;
		*--cp = byte % 10 + '0';
		byte /= 10;
		if (byte > 0) {
			*--cp = byte % 10 + '0';
			byte /= 10;
			if (byte > 0)
				*--cp = byte + '0';
		}
		*--cp = '.';
		addr >>= 8;
	} while (--n > 0);

	return cp + 1;
}

u_long
parseinaddr(cp)
	register char *cp;
{
	register u_long o, net;


	net = 0;
	if (isdigit(*cp)) {
		o = 0;
		do {
			o = o * 10 + (*cp++ - '0');
		} while (isdigit(*cp));
		net = o << 24;
		if (*cp == '.' && isdigit(cp[1])) {
			++cp;
			o = 0;
			do {
				o = o * 10 + (*cp++ - '0');
			} while (isdigit(*cp));
			net = (net >> 8) | (o << 24);
			if (*cp == '.' && isdigit(cp[1])) {
				++cp;
				o = 0;
				do {
					o = o * 10 + (*cp++ - '0');
				} while (isdigit(*cp));
				net = (net >> 8) | (o << 24);
			}
		}
	}
	if (strcasecmp(cp, ".in-addr.arpa.") != 0)
		return (0);

	return (net);
}

u_long
parseptr(cp, net, errstrp)
	register char *cp;
	u_long net;
	register char **errstrp;
{
	register u_long o, addr, addrmask;
	register int shift;

	addr = 0;
	addrmask = 0;
	shift = 0;
	while (isdigit(*cp) && shift < 32) {
		o = 0;
		do {
			o = o * 10 + (*cp++ - '0');
		} while (isdigit(*cp));
		addr |= o << shift;
		addrmask |= 0xff << shift;
		shift += 8;
		if (*cp != '.')
			break;
		++cp;
	}

	if (shift > 32) {
		*errstrp = "more than 4 octets";
		return (0);
	}

	if (shift == 32 && strcasecmp(cp, "in-addr.arpa.") == 0)
		return (addr);

	if (*cp != '\0') {
		*errstrp = "trailing junk";
		return (0);
	}
	if ((addrmask & net) != 0) {
		*errstrp = "too many octets for net";
		return (0);
	}
	return (net | addr);
}

int
checkwks(f, proto, smtpp, errstrp)
	register FILE *f;
	register char *proto;
	register int *smtpp;
	register char **errstrp;
{
	register int n, sawparen;
	register char *cp, *serv, **p;
	static char errstr[132];
	char buf[1024];

	/* Line count */
	n = 0;

	/* Terminate protocol */
	cp = proto;
	while (!isspace(*cp) && *cp != '\0')
		++cp;
	if (*cp != '\0')
		*cp++ = '\0';
	if (strcasecmp(proto, "tcp") == 0)
		p = tcpserv;
	else if (strcasecmp(proto, "udp") == 0)
		p = udpserv;
	else {
		(void)sprintf(errstr, "unknown protocol \"%s\"", proto);
		*errstrp = errstr;
		return (n);
	}

	/* Find services */
	*smtpp = 0;
	sawparen = 0;
	if (*cp == '(') {
		++sawparen;
		++cp;
		while (isspace(*cp))
			++cp;
	}
	for (;;) {
		if (*cp == '\0') {
			if (!sawparen)
				break;
			if (fgets(buf, sizeof(buf), f) == NULL) {
				*errstrp = "mismatched parens";
				return (n);
			}
			++n;
			cp = buf;
			while (isspace(*cp))
				++cp;
		}
		/* Find end of service, converting to lower case */
		for (serv = cp; !isspace(*cp) && *cp != '\0'; ++cp)
			if (isupper(*cp))
				*cp = tolower(*cp);
		if (*cp != '\0')
			*cp++ = '\0';
		if (sawparen && *cp == ')') {
			/* XXX should check for trailing junk */
			break;
		}
		if (*serv == 's' && strcmp(serv, "smtp") == 0)
			++*smtpp;
		if (!checkserv(serv, p)) {
			sprintf(errstr, "%s/%s unknown", proto, serv);
			*errstrp = errstr;
			break;
		}
	}

	return (n);
}

int
checkserv(serv, p)
	register char *serv, **p;
{
	for (; *p != NULL; ++p)
		if (*serv == **p && strcmp(serv, *p) == 0)
			return (1);
	return (0);
}

void
initserv()
{
	register char *cp, *cp2, **up, **tp;
	register struct servent *sp;

	tp = tcpserv;
	tp[(sizeof(tcpserv) / sizeof(tcpserv[0])) - 1] = (char *)1;
	up = udpserv;
	up[(sizeof(udpserv) / sizeof(udpserv[0])) - 1] = (char *)1;
	while ((sp = getservent()) != NULL) {
		cp = savestr(sp->s_name);
		/* Convert to lower case */
		for (cp2 = cp; *cp2 != '\0'; ++cp2)
			if (isupper(*cp2))
				*cp2 = tolower(*cp2);
		if (strcasecmp(sp->s_proto, "tcp") == 0) {
			*tp++ = cp;
			if (*tp !=  NULL) {
				fprintf(stderr, "%s: too many tcp protocols\n",
				    prog);
				exit(1);
			}
			continue;
		}
		if (strcasecmp(sp->s_proto, "udp") == 0) {
			*up++ = cp;
			if (*up !=  NULL) {
				fprintf(stderr, "%s: too many udp protocols\n",
				    prog);
				exit(1);
			}
			continue;
		}
		fprintf(stderr, "%s: %s/%s unknown service/protocol\n",
		    prog, sp->s_name, sp->s_proto);
		exit(1);
	}
}

/* Returns true if name contains a dot but not a trailing dot */
int
checkdots(name)
	register char *name;
{
	register char *cp;

	if (strchr(name, '.') == NULL)
		return (0);
	cp = name + strlen(name) - 1;
	if (cp >= name && *cp == '.')
		return (0);
	return (1);
}

int
cmpitem(ip1, ip2)
	const void *ip1, *ip2;
{
	register u_long a1, a2;

	a1 = (*(struct item **)ip1)->addr;
	a2 = (*(struct item **)ip2)->addr;

	if (a1 < a2)
		return (-1);
	else if (a1 > a2)
		return (1);
	else
		return (0);
}

/* Returns a pointer after the next token or quoted string, else NULL */
char *
parsequoted(cp)
	register char *cp;
{

	if (*cp == '"') {
		++cp;
		while (*cp != '"' && *cp != '\0')
			++cp;
		if (*cp != '"')
			return (NULL);
		++cp;
	} else {
		while (!isspace(*cp) && *cp != '\0')
			++cp;
	}
	return (cp);
}

__dead void
usage()
{
	fprintf(stderr, "usage: %s [-d] [-b named.boot] [-B nslint.boot]\n",
	    prog);
	exit(1);
}
