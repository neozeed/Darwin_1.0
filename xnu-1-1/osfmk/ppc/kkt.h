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
 * @OSF_FREE_COPYRIGHT@
 * 
 */

#ifndef _PPC_KKT_H_
#define _PPC_KKT_H_

#include <norma_scsi.h>
#include <dipc_xkern.h>

#if     NORMA_SCSI
#define	splkkt	splbio
#elif   DIPC_XKERN
/*
 * With DIPC/KKT/x-kernel, the whole protocol stack 
 * is executed in thread context. Thus, splkkt results 
 * in a no-op.
 */
#include <cpus.h>
#if     NCPUS > 1

#if     defined(__GNUC__)
#include <kern/spl.h>
#include <ppc/cpu_number.h>

#warning This code has been compiled but not tested

extern int curr_ipl[];
extern spl_t __inline__ splkkt(void);

extern spl_t __inline__ splkkt(void) {
	spl_t   ss;
	disable_preemption();
	ss = (spl_t)curr_ipl[cpu_number()];
	enable_preemption();
	return  ss;
}
#else   /* __GNUC__ */
#error  Not implemented
#endif  /* __GNUC__ */

#else   /* NCPUS == 1 */

extern int curr_ipl;
#define	splkkt()	((spl_t)curr_ipl)

#endif  /* NCPUS == 1 */
#endif  /* DIPC_XKERN */

typedef struct transport {
	struct request_block 	*next;
#if	NORMA_SCSI
	void *kkte;
#else	/* NORMA_SCSI */
        vm_offset_t save_address;
        vm_size_t save_size;
        boolean_t save_sg;
#endif	/* NORMA_SCSI */
} transport_t;

#endif  /* _PPC_KKT_H_ */
