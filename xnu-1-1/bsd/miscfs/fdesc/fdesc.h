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
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software donated to Berkeley by
 * Jan-Simon Pendry.
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
 *	@(#)fdesc.h	8.6 (Berkeley) 8/20/94
 *
 * #Id: fdesc.h,v 1.8 1993/04/06 15:28:33 jsp Exp #
 */

#ifdef KERNEL
struct fdescmount {
	struct vnode	*f_root;	/* Root node */
};

#define FD_ROOT		2
#define FD_DEVFD	3
#define FD_STDIN	4
#define FD_STDOUT	5
#define FD_STDERR	6
#define FD_CTTY		7
#define FD_DESC		8
#define FD_MAX		12

typedef enum {
	Froot,
	Fdevfd,
	Fdesc,
	Flink,
	Fctty
} fdntype;

struct fdescnode {
	LIST_ENTRY(fdescnode) fd_hash;	/* Hash list */
	struct vnode	*fd_vnode;	/* Back ptr to vnode */
	fdntype		fd_type;	/* Type of this node */
	unsigned	fd_fd;		/* Fd to be dup'ed */
	char		*fd_link;	/* Link to fd/n */
	int		fd_ix;		/* filesystem index */
};

#define VFSTOFDESC(mp)	((struct fdescmount *)((mp)->mnt_data))
#define	VTOFDESC(vp) ((struct fdescnode *)(vp)->v_data)

extern dev_t devctty;
extern int fdesc_init __P((struct vfsconf *));
extern int fdesc_root __P((struct mount *, struct vnode **));
extern int fdesc_allocvp __P((fdntype, int, struct mount *, struct vnode **));
extern int (**fdesc_vnodeop_p)();
extern struct vfsops fdesc_vfsops;
#endif /* KERNEL */
