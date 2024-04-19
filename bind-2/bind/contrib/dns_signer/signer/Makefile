## Copyright (c) 1996,1997,1999 by Internet Software Consortium
##
## Permission to use, copy, modify, and distribute this software for any
## purpose with or without fee is hereby granted, provided that the above
## copyright notice and this permission notice appear in all copies.
##
## THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
## ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
## OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
## CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
## DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
## PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
## ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
## SOFTWARE.

# $Id: Makefile,v 1.1.1.1 1999/10/04 22:25:52 wsanchez Exp $

DESTDIR=
CC= cc
SHELL= /bin/sh

CDEBUG= -g -Wall

#(net2 and its descendents)
SYSTYPE = bsdos
TOP = ../../../src/
INCL =  ${TOP}/include
PORTINCL = ${TOP}/port/${SYSTYPE}/include
PARSEINCL = ../parse
LIBBIND = ${TOP}/lib/libbind.${A}
LIBPARSE = ../parse/libparse.${A}
A=a
O=o
LEX = lex -I
YACC = yacc -d
SYSLIBS = -ll -lutil
PIDDIR = /var/run
DESTBIN = /usr/local/bin
DESTSBIN = /usr/local/sbin
DESTEXEC = /usr/local/libexec
DESTMAN = /usr/share/man
DESTHELP= /usr/share/misc
AR= ar cruv
INSTALL= install
STRIP=-s

PS=ps
LDFLAGS=
CFLAGS=	${CDEBUG} -I${PORTINCL} -I${INCL} -I${PARSEINCL} ${DEFS}

PROG=	dnssigner
HDRS=	dns_signer.h signer_crypto.h signer_debug.h signer_directives.h \
	signer_ingest.h signer_key_monitor.h signer_ns_print.h \
	signer_parameters.h signer_sign.h signer_tree.h signer_write.h \
	tree.h
SRCS=	dns_signer.c signer_crypto.c signer_debug.c signer_directives.c \
	signer_ingest.c signer_key_monitor.c signer_ns_print.c \
	signer_parameters.c signer_sign.c signer_tree.c signer_write.c \
	tree.c
OBJS=	dns_signer.${O} signer_crypto.${O} signer_debug.${O} \
	signer_directives.${O} signer_ingest.${O} signer_key_monitor.${O} \
	signer_ns_print.${O} signer_parameters.${O} signer_sign.${O} \
	signer_tree.${O} signer_write.${O} tree.${O}

all: ${PROG}

${PROG}: ${OBJS} ${LIBPARSE} ${LIBBIND} Makefile
	${CC} ${CDEBUG} ${LDFLAGS} -o ${PROG} ${OBJS} \
		${LIBPARSE} ${LIBBIND} ${CRYPTLIB} ${SYSLIBS}

distclean: clean
	rm -f ns_parser.c ns_parser.h

clean: FRC
	rm -f ${PROG} ${OBJS} core .depend
	rm -f *.BAK *.CKP *~ *.orig

depend: ${SRCS}
	mkdep ${CPPFLAGS} -I${INCL} -I${PORTINCL} ${DEFS} ${SRCS}

${DESTDIR}${DESTSBIN}:
	mkdir -p ${DESTDIR}${DESTSBIN}

install: ${DESTDIR}${DESTSBIN} ${PROG}
	${INSTALL} ${STRIP} -c -m 755 ${PROG} ${DESTDIR}${DESTSBIN}/${PROG}

links: FRC
	@ln -s SRC/*.[chy] SRC/test .

tags: FRC
	ctags ${SRCS} *.h

FRC:

