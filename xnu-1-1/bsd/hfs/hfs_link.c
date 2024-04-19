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

#if HFS_HARDLINKS

#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/namei.h>
#include <sys/stat.h>
#include <sys/vnode.h>
#include <vfs/vfs_support.h>

#include "hfs.h"
#include "hfscommon/headers/FileMgrInternal.h"


#define OFFSETOF(structure,field) ((size_t)&((structure *) 0)->field)

#define MAKE_DATANODE_NAME(NAME,FID) \
	    (void) sprintf((NAME), "%s%d", HFS_LINK_PREFIX, (FID))


#define ALIAS_VERS	2
#define ALIAS_TYPE	'alis'
#define ALIAS_ENDMARK	(-1)
#define ALIAS_RSRC_ID	0

/* alias kind is a file or directory */
enum {
	kFileAlias,		/* 0  file alias */
	kDirAlias		/* 1  directory/folder alias */
};
typedef int16_t AliasKind;


#if PRAGMA_STRUCT_ALIGN
	#pragma options align=mac68k
#elif PRAGMA_STRUCT_PACKPUSH
	#pragma pack(push, 2)
#elif PRAGMA_STRUCT_PACK
	#pragma pack(2)
#endif

/* Alias record as it is manipulated by Alias manager */
struct aliasrec {
	/* first two fields are for application use */
	OSType		userType;	/* appl stored type */
	u_int16_t 	aliasSize;	/* alias record size in bytes */

	/*
	 * What follows is a variable length amount of data that is
	 * private.  These private fields should not be accessed by
	 * apps directly.
	 */
	int16_t		aliasVersion;	/* alias version, used internally */
	AliasKind	thisAliasKind;	/* file or directory */
	Str27		volumeName;	/* volume name */
	u_int32_t	volumeCrDate;	/* volume creation date used as volID */
	u_int16_t	volumeSig;	/* volume signature (flat or hierarchical) */
	int16_t		volumeType;	/* 0 = HD */
	u_int32_t	parDirID;	/* parent directory ID */
	Str63		fileName;	/* file or directory(if dir Alias) name */
	u_int32_t	fileNum;	/* unique file number(also known as file ID) or directory ID */
	u_int32_t	fileCrDate;	/* file or directory creation date */
	OSType		fileType;	/* file type */
	OSType		fdCreator;	/* file's creator */
	int16_t		nlvlFrom; 	/* # of levels from fromFile/toFile until a common */
	int16_t		nlvlTo;		/* ancestor directory is found */
	u_int32_t 	volumeAttr;	/* bitfield for VolMntInfo exists or not, vol ejectable or not etc. */
	int16_t		volumeFSID;	/* file system ID for the volume */
	int8_t		unused[10];	/* 10 bytes for future expansion */
	int16_t 	vdwhat;		/* what kind of information */
	int16_t 	vdlen;		/* length of variable data */
};



struct rsrcforkhdr {
	u_int32_t	rsrcDataOffset;	/* Offset to the resource data from beginning of the file */
	u_int32_t	rsrcMapOffset;	/* Offset to the resource map from the beginning of the file */
	u_int32_t	rsrcDataLength;	/* Total length of resource data */
	u_int32_t	rsrcMapLength;	/* Total length of resource map */
};

/*
   For each resource type in a resource file, the resource map contains a type entry
   which specifies the resource type, how many resources there are of this type, and
   the offset from the beginning of the type list to the reference list for this type
*/
struct TypeEntry {
	ResType 	tType;		/* The resource type*/
	u_int16_t 	tCount;		/* Number of resources of this type, minus 1*/
	u_int16_t 	tOffset;	/* Offset to resource reference list for this resource type.*/
};
typedef struct TypeEntry TypeEntry;
typedef TypeEntry *TypeEntryPtr;
/*
   For each resource in a resource file, the resource map contains a reference entry
   which specifies the resource ID, the offset to the resourceÕs name (if any), the
   offset to the actual resource data from the beginning of the resource data, and
   the handle to the resource data in memory, if itÕs loaded.
*/


union ResAttrData {
	u_int8_t 	rAttr;		/* Attributes for this resource*/
	u_int32_t 	rDataLocation;	/* Offset from beginning of resource data to length of data for this resource.*/
};
typedef union ResAttrData ResAttrData;

struct ReferenceEntry {
	int16_t 	rID;		/* Resource ID*/
	int16_t 	rNameOffset;	/* Offset to resource name.  (-1 if no name)*/
	ResAttrData 	rAttrData;	/* Attributes or offset to length of data for this resource*/
	Handle 		rHandle;	/* Resource handle.  (0 if resource is not loaded)*/
};
typedef struct ReferenceEntry ReferenceEntry;

/*
   The resource map describes all of the resources in a file, with a list of type entries,
   reference entries, and a list of resource names.  It also contains the file reference
   number of the resource file, and some attributes.
   IÕve split the resource map into two structs, the rsrcmaphdr, which contains
   information that is in an empty resource map (a resource map that contains no resources)
   and the rsrcmap proper, which contains fields which describe type entries, reference
   entries, and the name list.
*/


struct rsrcmaphdr {
	struct rsrcforkhdr 	mHeader;	/* Copy of the resource header*/
	Handle			mNext;		/* Handle to next resource map*/
	u_int16_t 		mRefNum;	/* File reference number of this resource file*/
	u_int8_t 		mAttr;		/* Map attributes (read only, compact, changed)*/
	u_int8_t 		mInMemoryAttr;	/* Override attributes & decompression bit*/
	u_int16_t 		mTypes;		/* Offset from start of resource map to type list (typically 0x1C)*/
	u_int16_t 		mNames;		/* Offset from start of resource map to name list*/
};

struct rsrcmap {
	struct rsrcmaphdr 	mapHeader;	/* Header information for the resource map*/
	u_int16_t 		typeCount;	/* Number of resource types in this map, minus 1*/
	TypeEntry 		typeList[1];	/* Type entries for all the types in this resource file*/
	ReferenceEntry 		refList[1];	/* Reference entries for all resources in this file*/
};


struct ResourceFork {
	struct rsrcforkhdr	rsrcHead;
	u_int8_t		reserved[240];
	u_int32_t		rsrcLen;
	struct aliasrec		rsrcData;
	struct rsrcmap		rsrcMap;
};


#if PRAGMA_STRUCT_ALIGN
	#pragma options align=reset
#elif PRAGMA_STRUCT_PACKPUSH
	#pragma pack(pop)
#elif PRAGMA_STRUCT_PACK
	#pragma pack()
#endif


/*
 *
 */
int
readlinknode(ExtendedVCB *vcb, hfsCatalogInfo *catInfo, UInt32 *nodeID)
{
	int result;
	struct ResourceFork *rfp = NULL;
	struct buf *bp = NULL;
	int blksize;
	daddr_t diskblk;
	off_t diskoff;

	*nodeID = 0;
	blksize = VCBTOHFS(vcb)->hfs_phys_block_size;
	if (catInfo->nodeData.cnd_rsrcfork.logicalSize < sizeof(struct ResourceFork))
		return (ENOENT);
	
	diskoff = vcb->hfsPlusIOPosOffset;
	diskoff += (off_t)catInfo->nodeData.cnd_rsrcfork.extents[0].startBlock * (off_t)vcb->blockSize;
	diskblk = diskoff / blksize;

	result = bread(	VCBTOHFS(vcb)->hfs_devvp,
			IOBLKNOFORBLK(diskblk, blksize),
			IOBYTECCNTFORBLK(diskblk, blksize, blksize),
			NOCRED,
			&bp);
	if (result) {
        	if (bp != NULL)
        		brelse(bp);
        	return (result);
        }

	rfp = (struct ResourceFork *) ((char *)bp->b_data + IOBYTEOFFSETFORBLK(diskblk, blksize));

	/* Make sure we have an alias record */
	if ((rfp->rsrcData.aliasSize >= sizeof(struct aliasrec))	&&
	    (rfp->rsrcData.aliasVersion == ALIAS_VERS)			&&
	    (rfp->rsrcData.thisAliasKind == kFileAlias)
	   ) {
		*nodeID = rfp->rsrcData.fileNum;
	} else {
		brelse(bp);
		return (ENOENT);
	}

	brelse(bp);

	return (0);
}

/*
 * Create a new link node
 *
 * A link node is a reference to a data node.  The only useable fields in the
 * link are the parentID, name and text encoding.  All other catalog fields
 * are ignored.  The target data node reference is stored as an alias.
 */
static int
newlinknode(struct hfsnode *dnhp, UInt32 linkPID, char *linkName, struct ucred *cred)
{
	struct hfsCatalogInfo catInfo;
	struct buf *bp = NULL;
	struct ResourceFork *rfp = NULL;
	struct FInfo *fip;
	ExtendedVCB *vcb;
	int forksize;
	u_int32_t blk, diskblk;
	u_int32_t numblk;
	u_int32_t phyblksize;
	int result;
	
	blk = numblk = 0;
	vcb = HTOVCB(dnhp);
	phyblksize = VCBTOHFS(vcb)->hfs_phys_block_size;
	forksize = sizeof(struct ResourceFork);
	if (forksize > phyblksize)
		return (EINVAL);

	/* Create the link node directly in the catalog */
	result = hfsCreate(vcb, linkPID, linkName, IFREG);
	if (result) return (result);

	/* 
	 * XXX SER Here is a good example where hfsCreate should pass in a catinfo and return
	 * things like the hint and file ID there should be no reason to call lookup here 
	 */
	catInfo.hint = 0;
	result = hfsLookup(vcb, linkPID, linkName, -1, &catInfo);
	if (result) goto errExit;

	fip = (struct FInfo *)&catInfo.nodeData.cnd_finderInfo;
	fip->fdType = kHardLinkFileType;
	fip->fdCreator = kHardLinkCreator;
	fip->fdFlags |= kIsAlias + kHasBeenInited;

	/* Allocate space for link node's resource fork */
	result = MacToVFSError(BlockAllocate(vcb, 0, forksize, forksize, TRUE, &blk, &numblk));
	if (result) goto errExit;

	catInfo.nodeData.cnd_rsrcfork.extents[0].startBlock = blk;
	catInfo.nodeData.cnd_rsrcfork.extents[0].blockCount = numblk;
	catInfo.nodeData.cnd_rsrcfork.logicalSize = forksize;
	catInfo.nodeData.cnd_rsrcfork.totalBlocks = numblk;

	result = UpdateCatalogNode(vcb, linkPID, linkName, catInfo.hint, &catInfo.nodeData);
	if (result) goto errExit;

	/* map logical block to physical disk block */
	diskblk = blk * (vcb->blockSize / phyblksize) +
			vcb->hfsPlusIOPosOffset / phyblksize;

	bp = getblk (VCBTOHFS(vcb)->hfs_devvp,
		     IOBLKNOFORBLK(diskblk, phyblksize),
		     IOBYTECCNTFORBLK(diskblk, phyblksize, phyblksize),
		     0, 0);
	rfp = (struct ResourceFork *) ((char *)bp->b_data + IOBYTEOFFSETFORBLK(diskblk, phyblksize));
	bzero(rfp, phyblksize);

	/* fill in resource fork header... */
	rfp->rsrcHead.rsrcDataOffset = OFFSETOF(struct ResourceFork,rsrcLen);
	rfp->rsrcHead.rsrcMapOffset = OFFSETOF(struct ResourceFork,rsrcMap);
	rfp->rsrcHead.rsrcDataLength = sizeof(struct aliasrec) + 4;
	rfp->rsrcHead.rsrcMapLength = sizeof(struct rsrcmap);

	/* fill in resource map... */
	rfp->rsrcMap.mapHeader.mHeader = rfp->rsrcHead;
	rfp->rsrcMap.mapHeader.mTypes = sizeof(struct rsrcmaphdr);
	rfp->rsrcMap.mapHeader.mNames = sizeof(struct rsrcmap);
	rfp->rsrcMap.typeList[0].tType = ALIAS_TYPE;
	rfp->rsrcMap.typeList[0].tOffset = sizeof(TypeEntry) + 2;
	rfp->rsrcMap.refList[0].rID = ALIAS_RSRC_ID;
	rfp->rsrcMap.refList[0].rNameOffset = -1;

	/* fill in alias resource... */
	rfp->rsrcData.aliasSize = rfp->rsrcLen = sizeof(struct aliasrec);
	rfp->rsrcData.aliasVersion = ALIAS_VERS;
	rfp->rsrcData.thisAliasKind = kFileAlias;
//	rfp->rsrcData.fileType	= '????';
//	rfp->rsrcData.fdCreator = '????';
	rfp->rsrcData.fileNum = H_FILEID(dnhp);
	rfp->rsrcData.fileCrDate = to_hfs_time(dnhp->h_meta->h_crtime);
	rfp->rsrcData.volumeCrDate = UTCToLocal(vcb->vcbCrDate);
	rfp->rsrcData.volumeSig = kHFSSigWord;	/* always 'BD' */
	rfp->rsrcData.nlvlFrom = -1;
	rfp->rsrcData.nlvlTo = -1;
	rfp->rsrcData.vdwhat = ALIAS_ENDMARK; /* no extra data */
	
	/* XXX can this overflow volumeName field? */
	result = utf8_to_hfs(vcb, strlen(vcb->vcbVN), vcb->vcbVN, rfp->rsrcData.volumeName);
	if (result) goto errExit;

	rfp->rsrcData.parDirID = HTOHFS(dnhp)->hfs_private_metadata_dir;
	MAKE_DATANODE_NAME(&rfp->rsrcData.fileName[1], H_FILEID(dnhp));
	rfp->rsrcData.fileName[0] = strlen(&rfp->rsrcData.fileName[1]);


	/* Finally, write resource fork to disk */
	result = bwrite(bp);
	if (result) goto errExit;

	return (0);

errExit:
	/* get rid of link node */
	(void) hfsDelete(vcb, linkPID, linkName, TRUE, 0);
	if (numblk > 0)
		(void) BlockDeallocate(vcb, blk, numblk);

	return (result);
}


/*
 * 2 locks are needed (dvp and hp)
 * also need catalog lock and extents lock (for exchange)
 *
 * caller's responsibility:
 *		componentname cleanup
 *		unlocking dvp and hp
 */
static int
hfs_makelink(hp, dvp, cnp)
	struct hfsnode *hp;
	struct vnode *dvp;
	register struct componentname *cnp;
{
	struct proc *p = cnp->cn_proc;
	struct hfsnode *dhp = VTOH(dvp);
	u_int32_t ldirID;	/* directory ID of linked nodes directory */
	ExtendedVCB *vcb = VTOVCB(dvp);
	u_int32_t hint;
	char nodeName[32];
	int retval;

	ldirID = VTOHFS(dvp)->hfs_private_metadata_dir;

	/* We don't allow link nodes in our Private Meta Data folder! */
	if ( H_FILEID(dhp) == ldirID)
		return EPERM;

	if (vcb->freeBlocks == 0)
		return ENOSPC;

	/* lock catalog b-tree */
	retval = hfs_metafilelocking(VTOHFS(dvp), kHFSCatalogFileID, LK_EXCLUSIVE, p);
	if (retval != E_NONE) goto out2;

	/*
	 * Create a catalog entry for the new link (parentID + name).
	 */
	retval = newlinknode(hp, H_FILEID(dhp), cnp->cn_nameptr, cnp->cn_cred);
	if (retval) goto out;
	
	/*
	 * If this is a new hardlink then we need to create the data
	 * node (inode) and replace the original file with a link node.
	 */
	if (hp->h_meta->h_nlink == 1) {
		MAKE_DATANODE_NAME(nodeName, H_FILEID(hp));
		
		/* move source file to data node directory */
		hint = 0;
		retval = hfsMoveRename(vcb, H_DIRID(hp), H_NAME(hp), ldirID, nodeName, &hint);
		if (retval) goto err1Link;
		
		/* replace source file with link node */
		retval = newlinknode(hp, H_DIRID(hp), H_NAME(hp), cnp->cn_cred);
		if (retval) goto errMoved;

		hp->h_meta->h_nlink++;
		hp->h_nodeflags |= IN_CHANGE;
		hp->h_meta->h_metaflags |= IN_DATANODE;
  	}

out:;
	/* unlock catalog b-tree */
	(void) hfs_metafilelocking(VTOHFS(dvp), kHFSCatalogFileID, LK_RELEASE, p);

out2:;
	return retval;


errMoved:;
	/* put it source file back */
	hint = 0;
	(void) hfsMoveRename(vcb, ldirID, nodeName, H_DIRID(hp), H_NAME(hp), &hint);
	/* fall through */
err1Link:;
	/* get rid of target link node */
	(void) hfsDelete(vcb, H_FILEID(dhp), cnp->cn_nameptr, TRUE, 0);
	goto out;
}


/*
 * link vnode call
#% link		vp	U U U
#% link		tdvp	L U U
#
 vop_link {
     IN WILLRELE struct vnode *vp;
     IN struct vnode *targetPar_vp;
     IN struct componentname *cnp;

     */
int
hfs_link(ap)
struct vop_link_args /* {
	struct vnode *a_vp;
	struct vnode *a_tdvp;
	struct componentname *a_cnp;
} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct vnode *tdvp = ap->a_tdvp;
	struct componentname *cnp = ap->a_cnp;
	struct proc *p = cnp->cn_proc;
	struct hfsnode *hp;
	struct timeval tv;
	int error;

#if HFS_DIAGNOSTIC
	if ((cnp->cn_flags & HASBUF) == 0)
		panic("hfs_link: no name");
#endif
	if (tdvp->v_mount != vp->v_mount) {
		VOP_ABORTOP(tdvp, cnp);
		error = EXDEV;
		goto out2;
	}
	if (VTOVCB(tdvp)->vcbSigWord != kHFSPlusSigWord)
		return err_link(ap);	/* hfs disks don't support hard links */
	
	if (VTOHFS(vp)->hfs_private_metadata_dir == 0)
		return err_link(ap);	/* no private metadata dir, no links possible */

	if (tdvp != vp && (error = vn_lock(vp, LK_EXCLUSIVE, p))) {
		VOP_ABORTOP(tdvp, cnp);
		goto out2;
	}
	hp = VTOH(vp);
	if (hp->h_meta->h_nlink >= HFS_LINK_MAX) {
		VOP_ABORTOP(tdvp, cnp);
		error = EMLINK;
		goto out1;
	}
	if (hp->h_meta->h_pflags & (IMMUTABLE | APPEND)) {
		VOP_ABORTOP(tdvp, cnp);
		error = EPERM;
		goto out1;
	}
	hp->h_meta->h_nlink++;
	hp->h_nodeflags |= IN_CHANGE;
	tv = time;
	error = VOP_UPDATE(vp, &tv, &tv, 1);
	if (!error)
		error = hfs_makelink(hp, tdvp, cnp);
	if (error) {
		hp->h_meta->h_nlink--;
		hp->h_nodeflags |= IN_CHANGE;
	}
	FREE_ZONE(cnp->cn_pnbuf, cnp->cn_pnlen, M_NAMEI);
out1:
	if (tdvp != vp)
		VOP_UNLOCK(vp, 0, p);
out2:
	vput(tdvp);
	return (error);
}

#endif
