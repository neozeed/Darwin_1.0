# $Id: Makefile,v 1.1.1.1 1999/10/04 22:24:38 wsanchez Exp $

.PATH:	${.CURDIR}/../BIND/named \
	${.CURDIR}/../BIND/man

PROG=	named
SRCS=	version.c db_dump.c db_glue.c db_load.c db_lookup.c db_reload.c \
	db_save.c db_secure.c db_update.c ns_forw.c \
	ns_init.c ns_main.c ns_maint.c ns_ncache.c ns_req.c ns_resp.c \
	ns_sort.c ns_stats.c ns_validate.c storage.c tree.c
MAN8=	named.0

BINDIR=	/usr/sbin

CLEANFILES+= version.c

version.c: Version.c Makefile ${.CURDIR}/../Makefile.inc
	(u=$${USER-root} d=`pwd` h=`hostname` t=`date`; \
	sed -e "s|%WHEN%|$${t}|" -e "s|%VERSION%|"${VER}"|" \
	    -e "s|%WHOANDWHERE%|$${u}@$${h}:$${d}|" \
	    < ${.CURDIR}/../BIND/named/Version.c > version.c)

.include <bsd.prog.mk>
.include "${.CURDIR}/../Makefile.maninc"

.depend: version.c
