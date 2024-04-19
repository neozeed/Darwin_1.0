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
 * System routines
 * Copyright (C) 1989 by NeXT, Inc.
 */
#include "config.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <rpc/rpc.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/file.h>
#include <syslog.h>
#include <stdarg.h>

#include "system.h"
#include "socket_lock.h"

#ifdef NeXT
#include <libc.h>
#endif

#ifdef _THREAD_TYPE_CTHREAD_
#ifdef TRUE
#undef TRUE
#endif
#ifdef FALSE
#undef FALSE
#endif
#include <mach/cthreads.h>
static volatile mutex_t log_mutex;
#else
#include <pthread.h>
static pthread_mutex_t *log_mutex;
#define mutex_lock pthread_mutex_lock
#define mutex_unlock pthread_mutex_unlock
#endif

static char *msg_str = NULL;

void
sys_openlog(char * str, int flags, int facility)
{
	if (msg_str != NULL) free(msg_str);
	msg_str = NULL;
	if (str != NULL)
	{
		msg_str = malloc(strlen(str) + 1);
		strcpy(msg_str, str);
	}

	openlog(msg_str, flags, facility);
}
	
char *
sys_hostname(void)
{

	static char myhostname[MAXHOSTNAMELEN + 1];

	if (myhostname[0] == 0) {
		(void)gethostname(myhostname, sizeof(myhostname));
	}
	return (myhostname);
}

void
sys_msg(int debug, int priority, char *message, ...)
{
	va_list ap;
	int panic;

#ifdef _THREAD_TYPE_CTHREAD_
	if (log_mutex == NULL) log_mutex = mutex_alloc();
#else
	log_mutex = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
	pthread_mutex_init(log_mutex, NULL);
#endif

	mutex_lock(log_mutex);

	va_start(ap, message);
	if ((debug & DEBUG_SYSLOG) || (priority > LOG_DEBUG))
		vsyslog(priority, message, ap);
	if (debug & DEBUG_STDERR)
	{
		if (msg_str != NULL) fprintf(stderr, "%s: ", msg_str);
		vfprintf(stderr, message, ap);
		fprintf(stderr, "\n");
		fflush(stderr);
	}
	va_end(ap);

	panic = 0;
	if (priority <= LOG_ALERT)
	{
		syslog(priority, "aborting!");
		if (debug & DEBUG_STDERR)
		{
			fprintf(stderr, "aborting!\n");
			fflush(stderr);
		}
		panic = 1;
	}

	mutex_unlock(log_mutex);

	if (panic) abort();
}
	
int
sys_spawn(const char *fname, ...)
{
	va_list ap;
	char *args[10]; /* XXX */
	int i;
	int pid;
	
	va_start(ap, (char *)fname);
	args[0] = (char *)fname;
	for (i = 1; (args[i] = va_arg(ap, char *)) != NULL; i++) {}
	va_end(ap);

	switch (pid = fork())
	{
		case -1: return -1;
		case 0:
			execv(args[0], args);
			_exit(-1);
		default: return pid;
	}
}

long
sys_time(void)
{
	struct timeval tv;

	(void)gettimeofday(&tv, NULL);
	return (tv.tv_sec);
}
