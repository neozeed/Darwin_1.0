#define VERSION "0.0.1"

/* Probably too many inclusions but this is to keep 'gcc -Wall' happy... */
#include        <stdio.h>
#include        <stdlib.h>
#include        <sys/types.h>
#include        <netdb.h>
#include        <sys/socket.h>
#include        <netinet/in.h>
#include        <arpa/inet.h>
#include        <varargs.h>
#include        <sys/time.h>
#include        <errno.h>
#include        <unistd.h>
#include        <string.h>
#include        <signal.h>
#include <arpa/nameser.h>
#include <resolv.h>

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif

#ifndef _DB_H_
#ifdef __alpha__
#define u_int32_t unsigned int 
#else
#define u_int32_t unsigned long 
#endif
#endif

/* For solaris */
#ifndef INADDR_NONE
#define INADDR_NONE (in_addr_t)-1
#endif

struct list_in_addr
  {
    struct in_addr addr;
    void *next;
  };

void usage ();
void panic ();

char *getlocbyname ();
char *getlocbyaddr ();
char *getlocbynet ();
char *findRR ();
struct list_in_addr *findA ();

extern char *progname;
extern short debug;
