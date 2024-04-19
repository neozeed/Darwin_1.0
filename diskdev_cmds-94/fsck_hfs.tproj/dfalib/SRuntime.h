/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/* SRuntime.h */

#ifndef __SRUNTIME__
#define __SRUNTIME__

//#define BSD 1


//#include <MacTypes.h>
//#include <Errors.h>
#include <HFSVolumes.h>


#if BSD

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#else

#include <MacMemory.h>

#endif

#ifdef BSD
/* vcbFlags bits */
enum {
  kVCBFlagsIdleFlushBit           = 3,                                                    /* Set if volume should be flushed at idle time */
        kVCBFlagsIdleFlushMask          = 0x0008,
        kVCBFlagsHFSPlusAPIsBit         = 4,                                                    /* Set if volume implements HFS Plus APIs itself (not via emu\
												   lation) */
        kVCBFlagsHFSPlusAPIsMask        = 0x0010,
        kVCBFlagsHardwareGoneBit        = 5,                                                    /* Set if disk driver returned a hardwareGoneErr to Read or W\
												   rite */
        kVCBFlagsHardwareGoneMask       = 0x0020,
        kVCBFlagsVolumeDirtyBit         = 15,                                                   /* Set if volume information has changed since the last Flush\
												   Vol */
        kVCBFlagsVolumeDirtyMask        = 0x8000
};
#else
/* Disk Cache constants */
/*
 * UTGetBlock options
 */
enum {
	gbDefault					= 0,							/* default value - read if not found */
																/*	bits and masks */
	gbReadBit					= 0,							/* read block from disk (forced read) */
	gbReadMask					= 0x0001,
	gbExistBit					= 1,							/* get existing cache block */
	gbExistMask					= 0x0002,
	gbNoReadBit					= 2,							/* don't read block from disk if not found in cache */
	gbNoReadMask				= 0x0004,
	gbReleaseBit				= 3,							/* release block immediately after GetBlock */
	gbReleaseMask				= 0x0008
};


/*
 * UTReleaseBlock options
 */
enum {
	rbDefault					= 0,							/* default value - just mark the buffer not in-use */
																/*	bits and masks */
	rbWriteBit					= 0,							/* force write buffer to disk */
	rbWriteMask					= 0x0001,
	rbTrashBit					= 1,							/* trash buffer contents after release */
	rbTrashMask					= 0x0002,
	rbDirtyBit					= 2,							/* mark buffer dirty */
	rbDirtyMask					= 0x0004,
	rbFreeBit					= 3,							/* free the buffer (save in the hash) */
	rbFreeMask					= 0x000A						/* rbFreeMask (rbFreeBit + rbTrashBit) works as rbTrash on < System 7.0 RamCache; on >= System 7.0, rbfreeMask overrides rbTrash */
};


/*
 * UTFlushCache options
 */
enum {
	fcDefault					= 0,							/* default value - pass this fcOption to just flush any dirty buffers */
																/*	bits and masks */
	fcTrashBit					= 0,							/* (don't pass this as fcOption, use only for testing bit) */
	fcTrashMask					= 0x0001,						/* pass this fcOption value to flush and trash cache blocks */
	fcFreeBit					= 1,							/* (don't pass this as fcOption, use only for testing bit) */
	fcFreeMask					= 0x0003						/* pass this fcOption to flush and free cache blocks (Note: both fcTrash and fcFree bits are set) */
};


/*
 * UTCacheReadIP and UTCacheWriteIP cacheOption bits and masks are the ioPosMode
 * bits and masks in Files.x
 */

/*
 * Cache routine internal error codes
 */
enum {
	chNoBuf						= 1,							/* no free cache buffers (all in use) */
	chInUse						= 2,							/* requested block in use */
	chnotfound					= 3,							/* requested block not found */
	chNotInUse					= 4								/* block being released was not in use */
};


/*
 * FCBRec.fcbFlags bits
 */
enum {
	fcbWriteBit					= 0,							/* Data can be written to this file */
	fcbWriteMask				= 0x01,
	fcbResourceBit				= 1,							/* This file is a resource fork */
	fcbResourceMask				= 0x02,
	fcbWriteLockedBit			= 2,							/* File has a locked byte range */
	fcbWriteLockedMask			= 0x04,
	fcbSharedWriteBit			= 4,							/* File is open for shared write access */
	fcbSharedWriteMask			= 0x10,
	fcbFileLockedBit			= 5,							/* File is locked (write-protected) */
	fcbFileLockedMask			= 0x20,
	fcbOwnClumpBit				= 6,							/* File has clump size specified in FCB */
	fcbOwnClumpMask				= 0x40,
	fcbModifiedBit				= 7,							/* File has changed since it was last flushed */
	fcbModifiedMask				= 0x80
};

enum {
	fcbLargeFileBit				= 3,							/* File may grow beyond 2GB; cache uses file blocks, not bytes */
	fcbLargeFileMask			= 0x08
};
#endif

struct SFCB;

struct SVCB {
	UInt16		vcbSignature;
	UInt16		vcbVersion;
	UInt32		vcbAttributes;
	UInt32		vcbLastMountedVersion;
	UInt32		vcbReserved1;
	UInt32		vcbCreateDate;
	UInt32		vcbModifyDate;
	UInt32		vcbBackupDate;
	UInt32		vcbCheckedDate;
	UInt32		vcbFileCount;
	UInt32		vcbFolderCount;
	UInt32		vcbBlockSize;
	UInt32		vcbTotalBlocks;
	UInt32		vcbFreeBlocks;
	UInt32		vcbNextAllocation;
	UInt32 		vcbRsrcClumpSize;
	UInt32		vcbDataClumpSize;
	UInt32 		vcbNextCatalogID;
	UInt32		vcbWriteCount;
	UInt64		vcbEncodingsBitmap;
	UInt8		vcbFinderInfo[32];

	/* MDB-specific fields... */
	SInt16		vcbNmFls;	/* number of files in root folder */
	SInt16		vcbNmRtDirs;	/* number of directories in root folder */
	UInt16		vcbVBMSt;	/* first sector of HFS volume bitmap */
	UInt16		vcbAlBlSt;	/* first allocation block in HFS volume */
	UInt16		vcbVSeqNum;	/* volume backup sequence number */
	UInt16		vcbReserved2;
	Str27		vcbVN;		/* HFS volume name */

	/* runtime fields... */
	struct SFCB * 	vcbAllocationFile;
	struct SFCB * 	vcbExtentsFile;
	struct SFCB * 	vcbCatalogFile;
	struct SFCB * 	vcbAttributesFile;
	struct SFCB * 	vcbStartupFile;

	UInt32		vcbEmbeddedOffset;	/* Sector where HFS+ starts */
	UInt16		vcbFlags;
	SInt16		vcbDriveNumber;
	SInt16		vcbDriverReadRef;
	SInt16		vcbDriverWriteRef;

	/* deprecated fields... */
	SInt16		vcbVRefNum;
};
typedef struct SVCB SVCB;


struct SFCB {
	UInt32			fcbFileID;
	UInt32			fcbFlags;
	struct SVCB *		fcbVolume;
	void *			fcbBtree;
	HFSExtentRecord		fcbExtents16;
	HFSPlusExtentRecord	fcbExtents32;
	UInt32			fcbCatalogHint;
	UInt32			fcbClumpSize;
	UInt64 			fcbLogicalSize;
	UInt64			fcbPhysicalSize;
};
typedef struct SFCB SFCB;


extern OSErr GetDeviceSize(int driveRefNum, UInt32 *numBlocks, UInt32 *blockSize);

extern OSErr DeviceRead(int device, int drive, void* buffer, SInt64 offset, UInt32 reqBytes, UInt32 *actBytes);

extern OSErr DeviceWrite(int device, int drive, void* buffer, SInt64 offset, UInt32 reqBytes, UInt32 *actBytes);


#if BSD

#define AllocateMemory(size)		malloc((size_t)(size))
#define	AllocateClearMemory(size)	calloc(1,(size_t)(size))
#define ReallocateMemory(ptr,newSize)	SetPtrSize((void*)(ptr),(size_t)(newSize))
#define MemorySize(ptr)			malloc_size((void*)(ptr))
#define DisposeMemory(ptr)		free((void *)(ptr))
#define CopyMemory(src,dst,len)		bcopy((void*)(src),(void*)(dst),(size_t)(len))
#define ClearMemory(start,len)	 	bzero((void*)(start),(size_t)(len))

#else

#define AllocateMemory(size)		NewPtr((Size)(size))
#define	AllocateClearMemory(size)	NewPtrClear((Size)(size))
#define ReallocateMemory(ptr,newSize)	SetPtrSize((Ptr)(ptr),(Size)(newSize))
#define MemorySize(ptr)			GetPtrSize((Ptr)(ptr))
#define DisposeMemory(ptr)		DisposePtr((Ptr)(ptr))
#define CopyMemory(src,dst,len)		BlockMoveData((void *)(src),(void *)(dst),(Size)(len))
void ClearMemory(void* start, long len);
#endif


#endif /* __SRUNTIME__ */


