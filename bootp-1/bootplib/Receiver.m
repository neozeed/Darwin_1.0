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
 * Receiver.m
 * - receive packets from a socket and propogate them to
 *   each of the registered queues
 */

/*
 * Modification History:
 * 
 * April 29, 1999	Dieter Siegmund (dieter@apple.com)
 * - initial revision
 */

#import <objc/objc.h>
#import <stdio.h>
#import <mach/mach_init.h>
#import <mach/mach_interface.h>
#import <mach/mach_error.h>
#import <unistd.h>
#import <stdlib.h>
#import <stdio.h>
#import <string.h>
#import <sys/types.h>
#import <sys/errno.h>
#import <sys/socket.h>
#import <ctype.h>
#import <net/if.h>
#import <net/etherdefs.h>
#import <netinet/in.h>
#import <netinet/udp.h>
#import <netinet/in_systm.h>
#import <netinet/ip.h>
#import <sys/types.h>
#import <sys/errno.h>
#import <sys/socket.h>
#import <ctype.h>
#import <net/if.h>
#import <net/etherdefs.h>
#import <netinet/in.h>
#import <netinet/udp.h>
#import <netinet/in_systm.h>
#import <netinet/ip.h>
#import <objc/objc.h>
#import <objc/objc-runtime.h>
#import "Receiver.h"

#import <syslog.h>
#import "ts_log.h"
#import "dprintf.h"

@implementation Receiver

static void *
socket_receive_thread(void * arg)
{
    struct sockaddr_in 	from;
    int 		fromlen;
    int 		n;
    Receiver * 		receiver = (Receiver *)arg;
    dprintf(("Receive thread starting\n"));
    [receiver setRunning:TRUE];
    for (;;) {
	if ([receiver isRunning] == FALSE)
	    break;
	[receiver waitForReceivers];
	from.sin_family = AF_INET;
	fromlen = sizeof(struct sockaddr);
	n = recvfrom(receiver->fd, receiver->rxpkt, 
		     sizeof(receiver->rxpkt), 0,
		     (struct sockaddr *)&from, &fromlen);
	if (n < 0) {
	    if (errno == EAGAIN) {
		continue;
	    }
	    syslog(LOG_ERR, "Receiver: recvfrom() failed, %m");
	    continue;
	}
	if ([receiver packet:(void *)receiver->rxpkt Size:n 
		      From:from.sin_addr] == FALSE)
	    break;
    }
    dprintf(("Receive thread stopping\n"));
    UTHREAD_EXIT(0);
    return (NULL);
}

#if 0
static void
fd_receive_thread(Receiver * receiver)
{
    int 		n;

    dprintf(("Receive thread starting\n"));
    [receiver setRunning:TRUE];
    for (;;) {
	if ([receiver isRunning] == FALSE)
	    break;
	n = read(receiver->fd, receiver->rxpkt, 
		 sizeof(receiver->rxpkt));
	if (n < 0) {
	    if (errno == EAGAIN) {
		continue;
	    }
	    syslog(LOG_ERR, "Receiver: read() failed, %m");
	    continue;
	}
	if ([receiver packet:(void *)receiver->rxpkt Size:n
		      From:NULL] == FALSE)
	    break;
    }

    dprintf(("Receive thread stopping\n"));
    UTHREAD_EXIT(0);
}
#endif 0

- (boolean_t)isRunning
{
    return (thread_running);
}

- setRunning:(boolean_t)v
{
    mutex_lock(&lock);
    thread_running = v;
    condition_signal(&wakeup);
    mutex_unlock(&lock);
    return self;
}

- init
{
    [super init];

    mutex_init(&lock);
    condition_init(&wakeup);
    thread_running = FALSE;
    receive = FALSE;
    rxQ = [[MultiplexedQueue alloc] init];
    if (rxQ == nil)
	return [self free];
    return self;
}

- initWithSocket:(int)desc
{
    if ([self init] == nil)
	return nil;
    fd = desc;
    dprintf(("Forking Receiver thread\n"));
    if (UTHREAD_CREATE(&receiver, socket_receive_thread, self))
	return [self free];
    mutex_lock(&lock);
    while (thread_running == FALSE) {
	dprintf(("Waiting for Receiver to start\n"));
	condition_wait(&wakeup, &lock);
    }
    mutex_unlock(&lock);
    dprintf(("Receiver init complete\n"));
    return self;
}

#if 0
- initWithFileDescriptor:(int)desc
{
    if ([self init] == nil)
	return nil;

    fd = desc;
    dprintf(("Forking Receiver thread\n"));
    if (UTHREAD_CREATE(&receiver, fd_receive_thread, self))
	return [self free];
    mutex_lock(&lock);
    while (thread_running == FALSE) {
	dprintf(("Waiting for Receiver to start\n"));
	condition_wait(&wakeup, &lock);
    }
    mutex_unlock(&lock);
    dprintf(("Receiver init complete\n"));
    return self;
}
#endif 0

- free
{
    /* tell the thread to stop, then wait for it */
    dprintf(("killing Receiver\n"));
    mutex_lock(&lock);
    thread_running = FALSE;
    mutex_unlock(&lock);
    if (receiver != NULL)
	UTHREAD_JOIN(receiver, NULL);
    dprintf(("Receiver is dead\n"));
    [rxQ free];
    rxQ = nil;
    mutex_clear(&lock);
    condition_clear(&wakeup);
    return [super free];
}

- start
{
    receive = TRUE;
    return self;
}

- stop
{
    receive = FALSE;
    return self;
}

- (void) waitForReceivers
{
    mutex_lock(&lock);
    while ([rxQ count] == 0) {
#ifdef DEBUG
	ts_log(LOG_DEBUG, "Receiver: waiting for receivers...");
#endif DEBUG
	condition_wait(&wakeup, &lock);
    }
    mutex_unlock(&lock);
#ifdef DEBUG
    ts_log(LOG_DEBUG, "Receiver: receivers ready\n");
#endif DEBUG
    return;
}

- attach:(id<QueueAppend>) q
{
    [rxQ attach:q];
    mutex_lock(&lock);
    condition_signal(&wakeup);
    mutex_unlock(&lock);
    return (self);
}

- detach:(id<QueueAppend>) q
{
    return [rxQ detach:q];
}

/*
 * Method: packet:Size:From
 *
 * Purpose:
 *   Deliver packet to waiting receivers.  Called from receiver
 *   thread.
 * Returns:   
 *   0 if caller should stop, non-zero if it should continue
 */
- (boolean_t)packet:(void *)data Size:(unsigned)size From:(struct in_addr)ip
{
    dprintf(("got a packet of size %d\n", size));
    mutex_lock(&lock);
    if (thread_running == FALSE) {
	mutex_unlock(&lock);
	return (FALSE);
    }
    if (receive)
	[rxQ deliverData:data Size:size From:ip];
    mutex_unlock(&lock);
    return (TRUE);
}
@end
