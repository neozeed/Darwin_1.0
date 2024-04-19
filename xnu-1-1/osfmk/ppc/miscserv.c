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
 *	This file contains a thread that is used to do some random stuff on PPC machines.
 *	So far, we used it to adjust the number of free mappings.
 *
 */

#include <cpus.h>
#include <debug.h>
#include <mach_kgdb.h>
#include <mach_vm_debug.h>
#include <db_machine_commands.h>

#include <kern/thread.h>
#include <mach/vm_attributes.h>
#include <mach/vm_param.h>
#include <vm/vm_kern.h>
#include <vm/vm_map.h>
#include <vm/vm_page.h>
#include <kern/spl.h>

#include <kern/misc_protos.h>
#include <ppc/misc_protos.h>
#include <ppc/proc_reg.h>
#include <ppc/mem.h>
#include <ppc/pmap.h>
#include <ppc/pmap_internals.h>
#include <ppc/new_screen.h>
#include <ppc/Firmware.h>
#include <ppc/mappings.h>
#include <ppc/miscserv.h>
#include <ddb/db_output.h>

unsigned int	missy_run;								/* Run request */
decl_simple_lock_data(,missy_lock)

void missy_serv(void) {									/* Do various stuff */
	
	thread_t	thread;
	processor_set_t	pset;
	kern_return_t                   ret;
	policy_base_t                   base;
	policy_limit_t                  limit;
	policy_fifo_base_data_t         fifo_base;
	policy_fifo_limit_data_t        fifo_limit;
	spl_t			s;

	simple_lock_init(&missy_lock, 0);					/* Initialize our control lock */
	thread = current_thread();							/* Find out who we are */
	thread->vm_privilege = FALSE;						/* Don't let us dip into the reserved pool */
	stack_privilege(thread);							/* We really need to keep our stack */
	
	pset = thread->processor_set;						/* Where are we? */
	base = (policy_base_t) &fifo_base;					/* Get the base priority */
	limit = (policy_limit_t) &fifo_limit;
	fifo_base.base_priority = BASEPRI_KERNEL;
	fifo_limit.max_priority = BASEPRI_KERNEL;
	ret = thread_set_policy(thread->top_act, pset, POLICY_FIFO,	/* Make sure we have really high priority */
		base, POLICY_TIMESHARE_BASE_COUNT, 
		limit, POLICY_TIMESHARE_LIMIT_COUNT);
	if (ret != KERN_SUCCESS) printf("missy_serv: we couldn't get fifo...\n");

	while(1) {											/* Here we stay... */

		mapping_adjust();								/* Adjust mapping stuff */
	
		s = splhigh();									/* Don't bother from now on */
		simple_lock(&missy_lock);						/* Control ourselves */
		assert_wait((event_t) &missy_run, THREAD_UNINT);		/* Say that we will wait for this */
		simple_unlock(&missy_lock);						/* Uncontrol ourselves */
		splx(s);										/* Restore 'rupts */
	
		thread_block((void (*)(void)) 0);				/* Wait for the next time */
	
	}
}


