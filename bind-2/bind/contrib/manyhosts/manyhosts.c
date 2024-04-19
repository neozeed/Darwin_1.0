/*
 * manyhosts - thread-aware program to convert lots of addresses to hostnames
 * vix 12sep98 [wrote, during altavista contract]
 */

#ifndef lint
static const char rcsid[] = "$Id: manyhosts.c,v 1.1.1.1 1999/10/04 22:25:54 wsanchez Exp $";
#endif

#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>

#include <irs.h>
#include <netdb.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define P(x) if (!(x)) { perror(#x); exit(1); } else (void)0

static void *run(void *);

static int running = 0;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

main(int argc, char **argv) {
	int n;

	if (argc != 2) {
		fprintf(stderr, "usage: %s #threads\n", argv[0]);
		exit(1);
	}
	n = atoi(argv[1]);
	if (n < 1 || n > 65535) {
		fprintf(stderr, "%s: \"%s\" seems an unlikely #threads\n",
			argv[0], argv[1]);
		exit(1);
	}
	setlinebuf(stdout);
	if (getenv("RES_OPTIONS") == NULL)
		putenv("RES_OPTIONS=rotate attempts:1 timeout:10");
	P(pthread_mutex_lock(&mutex) == 0);
	while (n-- > 0) {
		pthread_t t;

		P(pthread_create(&t, NULL, run, NULL) == 0);
		P(pthread_detach(t) == 0);
		running++;
	}
	P(pthread_cond_wait(&cond, &mutex) == 0);
	exit(0);
}

static void *
run(void *uap) /*UNUSED*/ {
	struct irs_acc *irs_dns;
	struct irs_ho *irs_ho;
	char line[20];

	/*
	 * Instantiate an IRS DNS map class and host map object, which will
	 * (as of BIND 8.1.3) have its own private DNS resolver context.
	 */
	irs_dns = irs_dns_acc("");
	P(irs_dns != NULL);
	irs_ho = (*irs_dns->ho_map)(irs_dns);
	P(irs_ho != NULL);
	/*
	 * Read host addresses until EOF.
	 */
	while (fgets(line, sizeof line, stdin)) {	/* rendezvous point */
		char *nl = strchr(line, '\n');
		u_char addr[NS_INADDRSZ];
		struct hostent *hp;

		if (nl != NULL)
			*nl = '\0';
		if (inet_pton(AF_INET, line, addr) < 1) {
			printf("%s: _bad\n", line);
			continue;
		}
		hp = (*irs_ho->byaddr)(irs_ho, addr, NS_INADDRSZ, AF_INET);
		if (hp == NULL) {
			printf("%s: _not\n", line);	/* XXX reason? */
			continue;
		}
		printf("%s: %s\n", line, hp->h_name);
	}
	(*irs_ho->close)(irs_ho);
	(*irs_dns->close)(irs_dns);
	P(pthread_mutex_lock(&mutex) == 0);
	if (--running == 0)
		P(pthread_cond_signal(&cond) == 0);
	P(pthread_mutex_unlock(&mutex) == 0);
	return (NULL);
}
