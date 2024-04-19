CC=gcc
CFLAGS=-O0 -g -Wall
LIBS=-lresolv 
# For Solaris:
#LIBS=-lresolv -lnsl
DESTDIR=/usr/local
BINDIR=$(DESTDIR)/bin
DISTRIB= README INSTALL ALGO USAGE Makefile loc.h loc.c query-loc.c
VERSION=`grep VERSION loc.h | cut -d ' ' -f 3 | sed s/\"//g`

all: query-loc

query-loc: query-loc.o loc.o
	$(CC) -o $@ $^ $(LIBS)

query-loc.o: loc.h query-loc.c
	$(CC) $(CFLAGS) -c query-loc.c 

loc.o: loc.h loc.c
	$(CC) $(CFLAGS) -c loc.c 

clean:
	-rm -f *.o query-loc *~ 

distrib: clean
	@(echo Query-Loc is version ${VERSION}; \
	mkdir query-loc-${VERSION}; \
	cp $(DISTRIB) query-loc-${VERSION};\
	tar cvf query-loc-${VERSION}.tar query-loc-${VERSION}; \
	rm -rf query-loc-${VERSION}; \
	gzip -v -9 -f query-loc-${VERSION}.tar);

install:
	install -m 0755 query-loc $(BINDIR)
