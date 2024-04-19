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
#include <ppc/machine_routines.h>
#include <ppc/exception.h>
#include <ppc/misc_protos.h>
#include <ppc/Firmware.h>
#include <vm/vm_page.h>
#include <ppc/pmap.h>
#include <ppc/proc_reg.h>
#include <kern/processor.h>

boolean_t get_interrupts_enabled(void);
extern boolean_t set_interrupts_enabled(boolean_t);

/* Map memory map IO space */
vm_offset_t 
ml_io_map(
	vm_offset_t phys_addr, 
	vm_size_t size)
{
	return(io_map(phys_addr,size));
}

/* static memory allocation */
vm_offset_t 
ml_static_malloc(
	vm_size_t size)
{
	extern vm_offset_t static_memory_end;
	extern boolean_t pmap_initialized;
	vm_offset_t vaddr;

	if (pmap_initialized)
		return((vm_offset_t)NULL);
	else {
		vaddr = static_memory_end;
		static_memory_end = round_page(vaddr+size);
		return(vaddr);
	}
}

vm_offset_t
ml_static_ptovirt(
	vm_offset_t paddr)
{
	extern vm_offset_t static_memory_end;
	vm_offset_t vaddr;

	/* Static memory is map V=R */
	vaddr = paddr;
	if ( (vaddr < static_memory_end) && (pmap_extract(kernel_pmap, vaddr)==paddr) )
		return(vaddr);
	else
		return((vm_offset_t)NULL);
}

void
ml_static_mfree(
	vm_offset_t vaddr,
	vm_size_t size)
{
	vm_offset_t paddr_cur, vaddr_cur;

	for (vaddr_cur = round_page(vaddr);
	     vaddr_cur < trunc_page(vaddr+size);
	     vaddr_cur += PAGE_SIZE) {
		paddr_cur = pmap_extract(kernel_pmap, vaddr_cur);
		if (paddr_cur != (vm_offset_t)NULL) {
			vm_page_wire_count--;
			pmap_remove(kernel_pmap, vaddr_cur, vaddr_cur+PAGE_SIZE);
			vm_page_create(paddr_cur,paddr_cur+PAGE_SIZE);
		}
	}
}

/* virtual to physical on wired pages */
vm_offset_t ml_vtophys(
	vm_offset_t vaddr)
{
	return(pmap_extract(kernel_pmap, vaddr));
}

/* Initialize Interrupts */
void ml_install_interrupt_handler(
	void *nub,
	int source,
	void *target,
	IOInterruptHandler handler,
	void *refCon)
{
	int	current_cpu;
	boolean_t current_state;

	current_state = ml_get_interrupts_enabled();

	current_cpu = cpu_number();
	per_proc_info[current_cpu].interrupt_nub     = nub;
	per_proc_info[current_cpu].interrupt_source  = source;
	per_proc_info[current_cpu].interrupt_target  = target;
	per_proc_info[current_cpu].interrupt_handler = handler;
	per_proc_info[current_cpu].interrupt_refCon  = refCon;
	per_proc_info[current_cpu].get_interrupts_enabled 
						= get_interrupts_enabled;
	per_proc_info[current_cpu].set_interrupts_enabled 
						= set_interrupts_enabled;  

	(void) ml_set_interrupts_enabled(current_state);
}

boolean_t fake_get_interrupts_enabled(void)
{
	/*
	 * The scheduler is not active on this cpu. There is no need to disable 
	 * preemption. The current thread wont be dispatched on anhother cpu.
	 */
	return(per_proc_info[cpu_number()].cpu_flags & turnEEon);
}

boolean_t fake_set_interrupts_enabled(boolean_t enable)
{
	boolean_t interrupt_state_prev;

	/*
	 * The scheduler is not active on this cpu. There is no need to disable 
	 * preemption. The current thread wont be dispatched on anhother cpu.
	 */
	interrupt_state_prev = 
		(per_proc_info[cpu_number()].cpu_flags & turnEEon) != 0;
	if (interrupt_state_prev != enable)
		per_proc_info[cpu_number()].cpu_flags ^= turnEEon;
	return(interrupt_state_prev);
}

/* Get Interrupts Enabled */
boolean_t ml_get_interrupts_enabled(void)
{
	return(per_proc_info[cpu_number()].get_interrupts_enabled());
}

boolean_t get_interrupts_enabled(void)
{
	return((mfmsr() & MASK(MSR_EE)) != 0);
}

/* Set Interrupts Enabled */
boolean_t ml_set_interrupts_enabled(boolean_t enable)
{
	return(per_proc_info[cpu_number()].set_interrupts_enabled(enable));
}

/* Check if running at interrupt context */
boolean_t ml_at_interrupt_context(void)
{
	/*
	 * If running at interrupt context, the current thread won't be 
	 * dispatched on another cpu. There is no need to turn off preemption.
	 */
	return (per_proc_info[cpu_number()].istackptr == 0);
}

/* Generate a fake interrupt */
void ml_cause_interrupt(void)
{
	CreateFakeIO();
}

void machine_clock_assist(void)
{
	if (per_proc_info[cpu_number()].get_interrupts_enabled == fake_get_interrupts_enabled)
		CreateFakeDEC();
}

extern void cpu_signal_handler(void);

kern_return_t
ml_processor_register(
	cpu_id_t	cpu_id,
	vm_offset_t	start_paddr,
	processor_t	*processor,
	ipi_handler_t   *ipi_handler,
	boolean_t	boot_cpu)
{
	kern_return_t ret;
	int target_cpu;

	if (boot_cpu == FALSE) {
		 if (cpu_register(&target_cpu) != KERN_SUCCESS)
			return KERN_FAILURE;
	} else {
		/* boot_cpu is always 0 */
		target_cpu= 0;
	}

	per_proc_info[target_cpu].cpu_id = cpu_id;
	per_proc_info[target_cpu].start_paddr = start_paddr;
	*processor = cpu_to_processor(target_cpu);
	*ipi_handler = cpu_signal_handler;

	return KERN_SUCCESS;
}
