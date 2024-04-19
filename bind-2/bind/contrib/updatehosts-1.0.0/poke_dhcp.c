char sccs_id[] = "@(#) poke_dhcp.c 1.4 99/01/05 @(#)";

/*
 * send a TERM signal to the dhcpd process and restart it
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include "env.h"

#ifndef ENVFILE
#define	ENVFILE "/usr/local/etc/poke_dhcp.env"
#endif

struct environment environment[] = {
	"PIDFILE", "/etc/dhcpd.pid",
	"DHCPD",  "/usr/sbin/dhcpd",
	NULL, NULL
};

main(argc, argv)
int argc;
char **argv;
{
	FILE *fd;
	int cmd, pid, err, n;
	struct stat status;
	void usage(), execute();
	int lookup(), changeenv();
	int uid, gid;
	char name[BUFSIZ], *value, *strchr(), *getenvval();

	if (argc != 1) {
		usage();
		exit(1);
	}

	/* read in the environment, but check the permissions on the file */
	if (!safe(ENVFILE)) {
		exit(2);
	}
	if ((fd = fopen(ENVFILE, "r")) == NULL) {
		fprintf(stderr, "%s cannot be read\n", ENVFILE);
		exit(3);
	}

	err = 0;
	while (fgets(name, BUFSIZ, fd) != NULL) {
		name[strlen(name)-1] = '\0';
		value = strchr(name, '=');
		if (value == NULL) continue;
		*value = '\0';
		value++;
		if (!changeenv(name, value)) {
			fprintf(stderr, "bad environment value - %s\n", value);
			err = 1;
		}
	}
	if (err) exit(1);

	/* check permissions on /etc/named.pid */
	if (!safe(getenvval("PIDFILE"))) {
		exit(2);
	}

	/* if it is safe - then read pid */
	if ((fd = fopen(getenvval("PIDFILE"), "r")) == NULL) {
		fprintf(stderr, "%s cannot be read\n", getenvval("PIDFILE"));
		exit(3);
	}
	if (fscanf(fd, "%d", &pid) != 1) {
		fprintf(stderr, "%s does not contain an integer\n", getenvval("PIDFILE"));
		exit(3);
	}

	/* kill and restart the server */
	kill(pid, SIGTERM);
	execl(getenvval("DHCPD"), "dhcpd", 0);
}

void
usage()
{
	int i;

	fprintf(stderr, "usage: poke_dhcp ");
}
