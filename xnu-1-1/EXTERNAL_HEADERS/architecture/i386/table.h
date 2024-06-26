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
 * Copyright (c) 1992 NeXT Computer, Inc.
 *
 * Intel386 Family:	Descriptor tables.
 *
 * HISTORY
 *
 * 30 March 1992 ? at NeXT
 *	Created.
 */

#import <architecture/i386/desc.h>
#import <architecture/i386/tss.h>

/*
 * A totally generic descriptor
 * table entry.
 */

typedef union dt_entry {
    code_desc_t		code;
    data_desc_t		data;
    ldt_desc_t		ldt;
    tss_desc_t		task_state;
    call_gate_t		call_gate;
    trap_gate_t		trap_gate;
    intr_gate_t		intr_gate;
    task_gate_t		task_gate;
} dt_entry_t;

#define DESC_TBL_MAX	8192

/*
 * Global descriptor table.
 */

typedef union gdt_entry {
    code_desc_t		code;
    data_desc_t		data;
    ldt_desc_t		ldt;
    call_gate_t		call_gate;
    task_gate_t		task_gate;
    tss_desc_t		task_state;
} gdt_entry_t;

typedef gdt_entry_t	gdt_t;

/*
 * Interrupt descriptor table.
 */

typedef union idt_entry {
    trap_gate_t		trap_gate;
    intr_gate_t		intr_gate;
    task_gate_t		task_gate;
} idt_entry_t;

typedef idt_entry_t	idt_t;

/*
 * Local descriptor table.
 */

typedef union ldt_entry {
    code_desc_t		code;
    data_desc_t		data;
    call_gate_t		call_gate;
    task_gate_t		task_gate;
} ldt_entry_t;

typedef ldt_entry_t	ldt_t;
