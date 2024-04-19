
CC = gcc
LIBS = -lpcap
CFLAGS = -I. -O2
OBJ = snoof.o

.c.o:
	$(CC) $(CFLAGS) -c $*.c

snoof: $(OBJ)
	$(CC) -s -o $@ $(OBJ) $(LIBS)
