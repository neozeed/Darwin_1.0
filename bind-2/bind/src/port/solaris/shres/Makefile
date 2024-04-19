# $Id: Makefile,v 1.1.1.1 1999/10/04 22:25:00 wsanchez Exp $

CFLAGS=	-O -I../${INCL} -I../${COMPINCL} ${DEFS} ${LOCDEFS} ${SOLCOMPAT}
#
# The addition of -Dsocket=_socket is needed here because the resolver shared
# library may be loaded by the name service switch in the context of a SunOS 4
# program running in compatibility mode.  In this case, socket is bound to the
# version in /usr/4lib/libc.so.[12].8, which uses the BSD values of SOCK_DGRAM
# and SOCK_STREAM, which are the reverse of the SVR4 values.  This causes real
# problems.  To avoid this, the resolver shared library must use _socket, which
# is only defined in /usr/lib/libsocket.so.
#
SOLCOMPAT = -Dgethostbyname=res_gethostbyname \
	-Dgethostbyaddr=res_gethostbyaddr -Dgetnetbyname=res_getnetbyname \
	-Dgetnetbyaddr=res_getnetbyaddr -Dsethostent=res_sethostent \
	-Dendhostent=res_endhostent -Dgethostent=res_gethostent \
	-Dsetnetent=res_setnetent -Dendnetent=res_endnetent \
	-Dgetnetent=res_getnetent -Dsocket=_socket
LOCDEFS= -DUSE_OPTIONS_H

OBJS=	herror.o res_debug.o res_data.o \
	res_comp.o res_init.o res_mkquery.o res_query.o res_send.o \
	getnetbyaddr.o getnetbyname.o getnetent.o getnetnamadr.o \
	gethnamaddr.o sethostent.o nsap_addr.o \
	inet_addr.o

all: libresolv.so

$(OBJS):
	${SHCC} ${CFLAGS} ${PIC} -D_REENTRANT -c $? -o $@

distclean: clean

clean: FRC
	rm -f errs a.out core libresolv.so tags *.o *.BAK *.CKP *~ *.orig

depend: 
	@echo No dependencies in `pwd`

install: ${DESTDIR}${DESTLIB}/libresolv.so.2

${DESTDIR}${DESTLIB}/libresolv.so.2: libresolv.so
	${INSTALL} -c -o bin -g bin -m 755 libresolv.so \
	 ${DESTDIR}${DESTLIB}/libresolv.so.2
	rm -f ${DESTDIR}${DESTLIB}/libresolv.so
	ln -s libresolv.so.2 ${DESTDIR}${DESTLIB}/libresolv.so

FRC:

libresolv.so: ${OBJS}
	LD_OPTIONS="-h libresolv.so.2" $(SHLD) -o $@ $(OBJS) -lsocket -lnsl

herror.o: ../../res/herror.c
res_comp.o: ../../res/res_comp.c
res_debug.o: ../../res/res_debug.c
res_data.o: ../../res/res_data.c
res_init.o: ../../res/res_init.c
res_mkquery.o: ../../res/res_mkquery.c
res_query.o: ../../res/res_query.c
res_send.o: ../../res/res_send.c
getnetbyaddr.o: ../../res/getnetbyaddr.c
getnetbyname.o: ../../res/getnetbyname.c
getnetent.o: ../../res/getnetent.c
getnetnamadr.o: ../../res/getnetnamadr.c
gethnamaddr.o: ../../res/gethnamaddr.c
sethostent.o: ../../res/sethostent.c
nsap_addr.o: ../../res/nsap_addr.c
inet_addr.o: ../../res/inet_addr.c
