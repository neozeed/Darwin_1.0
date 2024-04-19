/*
 * Copyright (c) 1994, 1995
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
 * @(#) $Header: /cvs/Darwin/Services/bind/bind/contrib/nslint-1.5.1/lbl/os-irix5.h,v 1.1.1.1 1999/10/04 22:25:55 wsanchez Exp $ (LBL)
 */

#define SIGRET void
#define SIGRETVAL
#define WAITSTATUS int

#define	bcopy(s, d, n)	memcpy(d, s, n)
#define major(x)	((int)(((unsigned)(x)>>8)&0377))
#define minor(x)	((int)((x)&0377))
#define	bzero(s, n)	memset(s, 0, n)
#define	bcmp(s1, s2, n)	memcmp(s1, s2, n)
#define setlinebuf(f)	setvbuf(f, NULL, _IOLBF, 0)

/* Signal compat */
#ifndef _BSD_SIGNALS
#define sigmask(m)	(1 << ((m)-1))
#define signal(s, f)	sigset(s, f)
#endif

/* Prototypes missing in IRIX 5 */
#ifdef __STDC__
struct	ether_addr;
#endif
int	ether_hostton(char *, struct ether_addr *);
char	*ether_ntoa(struct ether_addr *);
#ifdef __STDC__
struct	utmp;
#endif
void	login(struct utmp *);
int	setenv(const char *, const char *, int);
int	sigblock(int);
int	sigsetmask(int);
int	snprintf(char *, size_t, const char *, ...);
time_t	time(time_t *);

#ifndef ETHERTYPE_REVARP
#define ETHERTYPE_REVARP 0x8035
#endif

#ifndef	IPPROTO_ND
/* From <netinet/in.h> on a Sun somewhere. */
#define	IPPROTO_ND	77
#endif

#ifndef REVARP_REQUEST
#define REVARP_REQUEST 3
#endif
#ifndef REVARP_REPLY
#define REVARP_REPLY 4
#endif

#ifndef ETHERTYPE_TRAIL
#define ETHERTYPE_TRAIL 0x1000
#endif

/* newish RIP commands */
#ifndef	RIPCMD_POLL
#define	RIPCMD_POLL 5
#endif
#ifndef	RIPCMD_POLLENTRY
#define	RIPCMD_POLLENTRY 6
#endif
