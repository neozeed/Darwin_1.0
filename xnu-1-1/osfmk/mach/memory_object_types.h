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
 * Copyright (c) 1991,1990,1989,1988 Carnegie Mellon University
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
 *	File:	memory_object.h
 *	Author:	Michael Wayne Young
 *
 *	External memory management interface definition.
 */

#ifndef	_MACH_MEMORY_OBJECT_TYPES_H_
#define _MACH_MEMORY_OBJECT_TYPES_H_

/*
 *	User-visible types used in the external memory
 *	management interface:
 */

#include <mach/port.h>
#include <mach/vm_types.h>
#include <mach/machine/vm_types.h>

typedef mach_port_t	memory_object_default_t;

typedef	mach_port_t	memory_object_t;
					/* A memory object ... */
					/*  Used by the kernel to retrieve */
					/*  or store data */

typedef	mach_port_t	memory_object_control_t;
					/* Provided to a memory manager; ... */
					/*  used to control a memory object */

typedef	mach_port_t	memory_object_name_t;
					/* Used to describe the memory ... */
					/*  object in vm_regions() calls */

typedef mach_port_t     memory_object_rep_t;
					/* Per-client handle for mem object */
					/*  Used by user programs to specify */
					/*  the object to map */

typedef	int		memory_object_copy_strategy_t;
					/* How memory manager handles copy: */
#define		MEMORY_OBJECT_COPY_NONE		0
					/* ... No special support */
#define		MEMORY_OBJECT_COPY_CALL		1
					/* ... Make call on memory manager */
#define		MEMORY_OBJECT_COPY_DELAY 	2
					/* ... Memory manager doesn't
					 *     change data externally.
					 */
#define		MEMORY_OBJECT_COPY_TEMPORARY 	3
					/* ... Memory manager doesn't
					 *     change data externally, and
					 *     doesn't need to see changes.
					 */
#define		MEMORY_OBJECT_COPY_SYMMETRIC 	4
					/* ... Memory manager doesn't
					 *     change data externally,
					 *     doesn't need to see changes,
					 *     and object will not be
					 *     multiply mapped.
					 *
					 *     XXX
					 *     Not yet safe for non-kernel use.
					 */

#define		MEMORY_OBJECT_COPY_INVALID	5
					/* ...	An invalid copy strategy,
					 *	for external objects which
					 *	have not been initialized.
					 *	Allows copy_strategy to be
					 *	examined without also
					 *	examining pager_ready and
					 *	internal.
					 */

typedef	int		memory_object_return_t;
					/* Which pages to return to manager
					   this time (lock_request) */
#define		MEMORY_OBJECT_RETURN_NONE	0
					/* ... don't return any. */
#define		MEMORY_OBJECT_RETURN_DIRTY	1
					/* ... only dirty pages. */
#define		MEMORY_OBJECT_RETURN_ALL	2
					/* ... dirty and precious pages. */
#define		MEMORY_OBJECT_RETURN_ANYTHING	3
					/* ... any resident page. */

#define		MEMORY_OBJECT_NULL	MACH_PORT_NULL


/*
 *	Types for the memory object flavor interfaces
 */

#define MEMORY_OBJECT_INFO_MAX      (1024) 
typedef int     *memory_object_info_t;      
typedef int	 memory_object_flavor_t;
typedef int      memory_object_info_data_t[MEMORY_OBJECT_INFO_MAX];


#define OLD_MEMORY_OBJECT_BEHAVIOR_INFO 	10	
#define MEMORY_OBJECT_PERFORMANCE_INFO	11
#define OLD_MEMORY_OBJECT_ATTRIBUTE_INFO	12
#define MEMORY_OBJECT_ATTRIBUTE_INFO	14
#define MEMORY_OBJECT_BEHAVIOR_INFO 	15	


struct old_memory_object_behave_info {
	memory_object_copy_strategy_t	copy_strategy;	
	boolean_t			temporary;
	boolean_t			invalidate;
};

struct memory_object_perf_info {
	vm_size_t			cluster_size;
	boolean_t			may_cache;
};

struct old_memory_object_attr_info {			/* old attr list */
        boolean_t       		object_ready;
        boolean_t       		may_cache;
        memory_object_copy_strategy_t 	copy_strategy;
};

struct memory_object_attr_info {
	memory_object_copy_strategy_t	copy_strategy;
	vm_offset_t			cluster_size;
	boolean_t			may_cache_object;
	boolean_t			temporary;
};

struct memory_object_behave_info {
	memory_object_copy_strategy_t	copy_strategy;	
	boolean_t			temporary;
	boolean_t			invalidate;
	boolean_t			silent_overwrite;
	boolean_t			advisory_pageout;
};

typedef struct old_memory_object_behave_info *old_memory_object_behave_info_t;
typedef struct old_memory_object_behave_info old_memory_object_behave_info_data_t;

typedef struct memory_object_behave_info *memory_object_behave_info_t;
typedef struct memory_object_behave_info memory_object_behave_info_data_t;

typedef struct memory_object_perf_info 	*memory_object_perf_info_t;
typedef struct memory_object_perf_info	memory_object_perf_info_data_t;

typedef struct old_memory_object_attr_info *old_memory_object_attr_info_t;
typedef struct old_memory_object_attr_info old_memory_object_attr_info_data_t;

typedef struct memory_object_attr_info	*memory_object_attr_info_t;
typedef struct memory_object_attr_info	memory_object_attr_info_data_t;

#define OLD_MEMORY_OBJECT_BEHAVE_INFO_COUNT   	\
                (sizeof(old_memory_object_behave_info_data_t)/sizeof(int))
#define MEMORY_OBJECT_BEHAVE_INFO_COUNT   	\
                (sizeof(memory_object_behave_info_data_t)/sizeof(int))
#define MEMORY_OBJECT_PERF_INFO_COUNT		\
		(sizeof(memory_object_perf_info_data_t)/sizeof(int))
#define OLD_MEMORY_OBJECT_ATTR_INFO_COUNT		\
		(sizeof(old_memory_object_attr_info_data_t)/sizeof(int))
#define MEMORY_OBJECT_ATTR_INFO_COUNT		\
		(sizeof(memory_object_attr_info_data_t)/sizeof(int))

#define invalid_memory_object_flavor(f)					\
	(f != MEMORY_OBJECT_ATTRIBUTE_INFO && 				\
	 f != MEMORY_OBJECT_PERFORMANCE_INFO && 			\
	 f != OLD_MEMORY_OBJECT_BEHAVIOR_INFO &&			\
	 f != MEMORY_OBJECT_BEHAVIOR_INFO &&				\
	 f != OLD_MEMORY_OBJECT_ATTRIBUTE_INFO)


/*
 *  Even before we have components, we do not want to export upl internal
 *  structure to non mach components.
 */
#ifndef	MACH_KERNEL_PRIVATE
typedef mach_port_t		upl_t; 
#endif

struct upl_page_info {
	vm_offset_t	phys_addr;
        unsigned int
                        pageout:1,      /* page is to be removed on commit */
                        absent:1,       /* No valid data in this page */
                        dirty:1,        /* Page must be cleaned (O) */
			precious:1,     /* must be cleaned, we have only copy */
                        :0;		/* force to long boundary */
};

typedef struct upl_page_info	upl_page_info_t;

typedef unsigned long long	memory_object_offset_t;
typedef unsigned long long	vm_object_offset_t;
typedef upl_page_info_t		*upl_page_list_ptr_t;
typedef mach_port_t		upl_object_t;



/* upl invocation flags */

#define UPL_COPYOUT_FROM	0x1
#define UPL_PRECIOUS		0x2
#define UPL_NO_SYNC		0x4
#define UPL_CLEAN_IN_PLACE	0x8
#define UPL_NOBLOCK		0x10
#define UPL_RET_ONLY_DIRTY	0x20
#define UPL_SET_INTERNAL	0x40

/* upl abort error flags */
#define UPL_ABORT_RESTART	0x1
#define UPL_ABORT_UNAVAILABLE	0x2
#define UPL_ABORT_ERROR		0x4
#define UPL_ABORT_FREE_ON_EMPTY	0x8

/* upl pages check flags */
#define UPL_CHECK_DIRTY         0x1


/* access macros for upl_t */

#define UPL_PAGE_PRESENT(upl, index)  \
	((upl)[(index)].phys_addr != 0)

#define UPL_PHYS_PAGE(upl, index) \
	((upl)[(index)].phys_addr != 0) ?  ((upl)[(index)].phys_addr) : NULL

#define UPL_DIRTY_PAGE(upl, index) \
	((upl)[(index)].phys_addr != 0) ? ((upl)[(index)].dirty) : FALSE

#define UPL_PRECIOUS_PAGE(upl, index) \
	((upl)[(index)].phys_addr != 0) ? ((upl)[(index)].precious) : FALSE

#define UPL_VALID_PAGE(upl, index) \
	((upl)[(index)].phys_addr != 0) ? (!((upl)[(index)].absent)) : FALSE

#define UPL_SET_PAGE_FREE_ON_COMMIT(upl, index) \
	if ((upl)[(index)].phys_addr != 0)     \
		((upl)[(index)].pageout) =  TRUE

#define UPL_CLR_PAGE_FREE_ON_COMMIT(upl, index) \
	if ((upl)[(index)].phys_addr != 0)     \
		((upl)[(index)].pageout) =  FALSE


#ifdef MACH_KERNEL
#include <mach/error.h>

/* The call prototyped below is used strictly by UPL_GET_INTERNAL_PAGE_LIST */

extern vm_size_t	upl_offset_to_pagelist;
extern vm_size_t upl_get_internal_pagelist_offset();

/* UPL_GET_INTERNAL_PAGE_LIST is only valid on internal objects where the */
/* list request was made with the UPL_INTERNAL flag */

#define UPL_GET_INTERNAL_PAGE_LIST(upl) \
	((upl_offset_to_pagelist == 0) ?  \
	upl + (upl_offset_to_pagelist = upl_get_internal_pagelist_offset()): \
	upl + upl_offset_to_pagelist) 

extern kern_return_t	vm_fault_list_request(
					vm_object_t		object,
					vm_object_offset_t	offset,
					vm_size_t		size,
					upl_t			*upl,
					upl_page_info_t	      **user_page_list,
					int			page_list_count,
					int			cntrol_flags);

extern kern_return_t	upl_system_list_request(
					vm_object_t		object,
					vm_object_offset_t	offset,
					vm_size_t		size,
					vm_size_t		super_size,
					upl_t			*upl,
					upl_page_info_t	      **user_page_list,
					int			page_list_count,
					int			cntrol_flags);

extern kern_return_t	upl_map(
					vm_map_t	map,
					upl_t		upl,
					vm_offset_t	*dst_addr);

extern kern_return_t	upl_un_map(
					vm_map_t	map,
					upl_t		upl);

extern kern_return_t	upl_commit_range(
					upl_t		upl,
					vm_offset_t	offset,
					vm_size_t	size,
					boolean_t	free_on_empty,
					upl_page_info_t	*page_list);

extern kern_return_t	upl_commit(
					upl_t		upl,
					upl_page_info_t	*page_list);

extern upl_t		upl_create(
					boolean_t	internal);

extern void 		upl_destroy(
					upl_t		page_list);

extern kern_return_t	upl_abort(
					upl_t		page_list,
					int		error);

extern kern_return_t	upl_abort_range(
					upl_t		page_list,
					vm_offset_t	offset,
					vm_size_t	size,
					int		error);

extern void upl_set_dirty(
       					 upl_t   upl);

extern void upl_clear_dirty(
       					 upl_t   upl);


#endif /* MACH_KERNEL */



#endif	/* _MACH_MEMORY_OBJECT_TYPES_H_ */
