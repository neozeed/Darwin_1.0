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
 * @OSF_FREE_COPYRIGHT@
 */

/* TODO NMGS REMOVE ALL OF THESE AND THEN THIS FILE !!! */

#include <types.h>
#include <ppc/trap.h>
#include <kern/misc_protos.h>
#include <kern/sched_prim.h>
#include <kern/kern_types.h>
#include <kern/exception.h>
#include <ppc/pmap.h>
#include <ppc/misc_protos.h>
#include <ppc/fpu_protos.h>
#include <mach/rpc.h>
#include <ppc/machine_rpc.h>

int
procitab(u_int spllvl,
         void (*handler)(int),
         int unit) 
{
	printf("NMGS TODO NOT YET");
	return 0;
}

void restart_mach_syscall(void)
{
	panic(__FUNCTION__/* NOT YET NMGS */);
}
