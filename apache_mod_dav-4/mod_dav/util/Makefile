#
# quickie Makefile to build the utilities
#

all: fixvers dbu lockview opendb testlock

fixvers: fixvers.c sdbm_combined.o
	gcc -Wall -o $@ fixvers.c -I.. sdbm_combined.o

dbu: dbu.c sdbm_combined.o
	gcc -Wall -o $@ dbu.c -I../sdbm sdbm_combined.o

lockview: lockview.c sdbm_combined.o
	gcc -Wall -o $@ lockview.c sdbm_combined.o

opendb: opendb.c sdbm_combined.o
	gcc -Wall -o $@ opendb.c -I.. sdbm_combined.o

testlock: testlock.c
	gcc -Wall -o $@ testlock.c

sdbm_combined.o:
	cd ..; make sdbm_combined.o && cp sdbm_combined.o util/
