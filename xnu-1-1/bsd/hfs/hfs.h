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
/*	@(#)hfs.h		3.0
*
*	(c) 1990, 1992 NeXT Computer, Inc.  All Rights Reserved
*	(c) 1997-1999 Apple Computer, Inc.  All Rights Reserved
*
*	hfs.h -- constants, structures, function declarations. etc.
*			for Macintosh file system vfs.
*
*	HISTORY
* 12-Aug-1999	Scott Roberts	Merge into HFSStruct, the FCB
*  6-Jun-1999	Don Brady	Minor cleanup of hfsmount struct.
* 22-Mar-1999	Don Brady	For POSIX delete semantics: add private metadata strings.
* 13-Jan-1999	Don Brady	Add ATTR_CMN_SCRIPT to HFS_ATTR_CMN_LOOKUPMASK (radar #2296613).
* 20-Nov-1998	Don Brady	Remove UFSToHFSStr and HFSToUFSStr prototypes (obsolete).
*							Move filename entry from FCB to hfsfilemeta, hfsdirentry
*							names are now 255 byte long.
* 10-Nov-1998	Pat Dirks	Added MAXLOGBLOCKSIZE and MAXLOGBLOCKSIZEBLOCKS and RELEASE_BUFFER flag.
*                           Added hfsLogicalBlockTableEntry and h_logicalblocktable field in struct hfsnode.
*  4-Sep-1998	Pat Dirks	Added hfs_log_block_size to hfsmount struct [again] and BestBlockSizeFit routine.
* 31-aug-1998	Don Brady	Add UL to MAC_GMT_FACTOR constant.
* 04-jun-1998	Don Brady	Add hfsMoveRename prototype to replace hfsMove and hfsRename.
*							Add VRELE and VPUT macros to catch bad ref counts.
* 28-May-1998	Pat Dirks	Move internal 'struct searchinfo' def'n here from attr.h
* 03-may-1998	Brent Knight	Add gTimeZone.
* 23-apr-1998	Don Brady	Add File type and creator for symbolic links.
* 22-apr-1998	Don Brady	Removed kMetaFile.
* 21-apr-1998	Don Brady	Add to_bsd_time and to_hfs_time prototypes.
* 20-apr-1998	Don Brady	Remove course-grained hfs metadata locking.
* 15-apr-1998	Don Brady	Add hasOverflowExtents and hfs_metafilelocking prototypes. Add kSysFile constant.
* 14-apr-1998	Deric Horn	Added searchinfo_t, definition of search criteria used by searchfs.
*  9-apr-1998	Don Brady	Added hfs_flushMDB and hfs_flushvolumeheader prototypes.
*  8-apr-1998	Don Brady	Add MAKE_VREFNUM macro.
* 26-mar-1998	Don Brady	Removed CloseBTreeFile and OpenBtreeFile prototypes.
*
* 12-nov-1997 	Scott Roberts	Added changes for HFSPlus
*/

#ifndef __HFS__
#define __HFS__

#include <sys/param.h>
#include <sys/lock.h>
#include <sys/queue.h>
#include <sys/attr.h>

#include <sys/dirent.h>

#include <hfs/hfs_format.h>
#include <hfs/hfs_macos_defs.h>
#include <hfs/hfs_encodings.h>


struct uio;				// This is more effective than #include <sys/uio.h> in case KERNEL is undefined...
struct hfslockf;		// For advisory locking

/*
 *	Just reported via MIG interface.
 */
#define VERSION_STRING	"hfs-2 (4-12-99)"


#define HFS_LINK_MAX	32767

/*
 *	Set to force READ_ONLY.
 */
#define	FORCE_READONLY	0

enum { kMDBSize = 512 };				/* Size of I/O transfer to read entire MDB */

enum { kMasterDirectoryBlock = 2 };			/* MDB offset on disk in 512-byte blocks */
enum { kMDBOffset = kMasterDirectoryBlock * 512 };	/* MDB offset on disk in bytes */

enum {
	kUnknownID = 0,
	kRootParID = 1,
	kRootDirID = 2
};

enum {
	kUndefinedFork 	= 0,
	kAnyFork,
	kDataFork,
	kRsrcFork,
	kDirectory,
	kSysFile,
	kDefault
};


/*
 *	File type and creator for symbolic links
 */
enum {
#if HFS_HARDLINKS
	kHardLinkFileType	= 0x686C6E6B,	/* 'hlnk' */
	kHardLinkCreator	= 0x72686170,	/* 'rhap' */
#endif
	kSymLinkFileType	= 0x736C6E6B,	/* 'slnk' */
	kSymLinkCreator		= 0x72686170	/* 'rhap' */
};


#define MAXLOGBLOCKSIZE PAGE_SIZE
#define MAXLOGBLOCKSIZEBLOCKS (MAXLOGBLOCKSIZE/512)
/* NOTE: Special support will be needed for LOGBLOCKMAPENTRIES > kHFSPlusExtentDensity (=8) */
#define LOGBLOCKMAPENTRIES 8

#define BUFFERPTRLISTSIZE 25

extern char * gBufferAddress[BUFFERPTRLISTSIZE];
extern struct buf *gBufferHeaderPtr[BUFFERPTRLISTSIZE];
extern int gBufferListIndex;
extern  simple_lock_data_t gBufferPtrListLock;

extern struct timezone gTimeZone;

/* Flag values for bexpand: */
#define RELEASE_BUFFER 0x00000001


/* Internal Data structures*/

struct vcb_t {
    int16_t			vcbFlags;
    u_int16_t 			vcbSigWord;
    u_int32_t 			vcbCrDate;
    u_int32_t 			vcbLsMod;
    int16_t 			vcbAtrb;
    u_int16_t 			vcbNmFls;			/* HFS only */
    int16_t 			vcbVBMSt;
    int16_t 			vcbAlBlSt;
    int32_t 			vcbClpSiz;
    int32_t 			vcbNxtCNID;
    u_int8_t		 	vcbVN[256];
    int16_t 			vcbVRefNum;
    u_int16_t 			vcbVSeqNum;
    u_int32_t 			vcbVolBkUp;
    int32_t 			vcbWrCnt;
    u_int16_t 			vcbNmRtDirs;		/* HFS only */
    u_int16_t 			vcbReserved;
    int32_t 			vcbFilCnt;
    int32_t 			vcbDirCnt;
    int32_t 			vcbFndrInfo[8];
    struct vnode *		extentsRefNum;
    struct vnode *		catalogRefNum;
    struct vnode *		allocationsRefNum;
    u_int32_t 			blockSize;			/*	size of allocation blocks - vcbAlBlkSiz*/
    u_int32_t 			totalBlocks;		/*	number of allocation blocks in volume */
    u_int32_t 			freeBlocks;			/*	number of unused allocation blocks - vcbFreeBks*/
    u_int32_t 			nextAllocation;		/*	start of next allocation search - vcbAllocPtr*/
	u_int32_t			altIDSector;		/* location of alternate MDB/VH */
    u_int32_t 			hfsPlusIOPosOffset;	/*	Disk block where HFS+ starts	*/
    u_int32_t 			checkedDate;		/*	date and time of last disk check	*/
    u_int64_t 			encodingsBitmap;	/* HFS Plus only*/
    u_int32_t	 		volumeNameEncodingHint;	/* Text encoding used for volume name*/
    char *	 			hintCachePtr;		/* points to this volumes heuristicHint cache*/
    simple_lock_data_t	vcbSimpleLock;		/* simple lock to allow concurrent access to vcb data */
};
typedef struct vcb_t ExtendedVCB;

/* vcbFlags */
#define			kHFS_DamagedVolume			0x1	/* This volume has errors, unmount dirty */


/*
 * NOTE: The code relies on being able to cast an ExtendedVCB* to a vfsVCB* in order
 *	 to gain access to the mount point pointer from a pointer
 *	 to an ExtendedVCB.  DO NOT INSERT OTHER FIELDS BEFORE THE vcb FIELD!!
 *
 * vcbFlags, vcbLsMod, vcbFilCnt, vcbDirCnt, vcbNxtCNID, etc
 * are locked by the hfs_lock simple lock.
 */
typedef struct vfsVCB {
    ExtendedVCB			vcb_vcb;
    struct hfsmount		*vcb_hfsmp;				/* Pointer to hfsmount structure */
} vfsVCB_t;



/* This structure describes the HFS specific mount structure data. */
typedef struct hfsmount {
	u_long				hfs_mount_flags;
	u_int8_t			hfs_fs_clean;			/* Whether contents have been flushed in clean state */
    u_int8_t			hfs_fs_ronly;			/* Whether this was mounted as read-initially  */

	/* Physical Description */
	u_long				hfs_phys_block_count;	/* Num of PHYSICAL blocks of volume */
	u_long				hfs_phys_block_size;	/* Always a multiple of 512 */

	/* Access to VFS and devices */
	struct mount		*hfs_mp;				/* filesystem vfs structure */
	struct vnode		*hfs_devvp;				/* block device mounted vnode */
	dev_t				hfs_raw_dev;			/* device mounted */
	u_int32_t			hfs_logBlockSize;		/* Size of buffer cache buffer for I/O */
	
	/* Default values for HFS standard and non-init access */
	uid_t				hfs_uid;				/* uid to set as owner of the files */
	gid_t				hfs_gid;				/* gid to set as owner of the files */
	mode_t				hfs_dir_mask;			/* mask to and with directory protection bits */
	mode_t				hfs_file_mask;			/* mask to and with file protection bits */
	u_long				hfs_encoding;			/* Defualt encoding for non hfs+ volumes */	

	/* HFS Specific */
	struct vfsVCB		hfs_vcb;
	u_long			hfs_private_metadata_dir; /* private/hidden directory for unlinked files */
	hfs_to_unicode_func_t	hfs_get_unicode;
	unicode_to_hfs_func_t	hfs_get_hfsname;
} hfsmount_t;


/*****************************************************************************
*
*	hfsnode structure
*
*
*
*****************************************************************************/

#define MAXHFSVNODELEN		31
typedef u_char FileNameStr[MAXHFSVNODELEN+1];

CIRCLEQ_HEAD(siblinghead, hfsnode)	;	/* Head of the sibling list */

struct hfsLogicalBlockTableEntry {
	u_int32_t			logicalBlockCount;
	off_t				extentLength;
};


struct  hfsnode {
	LIST_ENTRY(hfsnode) h_hash;				/* links on valid files */
	CIRCLEQ_ENTRY(hfsnode) h_sibling;		/* links on siblings */
	struct lock__bsd__	h_lock;				/* node lock. */
	struct hfslockf *	h_lockf;			/* Head of byte-level lock list. */
	u_int32_t			h_nodeflags;		/* flags, see below */
	struct vnode *		h_vp;				/* vnode associated with this inode. */
	struct hfsfilemeta *h_meta;				/* Ptr to file meta data */
	u_int8_t			h_type;				/* Type of info: dir, data, rsrc */

	/* Buffered Cache information */
	struct hfsLogicalBlockTableEntry h_logicalblocktable[LOGBLOCKMAPENTRIES];
	off_t				h_optimizedblocksizelimit;	/* End of range covered by h_logicalblocktable */
	daddr_t				h_uniformblocksizestart;	/* First LBN in fixed-size log. block range */
	
	/* FCB struct */
	int8_t 				fcbFlags;			/* FCB flags */
	u_int64_t 			fcbEOF;				/* Logical length or EOF in bytes */
	u_int64_t 			fcbPLen;			/* Physical file length in bytes */
	u_int32_t 			fcbClmpSize;		/* Number of bytes per clump */
	HFSPlusExtentRecord fcbExtents;			/* Extents of file */
	
	/* Used for Meta files */
	void * 				fcbBTCBPtr;			/* Pointer to B*-Tree control block for file, opaque */

	u_int32_t			h_valid;			/* is the vnode reference valid */
};
typedef struct hfsnode FCB;
typedef struct hfsnode *hfsnodeptr;


typedef struct hfsfilemeta {
	struct	siblinghead	h_siblinghead;		/* Head of the sibling list */
    simple_lock_data_t	h_siblinglock;		/* sibling list lock. */
    simple_lock_data_t	h_metalock;			/* SER XXX Should be deprecated. Locks the meta data. */
	u_int32_t			h_metaflags;		/* IN_LONGNAME, etc */
	struct vnode		*h_devvp;			/* vnode for block I/O. */

	dev_t				h_dev;				/* Device associated with the inode. */
	u_int32_t			h_nodeID;			/* specific id of this node */
	u_int32_t			h_dirID;			/* Parent Directory ID */
	u_int32_t			h_hint;				/* Catalog hint */

	off_t				h_size;				/* Total physical size of object */
	u_int16_t			h_usecount;			/* How many siblings */
	u_int16_t			h_mode;				/* IFMT, permissions; see below. */
	u_int32_t			h_pflags;			/* Permission flags (NODUMP, IMMUTABLE, APPEND etc.) */
	u_int32_t			h_uid;				/* File owner. */
	u_int32_t			h_gid;				/* File group. */
	dev_t				h_rdev;				/* Special device info for this node */
	u_int32_t			h_crtime;			/* UNIX-format creation date in secs. */
	u_int32_t			h_atime;			/* UNIX-format access date in secs. */
	u_int32_t			h_mtime;			/* UNIX-format mod date in seconds */
	u_int32_t			h_ctime;			/* UNIX-format status change date */
	u_int32_t			h_butime;			/* UNIX-format last backup date in secs. */
	int16_t		h_nlink;	/* File link count */
	u_short				h_namelen;			/* Length of name string */
	char *				h_namePtr;			/* Points the name of the file */
	FileNameStr			h_fileName;			/* CName of file */
	u_int32_t	h_valence;	/* directory child count */
} hfsfilemeta;


/*
 *	Macros for quick access to fields buried in the fcb inside an hfs node:
 */
#define H_FORKTYPE(HP)	((HP)->h_type)
#define H_FILEID(HP)	((HP)->h_meta->h_nodeID)
#define H_DIRID(HP)		((HP)->h_meta->h_dirID)
#define H_NAME(HP)		((HP)->h_meta->h_namePtr)
#define H_HINT(HP)		((HP)->h_meta->h_hint)
#define H_DEV(HP)		((HP)->h_meta->h_dev)

/* These flags are kept in flags. */
#define IN_ACCESS		0x0001		/* Access time update request. */
#define IN_CHANGE		0x0002		/* Change time update request. */
#define IN_UPDATE		0x0004		/* Modification time update request. */
#define IN_MODIFIED 	0x0008		/* Node has been modified. */
#define IN_RENAME		0x0010		/* Node is being renamed. */
#define IN_SHLOCK		0x0020		/* File has shared lock. */
#define IN_EXLOCK		0x0040		/* File has exclusive lock. */
#define IN_ALLOCATING	0x1000		/* vnode is in transit, wait or ignore */
#define IN_WANT			0x2000		/* Its being waited for */

/* These flags are kept in meta flags. */
#define IN_LONGNAME 	0x0400		/* File has long name buffer. */
#define IN_UNSETACCESS	0x0200		/* File has unset access. */
#define IN_DELETED	0x0800		/* File has been marked to be deleted */
#if HFS_HARDLINKS
#define	IN_DATANODE	0x1000		/* File is a data node (hard-linked) */
#endif


/* File permissions stored in mode */
#define IEXEC			0000100		/* Executable. */
#define IWRITE			0000200		/* Writeable. */
#define IREAD			0000400		/* Readable. */
#define ISVTX			0001000		/* Sticky bit. */
#define ISGID			0002000		/* Set-gid. */
#define ISUID			0004000		/* Set-uid. */

/* File types */
#define IFMT			0170000		/* Mask of file type. */
#define IFIFO			0010000		/* Named pipe (fifo). */
#define IFCHR			0020000		/* Character device. */
#define IFDIR			0040000		/* Directory file. */
#define IFBLK			0060000		/* Block device. */
#define IFREG			0100000		/* Regular file. */
#define IFLNK			0120000		/* Symbolic link. */
#define IFSOCK			0140000		/* UNIX domain socket. */
#define IFWHT			0160000		/* Whiteout. */

/* Value to make sure vnode is real and defined */
#define HFS_VNODE_MAGIC 0x4846532b	/* 'HFS+' */

/* To test wether the forkType is a sibling type */
#define SIBLING_FORKTYPE(FORK) 	((FORK==kDataFork) || (FORK==kRsrcFork))

/*
 *	Write check macro
 */
#define	WRITE_CK(VNODE, FUNC_NAME)	{				\
    if ((VNODE)->v_mount->mnt_flag & MNT_RDONLY) {			\
        DBG_ERR(("%s: ATTEMPT TO WRITE A READONLY VOLUME\n", 	\
                 FUNC_NAME));	\
                     return(EROFS);							\
    }									\
}


/*
 *	hfsmount locking and unlocking.
 *
 *	mvl_lock_flags
 */
#define MVL_LOCKED    0x00000001	/* debug only */

#if	HFS_DIAGNOSTIC
#define MVL_LOCK(mvip)    {				\
    (simple_lock(&(mvip)->mvl_lock));			\
        (mvip)->mvl_flags |= MVL_LOCKED;			\
}

#define MVL_UNLOCK(mvip)    {				\
    if(((mvip)->mvl_flags & MVL_LOCKED) == 0) {		\
        panic("MVL_UNLOCK - hfsnode not locked");	\
    }							\
    (simple_unlock(&(mvip)->mvl_lock));			\
        (mvip)->mvl_flags &= ~MVL_LOCKED;			\
}
#else	/* HFS_DIAGNOSTIC */
#define MVL_LOCK(mvip)		(simple_lock(&(mvip)->mvl_lock))
#define MVL_UNLOCK(mvip)	(simple_unlock(&(mvip)->mvl_lock))
#endif	/* HFS_DIAGNOSTIC */


/* structure to hold a "." or ".." directory entry (12 bytes) */
typedef struct hfsdotentry {
	u_int32_t	d_fileno;	/* unique file number */
	u_int16_t	d_reclen;	/* length of this structure */
	u_int8_t	d_type;		/* dirent file type */
	u_int8_t	d_namelen;	/* len of filename */
	char		d_name[4];	/* "." or ".." */
} hfsdotentry;

#define AVERAGE_HFSDIRENTRY_SIZE  (8+22+4)
#define MAX_HFSDIRENTRY_SIZE	sizeof(struct dirent)

#define DIRENTRY_SIZE(namlen) \
    ((sizeof(struct dirent) - (NAME_MAX+1)) + (((namlen)+1 + 3) &~ 3))


enum {
	kCatalogFolderNode = 1,
	kCatalogFileNode = 2
};

/* 
 * CatalogNodeData has same layout as the on-disk HFS Plus file/dir records.
 * Classic hfs file/dir records are converted to match this layout.
 * 
 * The cnd_extra padding allows big hfs plus thread records (520 bytes max)
 * to be read onto this stucture during a cnid lookup.
 */
struct CatalogNodeData {
	int16_t			cnd_type;
	u_int16_t		cnd_flags;
	union {
	    u_int32_t	cndu_valence;	/* dirs only */
	    u_int32_t	cndu_linkCount;	/* files only */
	} cnd_un;
	u_int32_t		cnd_nodeID;
	u_int32_t		cnd_createDate;
	u_int32_t		cnd_contentModDate;
	u_int32_t		cnd_attributeModDate;
	u_int32_t		cnd_accessDate;
	u_int32_t		cnd_backupDate;
	u_int32_t 		cnd_ownerID;
	u_int32_t 		cnd_groupID;
	u_int32_t 		cnd_permissions;
	u_int32_t 		cnd_specialDevice;
	u_int8_t		cnd_finderInfo[32];
	u_int32_t 		cnd_textEncoding;
	u_int32_t		cnd_reserved;
	HFSPlusForkData	cnd_datafork;
	HFSPlusForkData	cnd_rsrcfork;
	u_int8_t		cnd_extra[272];	/* make struct at least 520 bytes long */
};
typedef struct CatalogNodeData CatalogNodeData;

#define	cnd_valence	cnd_un.cndu_valence
#define	cnd_linkCount	cnd_un.cndu_linkCount


/* structure to hold a catalog record information */
/* Of everything you wanted to know about a catalog entry, file and directory */
typedef struct hfsCatalogInfo {
    CatalogNodeData 	nodeData;
	FSSpec 				spec;					/* filename */
    u_int32_t				hint;
} hfsCatalogInfo;

//	structure definition of the searchfs system trap for the search criterea.
struct directoryInfoSpec
{
	u_long				numFiles;
};

struct fileInfoSpec
{
	off_t				dataLogicalLength;
	off_t				dataPhysicalLength;
	off_t				resourceLogicalLength;
	off_t				resourcePhysicalLength;
};

struct searchinfospec
{
	u_char				name[512];
	u_long				nameLength;
	char				attributes;		// see IM:Files 2-100
	u_long				parentDirID;
	struct timespec		creationDate;		
	struct timespec		modificationDate;		
	struct timespec		lastBackupDate;	
	u_long				finderInfo[8];
    struct fileInfoSpec f;
	struct directoryInfoSpec d;
};
typedef struct searchinfospec searchinfospec_t;

#define HFSTIMES(hp, t1, t2) {						\
	if ((hp)->h_nodeflags & (IN_ACCESS | IN_CHANGE | IN_UPDATE)) {	\
		(hp)->h_nodeflags |= IN_MODIFIED;				\
		if ((hp)->h_nodeflags & IN_ACCESS) {			\
			(hp)->h_meta->h_atime = (t1)->tv_sec;			\
		};											\
		if ((hp)->h_nodeflags & IN_UPDATE) {			\
			(hp)->h_meta->h_mtime = (t2)->tv_sec;			\
		}											\
		if ((hp)->h_nodeflags & IN_CHANGE) {			\
			(hp)->h_meta->h_ctime = time.tv_sec;			\
		};											\
		(hp)->h_nodeflags &= ~(IN_ACCESS | IN_CHANGE | IN_UPDATE);	\
	}								\
}

/* macro to determine if hfsplus */
#define ISHFSPLUS(VCB) ((VCB)->vcbSigWord == kHFSPlusSigWord)


/*
 * Various ways to acquire a VNode pointer:
 */
#define HTOV(HP) ((HP)->h_vp)

/*
 * Various ways to acquire an HFS Node pointer:
 */
#define VTOH(VP) ((struct hfsnode *)((VP)->v_data))
#define FCBTOH(FCB) ((struct hfsnode *)FCB)

/*
 * Various ways to acquire an FCB pointer:
 */
#define HTOFCB(HP) (HP)
#define VTOFCB(VP) ((FCB *)((VP)->v_data))		/* Should be the same as VTOH */

/*
 * Various ways to acquire a VFS mount point pointer:
 */
#define VTOVFS(VP) ((VP)->v_mount)
#define	HTOVFS(HP) ((HP)->h_vp->v_mount)
#define FCBTOVFS(FCB) ((FCB)->h_vp->v_mount)
#define HFSTOVFS(HFSMP) ((HFSMP)->hfs_mp)
#define VCBTOVFS(VCB) (((struct vfsVCB *)(VCB))->vcb_hfsmp->hfs_mp)

/*
 * Various ways to acquire an HFS mount point pointer:
 */
#define VTOHFS(VP) ((struct hfsmount *)((VP)->v_mount->mnt_data))
#define	HTOHFS(HP) ((struct hfsmount *)(HP)->h_vp->v_mount->mnt_data)
#define FCBTOHFS(FCB) ((struct hfsmount *)(FCB)->h_vp->v_mount->mnt_data)
#define	VFSTOHFS(MP) ((struct hfsmount *)(MP)->mnt_data)	
#define VCBTOHFS(VCB) (((struct vfsVCB *)(VCB))->vcb_hfsmp)

/*
 * Various ways to acquire a VCB pointer:
 */
#define VTOVCB(VP) (&(((struct hfsmount *)((VP)->v_mount->mnt_data))->hfs_vcb.vcb_vcb))
#define HTOVCB(HP) (&(((struct hfsmount *)((HP)->h_vp->v_mount->mnt_data))->hfs_vcb.vcb_vcb))
#define FCBTOVCB(FCB) (&(((struct hfsmount *)((FCB)->h_vp->v_mount->mnt_data))->hfs_vcb.vcb_vcb))
#define VFSTOVCB(MP) (&(((struct hfsmount *)(MP)->mnt_data)->hfs_vcb.vcb_vcb))
#define HFSTOVCB(HFSMP) (&(HFSMP)->hfs_vcb.vcb_vcb)


#define E_NONE	0
#define kHFSBlockSize 512
#define kHFSBlockShift 9	/* 2^9 = 512 */

#define IOBLKNOFORBLK(STARTINGBLOCK, BLOCKSIZEINBYTES) ((daddr_t)((STARTINGBLOCK) / ((BLOCKSIZEINBYTES) >> 9)))
#define IOBLKCNTFORBLK(STARTINGBLOCK, BYTESTOTRANSFER, BLOCKSIZEINBYTES) \
    ((int)(IOBLKNOFORBYTE(((STARTINGBLOCK) * 512) + (BYTESTOTRANSFER) - 1, (BLOCKSIZEINBYTES)) - \
           IOBLKNOFORBLK((STARTINGBLOCK), (BLOCKSIZEINBYTES)) + 1))
#define IOBYTECCNTFORBLK(STARTINGBLOCK, BYTESTOTRANSFER, BLOCKSIZEINBYTES) \
    (IOBLKCNTFORBLK((STARTINGBLOCK),(BYTESTOTRANSFER),(BLOCKSIZEINBYTES)) * (BLOCKSIZEINBYTES))
#define IOBYTEOFFSETFORBLK(STARTINGBLOCK, BLOCKSIZEINBYTES) \
    (((STARTINGBLOCK) * 512) - \
     (IOBLKNOFORBLK((STARTINGBLOCK), (BLOCKSIZEINBYTES)) * (BLOCKSIZEINBYTES)))

#define IOBLKNOFORBYTE(STARTINGBYTE, BLOCKSIZEINBYTES) ((daddr_t)((STARTINGBYTE) / (BLOCKSIZEINBYTES)))
#define IOBLKCNTFORBYTE(STARTINGBYTE, BYTESTOTRANSFER, BLOCKSIZEINBYTES) \
((int)(IOBLKNOFORBYTE((STARTINGBYTE) + (BYTESTOTRANSFER) - 1, (BLOCKSIZEINBYTES)) - \
           IOBLKNOFORBYTE((STARTINGBYTE), (BLOCKSIZEINBYTES)) + 1))
#define IOBYTECNTFORBYTE(STARTINGBYTE, BYTESTOTRANSFER, BLOCKSIZEINBYTES) \
    (IOBLKCNTFORBYTE((STARTINGBYTE),(BYTESTOTRANSFER),(BLOCKSIZEINBYTES)) * (BLOCKSIZEINBYTES))
#define IOBYTEOFFSETFORBYTE(STARTINGBYTE, BLOCKSIZEINBYTES) ((STARTINGBYTE) - (IOBLKNOFORBYTE((STARTINGBYTE), (BLOCKSIZEINBYTES)) * (BLOCKSIZEINBYTES)))

#define MAKE_VREFNUM(x)	((int32_t)((x) & 0xffff))
/*
 *	This is the straight GMT conversion constant:
 *	00:00:00 January 1, 1970 - 00:00:00 January 1, 1904
 *	(3600 * 24 * ((365 * (1970 - 1904)) + (((1970 - 1904) / 4) + 1)))
 */
#define MAC_GMT_FACTOR		2082844800UL

#define HFS_ATTR_CMN_LOOKUPMASK (ATTR_CMN_SCRIPT | ATTR_CMN_FNDRINFO | ATTR_CMN_NAMEDATTRCOUNT | ATTR_CMN_NAMEDATTRLIST)
#define HFS_ATTR_DIR_LOOKUPMASK (ATTR_DIR_LINKCOUNT | ATTR_DIR_ENTRYCOUNT)
#define HFS_ATTR_FILE_LOOKUPMASK (ATTR_FILE_LINKCOUNT | ATTR_FILE_TOTALSIZE | ATTR_FILE_ALLOCSIZE | \
									ATTR_FILE_DATALENGTH | ATTR_FILE_DATAALLOCSIZE | ATTR_FILE_DATAEXTENTS | \
									ATTR_FILE_RSRCLENGTH | ATTR_FILE_RSRCALLOCSIZE | ATTR_FILE_RSRCEXTENTS)

#define VPUT(vp)	if (((vp)->v_usecount > 0) && (*((volatile int *)(&(vp)->v_interlock))==0)) vput((vp));  else panic("hfs: vput bad ref cnt (%d)!",  (vp)->v_usecount)
#define VRELE(vp)	if (((vp)->v_usecount > 0) && (*((volatile int *)(&(vp)->v_interlock))==0)) vrele((vp)); else panic("hfs: vrele bad ref cnt (%d)!", (vp)->v_usecount)

u_int32_t to_bsd_time(u_int32_t hfs_time);
u_int32_t to_hfs_time(u_int32_t bsd_time);

int hfs_flushfiles(struct mount *mp, int flags);
short hfs_flushMDB(struct hfsmount *hfsmp, int waitfor);
short hfs_flushvolumeheader(struct hfsmount *hfsmp, int waitfor);

short hfsLookup (ExtendedVCB *vcb, u_int32_t dirID, char *name, short len, hfsCatalogInfo *catInfo);
short hfsMoveRename (ExtendedVCB *vcb, u_int32_t oldDirID, char *oldName, u_int32_t newDirID, char *newName, u_int32_t *hint);
short hfsCreate (ExtendedVCB *vcb, u_int32_t dirID, char *name, int mode);
short hfsCreateFileID (ExtendedVCB *vcb, u_int32_t parentDirID, StringPtr name, u_int32_t catalogHint, u_int32_t *fileIDPtr);
short hfs_vcreate (ExtendedVCB *vcb, hfsCatalogInfo *catInfo, u_int8_t forkType, struct vnode **vpp);
short hfsDelete (ExtendedVCB *vcb, u_int32_t parentDirID, StringPtr name, short isfile, u_int32_t catalogHint);
short hfsUnmount(struct hfsmount *hfsmp, struct proc *p);

extern int hfs_metafilelocking(struct hfsmount *hfsmp, u_long fileID, u_int flags, struct proc *p);
extern int hasOverflowExtents(struct hfsnode *hp);

short MacToVFSError(OSErr err);
void MapFileOffset(struct hfsnode *hp, off_t filePosition, daddr_t *logBlockNumber, long *blockSize, long *blockOffset);
long LogicalBlockSize(struct hfsnode *hp, daddr_t logicalBlockNumber);
void UpdateBlockMappingTable(struct hfsnode *hp);
void CopyVNodeToCatalogNode (struct vnode *vp, struct CatalogNodeData *nodeData);
void CopyCatalogToHFSNode(struct hfsCatalogInfo *catalogInfo, struct hfsnode *hp);
int bexpand(struct buf *bp, int newsize, struct buf **nbpp, long flags);
u_long FindMetaDataDirectory(ExtendedVCB *vcb);


short make_dir_entry(FCB **fileptr, char *name, u_int32_t fileID);

int AttributeBlockSize(struct attrlist *attrlist);
void PackCommonAttributeBlock(struct attrlist *alist,
							  struct vnode *vp,
							  struct hfsCatalogInfo *catInfo,
							  void **attrbufptrptr,
							  void **varbufptrptr);
void PackVolAttributeBlock(struct attrlist *alist,
						   struct vnode *vp,
						   struct hfsCatalogInfo *catInfo,
						   void **attrbufptrptr,
						   void **varbufptrptr);
void PackFileDirAttributeBlock(struct attrlist *alist,
							   struct vnode *vp,
							   struct hfsCatalogInfo *catInfo,
							   void **attrbufptrptr,
							   void **varbufptrptr);
void PackForkAttributeBlock(struct attrlist *alist,
							struct vnode *vp,
							struct hfsCatalogInfo *catInfo,
							void **attrbufptrptr,
							void **varbufptrptr);
void PackAttributeBlock(struct attrlist *alist,
						struct vnode *vp,
						struct hfsCatalogInfo *catInfo,
						void **attrbufptrptr,
						void **varbufptrptr);
void PackCatalogInfoAttributeBlock (struct attrlist *alist,
						struct vnode * root_vp,
						struct hfsCatalogInfo *catInfo,
						void **attrbufptrptr,
						void **varbufptrptr);
void UnpackCommonAttributeBlock(struct attrlist *alist,
							  struct vnode *vp,
							  struct hfsCatalogInfo *catInfo,
							  void **attrbufptrptr,
							  void **varbufptrptr);
void UnpackAttributeBlock(struct attrlist *alist,
						struct vnode *vp,
						struct hfsCatalogInfo *catInfo,
						void **attrbufptrptr,
						void **varbufptrptr);
unsigned long BestBlockSizeFit(unsigned long allocationBlockSize,
                               unsigned long blockSizeLimit,
                               unsigned long baseMultiple);

OSErr	hfs_MountHFSVolume(struct hfsmount *hfsmp, HFSMasterDirectoryBlock *mdb,
		u_long sectors, struct proc *p);
OSErr	hfs_MountHFSPlusVolume(struct hfsmount *hfsmp, HFSPlusVolumeHeader *vhp,
		u_long embBlkOffset, u_long sectors, struct proc *p);
OSStatus  GetInitializedVNode(struct hfsmount *hfsmp, struct vnode **tmpvnode );

int hfs_getconverter(u_int32_t encoding, hfs_to_unicode_func_t *get_unicode,
		     unicode_to_hfs_func_t *get_hfsname);

int hfs_relconverter(u_int32_t encoding);

int hfs_to_utf8(ExtendedVCB *vcb, Str31 hfs_str, ByteCount maxDstLen,
		ByteCount *actualDstLen, unsigned char* dstStr);

int utf8_to_hfs(ExtendedVCB *vcb, ByteCount srcLen, const unsigned char* srcStr,
		Str31 dstStr);

#endif /* __HFS__ */
