/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 * Timer.m
 */

/*
 * Modification History:
 * 
 * May 10, 1999	Dieter Siegmund (dieter@apple.com)
 * - initial revision
 */

#import "Timer.h"

#import <objc/objc.h>
#import <stdio.h>
#import <unistd.h>
#import <stdlib.h>
#import <stdio.h>
#import <string.h>

#import "util.h"

#import "dprintf.h"

@implementation Timer
- (void)loop
{
    kern_return_t 	krtn;

#ifndef MOSX
    msg_header_t	header;

    memset(&header, 0, sizeof(header));

    header.msg_remote_port = PORT_NULL;
    header.msg_local_port = timeout_port;
    header.msg_size = sizeof(header);
#else MOSX
    mach_msg_header_t	header;

    memset(&header, 0, sizeof(header));
    header.msgh_remote_port = PORT_NULL;
#endif

    mutex_lock(&lock);
    for (;;) {
	struct timeval 	tv;

	timer_ready = TRUE;
	condition_signal(&wakeup);
	while (alarm_object == nil && timeout_port != PORT_NULL)
	    condition_wait(&wakeup, &lock);

	if (timeout_port == PORT_NULL)
	    break; /* we're being free'd */

	tv = alarm_time;
	timer_ready = FALSE;
	mutex_unlock(&lock);
	while (1) {
	    struct timeval 	current;
	    struct timeval 	delta;
	    msg_timeout_t	msecs;

	    gettimeofday(&current, 0);
	    timeval_subtract(tv, current, &delta);

	    if (delta.tv_sec < 0 
		|| (delta.tv_sec == 0 && delta.tv_usec < USECS_PER_MSEC)) {
		id<Timeout>	obj = alarm_object;	/* could be nil */

		/* deadline expired */
		dprintf(("Timer: timeout sent to %x\n", (unsigned)obj));
		[obj timeout];
		alarm_object = nil;
		break; /* out of while */
	    }
#ifdef DEBUG
#define MAX_SECONDS		(SECS_PER_MIN)
#else
#define MAX_SECONDS		(SECS_PER_DAY * 20)
#endif DEBUG
	    /* 
	     * msecs (32-bit unsigned integer) storing timeout in milliseconds,
	     * will wrap in ~49 days [2^32 / (1000 * 60 * 60 * 24) = ~49 days]
	     * avoid this by sleeping in chunks of MAX_SECONDS
	     */
	    if (delta.tv_sec > MAX_SECONDS) { 
		dprintf(("Timer: avoiding wrapping\n"));
		delta.tv_sec = MAX_SECONDS;
	    }
	    msecs = (delta.tv_sec * MSECS_PER_SEC)
		+ (delta.tv_usec / USECS_PER_MSEC);
#ifdef MOSX
	    header.msgh_size = sizeof(header);
	    header.msgh_local_port = timeout_port;
#endif MOSX
	    krtn = msg_receive(&header, RCV_TIMEOUT, msecs);
	    if (krtn != RCV_TIMED_OUT) {
		dprintf(("Timer: woken up\n"));
		/* we were woken up */
		break; /* out of while */
	    }
	}
	mutex_lock(&lock);
    }
    mutex_unlock(&lock);
    return;
}

static void *
timer_thread(void * arg)
{
    Timer * timer = (Timer *)arg;

    dprintf(("Timer timer_thread starting\n"));
    [timer loop];
    dprintf(("timer_thread: thread exiting\n"));
    UTHREAD_EXIT(0);
    return (NULL);
}

- wakeThread:(port_t)p
{
    msg_header_t	header;
    kern_return_t 	krtn;

    memset(&header, 0, sizeof(header));
#ifdef MOSX
    header.msgh_remote_port = p;
    header.msgh_local_port = MACH_PORT_NULL;
    header.msgh_size = sizeof(header);
    header.msgh_bits = MACH_MSG_TYPE_MAKE_SEND;
#else MOSX
    header.msg_remote_port = p;
    header.msg_local_port = PORT_NULL;
    header.msg_size = sizeof(header);
    header.msg_type = MSG_TYPE_NORMAL;
    header.msg_simple = 1;
#endif MOSX
    krtn = msg_send(&header, MSG_OPTION_NONE, 0);
	
    if (krtn) {
	dprintf(("Timer[wakeThread]: mach msg send failed, %s\n",
		 mach_error_string(krtn)));
    }
    return self;
}


- (void) waitForTimerReady
{
    if (timer_ready == FALSE) {
	alarm_object = nil;
	mutex_unlock(&lock);
	[self wakeThread:timeout_port];
	mutex_lock(&lock);
	while (timer_ready == FALSE)
	    condition_wait(&wakeup, &lock);
    }
    return;
}

- wake:obj Interval:(struct timeval)delta
{
    struct timeval current;
    struct timeval tv;

    gettimeofday(&current, 0);
    timeval_add(current, delta, &tv);
    return ([self wake:obj At:tv]);
}

- wake:obj At:(struct timeval)tv
{
    /* clear the old timeout */
    mutex_lock(&lock);
    [self waitForTimerReady];
    alarm_object = obj;
    alarm_time = tv;
    condition_signal(&wakeup);
    mutex_unlock(&lock);
    return (self);
}

- cancel
{
    mutex_lock(&lock);
    [self waitForTimerReady];
    mutex_unlock(&lock);
    return self;
}


- init
{
    kern_return_t 	krtn;

    [super init];

    mutex_init(&lock);
    condition_init(&wakeup);
    timer = NULL;
    alarm_object = nil;
    timer_ready = FALSE;
    timeout_port = PORT_NULL;
    krtn = port_allocate(task_self(), &timeout_port);

    /* use mach port for timeouts */
    if (krtn) {
	dprintf(("timer_thread(): port_allocate: %s\n",
		 mach_error_string(krtn)));
	return [self free];
    }

    /* start a timer thread */
    if (UTHREAD_CREATE(&timer, timer_thread, self))
	return [self free];

    mutex_lock(&lock);
    while (timer_ready == FALSE) {
	dprintf(("Waiting for Timer to start\n"));
	condition_wait(&wakeup, &lock);
    }
    mutex_unlock(&lock);
    return self;
}

- free
{
    port_t p = timeout_port;

    if (timer != NULL) {
	/* tell the thread to stop, then wait for it */
	dprintf(("killing Timer\n"));
	mutex_lock(&lock);
	timeout_port = PORT_NULL;
	condition_signal(&wakeup);
	mutex_unlock(&lock);
	[self wakeThread:p]; /* get it out of msg receive */
	UTHREAD_JOIN(timer, NULL);
	dprintf(("Timer is dead\n"));
    }
    if (p != PORT_NULL)
	port_deallocate(task_self(), p);
    mutex_clear(&lock);
    condition_clear(&wakeup);
    return [super free];
}

@end
