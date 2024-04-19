/*
 * Copyright (c) 1999-2000 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 1991, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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
 *      hfs_vfsops.c
 *  derived from	@(#)ufs_vfsops.c	8.8 (Berkeley) 5/20/95
 *
 *      (c) Copyright 1997-1998 Apple Computer, Inc. All rights reserved.
 *
 *      hfs_vfsops.c -- VFS layer for loadable HFS file system.
 *
 *      HISTORY
 *	 9-Nov-1999	Don Brady	Fix error handling in hfs_unmount [2399157].
 *	 9-Sep-1999	Don Brady	Clear system file fcbModified flags in hfs_flushvolumeheader/hfs_flushMDB.
 *	 5-Aug-1999	Pat Dirks	Moved special HFS flag from f_fsid.val[0][0] to mount flags (#2293117).
 *	23-Jul-1999	Pat Dirks	Added special-case code for root's parent directory in hfs_vget (#2263664).
 *	 9-Jun-1999	Don Brady	Fix hfs_mount for reload and read-only downgrade cases.
 *	 2-Jun-1999	Don Brady	Fix hfs_statfs to return correct f_files value.
 *	 4-May-1999	Don Brady	Remove obsolete loadable module code.
 *	22-Mar-1999	Don Brady	Hide our private meta data in hfs_vget.
 *		18-May-1999     Don Brady       Add hfs_mountroot for HFS Plus rooting.
 *		22-Mar-1999	Don Brady	Hide our private meta data in hfs_vget.
 *              12-Nov-1998     Pat Dirks       Changed hfs_statfs to return volume's actual log. block size (#2286198).
 *		22-Aug-1998	Scott Roberts	Assign uid,gid, and mask for default on objects.
 *		29-Jul-1998	Pat Dirks		Fixed changed hfs_vget() to release complex node when retrying for data fork node.
 *		27-Jul-1998	Scott Roberts	Changes hfs_vget() to return data forks instead of complex.
 *         14-Jul-1998  CHW			Added check for use count of device node in hfs_mountfs
 *	    1-Jul-1998	Don Brady		Always set kHFSVolumeUnmountedMask bit of vcb->vcbAtrb in hfs_unmount.
 *	   30-Jun-1998	Don Brady		Removed hard-coded EINVAL error in hfs_mountfs (for radar #2249539).
 *	   24-Jun-1998	Don Brady		Added setting of timezone to hfs_mount (radar #2226387).
 *		4-Jun-1998	Don Brady		Use VPUT/VRELE macros instead of vput/vrele.
 *		6-May-1998	Scott Roberts	Updated hfs_vget with kernel changes.
 *		29-Apr-1998	Don Brady		Update hfs_statfs to actually fill in statfs fields (radar #2227092).
 *		23-Apr-1998	Pat Dirks		Cleaned up code to call brelse() on errors from bread().
 *		 4/20/1998	Don Brady		Remove course-grained hfs metadata locking.
 *		 4/18/1998	Don Brady		Add VCB locking.
 *		 4/16/1998	Don Brady	hfs_unmount now flushes the volume bitmap. Add b-tree locking to hfs_vget.
 *		  4/8/1998	Don Brady	Replace hfs_mdbupdate with hfs_flushvolumeheader and hfs_flushMDB.
 *		  4/8/1998	Don Brady	In hfs_unmount call hfs_mdbupdate before trashing metafiles!
 *		  4/3/1998	Don Brady	Call InitCatalogCache instead of PostInitFS.
 *		  4/1/1998	Don Brady	Get rid of gHFSFlags, gReqstVol and gFlushOnlyFlag globals (not used).
 *		 3/30/1998	Don Brady	In hfs_unmount use SKIPSYSTEM option on first vflush.
 *		 3/26/1998	Don Brady	Changed hfs_unmount to call vflush before calling hfsUnmount.
 *								In hfs_mountfs don't mount hfs-wrapper.
 *		 3/19/1998	Pat Dirks	Fixed bug in hfs_mount where device vnode was being
 *								released on way out.
 *      11/14/1997	Pat Dirks	Derived from hfs_vfsops.c
 */
#include <sys/param.h>
#include <sys/systm.h>

#include <sys/namei.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/malloc.h>
#include <sys/stat.h>
#include <dev/disk.h>
#include <sys/lock.h>

#include <miscfs/specfs/specdev.h>
#include <kern/mapfs.h>

#include <hfs/hfs_mount.h>

#include "hfs.h"
#include "hfs_dbg.h"

#include "hfscommon/headers/FileMgrInternal.h"

#if	HFS_DIAGNOSTIC
int hfs_dbg_all = 0;
int hfs_dbg_vfs = 0;
int hfs_dbg_vop = 0;
int hfs_dbg_load = 0;
int hfs_dbg_io = 0;
int hfs_dbg_utils = 0;
int hfs_dbg_rw = 0;
int hfs_dbg_lookup = 0;
int hfs_dbg_tree = 0;
int hfs_dbg_err = 0;
int hfs_dbg_test = 0;
#endif

/*
 * HFS File System globals:
 */
Ptr					gBufferAddress[BUFFERPTRLISTSIZE];
struct buf			*gBufferHeaderPtr[BUFFERPTRLISTSIZE];
int					gBufferListIndex;
simple_lock_data_t	gBufferPtrListLock;

//static char hfs_fs_name[MFSNAMELEN] = "hfs";

/* The following represent information held in low-memory on the MacOS: */

struct FSVarsRec	*gFSMVars;

/*
 * Global variables defined in other modules:
 */
extern struct vnodeopv_desc hfs_vnodeop_opv_desc;

extern struct vnode *hfs_vhashget(dev_t dev, UInt32 nodeID, UInt8 forkType);

extern OSErr HFSPlusToHFSExtents( const HFSPlusExtentRecord	oldExtents, HFSExtentRecord newExtents);


extern void inittodr( time_t base);
extern OSErr GetVolumeNameFromCatalog(ExtendedVCB *vcb);
extern void CopyCatalogToObjectMeta(struct hfsCatalogInfo *catalogInfo, struct vnode *vp, struct hfsfilemeta *fm);
extern void CopyCatalogToFCB(struct hfsCatalogInfo *catalogInfo, struct vnode *vp);

int hfs_changefs(struct mount *mp, struct hfs_mount_args *args, struct proc *p);

int hfs_reload(struct mount *mp, struct ucred *cred, struct proc *p);
int hfs_mountfs(struct vnode *devvp, struct mount *mp, struct proc *p, struct hfs_mount_args *args);
int hfs_vget(struct mount *mp, void *objID, struct vnode **vpp);
void hfs_vhashinit();
void hfs_converterinit(void);


static int hfs_statfs();


/*
 * Called by vfs_mountroot when mounting HFS Plus as root.
 */
int
hfs_mountroot()
{
	extern struct vnode *rootvp;
	struct mount *mp;
	struct proc *p = current_proc();	/* XXX */
	struct hfsmount *hfsmp;
	int error;
	
	/*
	 * Get vnode for rootdev.
	 */
	if ((error = bdevvp(rootdev, &rootvp))) {
		printf("hfs_mountroot: can't setup bdevvp");
		return (error);
	}
	if ((error = vfs_rootmountalloc("hfs", "root_device", &mp)))
		return (error);
	if ((error = hfs_mountfs(rootvp, mp, p, NULL))) {
		mp->mnt_vfc->vfc_refcount--;
		vfs_unbusy(mp, p);
		_FREE_ZONE(mp, sizeof (struct mount), M_MOUNT);
		return (error);
	}
	simple_lock(&mountlist_slock);
	CIRCLEQ_INSERT_TAIL(&mountlist, mp, mnt_list);
	simple_unlock(&mountlist_slock);
	
	/* Init hfsmp */
	hfsmp = VFSTOHFS(mp);

	hfsmp->hfs_dir_mask = (S_IRWXU|S_IRWXG|S_IRWXO);		/* 0777 */
	hfsmp->hfs_file_mask = (S_IRWXU|S_IRWXG|S_IRWXO);		/* 0777 */

	(void)hfs_statfs(mp, &mp->mnt_stat, p);
	
	vfs_unbusy(mp, p);
	inittodr(to_bsd_time(HFSTOVCB(hfsmp)->vcbLsMod));
	return (0);
}


/*
 * VFS Operations.
 *
 * mount system call
 */

int
hfs_mount (mp, path, data, ndp, p)
	register struct mount *mp;
	char *path;
	caddr_t data;
	struct nameidata *ndp;
	struct proc *p;
{
	struct hfsmount *hfsmp = NULL;
	struct vnode *devvp;
	struct hfs_mount_args args;
	size_t size;
	int retval = E_NONE;
	int flags;
	mode_t accessmode;
	int loadconv = 0;

	if ((retval = copyin(data, (caddr_t)&args, sizeof(args))))
		goto error_exit;

	/*
	 * If updating, check whether changing from read-only to
	 * read/write; if there is no device name, that's all we do.
	 */
	if (mp->mnt_flag & MNT_UPDATE) {

		hfsmp = VFSTOHFS(mp);
		if (hfsmp->hfs_fs_ronly == 0 && (mp->mnt_flag & MNT_RDONLY)) {
		
			/* use VFS_SYNC to push out System (btree) files */
			retval = VFS_SYNC(mp, MNT_WAIT, p->p_ucred, p);
			if (retval && (mp->mnt_flag & MNT_FORCE) == 0)
				goto error_exit;
		
			flags = WRITECLOSE;
			if (mp->mnt_flag & MNT_FORCE)
				flags |= FORCECLOSE;
				
			if ((retval = hfs_flushfiles(mp, flags)))
				goto error_exit;
			hfsmp->hfs_fs_clean = 1;
			hfsmp->hfs_fs_ronly = 1;
			if (HFSTOVCB(hfsmp)->vcbSigWord == kHFSPlusSigWord)
				retval = hfs_flushvolumeheader(hfsmp, MNT_WAIT);
			else
				retval = hfs_flushMDB(hfsmp, MNT_WAIT);

			/* also get the volume bitmap blocks */
			if (!retval)
				retval = VOP_FSYNC(hfsmp->hfs_devvp, NOCRED, MNT_WAIT, p);

			if (retval) {
				hfsmp->hfs_fs_clean = 0;
				hfsmp->hfs_fs_ronly = 0;
				goto error_exit;
			}
		}

		if ((mp->mnt_flag & MNT_RELOAD) &&
			(retval = hfs_reload(mp, ndp->ni_cnd.cn_cred, p)))
			goto error_exit;

		if (hfsmp->hfs_fs_ronly && (mp->mnt_flag & MNT_WANTRDWR)) {
			/*
			 * If upgrade to read-write by non-root, then verify
			 * that user has necessary permissions on the device.
			 */
			if (p->p_ucred->cr_uid != 0) {
				devvp = hfsmp->hfs_devvp;
				vn_lock(devvp, LK_EXCLUSIVE | LK_RETRY, p);
				if ((retval = VOP_ACCESS(devvp, VREAD | VWRITE, p->p_ucred, p))) {
					VOP_UNLOCK(devvp, 0, p);
					goto error_exit;
				}
				VOP_UNLOCK(devvp, 0, p);
			}
			if (HFSTOVCB(hfsmp)->vcbSigWord == kHFSPlusSigWord)
				retval = hfs_flushvolumeheader(hfsmp, MNT_WAIT);
			else
				retval = hfs_flushMDB(hfsmp, MNT_WAIT);
	
			if (retval != E_NONE)
				goto error_exit;

			/* only change hfs_fs_ronly after a successfull write */
			hfsmp->hfs_fs_ronly = 0;
			hfsmp->hfs_fs_clean = 0;
		}

		if (hfsmp->hfs_fs_ronly == 0) {
			/* setup private/hidden directory for unlinked files */
			hfsmp->hfs_private_metadata_dir = FindMetaDataDirectory(HFSTOVCB(hfsmp));
		}

#if 0   /* XXX PPD */
		if (args.fspec == 0) {
			/*
			 * Process export requests.
			 */
			retval = (vfs_export(mp, &hfsmp->hfs_export, &args.export));
		}
#endif

	}

	/*
	 * Not an update, or updating the name: look up the name
	 * and verify that it refers to a sensible block device.
	 */
	NDINIT(ndp, LOOKUP, FOLLOW, UIO_USERSPACE, args.fspec, p);
	retval = namei(ndp);
	if (retval != E_NONE) {
		DBG_ERR(("hfs_mount: CAN'T GET DEVICE: %s, %x\n", args.fspec, ndp->ni_vp->v_rdev));
		goto error_exit;
	}

	devvp = ndp->ni_vp;

	if (devvp->v_type != VBLK) {
		VRELE(devvp);
		retval = ENOTBLK;
		goto error_exit;
	}
	if (major(devvp->v_rdev) >= nblkdev) {
		VRELE(devvp);
		retval = ENXIO;
		goto error_exit;
	}

	/*
	 * If mount by non-root, then verify that user has necessary
	 * permissions on the device.
	 */
	if (p->p_ucred->cr_uid != 0) {
		accessmode = VREAD;
		if ((mp->mnt_flag & MNT_RDONLY) == 0)
			accessmode |= VWRITE;
		vn_lock(devvp, LK_EXCLUSIVE | LK_RETRY, p);
		if ((retval = VOP_ACCESS(devvp, accessmode, p->p_ucred, p))) {
			VPUT(devvp);
			goto error_exit;
		}
		VOP_UNLOCK(devvp, 0, p);
	}

	if ((mp->mnt_flag & MNT_UPDATE) == 0) {
		retval = hfs_mountfs(devvp, mp, p, &args);
		if (retval != E_NONE)
			VRELE(devvp);
	} else {
		if (devvp != hfsmp->hfs_devvp)
			retval = EINVAL;	/* needs translation */
		else
			retval = hfs_changefs(mp, &args, p);
		VRELE(devvp);
	}

	if (retval != E_NONE) {
		goto error_exit;
	}

	
	/* Set the mount flag to indicate that we support volfs  */
	mp->mnt_flag |= MNT_DOVOLFS;
    if (VFSTOVCB(mp)->vcbSigWord == kHFSSigWord) {
    	/* HFS volumes only want roman-encoded names: */
    	mp->mnt_flag |= MNT_ROMANONLY;
    }
	(void) copyinstr(path, mp->mnt_stat.f_mntonname, MNAMELEN-1, &size);
	bzero(mp->mnt_stat.f_mntonname + size, MNAMELEN - size);
	(void) copyinstr(args.fspec, mp->mnt_stat.f_mntfromname, MNAMELEN - 1, &size);
	bzero(mp->mnt_stat.f_mntfromname + size, MNAMELEN - size);
	(void)hfs_statfs(mp, &mp->mnt_stat, p);
	return (E_NONE);

error_exit:

	return (retval);
}


/* change fs mount parameters */
int
hfs_changefs(mp, args, p)
	struct mount *mp;
	struct hfs_mount_args *args;
	struct proc *p;
{
	int retval;
	int namefix, permfix;
	struct hfsmount *hfsmp;
	struct hfsnode *hp;
	mode_t hfs_file_mask;
	ExtendedVCB *vcb;
	register struct vnode *vp, *nvp;

	namefix = permfix = 0;
	hfsmp = VFSTOHFS(mp);
	vcb = HFSTOVCB(hfsmp);

	/* change the hfs timezone (Note: this affects all hfs volumes) */
	if ((VFSTOVCB(mp)->vcbSigWord == kHFSSigWord)		&&
	    (args->hfs_timezone.tz_minuteswest != VNOVAL)) {
		gTimeZone = args->hfs_timezone;
	}

	/* change the default uid, gid and/or mask */
	if (args->hfs_uid != (uid_t)VNOVAL && hfsmp->hfs_uid != args->hfs_uid) {
		hfsmp->hfs_uid = args->hfs_uid;
		++permfix;
	}
	if (args->hfs_gid != (gid_t)VNOVAL && hfsmp->hfs_gid != args->hfs_gid) {
		hfsmp->hfs_gid = args->hfs_gid;
		++permfix;
	}
	if (args->hfs_mask != (mode_t)VNOVAL) {
		if (hfsmp->hfs_dir_mask != (args->hfs_mask & ALLPERMS)) {
			hfsmp->hfs_dir_mask = args->hfs_mask & ALLPERMS;
			hfsmp->hfs_file_mask = args->hfs_mask & ALLPERMS;
			if ((args->flags != VNOVAL) && (args->flags & HFSFSMNT_NOXONFILES))
				hfsmp->hfs_file_mask = (args->hfs_mask & DEFFILEMODE);
			++permfix;
		}
	}

	/* change the hfs encoding value (hfs only) */
	if ((HFSTOVCB(hfsmp)->vcbSigWord == kHFSSigWord)	&&
	    (hfsmp->hfs_encoding != (u_long)VNOVAL)		&&
	    (hfsmp->hfs_encoding != args->hfs_encoding)) {
		hfs_to_unicode_func_t	get_unicode_func;
		unicode_to_hfs_func_t	get_hfsname_func;

		retval = hfs_getconverter(args->hfs_encoding, &get_unicode_func, &get_hfsname_func);
		if (retval) goto error_exit;

		/* now release the old one */
		(void) hfs_relconverter(hfsmp->hfs_encoding);

		hfsmp->hfs_encoding = args->hfs_encoding;
		hfsmp->hfs_get_unicode = get_unicode_func;
		hfsmp->hfs_get_hfsname = get_hfsname_func;
		++namefix;
	}


	if (!(namefix || permfix))
		goto exit;

	/*
	 * For each active vnode fix things that changed
	 *
	 * Note that we can visit a vnode more than once
	 * and we can race with fsync.
	 */
	simple_lock(&mntvnode_slock);
loop:
	for (vp = mp->mnt_vnodelist.lh_first; vp != NULL; vp = nvp) {
		/*
		 * If the vnode that we are about to fix is no longer
		 * associated with this mount point, start over.
		 */
		if (vp->v_mount != mp)
	            goto loop;
	 
	        simple_lock(&vp->v_interlock);
	        nvp = vp->v_mntvnodes.le_next;
		if (vp->v_flag & VSYSTEM) {
			simple_unlock(&vp->v_interlock);
			continue;
		}
	        simple_unlock(&mntvnode_slock);
	        retval = vget(vp, LK_EXCLUSIVE | LK_NOWAIT | LK_INTERLOCK, p);
	        if (retval) {
	            simple_lock(&mntvnode_slock);
	            if (retval == ENOENT)
	                goto loop;
	            continue;
	        }
	
		hp = VTOH(vp);

		if (permfix && (hp->h_meta->h_metaflags & IN_UNSETACCESS)) {
			hp->h_meta->h_uid = hfsmp->hfs_uid;
			hp->h_meta->h_gid = hfsmp->hfs_gid;
			/* Default access is full read/write/execute: */
			hp->h_meta->h_mode = ACCESSPERMS;	/* 0777: rwxrwxrwx */
		
			/* ... but no more than that permitted by the mount point's: */
			if ((hp->h_meta->h_mode & IFMT) == IFDIR)
				hp->h_meta->h_mode &= hfsmp->hfs_dir_mask;
			else
				hp->h_meta->h_mode &= hfsmp->hfs_file_mask;
		}

		if (namefix) {
			hfsCatalogInfo catInfo;

			/* lookup by fileID since UTF-8 name will change */
			catInfo.hint = kNoHint;
			if (hfsLookup(vcb, H_FILEID(hp), NULL, -1, &catInfo) == 0) {
				H_HINT(hp) = catInfo.hint;
				hfs_set_metaname(catInfo.spec.name, hp->h_meta);
			}
			H_HINT(hp) = catInfo.hint;
			hfs_set_metaname(catInfo.spec.name, hp->h_meta);
		}

        VPUT(vp);
        simple_lock(&mntvnode_slock);

	} /* end for (vp...) */
	simple_unlock(&mntvnode_slock);


exit:	
	return (0);

error_exit:

	return (retval);
}


/*
 * Reload all incore data for a filesystem (used after running fsck on
 * the root filesystem and finding things to fix). The filesystem must
 * be mounted read-only.
 *
 * Things to do to update the mount:
 *	1) invalidate all cached meta-data.
 *	2) re-read volume header from disk.
 *	3) re-load meta-file info (extents, file size).
 *	4) invalidate all inactive vnodes.
 *	5) invalidate all cached file data.
 *	6) re-read hfsnode data for all active vnodes.
 */
int
hfs_reload(mountp, cred, p)
	register struct mount *mountp;
	struct ucred *cred;
	struct proc *p;
{
	register struct vnode *vp, *nvp, *devvp;
	struct hfsnode *hp;
	struct buf *bp;
	int 	size, error;
	struct hfsmount *hfsmp;
	struct HFSPlusVolumeHeader *vhp;
	ExtendedVCB *vcb;
	FCB *fcb;

	if ((mountp->mnt_flag & MNT_RDONLY) == 0)
		return (EINVAL);

    	hfsmp = VFSTOHFS(mountp);
	vcb = HFSTOVCB(hfsmp);

	if (vcb->vcbSigWord == kHFSSigWord)
		return (EINVAL);	/* rooting from HFS is not supported! */

	/*
	 * Step 1: invalidate all cached meta-data.
	 */
	devvp = hfsmp->hfs_devvp;
	if (vinvalbuf(devvp, 0, cred, p, 0, 0))
		panic("hfs_reload: dirty1");
    	InvalidateCatalogCache(vcb);

	/*
	 * Step 2: re-read VolumeHeader from disk.
	 */
	size = kMDBSize;
	error = bread(	hfsmp->hfs_devvp,
			IOBLKNOFORBLK((vcb->hfsPlusIOPosOffset / 512) + kMasterDirectoryBlock, size),
			IOBYTECCNTFORBLK(kMasterDirectoryBlock, kMDBSize, size),
			NOCRED,
			&bp);
	if (error) {
        	if (bp != NULL)
        		brelse(bp);
		return (error);
	}

	vhp = (HFSPlusVolumeHeader *) ((char *)bp->b_data +
			IOBYTEOFFSETFORBLK((vcb->hfsPlusIOPosOffset / 512) + kMasterDirectoryBlock, size));

	if ((ValidVolumeHeader(vhp) != 0) || (vcb->blockSize != vhp->blockSize)) {
		brelse(bp);
		return (EIO);		/* XXX needs translation */
	}
	
	vcb->vcbCrDate = LocalToUTC(vhp->createDate);
	vcb->vcbLsMod =	vhp->modifyDate;
	vcb->vcbAtrb = (UInt16) vhp->attributes;	/* VCB only uses lower 16 bits */
	vcb->vcbClpSiz = vhp->rsrcClumpSize;
	vcb->vcbNxtCNID = vhp->nextCatalogID;
	vcb->vcbVolBkUp = vhp->backupDate;
	vcb->vcbWrCnt =	vhp->writeCount;
	vcb->vcbFilCnt = vhp->fileCount;
	vcb->vcbDirCnt = vhp->folderCount;
	vcb->nextAllocation = vhp->nextAllocation;
	vcb->totalBlocks = vhp->totalBlocks;
	vcb->freeBlocks = vhp->freeBlocks;
	vcb->checkedDate = vhp->checkedDate;
	vcb->encodingsBitmap = vhp->encodingsBitmap;

	bcopy(vhp->finderInfo, vcb->vcbFndrInfo, sizeof(vhp->finderInfo));    

	/*
	 * Step 3: re-load meta-file vnode data (extent info, file size, etc).
	 */
	fcb = VTOFCB((struct vnode *)vcb->extentsRefNum);
	bcopy(vhp->extentsFile.extents, fcb->fcbExtents, sizeof(HFSPlusExtentRecord));
	fcb->fcbEOF = vhp->extentsFile.logicalSize;
	fcb->fcbPLen = vhp->extentsFile.totalBlocks * vcb->blockSize;
	fcb->fcbClmpSize = vhp->extentsFile.clumpSize;

	fcb = VTOFCB((struct vnode *)vcb->catalogRefNum);
	bcopy(vhp->catalogFile.extents, fcb->fcbExtents, sizeof(HFSPlusExtentRecord));
	fcb->fcbPLen = vhp->catalogFile.logicalSize;
	fcb->fcbPLen = vhp->catalogFile.totalBlocks * vcb->blockSize;
	fcb->fcbClmpSize = vhp->catalogFile.clumpSize;

	fcb = VTOFCB((struct vnode *)vcb->allocationsRefNum);
	bcopy(vhp->allocationFile.extents, fcb->fcbExtents, sizeof(HFSPlusExtentRecord));
	fcb->fcbEOF = vhp->allocationFile.logicalSize;
	fcb->fcbPLen = vhp->allocationFile.totalBlocks * vcb->blockSize;
	fcb->fcbClmpSize = vhp->allocationFile.clumpSize;

	/* Now that the catalog vnode is ready, get the volume name */
	if ((error = MacToVFSError( GetVolumeNameFromCatalog(vcb) ))) {	
		brelse(bp);
		return (error);
	}

	/* reestablish private/hidden directory for unlinked files */
	hfsmp->hfs_private_metadata_dir = FindMetaDataDirectory(vcb);

	brelse(bp);

loop:
	simple_lock(&mntvnode_slock);
	for (vp = mountp->mnt_vnodelist.lh_first; vp != NULL; vp = nvp) {
		if (vp->v_mount != mountp) {
			simple_unlock(&mntvnode_slock);
			goto loop;
		}
		nvp = vp->v_mntvnodes.le_next;

		/*
		 * Step 4: invalidate all inactive vnodes.
		 */
		if (vrecycle(vp, &mntvnode_slock, p))
			goto loop;

		/*
		 * Step 5: invalidate all cached file data.
		 */
		simple_lock(&vp->v_interlock);
		simple_unlock(&mntvnode_slock);
		if (vget(vp, LK_EXCLUSIVE | LK_INTERLOCK, p)) {
			goto loop;
		}
		if (vinvalbuf(vp, 0, cred, p, 0, 0))
			panic("hfs_reload: dirty2");

		/*
		 * Step 6: re-read hfsnode data for all active vnodes (non-metadata files).
		 */
		hp = VTOH(vp);
		if ((vp->v_flag & VSYSTEM) == 0) {
			hfsCatalogInfo catInfo;

			/* lookup by fileID since name could have changed */
			catInfo.hint = kNoHint;
			if ((error = hfsLookup(vcb, H_FILEID(hp), NULL, -1, &catInfo))) {
				vput(vp);
				return (error);
			}
			H_HINT(hp) = catInfo.hint;
            if (hp->h_meta->h_metaflags & IN_LONGNAME)
				FREE(H_NAME(hp), M_TEMP);
			H_NAME(hp) = NULL;
			CopyCatalogToObjectMeta(&catInfo, vp, hp->h_meta);
			CopyCatalogToFCB(&catInfo, vp);
		}

		vput(vp);
		simple_lock(&mntvnode_slock);
	}
	simple_unlock(&mntvnode_slock);

	return (0);
}


/*
 * Common code for mount and mountroot
 */
int
hfs_mountfs(struct vnode *devvp, struct mount *mp, struct proc *p, struct hfs_mount_args *args)
{
    int                         retval = E_NONE;
    register struct hfsmount	*hfsmp;
    struct buf					*bp;
    dev_t                       dev;
    HFSMasterDirectoryBlock		*mdbp;
    int                         ronly;
    struct ucred				*cred;
	u_long diskBlks;
	u_long blksize;
    DBG_VFS(("hfs_mountfs: mp = 0x%lX\n", (u_long)mp));

    dev = devvp->v_rdev;
    cred = p ? p->p_ucred : NOCRED;
    /*
     * Disallow multiple mounts of the same device.
     * Disallow mounting of a device that is currently in use
     * (except for root, which might share swap device for miniroot).
     * Flush out any old buffers remaining from a previous use.
     */
    if ((retval = vfs_mountedon(devvp)))
        return (retval);
    if (vcount(devvp) > 1 && devvp != rootvp)
                return (EBUSY);
    if ((retval = vinvalbuf(devvp, V_SAVE, cred, p, 0, 0)))
        return (retval);

    ronly = (mp->mnt_flag & MNT_RDONLY) != 0;
    DBG_VFS(("hfs_mountfs: opening device...\n"));
    if ((retval = VOP_OPEN(devvp, ronly ? FREAD : FREAD|FWRITE, FSCRED, p)))
        return (retval);

    blksize = kHFSBlockSize;
    DBG_VFS(("hfs_mountfs: size = %d (DEV_BSIZE = %d).\n", blksize, DEV_BSIZE));

    bp = NULL;
    hfsmp = NULL;

	/*	
	 * XXX SER Currently we only support 512 block size systems. This might change
	 * So this is a place holder to remind us that the mdb might not be 512 aligned
	 * retval = VOP_IOCTL(devvp, DKIOCGETBLOCKSIZE, &blksize, FWRITE, cred, p);
	 * if (retval) return retval;
	 */

	retval = VOP_IOCTL(devvp, DKIOCSETBLOCKSIZE, &blksize, FWRITE, cred, p);
	if (retval) return retval;


    DBG_VFS(("hfs_mountfs: reading MDB [block no. %d + %d bytes, size %d bytes]...\n",
             IOBLKNOFORBLK(kMasterDirectoryBlock, blksize),
             IOBYTEOFFSETFORBLK(kMasterDirectoryBlock, blksize),
             IOBYTECCNTFORBLK(kMasterDirectoryBlock, kMDBSize, blksize)));

    if ((retval = bread(devvp, IOBLKNOFORBLK(kMasterDirectoryBlock, blksize),
    						   IOBYTECCNTFORBLK(kMasterDirectoryBlock, kMDBSize, blksize), cred, &bp))) {
        goto error_exit;
	};
    mdbp = (HFSMasterDirectoryBlock*) ((char *)bp->b_data + IOBYTEOFFSETFORBLK(kMasterDirectoryBlock, blksize));

    MALLOC(hfsmp, struct hfsmount *, sizeof(struct hfsmount), M_HFSMNT, M_WAITOK);
    bzero(hfsmp, sizeof(struct hfsmount));

    DBG_VFS(("hfs_mountfs: Initializing hfsmount structure at 0x%lX...\n", (u_long)hfsmp));
    /*
     *  Init the volume information structure
     */
    mp->mnt_data = (qaddr_t)hfsmp;
    hfsmp->hfs_mp = mp;						/* Make VFSTOHFS work */
    hfsmp->hfs_vcb.vcb_hfsmp = hfsmp;		/* Make VCBTOHFS work */
    hfsmp->hfs_raw_dev = devvp->v_rdev;
    hfsmp->hfs_devvp = devvp;
    hfsmp->hfs_phys_block_size = blksize;
    /* The hfs_log_block_size field is updated in the respective hfs_MountHFS[Plus]Volume routine */
    hfsmp->hfs_logBlockSize = BestBlockSizeFit(mdbp->drAlBlkSiz, MAXBSIZE, hfsmp->hfs_phys_block_size);
    hfsmp->hfs_fs_ronly = ronly;
	if (args) {
		hfsmp->hfs_uid = args->hfs_uid;
		hfsmp->hfs_gid = args->hfs_gid;
		hfsmp->hfs_dir_mask = args->hfs_mask & ALLPERMS;
		if (args->flags & HFSFSMNT_NOXONFILES)
			hfsmp->hfs_file_mask = (args->hfs_mask & DEFFILEMODE);
		else
			hfsmp->hfs_file_mask = args->hfs_mask & ALLPERMS;
	}
	
	/*	See above comment for DKIOCGETBLOCKSIZE
	 * retval = VOP_IOCTL(devvp, DKIOCSETBLOCKSIZE, &blksize, FWRITE, cred, p);
	 * if (retval) return retval;
	 */

	retval = VOP_IOCTL(devvp, DKIOCNUMBLKS, &diskBlks, 0, cred, p);
	if (retval) return retval;

	if (mdbp->drSigWord == kHFSPlusSigWord) {
		/* mount wrapper-less HFS-Plus volume */
		(void) hfs_getconverter(0, &hfsmp->hfs_get_unicode, &hfsmp->hfs_get_hfsname);
		retval = hfs_MountHFSPlusVolume(hfsmp, (HFSPlusVolumeHeader*) bp->b_data, 0, diskBlks, p);

	} else if (mdbp->drEmbedSigWord == kHFSPlusSigWord) {
		u_long embBlkOffset;
		HFSPlusVolumeHeader	*vhp;

		embBlkOffset = mdbp->drAlBlSt +
		    (mdbp->drEmbedExtent.startBlock * (mdbp->drAlBlkSiz/kHFSBlockSize));
		/* calculate virtual number of 512-byte sectors */
		diskBlks = mdbp->drEmbedExtent.blockCount * (mdbp->drAlBlkSiz/kHFSBlockSize);
		brelse(bp);
		bp = NULL;		/* done with MDB, go grab Volume Header */
		mdbp = NULL;

		retval = bread(	devvp,
				IOBLKNOFORBLK(kMasterDirectoryBlock+embBlkOffset, blksize),
				IOBYTECCNTFORBLK(kMasterDirectoryBlock+embBlkOffset, kMDBSize, blksize),
				cred,
				&bp);
		if (retval) {
			goto error_exit;
		};
		vhp = (HFSPlusVolumeHeader*) ((char *)bp->b_data + IOBYTEOFFSETFORBLK(kMasterDirectoryBlock, blksize));

		/* mount embedded HFS Plus volume */
		(void) hfs_getconverter(0, &hfsmp->hfs_get_unicode, &hfsmp->hfs_get_hfsname);
		retval = hfs_MountHFSPlusVolume(hfsmp, vhp, embBlkOffset, diskBlks, p);

	} else if (devvp != rootvp) {
		if (args) {
			hfsmp->hfs_encoding = args->hfs_encoding;

			/* establish the zimezone (not used by HFS Plus) */
			gTimeZone = args->hfs_timezone;
		}

		retval = hfs_getconverter(hfsmp->hfs_encoding, &hfsmp->hfs_get_unicode, &hfsmp->hfs_get_hfsname);
		if (retval) goto error_exit;

		/* mount HFS volume */
		retval = hfs_MountHFSVolume( hfsmp, mdbp, diskBlks, p);
		
		if (retval)
			(void) hfs_relconverter(hfsmp->hfs_encoding);

	} else {
		/* sorry, we cannot root from HFS */
		retval = EINVAL;
	}

	if ( retval ) {
		goto error_exit;
	}

    brelse(bp);
    bp = NULL;
	
    mp->mnt_stat.f_fsid.val[0] = (long)dev;
    mp->mnt_stat.f_fsid.val[1] = mp->mnt_vfc->vfc_typenum;
    mp->mnt_maxsymlinklen = 0;
    devvp->v_specflags |= SI_MOUNTEDON;

    if (ronly == 0) {
        hfsmp->hfs_fs_clean = 0;
        if (HFSTOVCB(hfsmp)->vcbSigWord == kHFSPlusSigWord)
        	(void) hfs_flushvolumeheader(hfsmp, MNT_WAIT);
        else
        	(void) hfs_flushMDB(hfsmp, MNT_WAIT);
    }
    goto std_exit;

error_exit:
        DBG_VFS(("hfs_mountfs: exiting with error %d...\n", retval));

    if (bp)
        brelse(bp);
    (void)VOP_CLOSE(devvp, ronly ? FREAD : FREAD|FWRITE, cred, p);
    if (hfsmp) {
        FREE(hfsmp, M_HFSMNT);
        mp->mnt_data = (qaddr_t)0;
    }

std_exit:
        return (retval);
}


/*
 * Make a filesystem operational.
 * Nothing to do at the moment.
 */
/* ARGSUSED */
int hfs_start(mp, flags, p)
struct mount *mp;
int flags;
struct proc *p;
{
    DBG_FUNC_NAME("hfs_start");
    DBG_PRINT_FUNC_NAME();

    return (0);
}


/*
 * unmount system call
 */
int
hfs_unmount(mp, mntflags, p)
	struct mount *mp;
	int mntflags;
	struct proc *p;
{
	struct hfsmount *hfsmp = VFSTOHFS(mp);
	int retval = E_NONE;
	int flags;

	flags = 0;
	if (mntflags & MNT_FORCE)
		flags |= FORCECLOSE;

	if ((retval = hfs_flushfiles(mp, flags)))
 		return (retval);

	/*
	 * Flush out the volume bitmap and MDB/Volume Header
	 */
	if (hfsmp->hfs_fs_ronly == 0) {
		if (retval = VOP_FSYNC(hfsmp->hfs_devvp, NOCRED, MNT_WAIT, p)) {
			if ((mntflags & MNT_FORCE) == 0)
				return (retval);
		}
		
		/* See if this volume is damaged, is so do not unmount cleanly */
		if (HFSTOVCB(hfsmp)->vcbFlags & kHFS_DamagedVolume) {
			hfsmp->hfs_fs_clean = 0;
			HFSTOVCB(hfsmp)->vcbAtrb &= ~kHFSVolumeUnmountedMask;
		} else {
		hfsmp->hfs_fs_clean = 1;
		HFSTOVCB(hfsmp)->vcbAtrb |= kHFSVolumeUnmountedMask;
		}
		if (HFSTOVCB(hfsmp)->vcbSigWord == kHFSPlusSigWord)
			retval = hfs_flushvolumeheader(hfsmp, MNT_WAIT);
		else
        		retval = hfs_flushMDB(hfsmp, MNT_WAIT);
       
		if (retval) {
			hfsmp->hfs_fs_clean = 0;
			HFSTOVCB(hfsmp)->vcbAtrb &= ~kHFSVolumeUnmountedMask;
			if ((mntflags & MNT_FORCE) == 0)
				return (retval);	/* could not flush everything */
		}
	}

	/*
	 *	Invalidate our caches and release metadata vnodes
	 */
	(void) hfsUnmount(hfsmp, p);

	if (HFSTOVCB(hfsmp)->vcbSigWord == kHFSSigWord)
		(void) hfs_relconverter(hfsmp->hfs_encoding);

	hfsmp->hfs_devvp->v_specflags &= ~SI_MOUNTEDON;
	retval = VOP_CLOSE(hfsmp->hfs_devvp, hfsmp->hfs_fs_ronly ? FREAD : FREAD|FWRITE,
			NOCRED, p);
	VRELE(hfsmp->hfs_devvp);

	FREE(hfsmp, M_HFSMNT);
	mp->mnt_data = (qaddr_t)0;

	return (retval);
}


/*
 * Return the root of a filesystem.
 *
 *              OUT - vpp, should be locked and vget()'d (to increment usecount and lock)
 */
int hfs_root(mp, vpp)
struct mount *mp;
struct vnode **vpp;
{
    struct vnode *nvp;
    int retval;
    UInt32 rootObjID = kRootDirID;

    DBG_FUNC_NAME("hfs_root");
    DBG_PRINT_FUNC_NAME();

    if ((retval = VFS_VGET(mp, &rootObjID, &nvp)))
        return (retval);

    *vpp = nvp;
    return (0);
}

/*
 * Do operations associated with quotas
 */
int hfs_quotactl(mp, cmds, uid, arg, p)
struct mount *mp;
int cmds;
uid_t uid;
caddr_t arg;
struct proc *p;
{
    DBG_FUNC_NAME("hfs_quotactl");
    DBG_PRINT_FUNC_NAME();

    return (EOPNOTSUPP);
}



/*
 * Get file system statistics.
 */
static int
hfs_statfs(mp, sbp, p)
	struct mount *mp;
	register struct statfs *sbp;
	struct proc *p;
{
	ExtendedVCB *vcb = VFSTOVCB(mp);
	struct hfsmount *hfsmp = VFSTOHFS(mp);
	u_long freeCNIDs;

	DBG_FUNC_NAME("hfs_statfs");
	DBG_PRINT_FUNC_NAME();

	freeCNIDs = (u_long)0xFFFFFFFF - (u_long)vcb->vcbNxtCNID;

	sbp->f_bsize = vcb->blockSize;
	sbp->f_iosize = hfsmp->hfs_logBlockSize;
	sbp->f_blocks = vcb->totalBlocks;
	sbp->f_bfree = vcb->freeBlocks;
	sbp->f_bavail = vcb->freeBlocks;
	sbp->f_files = vcb->totalBlocks - 2;  /* max files is constrained by total blocks */
	sbp->f_ffree = MIN(freeCNIDs, vcb->freeBlocks);
	
	sbp->f_type = 0;
	if (sbp != &mp->mnt_stat) {
		sbp->f_type = mp->mnt_vfc->vfc_typenum;
		bcopy((caddr_t)mp->mnt_stat.f_mntonname,
			(caddr_t)&sbp->f_mntonname[0], MNAMELEN);
		bcopy((caddr_t)mp->mnt_stat.f_mntfromname,
			(caddr_t)&sbp->f_mntfromname[0], MNAMELEN);
	}
	return (0);
}


/*
 * Go through the disk queues to initiate sandbagged IO;
 * go through the inodes to write those that have been modified;
 * initiate the writing of the super block if it has been modified.
 *
 * Note: we are always called with the filesystem marked `MPBUSY'.
 */
static int hfs_sync(mp, waitfor, cred, p)
struct mount *mp;
int waitfor;
struct ucred *cred;
struct proc *p;
{
    struct vnode 		*nvp, *vp;
    struct hfsnode 		*hp;
    struct hfsmount		*hfsmp = VFSTOHFS(mp);
    ExtendedVCB			*vcb;
    int error, allerror = 0;

    DBG_FUNC_NAME("hfs_sync");
    DBG_PRINT_FUNC_NAME();

    hfsmp = VFSTOHFS(mp);
    if (hfsmp->hfs_fs_ronly != 0) {
        panic("update: rofs mod");
    };

    /*
     * Write back each 'modified' vnode
     */
    simple_lock(&mntvnode_slock);

loop:;

    for (vp = mp->mnt_vnodelist.lh_first;
         vp != NULL;
         vp = nvp) {
        /*
         * If the vnode that we are about to sync is no longer
         * associated with this mount point, start over.
         */
        if (vp->v_mount != mp)
            goto loop;
        simple_lock(&vp->v_interlock);
        nvp = vp->v_mntvnodes.le_next;
        hp = VTOH(vp);

        if ((vp->v_type == VNON) || (((hp->h_nodeflags & (IN_ACCESS | IN_CHANGE | IN_MODIFIED | IN_UPDATE)) == 0) &&
            (vp->v_dirtyblkhd.lh_first == NULL))) {
            simple_unlock(&vp->v_interlock);
            continue;
        }

        simple_unlock(&mntvnode_slock);
        error = vget(vp, LK_EXCLUSIVE | LK_NOWAIT | LK_INTERLOCK, p);
        if (error) {
            simple_lock(&mntvnode_slock);
            if (error == ENOENT)
                goto loop;
            continue;
        }
		
        if ((error = VOP_FSYNC(vp, cred, waitfor, p))) {
            DBG_ERR(("hfs_sync: error %d calling fsync on vnode 0x%X.\n", error, (u_int)vp));
            allerror = error;
        };
        DBG_ASSERT(*((volatile int *)(&(vp)->v_interlock))==0);
        VPUT(vp);
        simple_lock(&mntvnode_slock);
    };

	vcb = HFSTOVCB(hfsmp);

    /* Now reprocess the BTree node, stored above */
    {
    struct vnode 		*btvp;
        /*
         * If the vnode that we are about to sync is no longer
         * associated with this mount point, start over.
         */
        btvp = vcb->extentsRefNum;
        if ((btvp==0) || (btvp->v_type == VNON) || (btvp->v_mount != mp))
            goto skipBtree;
        simple_lock(&btvp->v_interlock);
        hp = VTOH(btvp);
        if ((hp->h_nodeflags & (IN_ACCESS | IN_CHANGE | IN_MODIFIED | IN_UPDATE)) == 0 &&
            btvp->v_dirtyblkhd.lh_first == NULL) {
            simple_unlock(&btvp->v_interlock);
            goto skipBtree;
        }
        simple_unlock(&mntvnode_slock);
        error = vget(btvp, LK_EXCLUSIVE | LK_NOWAIT | LK_INTERLOCK, p);
        if (error) {
            simple_lock(&mntvnode_slock);
            goto skipBtree;
        }
        if ((error = VOP_FSYNC(btvp, cred, waitfor, p)))
            allerror = error;
        VOP_UNLOCK(btvp, 0, p);
        VRELE(btvp);
        simple_lock(&mntvnode_slock);
    };

skipBtree:;

    simple_unlock(&mntvnode_slock);

    /*
     * Force stale file system control information to be flushed.
     */
    if ((error = VOP_FSYNC(hfsmp->hfs_devvp, cred, waitfor, p)))
        allerror = error;
    /*
     * Write back modified superblock.
     */

    if (IsVCBDirty(vcb)) {
    	if (vcb->vcbSigWord == kHFSPlusSigWord)
    		error = hfs_flushvolumeheader(hfsmp, waitfor);
    	else
    		error = hfs_flushMDB(hfsmp, waitfor);
    	
        if (error)
            allerror = error;
    };

    return (allerror);
}


/*
 * File handle to vnode
 *
 * Have to be really careful about stale file handles:
 * - check that the hfsnode number is valid
 * - call hfs_vget() to get the locked hfsnode
 * - check for an unallocated hfsnode (i_mode == 0)
 * - check that the given client host has export rights and return
 *   those rights via. exflagsp and credanonp
 */
int
hfs_fhtovp(mp, fhp, nam, vpp, exflagsp, credanonp)
register struct mount *mp;
struct fid *fhp;
struct mbuf *nam;
struct vnode **vpp;
int *exflagsp;
struct ucred **credanonp;
{
    DBG_FUNC_NAME("hfs_fhtovp");
    DBG_PRINT_FUNC_NAME();

    return (EOPNOTSUPP);
}

/*
 * Vnode pointer to File handle
 */
/* ARGSUSED */
static int hfs_vptofh(vp, fhp)
struct vnode *vp;
struct fid *fhp;
{
    DBG_FUNC_NAME("hfs_vptofh");
    DBG_PRINT_FUNC_NAME();

    return (EOPNOTSUPP);
}



/*
 * Initial HFS filesystems, done only once.
 */
int
hfs_init(vfsp)
struct vfsconf *vfsp;
{
    int i;
    static int done = 0;
    OSErr err;

    DBG_FUNC_NAME("hfs_init");
    DBG_PRINT_FUNC_NAME();

    if (done)
        return (0);
    done = 1;
    hfs_vhashinit();
    hfs_converterinit();

    simple_lock_init (&gBufferPtrListLock);

    for (i = BUFFERPTRLISTSIZE - 1; i >= 0; --i) {
        gBufferAddress[i] = NULL;
        gBufferHeaderPtr[i] = NULL;
    };
    gBufferListIndex = 0;

    /*
     * Do any initialization that the MacOS/MacOS X shared code relies on
     * (normally done as part of MacOS's startup):
     */
    MALLOC(gFSMVars, FSVarsRec *, sizeof(FSVarsRec), M_TEMP, M_WAITOK);
    bzero(gFSMVars, sizeof(FSVarsRec));

	/*
	 * Allocate Catalog Iterator cache...
	 */
	err = InitCatalogCache();
#if HFS_DIAGNOSTIC
    if (err) panic("hfs_init: Error returned from InitCatalogCache() call.");
#endif
	/*
	 * XXX do we need to setup the following?
	 *
	 * GMT offset, Unicode globals, CatSearch Buffers, BTSscanner
	 */

    return E_NONE;
}

/*
 * fast filesystem related variables.
 */
static int hfs_sysctl(name, namelen, oldp, oldlenp, newp, newlen, p)
int *name;
u_int namelen;
void *oldp;
size_t *oldlenp;
void *newp;
size_t newlen;
struct proc *p;
{
    DBG_FUNC_NAME("hfs_sysctl");
    DBG_PRINT_FUNC_NAME();

    return (EOPNOTSUPP);
}

/*	This will return a vnode of either a directory or a data vnode based on an object id. If
 *  it is a file id, its data fork will be returned.
 */

int hfs_vget(struct mount *mp,
             void *ino,
             struct vnode **vpp)
{
    struct hfsmount 	*hfsmp;
    dev_t 				dev;
    hfsCatalogInfo 		catInfo;
    int 				retval = E_NONE;

    DBG_VFS(("hfs_vget: ino = %ld\n", *(UInt32 *)ino));

	/* Check if unmount in progress */
	if (mp->mnt_flag & MNT_UNMOUNT) {
		return (EPERM);
	}

    hfsmp = VFSTOHFS(mp);
    dev = hfsmp->hfs_raw_dev;
	
	/* First check to see if it is in the cache */
    *vpp = hfs_vhashget(dev, *(UInt32 *)ino, kDefault);

	/* hide open files that have been deleted */
	if (*vpp != NULL && H_DIRID(VTOH(*vpp)) == hfsmp->hfs_private_metadata_dir) {
        vput(*vpp);
		retval = ENOENT;
		goto Err_Exit;
	}
	
	/* The vnode is not in the cache, so lets make it */
    if (*vpp == NULL)
      {
        struct proc			*p = current_proc();
        UInt8				forkType;

		/* Special-case the root's parent directory (DirID = 1) because
		   it doesn't actually exist in the catalog: */
		if ((*vpp == NULL) && (*(UInt32 *)ino == kRootParID)) {
			bzero(&catInfo, sizeof(catInfo));
			catInfo.nodeData.cnd_type = kCatalogFolderNode;
			catInfo.nodeData.cnd_nodeID = kRootParID;
			catInfo.nodeData.cnd_valence = 1;
			catInfo.nodeData.cnd_ownerID = 0;
			catInfo.nodeData.cnd_groupID = 0;
			catInfo.nodeData.cnd_permissions = (S_IFDIR | S_IRWXU | S_IRWXG | S_IRWXO);
            } else {
            /* lock catalog b-tree */
            retval = hfs_metafilelocking(hfsmp, kHFSCatalogFileID, LK_SHARED, p);
            if (retval != E_NONE) goto Err_Exit;

			catInfo.hint = kNoHint;
            retval = hfsLookup(VFSTOVCB(mp), *(UInt32 *)ino, NULL, -1, &catInfo);

            /* unlock catalog b-tree */
            (void) hfs_metafilelocking(hfsmp, kHFSCatalogFileID, LK_RELEASE, p);

            if (retval != E_NONE) goto Err_Exit;

            /* hide open files that have been deleted */
            if (catInfo.spec.parID == hfsmp->hfs_private_metadata_dir) {
                retval = ENOENT;
                goto Err_Exit;
                };
            };
        forkType = (catInfo.nodeData.cnd_type == kCatalogFolderNode) ? kDirectory : kDataFork;
        retval = hfs_vcreate(VFSTOVCB(mp), &catInfo, forkType, vpp);
      };

#if MACH_NBC
    if (*vpp && ((*vpp)->v_type == VREG)  && !((*vpp)->v_vm_info)){
        vm_info_init(*vpp);
    }
#endif /* MACH_NBC */

Err_Exit:

	if (retval != E_NONE) {
        DBG_VFS(("hfs_vget: Error returned of %d\n", retval));
		}
	else {
        DBG_VFS(("hfs_vget: vp = 0x%x\n", (u_int)*vpp));
		}

    return (retval);

}

/*
 * Flush out all the files in a filesystem.
 */
int
hfs_flushfiles(struct mount *mp, int flags)
{
	int error;

	error = vflush(mp, NULLVP, SKIPSYSTEM | flags);

	return (error);
}

short hfs_flushMDB(struct hfsmount *hfsmp, int waitfor)
{
	ExtendedVCB 			*vcb = HFSTOVCB(hfsmp);
	FCB						*fcb;
	HFSMasterDirectoryBlock	*mdb;
	struct buf 				*bp;
	int						retval;
	int                     size = kMDBSize;	/* 512 */
	size_t					namelen;

	if (vcb->vcbSigWord != kHFSSigWord)
		return EINVAL;

    DBG_ASSERT(hfsmp->hfs_devvp != NULL);

	retval = bread(hfsmp->hfs_devvp, IOBLKNOFORBLK(kMasterDirectoryBlock, size),
					IOBYTECCNTFORBLK(kMasterDirectoryBlock, kMDBSize, size), NOCRED, &bp);
	if (retval) {
	    DBG_VFS((" hfs_flushMDB bread return error! (%d)\n", retval));
		if (bp) brelse(bp);
		return retval;
	}

    DBG_ASSERT(bp != NULL);
    DBG_ASSERT(bp->b_data != NULL);
    DBG_ASSERT(bp->b_bcount == size);

	mdb = (HFSMasterDirectoryBlock *)((char *)bp->b_data + IOBYTEOFFSETFORBLK(kMasterDirectoryBlock, size));

	VCB_LOCK(vcb);
	mdb->drCrDate	= UTCToLocal(vcb->vcbCrDate);
	mdb->drLsMod	= UTCToLocal(vcb->vcbLsMod);
	mdb->drAtrb		= vcb->vcbAtrb;
	mdb->drNmFls	= vcb->vcbNmFls;
	mdb->drAllocPtr	= vcb->nextAllocation;
	mdb->drClpSiz	= vcb->vcbClpSiz;
	mdb->drNxtCNID	= vcb->vcbNxtCNID;
	mdb->drFreeBks	= vcb->freeBlocks;

	/* XXX need to do a real UTF-8 conversion here... */
	copystr(vcb->vcbVN, &mdb->drVN[1], sizeof(mdb->drVN)-1, &namelen);
	mdb->drVN[0] = namelen-1;
	mdb->drVN[namelen] = '\0';
	
	mdb->drVolBkUp	= UTCToLocal(vcb->vcbVolBkUp);
	mdb->drVSeqNum	= vcb->vcbVSeqNum;
	mdb->drWrCnt	= vcb->vcbWrCnt;
	mdb->drNmRtDirs	= vcb->vcbNmRtDirs;
	mdb->drFilCnt	= vcb->vcbFilCnt;
	mdb->drDirCnt	= vcb->vcbDirCnt;
	
	bcopy(vcb->vcbFndrInfo, mdb->drFndrInfo, sizeof(mdb->drFndrInfo));

	fcb = VTOFCB(vcb->extentsRefNum);
	HFSPlusToHFSExtents(fcb->fcbExtents, mdb->drXTExtRec);
	mdb->drXTFlSize	= fcb->fcbPLen;
	mdb->drXTClpSiz	= fcb->fcbClmpSize;
	
	fcb = VTOFCB(vcb->catalogRefNum);
	HFSPlusToHFSExtents(fcb->fcbExtents, mdb->drCTExtRec);
	mdb->drCTFlSize	= fcb->fcbPLen;
	mdb->drCTClpSiz	= fcb->fcbClmpSize;
	VCB_UNLOCK(vcb);


    if (waitfor != MNT_WAIT)
		bawrite(bp);
    else 
		retval = bwrite(bp);
 
	MarkVCBClean( vcb );

	return (retval);
}


short hfs_flushvolumeheader(struct hfsmount *hfsmp, int waitfor)
{
    ExtendedVCB 			*vcb = HFSTOVCB(hfsmp);
    FCB						*fcb;
    HFSPlusVolumeHeader		*volumeHeader;
    int						retval;
    int                     size = sizeof(HFSPlusVolumeHeader);
    struct buf 				*bp;

	if (vcb->vcbSigWord != kHFSPlusSigWord)
		return EINVAL;

	retval = bread(hfsmp->hfs_devvp, IOBLKNOFORBLK((vcb->hfsPlusIOPosOffset / 512) + kMasterDirectoryBlock, size),
					IOBYTECCNTFORBLK(kMasterDirectoryBlock, kMDBSize, size), NOCRED, &bp);
	if (retval) {
	    DBG_VFS((" hfs_flushvolumeheader bread return error! (%d)\n", retval));
		if (bp) brelse(bp);
		return retval;
	}

    DBG_ASSERT(bp != NULL);
    DBG_ASSERT(bp->b_data != NULL);
    DBG_ASSERT(bp->b_bcount == size);

	volumeHeader = (HFSPlusVolumeHeader *)((char *)bp->b_data +
					IOBYTEOFFSETFORBLK((vcb->hfsPlusIOPosOffset / 512) + kMasterDirectoryBlock, size));

	/*
	 * For embedded HFS+ volumes, update create date if neccessary
	 */
	if (vcb->hfsPlusIOPosOffset != 0 && volumeHeader->createDate != UTCToLocal(vcb->vcbCrDate))
	  {
		struct buf 				*bp2;
		HFSMasterDirectoryBlock	*mdb;

		retval = bread(hfsmp->hfs_devvp, IOBLKNOFORBLK(kMasterDirectoryBlock, kMDBSize),
						IOBYTECCNTFORBLK(kMasterDirectoryBlock, kMDBSize, kMDBSize), NOCRED, &bp2);
		if (retval != E_NONE) {
			if (bp2) brelse(bp2);
		} else {
			mdb = (HFSMasterDirectoryBlock *)((char *)bp2->b_data + IOBYTEOFFSETFORBLK(kMasterDirectoryBlock, kMDBSize));
			if ( mdb->drCrDate != UTCToLocal(vcb->vcbCrDate) )
			  {
				mdb->drCrDate = UTCToLocal(vcb->vcbCrDate);		/* pick up the new create date */
				(void) bwrite(bp2);					/* write out the changes */
			  }
			else
			  {
				brelse(bp2);						/* just release it */
			  }
		  }	
	  }

	VCB_LOCK(vcb);
	/* Note: only update the lower 16 bits worth of attributes */
	volumeHeader->attributes		 =	(volumeHeader->attributes & 0xFFFF0000) + (UInt16) vcb->vcbAtrb;
	volumeHeader->lastMountedVersion = kHFSPlusMountVersion;
	volumeHeader->createDate = UTCToLocal(vcb->vcbCrDate);
	volumeHeader->modifyDate = vcb->vcbLsMod;
	volumeHeader->backupDate = vcb->vcbVolBkUp;
	volumeHeader->checkedDate = vcb->checkedDate;
	volumeHeader->fileCount			 =	vcb->vcbFilCnt;
	volumeHeader->folderCount		 =	vcb->vcbDirCnt;
	volumeHeader->freeBlocks		 =	vcb->freeBlocks;
	volumeHeader->nextAllocation	 =	vcb->nextAllocation;
	volumeHeader->rsrcClumpSize		 =	vcb->vcbClpSiz;
	volumeHeader->dataClumpSize		 =	vcb->vcbClpSiz;
	volumeHeader->nextCatalogID		 =	vcb->vcbNxtCNID;
	volumeHeader->writeCount		 =	vcb->vcbWrCnt;
	volumeHeader->encodingsBitmap	 =	vcb->encodingsBitmap;

	bcopy( vcb->vcbFndrInfo, volumeHeader->finderInfo, sizeof(volumeHeader->finderInfo) );

	VCB_UNLOCK(vcb);

	fcb = VTOFCB(vcb->extentsRefNum);
	bcopy( fcb->fcbExtents, volumeHeader->extentsFile.extents, sizeof(HFSPlusExtentRecord) );
	fcb->fcbFlags &= ~fcbModifiedMask;
	volumeHeader->extentsFile.logicalSize = fcb->fcbEOF;
	volumeHeader->extentsFile.totalBlocks = fcb->fcbPLen / vcb->blockSize;
	volumeHeader->extentsFile.clumpSize = fcb->fcbClmpSize;

	fcb = VTOFCB(vcb->catalogRefNum);
	bcopy( fcb->fcbExtents, volumeHeader->catalogFile.extents, sizeof(HFSPlusExtentRecord) );
	fcb->fcbFlags &= ~fcbModifiedMask;
	volumeHeader->catalogFile.logicalSize = fcb->fcbEOF;
	volumeHeader->catalogFile.totalBlocks = fcb->fcbPLen / vcb->blockSize;
	volumeHeader->catalogFile.clumpSize = fcb->fcbClmpSize;

	fcb = VTOFCB(vcb->allocationsRefNum);
	bcopy( fcb->fcbExtents, volumeHeader->allocationFile.extents, sizeof(HFSPlusExtentRecord) );
	fcb->fcbFlags &= ~fcbModifiedMask;
	volumeHeader->allocationFile.logicalSize = fcb->fcbEOF;
	volumeHeader->allocationFile.totalBlocks = fcb->fcbPLen / vcb->blockSize;
	volumeHeader->allocationFile.clumpSize = fcb->fcbClmpSize;

    if (waitfor != MNT_WAIT)
        bawrite(bp);
    else 
		retval = bwrite(bp);
 
	MarkVCBClean( vcb );

	return (retval);
}


/*
 *      Moved here to avoid having to define prototypes
 */

/*
 * hfs vfs operations.
 */
struct vfsops hfs_vfsops = {
    hfs_mount,
    hfs_start,
    hfs_unmount,
    hfs_root,
    hfs_quotactl,
    hfs_statfs,
    hfs_sync,
    hfs_vget,
    hfs_fhtovp,
    hfs_vptofh,
    hfs_init,
    hfs_sysctl
};
