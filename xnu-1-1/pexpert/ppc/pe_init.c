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
 *    PPC platform expert initialization.
 */
#include <mach/boot_info.h>
#include <mach/time_value.h>
#include <pexpert/protos.h>
#include <pexpert/pexpert.h>
#include <pexpert/ppc/powermac.h>
#include <pexpert/ppc/interrupts.h>
#include <pexpert/device_tree.h>
#include <kern/debug.h>
#include "pe_images.h"

/* extern references */
void pe_identify_machine(void);

/* private globals */
PE_state_t PE_state;
/* powermac specific info */
powermac_info_t         	powermac_info;

static int DEBUGFlag;

static int PE_stub_read_write_time_of_day(unsigned int options, long * secs)
{
    // believe it or, BSD crashes if invalid time returned. FIXME.
    if( options == kPEReadTOD)
        *secs = 0xb2383c72;

    return 0;
}

static int PE_stub_poll_input(unsigned int options, char * c)
{
    *c = 0xff;

    return 1;
}

static int PE_stub_write_IIC(unsigned char addr, unsigned char reg,
				unsigned char data)
{
    return 1;
}

int (*PE_read_write_time_of_day)(unsigned int options, long * secs)
	= PE_stub_read_write_time_of_day;
int (*PE_poll_input)(unsigned int options, char * c)
	= PE_stub_poll_input;

int (*PE_write_IIC)(unsigned char addr, unsigned char reg,
				unsigned char data)
	= PE_stub_write_IIC;


int PE_initialize_console( PE_Video * info, int op )
{
    static int		last_console = -1;
    Boot_Video		bootInfo;
    Boot_Video	*	bInfo;

    if( info) {
        bootInfo.v_baseAddr		= info->v_baseAddr;
        bootInfo.v_rowBytes		= info->v_rowBytes;
        bootInfo.v_width		= info->v_width;
        bootInfo.v_height		= info->v_height;
        bootInfo.v_depth		= info->v_depth;
        bootInfo.v_display		= 0;
	bInfo = &bootInfo;
    } else
	bInfo = 0;

    switch( op ) {

	case kPEDisableScreen:
            initialize_screen((void *) bInfo, op);
            last_console = switch_to_serial_console();
	    break;

	case kPEEnableScreen:
            initialize_screen((void *) bInfo, op);
            if( last_console != -1)
                switch_to_old_console( last_console);
	    break;
	
	default:
            initialize_screen((void *) bInfo, op);
	    break;
    }

    return 0;
}

static boolean_t find_image( const char * name,
				void ** desc,
				unsigned char ** data,
				unsigned char ** clut )
{
    boolean_t	ok;
    DTEntry	entry;
    int		size;

#if 0
    // This is a little flawed now the device tree data
    // is freed.
    if( (kSuccess == DTLookupEntry(0, "/AAPL,images", &entry))
     && (kSuccess == DTLookupEntry(entry, name, &entry)) ) {

	ok = ( (kSuccess == DTGetProperty(entry, "desc",
				desc, &size))
            && (kSuccess == DTGetProperty(entry, "data",
				(void **)data, &size)));

        if( clut && (kSuccess != DTGetProperty(entry, "clut",
				(void **)clut, &size)))
	    *clut = appleClut8;
    } else
#endif
	ok = FALSE;

    return( ok );
}

void PE_init_iokit(void)
{
    kern_return_t	ret;
    void * 		desc;
    unsigned char *	data;
    unsigned char *	clut;

    PE_init_kprintf(TRUE);
    PE_init_printf(TRUE);

    // init this now to get mace debugger for iokit startup
    PE_init_ethernet_debugger();

    if( !find_image( "progress", &desc, &data, &clut)) {
        clut = appleClut8;
        desc = &default_progress;
        data = default_progress_data;
    }
    vc_progress_initialize( desc, data, clut );

    PE_initialize_console( (PE_Video *) 0, kPEAcquireScreen );

    ret = StartIOKit( PE_state.deviceTreeHead, PE_state.bootArgs,
			(void *)0, (void *)0);
}

void PE_init_platform(boolean_t vm_initialized, void *_args)
{
        boot_args *args = (boot_args *)_args;

	if (PE_state.initialized == FALSE)
	{
	  PE_state.initialized		= TRUE;
	  PE_state.bootArgs		= _args;
	  PE_state.deviceTreeHead	= args->deviceTreeP;
	  PE_state.video.v_baseAddr	= args->Video.v_baseAddr;
	  PE_state.video.v_rowBytes	= args->Video.v_rowBytes;
	  PE_state.video.v_width	= args->Video.v_width;
	  PE_state.video.v_height	= args->Video.v_height;
	  PE_state.video.v_depth	= args->Video.v_depth;
	  PE_state.video.v_display	= args->Video.v_display;
	  strcpy( PE_state.video.v_pixelFormat, "PPPPPPPP");
	}

	if (!vm_initialized)
	{
            /*
             * Setup the OpenFirmware Device Tree routines
             * so the console can be found and the right I/O space
             * can be used..
             */
            DTInit(PE_state.deviceTreeHead);
	
            /* Setup powermac_info and structures */
            pe_identify_machine();
	}
	else
	{
	    pe_init_debug();
	}
}

void PE_create_console( void )
{
  if (PE_state.video.v_display)
    PE_initialize_console( &PE_state.video, kPEGraphicsMode );
  else
    PE_initialize_console( &PE_state.video, kPETextMode );
}

int PE_current_console( PE_Video * info )
{
    *info = PE_state.video;
    return( 0);
}

void PE_display_icon(	unsigned int flags,
			const char * name )
{
    void * 		desc;
    unsigned char *	data;

    if( !find_image( name, &desc, &data, 0)) {
        desc = &default_roroot;
        data = default_noroot_data;
    }
    vc_display_icon( desc, data );
}

extern boolean_t PE_get_hotkey(
	unsigned char	key)
{
    unsigned char * adbKeymap;
    int		size;
    DTEntry	entry;

    if( (kSuccess != DTLookupEntry( 0, "/", &entry))
    ||  (kSuccess != DTGetProperty( entry, "AAPL,adb-keymap",
            (void **)&adbKeymap, &size))
    || (size != 16))

        return( FALSE);

    if( key > 127)
	return( FALSE);

    return( adbKeymap[ key / 8 ] & (0x80 >> (key & 7)));
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
  powermac_info.timebase_callback = callback;
  
  PE_call_timebase_callback();
}

void PE_call_timebase_callback(void)
{
  struct timebase_freq_t timebase_freq;
  unsigned long          num, den, cnt;
  
  num = powermac_info.bus_clock_rate_num * powermac_info.bus_to_dec_rate_num;
  den = powermac_info.bus_clock_rate_den * powermac_info.bus_to_dec_rate_den;
  
  cnt = 2;
  while (cnt <= den) {
    if ((num % cnt) || (den % cnt)) {
      cnt++;
      continue;
    }
    
    num /= cnt;
    den /= cnt;
  }
  
  timebase_freq.timebase_num = num;
  timebase_freq.timebase_den = den;
  
  if (powermac_info.timebase_callback)
    powermac_info.timebase_callback(&timebase_freq);
}
