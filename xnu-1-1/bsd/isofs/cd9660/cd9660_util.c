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
/*	$NetBSD: cd9660_util.c,v 1.8 1994/12/13 22:33:25 mycroft Exp $	*/

/*-
 * Copyright (c) 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley
 * by Pace Willisson (pace@blitz.com).  The Rock Ridge Extension
 * Support code is derived from software contributed to Berkeley
 * by Atsushi Murai (amurai@spec.co.jp).
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
 *	@(#)cd9660_util.c	8.3 (Berkeley) 12/5/94
 *
 * HISTORY
 *  7-Dec-98	Add ATTR_VOL_MOUNTFLAGS attribute support - djb
 * 18-Nov-98	Add support for volfs - djb
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/resourcevar.h>
#include <sys/kernel.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/conf.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <miscfs/specfs/specdev.h> /* XXX */
#include <miscfs/fifofs/fifo.h> /* XXX */
#include <sys/malloc.h>
#include <sys/dir.h>
#include <sys/attr.h>
#include <kern/assert.h>

#include <isofs/cd9660/iso.h>
#include <isofs/cd9660/cd9660_node.h>
#include <isofs/cd9660/iso_rrip.h>

/*
 * translate and compare a filename
 * Note: Version number plus ';' may be omitted.
 */
int
isofncmp(fn, fnlen, isofn, isolen)
	u_char *fn, *isofn;
	int fnlen, isolen;
{
	int i, j;
	char c;
	
	while (--fnlen >= 0) {
		if (--isolen < 0)
			return *fn;
		if ((c = *isofn++) == ';') {
			switch (*fn++) {
			default:
				return *--fn;
			case 0:
				return 0;
			case ';':
				break;
			}
			for (i = 0; --fnlen >= 0; i = i * 10 + *fn++ - '0') {
				if (*fn < '0' || *fn > '9') {
					return -1;
				}
			}
			for (j = 0; --isolen >= 0; j = j * 10 + *isofn++ - '0');
			return i - j;
		}
		if (c != *fn) {
			if (c >= 'A' && c <= 'Z') {
				if (c + ('a' - 'A') != *fn) {
					if (*fn >= 'a' && *fn <= 'z')
						return *fn - ('a' - 'A') - c;
					else
						return *fn - c;
				}
			} else
				return *fn - c;
		}
		fn++;
	}
	if (isolen > 0) {
		switch (*isofn) {
		default:
			return -1;
		case '.':
			if (isofn[1] != ';')
				return -1;
		case ';':
			return 0;
		}
	}
	return 0;
}

/*
 * translate a filename
 */
void
isofntrans(infn, infnlen, outfn, outfnlen, original, assoc)
	u_char *infn, *outfn;
	int infnlen;
	u_short *outfnlen;
	int original;
	int assoc;
{
	int fnidx = 0;
	
	if (assoc) {
		*outfn++ = ASSOCCHAR;
		fnidx++;
		infnlen++;
	}
	for (; fnidx < infnlen; fnidx++) {
		char c = *infn++;
		
		if (!original && c >= 'A' && c <= 'Z')
			*outfn++ = c + ('a' - 'A');
		else if (!original && c == '.' && *infn == ';')
			break;
		else if (!original && c == ';')
			break;
		else
			*outfn++ = c;
	}
	*outfnlen = fnidx;
}


int
attrcalcsize(struct attrlist *attrlist)
{
	int size;
	attrgroup_t a;
	
#if ((ATTR_CMN_NAME			| ATTR_CMN_DEVID			| ATTR_CMN_FSID 			| ATTR_CMN_OBJTYPE 		| \
      ATTR_CMN_OBJTAG		| ATTR_CMN_OBJID			| ATTR_CMN_OBJPERMANENTID	| ATTR_CMN_PAROBJID		| \
      ATTR_CMN_SCRIPT		| ATTR_CMN_CRTIME			| ATTR_CMN_MODTIME			| ATTR_CMN_CHGTIME		| \
      ATTR_CMN_ACCTIME		| ATTR_CMN_BKUPTIME			| ATTR_CMN_FNDRINFO			| ATTR_CMN_OWNERID		| \
      ATTR_CMN_GRPID		| ATTR_CMN_ACCESSMASK		| ATTR_CMN_NAMEDATTRCOUNT	| ATTR_CMN_NAMEDATTRLIST| \
      ATTR_CMN_FLAGS) != ATTR_CMN_VALIDMASK)
#warning AttributeBlockSize: Missing bits in common mask computation!
#endif
	assert((attrlist->commonattr & ~ATTR_CMN_VALIDMASK) == 0);

#if ((ATTR_VOL_FSTYPE		| ATTR_VOL_SIGNATURE		| ATTR_VOL_SIZE				| ATTR_VOL_SPACEFREE 	| \
      ATTR_VOL_SPACEAVAIL	| ATTR_VOL_MINALLOCATION	| ATTR_VOL_ALLOCATIONCLUMP	| ATTR_VOL_IOBLOCKSIZE	| \
      ATTR_VOL_OBJCOUNT		| ATTR_VOL_FILECOUNT		| ATTR_VOL_DIRCOUNT			| ATTR_VOL_MAXOBJCOUNT	| \
      ATTR_VOL_MOUNTPOINT	| ATTR_VOL_NAME				| ATTR_VOL_MOUNTFLAGS		| ATTR_VOL_INFO) != ATTR_VOL_VALIDMASK)
#warning AttributeBlockSize: Missing bits in volume mask computation!
#endif
	assert((attrlist->volattr & ~ATTR_VOL_VALIDMASK) == 0);

#if ((ATTR_DIR_LINKCOUNT | ATTR_DIR_ENTRYCOUNT) != ATTR_DIR_VALIDMASK)
#warning AttributeBlockSize: Missing bits in directory mask computation!
#endif
	assert((attrlist->dirattr & ~ATTR_DIR_VALIDMASK) == 0);
#if ((ATTR_FILE_LINKCOUNT	| ATTR_FILE_TOTALSIZE		| ATTR_FILE_ALLOCSIZE 		| ATTR_FILE_IOBLOCKSIZE 	| \
      ATTR_FILE_CLUMPSIZE	| ATTR_FILE_DEVTYPE			| ATTR_FILE_FILETYPE		| ATTR_FILE_FORKCOUNT		| \
      ATTR_FILE_FORKLIST	| ATTR_FILE_DATALENGTH		| ATTR_FILE_DATAALLOCSIZE	| ATTR_FILE_DATAEXTENTS		| \
      ATTR_FILE_RSRCLENGTH	| ATTR_FILE_RSRCALLOCSIZE	| ATTR_FILE_RSRCEXTENTS) != ATTR_FILE_VALIDMASK)
#warning AttributeBlockSize: Missing bits in file mask computation!
#endif
	assert((attrlist->fileattr & ~ATTR_FILE_VALIDMASK) == 0);

#if ((ATTR_FORK_TOTALSIZE | ATTR_FORK_ALLOCSIZE) != ATTR_FORK_VALIDMASK)
#warning AttributeBlockSize: Missing bits in fork mask computation!
#endif
	assert((attrlist->forkattr & ~ATTR_FORK_VALIDMASK) == 0);

	size = 0;
	
	if ((a = attrlist->commonattr) != 0) {
        if (a & ATTR_CMN_NAME) size += sizeof(struct attrreference);
		if (a & ATTR_CMN_DEVID) size += sizeof(dev_t);
		if (a & ATTR_CMN_FSID) size += sizeof(fsid_t);
		if (a & ATTR_CMN_OBJTYPE) size += sizeof(fsobj_type_t);
		if (a & ATTR_CMN_OBJTAG) size += sizeof(fsobj_tag_t);
		if (a & ATTR_CMN_OBJID) size += sizeof(fsobj_id_t);
        if (a & ATTR_CMN_OBJPERMANENTID) size += sizeof(fsobj_id_t);
		if (a & ATTR_CMN_PAROBJID) size += sizeof(fsobj_id_t);
		if (a & ATTR_CMN_SCRIPT) size += sizeof(text_encoding_t);
		if (a & ATTR_CMN_CRTIME) size += sizeof(struct timespec);
		if (a & ATTR_CMN_MODTIME) size += sizeof(struct timespec);
		if (a & ATTR_CMN_CHGTIME) size += sizeof(struct timespec);
		if (a & ATTR_CMN_ACCTIME) size += sizeof(struct timespec);
		if (a & ATTR_CMN_BKUPTIME) size += sizeof(struct timespec);
		if (a & ATTR_CMN_FNDRINFO) size += 32 * sizeof(u_int8_t);
		if (a & ATTR_CMN_OWNERID) size += sizeof(uid_t);
		if (a & ATTR_CMN_GRPID) size += sizeof(gid_t);
		if (a & ATTR_CMN_ACCESSMASK) size += sizeof(u_long);
		if (a & ATTR_CMN_NAMEDATTRCOUNT) size += sizeof(u_long);
		if (a & ATTR_CMN_NAMEDATTRLIST) size += sizeof(struct attrreference);
		if (a & ATTR_CMN_FLAGS) size += sizeof(u_long);
	};
	if ((a = attrlist->volattr) != 0) {
		if (a & ATTR_VOL_FSTYPE) size += sizeof(u_long);
		if (a & ATTR_VOL_SIGNATURE) size += sizeof(u_long);
		if (a & ATTR_VOL_SIZE) size += sizeof(off_t);
		if (a & ATTR_VOL_SPACEFREE) size += sizeof(off_t);
		if (a & ATTR_VOL_SPACEAVAIL) size += sizeof(off_t);
		if (a & ATTR_VOL_MINALLOCATION) size += sizeof(off_t);
		if (a & ATTR_VOL_ALLOCATIONCLUMP) size += sizeof(off_t);
		if (a & ATTR_VOL_IOBLOCKSIZE) size += sizeof(size_t);
		if (a & ATTR_VOL_OBJCOUNT) size += sizeof(u_long);
		if (a & ATTR_VOL_FILECOUNT) size += sizeof(u_long);
		if (a & ATTR_VOL_DIRCOUNT) size += sizeof(u_long);
		if (a & ATTR_VOL_MAXOBJCOUNT) size += sizeof(u_long);
		if (a & ATTR_VOL_MOUNTPOINT) size += sizeof(struct attrreference);
        if (a & ATTR_VOL_NAME) size += sizeof(struct attrreference);
        if (a & ATTR_VOL_MOUNTFLAGS) size += sizeof(u_long);
	};
	if ((a = attrlist->dirattr) != 0) {
		if (a & ATTR_DIR_LINKCOUNT) size += sizeof(u_long);
		if (a & ATTR_DIR_ENTRYCOUNT) size += sizeof(u_long);
	};
	if ((a = attrlist->fileattr) != 0) {
		if (a & ATTR_FILE_LINKCOUNT) size += sizeof(u_long);
		if (a & ATTR_FILE_TOTALSIZE) size += sizeof(off_t);
		if (a & ATTR_FILE_ALLOCSIZE) size += sizeof(off_t);
		if (a & ATTR_FILE_IOBLOCKSIZE) size += sizeof(size_t);
		if (a & ATTR_FILE_CLUMPSIZE) size += sizeof(off_t);
		if (a & ATTR_FILE_DEVTYPE) size += sizeof(u_long);
		if (a & ATTR_FILE_FILETYPE) size += sizeof(u_long);
		if (a & ATTR_FILE_FORKCOUNT) size += sizeof(u_long);
		if (a & ATTR_FILE_FORKLIST) size += sizeof(struct attrreference);
		if (a & ATTR_FILE_DATALENGTH) size += sizeof(off_t);
		if (a & ATTR_FILE_DATAALLOCSIZE) size += sizeof(off_t);
		if (a & ATTR_FILE_DATAEXTENTS) size += sizeof(extentrecord);
		if (a & ATTR_FILE_RSRCLENGTH) size += sizeof(off_t);
		if (a & ATTR_FILE_RSRCALLOCSIZE) size += sizeof(off_t);
		if (a & ATTR_FILE_RSRCEXTENTS) size += sizeof(extentrecord);
	};
	if ((a = attrlist->forkattr) != 0) {
		if (a & ATTR_FORK_TOTALSIZE) size += sizeof(off_t);
		if (a & ATTR_FORK_ALLOCSIZE) size += sizeof(off_t);
	};

    return size;
}



void
packvolattr (struct attrlist *alist,
			 struct iso_node *ip,	/* ip for root directory */
			 void **attrbufptrptr,
			 void **varbufptrptr)
{
    void *attrbufptr;
    void *varbufptr;
	struct iso_mnt *imp;
	attrgroup_t a;
	u_long attrlength;
	
	attrbufptr = *attrbufptrptr;
	varbufptr = *varbufptrptr;
	imp = ip->i_mnt;

    if ((a = alist->commonattr) != 0) {
        if (a & ATTR_CMN_NAME) {
            attrlength = strlen( imp->volume_id ) + 1;
            ((struct attrreference *)attrbufptr)->attr_dataoffset = varbufptr - attrbufptr;
            ((struct attrreference *)attrbufptr)->attr_length = attrlength;
            (void) strncpy((unsigned char *)varbufptr, imp->volume_id, attrlength);

            /* Advance beyond the space just allocated and round up to the next 4-byte boundary: */
            varbufptr += attrlength + ((4 - (attrlength & 3)) & 3);
            ++((struct attrreference *)attrbufptr);
        };
		if (a & ATTR_CMN_DEVID) *((dev_t *)attrbufptr)++ = imp->im_devvp->v_rdev;
		if (a & ATTR_CMN_FSID) *((fsid_t *)attrbufptr)++ = ITOV(ip)->v_mount->mnt_stat.f_fsid;
		if (a & ATTR_CMN_OBJTYPE) *((fsobj_type_t *)attrbufptr)++ = 0;
		if (a & ATTR_CMN_OBJTAG) *((fsobj_tag_t *)attrbufptr)++ = VT_ISOFS;
		if (a & ATTR_CMN_OBJID)	{
			((fsobj_id_t *)attrbufptr)->fid_objno = 0;
			((fsobj_id_t *)attrbufptr)->fid_generation = 0;
			++((fsobj_id_t *)attrbufptr);
		};
        if (a & ATTR_CMN_OBJPERMANENTID) {
            ((fsobj_id_t *)attrbufptr)->fid_objno = 0;
            ((fsobj_id_t *)attrbufptr)->fid_generation = 0;
            ++((fsobj_id_t *)attrbufptr);
        };
		if (a & ATTR_CMN_PAROBJID) {
            ((fsobj_id_t *)attrbufptr)->fid_objno = 0;
			((fsobj_id_t *)attrbufptr)->fid_generation = 0;
			++((fsobj_id_t *)attrbufptr);
		};
        if (a & ATTR_CMN_SCRIPT) *((text_encoding_t *)attrbufptr)++ = 0;
		if (a & ATTR_CMN_CRTIME) *((struct timespec *)attrbufptr)++ = imp->creation_date;
		if (a & ATTR_CMN_MODTIME) *((struct timespec *)attrbufptr)++ = imp->modification_date;
		if (a & ATTR_CMN_CHGTIME) *((struct timespec *)attrbufptr)++ = imp->modification_date;
		if (a & ATTR_CMN_ACCTIME) *((struct timespec *)attrbufptr)++ = imp->modification_date;
		if (a & ATTR_CMN_BKUPTIME) {
			((struct timespec *)attrbufptr)->tv_sec = 0;
			((struct timespec *)attrbufptr)->tv_nsec = 0;
			++((struct timespec *)attrbufptr);
		};
		if (a & ATTR_CMN_FNDRINFO) {
            bzero (attrbufptr, 32 * sizeof(u_int8_t));
            attrbufptr += 32 * sizeof(u_int8_t);
		};
		if (a & ATTR_CMN_OWNERID) *((uid_t *)attrbufptr)++ = ip->inode.iso_uid;
		if (a & ATTR_CMN_GRPID) *((gid_t *)attrbufptr)++ = ip->inode.iso_gid;
		if (a & ATTR_CMN_ACCESSMASK) *((u_long *)attrbufptr)++ = (u_long)ip->inode.iso_mode;
		if (a & ATTR_CMN_FLAGS) *((u_long *)attrbufptr)++ = 0;
	};
	
	if ((a = alist->volattr) != 0) {
		off_t blocksize = (off_t)imp->logical_block_size;

		if (a & ATTR_VOL_FSTYPE) *((u_long *)attrbufptr)++ = (u_long)imp->im_mountp->mnt_vfc->vfc_typenum;
		if (a & ATTR_VOL_SIGNATURE) *((u_long *)attrbufptr)++ = (u_long)ISO9660SIGNATURE;
        if (a & ATTR_VOL_SIZE) *((off_t *)attrbufptr)++ = (off_t)imp->volume_space_size * blocksize;
        if (a & ATTR_VOL_SPACEFREE) *((off_t *)attrbufptr)++ = 0;
        if (a & ATTR_VOL_SPACEAVAIL) *((off_t *)attrbufptr)++ = 0;
        if (a & ATTR_VOL_MINALLOCATION) *((off_t *)attrbufptr)++ = blocksize;
		if (a & ATTR_VOL_ALLOCATIONCLUMP) *((off_t *)attrbufptr)++ = blocksize;
        if (a & ATTR_VOL_IOBLOCKSIZE) *((size_t *)attrbufptr)++ = blocksize;
		if (a & ATTR_VOL_OBJCOUNT) *((u_long *)attrbufptr)++ = 0;
		if (a & ATTR_VOL_FILECOUNT) *((u_long *)attrbufptr)++ = 0;
		if (a & ATTR_VOL_DIRCOUNT) *((u_long *)attrbufptr)++ = 0;
		if (a & ATTR_VOL_MAXOBJCOUNT) *((u_long *)attrbufptr)++ = 0xFFFFFFFF;
        if (a & ATTR_VOL_NAME) {
            attrlength = strlen( imp->volume_id ) + 1;
            ((struct attrreference *)attrbufptr)->attr_dataoffset = varbufptr - attrbufptr;
            ((struct attrreference *)attrbufptr)->attr_length = attrlength;
            (void) strncpy((unsigned char *)varbufptr, imp->volume_id, attrlength);

            /* Advance beyond the space just allocated and round up to the next 4-byte boundary: */
            varbufptr += attrlength + ((4 - (attrlength & 3)) & 3);
            ++((struct attrreference *)attrbufptr);
        };
		if (a & ATTR_VOL_MOUNTFLAGS) *((u_long *)attrbufptr)++ = (u_long)imp->im_mountp->mnt_flag;
	};

	*attrbufptrptr = attrbufptr;
	*varbufptrptr = varbufptr;
}


void
packcommonattr (struct attrlist *alist,
				struct iso_node *ip,
				void **attrbufptrptr,
				void **varbufptrptr)
{
	void *attrbufptr;
	void *varbufptr;
	attrgroup_t a;
	u_long attrlength;
	
	attrbufptr = *attrbufptrptr;
	varbufptr = *varbufptrptr;
	
    if ((a = alist->commonattr) != 0) {
		struct iso_mnt *imp = ip->i_mnt;

        if (a & ATTR_CMN_NAME) {
			/* special case root since we know how to get it's name */
			if (ITOV(ip)->v_flag & VROOT) {
				attrlength = strlen( imp->volume_id ) + 1;
				(void) strncpy((unsigned char *)varbufptr, imp->volume_id, attrlength);
        	} else {
            	attrlength = strlen(ip->i_name) + 1;
				(void) strncpy((unsigned char *)varbufptr, ip->i_name, attrlength);
            }

			((struct attrreference *)attrbufptr)->attr_dataoffset = varbufptr - attrbufptr;
			((struct attrreference *)attrbufptr)->attr_length = attrlength;
            /* Advance beyond the space just allocated and round up to the next 4-byte boundary: */
            varbufptr += attrlength + ((4 - (attrlength & 3)) & 3);
            ++((struct attrreference *)attrbufptr);
        };
		if (a & ATTR_CMN_DEVID) *((dev_t *)attrbufptr)++ = ip->i_dev;
		if (a & ATTR_CMN_FSID) *((fsid_t *)attrbufptr)++ = ITOV(ip)->v_mount->mnt_stat.f_fsid;
		if (a & ATTR_CMN_OBJTYPE) *((fsobj_type_t *)attrbufptr)++ = ITOV(ip)->v_type;
		if (a & ATTR_CMN_OBJTAG) *((fsobj_tag_t *)attrbufptr)++ = ITOV(ip)->v_tag;
        if (a & ATTR_CMN_OBJID)	{
			if (ITOV(ip)->v_flag & VROOT)
				((fsobj_id_t *)attrbufptr)->fid_objno = 2;	/* force root to be 2 */
			else
            	((fsobj_id_t *)attrbufptr)->fid_objno = ip->i_number;
			((fsobj_id_t *)attrbufptr)->fid_generation = 0;
			++((fsobj_id_t *)attrbufptr);
		};
        if (a & ATTR_CMN_OBJPERMANENTID)	{
			if (ITOV(ip)->v_flag & VROOT)
				((fsobj_id_t *)attrbufptr)->fid_objno = 2;	/* force root to be 2 */
			else
            	((fsobj_id_t *)attrbufptr)->fid_objno = ip->i_number;
            ((fsobj_id_t *)attrbufptr)->fid_generation = 0;
            ++((fsobj_id_t *)attrbufptr);
        };
		if (a & ATTR_CMN_PAROBJID) {
			struct iso_directory_record *dp = (struct iso_directory_record *)imp->root;
			ino_t rootino = isodirino(dp, imp);

			if (ip->i_number == rootino)
				((fsobj_id_t *)attrbufptr)->fid_objno = 1;	/* force root parent to be 1 */
			else if (ip->i_parent == rootino)
				((fsobj_id_t *)attrbufptr)->fid_objno = 2;	/* force root to be 2 */
			else
            	((fsobj_id_t *)attrbufptr)->fid_objno = ip->i_parent;
			((fsobj_id_t *)attrbufptr)->fid_generation = 0;
			++((fsobj_id_t *)attrbufptr);
		};
        if (a & ATTR_CMN_SCRIPT) *((text_encoding_t *)attrbufptr)++ = 0;
		if (a & ATTR_CMN_CRTIME) *((struct timespec *)attrbufptr)++ = ip->inode.iso_mtime;
		if (a & ATTR_CMN_MODTIME) *((struct timespec *)attrbufptr)++ = ip->inode.iso_mtime;
		if (a & ATTR_CMN_CHGTIME) *((struct timespec *)attrbufptr)++ = ip->inode.iso_ctime;
		if (a & ATTR_CMN_ACCTIME) *((struct timespec *)attrbufptr)++ = ip->inode.iso_atime;
		if (a & ATTR_CMN_BKUPTIME) {
			((struct timespec *)attrbufptr)->tv_sec = 0;
			((struct timespec *)attrbufptr)->tv_nsec = 0;
			++((struct timespec *)attrbufptr);
		};
		if (a & ATTR_CMN_FNDRINFO) {
			struct finder_info finfo = {0};

			finfo.fdFlags = ip->i_FinderFlags;
			if (ITOV(ip)->v_type == VREG) {
				finfo.fdType = ip->i_FileType;
				finfo.fdCreator = ip->i_Creator;
			}
            bcopy (&finfo, attrbufptr, sizeof(finfo));
            attrbufptr += sizeof(finfo);
            bzero (attrbufptr, EXTFNDRINFOSIZE);
            attrbufptr += EXTFNDRINFOSIZE;
		};
		if (a & ATTR_CMN_OWNERID) *((uid_t *)attrbufptr)++ = ip->inode.iso_uid;
		if (a & ATTR_CMN_GRPID) *((gid_t *)attrbufptr)++ = ip->inode.iso_gid;
		if (a & ATTR_CMN_ACCESSMASK) *((u_long *)attrbufptr)++ = (u_long)ip->inode.iso_mode;
		if (a & ATTR_CMN_FLAGS) *((u_long *)attrbufptr)++ = 0; /* could also use ip->i_flag */
	};
	
	*attrbufptrptr = attrbufptr;
	*varbufptrptr = varbufptr;
}


void
packdirattr(struct attrlist *alist,
			struct iso_node *ip,
			void **attrbufptrptr,
			void **varbufptrptr)
{
    void *attrbufptr;
    attrgroup_t a;
	
	attrbufptr = *attrbufptrptr;
	
	a = alist->dirattr;
	if ((ITOV(ip)->v_type == VDIR) && (a != 0)) {
		if (a & ATTR_DIR_LINKCOUNT) *((u_long *)attrbufptr)++ = ip->inode.iso_links;
		if (a & ATTR_DIR_ENTRYCOUNT) *((u_long *)attrbufptr)++ = 2; /* XXX */
	};
	
	*attrbufptrptr = attrbufptr;
}


void
packfileattr(struct attrlist *alist,
			 struct iso_node *ip,
			 void **attrbufptrptr,
			 void **varbufptrptr)
{
    void *attrbufptr = *attrbufptrptr;
    void *varbufptr = *varbufptrptr;
    attrgroup_t a = alist->fileattr;
	
	if ((ITOV(ip)->v_type == VREG) && (a != 0)) {
		if (a & ATTR_FILE_LINKCOUNT) *((u_long *)attrbufptr)++ = ip->inode.iso_links;
		if (a & ATTR_FILE_TOTALSIZE) *((off_t *)attrbufptr)++ = (off_t)ip->i_size;
		if (a & ATTR_FILE_ALLOCSIZE) *((off_t *)attrbufptr)++ = (off_t)ip->i_size;
		if (a & ATTR_FILE_IOBLOCKSIZE) *((u_long *)attrbufptr)++ = ip->i_mnt->logical_block_size;
		if (a & ATTR_FILE_CLUMPSIZE) *((u_long *)attrbufptr)++ = ip->i_mnt->logical_block_size;
		if (a & ATTR_FILE_DEVTYPE) *((u_long *)attrbufptr)++ = (u_long)ip->inode.iso_rdev;
		if (0 /* XXX associated file */) {
			if (a & ATTR_FILE_RSRCLENGTH) *((off_t *)attrbufptr)++ = ip->i_size;
			if (a & ATTR_FILE_RSRCALLOCSIZE) *((off_t *)attrbufptr)++ = ip->i_size;
			if (a & ATTR_FILE_DATALENGTH) *((off_t *)attrbufptr)++ = (off_t)0;
			if (a & ATTR_FILE_DATAALLOCSIZE) *((off_t *)attrbufptr)++ = (off_t)0;
		} else {
			if (a & ATTR_FILE_DATALENGTH) *((off_t *)attrbufptr)++ = (off_t)ip->i_size;
			if (a & ATTR_FILE_DATAALLOCSIZE) *((off_t *)attrbufptr)++ = (off_t)ip->i_size;
			if (a & ATTR_FILE_RSRCLENGTH) *((off_t *)attrbufptr)++ = (off_t)0;
			if (a & ATTR_FILE_RSRCALLOCSIZE) *((off_t *)attrbufptr)++ = (off_t)0;
		};
	};
	
	*attrbufptrptr = attrbufptr;
	*varbufptrptr = varbufptr;
}


void
packattrblk(struct attrlist *alist,
			struct vnode *vp,
			void **attrbufptrptr,
			void **varbufptrptr)
{
	struct iso_node *ip = VTOI(vp);

	if (alist->volattr != 0) {
		packvolattr(alist, ip, attrbufptrptr, varbufptrptr);
	} else {
		packcommonattr(alist, ip, attrbufptrptr, varbufptrptr);
		
		switch (ITOV(ip)->v_type) {
		  case VDIR:
			packdirattr(alist, ip, attrbufptrptr, varbufptrptr);
			break;
			
		  case VREG:
			packfileattr(alist, ip, attrbufptrptr, varbufptrptr);
			break;
		  
	#if 0							/* XXX PPD TBC */
		  case VFORK:
			packforkattr(alist, ip, attrbufptrptr, varbufptrptr);
			break;
	#endif
		  
		  /* Without this the compiler complains about VNON,VBLK,VCHR,VLNK,VSOCK,VFIFO,VBAD and VSTR
		     not being handled...
		   */
		  default:
			/* XXX PPD - Panic? */
			break;
		};
	};
};
