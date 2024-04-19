char sccs_id[] = "@(#) poke_ns.c 1.10 99/01/05 @(#)";

/*
updatehosts DNS maintenance package
Copyright (C) 1998  Smoot Carl-Mitchell
 
This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

smoot@tic.com
*/

/*
 * simple front-end for sending signals to the bind process
 * run setuid to root with appropriate group permission
 * for your installation
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include "env.h"

#ifndef ENVFILE
#define	ENVFILE "/usr/local/etc/poke_ns.env"
#endif

struct {
	char *name;	/* command name */
	int sig;	/* signal to send to bind */
} commands[] = {
	"restart",	SIGTERM,
	"reload",	SIGHUP,
	"debug",	SIGUSR1,
	"nodebug",	SIGUSR2,
	"dump",		SIGINT,
	"terminate",	SIGTERM,
	NULL,		0
};

struct environment environment[] = {
	"PIDFILE", "/etc/named.pid",
	"NAMED",  "/usr/sbin/in.named",
	"DB_DUMP", "/var/tmp/named_dump.db",
	"DEBUG", "/var/tmp/named.run",
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
	char name[BUFSIZ], *value, *getenvval(), *strchr();

	if (argc != 2) {
		usage();
		exit(1);
	}

	/* match one of the commands */
	if ((cmd = lookup(argv[1])) == -1) {
		fprintf(stderr, "command %s not found\n", argv[1]);
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

	/* execute appropriate command */
	execute(cmd, pid);

	/* change ownership to real user of debugging files */
	uid = getuid();
	gid = getgid();
	chown(getenvval("DB_DUMP"), uid, gid);
	chown(getenvval("DEBUG"), uid, gid);
	exit(0);
}

void
usage()
{
	int i;

	fprintf(stderr, "usage: poke_ns ");
	for (i=0; commands[i].name != NULL; i++) {
		fprintf(stderr, commands[i].name);
		if (commands[i+1].name != NULL) {
			fprintf(stderr, " | ");
		}
		else {
			fprintf(stderr, "\n");
		}
	}
}
	
int
lookup(cmd)
char *cmd;
{
	int i;

	for (i=0; commands[i].name != NULL; i++) {
		if (strcmp(commands[i].name, cmd) == 0) {
			return i;
		}
	}
	return -1;
}

void
execute(cmd, pid)
int cmd;
int pid;
{
	int newpid, wstat;

	/* some sanity checking on pid */
	if (pid <= 2) {
		fprintf(stderr, "pid (%d) must be greater than 2\n", pid);
		exit(4);
	}

	/* send the signal to the process */
	if (kill(pid, commands[cmd].sig) == -1) {
		/* let restart work if no named process is running */
		if (!(cmd == 0 && errno == ESRCH)) {
			fprintf(stderr, "signal failed\n");
			exit(4);
		}
	}

	if (cmd != 0) {
		return;
	}
	/* special case of "restart" */

	/* wait and be sure process is dead */
	while (kill(pid, 0) != -1) {
		sleep(1);
	}
	/* restart named */
	newpid = fork();
	if (newpid == -1) {
		fprintf(stderr, "fork failed\n");
		exit(5);
	}
	if (newpid == 0) { /* child */
		execl(getenvval("NAMED"), getenvval("NAMED"), NULL);
		/* only if execl failed */
		fprintf(stderr, "execl failed\n");
		exit(5);
	}
	/* parent */
	wait (&wstat);
	if (WEXITSTATUS(wstat) != 0) {
		exit(5);
	}
	return;
}
