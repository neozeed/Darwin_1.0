/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 * lookupd.m
 *
 * lookupd is a proxy server for all local and network information and
 * directory services.  It is called by various routines in the System
 * framework (e.g. gethostbyname()).  Using (configurable) search 
 * policies for each category of item (e.g. users, printers), lookupd
 * queries information services on behalf of the calling client.
 * Caching and negative record machanisms are used to improve ovarall
 * system performance.
 *
 * Copyright (c) 1995, NeXT Computer Inc.
 * All rights reserved.
 *
 * Designed and written by Marc Majka
 */

#import <config.h>
#import <project_version.h>
#import <objc/objc-runtime.h>
#import <stdio.h>
#import "Syslog.h"
#import "Controller.h"
#import "Thread.h"
#import "SignalLock.h"
#import "MemoryWatchdog.h"
#import "sys.h"
#import <sys/file.h>
#import <sys/types.h>
#import <sys/ioctl.h>
#import <sys/resource.h>
#import <sys/signal.h>
#import <sys/wait.h>
#import <sys/syslog.h>
#import <unistd.h>
#import <sys/time.h>
#import <sys/resource.h>
#import <signal.h>
#import <project_version.h>

#define forever for (;;)

extern sys_port_type _lookupd_port(sys_port_type);
extern int getppid(void);
extern void interactive(FILE *, FILE*);


#ifdef _UNIX_BSD_43_
#define PID_FILE "/etc/lookupd.pid"
#define EXE_FILE "/usr/etc/lookupd"
#else
#define PID_FILE "/var/run/lookupd.pid"
#define EXE_FILE "/usr/sbin/lookupd"
#endif

SignalLock *restartLock = nil;

BOOL debugMode;

/*
 * GLOBALS - see LUGlobal.h
 */
id controller = nil;
id lookupLog = nil;
id machRPC = nil;
id rover = nil;
char *portName = NULL;
sys_port_type server_port = SYS_PORT_NULL;
#ifdef _SHADOW_
sys_port_type server_port_privileged = SYS_PORT_NULL;
sys_port_type server_port_unprivileged = SYS_PORT_NULL;
BOOL shadow_passwords = NO;
#endif

/* Controller.m uses this global */
BOOL restartLookupd = NO;

int
lookupd_running(char *name)
{
	sys_port_type p;

	if (!strcmp(name, DefaultName))
	{
		p = _lookupd_port(0);
		if (p == SYS_PORT_NULL) return 0;
		return 1;
	}

	return sys_server_running(name);
}

static void
writepid(void)
{
	FILE *fp;

	fp = fopen(PID_FILE, "w");
	if (fp != NULL)
	{
		fprintf(fp, "%d\n", getpid());
		fclose(fp);
	}
}

static void
detach(void)
{
	int i;
#ifdef _UNIX_BSD_43_
	int ttyfd;
#endif

	signal(SIGINT, SIG_IGN);
	signal(SIGPIPE, SIG_IGN);

	for (i = getdtablesize() - 1; i <= 0; i--) close(i);

	open("/dev/null", O_RDWR, 0);
	dup(0);
	dup(0);

#ifdef _UNIX_BSD_43_
	ttyfd = open("/dev/tty", O_RDWR, 0);
	if (ttyfd > 0)
	{
		ioctl(ttyfd, TIOCNOTTY, NULL);
		close(ttyfd);
	}

	setpgrp(0, getpid());
#else
	if (setsid() < 0) syslog(LOG_ERR, "lookupd: setsid() failed: %m");
#endif
}

void
parentexit(int x)
{
	exit(0);
}

void
goodbye(int x)
{
	exit(1);
}

void
handleSIGHUP()
{
	[lookupLog syslogError:"Caught SIGHUP - restarting"];
	restartLookupd = YES;
	[[Thread currentThread] yield];
	[restartLock signal];
}

/*
 * Restart everything.
 */
void
restart()
{
#ifdef _SHADOW_
	char *Argv[7], portstr2[32];
#else
	char *Argv[5];
#endif
	char pidstr[32], portstr1[32];
	int pid;

	if (debugMode)
	{
		fprintf(stderr, "Caught SIGHUP - exiting\n");
		
		[controller release];
		[rover release];
		exit(0);
	}

	[lookupLog syslogNotice:"Restarting lookupd"];

#ifdef _SHADOW_
	sprintf(pidstr,  "%d", getpid());
	sprintf(portstr1, "%d", server_port_unprivileged);
	sprintf(portstr2, "%d", server_port_privileged);

	Argv[0] = "lookupd";
	Argv[1] = "-r";
	Argv[2] = portstr1;
	Argv[3] = portstr2;
	Argv[4] = pidstr;
	Argv[5] = shadow_passwords ? NULL : "-u";
	Argv[6] = NULL;
#else
	sprintf(pidstr,  "%d", getpid());
	sprintf(portstr1, "%d", server_port);

	Argv[0] = "lookupd";
	Argv[1] = "-r";
	Argv[2] = portstr1;
	Argv[3] = pidstr;
	Argv[4] = NULL;
#endif

	pid = fork();
	if (pid > 0)
	{
		signal(SIGTERM, parentexit);
		forever [[Thread currentThread] sleep:1];
	}

	execv(EXE_FILE, Argv);
}

int
main(int argc, char *argv[])
{
	int i, len, pid;
	BOOL restarting;
	BOOL customName;
	BOOL printFinalStats;
	char *configDir;
	struct rlimit rlim;
	LUArray *sList;
	LUDictionary *stats;
	Thread *t;
	sys_task_port_type old_lu;
#ifdef _SHADOW_
	sys_port_type old_port_privileged;
	sys_port_type old_port_unprivileged;
#else
	sys_port_type old_port;
#endif

	/* Clean up and re-initialize state on SIGHUP */
	signal(SIGHUP, handleSIGHUP);

	objc_setMultithreaded(YES);

	t = [Thread currentThread];
	[t setState:ThreadStateActive];

	server_port = SYS_PORT_NULL;
#ifdef _SHADOW_
	server_port_unprivileged = SYS_PORT_NULL;
	old_port_unprivileged = SYS_PORT_NULL;
	server_port_privileged = SYS_PORT_NULL;
	old_port_privileged = SYS_PORT_NULL;
	shadow_passwords = YES;
#else
	old_port = SYS_PORT_NULL;
#endif

	pid = -1;
	restarting = NO;
	debugMode = NO;
	customName = NO;
	printFinalStats = NO;
	configDir = NULL;
	portName = DefaultName;

	for (i = 1; i < argc; i++)
	{
		if (!strcmp(argv[i], "-r"))
		{
			if (((argc - i) - 1) < 2) 
			{
#ifdef _SHADOW_
				fprintf(stderr,"usage: lookupd -r unprivport privport pid\n");
#else
				fprintf(stderr,"usage: lookupd -r port pid\n");
#endif
				exit(1);
			}

			restarting = YES;

#ifdef _SHADOW_
			old_port_unprivileged = (sys_port_type)atoi(argv[++i]);
			old_port_privileged = (sys_port_type)atoi(argv[++i]);
#else
			old_port = (sys_port_type)atoi(argv[++i]);
#endif
			pid = atoi(argv[++i]);
		}

		else if (!strcmp(argv[i], "-d"))
		{
			debugMode = YES;
			portName = DebugName;
		}

		else if (!strcmp(argv[i], "-D"))
		{
			debugMode = YES;
			customName = YES;
			if (((argc - i) - 1) < 1) 
			{
				fprintf(stderr,"usage: lookupd -D name\n");
				exit(1);
			}
			portName = argv[++i];
		}

		else if (!strcmp(argv[i], "-s")) printFinalStats = YES;

#ifdef _SHADOW_
		else if (!strcmp(argv[i], "-u")) shadow_passwords = NO;
#endif

		else
		{
			fprintf(stderr, "Unknown option: %s\n", argv[i]);
			exit(1);
		}
	}

	if (restarting && debugMode)
	{
		fprintf(stderr, "Can't restart in debug mode\n");
		exit(1);
	}

	if ((!restarting) && lookupd_running(portName))
	{
		if (debugMode)
		{
			if (customName)
			{
				fprintf(stderr, "lookupd -D %s is already running!\n",
					portName);
			}
			else
			{
				fprintf(stderr, "lookupd -d is already running!\n");
			}
		}
		else
		{
			fprintf(stderr, "lookupd is already running!\n");
			syslog(LOG_ERR, "lookupd is already running!\n");
		}
		exit(1);
	}

	rover = [[MemoryWatchdog alloc] init];

	if (debugMode)
	{
		controller = [[Controller alloc] initWithName:portName];
		if (controller == nil)
		{
			fprintf(stderr, "controller didn't init!\n");
			exit(1);
		}

		printf("lookupd version %s\n", _PROJECT_VERSION_);
		printf("Debug mode\n");
		interactive(stdin, stdout);

		if (printFinalStats)
		{
			sList = [controller allStatistics];
			len = [sList count];
			for (i = 0; i < len; i++)
			{
				stats = [sList objectAtIndex:i];
				if (stats != nil) [stats print:stdout];
			}
			[sList release];
		}

		[controller release];
		[rover release];
		exit(0);
	}

	if (restarting)
	{
		if (sys_task_for_pid(sys_task_self(), pid, &old_lu) != KERN_SUCCESS)
		{
			syslog(LOG_EMERG, "Can't get port for PID %d", pid);
			exit(1);
		}
#ifdef _SHADOW_
		if (sys_port_extract_receive_right(old_lu, old_port_unprivileged, &server_port_unprivileged)
			!= KERN_SUCCESS || 
		    sys_port_extract_receive_right(old_lu, old_port_privileged, &server_port_privileged)
			!= KERN_SUCCESS || 
		    port_set_allocate(task_self(), &server_port)
			!= KERN_SUCCESS || 
		    port_set_add(task_self(), server_port, server_port_unprivileged)
			!= KERN_SUCCESS || 
		    port_set_add(task_self(), server_port, server_port_privileged)
			!= KERN_SUCCESS)
#else
		if (sys_port_extract_receive_right(old_lu, old_port, &server_port)
			!= KERN_SUCCESS)
#endif
		{
			syslog(LOG_EMERG, "Can't grab port rights");
			kill(pid, SIGKILL);
			exit(1);
		}
	}
	else
	{
		pid = fork();
		if (pid > 0)
		{
			signal(SIGTERM, parentexit);
			forever sleep(1);
		}

		detach();
	}

	if (!debugMode) writepid();

	rlim.rlim_cur = rlim.rlim_max = RLIM_INFINITY;
	setrlimit(RLIMIT_CORE, &rlim);
	signal(SIGTERM, goodbye);

	controller = [[Controller alloc] initWithName:portName];
	if (controller == nil)
	{
		[lookupLog syslogEmergency:"controller didn't init!"];
		kill(getppid(), SIGTERM);
		exit(1);
	}

	restartLock = [[SignalLock alloc] init];
	kill(getppid(), SIGTERM);

	[t setState:ThreadStateIdle];
    [restartLock lock];
	while (!restartLookupd)
	{
		[restartLock waitForSignal];
	}

	/*
	 * We get here if the sighup handler signals restartLock.
	 * We need to restart this way because restarting in the
	 * signal handler's context blocks that signal from ever
	 * getting recieved again.
	 */

    [restartLock unlock];
	[restartLock release];
	restart();

	[controller release];
	[rover release];
	exit(0);
}
