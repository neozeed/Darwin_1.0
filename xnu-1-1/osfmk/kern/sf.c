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

#include <kern/misc_protos.h>
#include <kern/sf.h>
#include <kern/processor.h>
#include <kern/mk_sp.h>
#include <mach/policy.h>

/*
 * Procedure Prototypes
 */

void		sf_update_max_sizes(
			sp_info_size_t		info_size,
			sp_attributes_size_t	attributes_size);

/*
 * The array of scheduling policies supported by the Scheduling Framework.
 */

sched_policy_t	sched_policy[MAX_SCHED_POLS];

/*
 * Maximum sizes for scheduling-policy-specific information and 
 * attributes, calculated dynamically based on the scheduling
 * policies that are initialized at system startup time.
 */
sp_info_size_t		max_sched_info_size = 0;
sp_attributes_size_t 	max_sched_attributes_size = 0;

void
sf_init(void)
{
	int			i;
	sf_return_t		sfr;
	sf_priority_mask	pri_mask;
	processor_t		myprocessor;
	processor_set_t		pset;
	extern struct processor_set	default_pset;

	/* initialize scheduling framework */
	/*** XXX should this be done once for each node? ***/

	/*
	 * Initialize scheduling policies.  Each policy has its
	 * standard operation vector initialized and then has a
	 * routine called to initialize its internal state.
	 */
	/*** XXX should this be moved to `sched_init()' -- or it to here? ***/
	/*** XXX should this be done once for each CPU? ***/

	/*
	 * Set up metapriority mask that will be used by the standard
	 * MK scheduling policies.
	 */
	for (i=0; i < NRQS; i++) {
		setbit(i, pri_mask.bitmap);
	}
	clrbit(MAXPRI - LPRI + 1, pri_mask.bitmap);
	clrbit(MAXPRI - LPRI, pri_mask.bitmap);

	/* Initialize standard MK Timesharing policy */
	sched_policy[POLICY_TIMESHARE].sp_ops = mk_sp_ops;
	sfr = sched_policy[POLICY_TIMESHARE].
		sp_ops.sp_init(
			&sched_policy[POLICY_TIMESHARE], POLICY_TIMESHARE,
				sizeof(sf_priority_mask), &pri_mask);
	if (sfr != SF_SUCCESS) {
		printf("TIMESHARE failed to init: %d\n", sfr);
		panic("sf_init");
	}

	sf_update_max_sizes(sched_policy[POLICY_TIMESHARE].sched_info_size,
			    sched_policy[POLICY_TIMESHARE].sched_attributes_size);

	/* Initialize standard MK Round Robin policy */
	sched_policy[POLICY_RR].sp_ops = mk_sp_ops;
	sfr = sched_policy[POLICY_RR].
		sp_ops.sp_init(
			&sched_policy[POLICY_RR], POLICY_RR,
				sizeof(sf_priority_mask), &pri_mask);
	if (sfr != SF_SUCCESS) {
		printf("RR failed to init: %d\n", sfr);
		panic("sf_init");
	}

	sf_update_max_sizes(sched_policy[POLICY_RR].sched_info_size,
			    sched_policy[POLICY_RR].sched_attributes_size);

	/* Initialize standard MK FIFO policy */
	sched_policy[POLICY_FIFO].sp_ops = mk_sp_ops;
	sfr = sched_policy[POLICY_FIFO].
		sp_ops.sp_init(
			&sched_policy[POLICY_FIFO], POLICY_FIFO,
				sizeof(sf_priority_mask), &pri_mask);
	if (sfr != SF_SUCCESS) {
		printf("FIFO failed to init: %d\n", sfr);
		panic("sf_init");
	}

	sf_update_max_sizes(sched_policy[POLICY_FIFO].sched_info_size,
			    sched_policy[POLICY_FIFO].sched_attributes_size);

	/* Enable the MK Timesharing policy on the current processor set */
	myprocessor = cpu_to_processor(cpu_number());
	pset = myprocessor->processor_set;
	/*** XXX fix me ***/
	if (pset == PROCESSOR_SET_NULL)
		pset = &default_pset;

	sched_policy[POLICY_TIMESHARE].
	    sp_ops.sp_enable_processor_set(
			&sched_policy[POLICY_TIMESHARE], pset);
}

void
sf_update_max_sizes(
	sp_info_size_t		info_size,
	sp_attributes_size_t	attributes_size)
{
	/* Update maximum values (individually) based on new sizes */
	if (info_size > max_sched_info_size)
		max_sched_info_size = info_size;
	if (attributes_size > max_sched_attributes_size)
		max_sched_attributes_size = attributes_size;
}
