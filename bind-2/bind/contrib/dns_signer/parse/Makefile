# Copyright (c) 1996,1999 by Internet Software Consortium
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
# ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
# CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
# DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
# PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
# ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
# SOFTWARE.

# $Id: Makefile,v 1.1.1.1 1999/10/04 22:25:52 wsanchez Exp $

# these are only appropriate for BSD 4.4 or derivatives, and are used in
# development.  normal builds will be done in the top level directory and
# this Makefile will be invoked with a lot of overrides for the following:
SYSTYPE= bsdos
DESTDIR =
DESTLIB = /usr/local/lib
O=o
A=a
CC= cc
LD= ld
SHELL= /bin/sh
CDEBUG= -g
TOP= ../../../src/
INCL = ${TOP}/include
PORTINCL = ${TOP}/port/${SYSTYPE}/include
LIBBIND = ${TOP}/lib/libbind.${A}
LIBPARSE = libparse.${A}
CFLAGS=	${CDEBUG} -I${PORTINCL} -I${INCL}
LD_LIBFLAGS= -x -r
AR= ar cruv
RANLIB= ranlib
INSTALL= install

SORTER_DIR = `pwd`

SRCS=	parse_get_token.c master_parse.c parse_record.c \
	parse_error_message.c parse_rdata.c parse_number.c \
	parse_otherstuff.c WKS_n_RFC1700.c \
	iip_support.c dns_support.c bind_support.c

OBJS=	parse_get_token.${O} master_parse.${O} parse_record.${O} \
	parse_error_message.${O} parse_rdata.${O} parse_number.${O} \
	parse_otherstuff.${O} WKS_n_RFC1700.${O} \
	iip_support.${O} dns_support.${O} bind_support.${O}

all: ${LIBPARSE}

${LIBPARSE}: ${OBJS}
	rm -f ${LIBPARSE}
	${AR} ${LIBPARSE} ${ARPREF} ${OBJS} ${ARSUFF}
	${RANLIB} ${LIBPARSE}

.c.${O}:
	${CC} ${CPPFLAGS} ${CFLAGS} -DSORTER_DIR=\"${SORTER_DIR}\" -c $*.c
	-${LDS} ${LD} ${LD_LIBFLAGS} $*.${O} && ${LDS} mv a.out $*.${O}

distclean: clean

clean: FRC
	rm -f .depend a.out core ${LIBPARSE} tags
	rm -f *.${O} *.BAK *.CKP *~

depend: FRC
	mkdep -I${INCL} -I${PORTINCL} ${CPPFLAGS} ${SRCS}

links: FRC
	@set -e; ln -s SRC/*.[ch] .

install:

FRC:

# DO NOT DELETE THIS LINE -- mkdep uses it.
# DO NOT PUT ANYTHING AFTER THIS LINE, IT WILL GO AWAY.
