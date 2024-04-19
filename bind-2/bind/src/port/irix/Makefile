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

# $Id: Makefile,v 1.1.1.1 1999/10/04 22:24:55 wsanchez Exp $

# these are only appropriate for BSD 4.4 or derivatives, and are used in
# development.  normal builds will be done in the top level directory and
# this Makefile will be invoked with a lot of overrides for the following:
SYSTYPE= bsd
DESTDIR =
DESTLIB = /usr/local/lib
O=o
A=a
CC= cc
LD= ld
SHELL= /bin/sh
CDEBUG= -g
TOP= ../..
INCL = ${TOP}/include
PORTINCL = ${TOP}/port/${SYSTYPE}/include
LIBBIND = ${TOP}/lib/libbind.${A}
CFLAGS=	${CDEBUG} -I${PORTINCL} -I${INCL}
LD_LIBFLAGS= -x -r
AR= ar cru
RANLIB= :
INSTALL= install

SUBDIRS= include

SRCS=	noop.c

OBJS=	noop.${O}

MARGS= DESTDIR="${DESTDIR}" DESTINC="${DESTINC}" INSTALL="${INSTALL}"

depend::
	if [ -f "irix_patch_applied" ]; then\
		echo "IRIX patches are already in-place.";\
	else\
		echo "Applying IRIX patches.";\
		patch -cfN -d ../../ < irix_patch;\
		touch irix_patch_applied;\
	fi

all depend clean distclean install::
	@for x in ${SUBDIRS}; do \
		(cd $$x; pwd; ${MAKE} ${MARGS} $@); \
	done

all:: depend ${LIBBIND}

${LIBBIND}: ${OBJS}
	${AR} ${LIBBIND} ${ARPREF} ${OBJS} ${ARSUFF}
	${RANLIB} ${LIBBIND}

.c.${O}:
	${CC} ${CPPFLAGS} ${CFLAGS} -c $*.c
	-${LDS} ${LD} ${LD_LIBFLAGS} $*.${O} && ${LDS} mv a.out $*.${O}

distclean:: clean

clean:: FRC
	rm -f .depend a.out core tags
	rm -f *.${O} *.BAK *.CKP *~
	if [ -f "irix_patch_applied" ]; then\
		echo "Reversing IRIX patches...";\
		patch -cfR -d ../../ < irix_patch;\
		rm -fr irix_patch_applied;\
	else\
		echo "IRIX aren't in-place";\
	fi

depend:: FRC
	mkdep -I${INCL} -I${PORTINCL} ${CPPFLAGS} ${SRCS}

links: FRC
	@set -e; ln -s SRC/*.[ch] SRC/bin SRC/Makefile.set SRC/probe .
	@set -e; for x in ${SUBDIRS}; do \
		( mkdir $$x; cd $$x; pwd; ln -s ../SRC/$$x SRC; \
		cp SRC/Makefile Makefile; chmod +w Makefile; \
		${MAKE} ${MARGS} links ); \
	done

install:: FRC

FRC:

# DO NOT DELETE THIS LINE -- mkdep uses it.
# DO NOT PUT ANYTHING AFTER THIS LINE, IT WILL GO AWAY.
