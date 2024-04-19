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
/*
 *	File:	vm/memory_object.c
 *	Author:	Michael Wayne Young
 *
 *	External memory management interface control functions.
 */

#ifdef MACH_BSD
/* THIS code should be removed when the component merge is completed */
extern int vnode_pager_workaround;
#endif

#include <advisory_pageout.h>

/*
 *	Interface dependencies:
 */

#include <mach/std_types.h>	/* For pointer_t */
#include <mach/mach_types.h>

#include <mach/kern_return.h>
#include <mach/memory_object.h>
#include <mach/memory_object_default.h>
#include <mach/memory_object_control_server.h>
#include <mach/mach_host_server.h>
#include <mach/boolean.h>
#include <mach/vm_prot.h>
#include <mach/message.h>

#include <vm/vm_object.h>
#include <vm/vm_fault.h>
/*
 *	Implementation dependencies:
 */
#include <string.h>		/* For memcpy() */

#include <vm/memory_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pageout.h>
#include <vm/pmap.h>		/* For pmap_clear_modify */
#include <kern/xpr.h>		
#include <kern/thread.h>	/* For current_thread() */
#include <kern/host.h>
#include <vm/vm_kern.h>		/* For kernel_map, vm_move */
#include <vm/vm_map.h>		/* For vm_map_pageable */
#include <ipc/ipc_port.h>
#include <ipc/ipc_space.h>



#include <kern/misc_protos.h>

#if	MACH_PAGEMAP
#include <vm/vm_external.h>
#endif	/* MACH_PAGEMAP */

ipc_port_t	memory_manager_default = IP_NULL;
vm_size_t	memory_manager_default_cluster = 0;
decl_mutex_data(,memory_manager_default_lock)

/*
 *	Forward ref to file-local function:
 */
boolean_t
memory_object_update(vm_object_t,
	vm_offset_t, vm_size_t, memory_object_return_t, boolean_t, vm_prot_t);

/*
 *	Important note:
 *		All of the external (user interface) routines gain a reference
 *		to the object (first argument) as part of the automatic
 *		argument conversion. Explicit deallocation is necessary.
 */

kern_return_t
memory_object_data_supply(
	register
        vm_object_t		object,
	register
	vm_offset_t		offset,
	vm_offset_t		data,
	mach_msg_type_number_t	data_cnt,
	vm_prot_t		lock_value,
	boolean_t		precious,
	ipc_port_t		reply_to,
	mach_msg_type_name_t	reply_to_type)
{
	vm_map_copy_t	data_copy = (vm_map_copy_t)data;
	kern_return_t	result = KERN_SUCCESS;
	vm_offset_t	error_offset = 0;
	register
	vm_page_t	m;
	register
	vm_page_t	data_m;
	vm_size_t	original_length;
	vm_offset_t	original_offset;
	vm_page_t	*page_list;
	boolean_t	was_absent;
	vm_map_copy_t	orig_copy = data_copy;
	boolean_t	was_clustered;


        XPR(XPR_MEMORY_OBJECT,
            "m_o_data_supply, object 0x%X, offset 0x%X data_cnt 0x%X\n",
            (integer_t)object, offset, data_cnt, 0, 0);

	/*
	 *	Look for bogus arguments
	 */

	if (object == VM_OBJECT_NULL) {
		return(KERN_INVALID_ARGUMENT);
	}

	if (lock_value & ~VM_PROT_ALL) {
		vm_object_deallocate(object);
		return(KERN_INVALID_ARGUMENT);
	}

	if (! page_aligned(data_cnt)) {
	    vm_object_deallocate(object);
	    return(KERN_INVALID_ARGUMENT);
	}

	/*
	 *	Adjust the offset from the memory object to the offset
	 *	within the vm_object.
	 */

	original_length = data_cnt;
	original_offset = offset;

	assert(data_copy->type == VM_MAP_COPY_PAGE_LIST);
	page_list = &data_copy->cpy_page_list[0];

	vm_object_lock(object);
	vm_object_paging_begin(object);
	offset -= object->paging_offset;

	/*
	 *	Loop over copy stealing pages for pagein.
	 */

	for (; data_cnt > 0 ; data_cnt -= PAGE_SIZE, offset += PAGE_SIZE) {

		assert(data_copy != VM_MAP_COPY_NULL);
		assert(data_copy->type == VM_MAP_COPY_PAGE_LIST);
		assert(data_copy->cpy_npages > 0);
		data_m = *page_list;

		assert( !(data_m == VM_PAGE_NULL || data_m->tabled ||
		    data_m->error || data_m->absent || data_m->fictitious ||
		    data_m->restart));

		/*
		 *	Look up target page and check its state.
		 */

retry_lookup:
		m = vm_page_lookup(object,offset);
		was_clustered = FALSE;
		if (m == VM_PAGE_NULL) {
		    was_absent = FALSE;
		}
		else {
		    if (m->absent) {

			/*
			 *	Page was requested.  Free the busy
			 *	page waiting for it.  Insertion
			 *	of new page happens below.
			 */
			m->list_req_pending = FALSE;
			if (m->clustered) {
				was_clustered = TRUE;
				m->clustered = FALSE;
			}
			if (m->busy) {
				VM_PAGE_FREE(m);
			}
			was_absent = TRUE;
		    }
		    else {

			/*
			 *	Have to wait for page that is busy and
			 *	not absent.  This is probably going to
			 *	be an error, but go back and check.
			 */
			if (m->busy) {
				PAGE_ASSERT_WAIT(m, THREAD_UNINT);
				vm_object_unlock(object);
				thread_block((void (*)(void))0);
				vm_object_lock(object);
				goto retry_lookup;
			}

			/*
			 *	Page already present; error.
			 *	This is an error if data is precious.
			 */
			result = KERN_MEMORY_PRESENT;
			error_offset = offset + object->paging_offset;

			break;
		    }
		}

		/*
		 *	Ok to pagein page.  Target object now has no page
		 *	at offset.  Set the page parameters, then drop
		 *	in new page and set up pageout state.  Object is
		 *	still locked here.
		 *
		 *	Must clear busy bit in page before inserting it.
		 *	Ok to skip wakeup logic because nobody else
		 *	can possibly know about this page.
		 */

		data_m->lock_supplied = TRUE;
		data_m->busy = FALSE;
		data_m->dirty = FALSE;
		pmap_clear_modify(data_m->phys_addr);

		data_m->page_lock = lock_value;
		if (lock_value != VM_PROT_NONE)
			data_m->unusual = TRUE;
		else
			data_m->unusual = FALSE;

		data_m->unlock_request = VM_PROT_NONE;
		data_m->precious = precious;

		data_m->clustered = was_clustered;

		vm_page_insert(data_m, object, offset);

		vm_page_lock_queues();
		if (was_absent)
			vm_page_activate(data_m);
		else
			vm_page_deactivate(data_m);
		vm_page_unlock_queues();

		/*
		 *	Null out this page list entry, and advance to next
		 *	page.
		 */

		*page_list++ = VM_PAGE_NULL;

		if (--(data_copy->cpy_npages) == 0 &&
		    vm_map_copy_has_cont(data_copy)) {
			vm_map_copy_t	new_copy;

			vm_object_unlock(object);
			vm_map_copy_invoke_cont(data_copy, &new_copy, &result);
			vm_object_lock(object);

			if (result == KERN_SUCCESS) {

			    /*
			     *	Consume on success requires that
			     *	we keep the original vm_map_copy
			     *	around in case something fails.
			     *	Free the old copy if it's not the original
			     */
			    if (data_copy != orig_copy) {
				vm_map_copy_discard(data_copy);
			    }

			    if ((data_copy = new_copy) != VM_MAP_COPY_NULL)
				page_list = &data_copy->cpy_page_list[0];
			}
			else {
			    error_offset = offset + object->paging_offset +
						PAGE_SIZE;
			    break;
			}
		}
	}

	vm_object_paging_end(object);

	/*
	 *	If there is a continuation that has not been invoked,
	 *	there must have been an error.	Abort the continuation.
	 */

	if (data_copy != VM_MAP_COPY_NULL && vm_map_copy_has_cont(data_copy)) {
		vm_object_unlock(object);
		vm_map_copy_abort_cont(data_copy);
		vm_object_lock(object);
	}

	/*
	 *	If the pager wants a reply, send it one.
	 */

	if (IP_VALID(reply_to)) {
		vm_object_unlock(object);
		memory_object_supply_completed(
				reply_to, reply_to_type,
				object->pager_request,
				original_offset,
				original_length,
				result,
				error_offset);
	}
	else
		vm_object_unlock(object);

	vm_object_deallocate(object);

	/*
	 *	Consume on success:  The final data copy must be
	 *	discarded if it is not the original.  The original
	 *	gets discarded only if this routine succeeds.
	 */
	if (data_copy != VM_MAP_COPY_NULL &&
	    data_copy != orig_copy)
		vm_map_copy_discard(data_copy);
	if (result == KERN_SUCCESS)
		vm_map_copy_discard(orig_copy);

	return(result);
}

kern_return_t
memory_object_data_error(
	vm_object_t	object,
	vm_offset_t	offset,
	vm_size_t	size,
	kern_return_t	error_value)
{
        XPR(XPR_MEMORY_OBJECT,
            "m_o_data_error, object 0x%X, offset 0x%X size 0x%X\n",
            (integer_t)object, offset, size, 0, 0);

	if (object == VM_OBJECT_NULL)
		return(KERN_INVALID_ARGUMENT);

	if (size != round_page(size)) {
		vm_object_deallocate(object);
		return(KERN_INVALID_ARGUMENT);
	}

	vm_object_lock(object);
	offset -= object->paging_offset;

	while (size != 0) {
		register vm_page_t m;

		m = vm_page_lookup(object, offset);
		if ((m != VM_PAGE_NULL) && m->busy && m->absent) {
			if (error_value == KERN_MEMORY_DATA_MOVED) {
				m->restart = TRUE;
			} else {
				if (error_value > KERN_RETURN_MAX) {
					m->page_error = error_value;
				} else {
					m->page_error = KERN_MEMORY_ERROR;
				}
				m->error = TRUE;
			}

			/*
			 *	m->unusual was true because page was absent.
			 *	It remains true because page is either error
			 *	or restart.
			 */
			m->absent = FALSE;
			vm_object_absent_release(object);

			PAGE_WAKEUP_DONE(m);

			/*
			 * If this page was not requested (clustered is set),
			 * then throw it away since there is no one to which
			 * an error can be reported.
			 */
			vm_page_lock_queues();
			if (m->clustered)
				vm_page_free(m);
			else
				vm_page_activate(m);
			vm_page_unlock_queues();
		}

		size -= PAGE_SIZE;
		offset += PAGE_SIZE;
	 }
	vm_object_unlock(object);

	vm_object_deallocate(object);
	return(KERN_SUCCESS);
}

kern_return_t
memory_object_data_unavailable(
	vm_object_t	object,
	vm_offset_t	offset,
	vm_size_t	size)
{
        XPR(XPR_MEMORY_OBJECT,
            "m_o_data_unavailable, object 0x%X, offset 0x%X size 0x%X\n",
            (integer_t)object, offset, size, 0, 0);

	if (object == VM_OBJECT_NULL)
		return(KERN_INVALID_ARGUMENT);

	if (size != round_page(size)) {
		vm_object_deallocate(object);
		return(KERN_INVALID_ARGUMENT);
	}

	vm_object_lock(object);
	offset -= object->paging_offset;

	while (size != 0) {
		register vm_page_t m;

		/*
		 *	We're looking for pages that are both busy and
		 *	absent (waiting to be filled), converting them
		 *	to just absent.
		 *
		 *	Pages that are just busy can be ignored entirely.
		 */

		m = vm_page_lookup(object, offset);
		if (m == VM_PAGE_NULL) {
			if (object->absent_count < vm_object_absent_max)
				m = vm_page_grab_fictitious();
			if (m != VM_PAGE_NULL) {
				m->absent = TRUE;
				m->unusual = TRUE;
				m->clustered = FALSE;
				object->absent_count++;
				vm_page_lock_queues();
				vm_page_insert(m, object, offset);
				vm_page_activate(m);
				vm_page_unlock_queues();
				PAGE_WAKEUP_DONE(m);
			}
		} else if (m->busy && m->absent) {
			/* only consider real pageins in clustering stats */
			m->clustered = FALSE;

			PAGE_WAKEUP_DONE(m);

			vm_page_lock_queues();
			vm_page_activate(m);
			vm_page_unlock_queues();
		}
		size -= PAGE_SIZE;
		offset += PAGE_SIZE;
	}

	vm_object_unlock(object);

	vm_object_deallocate(object);
	return(KERN_SUCCESS);
}

/*
 *	Routine:	memory_object_should_return_page
 *
 *	Description:
 *		Determine whether the given page should be returned,
 *		based on the page's state and on the given return policy.
 *
 *		We should return the page if one of the following is true:
 *
 *		1. Page is dirty and should_return is not RETURN_NONE.
 *		2. Page is precious and should_return is RETURN_ALL.
 *		3. Should_return is RETURN_ANYTHING.
 *
 *		As a side effect, m->dirty will be made consistent
 *		with pmap_is_modified(m), if should_return is not
 *		MEMORY_OBJECT_RETURN_NONE.
 */

#define	memory_object_should_return_page(m, should_return) \
    (should_return != MEMORY_OBJECT_RETURN_NONE && \
     (((m)->dirty || ((m)->dirty = pmap_is_modified((m)->phys_addr))) || \
      ((m)->precious && (should_return) == MEMORY_OBJECT_RETURN_ALL) || \
      (should_return) == MEMORY_OBJECT_RETURN_ANYTHING))

typedef	int	memory_object_lock_result_t;

#define MEMORY_OBJECT_LOCK_RESULT_DONE          0
#define MEMORY_OBJECT_LOCK_RESULT_MUST_BLOCK    1
#define MEMORY_OBJECT_LOCK_RESULT_MUST_CLEAN    2
#define MEMORY_OBJECT_LOCK_RESULT_MUST_RETURN   3

memory_object_lock_result_t memory_object_lock_page(
				vm_page_t		m,
				memory_object_return_t	should_return,
				boolean_t		should_flush,
				vm_prot_t		prot);

/*
 *	Routine:	memory_object_lock_page
 *
 *	Description:
 *		Perform the appropriate lock operations on the
 *		given page.  See the description of
 *		"memory_object_lock_request" for the meanings
 *		of the arguments.
 *
 *		Returns an indication that the operation
 *		completed, blocked, or that the page must
 *		be cleaned.
 */
memory_object_lock_result_t
memory_object_lock_page(
	vm_page_t		m,
	memory_object_return_t	should_return,
	boolean_t		should_flush,
	vm_prot_t		prot)
{
        XPR(XPR_MEMORY_OBJECT,
            "m_o_lock_page, page 0x%X rtn %d flush %d prot %d\n",
            (integer_t)m, should_return, should_flush, prot, 0);

	/*
	 *	Don't worry about pages for which the kernel
	 *	does not have any data.
	 */

	if (m->absent || m->error || m->restart)
		return(MEMORY_OBJECT_LOCK_RESULT_DONE);

	/*
	 *	If we cannot change access to the page,
	 *	either because a mapping is in progress
	 *	(busy page) or because a mapping has been
	 *	wired, then give up.
	 */

	if (m->busy)
		return(MEMORY_OBJECT_LOCK_RESULT_MUST_BLOCK);

	assert(!m->fictitious);

	if (m->wire_count != 0) {
		/*
		 *	If no change would take place
		 *	anyway, return successfully.
		 *
		 *	No change means:
		 *		Not flushing AND
		 *		No change to page lock [2 checks]  AND
		 *		Should not return page
		 *
		 * XXX	This doesn't handle sending a copy of a wired
		 * XXX	page to the pager, but that will require some
		 * XXX	significant surgery.
		 */
		if (!should_flush &&
		    (m->page_lock == prot || prot == VM_PROT_NO_CHANGE) &&
		    ! memory_object_should_return_page(m, should_return)) {

			/*
			 *	Restart page unlock requests,
			 *	even though no change took place.
			 *	[Memory managers may be expecting
			 *	to see new requests.]
			 */
			m->unlock_request = VM_PROT_NONE;
			PAGE_WAKEUP(m);

			return(MEMORY_OBJECT_LOCK_RESULT_DONE);
		}

		return(MEMORY_OBJECT_LOCK_RESULT_MUST_BLOCK);
	}

	/*
	 *	If the page is to be flushed, allow
	 *	that to be done as part of the protection.
	 */

	if (should_flush)
		prot = VM_PROT_ALL;

	/*
	 *	Set the page lock.
	 *
	 *	If we are decreasing permission, do it now;
	 *	let the fault handler take care of increases
	 *	(pmap_page_protect may not increase protection).
	 */

	if (prot != VM_PROT_NO_CHANGE) {
		if ((m->page_lock ^ prot) & prot) {
			pmap_page_protect(m->phys_addr, VM_PROT_ALL & ~prot);
		}
		m->page_lock = prot;
		m->lock_supplied = TRUE;
		if (prot != VM_PROT_NONE)
			m->unusual = TRUE;
		else
			m->unusual = FALSE;

		/*
		 *	Restart any past unlock requests, even if no
		 *	change resulted.  If the manager explicitly
		 *	requested no protection change, then it is assumed
		 *	to be remembering past requests.
		 */

		m->unlock_request = VM_PROT_NONE;
		PAGE_WAKEUP(m);
	}

	/*
	 *	Handle page returning.
	 */

	if (memory_object_should_return_page(m, should_return)) {

		/*
		 *	If we weren't planning
		 *	to flush the page anyway,
		 *	we may need to remove the
		 *	page from the pageout
		 *	system and from physical
		 *	maps now.
		 */
		
		vm_page_lock_queues();
		VM_PAGE_QUEUES_REMOVE(m);
		vm_page_unlock_queues();

		if (!should_flush)
			pmap_page_protect(m->phys_addr, VM_PROT_NONE);

		if (m->dirty)
			return(MEMORY_OBJECT_LOCK_RESULT_MUST_CLEAN);
		else
			return(MEMORY_OBJECT_LOCK_RESULT_MUST_RETURN);
	}

	/*
	 *	Handle flushing
	 */

	if (should_flush) {
		VM_PAGE_FREE(m);
	} else {
		extern boolean_t vm_page_deactivate_hint;

		/*
		 *	XXX Make clean but not flush a paging hint,
		 *	and deactivate the pages.  This is a hack
		 *	because it overloads flush/clean with
		 *	implementation-dependent meaning.  This only
		 *	happens to pages that are already clean.
		 */

		if (vm_page_deactivate_hint &&
		    (should_return != MEMORY_OBJECT_RETURN_NONE)) {
			vm_page_lock_queues();
			vm_page_deactivate(m);
			vm_page_unlock_queues();
		}
	}

	return(MEMORY_OBJECT_LOCK_RESULT_DONE);
}
#define LIST_REQ_PAGEOUT_PAGES(object, new_object, new_offset, action, po) \
MACRO_BEGIN								\
									\
	register int		i;                                      \
	register vm_page_t	hp;					\
									\
	vm_object_unlock(object);					\
									\
	if(((rpc_subsystem_t)pager_mux_hash_lookup(object->pager)) ==	\
	((rpc_subsystem_t) &vnode_pager_workaround)) {			\
       	 	(void) vnode_pager_data_return(object->pager,		\
		object->pager_request,					\
		po,							\
		POINTER_T(0),						\
		new_offset,						\
		(action == MEMORY_OBJECT_LOCK_RESULT_MUST_CLEAN),	\
		!should_flush);                                 	\
	} else {							\
	   	(void) memory_object_data_return(object->pager,		\
		object->pager_request,					\
		po,							\
		POINTER_T(0),						\
		new_offset,						\
		(action == MEMORY_OBJECT_LOCK_RESULT_MUST_CLEAN),	\
		!should_flush);                                 	\
	}								\
									\
	vm_object_lock(object);						\
									\
        new_object = VM_OBJECT_NULL;					\
MACRO_END

#ifdef MACH_BSD
#define PAGEOUT_PAGES(object, new_object, new_offset, action, po)	\
MACRO_BEGIN								\
									\
	vm_map_copy_t		copy;					\
	register int		i;                                      \
	register vm_page_t	hp;					\
									\
	vm_object_unlock(object);					\
									\
	(void) vm_map_copyin_object(new_object, 0, new_offset, &copy);	\
									\
	if(((rpc_subsystem_t)pager_mux_hash_lookup(object->pager)) ==	\
	((rpc_subsystem_t) &vnode_pager_workaround)) {			\
       	 	(void) vnode_pager_data_return(object->pager,		\
		object->pager_request,					\
		po,							\
		POINTER_T(copy),					\
		new_offset,						\
		(action == MEMORY_OBJECT_LOCK_RESULT_MUST_CLEAN),	\
		!should_flush);                                 	\
	} else {							\
	   	(void) memory_object_data_return(object->pager,		\
		object->pager_request,					\
		po,							\
		POINTER_T(copy),					\
		new_offset,						\
		(action == MEMORY_OBJECT_LOCK_RESULT_MUST_CLEAN),	\
		!should_flush);                                 	\
	}								\
									\
	vm_object_lock(object);						\
									\
	for (i = 0; i < atop(new_offset); i++) {			\
		hp = holding_pages[i];					\
		if (hp != VM_PAGE_NULL) {				\
			vm_object_paging_end(object);			\
			VM_PAGE_FREE(hp);				\
		}							\
	}								\
									\
        new_object = VM_OBJECT_NULL;					\
MACRO_END
#else
#define PAGEOUT_PAGES(object, new_object, new_offset, action, po)	\
MACRO_BEGIN								\
									\
	vm_map_copy_t		copy;					\
	register int		i;                                      \
	register vm_page_t	hp;					\
									\
	vm_object_unlock(object);					\
									\
	(void) vm_map_copyin_object(new_object, 0, new_offset, &copy);	\
									\
	(void) memory_object_data_return(				\
			object->pager,					\
			object->pager_request,				\
			po,						\
			POINTER_T(copy),				\
			new_offset,					\
		(action == MEMORY_OBJECT_LOCK_RESULT_MUST_CLEAN),	\
			!should_flush);                                 \
									\
	vm_object_lock(object);						\
									\
	for (i = 0; i < atop(new_offset); i++) {			\
		hp = holding_pages[i];					\
		if (hp != VM_PAGE_NULL) {				\
			vm_object_paging_end(object);			\
			VM_PAGE_FREE(hp);				\
		}							\
	}								\
									\
        new_object = VM_OBJECT_NULL;					\
MACRO_END
#endif

/*
 *	Routine:	memory_object_lock_request [user interface]
 *
 *	Description:
 *		Control use of the data associated with the given
 *		memory object.  For each page in the given range,
 *		perform the following operations, in order:
 *			1)  restrict access to the page (disallow
 *			    forms specified by "prot");
 *			2)  return data to the manager (if "should_return"
 *			    is RETURN_DIRTY and the page is dirty, or
 * 			    "should_return" is RETURN_ALL and the page
 *			    is either dirty or precious); and,
 *			3)  flush the cached copy (if "should_flush"
 *			    is asserted).
 *		The set of pages is defined by a starting offset
 *		("offset") and size ("size").  Only pages with the
 *		same page alignment as the starting offset are
 *		considered.
 *
 *		A single acknowledgement is sent (to the "reply_to"
 *		port) when these actions are complete.  If successful,
 *		the naked send right for reply_to is consumed.
 */

kern_return_t
memory_object_lock_request(
	register vm_object_t	object,
	register vm_offset_t	offset,
	register vm_size_t	size,
	memory_object_return_t	should_return,
	boolean_t		should_flush,
	vm_prot_t		prot,
	ipc_port_t		reply_to,
	mach_msg_type_name_t	reply_to_type)
{
	vm_offset_t		original_offset = offset;

        XPR(XPR_MEMORY_OBJECT,
	    "m_o_lock_request, obj 0x%X off 0x%X size 0x%X flags %X prot %X\n",
	    (integer_t)object, offset, size, 
 	    (((should_return&1)<<1)|should_flush), prot);

	/*
	 *	Check for bogus arguments.
	 */
	if (object == VM_OBJECT_NULL)
		return (KERN_INVALID_ARGUMENT);

	if ((prot & ~VM_PROT_ALL) != 0 && prot != VM_PROT_NO_CHANGE) {
		vm_object_deallocate(object);
		return (KERN_INVALID_ARGUMENT);
	}

	size = round_page(size);

	/*
	 *	Lock the object, and acquire a paging reference to
	 *	prevent the memory_object and control ports from
	 *	being destroyed.
	 */

	vm_object_lock(object);
	vm_object_paging_begin(object);
	offset -= object->paging_offset;

	(void)memory_object_update(object,
		offset, size, should_return, should_flush, prot);

	if (IP_VALID(reply_to)) {
		vm_object_unlock(object);

		/* consumes our naked send-once/send right for reply_to */
		(void) memory_object_lock_completed(reply_to, reply_to_type,
			object->pager_request, original_offset, size);

		vm_object_lock(object);
	}

	vm_object_paging_end(object);
	vm_object_unlock(object);
	vm_object_deallocate(object);

	return (KERN_SUCCESS);
}

/*
 *	Routine:	memory_object_sync
 *
 *	Kernel internal function to synch out pages in a given
 *	range within an object to its memory manager.  Much the
 *	same as memory_object_lock_request but page protection
 *	is not changed.
 *
 *	If the should_flush and should_return flags are true pages
 *	are flushed, that is dirty & precious pages are written to
 *	the memory manager and then discarded.  If should_return
 *	is false, only precious pages are returned to the memory
 *	manager.
 *
 *	If should flush is false and should_return true, the memory
 *	manager's copy of the pages is updated.  If should_return
 *	is also false, only the precious pages are updated.  This
 *	last option is of limited utility.
 *
 *	Returns:
 *	FALSE		if no pages were returned to the pager
 *	TRUE		otherwise.
 */

boolean_t
memory_object_sync(
	vm_object_t	object,
	vm_offset_t	offset,
	vm_size_t	size,
	boolean_t	should_flush,
	boolean_t	should_return)
{
	boolean_t	rv;

        XPR(XPR_MEMORY_OBJECT,
            "m_o_sync, object 0x%X, offset 0x%X size 0x%x flush %d rtn %d\n",
            (integer_t)object, offset, size, should_flush, should_return);

	/*
	 * Lock the object, and acquire a paging reference to
	 * prevent the memory_object and control ports from
	 * being destroyed.
	 */
	vm_object_lock(object);
	vm_object_paging_begin(object);

	rv = memory_object_update(object, offset, size,
		(should_return) ?
			MEMORY_OBJECT_RETURN_ALL :
			MEMORY_OBJECT_RETURN_NONE,
		should_flush,
		VM_PROT_NO_CHANGE);


	vm_object_paging_end(object);
	vm_object_unlock(object);
	return rv;
}

/*
 *	Routine:	memory_object_update
 *	Description:
 *		Work function for m_o_lock_request(), m_o_sync().
 *
 *		Called with object locked and paging ref taken.
 */
kern_return_t
memory_object_update(
	register vm_object_t	object,
	register vm_offset_t	offset,
	register vm_size_t	size,
	memory_object_return_t	should_return,
	boolean_t		should_flush,
	vm_prot_t		prot)
{
	register vm_page_t	m;
	vm_page_t		holding_page;
	vm_size_t		original_size = size;
	vm_offset_t		paging_offset = 0;
	vm_object_t		new_object = VM_OBJECT_NULL;
	vm_object_t		copy_object;
	vm_offset_t		new_offset = 0;
	vm_offset_t		last_offset = offset;
	memory_object_lock_result_t	page_lock_result;
	memory_object_lock_result_t	pageout_action;
	vm_page_t		holding_pages[DATA_WRITE_MAX];
	boolean_t		data_returned = FALSE;
#ifndef NOT_LIST_REQ
	boolean_t		pending_pageout = FALSE;
#endif

	/*
	 *	To avoid blocking while scanning for pages, save
	 *	dirty pages to be cleaned all at once.
	 *
	 *	XXXO A similar strategy could be used to limit the
	 *	number of times that a scan must be restarted for
	 *	other reasons.  Those pages that would require blocking
	 *	could be temporarily collected in another list, or
	 *	their offsets could be recorded in a small array.
	 */

	/*
	 * XXX	NOTE: May want to consider converting this to a page list
	 * XXX	vm_map_copy interface.  Need to understand object
	 * XXX	coalescing implications before doing so.
	 */


	if(((copy_object = object->copy) != NULL) && should_flush) {
		vm_size_t	i;
		vm_size_t	copy_size;
		vm_offset_t	copy_offset;
		vm_prot_t	prot;
		vm_page_t	page;
		vm_page_t	top_page;
		kern_return_t	error = 0;



		/* translate offset with respect to shadow's offset */
		copy_offset = (offset >= copy_object->shadow_offset)?
			offset - copy_object->shadow_offset :
			(vm_offset_t)0;
		if(copy_offset > copy_object->size)
			copy_offset = copy_object->size;

		/* clip size with respect to shadow offset */
		copy_size = (offset >= copy_object->shadow_offset) ?
			size : size - (copy_object->shadow_offset - offset);

		if(copy_size <= 0) {
			copy_size = 0;
		} else {
			copy_size = ((copy_offset + copy_size) 
				<= copy_object->size) ?
				copy_size : copy_object->size - copy_offset;
		}
		/* check for a copy_offset which is beyond the end of */
		/* the copy_object */
		if(copy_size < 0)
			copy_size = 0;

		vm_object_unlock(object);
		vm_object_lock(copy_object);
		vm_object_paging_begin(copy_object);
		for (i=copy_offset; i<copy_size; i+=PAGE_SIZE) {
	RETRY_COW_OF_LOCK_REQUEST:
			prot = 	VM_PROT_WRITE|VM_PROT_READ;
			switch (vm_fault_page(copy_object, i, 
				VM_PROT_WRITE|VM_PROT_READ,
				FALSE,
				THREAD_UNINT,
				copy_offset,
				copy_offset+copy_size,
				VM_BEHAVIOR_SEQUENTIAL,
				&prot,
				&page,
				&top_page,
				(int *)0,
				&error,
			        FALSE,
				FALSE)) {

			case VM_FAULT_SUCCESS:
				if(top_page) {
					vm_fault_cleanup(
						page->object, top_page);
					PAGE_WAKEUP_DONE(page);
					vm_page_lock_queues();
					if (!page->active && !page->inactive)
						vm_page_activate(page);
					vm_page_unlock_queues();
					vm_object_lock(copy_object);
					vm_object_paging_begin(copy_object);
				} else {
					PAGE_WAKEUP_DONE(page);
					vm_page_lock_queues();
					if (!page->active && !page->inactive)
						vm_page_activate(page);
					vm_page_unlock_queues();
				}
				break;
			case VM_FAULT_RETRY:
				prot = 	VM_PROT_WRITE|VM_PROT_READ;
				vm_object_lock(copy_object);
				vm_object_paging_begin(copy_object);
				goto RETRY_COW_OF_LOCK_REQUEST;
			case VM_FAULT_INTERRUPTED:
				prot = 	VM_PROT_WRITE|VM_PROT_READ;
				vm_object_lock(copy_object);
				vm_object_paging_begin(copy_object);
				goto RETRY_COW_OF_LOCK_REQUEST;
			case VM_FAULT_MEMORY_SHORTAGE:
				VM_PAGE_WAIT();
				prot = 	VM_PROT_WRITE|VM_PROT_READ;
				vm_object_lock(copy_object);
				vm_object_paging_begin(copy_object);
				goto RETRY_COW_OF_LOCK_REQUEST;
			case VM_FAULT_FICTITIOUS_SHORTAGE:
				vm_page_more_fictitious();
				prot = 	VM_PROT_WRITE|VM_PROT_READ;
				vm_object_lock(copy_object);
				vm_object_paging_begin(copy_object);
				goto RETRY_COW_OF_LOCK_REQUEST;
			case VM_FAULT_MEMORY_ERROR:
				vm_object_lock(object);
				goto BYPASS_COW_COPYIN;
			}
				

		}
		vm_object_paging_end(copy_object);
		vm_object_unlock(copy_object);
		vm_object_lock(object);
	}
BYPASS_COW_COPYIN:

	for (;
	     size != 0;
	     size -= PAGE_SIZE, offset += PAGE_SIZE)
	{
	    /*
	     *	Limit the number of pages to be cleaned at once.
	     */
#ifdef NOT_LIST_REQ
	    if (new_object != VM_OBJECT_NULL &&
		    new_offset >= PAGE_SIZE * DATA_WRITE_MAX)
	    {
 		PAGEOUT_PAGES(object, new_object, new_offset, pageout_action,
				paging_offset);
	    }
#else 
/*
	if(((rpc_subsystem_t)pager_mux_hash_lookup(object->pager)) !=
				((rpc_subsystem_t) &vnode_pager_workaround)) {
	    if (new_object != VM_OBJECT_NULL &&
		    new_offset >= PAGE_SIZE * DATA_WRITE_MAX)
	    {
 		PAGEOUT_PAGES(object, new_object, new_offset, pageout_action,
				paging_offset);
	    }
	} else {
*/
	    if (pending_pageout &&
		    new_offset >= PAGE_SIZE * DATA_WRITE_MAX)
	    {
 		LIST_REQ_PAGEOUT_PAGES(object, new_object, 
				new_offset, 
				pageout_action, paging_offset);
		pending_pageout = FALSE;
	    }
/*
	}
*/
#endif

	    while ((m = vm_page_lookup(object, offset)) != VM_PAGE_NULL) {
		page_lock_result = memory_object_lock_page(m, should_return,
					should_flush, prot);

		XPR(XPR_MEMORY_OBJECT,
                    "m_o_update: lock_page, obj 0x%X offset 0x%X result %d\n",
                    (integer_t)object, offset, page_lock_result, 0, 0);

		switch (page_lock_result)
		{
		    case MEMORY_OBJECT_LOCK_RESULT_DONE:
			/*
			 *	End of a cluster of dirty pages.
			 */
#ifdef NOT_LIST_REQ
			if (new_object != VM_OBJECT_NULL) {
 	    		    PAGEOUT_PAGES(object, new_object, new_offset,
					pageout_action, paging_offset);
			    continue;
			}
#else
/*
	if(((rpc_subsystem_t) pager_mux_hash_lookup(object->pager)) !=
				((rpc_subsystem_t) &vnode_pager_workaround)) {
			if (new_object != VM_OBJECT_NULL) {
 	    		    PAGEOUT_PAGES(object, new_object, new_offset,
					pageout_action, paging_offset);
			    continue;
			}
	} else {
*/
			if(pending_pageout) {
 	    		    	LIST_REQ_PAGEOUT_PAGES(object, new_object, 
					new_offset, pageout_action, 
					paging_offset);
				pending_pageout = FALSE;
				continue;
			}
/*
	}
*/
#endif
			break;

		    case MEMORY_OBJECT_LOCK_RESULT_MUST_BLOCK:
			/*
			 *	Since it is necessary to block,
			 *	clean any dirty pages now.
			 */
#ifdef NOT_LIST_REQ
			if (new_object != VM_OBJECT_NULL) {
 	    		    PAGEOUT_PAGES(object, new_object, new_offset,
					pageout_action, paging_offset);
			    continue;
			}
#else
/*
	if(((rpc_subsystem_t) pager_mux_hash_lookup(object->pager)) !=
				((rpc_subsystem_t) &vnode_pager_workaround)) {
			if (new_object != VM_OBJECT_NULL) {
 	    		    PAGEOUT_PAGES(object, new_object, new_offset,
					pageout_action, paging_offset);
			    continue;
			}
	} else {
*/
			if(pending_pageout) {
 	    		    	LIST_REQ_PAGEOUT_PAGES(object, new_object, 
					new_offset, pageout_action, 
					paging_offset);
				pending_pageout = FALSE;
				continue;
			}
/*
	}
*/
#endif

			PAGE_ASSERT_WAIT(m, THREAD_UNINT);
			vm_object_unlock(object);
			thread_block((void (*)(void))0);
			vm_object_lock(object);
			continue;

		    case MEMORY_OBJECT_LOCK_RESULT_MUST_CLEAN:
		    case MEMORY_OBJECT_LOCK_RESULT_MUST_RETURN:
			/*
			 * The clean and return cases are similar.
			 *
			 */

			/*
			 * if this would form a discontiguous block,
			 * clean the old pages and start anew.
			 *
			 * NOTE: The first time through here, new_object
			 * is null, hiding the fact that pageout_action
			 * is not initialized.
			 */
#ifdef NOT_LIST_REQ
			/*
			 * Mark the page busy since we unlock the
			 * object below.
			 */
			m->busy = TRUE;
			if (new_object != VM_OBJECT_NULL &&
			    (last_offset != offset ||
			     pageout_action != page_lock_result)) {
 	    			PAGEOUT_PAGES(object, new_object, new_offset,
						pageout_action, paging_offset);
			}
			vm_object_unlock(object);

			/*
			 *	If we have not already allocated an object
			 *	for a range of pages to be written, do so
			 *	now.
			 */
			if (new_object == VM_OBJECT_NULL) {
				if (should_flush) {
					/* use clean-in-place if possible */
					new_object = vm_pageout_object_allocate(
							m,
							original_size,
							m->offset);
				} else {
					/* do not use clean-in-place */
					new_object =
					    vm_object_allocate(original_size);
				}
				new_offset = 0;
				paging_offset = m->offset +
					object->paging_offset;
				pageout_action = page_lock_result;
			}

			if (should_flush) {
				/*
				 *	Move or shadow the dirty page into the
				 *	new object.
				 *	This applies to either trusted or
				 *	untrusted pagers.
				 *	Get a paging ref to donate to
				 *	vm_pageout_setup().
				 */
				vm_object_lock(object);
				vm_object_paging_begin(object);
				vm_object_unlock(object);
				holding_page = vm_pageout_setup(m,
						new_object,
						new_offset);
			} else {
				/*
				 * Clean but do not flush, always
				 * copying the page.  Allowing clean-in-place
				 * is potentially confusing to pagers and XMM
				 * that ask for clean pages, since the page
				 * could become dirty before the pager
				 * looks at it.
				 */
				vm_page_t	new_m;

				while ((new_m=vm_page_grab()) == VM_PAGE_NULL) {
					VM_PAGE_WAIT();
				}
				assert(new_m != VM_PAGE_NULL);

				vm_object_lock(m->object);
				m->busy = FALSE;
				vm_page_lock_queues();
				vm_pageclean_copy(m, new_m, new_object,
								new_offset);
				vm_page_unlock_queues();
				vm_object_unlock(m->object);
				holding_page = VM_PAGE_NULL;
			}
			/*
			 *	Save the holding page if there is one.
			 */
			holding_pages[atop(new_offset)] = holding_page;
#else
#if 0
	if(((rpc_subsystem_t) pager_mux_hash_lookup(object->pager)) !=
				((rpc_subsystem_t) &vnode_pager_workaround)) {
			/*
			 * Mark the page busy since we unlock the
			 * object below.
			 */
			m->busy = TRUE;
			if (new_object != VM_OBJECT_NULL &&
			    (last_offset != offset ||
			     pageout_action != page_lock_result)) {
 	    			PAGEOUT_PAGES(object, new_object, new_offset,
						pageout_action, paging_offset);
			}
			vm_object_unlock(object);

			/*
			 *	If we have not already allocated an object
			 *	for a range of pages to be written, do so
			 *	now.
			 */
			if (new_object == VM_OBJECT_NULL) {
				if (should_flush) {
					/* use clean-in-place if possible */
					new_object = vm_pageout_object_allocate(
							m,
							original_size,
							m->offset);
				} else {
					/* do not use clean-in-place */
					new_object =
					    vm_object_allocate(original_size);
				}
				new_offset = 0;
				paging_offset = m->offset +
					object->paging_offset;
				pageout_action = page_lock_result;
			}

			if (should_flush) {
				/*
				 *	Move or shadow the dirty page into the
				 *	new object.
				 *	This applies to either trusted or
				 *	untrusted pagers.
				 *	Get a paging ref to donate to
				 *	vm_pageout_setup().
				 */
				vm_object_lock(object);
				vm_object_paging_begin(object);
				vm_object_unlock(object);
				holding_page = vm_pageout_setup(m,
						new_object,
						new_offset);
			} else {
				/*
				 * Clean but do not flush, always
				 * copying the page.  Allowing clean-in-place
				 * is potentially confusing to pagers and XMM
				 * that ask for clean pages, since the page
				 * could become dirty before the pager
				 * looks at it.
				 */
				vm_page_t	new_m;

				while ((new_m=vm_page_grab()) == VM_PAGE_NULL) {
					VM_PAGE_WAIT();
				}
				assert(new_m != VM_PAGE_NULL);

				vm_object_lock(m->object);
				m->busy = FALSE;
				vm_page_lock_queues();
				vm_pageclean_copy(m, new_m, new_object,
								new_offset);
				vm_page_unlock_queues();
				vm_object_unlock(m->object);
				holding_page = VM_PAGE_NULL;
			}
			/*
			 *	Save the holding page if there is one.
			 */
			holding_pages[atop(new_offset)] = holding_page;
	} else {
#endif
			if (pending_pageout &&
			    (last_offset != offset ||
			     pageout_action != page_lock_result)) {
 	    			LIST_REQ_PAGEOUT_PAGES(object, new_object, 
						new_offset, pageout_action, 
						paging_offset);
				pending_pageout = FALSE;
			}
			holding_page = VM_PAGE_NULL;
			if(!pending_pageout) {
				pending_pageout = TRUE;
				pageout_action = page_lock_result;
				paging_offset = offset;
			}
			m->list_req_pending = TRUE;
			m->cleaning = TRUE;
			if (should_flush) {
				m->busy = TRUE;
				m->pageout = TRUE;
			}
			vm_object_unlock(object);
/*
	}
*/

#endif

			new_offset += PAGE_SIZE;
			last_offset = offset + PAGE_SIZE;
			data_returned = TRUE;

			vm_object_lock(object);
			break;
		}
		break;
	    }
	}

	/*
	 *	We have completed the scan for applicable pages.
	 *	Clean any pages that have been saved.
	 */
#ifdef NOT_LIST_REQ
	if (new_object != VM_OBJECT_NULL) {
 	    PAGEOUT_PAGES(object, new_object, new_offset, pageout_action,
			paging_offset);
	}
#else
/*
	if(((rpc_subsystem_t) pager_mux_hash_lookup(object->pager)) !=
				((rpc_subsystem_t) &vnode_pager_workaround)) {
	if (new_object != VM_OBJECT_NULL) {
 	    PAGEOUT_PAGES(object, new_object, new_offset, pageout_action,
			paging_offset);
	}
	} else {
*/
	if (pending_pageout) {
 	    LIST_REQ_PAGEOUT_PAGES(object, new_object, 
				new_offset, pageout_action, paging_offset);
	}
/*
	}
*/
#endif
	return (data_returned);
}

/*
 *	Routine:	memory_object_synchronize_completed [user interface]
 *
 *	Tell kernel that previously synchronized data
 *	(memory_object_synchronize) has been queue or placed on the
 *	backing storage.
 *
 *	Note: there may be multiple synchronize requests for a given
 *	memory object outstanding but they will not overlap.
 */

kern_return_t
memory_object_synchronize_completed(
	vm_object_t	object,
	vm_offset_t	offset,
	vm_offset_t	length)
{
        msync_req_t             msr;

        XPR(XPR_MEMORY_OBJECT,
	    "m_o_sync_completed, object 0x%X, offset 0x%X length 0x%X\n",
	    (integer_t)object, offset, length, 0, 0);

	/*
	 *      Look for bogus arguments
	 */

	if (object == VM_OBJECT_NULL) {
		return KERN_INVALID_ARGUMENT;
	}

	vm_object_lock(object);

/*
 *	search for sync request structure
 */
	queue_iterate(&object->msr_q, msr, msync_req_t, msr_q) {
 		if (msr->offset == offset && msr->length == length) {
			queue_remove(&object->msr_q, msr, msync_req_t, msr_q);
			break;
		}
        }/* queue_iterate */

	if (queue_end(&object->msr_q, (queue_entry_t)msr)) {
		vm_object_unlock(object);
		vm_object_deallocate(object);
		return KERN_INVALID_ARGUMENT;
	}

	msr_lock(msr);
	vm_object_unlock(object);
	msr->flag = VM_MSYNC_DONE;
	msr_unlock(msr);
	thread_wakeup((event_t) msr);
	vm_object_deallocate(object);

	return KERN_SUCCESS;
}/* memory_object_synchronize_completed */
	  
kern_return_t
memory_object_set_attributes_common(
	vm_object_t	object,
	boolean_t	may_cache,
	memory_object_copy_strategy_t copy_strategy,
	boolean_t	temporary,
	vm_size_t	cluster_size,
        boolean_t	silent_overwrite,
	boolean_t	advisory_pageout)
{
	boolean_t	object_became_ready;

        XPR(XPR_MEMORY_OBJECT,
	    "m_o_set_attr_com, object 0x%X flg %x strat %d\n",
	    (integer_t)object, (may_cache&1)|((temporary&1)<1), copy_strategy, 0, 0);

	if (object == VM_OBJECT_NULL)
		return(KERN_INVALID_ARGUMENT);

	/*
	 *	Verify the attributes of importance
	 */

	switch(copy_strategy) {
		case MEMORY_OBJECT_COPY_NONE:
		case MEMORY_OBJECT_COPY_DELAY:
			break;
		default:
			vm_object_deallocate(object);
			return(KERN_INVALID_ARGUMENT);
	}

#if	!ADVISORY_PAGEOUT
	if (silent_overwrite || advisory_pageout) {
		vm_object_deallocate(object);
		return(KERN_INVALID_ARGUMENT);
	}
#endif	/* !ADVISORY_PAGEOUT */
	if (may_cache)
		may_cache = TRUE;
	if (temporary)
		temporary = TRUE;
	if (cluster_size != 0) {
		int	pages_per_cluster;
		pages_per_cluster = atop(cluster_size);
		/*
		 * Cluster size must be integral multiple of page size,
		 * and be a power of 2 number of pages.
		 */
		if ((cluster_size & (PAGE_SIZE-1)) ||
		    ((pages_per_cluster-1) & pages_per_cluster)) {
			vm_object_deallocate(object);
			return KERN_INVALID_ARGUMENT;
		}
	}

	vm_object_lock(object);

	/*
	 *	Copy the attributes
	 */
	assert(!object->internal);
	object_became_ready = !object->pager_ready;
	object->copy_strategy = copy_strategy;
	object->can_persist = may_cache;
	object->temporary = temporary;
	object->silent_overwrite = silent_overwrite;
	object->advisory_pageout = advisory_pageout;
	if (cluster_size == 0)
		cluster_size = PAGE_SIZE;
	object->cluster_size = cluster_size;

	assert(cluster_size >= PAGE_SIZE &&
	       cluster_size % PAGE_SIZE == 0);

	/*
	 *	Wake up anyone waiting for the ready attribute
	 *	to become asserted.
	 */

	if (object_became_ready) {
		object->pager_ready = TRUE;
		vm_object_wakeup(object, VM_OBJECT_EVENT_PAGER_READY);
	}

	vm_object_unlock(object);

	vm_object_deallocate(object);

	return(KERN_SUCCESS);
}

/*
 *	Set the memory object attribute as provided.
 *
 *	XXX This routine cannot be completed until the vm_msync, clean 
 *	     in place, and cluster work is completed. See ifdef notyet
 *	     below and note that memory_object_set_attributes_common()
 *	     may have to be expanded.
 */
kern_return_t
memory_object_change_attributes(
        vm_object_t             object,
        memory_object_flavor_t  flavor,
	memory_object_info_t	attributes,
	mach_msg_type_number_t	count,
        ipc_port_t              reply_to,
        mach_msg_type_name_t    reply_to_type)
{
        kern_return_t   		result = KERN_SUCCESS;
        boolean_t       		temporary;
        boolean_t       		may_cache;
        boolean_t       		invalidate;
	vm_size_t			cluster_size;
	memory_object_copy_strategy_t	copy_strategy;
        boolean_t       		silent_overwrite;
	boolean_t			advisory_pageout;

	if (object == VM_OBJECT_NULL)
		return(KERN_INVALID_ARGUMENT);

	vm_object_lock(object);
	temporary = object->temporary;
	may_cache = object->can_persist;
	copy_strategy = object->copy_strategy;
	silent_overwrite = object->silent_overwrite;
	advisory_pageout = object->advisory_pageout;
#if notyet
	invalidate = object->invalidate;
#endif
	cluster_size = object->cluster_size;
	vm_object_unlock(object);	

	switch (flavor) {
	    case OLD_MEMORY_OBJECT_BEHAVIOR_INFO:
	    {
                old_memory_object_behave_info_t     behave;

                if (count != OLD_MEMORY_OBJECT_BEHAVE_INFO_COUNT) {
                        result = KERN_INVALID_ARGUMENT;
                        break;
                }

                behave = (old_memory_object_behave_info_t) attributes;

		temporary = behave->temporary;
		invalidate = behave->invalidate;
		copy_strategy = behave->copy_strategy;

		break;
	    }

	    case MEMORY_OBJECT_BEHAVIOR_INFO:
	    {
                memory_object_behave_info_t     behave;

                if (count != MEMORY_OBJECT_BEHAVE_INFO_COUNT) {
                        result = KERN_INVALID_ARGUMENT;
                        break;
                }

                behave = (memory_object_behave_info_t) attributes;

		temporary = behave->temporary;
		invalidate = behave->invalidate;
		copy_strategy = behave->copy_strategy;
		silent_overwrite = behave->silent_overwrite;
		advisory_pageout = behave->advisory_pageout;
		break;
	    }

	    case MEMORY_OBJECT_PERFORMANCE_INFO:
	    {
		memory_object_perf_info_t	perf;

                if (count != MEMORY_OBJECT_PERF_INFO_COUNT) {
                        result = KERN_INVALID_ARGUMENT;
                        break;
                }

                perf = (memory_object_perf_info_t) attributes;

		may_cache = perf->may_cache;
		cluster_size = round_page(perf->cluster_size);

		break;
	    }

	    case OLD_MEMORY_OBJECT_ATTRIBUTE_INFO:
	    {
		old_memory_object_attr_info_t	attr;

                if (count != OLD_MEMORY_OBJECT_ATTR_INFO_COUNT) {
                        result = KERN_INVALID_ARGUMENT;
                        break;
                }

		attr = (old_memory_object_attr_info_t) attributes;

                may_cache = attr->may_cache;
                copy_strategy = attr->copy_strategy;
		cluster_size = page_size;

		break;
	    }

	    case MEMORY_OBJECT_ATTRIBUTE_INFO:
	    {
		memory_object_attr_info_t	attr;

                if (count != MEMORY_OBJECT_ATTR_INFO_COUNT) {
                        result = KERN_INVALID_ARGUMENT;
                        break;
                }

		attr = (memory_object_attr_info_t) attributes;

		copy_strategy = attr->copy_strategy;
                may_cache = attr->may_cache_object;
		cluster_size = attr->cluster_size;
		temporary = attr->temporary;

		break;
	    }

	    default:
		result = KERN_INVALID_ARGUMENT;
		break;
	}

	if (result != KERN_SUCCESS) {
		vm_object_deallocate(object);
		return(result);
	}

	if (copy_strategy == MEMORY_OBJECT_COPY_TEMPORARY) {
		copy_strategy = MEMORY_OBJECT_COPY_DELAY;
		temporary = TRUE;
	} else {
		temporary = FALSE;
	}

	/*
	 *	Do the work and throw away our object reference.  It
	 *	is important that the object reference be deallocated
	 *	BEFORE sending the reply.  The whole point of the reply
	 *	is that it shows up after the terminate message that
	 *	may be generated by setting the object uncacheable.
	 *
	 * XXX	may_cache may become a tri-valued variable to handle
	 * XXX	uncache if not in use.
	 */
	result = memory_object_set_attributes_common(object,
						     may_cache,
						     copy_strategy,
						     temporary,
						     cluster_size,
						     silent_overwrite,
						     advisory_pageout);

        if (IP_VALID(reply_to)) {
                /* consumes our naked send-once/send right for reply_to */
                (void) memory_object_change_completed(reply_to, reply_to_type,
		       object->alive ?
		      		 object->pager_request : PAGER_REQUEST_NULL,
		       flavor);
	}

	return(result);
}

kern_return_t
memory_object_get_attributes(
        vm_object_t     	object,
        memory_object_flavor_t 	flavor,
	memory_object_info_t	attributes,	/* pointer to OUT array */
	mach_msg_type_number_t	*count)		/* IN/OUT */
{
	kern_return_t ret = KERN_SUCCESS;

        if (object == VM_OBJECT_NULL)
                return(KERN_INVALID_ARGUMENT);

        vm_object_lock(object);

	switch (flavor) {
	    case OLD_MEMORY_OBJECT_BEHAVIOR_INFO:
	    {
		old_memory_object_behave_info_t	behave;

		if (*count < OLD_MEMORY_OBJECT_BEHAVE_INFO_COUNT) {
			ret = KERN_INVALID_ARGUMENT;
			break;
		}

		behave = (old_memory_object_behave_info_t) attributes;
		behave->copy_strategy = object->copy_strategy;
		behave->temporary = object->temporary;
#if notyet	/* remove when vm_msync complies and clean in place fini */
                behave->invalidate = object->invalidate;
#else
		behave->invalidate = FALSE;
#endif

		*count = OLD_MEMORY_OBJECT_BEHAVE_INFO_COUNT;
		break;
	    }

	    case MEMORY_OBJECT_BEHAVIOR_INFO:
	    {
		memory_object_behave_info_t	behave;

		if (*count < MEMORY_OBJECT_BEHAVE_INFO_COUNT) {
                        ret = KERN_INVALID_ARGUMENT;
                        break;
                }

                behave = (memory_object_behave_info_t) attributes;
                behave->copy_strategy = object->copy_strategy;
		behave->temporary = object->temporary;
#if notyet	/* remove when vm_msync complies and clean in place fini */
                behave->invalidate = object->invalidate;
#else
		behave->invalidate = FALSE;
#endif
		behave->advisory_pageout = object->advisory_pageout;
		behave->silent_overwrite = object->silent_overwrite;
                *count = MEMORY_OBJECT_BEHAVE_INFO_COUNT;
		break;
	    }

	    case MEMORY_OBJECT_PERFORMANCE_INFO:
	    {
		memory_object_perf_info_t	perf;

		if (*count < MEMORY_OBJECT_PERF_INFO_COUNT) {
			ret = KERN_INVALID_ARGUMENT;
			break;
		}

		perf = (memory_object_perf_info_t) attributes;
		perf->cluster_size = object->cluster_size;
		perf->may_cache = object->can_persist;

		*count = MEMORY_OBJECT_PERF_INFO_COUNT;
		break;
	    }

            case OLD_MEMORY_OBJECT_ATTRIBUTE_INFO:
            {
                old_memory_object_attr_info_t       attr;

                if (*count < OLD_MEMORY_OBJECT_ATTR_INFO_COUNT) {
                        ret = KERN_INVALID_ARGUMENT;
                        break;
                }

                attr = (old_memory_object_attr_info_t) attributes;
        	attr->may_cache = object->can_persist;
        	attr->copy_strategy = object->copy_strategy;

                *count = OLD_MEMORY_OBJECT_ATTR_INFO_COUNT;
                break;
            }

            case MEMORY_OBJECT_ATTRIBUTE_INFO:
            {
                memory_object_attr_info_t       attr;

                if (*count < MEMORY_OBJECT_ATTR_INFO_COUNT) {
                        ret = KERN_INVALID_ARGUMENT;
                        break;
                }

                attr = (memory_object_attr_info_t) attributes;
        	attr->copy_strategy = object->copy_strategy;
		attr->cluster_size = object->cluster_size;
        	attr->may_cache_object = object->can_persist;
		attr->temporary = object->temporary;

                *count = MEMORY_OBJECT_ATTR_INFO_COUNT;
                break;
            }

	    default:
		ret = KERN_INVALID_ARGUMENT;
		break;
	}

        vm_object_unlock(object);

        vm_object_deallocate(object);

        return(ret);
}

int vm_stat_discard_cleared_reply = 0;
int vm_stat_discard_cleared_unset = 0;
int vm_stat_discard_cleared_too_late = 0;

#if	ADVISORY_PAGEOUT
kern_return_t
memory_object_discard_reply(
	vm_object_t		object,
	vm_offset_t		requested_offset,
	vm_size_t		requested_size,
	vm_offset_t		discard_offset,
	vm_size_t		discard_size,
	memory_object_return_t	should_return,
	ipc_port_t		reply_to,
	mach_msg_type_name_t	reply_to_type)
{
	vm_offset_t	original_offset = discard_offset;
	vm_size_t	original_size = discard_size;

	XPR(XPR_MEMORY_OBJECT,
	    "m_o_discard_reply, obj 0x%X req_off 0x%X req_size 0x%X "
	    "discard_off 0x%X discard_size 0x%X\n",
	    (integer_t) object, requested_offset, requested_size,
	    discard_offset, discard_size);

	/*
	 *	Check for bogus arguments.
	 */
	if (object == VM_OBJECT_NULL)
		return KERN_INVALID_ARGUMENT;

	requested_size = round_page(requested_size);
	discard_size = round_page(discard_size);

	/*
	 * 	Lock the object, and acquire a paging reference to
	 *	prevent the memory_object and control ports from
	 * 	being destroyed.
	 */

	vm_object_lock(object);
	vm_object_paging_begin(object);
	discard_offset -= object->paging_offset;
	requested_offset -= object->paging_offset;

	if (discard_size != 0) {
		(void) memory_object_update(object,
					    discard_offset, discard_size,
					    should_return, TRUE, VM_PROT_ALL);
	}

	if (original_offset != requested_offset ||
	    discard_size != requested_size) {
		/*
		 * We're not discarding the requested pages, so reset 
		 * the "discard_request" flag on the requested pages.
		 * But don't rehabilitate more pages than we discard.
		 */
		while (requested_size != 0) {
			vm_page_t m;

			m = vm_page_lookup(object, requested_offset);
			if (m != VM_PAGE_NULL) {
				if (discard_size != 0) {
					if (m->discard_request) {
						m->discard_request = FALSE;
						vm_stat_discard_cleared_reply++;
					} else {
						vm_stat_discard_cleared_unset++;
					}
				}
			} else {
				vm_stat_discard_cleared_too_late++;
			}

			requested_offset += PAGE_SIZE;
			requested_size -= PAGE_SIZE;
			discard_size -= PAGE_SIZE;
		}
	}

	if (IP_VALID(reply_to)) {
		vm_object_unlock(object);

		/* consumes our naked send-once/send right for reply_to */
		(void) memory_object_lock_completed(reply_to, reply_to_type,
						    object->pager_request,
						    original_offset,
						    original_size);

		vm_object_lock(object);
	}

	vm_object_paging_end(object);
	vm_object_unlock(object);
	vm_object_deallocate(object);

	return KERN_SUCCESS;
}
#endif /* ADVISORY_PAGEOUT */

/*
 * vm_set_default_memory_manager(): 
 *	[Obsolete]
 */
kern_return_t
vm_set_default_memory_manager(
	host_t		host,
	ipc_port_t	*default_manager)
{
	return(host_default_memory_manager(host, default_manager, 4*PAGE_SIZE));
}

/*
 *	Routine:	host_default_memory_manager
 *	Purpose:
 *		set/get the default memory manager port and default cluster
 *		size.
 *
 *		If successful, consumes the supplied naked send right.
 */
kern_return_t
host_default_memory_manager(
	host_t		host,
	ipc_port_t	*default_manager,
	vm_size_t	cluster_size)
{
	ipc_port_t current_manager;
	ipc_port_t new_manager;
	ipc_port_t returned_manager;

	if (host == HOST_NULL)
		return(KERN_INVALID_HOST);

	new_manager = *default_manager;
	mutex_lock(&memory_manager_default_lock);
	current_manager = memory_manager_default;

	if (new_manager == IP_NULL) {
		/*
		 *	Retrieve the current value.
		 */

		returned_manager = ipc_port_copy_send(current_manager);
	} else {
		/*
		 *	Retrieve the current value,
		 *	and replace it with the supplied value.
		 *	We consume the supplied naked send right.
		 */

		returned_manager = current_manager;
		memory_manager_default = new_manager;
		if (cluster_size % PAGE_SIZE != 0) {
#if 0
			mutex_unlock(&memory_manager_default_lock);
			return KERN_INVALID_ARGUMENT;
#else
			cluster_size = round_page(cluster_size);
#endif
		}
		memory_manager_default_cluster = cluster_size;

		/*
		 *	In case anyone's been waiting for a memory
		 *	manager to be established, wake them up.
		 */

		thread_wakeup((event_t) &memory_manager_default);
	}

	mutex_unlock(&memory_manager_default_lock);

	*default_manager = returned_manager;
	return(KERN_SUCCESS);
}

/*
 *	Routine:	memory_manager_default_reference
 *	Purpose:
 *		Returns a naked send right for the default
 *		memory manager.  The returned right is always
 *		valid (not IP_NULL or IP_DEAD).
 */

ipc_port_t
memory_manager_default_reference(
	vm_size_t	*cluster_size)
{
	ipc_port_t current_manager;

	mutex_lock(&memory_manager_default_lock);

	while (current_manager = ipc_port_copy_send(memory_manager_default),
	       !IP_VALID(current_manager)) {
		thread_sleep_mutex((event_t) &memory_manager_default,
			&memory_manager_default_lock, THREAD_UNINT);
		mutex_lock(&memory_manager_default_lock);
	}
	*cluster_size = memory_manager_default_cluster;

	mutex_unlock(&memory_manager_default_lock);

	return current_manager;
}

/*
 *	Routine:	memory_manager_default_port
 *	Purpose:
 *		Returns true if the receiver for the port
 *		is the default memory manager.
 *
 *		This is a hack to let ds_read_done
 *		know when it should keep memory wired.
 */

boolean_t
memory_manager_default_port(
	ipc_port_t port)
{
	ipc_port_t current;
	boolean_t result;

	mutex_lock(&memory_manager_default_lock);
	current = memory_manager_default;
	if (IP_VALID(current)) {
		/*
		 *	There is no point in bothering to lock
		 *	both ports, which would be painful to do.
		 *	If the receive rights are moving around,
		 *	we might be inaccurate.
		 */

		result = port->ip_receiver == current->ip_receiver;
	} else
		result = FALSE;
	mutex_unlock(&memory_manager_default_lock);

	return result;
}

/*
 *	Routine:	memory_manager_default_check
 *
 *	Purpose:
 *		Check whether a default memory manager has been set
 *		up yet, or not. Returns KERN_SUCCESS if dmm exists,
 *		and KERN_FAILURE if dmm does not exist.
 *
 *		If there is no default memory manager, log an error,
 *		but only the first time.
 *
 */
kern_return_t
memory_manager_default_check(void)
{
	ipc_port_t current;

	mutex_lock(&memory_manager_default_lock);
	current = memory_manager_default;
	if (!IP_VALID(current)) {
		static boolean_t logged;	/* initialized to 0 */
		boolean_t	complain = !logged;
		logged = TRUE;
		mutex_unlock(&memory_manager_default_lock);
		if (complain)
			printf("Warning: No default memory manager\n");
		return(KERN_FAILURE);
	} else {
		mutex_unlock(&memory_manager_default_lock);
		return(KERN_SUCCESS);
	}
}

void
memory_manager_default_init(void)
{
	memory_manager_default = IP_NULL;
	mutex_init(&memory_manager_default_lock, ETAP_VM_MEMMAN);
}


void
memory_object_kill_pages(
	vm_object_t	object,
	vm_offset_t	offset,
	vm_size_t	size)
{
	register vm_page_t	m;

	/*
	 * entered with object lock held, acquire a paging reference to
	 * prevent the memory_object and control ports from
	 * being destroyed.
	 */
	vm_object_paging_begin(object);

	for (;
	     size != 0;
	     size -= PAGE_SIZE, offset += PAGE_SIZE)
	{
	        if ((m = vm_page_lookup(object, offset)) != VM_PAGE_NULL) {
		        if (memory_object_lock_page(m, MEMORY_OBJECT_RETURN_NONE, TRUE, VM_PROT_NO_CHANGE) ==
			    MEMORY_OBJECT_LOCK_RESULT_DONE)
			        vm_external_state_clr(object->existence_map, offset);
		}
	}
	vm_object_paging_end(object);
}
