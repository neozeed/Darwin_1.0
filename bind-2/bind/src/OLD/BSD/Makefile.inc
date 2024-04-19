# $Id: Makefile.inc,v 1.1.1.1 1999/10/04 22:24:37 wsanchez Exp $

.ifndef (Mk.Inc)
Mk.Inc?=defined

BIND_DIR=	${.CURDIR}/../BIND

VER!=		awk -F' *= *' '$$1 == "VER" { print $$2 ; exit }' \
		${BIND_DIR}/Makefile

PIDDIR=		/var/run
INDOT=
XFER_INDOT=	${INDOT}named_xfer
PS=		ps
DESTSBIN=	/usr/sbin
IOT=		ABRT

CONFIG?=	-DUSE_OPTIONS_H
INCLUDE?=	-I. -I${BIND_DIR} -I${BIND_DIR}/include
CFLAGS=		-O2 ${INCLUDE} ${CONFIG}
LDADD+=		-lresolv

BINGRP=		bin
BINOWN=		bin

CLEANFILES+=	*.0
.endif
