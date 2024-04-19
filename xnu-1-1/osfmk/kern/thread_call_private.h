/*
 * Copyright (c) 1993-1995, 1999-2000 Apple Computer, Inc.
 * All rights reserved.
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
 * Private declarations for thread-based callouts.
 *
 * HISTORY
 *
 * 10 July 1999 (debo)
 *  Pulled into Mac OS X (microkernel).
 *
 * 3 July 1993 (debo)
 *	Created.
 */

#ifndef	_KERN_THREAD_CALL_PRIVATE_H_
#define	_KERN_THREAD_CALL_PRIVATE_H_

#include <kern/queue.h>

typedef struct thread_call {
    queue_chain_t		q_link;
    thread_call_func_t	func;
    thread_call_param_t	param0;
    thread_call_param_t	param1;
    AbsoluteTime		deadline;
    enum {
	  IDLE,
	  PENDING,
	  DELAYED }			status;
} thread_call_data_t;

#define	thread_call_setup(call, pfun, p0)				\
MACRO_BEGIN												\
	(call)->func		= (thread_call_func_t)(pfun);	\
	(call)->param0		= (thread_call_param_t)(p0);	\
	(call)->status		= IDLE;							\
MACRO_END

#endif	/* _KERN_THREAD_CALL_PRIVATE_H_ */
