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
/* Copyright (c) 1995 NeXT Computer, Inc. All Rights Reserved */
/*
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)vm.h	8.5 (Berkeley) 5/11/95
 */
/* HISTORY
 *  05-Jun-95  Mac Gillon (mgillon) at NeXT
 * 	4.4 code uses this file to import MACH API
 */

#ifndef _SYS_VM_H
#define _SYS_VM_H

/* Machine specific config stuff */
#if	defined(KERNEL) && !defined(MACH_USER_API)	
#include <sys/vmmeter.h>
#include <sys/queue.h>
#include <mach/vm_param.h>
#endif

/*
 * Shareable process virtual address space.
 * May eventually be merged with vm_map.
 * Several fields are temporary (text, data stuff).
 */
struct vmspace {
	int	vm_refcnt;	/* number of references */
	caddr_t	vm_shm;		/* SYS5 shared memory private data XXX */
/* we copy from vm_startcopy to the end of the structure on fork */
#define vm_startcopy vm_rssize
	segsz_t vm_rssize; 	/* current resident set size in pages */
	segsz_t vm_swrss;	/* resident set size before last swap */
	segsz_t vm_tsize;	/* text size (pages) XXX */
	segsz_t vm_dsize;	/* data size (pages) XXX */
	segsz_t vm_ssize;	/* stack size (pages) */
	caddr_t	vm_taddr;	/* user virtual address of text XXX */
	caddr_t	vm_daddr;	/* user virtual address of data XXX */
	caddr_t vm_maxsaddr;	/* user VA at max stack growth */
};

#ifdef KERNEL

#ifdef BSD_BUILD
#include <kern/thread.h>

#define current_proc()	((struct proc *)get_bsdtask_info(current_task()))
#else
struct proc * current_proc(void);
#endif

#endif /* KERNEL */

#endif /* _SYS_VM_H */
