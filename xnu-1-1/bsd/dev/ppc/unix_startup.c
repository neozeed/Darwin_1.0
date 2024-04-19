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
 * Copyright (c) 1992,7 NeXT Computer, Inc.
 *
 * Unix data structure initialization.
 *
 * HISTORY
 *
 * 11 Mar 1997 Eryk Vershen @ NeXT
 *	Modified from hppa version.
 * 26 May 1992 ? at NeXT
 *	Created from 68k version.
 */

#include <mach_nbc.h>
#include <mach/mach_types.h>

#include <vm/vm_kern.h>
#include <mach/vm_prot.h>

#include <sys/param.h>
#include <sys/buf.h>
#include <sys/clist.h>
#include <sys/mbuf.h>
#include <sys/systm.h>
#include <sys/tty.h>
#include <bsd/dev/ppc/cons.h>


extern vm_map_t	mb_map;
/*
 * Declare these as initialized data so we can patch them.
 */
int	niobuf = 0;

#ifdef	NBUF
int		nbuf = NBUF;
#else
int		nbuf = 0;
#endif
#ifdef	NMFSBUF
int	nmfsbuf = NMFSBUF
#else
int	nmfsbuf = 0;
#endif
#ifdef	BUFPAGES
int		bufpages = BUFPAGES;
#else
int		bufpages = 0;
#endif

int	srv = 0;          /* Flag indicates a server boot when set */
int	ncl = 0;

vm_map_t	buffer_map;
vm_map_t	bufferhdr_map;


void
bsd_startupearly()
{
	vm_offset_t		v, firstaddr, trash_offset;
	vm_size_t		size;
	kern_return_t	ret;


	/*
	 *      Since these pages are virtual-size pages (larger
	 *      than physical page size), use only one page
	 *      per buffer.
	 */

    if (bufpages == 0) {
		if (srv) {
			if (mem_size > (64 * 1024 * 1024)) {
				bufpages = atop(mem_size) / 2;

			if ((mem_size - ptoa(bufpages)) < (64 * 1024 * 1024))
				bufpages = atop(mem_size - (64 * 1024 * 1024));
			} else {
				bufpages = atop(mem_size / 100);
				bufpages = bufpages * 3; /* 3% */
			}
		} else {
			bufpages = atop(mem_size / 100);
			bufpages = bufpages * 3;  /* 3% */
		}
	}

    if (nbuf == 0) {
#if     PRIVATE_BUFS
		nbuf = 100;
#else   PRIVATE_BUFS
	    /* Go for a 1-1 correspondence between the number of buffer
	     * headers and bufpages.  Then add some extra (empty) buffer
	     * headers to aid clustering.
	     */
	    if (bufpages > 65536)
	            bufpages = 65536;
	    if ((nbuf = bufpages) < 16)
	            nbuf = 16;
	    nbuf += 64;
#endif  PRIVATE_BUFS
	} else if (nbuf > 65536)
		nbuf = 65536;

    if (bufpages > nbuf * (MAXBSIZE / page_size))
		bufpages = nbuf * (MAXBSIZE / page_size);

	if (niobuf == 0) {
		niobuf = bufpages / (MAXPHYSIO / page_size);
		if (niobuf < 128)
			niobuf = 128;
	}
	if (niobuf > 4096)
		niobuf = 4096;

	size = (nbuf + niobuf) * sizeof (struct buf);
	size = round_page(size);

	ret = kmem_suballoc(kernel_map,
			&firstaddr,
			size,
			FALSE,
			TRUE,
			&bufferhdr_map);

	if (ret != KERN_SUCCESS) 
		panic("Failed to create bufferhdr_map\n");
	
	if (kernel_memory_allocate(bufferhdr_map, &firstaddr, size,
   		0,
		KMA_HERE | KMA_KOBJECT) != KERN_SUCCESS)
			panic("Failed to allocate bufferhdr_map\n");

	buf = (struct buf * )firstaddr;
	bzero(buf,size);


	/*
	 * Unless set at the boot command line, mfs gets no more than
	 * half of the system's bufs.  Hack to prevent buf starvation
	 * and system hang.
	 */
	if (nmfsbuf == 0)
		nmfsbuf = nbuf / 2;

	if ((mem_size > (64 * 1024 * 1024)) || ncl) {
		int scale;
		extern u_long tcp_sendspace;
		extern u_long tcp_recvspace;

		if ((nmbclusters = ncl) == 0) {
			if ((nmbclusters = ((mem_size / 16) / MCLBYTES)) > 8192)
				nmbclusters = 8192;
		}
		if ((scale = nmbclusters / NMBCLUSTERS) > 1) {
			tcp_sendspace *= scale;
			tcp_recvspace *= scale;

			if (tcp_sendspace > (32 * 1024))
				tcp_sendspace = 32 * 1024;
			if (tcp_recvspace > (32 * 1024))
				tcp_recvspace = 32 * 1024;
		}
	}
}

void
bsd_bufferinit()
{
    unsigned int	i;
    vm_offset_t		v;
    vm_size_t		size;
    kern_return_t	ret;
    vm_offset_t		trash_offset, firstaddr;
    int			base, residual;

    cons.t_dev = makedev(12, 0);


	bsd_startupearly();

	size = round_page(nbuf * MAXBSIZE) + (niobuf * MAXPHYSIO);
   	 ret = kmem_suballoc(kernel_map,
			&firstaddr,
			size,
			TRUE,
			TRUE,
			&buffer_map);

	if (ret != KERN_SUCCESS) 
		panic("Failed to create buffer_map\n");
	buffers = firstaddr;
	printf("Buffer cache at %x\n",buffers);
	base = bufpages / nbuf;
	residual = bufpages % nbuf;

    for (i = 0; i < nbuf; i++) {
	vm_size_t	thisbsize;
	vm_offset_t	curbuf;

	/*
	 * First <residual> buffers get (base+1) physical pages
	 * allocated for them.  The rest get (base) physical pages.
	 *
	 * The rest of each buffer occupies virtual space,
	 * but has no physical memory allocated for it.
	 */

	thisbsize = page_size*(i < residual ? base+1 : base);
	curbuf = (vm_offset_t)buffers + i * MAXBSIZE;
	if (thisbsize) {
	ret = vm_map_enter(buffer_map, &curbuf, thisbsize,
			  (vm_offset_t) 0, FALSE,
			  VM_OBJECT_NULL, (vm_offset_t) 0, FALSE,
			  VM_PROT_DEFAULT, VM_PROT_ALL, VM_INHERIT_DEFAULT);
	if (ret != KERN_SUCCESS) {
		panic("Failed to allocate buffer cache pages\n");
	}
	ret = vm_map_wire(buffer_map, curbuf, curbuf+thisbsize, VM_PROT_READ | VM_PROT_WRITE, FALSE);
	if (ret != KERN_SUCCESS)
		panic("Failed to wire buffer cache pages\n");

	} else {
		/* printf("skipping allocating buffer page to buf %d\n",i); */
	}
    }


#define MEG	(1024*1024)
    {
	register int	nbytes;

	nbytes = ptoa(bufpages);
	printf("using %d buffers containing %d.%d%d megabytes of memory\n",
		nbuf,
		nbytes/MEG,
		((nbytes%MEG)*10)/MEG,
		((nbytes%(MEG/10))*100)/MEG);

    }

   	 ret = kmem_suballoc(kernel_map,
			&mbutl,
			(vm_size_t) (nmbclusters * MCLBYTES),
			FALSE,
			TRUE,
			&mb_map);

	if (ret != KERN_SUCCESS) 
		panic("Failed to allocate mb_map\n");
	/* embutl = ((unsigned char *)mbutl + (nmbclusters * MCLBYTES)); */
	
    /*
     * Set up buffers, so they can be used to read disk labels.
     */
    bufinit();
}

