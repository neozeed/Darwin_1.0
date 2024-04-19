# $Id: Makefile,v 1.1.1.1 1999/10/04 22:24:38 wsanchez Exp $

.PATH:	${.CURDIR}/../BIND/named \
	${.CURDIR}/../BIND/man

BINDIR=	${DESTSBIN}
FINSTALL?= install -c -o ${BINOWN} -g ${BINGRP}
CLEANFILES+= named.restart
MAN8=	named.restart.0

all:	named.restart ${MAN8}

install:: maninstall
	${FINSTALL} -m 755 named.restart ${DESTDIR}${BINDIR}/named.restart

named.restart:	named.restart.sh Makefile ${.CURDIR}/../Makefile.inc
	sed -e "s|%INDOT%|${INDOT}|" \
	    -e "s|%DESTSBIN%|${DESTSBIN}|" \
	    < ${.CURDIR}/../BIND/named/named.restart.sh > named.restart
	chmod +x named.restart

.include <bsd.prog.mk>
.include "${.CURDIR}/../Makefile.maninc"
