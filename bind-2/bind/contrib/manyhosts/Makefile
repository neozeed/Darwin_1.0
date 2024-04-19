CDEBUG= -g -pthread
CDEFS=
CFLAGS= $(CDEBUG) $(CDEFS) -I/usr/local/bind/include
LDFLAGS= -L/usr/local/bind/lib
LIBS= -lbind -lpthread -lexc
DEPLIBS= /usr/local/bind/lib/libbind.a

ALL= manyhosts

all: $(ALL)

kit:; shar Makefile manyhosts.c > kit

clean:
	rm -f $(ALL)
	rm -f *.o *.BAK *.CKP *~

MANYHOSTS_O= manyhosts.o

manyhosts: $(MANYHOSTS_O) $(DEPLIBS)
	$(CC) $(LDFLAGS) -o manyhosts $(MANYHOSTS_O) $(LIBS)
