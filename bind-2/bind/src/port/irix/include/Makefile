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

SUBDIRS = sys
HFILES =

DESTDIR=
DESTINC= /usr/local/bind/include
INSTALL= install

MARGS= DESTDIR="${DESTDIR}" DESTINC="${DESTINC}" INSTALL="${INSTALL}"

all depend clean distclean install::
	@for x in ${SUBDIRS}; do \
		(cd $$x; pwd; ${MAKE} ${MARGS} $@); \
	done

distclean:: clean

clean::
	rm -f *~ *.BAK *.CKP *.orig

links: FRC
	@set -e; ln -s SRC/*.h .
	@set -e; for x in ${SUBDIRS}; do \
		( mkdir $$x; cd $$x; pwd; ln -s ../SRC/$$x SRC; \
		cp SRC/Makefile Makefile; chmod +w Makefile; \
		${MAKE} ${MARGS} links ); \
	done

install:: ${DESTDIR}${DESTINC}
	-for x in "" ${HFILES}; do \
	    if [ -n "$$x" ]; then \
		${INSTALL} -c -m 444 $$x ${DESTDIR}${DESTINC}/$$x; \
	    fi; \
	done

${DESTDIR}${DESTINC}:
	mkdir -p ${DESTDIR}${DESTINC}

FRC:
