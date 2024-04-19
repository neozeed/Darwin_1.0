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
 * Timer.h
 */

/*
 * Modification History:
 * 
 * May 10, 1999	Dieter Siegmund (dieter@apple.com)
 * - initial revision
 */

#import <objc/Object.h>
#import "threadcompat.h"
#import "machcompat.h"
#import <mach/boolean.h>
#import <sys/time.h>
#import "Timeout.h"

@interface Timer:Object
{
    MUTEX_STRUCT	lock;
    CONDITION_STRUCT	wakeup;

    id<Timeout>		alarm_object;
    struct timeval	alarm_time;

    UTHREAD_T		timer;
    boolean_t		timer_ready;
    port_t		timeout_port;
}
- init;
- free;
- (void)loop;
- wake:obj Interval:(struct timeval)delta;
- wake:obj At:(struct timeval)tv;
- cancel;
@end

#define MSECS_PER_SEC	1000
#define USECS_PER_MSEC	1000
#define SECS_PER_MIN	60
#define MINS_PER_HR	60
#define HRS_PER_DAY	24

#define SECS_PER_DAY 	(SECS_PER_MIN * MINS_PER_HR * HRS_PER_DAY)
#define MSECS_PER_DAY	(SECS_PER_DAY * MSECS_PER_SEC)

