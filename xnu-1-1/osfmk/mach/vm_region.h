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
 * 
 * Revision 1.1.1.1  1998/09/22 21:05:31  wsanchez
 * Import of Mac OS X kernel (~semeria)
 *
 * Revision 1.1.1.1  1998/03/07 02:25:46  wsanchez
 * Import of OSF Mach kernel (~mburg)
 *
 * Revision 1.1.5.1  1995/01/16  17:22:27  bolinger
 * 	Import files unchanged from osc1.3b11 into cnmk_shared.
 * 	[1995/01/16  17:20:37  bolinger]
 *
 * Revision 1.1.3.2  1993/10/05  22:23:22  watkins
 * 	Merge forward.
 * 	[1993/10/05  22:05:05  watkins]
 * 
 * Revision 1.1.1.2  1993/09/28  19:42:50  watkins
 * 	Created to comply with spec.
 * 
 * $EndLog$
 */
/*
 *	File:	mach/vm_region.h
 *
 *	Define the attributes of a task's memory region
 *
 */

#ifndef	_MACH_VM_REGION_H_
#define _MACH_VM_REGION_H_

#include <mach/boolean.h>
#include <mach/vm_prot.h>
#include <mach/vm_inherit.h>
#include <mach/vm_behavior.h>

/*
 *	Types defined:
 *
 *	vm_region_info_t	memory region attributes
 */

#define VM_REGION_INFO_MAX      (1024)
typedef int	*vm_region_info_t;
typedef int	 vm_region_flavor_t;
typedef int	 vm_region_info_data_t[VM_REGION_INFO_MAX];

#define VM_REGION_BASIC_INFO	10

struct vm_region_basic_info {
	vm_prot_t		protection;
	vm_prot_t		max_protection;
	vm_inherit_t		inheritance;
	boolean_t		shared;
	boolean_t		reserved;
	vm_offset_t		offset;
	vm_behavior_t		behavior;
	unsigned short		user_wired_count;
};

typedef struct vm_region_basic_info		*vm_region_basic_info_t;
typedef struct vm_region_basic_info		 vm_region_basic_info_data_t;

#define VM_REGION_BASIC_INFO_COUNT		\
	(sizeof(vm_region_basic_info_data_t)/sizeof(int))



#define VM_REGION_EXTENDED_INFO	11

#define SM_COW             1
#define SM_PRIVATE         2
#define SM_EMPTY           3
#define SM_SHARED          4
#define SM_TRUESHARED      5
#define SM_PRIVATE_ALIASED 6
#define SM_SHARED_ALIASED  7


struct vm_region_extended_info {
	vm_prot_t		protection;
        unsigned int            user_tag;
        unsigned int            pages_resident;
        unsigned int            pages_shared_now_private;
        unsigned int            pages_swapped_out;
        unsigned int            pages_referenced;
        unsigned int            ref_count;
        unsigned short          shadow_depth;
        unsigned char           external_pager;
        unsigned char           share_mode;
};

typedef struct vm_region_extended_info		*vm_region_extended_info_t;
typedef struct vm_region_extended_info		 vm_region_extended_info_data_t;

#define VM_REGION_EXTENDED_INFO_COUNT		\
	(sizeof(vm_region_extended_info_data_t)/sizeof(int))


#define VM_REGION_TOP_INFO	12

struct vm_region_top_info {
        unsigned int            obj_id;
        unsigned int            ref_count;
        unsigned int            private_pages_resident;
        unsigned int            shared_pages_resident;
        unsigned char           share_mode;
};

typedef struct vm_region_top_info		*vm_region_top_info_t;
typedef struct vm_region_top_info		 vm_region_top_info_data_t;

#define VM_REGION_TOP_INFO_COUNT		\
	(sizeof(vm_region_top_info_data_t)/sizeof(int))


struct vm_read_entry {
	vm_address_t	address;
	vm_size_t	size;
};

#define VM_MAP_ENTRY_MAX  (256)

typedef struct vm_read_entry	vm_read_entry_t[VM_MAP_ENTRY_MAX];

#endif	/*_MACH_VM_REGION_H_*/
