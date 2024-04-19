#!/bin/make
#Makefile,v 1.1.1.1 1995/12/08 03:45:02 kim Exp
SHELL=	/bin/sh
#
CMD=		lamers
#
DESTDIR=	/usr/local/etc
LOGFILE=	/var/log/daemon
TEMPDIR=	/var/tmp
ADMIN=		root
MAILER=		/usr/lib/sendmail -f nobody -t
SENDER=		hostmaster@gw.com

all: ${CMD}

.sh:
	sed \
	    -e "s|@CMD@|${CMD}|" \
	    -e "s|@DESTDIR@|${DESTDIR}|" \
	    -e "s|@LOGFILE@|${LOGFILE}|" \
	    -e "s|@TEMPDIR@|${TEMPDIR}|" \
	    -e "s|@ADMIN@|${ADMIN}|" \
	    -e "s|@MAILER@|${MAILER}|" \
	    -e "s|@SENDER@|${SENDER}|" \
	    < $< > $@
	chmod +x $@

install: all
	install -c -m 755 ${CMD} $(DESTDIR)

clean:
	-rm -f ${CMD} *~ \#* core
