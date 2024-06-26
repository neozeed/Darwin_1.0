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
/*-
 * Copyright (c) 1993
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
 *	@(#)ufs_readwrite.c	8.11 (Berkeley) 5/8/95
 */
#include <mach_nbc.h>
#if MACH_NBC
#include <kern/mapfs.h>
#endif /* MACH_NBC */

#ifdef LFS_READWRITE
#define	BLKSIZE(a, b, c)	blksize(a, b, c)
#define	FS			struct lfs
#define	I_FS			i_lfs
#define	READ			lfs_read
#define	READ_S			"lfs_read"
#define	WRITE			lfs_write
#define	WRITE_S			"lfs_write"
#define	PGRD			lfs_pgrd
#define	PGRD_S			"lfs_pgrd"
#define	PGWR			lfs_pgwr
#define	PGWR_S			"lfs_pgwr"

#define	fs_bsize		lfs_bsize
#define	fs_maxfilesize		lfs_maxfilesize
#else
#define	BLKSIZE(a, b, c)	blksize(a, b, c)
#define	FS			struct fs
#define	I_FS			i_fs
#define	READ			ffs_read
#define	READ_S			"ffs_read"
#define	WRITE			ffs_write
#define	WRITE_S			"ffs_write"
#define	PGRD			ffs_pgrd
#define	PGRD_S			"ffs_pgrd"
#define	PGWR			ffs_pgwr
#define	PGWR_S			"ffs_pgwr"
#endif

/*
 * Vnode op for reading.
 */
/* ARGSUSED */
READ(ap)
	struct vop_read_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int a_ioflag;
		struct ucred *a_cred;
	} */ *ap;
{
	register struct vnode *vp;
	register struct inode *ip;
	register struct uio *uio;
	register FS *fs;
	struct buf *bp;
	ufs_daddr_t lbn, nextlbn;
	off_t bytesinfile;
	long size, xfersize, blkoffset;
#ifdef NeXT
	int devBlockSize=0;
	int firstpass;
#endif /* NeXT */
	int error;
	u_short mode;
	int seq;	/* used only by the cluster read code */
#if REV_ENDIAN_FS
	int rev_endian=0;
#endif /* REV_ENDIAN_FS */

	vp = ap->a_vp;
	ip = VTOI(vp);
	mode = ip->i_mode;
	uio = ap->a_uio;

#if REV_ENDIAN_FS
	rev_endian=(vp->v_mount->mnt_flag & MNT_REVEND);
#endif /* REV_ENDIAN_FS */

#if DIAGNOSTIC
	if (uio->uio_rw != UIO_READ)
		panic("%s: mode", READ_S);

	if (vp->v_type == VLNK) {
		if ((int)ip->i_size < vp->v_mount->mnt_maxsymlinklen)
			panic("%s: short symlink", READ_S);
	} else if (vp->v_type != VREG && vp->v_type != VDIR)
		panic("%s: type %d", READ_S, vp->v_type);
#endif
	fs = ip->I_FS;
	if ((u_int64_t)uio->uio_offset > fs->fs_maxfilesize)
		return (EFBIG);

#ifdef NeXT
	VOP_DEVBLOCKSIZE(ip->i_devvp, &devBlockSize);
#endif /* NeXT */

	for (error = 0, bp = NULL, firstpass = TRUE; uio->uio_resid > 0; 
	    bp = NULL) {
		if ((bytesinfile = ip->i_size - uio->uio_offset) <= 0)
			break;
		lbn = lblkno(fs, uio->uio_offset);
		nextlbn = lbn + 1;
		size = BLKSIZE(fs, ip, lbn);
		blkoffset = blkoff(fs, uio->uio_offset);
		xfersize = fs->fs_bsize - blkoffset;
		if (uio->uio_resid < xfersize)
			xfersize = uio->uio_resid;
		if (bytesinfile < xfersize)
			xfersize = bytesinfile;

#ifdef LFS_READWRITE
		(void)lfs_check(vp, lbn);
#ifdef NeXT
		error = cluster_read(vp, ip->i_size, lbn, size, NOCRED, &bp,
				     devBlockSize, firstpass, 
							 (uio->uio_resid + blkoffset), &seq);
#else
		error = cluster_read(vp, ip->i_size, lbn, size, NOCRED, &bp);
#endif /* NeXT */
#else
		if (lblktosize(fs, nextlbn) >= ip->i_size)
			error = bread(vp, lbn, size, NOCRED, &bp);
		else if (doclusterread && !(vp->v_flag & VRAOFF))
#ifdef NeXT
			error = cluster_read(vp,
			    ip->i_size, lbn, size, NOCRED, &bp, devBlockSize,
				firstpass, (uio->uio_resid + blkoffset), &seq);
#else
			error = cluster_read(vp,
			     ip->i_size, lbn, size, NOCRED, &bp);
#endif /* NeXT */
		else if (lbn - 1 == vp->v_lastr && !(vp->v_flag & VRAOFF)) {
			int nextsize = BLKSIZE(fs, ip, nextlbn);
			error = breadn(vp, lbn,
			    size, &nextlbn, &nextsize, 1, NOCRED, &bp);
		} else
			error = bread(vp, lbn, size, NOCRED, &bp);
#endif
		firstpass = FALSE;
		if (error)
			break;
		vp->v_lastr = lbn;

		/*
		 * We should only get non-zero b_resid when an I/O error
		 * has occurred, which should cause us to break above.
		 * However, if the short read did not cause an error,
		 * then we want to ensure that we do not uiomove bad
		 * or uninitialized data.
		 */
		size -= bp->b_resid;
		if (size < xfersize) {
			if (size == 0)
				break;
			xfersize = size;
		}
#if REV_ENDIAN_FS
		if (rev_endian && S_ISDIR(mode)) {
			byte_swap_dir_block_in((char *)bp->b_data + blkoffset, xfersize);
		}
#endif /* REV_ENDIAN_FS */
		if (error =
		    uiomove((char *)bp->b_data + blkoffset, (int)xfersize, uio)) {
#if REV_ENDIAN_FS
			if (rev_endian && S_ISDIR(mode)) {
			byte_swap_dir_block_in((char *)bp->b_data + blkoffset, xfersize);
			}
#endif /* REV_ENDIAN_FS */
			break;
	}

#if REV_ENDIAN_FS
		if (rev_endian && S_ISDIR(mode)) {
			byte_swap_dir_out((char *)bp->b_data + blkoffset, xfersize);
		}
#endif /* REV_ENDIAN_FS */
		if (S_ISREG(mode) && (xfersize + blkoffset == fs->fs_bsize ||
		    uio->uio_offset == ip->i_size))
			bp->b_flags |= B_AGE;
		brelse(bp);
	}
	if (bp != NULL)
		brelse(bp);
	ip->i_flag |= IN_ACCESS;
	return (error);
}

/*
 * Vnode op for writing.
 */
WRITE(ap)
	struct vop_write_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int a_ioflag;
		struct ucred *a_cred;
	} */ *ap;
{
	register struct vnode *vp;
	register struct uio *uio;
	register struct inode *ip;
	register FS *fs;
	struct buf *bp;
	struct proc *p;
	ufs_daddr_t lbn;
	off_t osize;
	int blkoffset, error, flags, ioflag, resid, size, xfersize;
#ifdef NeXT
	int devBlockSize=0;
#endif /* NeXT */
#if REV_ENDIAN_FS
	int rev_endian=0;
#endif /* REV_ENDIAN_FS */

	ioflag = ap->a_ioflag;
	uio = ap->a_uio;
	vp = ap->a_vp;
	ip = VTOI(vp);
#if REV_ENDIAN_FS
	rev_endian=(vp->v_mount->mnt_flag & MNT_REVEND);
#endif /* REV_ENDIAN_FS */

#if DIAGNOSTIC
	if (uio->uio_rw != UIO_WRITE)
		panic("%s: mode", WRITE_S);
#endif

	switch (vp->v_type) {
	case VREG:
		if (ioflag & IO_APPEND)
			uio->uio_offset = ip->i_size;
		if ((ip->i_flags & APPEND) && uio->uio_offset != ip->i_size)
			return (EPERM);
		/* FALLTHROUGH */
	case VLNK:
		break;
	case VDIR:
		if ((ioflag & IO_SYNC) == 0)
			panic("%s: nonsync dir write", WRITE_S);
		break;
	default:
		panic("%s: type", WRITE_S);
	}

	fs = ip->I_FS;
	if (uio->uio_offset < 0 ||
	    (u_int64_t)uio->uio_offset + uio->uio_resid > fs->fs_maxfilesize)
		return (EFBIG);

#ifdef NeXT
	VOP_DEVBLOCKSIZE(ip->i_devvp, &devBlockSize);
#endif /* NeXT */

	/*
	 * Maybe this should be above the vnode op call, but so long as
	 * file servers have no limits, I don't think it matters.
	 */
	p = uio->uio_procp;
	if (vp->v_type == VREG && p &&
	    uio->uio_offset + uio->uio_resid >
	    p->p_rlimit[RLIMIT_FSIZE].rlim_cur) {
		psignal(p, SIGXFSZ);
		return (EFBIG);
	}

	resid = uio->uio_resid;
	osize = ip->i_size;
	flags = ioflag & IO_SYNC ? B_SYNC : 0;

	for (error = 0; uio->uio_resid > 0;) {
		lbn = lblkno(fs, uio->uio_offset);
		blkoffset = blkoff(fs, uio->uio_offset);
		xfersize = fs->fs_bsize - blkoffset;
		if (uio->uio_resid < xfersize)
			xfersize = uio->uio_resid;
#ifdef LFS_READWRITE
		(void)lfs_check(vp, lbn);
		error = lfs_balloc(vp, blkoffset, xfersize, lbn, &bp);
#else
		if (fs->fs_bsize > xfersize)
			flags |= B_CLRBUF;
		else
			flags &= ~B_CLRBUF;

		error = ffs_balloc(ip,
		    lbn, blkoffset + xfersize, ap->a_cred, &bp, flags);
#endif
		if (error)
			break;
		if (uio->uio_offset + xfersize > ip->i_size) {
			ip->i_size = uio->uio_offset + xfersize;

#if MACH_NBC
			if ((vp->v_type == VREG) && (vp->v_vm_info && !(vp->v_vm_info->mapped))) {
#endif /* MACH_NBC */
			vnode_pager_setsize(vp, (u_long)ip->i_size);
#if MACH_NBC
			}
#endif /* MACH_NBC */
		}
#if !MACH_NBC
		(void)vnode_uncache(vp);
#endif /* MACH_NBC */

		size = BLKSIZE(fs, ip, lbn) - bp->b_resid;
		if (size < xfersize)
			xfersize = size;

		error =
		    uiomove((char *)bp->b_data + blkoffset, (int)xfersize, uio);
#if REV_ENDIAN_FS
		if (rev_endian && S_ISDIR(ip->i_mode)) {
			byte_swap_dir_out((char *)bp->b_data + blkoffset, xfersize);
		}
#endif /* REV_ENDIAN_FS */
#ifdef LFS_READWRITE
		(void)VOP_BWRITE(bp);
#else
		if (ioflag & IO_SYNC)
			(void)bwrite(bp);
		else if (xfersize + blkoffset == fs->fs_bsize)
			if (doclusterwrite)
			{
#ifdef NeXT
				cluster_write(bp, ip->i_size, devBlockSize);
#else
				cluster_write(bp, ip->i_size);
#endif /* NeXT */

			} else {
				bp->b_flags |= B_AGE;
				bawrite(bp);
			}
		else
			bdwrite(bp);
#endif
		if (error || xfersize == 0)
			break;
		ip->i_flag |= IN_CHANGE | IN_UPDATE;
	}
	/*
	 * If we successfully wrote any data, and we are not the superuser
	 * we clear the setuid and setgid bits as a precaution against
	 * tampering.
	 */
	if (resid > uio->uio_resid && ap->a_cred && ap->a_cred->cr_uid != 0)
		ip->i_mode &= ~(ISUID | ISGID);
	if (error) {
		if (ioflag & IO_UNIT) {
			(void)VOP_TRUNCATE(vp, osize,
			    ioflag & IO_SYNC, ap->a_cred, uio->uio_procp);
			uio->uio_offset -= resid - uio->uio_resid;
			uio->uio_resid = resid;
		}
	} else if (resid > uio->uio_resid && (ioflag & IO_SYNC))
		error = VOP_UPDATE(vp, &time, &time, 1);
	return (error);
}

/*
 * Vnode op for page read.
 */
/* ARGSUSED */
PGRD(ap)
	struct vop_pgrd_args /* {
	struct vnode *a_vp;
	struct uio *a_uio;
	struct ucred *a_cred;
	} */ *ap;
{

#warning ufs_readwrite PGRD need to implement
return (EOPNOTSUPP);

}

/*
 * Vnode op for page read.
 */
/* ARGSUSED */
PGWR(ap)
	struct vop_pgwr_args /* {
	struct vnode *a_vp;
	struct uio *a_uio;
	struct ucred *a_cred;
	memory_object_t a_pager;
	vm_offset_t a_offset;
	} */ *ap;
{

#warning ufs_readwrite PGWR need to implement
return (EOPNOTSUPP);

}


