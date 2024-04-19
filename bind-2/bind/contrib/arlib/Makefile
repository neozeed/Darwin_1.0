#
# (C)1992, 1994, 1995 by Darren Reed.
# darrenr@cyber.com.au
#
CC=cc
RM=/bin/rm
MV=/bin/mv
CP=/bin/cp
CHMOD=/bin/chmod
CFLAGS=-g -I../../include -I../../compat/include
LIBDIR=/usr/local/lib
MANDIR=/usr/local/man
RES=-L../../res/ -lresolv #-l44bsd

all: libares.a example

example: sample.o libares.a
	$(CC) $(CFLAGS) sample.o -o example -L. -lares $(RES)

libares.a: arlib.o
	ld -x -r arlib.o
	$(MV) arlib.o arlib.o.tmp
	$(MV) a.out arlib.o
	$(MV) arlib.o.tmp arlib.o
	ar crv libares.a arlib.o
	ranlib libares.a

arlib.o: arlib.c arlib.h
	$(CC) $(CFLAGS) -c arlib.c

install:
	$(RM) -f $(MANDIR)/man3/arlib.3
	$(CP) arlib.3 $(MANDIR)/man3/arlib.3
	$(CHMOD) 444 $(MANDIR)/man3/arlib.3
	$(RM) -f $(LIBDIR)/libares.a
	$(CP) libares.a $(LIBDIR)
	$(CHMOD) 444 $(LIBDIR)/libares.a

clean:
	$(RM) -f example *.o core *~ *.a #*
