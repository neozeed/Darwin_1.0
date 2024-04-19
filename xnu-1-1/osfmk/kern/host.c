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
 *	host.c
 *
 *	Non-ipc host functions.
 */

#include <cpus.h>
#include <dipc.h>
#include <mach_host.h>

#include <mach/boolean.h>
#include <kern/assert.h>
#include <kern/kalloc.h>
#include <kern/rtmalloc.h>
#include <kern/host.h>
#include <kern/host_statistics.h>
#include <kern/ipc_host.h>
#include <kern/misc_protos.h>
#include <mach/host_info.h>
#include <mach/kern_return.h>
#include <mach/machine.h>
#include <mach/port.h>
#include <kern/processor.h>
#include <mach/processor_info.h>
#include <mach/vm_param.h>
#include <mach/mach_host_server.h>
#if     DIPC
#include <dipc/dipc_funcs.h>
#include <dipc/special_ports.h>
#endif

vm_statistics_data_t	vm_stat[NCPUS];

host_data_t	realhost;

kern_return_t
host_processors(
	host_t			host,
	processor_array_t	*processor_list,
	mach_msg_type_number_t	*countp)
{
	register int		i;
	register processor_t	*tp;
	vm_offset_t		addr;
	unsigned int		count;
	boolean_t rt = FALSE; /* ### This boolean is FALSE, because there
			       * currently exists no mechanism to determine
			       * whether or not the reply port is an RT port
			       */

	if (host == HOST_NULL)
		return(KERN_INVALID_ARGUMENT);

	/*
	 *	Determine how many processors we have.
	 *	(This number shouldn't change.)
	 */

	count = 0;
	for (i = 0; i < NCPUS; i++)
		if (machine_slot[i].is_cpu)
			count++;

	if (count == 0)
		panic("host_processors");

	addr = KALLOC((vm_size_t) (count * sizeof(mach_port_t)), rt);
	if (addr == 0)
		return KERN_RESOURCE_SHORTAGE;

	tp = (processor_t *) addr;
	for (i = 0; i < NCPUS; i++)
		if (machine_slot[i].is_cpu)
			*tp++ = cpu_to_processor(i);

	*countp = count;
	*processor_list = (processor_array_t)addr;

	/* do the conversion that Mig should handle */

	tp = (processor_t *) addr;
	for (i = 0; i < count; i++)
		((mach_port_t *) tp)[i] =
		      (mach_port_t)convert_processor_to_port(tp[i]);

	return KERN_SUCCESS;
}

kern_return_t
host_info(
	host_t			host,
	host_flavor_t		flavor,
	host_info_t		info,
	mach_msg_type_number_t	*count)
{

	if (host == HOST_NULL)
		return(KERN_INVALID_ARGUMENT);
	
	switch(flavor) {

	case HOST_BASIC_INFO:
		{ register host_basic_info_t	basic_info;
		/*
		 *	Basic information about this host.
		 */
		if (*count < HOST_BASIC_INFO_COUNT)
			return(KERN_FAILURE);

		basic_info = (host_basic_info_t) info;

		basic_info->max_cpus = machine_info.max_cpus;
		basic_info->avail_cpus = machine_info.avail_cpus;
		basic_info->memory_size = machine_info.memory_size;
		basic_info->cpu_type =
			machine_slot[master_processor->slot_num].cpu_type;
		basic_info->cpu_subtype =
			machine_slot[master_processor->slot_num].cpu_subtype;

		*count = HOST_BASIC_INFO_COUNT;
		return(KERN_SUCCESS);
		}

	case HOST_SCHED_INFO:
		{ register host_sched_info_t	sched_info;
		  extern int tick; /* XXX */
		/*
		 *	Return scheduler information.
		 */
		if (*count < HOST_SCHED_INFO_COUNT)
			return(KERN_FAILURE);

		sched_info = (host_sched_info_t) info;

		sched_info->min_timeout = tick / 1000; /* XXX */
		sched_info->min_quantum = tick / 1000; /* XXX */

		*count = HOST_SCHED_INFO_COUNT;
		return(KERN_SUCCESS);
		}

        case HOST_RESOURCE_SIZES:
		{ 
                  /*
                   * Return sizes of kernel data structures
                   */
                  if (*count < HOST_RESOURCE_SIZES_COUNT)
			return(KERN_FAILURE);

                  /* XXX Fail until ledgers are implemented */
                  return(KERN_INVALID_ARGUMENT);
               }
                  
	case HOST_PRIORITY_INFO:
		{ register host_priority_info_t	priority_info;

		if (*count < HOST_PRIORITY_INFO_COUNT)
			return(KERN_FAILURE);

		priority_info = (host_priority_info_t) info;

		priority_info->kernel_priority = BASEPRI_KERNEL;
		priority_info->system_priority= BASEPRI_SYSTEM;
		priority_info->server_priority= BASEPRI_SERVER;
		priority_info->user_priority= BASEPRI_USER;
		priority_info->depress_priority= DEPRESSPRI;
		priority_info->idle_priority= IDLEPRI;
		priority_info->minimum_priority= MINPRI;
		priority_info->maximum_priority= 0;

		*count = HOST_PRIORITY_INFO_COUNT;
		return(KERN_SUCCESS);
		}

		/*
		 */
	default:
		return(KERN_INVALID_ARGUMENT);
	}
}

kern_return_t
host_statistics(
	host_t			host,
	host_flavor_t		flavor,
	host_info_t		info,
	mach_msg_type_number_t	*count)
{

	if (host == HOST_NULL)
		return(KERN_INVALID_HOST);
	
	switch(flavor) {

	case HOST_LOAD_INFO: {
		register host_load_info_t load_info;
		extern integer_t avenrun[3], mach_factor[3];

		if (*count < HOST_LOAD_INFO_COUNT)
			return(KERN_FAILURE);

		load_info = (host_load_info_t) info;

		bcopy((char *) avenrun,
		      (char *) load_info->avenrun,
		      sizeof avenrun);
		bcopy((char *) mach_factor,
		      (char *) load_info->mach_factor,
		      sizeof mach_factor);

		*count = HOST_LOAD_INFO_COUNT;
		return(KERN_SUCCESS);
	    }

	case HOST_VM_INFO: {
		register vm_statistics_t stat;
		vm_statistics_data_t host_vm_stat;
		extern int vm_page_free_count, vm_page_active_count,
			   vm_page_inactive_count, vm_page_wire_count;
                
		if (*count < HOST_VM_INFO_COUNT)
			return(KERN_FAILURE);

		stat = &vm_stat[0];
		host_vm_stat = *stat;
#if NCPUS > 1
		{
			register int i;

			for (i = 1; i < NCPUS; i++) {
				stat++;
				host_vm_stat.zero_fill_count +=
						stat->zero_fill_count;
				host_vm_stat.reactivations +=
						stat->reactivations;
				host_vm_stat.pageins += stat->pageins;
				host_vm_stat.pageouts += stat->pageouts;
				host_vm_stat.faults += stat->faults;
				host_vm_stat.cow_faults += stat->cow_faults;
				host_vm_stat.lookups += stat->lookups;
				host_vm_stat.hits += stat->hits;
			}
		}
#endif

		stat = (vm_statistics_t) info;

                stat->free_count = vm_page_free_count;
                stat->active_count = vm_page_active_count;
                stat->inactive_count = vm_page_inactive_count;
                stat->wire_count = vm_page_wire_count;
                stat->zero_fill_count = host_vm_stat.zero_fill_count;
                stat->reactivations = host_vm_stat.reactivations;
                stat->pageins = host_vm_stat.pageins;
                stat->pageouts = host_vm_stat.pageouts;
                stat->faults = host_vm_stat.faults;
                stat->cow_faults = host_vm_stat.cow_faults;
                stat->lookups = host_vm_stat.lookups;
                stat->hits = host_vm_stat.hits;

		*count = HOST_VM_INFO_COUNT;
		return(KERN_SUCCESS);
	    }
                
	case HOST_CPU_LOAD_INFO: {
		host_cpu_load_info_t	cpu_load_info;
		unsigned long		ticks_value1, ticks_value2;
		int			i;

#define GET_TICKS_VALUE(__cpu,__state) \
MACRO_BEGIN \
	do { \
		ticks_value1 = *(volatile integer_t *) \
			(&machine_slot[(__cpu)].cpu_ticks[(__state)]); \
		ticks_value2 = *(volatile integer_t *) \
			(&machine_slot[(__cpu)].cpu_ticks[(__state)]); \
	} while (ticks_value1 != ticks_value2); \
	cpu_load_info->cpu_ticks[(__state)] += ticks_value1; \
MACRO_END

		if (*count < HOST_CPU_LOAD_INFO_COUNT)
			return KERN_FAILURE;

		cpu_load_info = (host_cpu_load_info_t) info;

		cpu_load_info->cpu_ticks[CPU_STATE_USER] = 0;
		cpu_load_info->cpu_ticks[CPU_STATE_NICE] = 0;
		cpu_load_info->cpu_ticks[CPU_STATE_SYSTEM] = 0;
		cpu_load_info->cpu_ticks[CPU_STATE_IDLE] = 0;
		for (i = 0; i < NCPUS; i++) {
			if (!machine_slot[i].is_cpu ||
			    !machine_slot[i].running)
				continue;
			GET_TICKS_VALUE(i, CPU_STATE_USER);
			GET_TICKS_VALUE(i, CPU_STATE_NICE);
			GET_TICKS_VALUE(i, CPU_STATE_SYSTEM);
			GET_TICKS_VALUE(i, CPU_STATE_IDLE);
		}

		*count = HOST_CPU_LOAD_INFO_COUNT;
		return KERN_SUCCESS;
	    }

	default:
		return(KERN_INVALID_ARGUMENT);
	}
}

kern_return_t
host_page_size(
	host_t		host,
	vm_size_t	*out_page_size)
{
	if (host == HOST_NULL)
		return(KERN_INVALID_ARGUMENT);

        *out_page_size = PAGE_SIZE;

	return(KERN_SUCCESS);
}

/*
 *	Return kernel version string (more than you ever
 *	wanted to know about what version of the kernel this is).
 */

kern_return_t
host_kernel_version(
	host_t			host,
	kernel_version_t	out_version)
{
	extern char	version[];

	if (host == HOST_NULL)
		return(KERN_INVALID_ARGUMENT);

	(void) strncpy(out_version, version, sizeof(kernel_version_t));

	return(KERN_SUCCESS);
}

/*
 *	host_processor_sets:
 *
 *	List all processor sets on the host.
 */
#if	MACH_HOST

kern_return_t
host_processor_sets(
	host_t				host,
	processor_set_name_array_t	*pset_list,
	mach_msg_type_number_t		*count)
{
	unsigned int actual;	/* this many psets */
	processor_set_t pset;
	processor_set_t *psets;
	int i;
	boolean_t rt = FALSE; /* ### This boolean is FALSE, because there
			       * currently exists no mechanism to determine
			       * whether or not the reply port is an RT port
			       */

	vm_size_t size;
	vm_size_t size_needed;
	vm_offset_t addr;

	if (host == HOST_NULL)
		return KERN_INVALID_ARGUMENT;

	size = 0; addr = 0;

	for (;;) {
		mutex_lock(&all_psets_lock);
		actual = all_psets_count;

		/* do we have the memory we need? */

		size_needed = actual * sizeof(mach_port_t);
		if (size_needed <= size)
			break;

		/* unlock and allocate more memory */
		mutex_unlock(&all_psets_lock);

		if (size != 0)
			KFREE(addr, size, rt);

		assert(size_needed > 0);
		size = size_needed;

		addr = KALLOC(size, rt);
		if (addr == 0)
			return KERN_RESOURCE_SHORTAGE;
	}

	/* OK, have memory and the all_psets_lock */

	psets = (processor_set_t *) addr;

	for (i = 0, pset = (processor_set_t) queue_first(&all_psets);
	     i < actual;
	     i++, pset = (processor_set_t) queue_next(&pset->all_psets)) {
		/* take ref for convert_pset_name_to_port */
		pset_reference(pset);
		psets[i] = pset;
	}
	assert(queue_end(&all_psets, (queue_entry_t) pset));

	/* can unlock now that we've got the pset refs */
	mutex_unlock(&all_psets_lock);

	/*
	 *	Always have default port.
	 */

	assert(actual > 0);

	/* if we allocated too much, must copy */

	if (size_needed < size) {
		vm_offset_t newaddr;

		newaddr = KALLOC(size_needed, rt);
		if (newaddr == 0) {
			for (i = 0; i < actual; i++)
				pset_deallocate(psets[i]);
			KFREE(addr, size, rt);
			return KERN_RESOURCE_SHORTAGE;
		}

		bcopy((char *) addr, (char *) newaddr, size_needed);
		KFREE(addr, size, rt);
		psets = (processor_set_t *) newaddr;
	}

	*pset_list = (mach_port_t *) psets;
	*count = actual;

	/* do the conversion that Mig should handle */

	for (i = 0; i < actual; i++)
		((ipc_port_t *) psets)[i] = convert_pset_name_to_port(psets[i]);

	return KERN_SUCCESS;
}
#else	/* MACH_HOST */
/*
 *	Only one processor set, the default processor set, in this case.
 */
kern_return_t
host_processor_sets(
	host_t				host,
	processor_set_name_array_t	*pset_list,
	mach_msg_type_number_t		*count)
{
	vm_offset_t addr;
	boolean_t rt = FALSE; /* ### This boolean is FALSE, because there
			       * currently exists no mechanism to determine
			       * whether or not the reply port is an RT port
			       */

	if (host == HOST_NULL)
		return KERN_INVALID_ARGUMENT;

	/*
	 *	Allocate memory.  Can be pageable because it won't be
	 *	touched while holding a lock.
	 */

	addr = KALLOC((vm_size_t) sizeof(mach_port_t), rt);
	if (addr == 0)
		return KERN_RESOURCE_SHORTAGE;

	/* take ref for convert_pset_name_to_port */
	pset_reference(&default_pset);
	/* do the conversion that Mig should handle */
	*((ipc_port_t *) addr) = convert_pset_name_to_port(&default_pset);

	*pset_list = (processor_set_array_t)addr;
	*count = 1;

	return KERN_SUCCESS;
}
#endif	/* MACH_HOST */

/*
 *	host_processor_set_priv:
 *
 *	Return control port for given processor set.
 */
kern_return_t
host_processor_set_priv(
	host_t		host,
	processor_set_t	pset_name,
	processor_set_t	*pset)
{
    if ((host == HOST_NULL) || (pset_name == PROCESSOR_SET_NULL)) {
	*pset = PROCESSOR_SET_NULL;
	return(KERN_INVALID_ARGUMENT);
    }

    *pset = pset_name;
    pset_reference(*pset);
    return(KERN_SUCCESS);
}

/*
 *	host_processor_slots:
 *
 *	Return number of slots with active processors in them
 */
kern_return_t
host_processor_slots(
	host_t			host,
	processor_slot_t	slots,
	mach_msg_type_number_t	*count)
{
    int i;

#ifdef	lint
    host++;
#endif	/* lint */
        
    if (*count < NCPUS)
        return(KERN_INVALID_ARGUMENT);

    *count = 0;
    for (i = 0; i < NCPUS; i++) {
        if (machine_slot[i].is_cpu &&
            machine_slot[i].running) {
            *slots++ = i;
            (*count)++;
        }
    }
    return(KERN_SUCCESS);
}


/*
 * 	host_node_self
 *
 *	Find the current host node number, if we are distributed.
 */
kern_return_t
host_node_self(
        host_t host,
        int *node)
{
#if     DIPC
        *node = dipc_node_self();
        return KERN_SUCCESS;
#else
	*node = 0;
        return KERN_SUCCESS;
#endif
}

/*
 *      User interface for setting a special port.
 *
 *      Only permits the user to set a user-owned special port
 *      ID, rejecting a kernel-owned special port ID.
 *
 *      A special kernel port cannot be set up using this
 *      routine; use kernel_set_special_port() instead.
 */
kern_return_t
host_set_special_port(
        host_t          host,
        int             id,
        ipc_port_t      port)
{
#if     DIPC
        return norma_set_special_port(host, id, port);
#else
        return KERN_FAILURE;
#endif
}


/*
 *      User interface for retrieving a special port.
 *
 *      When all processing is local, this call does not block.
 *      If processing goes remote to discover a remote UID,
 *      this call blocks but not indefinitely.  If the remote
 *      node does not exist, has panic'ed, or is booting but
 *      hasn't yet turned on DIPC, then we expect the transport
 *      to return an error.
 *
 *      This routine always returns SUCCESS, even if there's
 *      no resulting port.
 *
 *      Note that there is nothing to prevent a user special
 *      port from disappearing after it has been discovered by
 *      the caller; thus, using a special port can always result
 *      in a "port not valid" error.
 */

kern_return_t
host_get_special_port(
        host_t          host,
        int             node,
        int             id,
        ipc_port_t      *portp)
{
#if     DIPC
        return norma_get_special_port(host, node, id, portp);
#else
        return KERN_FAILURE;
#endif
}

