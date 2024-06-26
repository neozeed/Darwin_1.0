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

#ifndef	_PPC_MACHINE_ROUTINES_H_
#define	_PPC_MACHINE_ROUTINES_H_

#include <mach/mach_types.h>
#include <mach/boolean.h>
#include <kern/kern_types.h>
#include <pexpert/pexpert.h>


#if	defined(PEXPERT_KERNEL_PRIVATE) || defined(MACH_KERNEL_PRIVATE)
/* IO memory map services */

/* Map memory map IO space */
vm_offset_t ml_io_map(
	vm_offset_t phys_addr, 
	vm_size_t size);

/* boot memory allocation */
vm_offset_t ml_static_malloc(
	vm_size_t size);
#endif

vm_offset_t
ml_static_ptovirt(
	vm_offset_t);

void ml_static_mfree(
	vm_offset_t,
	vm_size_t);
        
/* virtual to physical on wired pages */
vm_offset_t ml_vtophys(
	vm_offset_t vaddr);

/* Init Interrupts */
void ml_install_interrupt_handler(
	void *nub,
	int source,
	void *target,
	IOInterruptHandler handler,
	void *refCon);
               
#ifdef	MACH_KERNEL_PRIVATE
boolean_t fake_get_interrupts_enabled(void);
boolean_t fake_set_interrupts_enabled(boolean_t enable);
#endif

/* Get Interrupts Enabled */
boolean_t ml_get_interrupts_enabled(void);

/* Set Interrupts Enabled */
boolean_t ml_set_interrupts_enabled(boolean_t enable);

/* Check if running at interrupt context */
boolean_t ml_at_interrupt_context(void);

/* Generate a fake interrupt */
void ml_cause_interrupt(void);

#ifdef	MACH_KERNEL_PRIVATE
/* check pending timers */
void machine_clock_assist(void);
#endif

/* PCI config cycle probing */
boolean_t ml_probe_read(
	vm_offset_t paddr,
	unsigned int *val);

/* Read physical address */
unsigned int ml_phys_read(
	vm_offset_t paddr);

/* Write physical address */
void ml_phys_write(
	vm_offset_t paddr, unsigned int data);

/* Type for the IPI Hander */
typedef void (*ipi_handler_t)(void);

/* Register a processor */
kern_return_t ml_processor_register(
	cpu_id_t cpu_id,
	vm_offset_t start_paddr,
	processor_t *processor,
	ipi_handler_t *ipi_handler,
	boolean_t boot_cpu);

#endif /* _PPC_MACHINE_ROUTINES_H_ */
