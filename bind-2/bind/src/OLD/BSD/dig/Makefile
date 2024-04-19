# $Id: Makefile,v 1.1.1.1 1999/10/04 22:24:37 wsanchez Exp $

.PATH:	${.CURDIR}/../BIND/tools \
	${.CURDIR}/../BIND/tools/nslookup \
	${.CURDIR}/../BIND/man

PROG=	dig
SRCS=	dig.c list.c subr.c debug.c send.c

BINDIR=	/usr/bin

.include <bsd.prog.mk>
.include "${.CURDIR}/../Makefile.maninc"
