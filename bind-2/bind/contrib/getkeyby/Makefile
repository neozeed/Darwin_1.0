# Truly simple Makefile for getkeyby() subroutine and getkey test program
#
all:  getkey
getkeyby.o:  getkeyby.c
parseresp.o:  parseresp.c
getkey.o:  getkey.c

# On some systems, you may have to link with -lresolv.  In other systems
# the DNS name resolver is in libc already.
#LIBS=-lresolv
LIBS=

getkey: getkey.o getkeyby.o parseresp.o
	$(CC) $(CFLAGS) -o getkey getkey.o getkeyby.o parseresp.o $(LIBS)

clean:
	rm -f *.o getkey
