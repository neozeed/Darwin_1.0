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
 * file: pe_init.c
 *    i386 platform expert initialization.
 */
#include <pexpert/pexpert.h>
#include <pexpert/boot.h>

#include "fakePPCStructs.h"
#include "fakePPCDeviceTree.h"

/* extern references */
void pe_identify_machine(void *args);

/* Local references */
void display_machine_class(void);

/* private globals */
PE_state_t PE_state;

static __inline__ void outb(unsigned short port,
                                unsigned char datum)
{
        __asm__ volatile("outb %0, %1" : : "a" (datum), "d" (port));
}

void PE_init_iokit(void)
{
    extern int StartIOKit( void * p1, void * p2, void * p3, void * p4);
    long *dt;

    dt = (long *) createdt(
		fakePPCDeviceTree,
            	&((boot_args*)PE_state.fakePPCBootArgs)->deviceTreeLength);

    /* Setup powermac_info and powermac_machine_info structures */

    ((boot_args*)PE_state.fakePPCBootArgs)->deviceTreeP	= (unsigned long) dt;
    ((boot_args*)PE_state.fakePPCBootArgs)->topOfKernelData	= (unsigned int) kalloc(0x2000);

    /* 
     * Setup the OpenFirmware Device Tree routines
     * so the console can be found and the right I/O space 
     * can be used..
     */
    DTInit(dt);

    (void) StartIOKit( (void*)dt, (void*)PE_state.fakePPCBootArgs, 0, 0);
}



void PE_init_platform(boolean_t vm_initialized, void *args)
{
	if (PE_state.initialized == FALSE)
	{
	    PE_state.initialized			= TRUE;
	    PE_state.bootArgs				= args;
	    PE_state.video.v_baseAddr			= ((KERNBOOTSTRUCT *)args)->video.v_baseAddr;
	    PE_state.video.v_rowBytes			= ((KERNBOOTSTRUCT *)args)->video.v_rowBytes;
	    PE_state.video.v_height			= ((KERNBOOTSTRUCT *)args)->video.v_height;
	    PE_state.video.v_width			= ((KERNBOOTSTRUCT *)args)->video.v_width;
	    PE_state.video.v_depth			= ((KERNBOOTSTRUCT *)args)->video.v_depth;
	    PE_state.fakePPCBootArgs			= (boot_args *)&fakePPCBootArgs;
	    ((boot_args *)PE_state.fakePPCBootArgs)->machineType	= 386;
	}

	if (!vm_initialized)
	{
		/* Hack! FIXME.. */ 
    		outb(0x21, 0xff);   /* Maskout all interrupts Pic1 */
    		outb(0xa1, 0xff);   /* Maskout all interrupts Pic2 */
 
	    //pe_identify_machine(args);
	}
	else
	{
	}
}

int PE_current_console( PE_Video * info )
{
    *info = PE_state.video;
    return( 0);
}

int PE_initialize_console( PE_Video * info, int enable )
{
    return( 0);
}

extern boolean_t PE_get_hotkey( unsigned char	key)
{
    return( FALSE);
}

void PE_display_icon(	unsigned int flags,
			const char * name )
{
}

void display_machine_class()
{
}

void PE_pause(int seconds)
{
    int x, y, z;

    for (x = 0; x < (500000*seconds); x++)
        for (y = 0; y < 100; y++)
            z = x ^ y;
}

void NO_ENTRY() { }

void PE_register_timebase_callback(timebase_callback_func callback)
{
  // This should save the callback.
//  powermac_info.timebase_callback = callback;
  
  PE_call_timebase_callback();
}

void PE_call_timebase_callback(void)
{
  struct timebase_freq_t timebase_freq;
  
  // This should return the real time base freq...
  timebase_freq.timebase_num = 25000000;
  timebase_freq.timebase_den = 1;
  
  // This should call the saved callback.
//  if (powermac_info.timebase_callback)
//    powermac_info.timebase_callback(&timebase_freq);
}
