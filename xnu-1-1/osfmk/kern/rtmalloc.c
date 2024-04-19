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
/*
 * HISTORY
 * $Log: rtmalloc.c,v $
 * Revision 1.1.1.1  2000/03/25 21:02:59  wsanchez
 * Import of xnu-68.4
 *
 * Revision 1.3  2000/01/26 05:57:35  wsanchez
 * Add APSL
 *
 * Revision 1.2  1998/10/20 03:35:27  wsanchez
 * Merged in Bill's mother-of-all-diffs.
 *
 * Revision 1.1.2.1  1998/10/20 02:52:22  angell
 * Changed PTE handling.  Added SMP and RT. Improved context saving.
 *
 * Revision 1.1.1.1  1998/09/22 21:05:33  wsanchez
 * Import of Mac OS X kernel (~semeria)
 *
 * Revision 1.1.1.1  1998/03/07 02:25:56  wsanchez
 * Import of OSF Mach kernel (~mburg)
 *
 * Revision 1.1.4.4  1995/10/09  17:16:24  devrcs
 * 	Merge RT IPC
 * 	[1995/09/21  23:10:26  widyono]
 *
 * Revision 1.1.4.3  1995/01/10  05:13:37  devrcs
 * 	mk6 CR801 - copyright marker not FREE_
 * 	[1994/12/01  19:24:57  dwm]
 * 
 * 	mk6 CR668 - 1.3b26 merge
 * 	zchange to zone_change
 * 	[1994/11/04  09:29:18  dwm]
 * 
 * Revision 1.1.4.1  1994/08/18  23:12:56  widyono
 * 	RT IPC from RT2_SHARED
 * 	[1994/08/18  15:50:06  widyono]
 * 
 * Revision 1.1.2.1  1994/07/29  07:34:38  widyono
 * 	Explicitly make RT zones non-collectable, non-expandable
 * 	[1994/07/29  06:26:39  widyono]
 * 
 * 	Update to rt2_shared: zchange -> zone_change
 * 	[1994/07/29  05:07:59  widyono]
 * 
 * 	RT zone allocation routines, new module
 * 	[1994/07/28  22:31:19  widyono]
 * 
 * $EndLog$
 */
/* CMU_HIST */
/*
 * Revision 2.9  91/05/14  16:43:17  mrt
 * 	Correcting copyright
 * 
 * Revision 2.8  91/03/16  14:50:37  rpd
 * 	Updated for new kmem_alloc interface.
 * 	[91/03/03            rpd]
 * 
 * Revision 2.7  91/02/05  17:27:22  mrt
 * 	Changed to new Mach copyright
 * 	[91/02/01  16:14:12  mrt]
 * 
 * Revision 2.6  90/06/19  22:59:06  rpd
 * 	Made the big kalloc zones collectable.
 * 	[90/06/05            rpd]
 * 
 * Revision 2.5  90/06/02  14:54:47  rpd
 * 	Added kalloc_max, kalloc_map_size.
 * 	[90/03/26  22:06:39  rpd]
 * 
 * Revision 2.4  90/01/11  11:43:13  dbg
 * 	De-lint.
 * 	[89/12/06            dbg]
 * 
 * Revision 2.3  89/09/08  11:25:51  dbg
 * 	MACH_KERNEL: remove non-MACH data types.
 * 	[89/07/11            dbg]
 * 
 * Revision 2.2  89/08/31  16:18:59  rwd
 * 	First Checkin
 * 	[89/08/23  15:41:37  rwd]
 * 
 * Revision 2.6  89/08/02  08:03:28  jsb
 * 	Make all kalloc zones 8 MB big. (No more kalloc panics!)
 * 	[89/08/01  14:10:17  jsb]
 * 
 * Revision 2.4  89/04/05  13:03:10  rvb
 * 	Guarantee a zone max of at least 100 elements or 10 pages
 * 	which ever is greater.  Afs (AllocDouble()) puts a great demand
 * 	on the 2048 zone and used to blow away.
 * 	[89/03/09            rvb]
 * 
 * Revision 2.3  89/02/25  18:04:39  gm0w
 * 	Changes for cleanup.
 * 
 * Revision 2.2  89/01/18  02:07:04  jsb
 * 	Give each kalloc zone a meaningful name (for panics);
 * 	create a zone for each power of 2 between MINSIZE
 * 	and PAGE_SIZE, instead of using (obsoleted) NQUEUES.
 * 	[89/01/17  10:16:33  jsb]
 * 
 *
 * 13-Feb-88  John Seamons (jks) at NeXT
 *	Updated to use kmem routines instead of vmem routines.
 *
 * 21-Jun-85  Avadis Tevanian (avie) at Carnegie-Mellon University
 *	Created.
 */
/* CMU_ENDHIST */
/* 
 * Mach Operating System
 * Copyright (c) 1991,1990,1989,1988,1987 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */
/*
 */
/*
 *	File:	kern/rtmalloc.c
 *	Author:	Ron Widyono
 *	Date:	1994
 *
 *	General real-time kernel memory allocator.  This allocator is designed
 *	to be used by the kernel to manage dynamic memory fast, but exclusively
 *      for real-time purposes.  In fact, only real-time IPC users compete for
 *      these resources.
 *
 *      This is a dirty implementation--a clone of kalloc.  A cleaner imple-
 *      mentation is to have both k_zones and rt_zones use common code.
 */

#include <mach/boolean.h>
#include <mach/machine/vm_types.h>
#include <mach/vm_param.h>
#include <kern/misc_protos.h>
#include <kern/zalloc.h>
#include <kern/rtmalloc.h>
#include <vm/vm_kern.h>
#include <vm/vm_object.h>
#include <vm/vm_map.h>

extern void	Debugger(const char *);

vm_map_t rtmalloc_map;
vm_size_t rtmalloc_map_size = 8 * 1024 * 1024;
vm_size_t rtmalloc_max;
vm_size_t rtmalloc_max_prerounded;

/*
 *	All allocations of size less than rtmalloc_max are rounded to the
 *	next highest power of 2.  This allocator is built on top of
 *	the zone allocator.  A zone is created for each potential size
 *	that we are willing to get in small blocks.
 *
 *	We assume that rtmalloc_max is not greater than 64K;
 *	thus 16 is a safe array size for rt_zone and rt_zone_name.
 *
 *	Note that rtmalloc_max is somewhat confusingly named.
 *	It represents the first power of two for which no zone exists.
 *	rtmalloc_max_prerounded is the smallest allocation size, before
 *	rounding, for which no zone exists.
 */

int first_rt_zone = -1;
struct zone *rt_zone[16];
static char *rt_zone_name[16] = {
	"rtmalloc.1",		"rtmalloc.2",
	"rtmalloc.4",		"rtmalloc.8",
	"rtmalloc.16",		"rtmalloc.32",
	"rtmalloc.64",		"rtmalloc.128",
	"rtmalloc.256",		"rtmalloc.512",
	"rtmalloc.1024",		"rtmalloc.2048",
	"rtmalloc.4096",		"rtmalloc.8192",
	"rtmalloc.16384",	"rtmalloc.32768"
};

/*
 *  Max number of elements per zone.  zinit rounds things up correctly
 *  Doing things this way permits each zone to have a different maximum size
 *  based on need, rather than just guessing; it also
 *  means its patchable in case you're wrong!
 */
unsigned long rt_zone_max[16] = {
      RT_ZONE_MAX_1,	
      RT_ZONE_MAX_2,	
      RT_ZONE_MAX_4,	
      RT_ZONE_MAX_8,	
      RT_ZONE_MAX_16,	
      RT_ZONE_MAX_32,	
      RT_ZONE_MAX_64,	
      RT_ZONE_MAX_128,	
      RT_ZONE_MAX_256,	
      RT_ZONE_MAX_512,	
      RT_ZONE_MAX_1024,	
      RT_ZONE_MAX_2048,	
      RT_ZONE_MAX_4096,	
      RT_ZONE_MAX_8192,	
      RT_ZONE_MAX_16384,
      RT_ZONE_MAX_32768
};

/*
 *	Initialize the memory allocator.  This should be called only
 *	once on a system wide basis (i.e. first processor to get here
 *	does the initialization).
 *
 *	This initializes all of the zones.
 */

void
rtmalloc_init(
	void)
{
	kern_return_t retval;
	vm_offset_t min, addr;
	vm_size_t size;
	register int i;

	retval = kmem_suballoc(kernel_map, &min, rtmalloc_map_size,
			       FALSE, TRUE, &rtmalloc_map);
	if (retval != KERN_SUCCESS)
		panic("rtmalloc_init: kmem_suballoc failed");

	/*
	 *	Ensure that zones up to size 8192 bytes exist.
	 *	This is desirable because messages are allocated
	 *	with rtmalloc, and messages up through size 8192 are common.
	 */

	rtmalloc_max = 16 * 1024;
	rtmalloc_max_prerounded = rtmalloc_max / 2 + 1;

	/*
	 *	Allocate a zone for each size we are going to handle.
	 *	We specify non-paged memory.  Make zone exhaustible.
	 */
	for (i = 0, size = 1; size < rtmalloc_max; i++, size <<= 1) {
		if (size < rtmalloc_MINSIZE) {
			rt_zone[i] = 0;
			continue;
		}
		if (size == rtmalloc_MINSIZE) {
			first_rt_zone = i;
		}
		rt_zone[i] = zinit(size, rt_zone_max[i] * size, size,
				  rt_zone_name[i]);
		zone_change(rt_zone[i], Z_EXHAUST, TRUE);
		zone_change(rt_zone[i], Z_COLLECT, FALSE);
		zone_change(rt_zone[i], Z_EXPAND, FALSE);

		/*
		 * Get space from the zone_map.  Since these zones are
		 * not collectable, no pages containing elements from these
		 * zones will ever be reclaimed by the garbage collection
		 * scheme below.
		 */

		zprealloc(rt_zone[i], rt_zone_max[i] * size);
	}
}

vm_offset_t
rtmalloc(
	vm_size_t	size)
{
	register int zindex;
	register vm_size_t allocsize;

	/*
	 * If size is too large for a zone, return error (0)
	 */

	if (size >= rtmalloc_max_prerounded) {
		return(0);
	}

	/* compute the size of the block that we will actually allocate */

	allocsize = rtmalloc_MINSIZE;
	zindex = first_rt_zone;
	while (allocsize < size) {
		allocsize <<= 1;
		zindex++;
	}

	/* allocate from the appropriate zone */

	assert(allocsize < rtmalloc_max);
	return(zalloc(rt_zone[zindex]));
}

vm_offset_t
rtget(
	vm_size_t	size)
{
	register int zindex;
	register vm_size_t allocsize;

	/* size must not be too large for a zone */

	if (size >= rtmalloc_max_prerounded) {
		/* This will never work, so we might as well panic */
		panic("rtget");
	}

	/* compute the size of the block that we will actually allocate */

	allocsize = rtmalloc_MINSIZE;
	zindex = first_rt_zone;
	while (allocsize < size) {
		allocsize <<= 1;
		zindex++;
	}

	/* allocate from the appropriate zone */

	assert(allocsize < rtmalloc_max);
	return(zget(rt_zone[zindex]));
}

void
rtmfree(
	vm_offset_t	data,
	vm_size_t	size)
{
	register int zindex;
	register vm_size_t freesize;

	/*
	 * if size was too large for a RT zone, this is impossible;
	 * just return
	 */

	assert(size < rtmalloc_max_prerounded);
	if (size >= rtmalloc_max_prerounded) {
		return;
	}

	/* compute the size of the block that we actually allocated from */

	freesize = rtmalloc_MINSIZE;
	zindex = first_rt_zone;
	while (freesize < size) {
		freesize <<= 1;
		zindex++;
	}

	/* free to the appropriate zone */

	assert(freesize < rtmalloc_max);
	zfree(rt_zone[zindex], data);
}
