CFLAGS = -Wall -W -g

all: srvtest

srvtest: main.o srv.o Makefile
	$(CC) -g -o $@ main.o srv.o

srv.c: srv.h Makefile

main.c: srv.h Makefile
