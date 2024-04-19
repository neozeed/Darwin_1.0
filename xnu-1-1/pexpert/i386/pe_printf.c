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
 * file: pe_printf.c
 *    i386 platform expert output initialization.
 */
#include <sys/types.h>
#include <mach/vm_param.h>
#include <stdarg.h>
#include <pexpert/protos.h>
#include <pexpert/pexpert.h>
#include <pexpert/i386/kdsoft.h>

#define KA_NORMAL       0x07
#define COLOR_PLATNUM   0x80

/* extern references */
extern void printf(const char *format, ...);
extern void cninit(void);
extern void display_putc(char c);
extern void cnputc(char c);

/* local references */
void PE_printf(const char *fmt, ...);
unsigned char *mapframebuffer(caddr_t,int);
void (*PE_putc)(char c);
u_char *vid_start;

void PE_init_printf(boolean_t vm_initialized)
{
	PE_Video v;



    if (!vm_initialized)
    {
	/*
	 * Initialize the console so we can print.  Start out by pointing to 0xa0000, but eventually
	 * map our space in and then update the console "vid_start".
	 */
	/* FIX ME! */
	
	vid_start = 0xa0000;
	v.v_width = PE_state.video.v_width;
        v.v_depth = PE_state.video.v_depth;
        v.v_rowBytes = PE_state.video.v_rowBytes;
	/* HACK - Only "one segment" is available on the screen  */
        v.v_height = 0x10000 / (PE_state.video.v_rowBytes);
	initialize_screen(&v, vid_start);
    }
    else
    {
	/*
	 * Initialize the console so we can print.  Now that VM is initialized, map out physical
	 * space in.
	 */

	if (PE_state.video.v_baseAddr != 0)
	{
    	    unsigned char *fb;

	    fb = mapframebuffer((caddr_t)PE_state.video.v_baseAddr, 
				PE_state.video.v_rowBytes * PE_state.video.v_height);
	    vid_start = fb;
	    initialize_screen(&PE_state.video, vid_start);
	    printf("Console initialized.  Mode: Linear (%ldx%ldx%ld)\n", PE_state.video.v_width,
								      PE_state.video.v_height,
								      PE_state.video.v_depth);
	    init_display_putc(fb, PE_state.video.v_width * PE_state.video.v_depth / 8, PE_state.video.v_height);
	    PE_putc = display_putc;
	    PE_printf("Debug console initialized.\n"); PE_pause(1);
	    
	}
    }
}

void PE_printf(const char *fmt, ...)
{
        va_list listp;

        va_start(listp, fmt);
	_doprnt(fmt, &listp, PE_putc, 16);
        va_end(listp);
}

/*
 * mapblit: map the framebuffer into kernel vm and return the (virtual)
 * address.
 */
unsigned char *
mapframebuffer(
        caddr_t physaddr,               /* start of framebuffer */
        int     length)                 /* num bytes to map */
{
        vm_offset_t vmaddr;

        if (physaddr != (caddr_t)trunc_page(physaddr))
                panic("Framebuffer not on page boundary");

        vmaddr = io_map((vm_offset_t)physaddr, length);
        if (vmaddr == 0)
                panic("can't alloc VM for framebuffer");

        return((unsigned char *)vmaddr);
}
