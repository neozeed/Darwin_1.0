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
 * SignalLock.m
 *
 * A very simple condition lock class.
 *
 * Copyright (c) 1999, Apple Computer Inc.
 * All rights reserved.
 * Written by Marc Majka
 */

#import "SignalLock.h"
#ifdef _THREAD_TYPE_PTHREAD_
#import <stdlib.h>
#endif

@implementation SignalLock

- (id)init
{
#ifdef _THREAD_TYPE_PTHREAD_
	pthread_condattr_t *attr = NULL;
#endif

	[super init];
#ifdef _THREAD_TYPE_PTHREAD_
	condition = (pthread_cond_t *)malloc(sizeof(pthread_cond_t));
	pthread_cond_init(condition, attr);
#else
	condition = condition_alloc();
	condition_init(condition);
#endif
	return self;
}

- free
{
#ifdef _THREAD_TYPE_PTHREAD_
	pthread_cond_destroy(condition);
#else
	condition_free(condition);
#endif
	return [super free];
}

- (void)waitForSignal
{
#ifdef _THREAD_TYPE_PTHREAD_
	pthread_cond_wait(condition, mutex);
#else
	condition_wait(condition, mutex);
#endif
}

- (void)signal
{
#ifdef _THREAD_TYPE_PTHREAD_
	pthread_cond_signal(condition);
#else
	condition_signal(condition);
#endif
}

@end
