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
 * 
 */

#include <cpus.h>
#include <cputypes.h>

#include <kern/kern_types.h>
#include <kern/cpu_number.h>
#include <kern/cpu_data.h>
#include <kern/thread.h>

int master_cpu = 0;

#ifdef	PPC

cpu_data_t	cpu_data[NCPUS] =
	{ { THREAD_NULL,		/* active_thread */
	    0,				/* preemption_level */
	    0,				/* simple_lock_cout */
	    0			/* interrupt_level */
	}, };

#else	/* PPC */

cpu_data_t	cpu_data[NCPUS];

#endif	/* PPC */
