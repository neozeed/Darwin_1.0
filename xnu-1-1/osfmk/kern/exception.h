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
 * @OSF_COPYRIGHT@
 */

#ifndef _EXCEPTION_H_
#define _EXCEPTION_H_

#include <ipc/ipc_port.h>
#include <ipc/ipc_kmsg.h>
#include <mach/exception_types.h>

/* Make an up-call to a thread's exception server */
extern void exception(
	exception_type_t	exception,
	exception_data_t	code,
	mach_msg_type_number_t	codeCnt);

/* Continue after blocking for an exception */
extern void exception_raise_continue(void);

/* Special-purpose fast continuation for exceptions */
extern void exception_raise_continue_fast(
	ipc_port_t		reply_port,
	ipc_kmsg_t		kmsg,
	mach_msg_type_number_t	codeCnt	);

/*
 * These must be implemented in machine dependent code
 */

/* Restart syscall */
extern void restart_mach_syscall(void);

/* Make an up-call to a thread's alert handler */
extern kern_return_t alert_exception(
	exception_type_t	exception,
	exception_data_t	code,
	int			codeCnt	);

#endif	/* _EXCEPTION_H_ */
