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
#include <pexpert/pexpert.h>
#include <pexpert/protos.h>
#include <pexpert/ppc/powermac.h>
#include <pexpert/ppc/interrupts.h>
#include <machine/machine_routines.h>
#include <kern/debug.h>

#include <IOKit/IOInterrupts.h>

void 	(*platform_interrupt)(int type, struct ppc_saved_state *ssp,
		unsigned int dsisr, unsigned int dar, spl_t old_lvl);

static void *             interrupt_nub;
static int                interrupt_source;
static void *             interrupt_target;
static IOInterruptHandler interrupt_handler;
static void *             interrupt_refCon;


static int DEBUGFlag;

void PE_enter_debugger( char * cause )
{
	if( DEBUGFlag & DB_NMI)
            Debugger(cause);
}

void PE_install_interrupt_handler(void *nub, int source,
				  void *target,
				  IOInterruptHandler handler,
				  void *refCon)
{
  interrupt_nub     = nub;
  interrupt_source  = source;
  interrupt_target  = target;
  interrupt_handler = handler;
  interrupt_refCon  = refCon;

  ml_init_interrupts();

    if( !PE_parse_boot_arg( "debug", &DEBUGFlag ))
        DEBUGFlag = 0;
}

void PE_incoming_interrupt(int type, struct ppc_saved_state *ssp,
			   unsigned int dsisr, unsigned int dar)
{
  interrupt_handler(interrupt_target, interrupt_refCon,
		    interrupt_nub, interrupt_source);
}
