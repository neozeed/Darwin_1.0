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
#include <pexpert/pexpert.h>
#include <pexpert/protos.h>
#include <pexpert/device_tree.h>

/* Local declarations */
void		pe_identify_machine(boot_args *args);

/* pe_identify_machine:
 *
 *   Sets up platform parameters.
 *   Returns:    nothing
 */
void pe_identify_machine(boot_args *args)
{
}


int pe_map_physical_range( vm_offset_t phys, unsigned int size,
		int findSpace, int cache, vm_offset_t * virt);

int pe_map_physical_range( vm_offset_t phys, unsigned int size,
		int findSpace, int cache, vm_offset_t * virt)
{
        *virt = io_map( phys, size);
        return( 0);
}
