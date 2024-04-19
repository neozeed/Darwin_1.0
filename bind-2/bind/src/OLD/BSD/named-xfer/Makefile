# $Id: Makefile,v 1.1.1.1 1999/10/04 22:24:38 wsanchez Exp $

.PATH:	${.CURDIR}/../BIND/named \
	${.CURDIR}/../named/obj \
	${.CURDIR}/../BIND/man

PROG=	named-xfer
SRCS=	named-xfer.c db_glue.c storage.c dmalloc.c version.c
#LDADD+=	-lutil
#DPADD+=	${LIBUTIL}

BINDIR=	/usr/libexec

.include <bsd.prog.mk>
.include "${.CURDIR}/../Makefile.maninc"
