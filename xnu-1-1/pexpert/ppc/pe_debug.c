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
#include <pexpert/ppc/powermac.h>
#include <kern/debug.h>

static int DEBUGFlag;

pe_init_debug(void)
{
	if( !PE_parse_boot_arg( "debug", &DEBUGFlag ))
		DEBUGFlag = 0;
#ifdef	DEBUG
	dump_machine_info();
#endif
}

void PE_enter_debugger( char * cause )
{
        if( DEBUGFlag & DB_NMI)
            Debugger(cause);
}   


void dump_machine_info(void)
{
	kprintf("--- machine info ---\n");
	kprintf("  processor version: 0x%x\n", powermac_info.processor_version);
	kprintf("  cpu clock rate: %d hz\n", powermac_info.cpu_clock_rate_hz);
	kprintf("  bus clock rate: %d hz\n", powermac_info.bus_clock_rate_hz);
	kprintf("  dec clock rate: %d hz\n", powermac_info.dec_clock_rate_hz);
	kprintf("  caches:\n    dcache size:%dk;  block size: %db\n", 
			powermac_info.dcache_size / 1024,
			powermac_info.dcache_block_size);
	kprintf("    icache size: %dk  %s\n", 
			powermac_info.icache_size / 1024,
			powermac_info.caches_unified ? "(unified)" : "");
	kprintf("--------------------\n");
}

