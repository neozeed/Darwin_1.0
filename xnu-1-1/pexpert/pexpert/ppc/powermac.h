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
#ifndef _PEXPERT_PPC_POWERMAC_H_
#define _PEXPERT_PPC_POWERMAC_H_

#ifndef ASSEMBLER

#include <mach/ppc/vm_types.h>

#include <pexpert/pexpert.h>
#include <pexpert/protos.h>
#include <pexpert/ppc/boot.h>


typedef struct powermac_info {
	int		struct_version;
	int		bus_clock_rate_hz;	/* Bus frequency */
	unsigned int    dec_clock_period;	/* Fixed point number 8.24 */

	/* to convert from real time ticks to nsec convert by this*/
	unsigned long	proc_clock_to_nsec_numerator; 
	unsigned long	proc_clock_to_nsec_denominator; 
	
	unsigned long   dcache_block_size;  	/* number of bytes */
	unsigned long   dcache_size;        	/* number of bytes */
	unsigned long   icache_size;        	/* number of bytes */
	unsigned long   caches_unified;     	/* boolean_t */
	unsigned long   processor_version;  	/* contents of PVR */
	unsigned long   cpu_clock_rate_hz;
	unsigned long   dec_clock_rate_hz;

        timebase_callback_func timebase_callback;

	unsigned long   bus_clock_rate_num;
	unsigned long   bus_clock_rate_den;
	unsigned long   bus_to_cpu_rate_num;
	unsigned long   bus_to_cpu_rate_den;
	unsigned long   bus_to_dec_rate_num;
	unsigned long   bus_to_dec_rate_den;

	boolean_t	io_coherent;		/* Is I/O coherent? */

} powermac_info_t;

#define POWERMAC_INFO_VERSION	1

extern powermac_info_t powermac_info;

extern void NO_ENTRY(void);

/* prototypes */

vm_offset_t PE_find_scc( void );

extern void powermac_powerdown(void);
extern void powermac_reboot(void);

/* Some useful typedefs for accessing control registers */

typedef volatile unsigned char	v_u_char;
typedef volatile unsigned short v_u_short;
typedef volatile unsigned int	v_u_int;
typedef volatile unsigned long  v_u_long;

/* And some useful defines for reading 'volatile' structures,
 * don't forget to be be careful about sync()s and eieio()s
 */
#define reg8(reg) (*(v_u_char *)reg)
#define reg16(reg) (*(v_u_short *)reg)
#define reg32(reg) (*(v_u_int *)reg)

/* Non-cached version of bcopy */
extern void	bcopy_nc(char *from, char *to, int size);

#endif /* ASSEMBLER */

#endif /* _PEXPERT_PPC_POWERMAC_H_ */
