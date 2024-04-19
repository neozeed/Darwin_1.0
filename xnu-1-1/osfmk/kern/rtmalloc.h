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

#ifndef	_KERN_rtmalloc_H_
#define _KERN_rtmalloc_H_

#include <mach/machine/vm_types.h>

#define rtmalloc_MINSIZE		16

#define RT_ZONE_MAX_1		0
#define RT_ZONE_MAX_2		0
#define RT_ZONE_MAX_4		0
#define RT_ZONE_MAX_8		0
#define RT_ZONE_MAX_16		0
#define RT_ZONE_MAX_32		0
#define RT_ZONE_MAX_64		0
#define RT_ZONE_MAX_128		0
#define RT_ZONE_MAX_256		0
#define RT_ZONE_MAX_512		0
#define RT_ZONE_MAX_1024	0
#define RT_ZONE_MAX_2048        0
#define RT_ZONE_MAX_4096	0
#define RT_ZONE_MAX_8192	0
#define RT_ZONE_MAX_16384	0
#define RT_ZONE_MAX_32768	0

/*
 * Zones start with rtmalloc_MINSIZE
 */
#define RT_ZONE_SIZE		\
		(RT_ZONE_MAX_16		* 16 + \
		 RT_ZONE_MAX_32		* 32 + \
		 RT_ZONE_MAX_64		* 64 + \
		 RT_ZONE_MAX_128	* 128 + \
		 RT_ZONE_MAX_256	* 256 + \
		 RT_ZONE_MAX_512	* 512 + \
		 RT_ZONE_MAX_1024	* 1024 + \
		 RT_ZONE_MAX_2048	* 2048 + \
		 RT_ZONE_MAX_4096	* 4096 + \
		 RT_ZONE_MAX_8192	* 8192)

extern vm_offset_t	rtmalloc(
				vm_size_t	size);

extern vm_offset_t	rtget(
				vm_size_t	size);

extern void		rtmfree(
				vm_offset_t	data,
				vm_size_t	size);

extern void		rtmalloc_init(
				void);

#endif	/* _KERN_rtmalloc_H_ */
