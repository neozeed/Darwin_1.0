# This is a generic makefile for a UNIX system (SUN412) make utility. 
# Target libbsafe2.a

SYSTYPE= bsdos
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
LIBBINDR = ../${TOP}/lib/libbind_r.${A}
CFLAGS=	${CDEBUG}
CPPFLAGS= -I${PORTINCL} -I${INCL}
LD_LIBFLAGS= -x -r
AR= ar cruv
RANLIB= ranlib
INSTALL= install
INSTALL_EXEC=
INSTALL_LIB=-o bin -g bin
THREADED= threaded
# BSAFE2 source directories.

# Define PROTOTYPES=0 in CFLAGS if the target environment does not
# support function prototyping.

#BSAFE2LIB = libbsafe2.a

SRCS=	bgclrbit.c bgmdmpyx.c bgmdsqx.c bgmodexp.c\
	bgpegcd.c big2exp.c bigabs.c bigacc.c bigarith.c\
	bigcmp.c bigconst.c biginv.c biglen.c bigmodx.c\
	bigmpy.c bigpdiv.c bigpmpy.c bigpmpyh.c bigpmpyl.c\
	bigpsq.c bigqrx.c bigsmod.c bigtocan.c bigu.c\
	bigunexp.c cantobig.c crt2.c \
	digrand.c intbits.c md5.c md5rand.c prime.c\
	rsa.c rsakeygn.c seccbcd.c seccbce.c surrendr.c\
	ahchdig.c ahchencr.c ahchgen.c ahchrand.c ahdigest.c\
	ahencryp.c ahgen.c ahrandom.c ahrsaenc.c ahrsaepr.c\
	ahrsaepu.c aichdig.c aichenc8.c aichencn.c aichencr.c\
	aichgen.c aichrand.c aimd5.c\
	aimd5ran.c ainfotyp.c ainull.c airsaepr.c airsaepu.c\
	airsakgn.c airsaprv.c airsapub.c algchoic.c algobj.c\
	amcrte.c ammd5.c ammd5r.c\
	amrkg.c amrsae.c balg.c binfocsh.c bkey.c bmempool.c\
	digest.c encrypt.c generate.c intitem.c\
	keyobj.c ki8byte.c kiitem.c kinfotyp.c\
	kifulprv.c kipkcrpr.c kirsacrt.c kirsapub.c random.c

OBJS=	bgclrbit.$(O) bgmdmpyx.$(O) bgmdsqx.$(O) bgmodexp.$(O)\
	bgpegcd.$(O) big2exp.$(O) bigabs.$(O) bigacc.$(O) bigarith.$(O)\
	bigcmp.$(O) bigconst.$(O) biginv.$(O) biglen.$(O) bigmodx.$(O)\
	bigmpy.$(O) bigpdiv.$(O) bigpmpy.$(O) bigpmpyh.$(O) bigpmpyl.$(O)\
	bigpsq.$(O) bigqrx.$(O) bigsmod.$(O) bigtocan.$(O) bigu.$(O)\
	bigunexp.$(O) cantobig.$(O) crt2.$(O) \
	digrand.$(O) intbits.$(O) md5.$(O) md5rand.$(O) prime.$(O)\
	rsa.$(O) rsakeygn.$(O) seccbcd.$(O) seccbce.$(O) surrendr.$(O)\
	ahchdig.$(O) ahchencr.$(O) ahchgen.$(O) ahchrand.$(O) ahdigest.$(O)\
	ahencryp.$(O) ahgen.$(O) ahrandom.$(O) ahrsaenc.$(O) ahrsaepr.$(O)\
	ahrsaepu.$(O) aichdig.$(O) aichenc8.$(O) aichencn.$(O) aichencr.$(O)\
	aichgen.$(O) aichrand.$(O) aimd5.$(O)\
	aimd5ran.$(O) ainfotyp.$(O) ainull.$(O) airsaepr.$(O) airsaepu.$(O)\
	airsakgn.$(O) airsaprv.$(O) airsapub.$(O) algchoic.$(O) algobj.$(O)\
	amcrte.$(O) ammd5.$(O) ammd5r.$(O)\
	amrkg.$(O) amrsae.$(O) balg.$(O) binfocsh.$(O) bkey.$(O) bmempool.$(O)\
	digest.$(O) encrypt.$(O) generate.$(O) intitem.$(O)\
	keyobj.$(O) ki8byte.$(O) kiitem.$(O) kinfotyp.$(O)\
	kifulprv.$(O) kipkcrpr.$(O) kirsacrt.$(O) kirsapub.$(O) random.$(O)

all: ${LIBBIND}

${LIBBIND}: ${OBJS}
	( cd ${THREADED} ; \
	${AR} ${LIBBINDR} ${ARPREF} ${OBJS} ${ARSUFF} ; \
	${RANLIB} ${LIBBINDR} )
	${AR} ${LIBBIND} ${ARPREF} ${OBJS} ${ARSUFF}
	${RANLIB} ${LIBBIND}


.c.${O}:
	if test ! -d ${THREADED} ; then mkdir ${THREADED} ; fi
	${CC} ${CPPFLAGS} ${CFLAGS} ${BOUNDS} ${REENTRANT} -c $*.c \
	 -o ${THREADED}/$*.${O}
	-${LDS} ${LD} ${LD_LIBFLAGS} ${THREADED}/$*.${O} && \
	 ${LDS} mv a.out ${THREADED}/$*.${O}
	${CC} ${CPPFLAGS} ${CFLAGS} -c $*.c
	-${LDS} ${LD} ${LD_LIBFLAGS} $*.${O} && ${LDS} mv a.out $*.${O}

distclean: clean

clean: FRC
	rm -f .depend a.out core ${LIB} tags
	rm -f *.${O} *.BAK *.CKP *~
	rm -f ${THREADED}/*.${O}
	-rmdir ${THREADED}

depend: FRC
	mkdep -I${INCL} -I${PORTINCL} ${CPPFLAGS} ${SRCS}

links: FRC
	@set -e; ln -s SRC/*.[ch] .

install:

FRC:


#$(BSAFE2LIB): $(BSAFE_OBJS) $(ALGAE_OBJS)
#	ar r $@ $?
#	ranlib $@

# DO NOT DELETE THIS LINE -- mkdep uses it.
# DO NOT PUT ANYTHING AFTER THIS LINE, IT WILL GO AWAY.




