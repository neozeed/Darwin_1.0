# $Id: Makefile,v 1.1.1.1 1999/10/04 22:24:38 wsanchez Exp $

.PATH:	${.CURDIR}/../BIND/named \
	${.CURDIR}/../BIND/man

BINDIR=	${DESTSBIN}
FINSTALL?= install -c -o ${BINOWN} -g ${BINGRP}
CLEANFILES+= named.reload
MAN8=	named.reload.0

all:	named.reload ${MAN8}

install:: maninstall
	${FINSTALL} -m 755 named.reload ${DESTDIR}${BINDIR}/named.reload

named.reload:	named.reload.sh Makefile ${.CURDIR}/../Makefile.inc
	sed -e "s|%INDOT%|${INDOT}|" \
	    -e "s|%DESTSBIN%|${DESTSBIN}|" \
	    < ${.CURDIR}/../BIND/named/named.reload.sh > named.reload
	chmod +x named.reload

.include <bsd.prog.mk>
.include "${.CURDIR}/../Makefile.maninc"
