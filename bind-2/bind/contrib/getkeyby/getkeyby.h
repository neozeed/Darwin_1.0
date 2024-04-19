/*
 * Get public keys from the Domain Name System.
 * John Gilmore, 14 June 1996.
 *
 * This file is designed to be used in applications, until the Domain
 * Name System on all hosts platforms of interest has been updated to
 * include these definitions.  Include it after <resolv.h> and
 * <arpa/nameser.h>.
 *
 * In the DNS implementation itself, all the definitions in this file
 * are already in <resolv.h> and <arpa/nameser.h>.
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
static const char rcsid[] = "$Id: getkeyby.h,v 1.1.1.1 1999/10/04 22:25:53 wsanchez Exp $";
#endif

/* First a little ANSI-ism */
#ifndef __P
#ifdef __STDC__
#define	__P(x) x
#else
#define	__P(x) ()
#ifndef const
#define const /*nothing*/
#endif /* const */
#endif /* STDC */
#endif /* __P */

/* Things missing from old versions of <arpa/nameser.h> */
#ifndef HFIXEDSZ
#define	HFIXEDSZ 12
#endif
#ifndef INT16SZ
#define	INT16SZ 2
#endif
#ifndef INT32SZ
#define	INT32SZ 4
#endif

#ifndef T_KEY			/* When used with old nameser.h */
#define	T_KEY		25		/* security key */
#endif

#ifndef KEYFLAG_IPSEC			/* More old nameser.h support */
/*
 * Flags field of the KEY RR rdata
 */
#define	KEYFLAG_TYPEMASK	0xC000	/* Mask for "type" bits */
#define	KEYFLAG_TYPE_AUTH_CONF	0x0000	/* Key usable for both */
#define	KEYFLAG_TYPE_CONF_ONLY	0x8000	/* Key usable for confidentiality */
#define	KEYFLAG_TYPE_AUTH_ONLY	0x4000	/* Key usable for authentication */
#define	KEYFLAG_TYPE_NO_KEY	0xC000	/* No key usable for either; no key */
/* The type bits can also be interpreted independently, as single bits: */
#define	KEYFLAG_NO_AUTH		0x8000	/* Key not usable for authentication */
#define	KEYFLAG_NO_CONF		0x4000	/* Key not usable for confidentiality */

#define	KEYFLAG_EXPERIMENTAL	0x2000	/* Security is *mandatory* if bit=0 */
#define	KEYFLAG_RESERVED3	0x1000  /* reserved - must be zero */
#define	KEYFLAG_RESERVED4	0x0800  /* reserved - must be zero */
#define	KEYFLAG_USERACCOUNT	0x0400	/* key is assoc. with a user acct */
#define	KEYFLAG_ENTITY		0x0200	/* key is assoc. with entity eg host */
#define	KEYFLAG_ZONEKEY		0x0100	/* key is zone key for the zone named */
#define	KEYFLAG_IPSEC		0x0080  /* key is for IPSEC use (host or user)*/
#define	KEYFLAG_EMAIL		0x0040  /* key is for email (MIME security) */
#define	KEYFLAG_RESERVED10	0x0020  /* reserved - must be zero */
#define	KEYFLAG_RESERVED11	0x0010  /* reserved - must be zero */
#define	KEYFLAG_SIGNATORYMASK	0x000F	/* key can sign DNS RR's of same name */

#define  KEYFLAG_RESERVED_BITMASK ( KEYFLAG_RESERVED3 | \
				    KEYFLAG_RESERVED4 | \
				    KEYFLAG_RESERVED10| KEYFLAG_RESERVED11) 

/* The Algorithm field of the KEY and SIG RR's is an integer, {1..254} */
#define	ALGORITHM_MD5RSA	1	/* MD5 with RSA */
#define	ALGORITHM_EXPIRE_ONLY	253	/* No alg, no security */
#define	ALGORITHM_PRIVATE_OID	254	/* Key begins with OID indicating alg */

/* Signatures */
					/* Size of a mod or exp in bits */
#define	MIN_MD5RSA_KEY_PART_BITS	 512
#define	MAX_MD5RSA_KEY_PART_BITS	2552
					/* Total of binary mod and exp, bytes */
#define	MAX_MD5RSA_KEY_BYTES		((MAX_MD5RSA_KEY_PART_BITS+7/8)*2+3)
					/* Max length of text sig block */
#define	MAX_KEY_BASE64			(((MAX_MD5RSA_KEY_BYTES+2)/3)*4)
#endif
/* End of definitions for old nameser.h support.  */



#ifndef DNSKEY_NO_OPTIONS		/* Until these are in nameser.h
					   and resolv.h. */

/* Representation of a key from the Domain Name System.  */
struct dnskey {
	char	      *dname;		/* Domain name involved */
	u_long 	      ttl;		/* Time to live (key validity period) */
	unsigned      keyflags;		/* Flag bits */
	u_char        protocol;		/* Network protocol this key is for */
	u_char        algorithm;	/* Crypto algorithm this key is for */
	u_char        keylen;		/* Length of key in key space */
	u_char        *key;		/* The key itself */
};

int	getkeybyname __P ((struct dnskey *, unsigned, unsigned));

/* Options for getkeybyname() */
#define	DNSKEY_NO_OPTIONS		0	/* No options */
#define	DNSKEY_AUTHORITATIVE_OPTION	0x0001	/* Require authoritative data */
#define	DNSKEY_CRYPTOVALID_OPTION	0x0002	/* Require cryptovalid data */



/* A representation of a DNS resource record. */
struct dns_rr {
	char	*name;			/* Name of this RR, e.g. "toad.com" */
	unsigned	type;		/* RR type e.g. T_A, T_CNAME, T_SOA */
	unsigned	class;		/* RR class, effectively always IN */
	u_long	ttl;			/* RR's remaining Time To Live */
	unsigned	rdlength;	/* Length in bytes of RR data field */
	u_char 	*rdata;			/* The RR data field */
	struct	dns_response *response;	/* Whole response containing this RR */
};
typedef struct dns_rr dns_rr;

/* A representation of the answer that the DNS sends back to a resolver. */
struct dns_response {
	void	*msg, *eom;	/* Args for dn_expand of rdata's */
	/* The query FIXME-not here yet */
	char	*queryname;	/* Name used in query e.g. "toad.com" */
	unsigned	ans_count;
	struct	dns_rr *answers;
	unsigned	auth_count;
	struct	dns_rr *authorities;
	unsigned	glue_count;
	struct	dns_rr *glue;
};

#define	res_parse_response __res_parse_response

struct dns_response *
		res_parse_response __P((const unsigned char *, int));

#endif /* DNSKEY_NO_OPTIONS */
