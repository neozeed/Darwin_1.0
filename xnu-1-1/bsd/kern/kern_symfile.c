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
/* Copyright (c) 1998 Apple Computer, Inc.  All rights reserved.
 *
 *	File:	bsd/kern/kern_symfile.c
 *
 *	This file contains creates a dummy symbol file for mach_kernel based on
 *      the symbol table information passed by the SecondaryLoader/PlatformExpert.
 *      This allows us to correctly link other executables (drivers, etc) against the 
 *      the kernel in cases where the kernel image on the root device does not match
 *      the live kernel. This can occur during net-booting where the actual kernel
 *      image is obtained from the network via tftp rather than the root
 *      device.
 *
 *      If a symbol table is available, then the file /mach.sym will be created
 *      containing a Mach Header and a LC_SYMTAB load command followed by the
 *      the symbol table data for mach_kernel.
 *
 * HISTORY
 * 
 *	.
 */

#import <mach/vm_param.h>

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/signalvar.h>
#include <sys/resourcevar.h>
#include <sys/namei.h>
#include <sys/vnode.h>
#include <sys/proc.h>
#include <sys/timeb.h>
#include <sys/times.h>
#include <sys/buf.h>
#include <sys/acct.h>
#include <sys/file.h>
#include <sys/uio.h>
#include <sys/kernel.h>
#include <sys/stat.h>

#include <mach-o/loader.h>
#include <mach-o/nlist.h>

#include <vm/vm_kern.h>

extern unsigned char 	rootdevice[];
extern vm_size_t	page_size;


int kernel_symfile_opened = 0;
int error_code = 0;

extern int  IODTGetLoaderInfo(  char *key, void **infoAddr, int *infoSize );
extern void IODTFreeLoaderInfo( char *key, void *infoAddr, int infoSize );

/*
 * 
 */
int get_kernel_symfile( struct proc *p, char **symfile )
{
    if ( kernel_symfile_opened == 0 )
    {
        kernel_symfile_opened = 1;
        error_code = output_kernel_symbols( p );
    }
    if ( error_code == 0 ) *symfile = "\\mach.sym";

    return error_code;
}

/*
 * 
 */
int output_kernel_symbols( register struct proc *p )
{
	register struct vnode		*vp;
	register struct pcred 		*pcred = p->p_cred;
	register struct ucred 		*cred = pcred->pc_ucred;
	struct nameidata 		nd;
	struct vattr			vattr;
	struct mach_header		*orig_mh, *mh;
        struct load_command		*lc;
        struct segment_command		*sg;
        struct section			*se;
	struct symtab_command 		*sc, *sc0;
        struct nlist                    *nl;
	vm_size_t			orig_mhsize, sc0_size;
        vm_offset_t			header;
        vm_size_t			header_size;
	int				error, error1;
        int				i, j;
        int				symfoffset, symsize;
        int                             rc_mh, rc_sc;

        error = EFAULT;

        vp       = NULL;
        header   = NULL;
        orig_mh  = NULL;
        sc0      = NULL;
        
        rc_mh = IODTGetLoaderInfo( "Kernel-__HEADER", (void **)&orig_mh, &orig_mhsize );
        rc_sc = IODTGetLoaderInfo( "Kernel-__SYMTAB", (void **)&sc0, &sc0_size );
        
        if ( rc_mh != 0 || orig_mh == 0 || orig_mhsize < sizeof(struct mach_header) )    goto out;
        if ( rc_sc != 0 || sc0 == 0     || sc0_size    < sizeof(struct symtab_command) ) goto out;

	if ( pcred->p_svuid != pcred->p_ruid || pcred->p_svgid != pcred->p_rgid ) goto out;

        if ( rootdevice[0] == 'e' && rootdevice[1] == 'n' ) goto out;

	NDINIT(&nd, LOOKUP, FOLLOW, UIO_SYSSPACE, "mach.sym", p);
	if( (error = vn_open(&nd, O_CREAT | FWRITE, S_IRUSR | S_IRGRP | S_IROTH )) != 0 ) goto out;

	vp = nd.ni_vp;
	
	/* Don't dump to non-regular files or files with links. */
        error = EFAULT;
	if (vp->v_type != VREG || VOP_GETATTR(vp, &vattr, cred, p) || vattr.va_nlink != 1) goto out;

	VATTR_NULL(&vattr);
	vattr.va_size = 0;
	VOP_LEASE(vp, p, cred, LEASE_WRITE);
	VOP_SETATTR(vp, &vattr, cred, p);
	p->p_acflag |= ACORE;

	header_size =   sizeof(struct mach_header) 
                      + sizeof(struct segment_command)
                      + sizeof(struct section)
                      + sizeof(struct symtab_command);

	(void) kmem_alloc_wired( kernel_map,
			        (vm_offset_t *)&header,
			        (vm_size_t)header_size);

        if ( header == NULL ) goto out;

        bzero( (void *)header, header_size );

	/*
	 *	Set up Mach-O header.
	 */
	mh = (struct mach_header *) header;
	mh->magic = orig_mh->magic;
	mh->cputype = orig_mh->cputype;
	mh->cpusubtype = orig_mh->cpusubtype;
	mh->filetype = orig_mh->filetype;
	mh->ncmds = 2;
	mh->sizeofcmds = header_size - sizeof(struct mach_header);

	/*
	 *	Setup a dummy LC_SEGMENT command to force page alignment of  sections.
	 */
        sg = (struct segment_command *)(mh+1);
        sg->cmd      = LC_SEGMENT;
        sg->cmdsize  = sizeof(struct segment_command) + sizeof(struct section);
        strcpy( sg->segname, SEG_DATA );
        sg->vmaddr   = 0;
        sg->vmsize   = 0x1000;
        sg->fileoff  = 0;
        sg->filesize = 0;
        sg->maxprot  = 0;
        sg->initprot = 0;
        sg->nsects   = 1;
        sg->flags    = 0;

	/*
	 *	Set up alignment section.
	 */
        for ( j=page_size-1, i=0; j; i++, j >>= 1 )
          ;

        se = (struct section *)(sg+1);
        strcpy( se->sectname, SECT_BSS );
        strcpy( se->segname, sg->segname );
        se->addr  = 0;
        se->size  = 0;
        se->align = i;
        se->flags = S_ZEROFILL;

        symfoffset = round_page(header_size);

	/*
	 *	Set up LC_SYMTAB command
	 */
	sc          = (struct symtab_command *)(se+1);
	sc->cmd     = LC_SYMTAB;
	sc->cmdsize = sizeof(struct symtab_command);
        sc->symoff  = symfoffset;
        sc->nsyms   = sc0->nsyms;
        sc->strsize = sc0->strsize;
        sc->stroff =  symfoffset + sc->nsyms * sizeof(struct nlist);    

        symsize = sc->nsyms * sizeof(struct nlist) + sc->strsize;

        nl = (struct nlist *)(sc0+1);
        for (i = 0; i < sc->nsyms; i++, nl++ )
        {
            if ( (nl->n_type & N_TYPE) == N_SECT )
            {
                nl->n_sect = NO_SECT;
                nl->n_type = (nl->n_type & ~N_TYPE) | N_ABS;
            }
        }

	/*
	 *	Write out the load commands at the beginning of the
	 *	file.
	 */
	error = vn_rdwr(UIO_WRITE, vp, (caddr_t)mh, header_size, (off_t)0,
   		        UIO_SYSSPACE, IO_NODELOCKED|IO_UNIT, cred, (int *) 0, p);
        if ( error != 0 ) goto out;
        
        /*
         * 	Write out kernel symbols
         */
	error = vn_rdwr(UIO_WRITE, vp, (caddr_t)(sc0+1), symsize, symfoffset,
	    		UIO_SYSSPACE, IO_NODELOCKED|IO_UNIT, cred, (int *) 0, p);
        if ( error != 0 ) goto out;

out:
        if ( header != 0 ) kmem_free(kernel_map, header, header_size);
        if ( orig_mh != 0 ) IODTFreeLoaderInfo( "Kernel-__HEADER", (void *)orig_mh, round_page(orig_mhsize) );
        if ( sc0     != 0 ) IODTFreeLoaderInfo( "Kernel-__SYMTAB", (void *)sc0,     round_page(sc0_size) );        
  
        if ( vp != 0 )
        {
	    VOP_UNLOCK(vp, 0, p);
	    error1 = vn_close(vp, FWRITE, cred, p);
	    if (error == 0) error = error1;
        }

	return(error);
}
